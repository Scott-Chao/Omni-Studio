#ifndef SEARCHPANEL_H
#define SEARCHPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QVector>

class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    struct SearchResult {
        QString filePath;
        int lineNumber;      // 1-based
        QString snippet;
    };

    explicit SearchPanel(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    void clearSearch();
    void focusSearchInput();

signals:
    void resultClicked(const QString &filePath, int lineNumber,
                       const QString &searchText);

private slots:
    void onSearchTextChanged();
    void performSearch();
    void onResultItemClicked(QListWidgetItem *item);

private:
    static void collectTextFiles(const QString &rootPath,
                                 QStringList &outFiles);
    void addResultItem(const SearchResult &result);
    void updateStatusLabel();
    QString extractSnippet(const QString &line) const;

    QLineEdit *m_searchInput;
    QListWidget *m_resultList;
    QLabel *m_statusLabel;
    QTimer *m_debounceTimer;
    void refreshStyle();

    QString m_rootPath;
    QString m_searchText;
    QVector<SearchResult> m_results;
};

#endif // SEARCHPANEL_H
