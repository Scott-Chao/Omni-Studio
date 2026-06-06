#include "backlinkindex.h"
#include "core/utilities.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTextStream>
#include <QSet>

void BacklinkIndex::buildIndex(const QString &rootPath, const QMap<QString, QStringList> &fileIndex)
{
    m_backlinks.clear();
    m_forwardLinks.clear();

    if (!TextFileUtils::isSafeRootPath(rootPath)) return;

    QDirIterator it(rootPath, TextFileUtils::scanNameFilters(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        addFileLinks(it.next(), rootPath, fileIndex);
    }
}

BacklinkIndex::BacklinkData BacklinkIndex::buildFromPath(const QString &rootPath,
                                                         const QMap<QString, QStringList> &fileIndex)
{
    BacklinkData data;

    if (!TextFileUtils::isSafeRootPath(rootPath))
        return data;

    // Same recursive regex used in EditorWidget::refreshPreview
    static const QRegularExpression wikiRegExp(
        QStringLiteral(R"(\[\[((?:[^\[\]]|\[(?1)\])*)\]\])"));

    QDirIterator it(rootPath, TextFileUtils::scanNameFilters(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        QSet<QString> targets;
        QRegularExpressionMatchIterator mit = wikiRegExp.globalMatch(content);
        QFileInfo fi(filePath);
        QString currentDir = fi.absolutePath();
        while (mit.hasNext()) {
            QRegularExpressionMatch match = mit.next();
            QString resolved = resolveTarget(match.captured(1), rootPath, fileIndex, currentDir);
            if (!resolved.isEmpty() && resolved != filePath)
                targets.insert(resolved);
        }

        if (!targets.isEmpty()) {
            QStringList targetsList = targets.values();
            for (const QString &target : targetsList)
                data.backlinks[target].append(filePath);
            data.forwardLinks[filePath] = std::move(targetsList);
        }
    }

    return data;
}

void BacklinkIndex::setData(BacklinkData data)
{
    m_backlinks = std::move(data.backlinks);
    m_forwardLinks = std::move(data.forwardLinks);
}

void BacklinkIndex::rebuildFile(const QString &filePath, const QString &rootPath,
                                 const QMap<QString, QStringList> &fileIndex)
{
    if (filePath.isEmpty()) return;
    removeFile(filePath);
    addFileLinks(filePath, rootPath, fileIndex);
}

void BacklinkIndex::onFileRenamed(const QString &oldPath, const QString &newPath)
{
    if (oldPath == newPath) return;

    // Migrate forward links key
    if (m_forwardLinks.contains(oldPath)) {
        m_forwardLinks[newPath] = m_forwardLinks.take(oldPath);
    }

    // Migrate backlinks target key (the renamed file was being linked to)
    if (m_backlinks.contains(oldPath)) {
        m_backlinks[newPath] = m_backlinks.take(oldPath);
    }

    // Update source path references in every backlink value list
    for (auto it = m_backlinks.begin(); it != m_backlinks.end(); ++it) {
        QStringList &sources = it.value();
        for (int i = 0; i < sources.size(); ++i) {
            if (sources[i] == oldPath) {
                sources[i] = newPath;
            }
        }
    }

    // Update target references in every forward links value list
    // 确保后续 rebuildFile → removeFile 能找到正确的 target 进行清理
    for (auto it = m_forwardLinks.begin(); it != m_forwardLinks.end(); ++it) {
        QStringList &targets = it.value();
        for (int i = 0; i < targets.size(); ++i) {
            if (targets[i] == oldPath) {
                targets[i] = newPath;
            }
        }
    }
}

void BacklinkIndex::onFileDeleted(const QString &path)
{
    removeFile(path);
    // Also remove the deleted file as a target (no navigable file remains)
    m_backlinks.remove(path);
}

QStringList BacklinkIndex::backlinksFor(const QString &filePath) const
{
    if (filePath.isEmpty()) return {};
    return m_backlinks.value(filePath);
}

void BacklinkIndex::removeFile(const QString &path)
{
    if (!m_forwardLinks.contains(path)) return;

    // Remove this file from each target's backlink list
    for (const QString &target : m_forwardLinks[path]) {
        if (m_backlinks.contains(target)) {
            m_backlinks[target].removeAll(path);
            if (m_backlinks[target].isEmpty()) {
                m_backlinks.remove(target);
            }
        }
    }
    m_forwardLinks.remove(path);
}

void BacklinkIndex::addFileLinks(const QString &filePath, const QString &rootPath,
                                  const QMap<QString, QStringList> &fileIndex)
{
    QString content = TextFileUtils::readTextFile(filePath);
    if (content.isNull())
        return;

    // Same recursive regex used in EditorWidget::refreshPreview
    static const QRegularExpression wikiRegExp(
        QStringLiteral(R"(\[\[((?:[^\[\]]|\[(?1)\])*)\]\])"));

    QSet<QString> targets;
    QRegularExpressionMatchIterator it = wikiRegExp.globalMatch(content);
    QFileInfo fi(filePath);
    QString currentDir = fi.absolutePath();
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString linkName = match.captured(1);
        QString resolved = resolveTarget(linkName, rootPath, fileIndex, currentDir);
        if (!resolved.isEmpty() && resolved != filePath) {
            targets.insert(resolved);
        }
    }

    if (!targets.isEmpty()) {
        QStringList targetsList = targets.values();
        for (const QString &target : targetsList) {
            m_backlinks[target].append(filePath);
        }
        m_forwardLinks[filePath] = std::move(targetsList);
    }
}

QString BacklinkIndex::resolveTarget(const QString &linkName, const QString &rootPath,
                                      const QMap<QString, QStringList> &fileIndex,
                                      const QString &currentDir)
{
    const QStringList exts = TextFileUtils::textExtensions();

    // 1. Check current directory first (prefer files co-located with the source)
    if (!currentDir.isEmpty()) {
        for (const QString &ext : exts) {
            QString localPath = currentDir + "/" + linkName + "." + ext;
            if (QFile::exists(localPath))
                return QDir::cleanPath(localPath);
        }
    }

    // 2. Exact path under root
    for (const QString &ext : exts) {
        QString directPath = rootPath + "/" + linkName + "." + ext;
        if (QFile::exists(directPath))
            return QDir::cleanPath(directPath);
    }

    // 3. Index lookup by base name
    QString baseName = QFileInfo(linkName).completeBaseName();
    if (!fileIndex.contains(baseName)) return {};

    const QStringList &candidates = fileIndex[baseName];
    if (candidates.isEmpty()) return {};
    if (candidates.size() == 1) return candidates.first();

    // 4. Multiple matches → deterministic shortest-path tiebreaker
    QString best = candidates.first();
    int minDepth = best.count('/');
    for (const QString &path : candidates) {
        int depth = path.count('/');
        if (depth < minDepth) {
            minDepth = depth;
            best = path;
        }
    }
    return best;
}
