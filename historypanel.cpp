#include <historypanel.h>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QDir>
#include <utility>
#include <QMessageBox>

HistoryPanel::HistoryPanel(SettingsManager *settings, QWidget *parent)
    : QWidget(parent), m_settings(settings) {
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    connect(m_listWidget, &QListWidget::itemClicked, this, &HistoryPanel::onItemClicked);

    m_clearButton = new QPushButton(tr("清空历史记录"), this);
    m_clearButton->setStyleSheet("color: red");
    connect(m_clearButton, &QPushButton::clicked, this, &HistoryPanel::clearHistory);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);
    layout->addWidget(m_clearButton);
}

void HistoryPanel::addFile(const QString &rawPath)
{
    QString filePath = QDir::cleanPath(QFileInfo(rawPath).absoluteFilePath());
    if (filePath.isEmpty()) return;

    // 从数据和 UI 中彻底清除所有同名项（从后往前，防止索引变化）
    for (int i = m_filePaths.size() - 1; i >= 0; --i) {
        if (QString::compare(m_filePaths.at(i), filePath, Qt::CaseInsensitive) == 0) {
            m_filePaths.removeAt(i);
            delete m_listWidget->takeItem(i);
        }
    }

    // 插入到顶部
    m_filePaths.prepend(filePath);
    QListWidgetItem *item = new QListWidgetItem(QFileInfo(filePath).fileName());
    item->setToolTip(filePath);
    item->setData(Qt::UserRole, filePath);
    m_listWidget->insertItem(0, item);

    // 限制数量并持久化
    while (m_filePaths.size() > MaxHistorySize) {
        m_filePaths.removeLast();
        delete m_listWidget->takeItem(m_listWidget->count() - 1);
    }
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

    m_listWidget->clear();
    for (const QString &path : std::as_const(m_filePaths)) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        m_listWidget->addItem(item); // 顶部是第一个元素
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

void HistoryPanel::clearHistory()
{
    // 弹出确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("确认清空"),
        tr("确定要清空所有历史记录吗？此操作不可撤销。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No); // 默认按钮为 No

    if (reply == QMessageBox::Yes) {
        m_filePaths.clear();
        m_listWidget->clear();
        m_settings->setRecentFiles(QStringList());
    }
}
