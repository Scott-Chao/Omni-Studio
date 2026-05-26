#ifndef INDEXMANAGER_H
#define INDEXMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <memory>
#include <atomic>
#include <functional>
#include <QFileInfo>

class BacklinkIndex;
class TagIndex;
class TabManager;
class EditorWidget;

class IndexManager : public QObject
{
    Q_OBJECT
public:
    explicit IndexManager(QObject *parent = nullptr);
    ~IndexManager();

    void setTabManager(TabManager *manager);

    // Full index build (file index + backlinks + tags)
    void startAsyncIndexBuild(const QString &rootPath);

    // Lightweight: file index only
    void buildFileIndexAsync(const QString &rootPath,
                             std::function<void()> onComplete = nullptr);

    // Incremental updates (index data only)
    void onFileRenamedInIndex(const QString &oldPath, const QString &newPath);
    void onFileDeletedInIndex(const QString &path);

    // Resolve [[wiki-link]] to an absolute file path
    QString findWikiTarget(const QString &fileName,
                           const QString &rootPath,
                           const QString &currentDir) const;

    // Update completions on the given editor
    void updateCurrentEditorCompletions(EditorWidget *editor) const;

    // Accessors
    const QMap<QString, QStringList> &fileIndex() const { return m_fileIndex; }
    BacklinkIndex *backlinkIndex() const { return m_backlinkIndex; }
    TagIndex *tagIndex() const { return m_tagIndex; }

    // Wiki-link rename operation tracking (cancellation token)
    int nextWikiLinkUpdateId() { return ++m_wikiLinkUpdateId; }
    int currentWikiLinkUpdateId() const { return m_wikiLinkUpdateId.load(); }

    void cancelAllScans();

signals:
    void fileIndexReady();
    void fullIndexReady();

private:
    void cancelScan(std::shared_ptr<std::atomic<bool>> &flag,
                    std::atomic<uint64_t> &scanId,
                    uint64_t &nextId);

    TabManager *m_tabManager = nullptr;

    QMap<QString, QStringList> m_fileIndex;
    BacklinkIndex *m_backlinkIndex = nullptr;
    TagIndex *m_tagIndex = nullptr;

    std::shared_ptr<std::atomic<bool>> m_scanCancelled;
    std::atomic<uint64_t> m_scanId{0};
    std::shared_ptr<std::atomic<bool>> m_fileIdxCancelled;
    std::atomic<uint64_t> m_fileIdxScanId{0};
    std::atomic<int> m_wikiLinkUpdateId{0};
};

#endif // INDEXMANAGER_H
