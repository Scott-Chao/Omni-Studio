#ifndef BACKLINKINDEX_H
#define BACKLINKINDEX_H

#include <QString>
#include <QStringList>
#include <QMap>

class BacklinkIndex
{
public:
    BacklinkIndex() = default;

    void buildIndex(const QString &rootPath, const QMap<QString, QStringList> &fileIndex);
    void rebuildFile(const QString &filePath, const QString &rootPath,
                     const QMap<QString, QStringList> &fileIndex);

    void onFileRenamed(const QString &oldPath, const QString &newPath);
    void onFileDeleted(const QString &path);

    QStringList backlinksFor(const QString &filePath) const;

private:
    // target absolute path → list of source file paths that link to it
    QMap<QString, QStringList> m_backlinks;
    // source file path → list of resolved target paths (for incremental updates)
    QMap<QString, QStringList> m_forwardLinks;

    void removeFile(const QString &path);
    void addFileLinks(const QString &filePath, const QString &rootPath,
                      const QMap<QString, QStringList> &fileIndex);
    QString resolveTarget(const QString &linkName, const QString &rootPath,
                          const QMap<QString, QStringList> &fileIndex) const;
};

#endif // BACKLINKINDEX_H
