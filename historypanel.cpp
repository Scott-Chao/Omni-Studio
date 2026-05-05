#include <historypanel.h>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QDir>
#include <utility>

HistoryPanel::HistoryPanel(SettingsManager *settings, QWidget *parent)
    : QWidget(parent), m_settings(settings) {
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_listWidget, &QListWidget::itemClicked, this, &HistoryPanel::onItemClicked);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);
}

void HistoryPanel::addFile(const QString &rawPath)
{
    QString filePath = QDir::cleanPath(QFileInfo(rawPath).absoluteFilePath());
    if (filePath.isEmpty()) return;

    // 移除所有重复项（仅维护数据）
    m_filePaths.erase(
        std::remove_if(m_filePaths.begin(), m_filePaths.end(),
                       [&filePath](const QString &p) {
                           return QString::compare(p, filePath, Qt::CaseInsensitive) == 0;
                       }),
        m_filePaths.end());

    m_filePaths.prepend(filePath); // 插入到顶部

    // 限制最大条数
    while (m_filePaths.size() > MaxHistorySize)
        m_filePaths.removeLast();

    rebuildList(); // 完全重建 UI（保证与数据一致）
    saveHistory();
}

void HistoryPanel::loadHistory()
{
    m_filePaths.clear();
    const QStringList files = m_settings->recentFiles();
    for (const QString &path : files) {
        QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        if (!QFileInfo::exists(cleanPath))
            continue;
        if (m_filePaths.contains(cleanPath, Qt::CaseInsensitive))
            continue;
        m_filePaths.append(cleanPath);
    }
    rebuildList();
}

void HistoryPanel::rebuildList()
{
    m_listWidget->clear();
    for (const QString &path : std::as_const(m_filePaths)) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        m_listWidget->addItem(item); // 最新在上
    }
}
void HistoryPanel::saveHistory() {
    m_settings->setRecentFiles(m_filePaths);
}

void HistoryPanel::onItemClicked(QListWidgetItem *item) {
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        emit fileClicked(path);
}
