#include "indexmanager.h"
#include "backlinkindex.h"
#include "tagindex.h"
#include "tabmanager.h"
#include "editorwidget.h"
#include "fileutils.h"

#include <QThread>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QMetaObject>

IndexManager::IndexManager(QObject *parent)
    : QObject(parent)
    , m_backlinkIndex(new BacklinkIndex)
    , m_tagIndex(new TagIndex)
{
}

IndexManager::~IndexManager()
{
    cancelAllScans();
    delete m_backlinkIndex;
    delete m_tagIndex;
}

void IndexManager::setTabManager(TabManager *manager)
{
    m_tabManager = manager;
}

void IndexManager::cancelAllScans()
{
    if (m_scanCancelled)
        m_scanCancelled->store(true);
    if (m_fileIdxCancelled)
        m_fileIdxCancelled->store(true);
}

void IndexManager::startAsyncIndexBuild(const QString &rootPath)
{
    // Cancel any in-flight scan
    if (m_scanCancelled)
        m_scanCancelled->store(true);
    m_scanCancelled = std::make_shared<std::atomic<bool>>(false);
    uint64_t scanId = ++m_scanId;

    if (!TextFileUtils::isSafeRootPath(rootPath)) {
        m_fileIndex.clear();
        m_backlinkIndex->setData({});
        emit fileIndexReady();
        return;
    }

    auto cancelled = m_scanCancelled;
    QThread::create([this, cancelled, scanId, rootPath]() {
        // Phase 1: Build file index
        QMap<QString, QStringList> fileIndex;
        QDirIterator it(rootPath, TextFileUtils::scanNameFilters(), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled->load()) return;
            QString fullPath = it.next();
            QFileInfo info(fullPath);
            fileIndex[info.completeBaseName()].append(fullPath);
        }
        if (cancelled->load()) return;

        // Phase 2: Build backlink index
        BacklinkIndex::BacklinkData backlinkData =
            BacklinkIndex::buildFromPath(rootPath, fileIndex);
        if (cancelled->load()) return;

        // Phase 3: Build tag index
        TagIndex::TagData tagData = TagIndex::buildFromPath(rootPath);
        if (cancelled->load()) return;

        // Deliver results to main thread
        QMetaObject::invokeMethod(this, [this, scanId,
                                         fileIndex = std::move(fileIndex),
                                         data = std::move(backlinkData),
                                         tagData = std::move(tagData)]() {
            if (scanId != m_scanId.load()) return; // Stale
            m_fileIndex = std::move(fileIndex);
            m_backlinkIndex->setData(std::move(data));
            m_tagIndex->setData(std::move(tagData));
            emit fullIndexReady();
        }, Qt::QueuedConnection);
    })->start();
}

void IndexManager::buildFileIndexAsync(const QString &rootPath,
                                        std::function<void()> onComplete)
{
    // Cancel any in-flight file-index-only scan
    if (m_fileIdxCancelled)
        m_fileIdxCancelled->store(true);
    m_fileIdxCancelled = std::make_shared<std::atomic<bool>>(false);
    uint64_t scanId = ++m_fileIdxScanId;

    if (!TextFileUtils::isSafeRootPath(rootPath)) {
        m_fileIndex.clear();
        emit fileIndexReady();
        if (onComplete)
            onComplete();
        return;
    }

    auto cancelled = m_fileIdxCancelled;
    QThread::create([this, cancelled, scanId, rootPath,
                     onComplete = std::move(onComplete)]() {
        QMap<QString, QStringList> fileIndex;
        QDirIterator it(rootPath, TextFileUtils::scanNameFilters(), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled->load()) return;
            QString fullPath = it.next();
            QFileInfo info(fullPath);
            fileIndex[info.completeBaseName()].append(fullPath);
        }
        if (cancelled->load()) return;

        QMetaObject::invokeMethod(this, [this, scanId,
                                         fileIndex = std::move(fileIndex),
                                         onComplete = std::move(onComplete)]() {
            if (scanId != m_fileIdxScanId.load()) return;
            m_fileIndex = std::move(fileIndex);
            emit fileIndexReady();
            if (onComplete)
                onComplete();
        }, Qt::QueuedConnection);
    })->start();
}

void IndexManager::onFileRenamedInIndex(const QString &oldPath, const QString &newPath)
{
    if (m_tabManager)
        m_tabManager->updateEditorFilePath(oldPath, newPath);

    m_backlinkIndex->onFileRenamed(oldPath, newPath);
    m_tagIndex->onFileRenamed(oldPath, newPath);
}

void IndexManager::onFileDeletedInIndex(const QString &path)
{
    m_backlinkIndex->onFileDeleted(path);
    m_tagIndex->onFileDeleted(path);
}

QString IndexManager::findWikiTarget(const QString &fileName,
                                      const QString &rootPath,
                                      const QString &currentDir) const
{
    if (rootPath.isEmpty()) return {};

    // Try direct path under root with known extensions
    const QStringList exts = TextFileUtils::textExtensions();
    for (const QString &ext : exts) {
        QString directPath = rootPath + "/" + fileName + "." + ext;
        if (QFile::exists(directPath))
            return QDir::cleanPath(directPath);
    }

    // Try current directory
    if (!currentDir.isEmpty()) {
        QString localPath = currentDir + "/" + fileName;
        for (const QString &ext : exts) {
            if (QFile::exists(localPath + "." + ext))
                return QDir::cleanPath(localPath + "." + ext);
        }
    }

    // Use global index
    QString baseName = QFileInfo(fileName).completeBaseName();
    if (m_fileIndex.contains(baseName)) {
        const QStringList &candidates = m_fileIndex[baseName];
        if (candidates.isEmpty()) return {};

        if (candidates.size() == 1) return candidates.first();

        // Multiple matches: pick closest to current directory
        QString bestMatch = candidates.first();
        int minDistance = 999;
        for (const QString &path : candidates) {
            QString rel = QDir(currentDir).relativeFilePath(path);
            int distance = rel.count("/");
            if (distance < minDistance) {
                minDistance = distance;
                bestMatch = path;
            }
        }
        return bestMatch;
    }

    return {};
}

void IndexManager::updateCurrentEditorCompletions(EditorWidget *editor) const
{
    if (editor) {
        editor->setFileNames(m_fileIndex.keys());
        editor->setTagNames(m_tagIndex->allTags());
    }
}
