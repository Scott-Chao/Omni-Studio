#include "searchpanel.h"
#include "thememanager.h"
#include "utilities.h"
#include "configmanager.h"
#include "settingsmanager.h"

#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QListWidgetItem>
#include <QLabel>
#include <QThread>

SearchPanel::SearchPanel(QWidget *parent)
    : QWidget(parent)
{
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(tr("搜索文件..."));
    m_searchInput->setClearButtonEnabled(true);

    m_resultList = new QListWidget(this);
    m_resultList->setSelectionMode(QAbstractItemView::NoSelection);
    m_resultList->setWordWrap(true);
    m_resultList->setSpacing(2);

    m_statusLabel = new QLabel(tr("打开文件夹以搜索文件"), this);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(ConfigManager::instance().searchDebounceMs());

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_searchInput);
    layout->addWidget(m_resultList);
    layout->addWidget(m_statusLabel);

    connect(m_searchInput, &QLineEdit::textChanged,
            this, &SearchPanel::onSearchTextChanged);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &SearchPanel::performSearch);
    connect(m_resultList, &QListWidget::itemClicked,
            this, &SearchPanel::onResultItemClicked);

    ThemeManager::watchTheme(this, &SearchPanel::refreshStyle);
    refreshStyle();
}

SearchPanel::~SearchPanel()
{
    if (m_searchCancelled)
        m_searchCancelled->store(true);
}

void SearchPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QString("SearchPanel { background-color: %1; }")
        .arg(tm.color("sideBar.background").name()));

    m_searchInput->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 3px 6px; }")
        .arg(tm.color("input.background").name(),
             tm.color("input.foreground").name(),
             tm.color("input.border").name()));

    m_resultList->setStyleSheet(QString(
        "QListWidget { background: %1; color: %2; border: none; }"
        "QListWidget::item { padding: 2px 0; }"
        "QListWidget::item:hover { background: %3; }")
        .arg(tm.color("sideBar.background").name(),
             tm.color("sideBar.foreground").name(),
             tm.color("list.hoverBackground").name()));

    m_statusLabel->setStyleSheet(QString("color: %1; padding: 4px 8px; font-size: 12px;")
        .arg(tm.color("editorLineNumber.foreground").name()));

    // Update result item child widget styles
    for (int i = 0; i < m_resultList->count(); ++i) {
        QWidget *w = m_resultList->itemWidget(m_resultList->item(i));
        if (!w) continue;
        QLabel *title = w->findChild<QLabel*>("searchResultTitle");
        if (title) {
            title->setStyleSheet(QString("font-weight: bold; font-size: 12px; color: %1;")
                .arg(tm.color("sideBar.foreground").name()));
        }
        QLabel *snippet = w->findChild<QLabel*>("searchResultSnippet");
        if (snippet) {
            snippet->setStyleSheet(QString("color: %1; font-size: 11px;")
                .arg(tm.color("tab.inactiveForeground").name()));
        }
    }
}

void SearchPanel::setRootPath(const QString &path)
{
    m_rootPath = path;
    m_resultList->clear();
    m_results.clear();

    if (m_rootPath.isEmpty() || m_rootPath == QDir::rootPath()) {
        m_statusLabel->setText(tr("打开文件夹以搜索文件"));
        return;
    }

    if (!m_searchInput->text().trimmed().isEmpty()) {
        performSearch();
    } else {
        m_statusLabel->setText(tr("输入关键词以搜索"));
    }
}

void SearchPanel::clearSearch()
{
    m_searchInput->clear();
    m_resultList->clear();
    m_results.clear();
    m_searchText.clear();
    m_statusLabel->setText(tr("输入关键词以搜索"));
}

void SearchPanel::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void SearchPanel::onSearchTextChanged()
{
    m_debounceTimer->start();
    emit searchTextChanged();
}

void SearchPanel::performSearch()
{
    QString rawText = m_searchInput->text().trimmed();
    m_searchText = rawText.toLower();

    m_resultList->clear();
    m_results.clear();

    if (m_searchText.isEmpty()) {
        m_statusLabel->setText(tr("输入关键词以搜索"));
        return;
    }

    if (m_rootPath.isEmpty() || m_rootPath == QDir::rootPath()) {
        m_statusLabel->setText(tr("Open a folder to search files"));
        return;
    }

    // Cancel any in-flight search
    if (m_searchCancelled)
        m_searchCancelled->store(true);

    m_searchCancelled = std::make_shared<std::atomic<bool>>(false);
    uint64_t searchId = ++m_searchId;

    m_statusLabel->setText(tr("搜索中..."));

    // Capture all needed data by value for the worker thread
    QString searchText = m_searchText;
    QString rootPath = m_rootPath;
    auto &sm = SettingsManager::instance();
    const auto &cfg = ConfigManager::instance();
    int maxPerFile = sm.value("search_panel.max_per_file", cfg.searchMaxPerFile()).toInt();
    int maxTotalResults = sm.value("search_panel.max_total_results", cfg.searchMaxTotalResults()).toInt();
    int snippetMaxLen = sm.value("search_panel.snippet_max_length", cfg.searchSnippetMaxLength()).toInt();
    auto cancelled = m_searchCancelled;

    QThread::create([this, cancelled, searchId, searchText, rootPath,
                     maxPerFile, maxTotalResults, snippetMaxLen]() {
        QStringList files;
        collectTextFiles(rootPath, files);

        if (cancelled->load()) return;

        QVector<SearchResult> batch;
        int totalResults = 0;

        for (const QString &filePath : files) {
            if (cancelled->load()) return;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            int lineNum = 0;
            int fileMatches = 0;

            while (!in.atEnd() && fileMatches < maxPerFile
                   && totalResults < maxTotalResults) {
                if (cancelled->load()) {
                    file.close();
                    return;
                }

                QString line = in.readLine();
                ++lineNum;

                if (line.toLower().contains(searchText)) {
                    SearchResult result;
                    result.filePath = filePath;
                    result.lineNumber = lineNum;
                    result.snippet = extractSnippet(line, searchText,
                                                    snippetMaxLen);

                    batch.append(result);
                    ++fileMatches;
                    ++totalResults;
                }
            }
            file.close();

            if (totalResults >= maxTotalResults)
                break;
        }

        if (cancelled->load()) return;

        // Deliver results to main thread
        QMetaObject::invokeMethod(this, [this, searchId,
                                         batch = std::move(batch)]() mutable {
            if (searchId != m_searchId.load()) return;

            for (const auto &r : batch) {
                m_results.append(r);
                addResultItem(r);
            }
            updateStatusLabel();
        }, Qt::QueuedConnection);
    })->start();
}

void SearchPanel::collectTextFiles(const QString &rootPath,
                                   QStringList &outFiles)
{
    QDirIterator it(rootPath, TextFileUtils::scanNameFilters(),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        outFiles.append(it.next());
    }
}

QString SearchPanel::extractSnippet(const QString &line,
                                         const QString &searchText,
                                         int maxLen)
{
    QString trimmed = line.trimmed();
    if (trimmed.length() > maxLen) {
        int matchIdx = trimmed.toLower().indexOf(searchText);
        int contextLen = (maxLen - searchText.length()) / 2;
        if (matchIdx >= 0) {
            int start = qMax(0, matchIdx - contextLen);
            int end = qMin(trimmed.length(),
                           matchIdx + searchText.length() + contextLen);
            QString snippet = trimmed.mid(start, end - start);
            if (start > 0) snippet.prepend("...");
            if (end < trimmed.length()) snippet.append("...");
            return snippet;
        }
        return trimmed.left(maxLen) + "...";
    }
    return trimmed;
}

QString SearchPanel::extractSnippet(const QString &line) const
{
    int maxLen = SettingsManager::instance().value("search_panel.snippet_max_length",
                   ConfigManager::instance().searchSnippetMaxLength()).toInt();
    return extractSnippet(line, m_searchText, maxLen);
}

void SearchPanel::addResultItem(const SearchResult &result)
{
    QFileInfo fi(result.filePath);

    QWidget *itemWidget = new QWidget();
    itemWidget->setAutoFillBackground(true);

    QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);
    itemLayout->setContentsMargins(6, 3, 6, 3);
    itemLayout->setSpacing(1);

    QLabel *titleLabel = new QLabel(
        QString("%1  (第 %2 行)")
            .arg(fi.fileName())
            .arg(result.lineNumber));
    titleLabel->setObjectName("searchResultTitle");
    {
        auto &tm = ThemeManager::instance();
        titleLabel->setStyleSheet(QString("font-weight: bold; font-size: 12px; color: %1;")
            .arg(tm.color("sideBar.foreground").name()));
    }

    QLabel *snippetLabel = new QLabel(result.snippet);
    snippetLabel->setObjectName("searchResultSnippet");
    {
        auto &tm = ThemeManager::instance();
        snippetLabel->setStyleSheet(QString("color: %1; font-size: 11px;")
            .arg(tm.color("tab.inactiveForeground").name()));
    }
    snippetLabel->setWordWrap(true);

    itemLayout->addWidget(titleLabel);
    itemLayout->addWidget(snippetLabel);

    QListWidgetItem *listItem = new QListWidgetItem();
    listItem->setData(Qt::UserRole, result.filePath);
    listItem->setData(Qt::UserRole + 1, result.lineNumber);

    int titleHeight = titleLabel->sizeHint().height();
    snippetLabel->setFixedWidth(m_resultList->viewport()->width() - 24);
    int snippetHeight = snippetLabel->sizeHint().height();
    listItem->setSizeHint(QSize(0, titleHeight + snippetHeight + 10));

    m_resultList->addItem(listItem);
    m_resultList->setItemWidget(listItem, itemWidget);
}

void SearchPanel::onResultItemClicked(QListWidgetItem *item)
{
    QString filePath = item->data(Qt::UserRole).toString();
    int lineNumber = item->data(Qt::UserRole + 1).toInt();
    if (!filePath.isEmpty()) {
        emit resultClicked(filePath, lineNumber, m_searchText);
    }
}

void SearchPanel::updateStatusLabel()
{
    int count = m_results.size();
    QString rawText = m_searchInput->text().trimmed();
    if (count == 0) {
        m_statusLabel->setText(tr("未找到 \"%1\" 的结果").arg(rawText));
    } else {
        m_statusLabel->setText(
            tr("找到 %1 个 \"%2\" 的结果")
                .arg(count)
                .arg(rawText));
    }
}
