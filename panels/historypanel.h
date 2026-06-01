#include <QWidget>
#include <QListWidget>
#include <QSettings>
#include <QPushButton>
#include "config/settingsmanager.h"

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(SettingsManager *settings, QWidget *parent = nullptr);
    void addFile(const QString &filePath); // 加入历史并刷新列表
    void removeFile(const QString &filePath); // 移除单个或多个历史记录
    void loadHistory(); // 从配置加载并显示
    void saveHistory(); // 持久化到配置
    void replacePath(const QString &oldBase, const QString &newBase);

signals:
    void fileClicked(const QString &filePath); // 用户点击历史文件

private slots:
    void onItemClicked(QListWidgetItem *item);
    void clearHistory();

private:
    void refreshStyle();
    QListWidget *m_listWidget;
    QStringList m_filePaths; // 按显示顺序的完整路径
    SettingsManager *m_settings;
    static const int MaxHistorySize = 50;
    QPushButton *m_clearButton; // 清空按钮
};
