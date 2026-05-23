#ifndef SEARCHPANEL_H
#define SEARCHPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <memory>

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
    ~SearchPanel();

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
    static QString extractSnippet(const QString &line,
                                  const QString &searchText, int maxLen);
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

    std::shared_ptr<std::atomic<bool>> m_searchCancelled;
    std::atomic<uint64_t> m_searchId{0};
};

#endif // SEARCHPANEL_H
