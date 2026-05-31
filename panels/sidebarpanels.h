#ifndef SIDEBARPANELS_H
#define SIDEBARPANELS_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

// ── Backlinks panel ─────────────────────────────────────────────────────────

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

// ── Tag panel ───────────────────────────────────────────────────────────────

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

private:
    void onItemClicked(QListWidgetItem *item);
    QListWidget *m_listWidget;

    bool m_showingFiles = false;
    QStringList m_allTags;
};

#endif // SIDEBARPANELS_H
