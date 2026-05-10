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

static const QByteArray kUserAgent = QByteArrayLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

// Debug logging helper – call clearLog() at startup, then debugLog() throughout
static void debugLog(const QString &msg)
{
    QFile file(QStringLiteral("crawler_debug.log"));
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz ")) << msg << "\n";
    }
}

static void clearLog()
{
    QFile file(QStringLiteral("crawler_debug.log"));
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.close();
}

Crawler::Crawler(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl(QStringLiteral("http://cxsjsx.openjudge.cn"))
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

    QUrl url(m_baseUrl + "/login/");
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onLoginPageFinished(reply);
    });
}

void Crawler::fetchMainPage()
{
    QUrl url(m_baseUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onMainPageFinished(reply);
    });
}

void Crawler::fetchHomeworkProblems(const QString &url)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setTransferTimeout(15000);

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
    request.setRawHeader("User-Agent", kUserAgent);
    request.setTransferTimeout(30000);

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
    request.setRawHeader("User-Agent", kUserAgent);
    request.setTransferTimeout(15000);

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
        QUrl url(m_baseUrl);
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", kUserAgent);

        QNetworkReply *mainReply = m_manager->get(request);
        connect(mainReply, &QNetworkReply::finished, this, [this, mainReply]() {
            mainReply->deleteLater();
            QString html = QString::fromUtf8(mainReply->readAll());
            QString csrf = extractCsrfToken(html);

            if (!csrf.isEmpty()) {
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("username"), m_username);
                query.addQueryItem(QStringLiteral("password"), m_password);
                query.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), csrf);

                QUrl loginUrl(m_baseUrl + "/login/");
                QNetworkRequest postReq(loginUrl);
                postReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                  QByteArrayLiteral("application/x-www-form-urlencoded"));
                postReq.setRawHeader("User-Agent", kUserAgent);
                postReq.setRawHeader("Referer", m_baseUrl.toUtf8());

                QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
                QNetworkReply *lr = m_manager->post(postReq, postData);
                connect(lr, &QNetworkReply::finished, this, [this, lr]() {
                    onLoginFinished(lr);
                });
            } else {
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

    QString html = QString::fromUtf8(reply->readAll());
    QString csrf = extractCsrfToken(html);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("username"), m_username);
    query.addQueryItem(QStringLiteral("password"), m_password);
    query.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), csrf);

    QUrl loginUrl(m_baseUrl + "/login/");
    QNetworkRequest request(loginUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", kUserAgent);
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

    QUrl url(m_baseUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);

    QNetworkReply *mainReply = m_manager->get(request);
    connect(mainReply, &QNetworkReply::finished, this, [this, mainReply]() {
        onMainPageFinished(mainReply);
    });
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

    if (redirectedUrl.path().contains(QLatin1String("/login"))
        || html.contains(QStringLiteral("请输入用户名"))
        || html.contains(QStringLiteral("登录")))
    {
        emit loginFailed(QStringLiteral("登录失败，请检查用户名和密码"));
        return;
    }

    emit loginSuccess();

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
