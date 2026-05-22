#ifndef TAGPANEL_H
#define TAGPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

class TagPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TagPanel(QWidget *parent = nullptr);

    void showAllTags(const QStringList &tags);
    void showFilesForTag(const QString &tag, const QStringList &files);

signals:
    void fileClicked(const QString &filePath);
    void tagClicked(const QString &tag);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onBackClicked();

private:
    void refreshStyle();
    QListWidget *m_listWidget;
    QPushButton *m_backButton;
    QLabel *m_titleLabel;

    bool m_showingFiles = false;
    QStringList m_allTags;
};

#endif // TAGPANEL_H
