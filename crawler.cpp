#include "crawler.h"

#include <QNetworkCookieJar>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QSet>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QCoreApplication>
#include <QTimer>
#include "configmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

static const QByteArray &ua()
{
    static const QByteArray v = ConfigManager::instance().openJudgeUserAgent().toUtf8();
    return v;
}

// Debug logging helper – call clearLog() at startup, then debugLog() throughout
static void debugLog(const QString &msg)
{
    QFile file(ConfigManager::instance().openJudgeDebugLogFile());
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz ")) << msg << "\n";
    }
}

static void clearLog()
{
    QFile file(ConfigManager::instance().openJudgeDebugLogFile());
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.close();
}

Crawler::Crawler(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl(ConfigManager::instance().openJudgeBaseUrl())
{
    m_manager->setCookieJar(new QNetworkCookieJar(m_manager));
    clearLog();
    debugLog(QStringLiteral("=== Crawler started ==="));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Crawler::login(const QString &username, const QString &password)
{
    m_username = username;
    m_password = password;

    debugLog(QStringLiteral("login: fetching login page to establish session"));

    // Step 1: fetch the login page to get session cookie (PHPSESSID)
    QString loginPageUrl = m_baseUrl + QStringLiteral("/auth/login/");
    QNetworkRequest pageReq(loginPageUrl);
    pageReq.setRawHeader("User-Agent", ua());
    pageReq.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *pageReply = m_manager->get(pageReq);
    connect(pageReply, &QNetworkReply::finished, this, [this, pageReply]() {
        pageReply->deleteLater();

        if (pageReply->error() != QNetworkReply::NoError) {
            debugLog(QStringLiteral("login: login page error: %1").arg(pageReply->errorString()));
            emit loginFailed(QStringLiteral("无法访问登录页面: ") + pageReply->errorString());
            return;
        }

        debugLog(QStringLiteral("login: login page loaded, session cookie established"));

        // Step 2: POST credentials to JSON API
        QUrl apiUrl(m_baseUrl + QStringLiteral("/api/auth/login/"));
        QNetworkRequest apiReq(apiUrl);
        apiReq.setHeader(QNetworkRequest::ContentTypeHeader,
                         QByteArrayLiteral("application/x-www-form-urlencoded"));
        apiReq.setRawHeader("User-Agent", ua());
        apiReq.setRawHeader("X-Requested-With", QByteArrayLiteral("XMLHttpRequest"));
        apiReq.setRawHeader("Referer", (m_baseUrl + "/auth/login/").toUtf8());

        QUrlQuery query;
        query.addQueryItem(QStringLiteral("email"), m_username);
        query.addQueryItem(QStringLiteral("password"), m_password);

        QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
        debugLog(QStringLiteral("login: POSTing credentials to /api/auth/login/"));

        QNetworkReply *lr = m_manager->post(apiReq, postData);
        connect(lr, &QNetworkReply::finished, this, [this, lr]() {
            onLoginFinished(lr);
        });
    });
}

void Crawler::fetchMainPage()
{
    QUrl url(m_baseUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onMainPageFinished(reply);
    });
}

void Crawler::fetchHomeworkProblems(const QString &url)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());

        QList<HomeworkItem> problems;
        QUrl baseUrl(url);

        // Nav keywords – skip short nav-page links
        static const QStringList kBadTitles = {
            QStringLiteral("题目"), QStringLiteral("排名"), QStringLiteral("状态"),
            QStringLiteral("统计"), QStringLiteral("提问"), QStringLiteral("查看"),
            QStringLiteral("提交"), QStringLiteral("帮助"), QStringLiteral("关于"),
            QStringLiteral("登录"), QStringLiteral("注册"), QStringLiteral("管理"),
            QStringLiteral("密码"), QStringLiteral("首页"), QStringLiteral("计划中的比赛"),
            QStringLiteral("已结束的比赛"), QStringLiteral("English")
        };

        QRegularExpression rx(QStringLiteral("<a\\s+href=\"([^\"]+)\"[^>]*>([^<]+)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = rx.globalMatch(html);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString rawUrl = match.captured(1).trimmed();
            QString title = decodeHtmlEntities(match.captured(2).trimmed()).simplified();

            if (title.isEmpty()) continue;

            bool isBad = false;
            for (const auto &kw : kBadTitles)
                if (title == kw || title.startsWith(kw + QStringLiteral(" ")))
                { isBad = true; break; }
            if (isBad) continue;

            QUrl resolved = baseUrl.resolved(QUrl(rawUrl));
            QString fullUrl = resolved.toString();
            QString path = QUrl(rawUrl).path();
            if (path.isEmpty()) path = QUrl(fullUrl).path();

            QStringList segments = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            if (segments.size() < 2) continue;

            // Skip non-problem path segments (user profile, auth, etc.)
            static const QSet<QString> kSkipPathSegments = {
                QStringLiteral("user"), QStringLiteral("profile"), QStringLiteral("member"),
                QStringLiteral("auth"), QStringLiteral("login"), QStringLiteral("register"),
                QStringLiteral("password"), QStringLiteral("forget")
            };
            if (kSkipPathSegments.contains(segments.first())) continue;

            QString lastSeg = segments.last();

            static const QStringList kNavPages = {
                QStringLiteral("ranking"), QStringLiteral("status"),
                QStringLiteral("statistics"), QStringLiteral("clarify"),
                QStringLiteral("submit"), QStringLiteral("standing"),
                QStringLiteral("my"), QStringLiteral("coming"), QStringLiteral("past")
            };
            if (kNavPages.contains(lastSeg)) continue;

            bool looksLikeProblemId = false;
            bool allDigits = true;
            for (const QChar &c : lastSeg) { if (!c.isDigit()) { allDigits = false; break; } }
            if (allDigits && !lastSeg.isEmpty()) looksLikeProblemId = true;
            if (!looksLikeProblemId && lastSeg.length() == 1 && lastSeg[0].isLetter())
                looksLikeProblemId = true;
            if (!looksLikeProblemId) continue;

            QString displayTitle = title;
            if (!displayTitle.startsWith(lastSeg))
                displayTitle = lastSeg + QStringLiteral(": ") + displayTitle;

            problems.append({displayTitle, fullUrl});
        }

        // Deduplicate by URL – keep longer title, preserve original order
        QHash<QString, QString> best;  // url → best title
        QList<QString> urlOrder;       // insertion order
        for (const auto &p : problems) {
            auto it2 = best.constFind(p.url);
            if (it2 == best.constEnd()) {
                best[p.url] = p.title;
                urlOrder.append(p.url);
            } else if (p.title.length() > it2->length()) {
                best[p.url] = p.title;
            }
        }
        problems.clear();
        for (const auto &url : urlOrder)
            problems.append({best[url], url});

        QRegularExpression titleRx(QStringLiteral("<title>([^<]+)</title>"));
        QRegularExpressionMatch titleMatch = titleRx.match(html);
        QString homeworkTitle = titleMatch.hasMatch()
            ? decodeHtmlEntities(titleMatch.captured(1).trimmed()) : url;

        emit homeworkProblemsReady(homeworkTitle, problems);
    });
}

void Crawler::fetchProblemDetail(const QString &url)
{
    QUrl qurl(url);
    if (!qurl.isValid()) {
        emit networkError(QStringLiteral("无效的URL: ") + url);
        return;
    }

    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeProblemDetailTimeoutMs());

    debugLog(QStringLiteral("fetchProblemDetail: %1").arg(url));

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            debugLog(QStringLiteral("fetchProblemDetail error: %1").arg(reply->errorString()));
            if (reply->error() == QNetworkReply::OperationCanceledError
                || reply->error() == QNetworkReply::TimeoutError)
                emit networkError(QStringLiteral("请求超时: ") + url);
            else
                emit networkError(QStringLiteral("请求失败(%1): %2")
                    .arg(reply->error()).arg(reply->errorString()));
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());
        ProblemDetail detail = parseProblemDetail(html);
        if (detail.title.isEmpty())
            detail.title = url.section(QLatin1Char('/'), -2, -2);
        emit problemDetailReady(detail);
    });
}

void Crawler::fetchPastPage(const QString &url)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());
        QList<HomeworkItem> past;
        PageInfo info;

        // Parse past-contest items
        QRegularExpression rx(QStringLiteral(
            "<a\\s+href=\"([^\"]+)\"[^>]*>([^<]+)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = rx.globalMatch(html);

        // Find the past-contest section
        QString sectionHtml;
        QRegularExpression secRx(QStringLiteral(
            "<div[^>]*class=\"past-list\"[^>]*>(.*?)</div>\\s*(?=</div>)"),
            QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch secMatch = secRx.match(html);
        if (secMatch.hasMatch())
            sectionHtml = secMatch.captured(1);
        else
            sectionHtml = html; // fallback

        // Fallback: just find all contest-like links on the page
        QRegularExpression contestRx(QStringLiteral(
            "<a\\s+href=\"/([^/\"]+/)\"[^>]*>([^<]+)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator cit = contestRx.globalMatch(html);

        QSet<QString> seen;
        while (cit.hasNext()) {
            QRegularExpressionMatch m = cit.next();
            QString linkUrl = m.captured(1).trimmed();
            QString title = decodeHtmlEntities(m.captured(2).trimmed()).simplified();
            if (title.isEmpty() || seen.contains(linkUrl)) continue;
            seen.insert(linkUrl);

            // Skip nav links
            if (title == QStringLiteral("登录") || title == QStringLiteral("注册")
                || title == QStringLiteral("帮助") || title == QStringLiteral("关于")
                || title == QStringLiteral("English"))
                continue;

            // Only keep contest-like URLs (hw, practise, midexam, pool)
            if (linkUrl.contains(QLatin1String("hw"))
                || linkUrl.contains(QLatin1String("practise"))
                || linkUrl.contains(QLatin1String("midexam"))
                || linkUrl.contains(QLatin1String("pool"))
                || linkUrl.contains(QLatin1String("contest")))
            {
                QString fullUrl = m_baseUrl + QLatin1Char('/') + linkUrl;
                past.append({title, fullUrl});
            }
        }

        // Parse pagination links
        QRegularExpression pageRx(QStringLiteral(
            "<a\\s+href=\"([^\"]*page=([0-9]+)[^\"]*)\"[^>]*>([0-9]+)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator pit = pageRx.globalMatch(html);
        int maxPage = 0;
        while (pit.hasNext()) {
            QRegularExpressionMatch pm = pit.next();
            int p = pm.captured(2).toInt();
            if (p > maxPage) maxPage = p;
        }

        // Determine current page from URL query
        QUrl currentUrl(reply->url());
        int curPage = QUrlQuery(currentUrl).queryItemValue(QStringLiteral("page")).toInt();
        if (curPage < 1) curPage = 1;

        info.url = currentUrl.toString();
        info.currentPage = curPage;
        info.hasPrev = (curPage > 1);
        info.hasNext = (curPage < maxPage) || (maxPage == 0 && past.size() >= 20);

        emit pastPageReady(past, info);
    });
}

// ---------------------------------------------------------------------------
// Login flow
// ---------------------------------------------------------------------------

void Crawler::onLoginPageFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        debugLog(QStringLiteral("onLoginPageFinished: NETWORK ERROR getting /login/ -- %1").arg(reply->errorString()));
        debugLog(QStringLiteral("onLoginPageFinished: falling back to main page login flow"));
        QUrl url(m_baseUrl);
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", ua());

        QNetworkReply *mainReply = m_manager->get(request);
        connect(mainReply, &QNetworkReply::finished, this, [this, mainReply]() {
            mainReply->deleteLater();
            QString html = QString::fromUtf8(mainReply->readAll());
            QString csrf = extractCsrfToken(html);

            if (!csrf.isEmpty()) {
                debugLog(QStringLiteral("onLoginPageFinished fallback: CSRF found on main page, POSTing login"));
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("username"), m_username);
                query.addQueryItem(QStringLiteral("password"), m_password);
                query.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), csrf);

                QUrl loginUrl(m_baseUrl + "/login/");
                QNetworkRequest postReq(loginUrl);
                postReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                  QByteArrayLiteral("application/x-www-form-urlencoded"));
                postReq.setRawHeader("User-Agent", ua());
                postReq.setRawHeader("Referer", m_baseUrl.toUtf8());

                QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
                QNetworkReply *lr = m_manager->post(postReq, postData);
                connect(lr, &QNetworkReply::finished, this, [this, lr]() {
                    onLoginFinished(lr);
                });
            } else {
                debugLog(QStringLiteral("onLoginPageFinished fallback: no CSRF found on main page, treating as public content"));
                // No login form – content is public
                QList<HomeworkItem> ongoing, past;
                PageInfo pastPage;
                parseMainPage(html, ongoing, past, pastPage);
                if (!ongoing.isEmpty() || !past.isEmpty())
                    emit mainPageReady(ongoing, past, pastPage);
                else
                    emit loginFailed(QStringLiteral("无法获取页面内容"));
            }
        });
        return;
    }

    debugLog(QStringLiteral("onLoginPageFinished: /login/ page loaded OK, replyUrl=%1").arg(reply->url().toString()));
    QString html = QString::fromUtf8(reply->readAll());
    QString csrf = extractCsrfToken(html);
    debugLog(QStringLiteral("onLoginPageFinished: CSRF token found=%1, token=%2")
        .arg(!csrf.isEmpty()).arg(csrf.left(20)));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("username"), m_username);
    query.addQueryItem(QStringLiteral("password"), m_password);
    query.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), csrf);

    QUrl loginUrl(m_baseUrl + "/login/");
    QNetworkRequest request(loginUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", ua());
    request.setRawHeader("Referer", (m_baseUrl + "/login/").toUtf8());

    QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *loginReply = m_manager->post(request, postData);
    connect(loginReply, &QNetworkReply::finished, this, [this, loginReply]() {
        onLoginFinished(loginReply);
    });
}

void Crawler::onLoginFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        debugLog(QStringLiteral("onLoginFinished: NETWORK ERROR: %1").arg(reply->errorString()));
        emit loginFailed(QStringLiteral("网络错误: ") + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    debugLog(QStringLiteral("onLoginFinished: response='%1'")
        .arg(QString::fromUtf8(data.left(200))));

    // Parse JSON response
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        debugLog(QStringLiteral("onLoginFinished: JSON parse error: %1").arg(parseError.errorString()));
        emit loginFailed(QStringLiteral("登录返回数据异常"));
        return;
    }

    QJsonObject obj = doc.object();
    QString result = obj.value(QStringLiteral("result")).toString();

    if (result == QStringLiteral("SUCCESS")) {
        debugLog(QStringLiteral("onLoginFinished: login SUCCESS"));
        emit loginSuccess();

        // Fetch the main page to populate the contest list
        QUrl url(m_baseUrl);
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", ua());
        request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());
        QNetworkReply *mainReply = m_manager->get(request);
        connect(mainReply, &QNetworkReply::finished, this, [this, mainReply]() {
            onMainPageFinished(mainReply);
        });
    } else {
        QString message = obj.value(QStringLiteral("message")).toString();
        if (message.isEmpty())
            message = QStringLiteral("登录失败，请检查用户名和密码");
        debugLog(QStringLiteral("onLoginFinished: login FAILED: %1").arg(message));
        emit loginFailed(message);
    }
}

void Crawler::onMainPageFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit networkError(reply->errorString());
        return;
    }

    QUrl redirectedUrl = reply->url();
    QString html = QString::fromUtf8(reply->readAll());

    // Login detection only runs during the login confirmation flow
    if (m_isLoginFlow) {
        m_isLoginFlow = false;

        bool urlIsLogin = redirectedUrl.path().contains(QLatin1String("/login"));
        bool hasLoginError = html.contains(QStringLiteral("用户名或密码错误"))
                             || html.contains(QStringLiteral("请输入正确的用户名"));
        bool hasLoginForm = html.contains(QStringLiteral("请输入用户名"))
                            || html.contains(QStringLiteral("type=\"password\""))
                            || html.contains(QStringLiteral("name=\"password\""));

        // Only check username indicator if m_username is non-empty
        bool usernameInHtml = !m_username.isEmpty() && html.contains(m_username);
        bool hasLoggedInIndicator = usernameInHtml
                                    || html.contains(QStringLiteral(">退出<"))
                                    || html.contains(QStringLiteral(">注销<"))
                                    || html.contains(QStringLiteral(">退出登录<"))
                                    || html.contains(QStringLiteral(">注销用户<"));
        debugLog(QStringLiteral("onMainPageFinished: urlPath=%1, urlIsLogin=%2, hasLoginError=%3, hasLoginForm=%4, hasLoggedInIndicator=%5")
            .arg(redirectedUrl.path()).arg(urlIsLogin).arg(hasLoginError).arg(hasLoginForm).arg(hasLoggedInIndicator));

        if (urlIsLogin || hasLoginError) {
            debugLog(QStringLiteral("onMainPageFinished: login FAILED (url or error)"));
            emit loginFailed(QStringLiteral("登录失败，请检查用户名和密码"));
            return;
        }

        if (hasLoginForm && !hasLoggedInIndicator) {
            debugLog(QStringLiteral("onMainPageFinished: login FAILED (form without login)"));
            emit loginFailed(QStringLiteral("登录失败，请检查用户名和密码"));
            return;
        }

        debugLog(QStringLiteral("onMainPageFinished: login SUCCESS"));
        emit loginSuccess();
    }

    QList<HomeworkItem> ongoing, past;
    PageInfo pastPage;
    parseMainPage(html, ongoing, past, pastPage);

    emit mainPageReady(ongoing, past, pastPage);
    // Backward compat
    emit homeworkListReady(ongoing);
}

// ---------------------------------------------------------------------------
// HTML helpers
// ---------------------------------------------------------------------------

QString Crawler::extractCsrfToken(const QString &html)
{
    QRegularExpression rx(QStringLiteral(
        "<input[^>]*name=['\"]csrfmiddlewaretoken['\"][^>]*value=['\"]([^'\"]+)['\"][^>]*>"));
    QRegularExpressionMatch m = rx.match(html);
    if (m.hasMatch()) return m.captured(1);

    QRegularExpression rx2(QStringLiteral(
        "csrfmiddlewaretoken['\"]\\s*value=['\"]([^'\"]+)"));
    QRegularExpressionMatch m2 = rx2.match(html);
    return m2.hasMatch() ? m2.captured(1) : QString();
}

void Crawler::parseMainPage(const QString &html,
                            QList<HomeworkItem> &ongoing,
                            QList<HomeworkItem> &past,
                            PageInfo &pastPage)
{
    auto extractLinks = [&](const QString &sectionHtml,
                            QList<HomeworkItem> &out) {
        static const QRegularExpression ddlRx(
            QStringLiteral("(\\d{4}[-/]\\d{1,2}[-/]\\d{1,2}\\s*\\d{1,2}:\\d{2})"));
        QRegularExpression rx(QStringLiteral(
            "<a\\s+href=\"([^\"]+)\"[^>]*>([^<]+)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = rx.globalMatch(sectionHtml);
        QSet<QString> seen;
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            QString url = m.captured(1).trimmed();
            QString title = decodeHtmlEntities(m.captured(2).trimmed()).simplified();
            if (title.isEmpty() || seen.contains(url)) continue;
            seen.insert(url);

            // Skip nav
            if (title == QStringLiteral("登录") || title == QStringLiteral("注册")
                || title == QStringLiteral("帮助") || title == QStringLiteral("关于")
                || title == QStringLiteral("English") || title == QStringLiteral("计划中的比赛"))
                continue;

            // Extract deadline from surrounding HTML (look within 200 chars after <a>)
            QString deadline;
            QString surrounding = sectionHtml.mid(m.capturedStart(),
                qMin(200, sectionHtml.length() - m.capturedStart()));
            QRegularExpressionMatch ddlMatch = ddlRx.match(surrounding);
            if (ddlMatch.hasMatch())
                deadline = ddlMatch.captured(1);

            if (url.startsWith(QLatin1Char('/')))
                url = m_baseUrl + url;
            out.append({title, url, deadline});
        }
    };

    debugLog(QStringLiteral("parseMainPage: HTML len=%1").arg(html.length()));
    debugLog(QStringLiteral("  contains 'current-contest'=%1, contains 'past-contest'=%2")
        .arg(html.contains(QStringLiteral("current-contest")))
        .arg(html.contains(QStringLiteral("past-contest"))));

    // A quick check: log the 200 chars around "contest" to verify class names used
    {
        QRegularExpression anyContest(QStringLiteral("<[^>]*contest[^>]*>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it2 = anyContest.globalMatch(html);
        int sample = 0;
        while (it2.hasNext() && sample < 3) {
            QRegularExpressionMatch m2 = it2.next();
            debugLog(QStringLiteral("  sample contest tag: %1").arg(m2.captured(0).left(120)));
            sample++;
        }
    }

    // Extract all current-contest sections
    {
        QRegularExpression secRx(QStringLiteral(
            "<ul[^>]*class=[\"']current-contest[^\"']*[\"'][^>]*>(.*?)</ul>"),
            QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator sit = secRx.globalMatch(html);
        int sectionCount = 0;
        while (sit.hasNext()) {
            QRegularExpressionMatch sm = sit.next();
            sectionCount++;
            int before = ongoing.size();
            extractLinks(sm.captured(1), ongoing);
            int after = ongoing.size();
            debugLog(QStringLiteral("  current-contress section #%1: captured len=%2, links found=%3")
                .arg(sectionCount).arg(sm.captured(1).length()).arg(after - before));
        }
        debugLog(QStringLiteral("  total current-contest sections=%1, ongoing items=%2")
            .arg(sectionCount).arg(ongoing.size()));
        for (const auto &o : ongoing)
            debugLog(QStringLiteral("    '%1' deadline='%2'").arg(o.title).arg(o.deadline));
    }

    // Extract past-contest section items and pagination URL
    {
        QRegularExpression secRx(QStringLiteral(
            "<div[^>]*class=\"past-contest[^\"]*\"[^>]*>(.*?)</div>\\s*(?=</div>|<div)"),
            QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch sm = secRx.match(html);
        if (sm.hasMatch()) {
            QString secHtml = sm.captured(1);
            extractLinks(secHtml, past);

            // Find "更多" or pagination link to the full past page
            QRegularExpression moreRx(QStringLiteral(
                "<a\\s+href=\"(/contests/past[^\"]*)\""));
            QRegularExpressionMatch more = moreRx.match(secHtml);
            if (more.hasMatch()) {
                QString pageUrl = m_baseUrl + more.captured(1);
                pastPage.url = pageUrl;
                pastPage.currentPage = 1;
                pastPage.hasNext = true;  // assume more pages exist
            }
        }
    }

    // If no past items found, try broader match
    if (past.isEmpty()) {
        // Broader matching
    }
}

QString Crawler::decodeHtmlEntities(const QString &html)
{
    QString result = html;
    result.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    result.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    result.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    result.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    result.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    result.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

    QRegularExpression numRx(QStringLiteral("&#(\\d+);"));
    QRegularExpressionMatchIterator it = numRx.globalMatch(result);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result.replace(match.captured(0), QChar(match.captured(1).toInt()));
    }
    return result.trimmed();
}

QString Crawler::stripHtmlTags(const QString &html)
{
    QString result = html;
    QRegularExpression scriptRx(QStringLiteral("<script[^>]*>.*?</script>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    result.replace(scriptRx, QString());
    QRegularExpression styleRx(QStringLiteral("<style[^>]*>.*?</style>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    result.replace(styleRx, QString());

    result.replace(QRegularExpression(QStringLiteral("<br\\s*/?>")), QStringLiteral("\n"));
    result.replace(QRegularExpression(QStringLiteral("</p>")), QStringLiteral("\n"));
    result.replace(QRegularExpression(QStringLiteral("</div>")), QStringLiteral("\n"));
    result.replace(QRegularExpression(QStringLiteral("</li>")), QStringLiteral("\n"));
    result.replace(QRegularExpression(QStringLiteral("</tr>")), QStringLiteral("\n"));

    result.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QString());
    result = decodeHtmlEntities(result);
    result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return result.trimmed();
}

ProblemDetail Crawler::parseProblemDetail(const QString &html)
{
    ProblemDetail detail;

    QRegularExpression titleRx(QStringLiteral("<title>([^<]+)</title>"));
    QRegularExpressionMatch titleMatch = titleRx.match(html);
    if (titleMatch.hasMatch())
        detail.title = decodeHtmlEntities(titleMatch.captured(1).trimmed());

    debugLog(QStringLiteral("parseProblemDetail title=%1").arg(detail.title));
    debugLog(QStringLiteral("  contains &lt;dt&gt;=%1, contains &lt;h3&gt;=%2, contains &lt;pre&gt;=%3")
        .arg(html.contains(QStringLiteral("<dt")))
        .arg(html.contains(QStringLiteral("<h3")))
        .arg(html.contains(QStringLiteral("<pre"))));

    // Log form elements on the problem page (may contain language select for submit)
    {
        QRegularExpression selRx(QStringLiteral("<select[^>]*>(.*?)</select>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator selIt = selRx.globalMatch(html);
        while (selIt.hasNext()) {
            QRegularExpressionMatch selM = selIt.next();
            QRegularExpression nameRx(QStringLiteral("name=\"([^\"]+)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch nameM = nameRx.match(selM.captured(0));
            QString selName = nameM.hasMatch() ? nameM.captured(1) : "(unnamed)";
            debugLog(QStringLiteral("  PROBLEM PAGE select name='%1'").arg(selName));
            QRegularExpression optRx(QStringLiteral("<option[^>]*value=\"([^\"]*)\"[^>]*>([^<]+)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator optIt = optRx.globalMatch(selM.captured(0));
            while (optIt.hasNext()) {
                QRegularExpressionMatch optM = optIt.next();
                bool selected = optM.captured(0).contains(QLatin1String("selected"));
                debugLog(QStringLiteral("    option value='%1'%2 text='%3'")
                    .arg(optM.captured(1), selected ? QStringLiteral(" [SELECTED]") : QString(), optM.captured(2).trimmed()));
            }
        }
        QRegularExpression formRx(QStringLiteral("<form[^>]*action=\"([^\"]*)\"[^>]*method=\"([^\"]*)\""),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator formIt = formRx.globalMatch(html);
        while (formIt.hasNext()) {
            QRegularExpressionMatch formM = formIt.next();
            if (!formM.captured(1).contains(QLatin1String("search")))
                debugLog(QStringLiteral("  PROBLEM PAGE form action='%1' method=%2")
                    .arg(formM.captured(1)).arg(formM.captured(2)));
        }
    }

    static const QStringList kSectionKeywords = {
        QStringLiteral("样例输入"),
        QStringLiteral("样例输出"),
        QStringLiteral("描述"),
        QStringLiteral("输入"),
        QStringLiteral("输出"),
        QStringLiteral("提示"),
        QStringLiteral("来源")
    };

    // --- Strategy 1: <dt> / <dd> pattern (most common in OpenJudge) ---
    {
        QRegularExpression dtRx(QStringLiteral("<dt[^>]*>(.*?)</dt>"),
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator dtIt = dtRx.globalMatch(html);

        struct HPos { QString heading; qsizetype pos; };
        QList<HPos> headings;

        while (dtIt.hasNext()) {
            QRegularExpressionMatch m = dtIt.next();
            QString heading = stripHtmlTags(m.captured(1)).trimmed();
            if (heading.endsWith(QLatin1Char(':'))) heading.chop(1);
            heading = heading.trimmed();

            for (const QString &kw : kSectionKeywords) {
                if (heading.contains(kw)) {
                    headings.append({kw, m.capturedEnd()});
                    break;
                }
            }
        }

        for (qsizetype i = 0; i < headings.size(); ++i) {
            qsizetype start = headings[i].pos;
            qsizetype end = (i + 1 < headings.size()) ? headings[i + 1].pos : html.length();
            QString rawHtml = html.mid(start, end - start).trimmed();

            // Truncate at </dl> to strip nav/footer content after the problem section
            {
                int dlEnd = rawHtml.indexOf(QStringLiteral("</dl>"), 0, Qt::CaseInsensitive);
                if (dlEnd >= 0)
                    rawHtml = rawHtml.left(dlEnd).trimmed();
            }

            // Use ALL content between section boundaries.
            // Strip <dd> / </dd> tags (standard OpenJudge wrapper),
            // and also keep any content after </dd> that appears before
            // the next <dt> (to catch content placed outside <dd>).
            QRegularExpression ddContentRx(QStringLiteral("<dd[^>]*>(.*?)</dd>"),
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch ddMatch = ddContentRx.match(rawHtml);
            QString contentHtml;
            if (ddMatch.hasMatch()) {
                contentHtml = ddMatch.captured(1).trimmed();
                // Check for content after </dd> but before the next <dt>
                QString after = rawHtml.mid(ddMatch.capturedEnd()).trimmed();
                int dtPos = after.indexOf(QStringLiteral("<dt"), 0, Qt::CaseInsensitive);
                if (dtPos >= 0)
                    after = after.left(dtPos).trimmed();
                if (!after.isEmpty())
                    contentHtml += QStringLiteral("\n") + after;
            } else {
                contentHtml = rawHtml;
            }

            // Clean scripts/style but keep structural HTML (<p>, <pre>, etc.)
            contentHtml.replace(QRegularExpression(QStringLiteral("<script[^>]*>.*?</script>"),
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption), QString());
            contentHtml.replace(QRegularExpression(QStringLiteral("<style[^>]*>.*?</style>"),
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption), QString());

            detail.sections.append({headings[i].heading, contentHtml.trimmed()});

            debugLog(QStringLiteral("  section[%1] '%2': rawHtml len=%3, hasDd=%4, contentHtml len=%5, hasPre=%6")
                .arg(i).arg(headings[i].heading).arg(rawHtml.length()).arg(ddMatch.hasMatch())
                .arg(contentHtml.trimmed().length())
                .arg(contentHtml.contains(QStringLiteral("<pre"))));
        }
    }

    debugLog(QStringLiteral("  Strategy 1 (dt/dd) found %1 sections").arg(detail.sections.size()));

    // --- Strategy 2: <h3> pattern (fallback) ---
    if (detail.sections.isEmpty()) {
        debugLog(QStringLiteral("  Using Strategy 2 (h3)"));
        QRegularExpression h3Rx(QStringLiteral("<h3[^>]*>(.*?)</h3>"),
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator h3It = h3Rx.globalMatch(html);

        struct HPos2 { QString heading; qsizetype pos; };
        QList<HPos2> headings;

        while (h3It.hasNext()) {
            QRegularExpressionMatch m = h3It.next();
            QString heading = stripHtmlTags(m.captured(1)).trimmed();
            if (kSectionKeywords.contains(heading))
                headings.append({heading, m.capturedEnd()});
        }

        for (qsizetype i = 0; i < headings.size(); ++i) {
            qsizetype start = headings[i].pos;
            qsizetype end = (i + 1 < headings.size()) ? headings[i + 1].pos : html.length();
            QString contentHtml = html.mid(start, end - start).trimmed();

            // Truncate at </dl> to strip nav/footer content
            {
                int dlEnd = contentHtml.indexOf(QStringLiteral("</dl>"), 0, Qt::CaseInsensitive);
                if (dlEnd >= 0)
                    contentHtml = contentHtml.left(dlEnd).trimmed();
            }

            contentHtml.replace(QRegularExpression(QStringLiteral("<script[^>]*>.*?</script>"),
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption), QString());
            contentHtml.replace(QRegularExpression(QStringLiteral("<style[^>]*>.*?</style>"),
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption), QString());

            detail.sections.append({headings[i].heading, contentHtml.trimmed()});
        }
    }

    // Filter out unwanted sections (e.g. "来源")
    static const QStringList kSkipSections = { QStringLiteral("来源") };
    QList<ProblemSection> filtered;
    for (const auto &sec : detail.sections) {
        bool skip = false;
        for (const auto &kw : kSkipSections)
            if (sec.heading.contains(kw)) { skip = true; break; }
        if (!skip) filtered.append(sec);
    }
    detail.sections = filtered;

    debugLog(QStringLiteral("  final sections=%1").arg(detail.sections.size()));
    for (const auto &sec : detail.sections)
        debugLog(QStringLiteral("    '%1': len=%2 hasPre=%3")
            .arg(sec.heading).arg(sec.contentHtml.length())
            .arg(sec.contentHtml.contains(QStringLiteral("<pre"))));

    return detail;
}

// ======================================================================
// Submission API
// ======================================================================

void Crawler::submitCode(const QString &problemUrl, const QString &sourceCode, int languageId)
{
    debugLog(QStringLiteral("submitCode: problemUrl=%1 lang=%2").arg(problemUrl).arg(languageId));

    QString submitUrl = problemUrl;
    if (!submitUrl.endsWith(QLatin1Char('/')))
        submitUrl += QLatin1Char('/');
    submitUrl += QStringLiteral("submit/");

    QNetworkRequest request(submitUrl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, problemUrl, sourceCode, languageId]() {
        onSubmitPageFinished(reply, problemUrl, sourceCode, languageId);
    });
}

void Crawler::onSubmitPageFinished(QNetworkReply *reply, const QString &problemUrl,
                                    const QString &sourceCode, int languageId)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        debugLog(QStringLiteral("onSubmitPageFinished error: %1").arg(reply->errorString()));
        emit submissionFailed(QStringLiteral("无法访问提交页面: ") + reply->errorString());
        return;
    }

    QString html = QString::fromUtf8(reply->readAll());
    QString csrf = extractCsrfToken(html);

    debugLog(QStringLiteral("onSubmitPageFinished: CSRF found=%1").arg(!csrf.isEmpty()));

    // Log the submit page form fields to understand expected parameter names
    {
        QRegularExpression taRx(QStringLiteral("<textarea[^>]*name=\"([^\"]+)\""),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch taM = taRx.match(html);
        if (taM.hasMatch())
            debugLog(QStringLiteral("  textarea name='%1'").arg(taM.captured(1)));

        // Log ALL <select> elements and their options
        QRegularExpression selRx(QStringLiteral("<select[^>]*>(.*?)</select>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator selIt = selRx.globalMatch(html);
        while (selIt.hasNext()) {
            QRegularExpressionMatch selM = selIt.next();
            QRegularExpression nameRx(QStringLiteral("name=\"([^\"]+)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch nameM = nameRx.match(selM.captured(0));
            QString selName = nameM.hasMatch() ? nameM.captured(1) : "(unnamed)";
            debugLog(QStringLiteral("  select name='%1'").arg(selName));
            QRegularExpression optRx(QStringLiteral("<option[^>]*value=\"([^\"]*)\"[^>]*>([^<]+)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator optIt = optRx.globalMatch(selM.captured(0));
            while (optIt.hasNext()) {
                QRegularExpressionMatch optM = optIt.next();
                bool selected = optM.captured(0).contains(QLatin1String("selected"));
                debugLog(QStringLiteral("    option value='%1'%2 text='%3'")
                    .arg(optM.captured(1), selected ? QStringLiteral(" [SELECTED]") : QString(), optM.captured(2).trimmed()));
            }
        }

        // Log ALL <input> elements (not just hidden) - use a more flexible regex for multiline
        QRegularExpression inputRx(QStringLiteral("<input[^>]*>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator inputIt = inputRx.globalMatch(html);
        while (inputIt.hasNext()) {
            QRegularExpressionMatch iM = inputIt.next();
            QString tag = iM.captured(0);
            QString name, type, value;
            QRegularExpression nameRx(QStringLiteral("name\\s*=\\s*\"([^\"]+)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch nameM = nameRx.match(tag);
            if (nameM.hasMatch()) name = nameM.captured(1); else continue; // skip inputs without name
            QRegularExpression typeRx(QStringLiteral("type\\s*=\\s*\"([^\"]+)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch typeM = typeRx.match(tag);
            if (typeM.hasMatch()) type = typeM.captured(1);
            QRegularExpression valRx(QStringLiteral("value\\s*=\\s*\"([^\"]*)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch valM = valRx.match(tag);
            if (valM.hasMatch()) value = valM.captured(1);
            debugLog(QStringLiteral("  input name='%1' type='%2' value='%3'")
                .arg(name, type, value.left(40)));
        }

        // Find ALL forms (not just the submit form)
        QRegularExpression formRx(QStringLiteral("<form[^>]*action=\"([^\"]*)\"[^>]*method=\"([^\"]*)\""),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator formIt = formRx.globalMatch(html);
        while (formIt.hasNext()) {
            QRegularExpressionMatch formM = formIt.next();
            if (!formM.captured(1).contains(QLatin1String("search")))
                debugLog(QStringLiteral("  form action='%1' method=%2")
                    .arg(formM.captured(1)).arg(formM.captured(2)));
        }
    }
    // PHP-based OpenJudge (PHPSESSID) uses /api/solution/submitv2/ with base64-encoded source

    // Extract hidden fields and language radio from the submit form
    QString contestId, problemNumber, languageValue;
    {
        QRegularExpression ciRx(QStringLiteral("<input[^>]*name=\"contestId\"[^>]*value=\"([^\"]+)\""),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch ciM = ciRx.match(html);
        if (ciM.hasMatch()) contestId = ciM.captured(1);

        QRegularExpression pnRx(QStringLiteral("<input[^>]*name=\"problemNumber\"[^>]*value=\"([^\"]+)\""),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch pnM = pnRx.match(html);
        if (pnM.hasMatch()) problemNumber = pnM.captured(1);

        // Extract language radio button value (use checked one, or first one)
        // Log the raw HTML around language for debugging
        {
            qsizetype langPos = html.indexOf(QLatin1String("name=\"language\""), 0, Qt::CaseInsensitive);
            if (langPos < 0) langPos = html.indexOf(QLatin1String("name=language"), 0, Qt::CaseInsensitive);
            if (langPos >= 0) {
                qsizetype start = qMax(qsizetype(0), langPos - 60);
                qsizetype end = qMin(html.length(), langPos + 100);
                QString snippet = html.mid(start, end - start);
                snippet.replace(QLatin1Char('\n'), QLatin1Char(' '));
                snippet.replace(QLatin1Char('\r'), QLatin1Char(' '));
                debugLog(QStringLiteral("  language input raw HTML: ...%1...").arg(snippet.trimmed()));
            } else {
                debugLog(QStringLiteral("  'name=\"language\"' NOT FOUND in HTML"));
            }
        }
        QRegularExpression langRx(QStringLiteral(
            "<input[^>]*name=\"language\"[^>]*type=\"radio\"[^>]*value=\"([^\"]+)\"[^>]*>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator langIt = langRx.globalMatch(html);
        QString firstLangValue;
        while (langIt.hasNext()) {
            QRegularExpressionMatch langM = langIt.next();
            QString tag = langM.captured(0);
            QString val = langM.captured(1);
            if (firstLangValue.isEmpty()) firstLangValue = val;
            if (tag.contains(QLatin1String("checked"))) {
                languageValue = val;
                debugLog(QStringLiteral("  language radio (checked) value='%1'").arg(val));
            } else {
                debugLog(QStringLiteral("  language radio value='%1'").arg(val));
            }
        }
        if (languageValue.isEmpty()) {
            languageValue = firstLangValue;
            if (!languageValue.isEmpty())
                debugLog(QStringLiteral("  using first language radio value='%1'").arg(languageValue));
        }
        if (languageValue.isEmpty()) {
            // Fallback: more flexible regex - just find name="language" and capture value
            QRegularExpression fallbackRx(QStringLiteral("name=\"language\"[^>]*value=\"([^\"]+)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch fallbackM = fallbackRx.match(html);
            if (fallbackM.hasMatch()) {
                languageValue = fallbackM.captured(1);
                debugLog(QStringLiteral("  using fallback language value='%1'").arg(languageValue));
            }
        }
        if (languageValue.isEmpty()) {
            // Fallback: use caller-provided languageId
            debugLog(QStringLiteral("  no language radio found, using caller lang=%1").arg(languageId));
            languageValue = QString::number(languageId);
        }
    }

    // POST to the JSON API endpoint
    QString apiUrl = m_baseUrl + QStringLiteral("/api/solution/submitv2/");
    QNetworkRequest apiReq(apiUrl);
    apiReq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QByteArrayLiteral("application/x-www-form-urlencoded"));
    apiReq.setRawHeader("User-Agent", ua());
    apiReq.setRawHeader("X-Requested-With", QByteArrayLiteral("XMLHttpRequest"));
    apiReq.setRawHeader("Referer", problemUrl.toUtf8());

    // Build POST body manually to ensure proper percent-encoding.
    // Using QUrlQuery would leave '+' in base64 unencoded, but in
    // application/x-www-form-urlencoded '+' means space, corrupting the
    // source.  We also skip sourceEncode=base64 since some contest
    // instances (GCC/G++) fail to decode it, resulting in empty source.
    QByteArray postData;
    postData += "source=" + QUrl::toPercentEncoding(QString::fromUtf8(sourceCode.toUtf8()));
    postData += "&contestId=" + QUrl::toPercentEncoding(contestId);
    postData += "&problemNumber=" + QUrl::toPercentEncoding(problemNumber);
    postData += "&language=" + QUrl::toPercentEncoding(languageValue);
    debugLog(QStringLiteral("onSubmitPageFinished: POSTing code to %1 (contest=%2, problem=%3, lang=%4)")
        .arg(apiUrl, contestId, problemNumber, languageValue));
    debugLog(QStringLiteral("  raw POST data: %1").arg(QString::fromUtf8(postData.left(500))));

    QNetworkReply *postReply = m_manager->post(apiReq, postData);
    connect(postReply, &QNetworkReply::finished, this, [this, postReply, problemUrl]() {
        postReply->deleteLater();

        if (postReply->error() != QNetworkReply::NoError) {
            debugLog(QStringLiteral("submitCode POST error: %1").arg(postReply->errorString()));
            emit submissionFailed(QStringLiteral("提交失败: ") + postReply->errorString());
            return;
        }

        QByteArray responseData = postReply->readAll();
        debugLog(QStringLiteral("submitCode POST done: url=%1 response='%2'")
            .arg(postReply->url().toString(), QString::fromUtf8(responseData.left(300))));

        // Parse JSON response (PHP API returns JSON)
        QJsonParseError jsonErr;
        QJsonDocument doc = QJsonDocument::fromJson(responseData, &jsonErr);
        if (jsonErr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj.value(QStringLiteral("result")).toString();
            debugLog(QStringLiteral("submitCode API: result=%1").arg(result));
            if (result == QStringLiteral("SUCCESS")) {
                // Use the redirect URL from API response to track the submission
                QString redirectUrl = obj.value(QStringLiteral("redirect")).toString();
                if (!redirectUrl.isEmpty()) {
                    m_pollStatusUrl = redirectUrl;
                    debugLog(QStringLiteral("submitCode: using redirect URL for polling: %1").arg(m_pollStatusUrl));
                } else {
                    // Fallback: construct status URL
                    QUrl base(problemUrl);
                    QString path = base.path();
                    while (path.endsWith(QLatin1Char('/')))
                        path.chop(1);
                    int lastSlash = path.lastIndexOf(QLatin1Char('/'));
                    if (lastSlash > 0)
                        path = path.left(lastSlash);
                    m_pollStatusUrl = base.scheme() + QStringLiteral("://") + base.host() + path + QStringLiteral("/status/");
                    debugLog(QStringLiteral("submitCode: constructed statusUrl=%1").arg(m_pollStatusUrl));
                }
                m_pollCount = 0;
                m_pendingRunId.clear();
                m_pendingCeUrl.clear();
                if (!m_pollTimer) {
                    m_pollTimer = new QTimer(this);
                    connect(m_pollTimer, &QTimer::timeout, this, &Crawler::doPollSubmissionStatus);
                }
                doPollSubmissionStatus();
                m_pollTimer->start(ConfigManager::instance().openJudgePollIntervalMs());
                return;
            } else {
                QString msg = obj.value(QStringLiteral("message")).toString();
                if (msg.isEmpty()) msg = QStringLiteral("提交被拒绝");
                debugLog(QStringLiteral("submitCode API ERROR: %1").arg(msg));
                emit submissionFailed(msg);
                return;
            }
        }

        // Fallback: treat as HTML (old Django behavior)
        QString responseHtml = QString::fromUtf8(responseData);
        QString statusUrl;
        if (postReply->url().path().contains(QLatin1String("status")))
            statusUrl = postReply->url().toString();

        if (statusUrl.isEmpty()) {
            QUrl base(problemUrl);
            QString path = base.path();
            while (path.endsWith(QLatin1Char('/')))
                path.chop(1);
            int lastSlash = path.lastIndexOf(QLatin1Char('/'));
            if (lastSlash > 0)
                path = path.left(lastSlash);
            statusUrl = base.scheme() + QStringLiteral("://") + base.host() + path + QStringLiteral("/status/");
        }

        debugLog(QStringLiteral("submitCode: statusUrl=%1").arg(statusUrl));

        m_pollStatusUrl = statusUrl;
        m_pollCount = 0;
        m_pendingRunId.clear();
        m_pendingCeUrl.clear();

        if (!m_pollTimer) {
            m_pollTimer = new QTimer(this);
            connect(m_pollTimer, &QTimer::timeout, this, &Crawler::doPollSubmissionStatus);
        }

        doPollSubmissionStatus();
        m_pollTimer->start(ConfigManager::instance().openJudgePollIntervalMs());
    });
}

void Crawler::fetchSubmissionStatus(const QString &statusPageUrl)
{
    debugLog(QStringLiteral("fetchSubmissionStatus: %1").arg(statusPageUrl));

    QNetworkRequest request(statusPageUrl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            debugLog(QStringLiteral("fetchSubmissionStatus error: %1").arg(reply->errorString()));
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());

        QRegularExpression trRx(QStringLiteral("<tr[^>]*>(.*?)</tr>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator trIt = trRx.globalMatch(html);

        SubmissionResult result;
        bool found = false;

        while (trIt.hasNext()) {
            QRegularExpressionMatch trMatch = trIt.next();
            QString rowHtml = trMatch.captured(1);

            QRegularExpression tdRx(QStringLiteral("<td[^>]*>(.*?)</td>"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatchIterator tdIt = tdRx.globalMatch(rowHtml);

            QStringList cells;
            while (tdIt.hasNext()) {
                QRegularExpressionMatch tdMatch = tdIt.next();
                cells.append(tdMatch.captured(1).trimmed());
            }

            if (cells.size() < 6)
                continue;

            // cells[0] = submit-user (format "studentID-name")
            // cells[1] = class, cells[2] = title, cells[3] = result
            // cells[4] = memory, cells[5] = time
            QString fullUser = stripHtmlTags(cells.value(0)).trimmed();
            // Extract student ID from "2500012947-宋睿宇(dxz)" format
            QString userId = fullUser.section(QLatin1Char('-'), 0, 0).trimmed();
            if (userId.isEmpty())
                continue;

            if (!m_username.isEmpty() && userId != m_username)
                continue;

            QString resultHtml = cells.value(3);
            QString resultText = stripHtmlTags(resultHtml).trimmed();

            if (resultText.contains(QStringLiteral("Pending"))
                || resultText.contains(QStringLiteral("Judging"))
                || resultText.contains(QStringLiteral("Waiting")))
                continue;

            found = true;

            // Extract run ID from solution link: /hw202613/solution/52509098/
            {
                static const QRegularExpression solRx(QStringLiteral("/solution/(\\d+)/"));
                QRegularExpressionMatch solMatch = solRx.match(resultHtml);
                if (solMatch.hasMatch())
                    result.runId = solMatch.captured(1);
            }

            result.status = resultText;

            QString timeStr = stripHtmlTags(cells.value(5)).trimmed();
            timeStr.remove(QStringLiteral("ms"));
            bool ok = false;
            int t = timeStr.toInt(&ok);
            if (ok) result.timeMs = t;

            QString memStr = stripHtmlTags(cells.value(4)).trimmed();
            memStr.remove(QStringLiteral("kB"), Qt::CaseInsensitive);
            ok = false;
            int m = memStr.toInt(&ok);
            if (ok) result.memoryKb = m;

            QRegularExpression ceLinkRx(QStringLiteral("<a\\s+href=\"([^\"]+compile[^\"-]*error[^\"]*)\""),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch ceMatch = ceLinkRx.match(resultHtml);
            if (ceMatch.hasMatch()) {
                m_pendingCeUrl = ceMatch.captured(1);
                if (m_pendingCeUrl.startsWith(QLatin1Char('/')))
                    m_pendingCeUrl = m_baseUrl + m_pendingCeUrl;
                debugLog(QStringLiteral("CE url found: %1").arg(m_pendingCeUrl));
            }

            break;
        }

        if (found) {
            if (!m_pendingCeUrl.isEmpty()) {
                QNetworkRequest ceReq(m_pendingCeUrl);
                ceReq.setRawHeader("User-Agent", ua());
                ceReq.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());
                QNetworkReply *ceReply = m_manager->get(ceReq);
                connect(ceReply, &QNetworkReply::finished, this, [this, ceReply, result]() mutable {
                    onCompileErrorPageFinished(ceReply, result);
                });
            } else {
                emit submissionResultReady(result);
            }
        }
    });
}

void Crawler::doPollSubmissionStatus()
{
    m_pollCount++;
    debugLog(QStringLiteral("doPollSubmissionStatus: attempt %1/%2")
        .arg(m_pollCount).arg(ConfigManager::instance().openJudgeMaxPollAttempts()));

    if (m_pollCount > ConfigManager::instance().openJudgeMaxPollAttempts()) {
        m_pollTimer->stop();
        emit submitPollTimeout();
        return;
    }

    QNetworkRequest request(m_pollStatusUrl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            debugLog(QStringLiteral("doPollSubmissionStatus network error: %1").arg(reply->errorString()));
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());

        // Extract body text (strip HTML) for parsing
        QString text;
        {
            QRegularExpression bodyRx(QStringLiteral("<body[^>]*>(.*)</body>"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
            QRegularExpressionMatch bodyM = bodyRx.match(html);
            if (bodyM.hasMatch()) {
                QString bodyContent = bodyM.captured(1);
                text = bodyContent;
                text.replace(QRegularExpression(QStringLiteral("<script[^>]*>.*?</script>")), QString());
                text.replace(QRegularExpression(QStringLiteral("<style[^>]*>.*?</style>")), QString());
                text.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QString());
                text = decodeHtmlEntities(text);
                text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
                text = text.trimmed();
                debugLog(QStringLiteral("  BODY_TEXT=%1").arg(text.left(500)));
            } else {
                debugLog(QStringLiteral("  BODY not found in HTML"));
            }
        }

        SubmissionResult result;
        bool found = false;

        // Parse solution page text for submission status
        // Expected format: "状态: Accepted" or "状态: Waiting"
        {
            // Status is ASCII text (e.g. "Accepted", "Compile Error") - stop at Chinese char
            QRegularExpression statusRx(QStringLiteral(u"状态:\\s*([A-Za-z\\s]+?)\\s+[^A-Za-z]"));
            QRegularExpressionMatch statusM = statusRx.match(text);
            if (statusM.hasMatch()) {
                QString statusText = statusM.captured(1).trimmed();

                debugLog(QStringLiteral("  raw status='%1'").arg(statusText));

                // Check if still pending
                if (statusText.contains(QStringLiteral("Waiting"))
                    || statusText.contains(QStringLiteral("Judging"))
                    || statusText.contains(QStringLiteral("Pending"))) {
                    // Still pending — timer will fire again
                    return;
                }

                result.status = statusText;
                found = true;
            } else {
                debugLog(QStringLiteral("  status not found in text"));
            }
        }

        if (found) {
            // Extract Run ID from URL: /solution/NUMBER/
            {
                QRegularExpression runIdRx(QStringLiteral("/solution/(\\d+)/"));
                QRegularExpressionMatch runIdM = runIdRx.match(html);
                if (runIdM.hasMatch())
                    result.runId = runIdM.captured(1);
            }

            // Extract time: "时间: 23ms"
            {
                QRegularExpression timeRx(QStringLiteral(u"时间:\\s*(\\d+)\\s*ms"));
                QRegularExpressionMatch timeM = timeRx.match(text);
                if (timeM.hasMatch())
                    result.timeMs = timeM.captured(1).toInt();
            }

            // Extract memory: "内存: 7272kB"
            {
                QRegularExpression memRx(QStringLiteral(u"内存:\\s*(\\d+)\\s*kB"));
                QRegularExpressionMatch memM = memRx.match(text);
                if (memM.hasMatch())
                    result.memoryKb = memM.captured(1).toInt();
            }

            // Check for Compile Error link
            {
                QRegularExpression ceLinkRx(QStringLiteral("compile[^\"\\s]*error[^\"\\s]*",
                    QRegularExpression::CaseInsensitiveOption));
                QRegularExpressionMatch ceMatch = ceLinkRx.match(html);
                if (ceMatch.hasMatch()) {
                    QRegularExpression hrefRx(QStringLiteral("href=\"([^\"]*%1[^\"]*)\"").arg(ceMatch.captured(0)),
                        QRegularExpression::CaseInsensitiveOption);
                    QRegularExpressionMatch hrefM = hrefRx.match(html);
                    if (hrefM.hasMatch()) {
                        m_pendingCeUrl = hrefM.captured(1);
                        if (m_pendingCeUrl.startsWith(QLatin1Char('/')))
                            m_pendingCeUrl = m_baseUrl + m_pendingCeUrl;
                        debugLog(QStringLiteral("  CE url=%1").arg(m_pendingCeUrl));
                    }
                }
            }

            m_pollTimer->stop();
            if (!m_pendingCeUrl.isEmpty()) {
                QNetworkRequest ceReq(m_pendingCeUrl);
                ceReq.setRawHeader("User-Agent", ua());
                ceReq.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());
                QNetworkReply *ceReply = m_manager->get(ceReq);
                connect(ceReply, &QNetworkReply::finished, this, [this, ceReply, result]() mutable {
                    onCompileErrorPageFinished(ceReply, result);
                });
            } else {
                emit submissionResultReady(result);
            }
        }
    });
}

void Crawler::onCompileErrorPageFinished(QNetworkReply *reply, SubmissionResult &result)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        debugLog(QStringLiteral("onCompileErrorPageFinished error: %1").arg(reply->errorString()));
        emit submissionResultReady(result);
        return;
    }

    QString html = QString::fromUtf8(reply->readAll());
    debugLog(QStringLiteral("onCompileErrorPageFinished: html len=%1").arg(html.length()));

    QRegularExpression preRx(QStringLiteral("<pre[^>]*>(.*?)</pre>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch preMatch = preRx.match(html);
    if (preMatch.hasMatch()) {
        result.compileError = decodeHtmlEntities(preMatch.captured(1).trimmed());
    } else {
        result.compileError = stripHtmlTags(html);
    }

    debugLog(QStringLiteral("CE error text: %1 chars").arg(result.compileError.size()));
    emit submissionResultReady(result);
}

void Crawler::fetchCompileError(const QString &ceUrl)
{
    QNetworkRequest request(ceUrl);
    request.setRawHeader("User-Agent", ua());
    request.setTransferTimeout(ConfigManager::instance().openJudgeTransferTimeoutMs());

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit submissionFailed(QStringLiteral("获取编译错误详情失败: ") + reply->errorString());
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());
        SubmissionResult result;
        QRegularExpression preRx(QStringLiteral("<pre[^>]*>(.*?)</pre>"),
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch preMatch = preRx.match(html);
        if (preMatch.hasMatch())
            result.compileError = decodeHtmlEntities(preMatch.captured(1).trimmed());
        else
            result.compileError = stripHtmlTags(html);

        emit submissionResultReady(result);
    });
}

void Crawler::stopPolling()
{
    if (m_pollTimer)
        m_pollTimer->stop();
    m_pollCount = 0;
}

void Crawler::clearCookies()
{
    m_manager->setCookieJar(new QNetworkCookieJar(m_manager));
}
