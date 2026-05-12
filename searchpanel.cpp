#include "searchpanel.h"
#include "fileutils.h"
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
    m_statusLabel->setStyleSheet(QString("color: #888; padding: 4px 8px; font-size: 12px;"));

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

    m_statusLabel->setText(tr("搜索中..."));

    QStringList files;
    collectTextFiles(m_rootPath, files);

    int totalResults = 0;

    for (const QString &filePath : files) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&file);
        int lineNum = 0;
        int fileMatches = 0;

        auto &sm = SettingsManager::instance();
        const auto &cfg = ConfigManager::instance();
        int maxPerFile = sm.value("search_panel.max_per_file", cfg.searchMaxPerFile()).toInt();
        int maxTotalResults = sm.value("search_panel.max_total_results", cfg.searchMaxTotalResults()).toInt();
        while (!in.atEnd() && fileMatches < maxPerFile
               && totalResults < maxTotalResults) {
            QString line = in.readLine();
            ++lineNum;

            if (line.toLower().contains(m_searchText)) {
                SearchResult result;
                result.filePath = filePath;
                result.lineNumber = lineNum;
                result.snippet = extractSnippet(line);

                m_results.append(result);
                addResultItem(result);
                ++fileMatches;
                ++totalResults;
            }
        }
        file.close();

        if (totalResults >= maxTotalResults)
            break;
    }

    updateStatusLabel();
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

QString SearchPanel::extractSnippet(const QString &line) const
{
    QString trimmed = line.trimmed();
    int maxLen = SettingsManager::instance().value("search_panel.snippet_max_length",
                   ConfigManager::instance().searchSnippetMaxLength()).toInt();
    if (trimmed.length() > maxLen) {
        int matchIdx = trimmed.toLower().indexOf(m_searchText);
        int contextLen = (maxLen - m_searchText.length()) / 2;
        if (matchIdx >= 0) {
            int start = qMax(0, matchIdx - contextLen);
            int end = qMin(trimmed.length(),
                           matchIdx + m_searchText.length() + contextLen);
            QString snippet = trimmed.mid(start, end - start);
            if (start > 0) snippet.prepend("...");
            if (end < trimmed.length()) snippet.append("...");
            return snippet;
        }
        return trimmed.left(maxLen) + "...";
    }
    return trimmed;
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
    titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));

    QLabel *snippetLabel = new QLabel(result.snippet);
    snippetLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 11px;"));
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
