#ifndef BACKLINKSPANEL_H
#define BACKLINKSPANEL_H

#include <QWidget>
#include <QListWidget>

class BacklinksPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BacklinksPanel(QWidget *parent = nullptr);

    void showBacklinks(const QStringList &sourceFiles);

signals:
    void fileClicked(const QString &filePath);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget *m_listWidget;
    QStringList m_sourceFiles;
};

#endif // BACKLINKSPANEL_H
