#ifndef BACKLINKINDEX_H
#define BACKLINKINDEX_H

#include <QString>
#include <QStringList>
#include <QMap>

class BacklinkIndex
{
public:
    struct BacklinkData {
        QMap<QString, QStringList> backlinks;
        QMap<QString, QStringList> forwardLinks;
    };

    BacklinkIndex() = default;

    void buildIndex(const QString &rootPath, const QMap<QString, QStringList> &fileIndex);
    void rebuildFile(const QString &filePath, const QString &rootPath,
                     const QMap<QString, QStringList> &fileIndex);

    void onFileRenamed(const QString &oldPath, const QString &newPath);
    void onFileDeleted(const QString &path);

    QStringList backlinksFor(const QString &filePath) const;

    void setData(BacklinkData data);
    static BacklinkData buildFromPath(const QString &rootPath,
                                      const QMap<QString, QStringList> &fileIndex);
    static QString resolveTarget(const QString &linkName, const QString &rootPath,
                                 const QMap<QString, QStringList> &fileIndex,
                                 const QString &currentDir = {});

private:
    // target absolute path → list of source file paths that link to it
    QMap<QString, QStringList> m_backlinks;
    // source file path → list of resolved target paths (for incremental updates)
    QMap<QString, QStringList> m_forwardLinks;

    void removeFile(const QString &path);
    void addFileLinks(const QString &filePath, const QString &rootPath,
                      const QMap<QString, QStringList> &fileIndex);
};

#endif // BACKLINKINDEX_H
