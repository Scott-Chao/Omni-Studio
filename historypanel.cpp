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
    m_clearButton->setStyleSheet(
        "QPushButton { color: #E74C3C; background: transparent; border: none; padding: 4px 8px; }"
        "QPushButton:hover { color: #FF6B5A; }");
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

void HistoryPanel::removeFile(const QString &rawPath)
{
    QString filePath = QDir::cleanPath(QFileInfo(rawPath).absoluteFilePath());
    if (filePath.isEmpty()) return;

    // 是否精确匹配文件或是否为文件夹前缀
    for (int i = m_filePaths.size() - 1; i >= 0; --i) {
        const QString &cur = m_filePaths.at(i);
        if (cur == filePath || cur.startsWith(filePath + "/")) {
            m_filePaths.removeAt(i);
            delete m_listWidget->takeItem(i);
        }
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

void HistoryPanel::replacePath(const QString &oldBase, const QString &newBase)
{
    const QString oldNorm = QDir::cleanPath(oldBase);
    const QString newNorm = QDir::cleanPath(newBase);

    bool changed = false;
    for (int i = 0; i < m_filePaths.size(); ++i) {
        const QString &cur = m_filePaths.at(i);
        if (cur == oldNorm) {
            m_filePaths[i] = newNorm;
            changed = true;
        } else if (cur.startsWith(oldNorm + "/")) {
            m_filePaths[i] = newNorm + cur.mid(oldNorm.length());
            changed = true;
        }
    }

    if (changed) {
        // 完全重建 UI 列表，确保与 m_filePaths 严格一致
        m_listWidget->clear();
        for (const QString &path : std::as_const(m_filePaths)) {
            QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
            m_listWidget->addItem(item);
        }
    }
}
