#ifndef TAGINDEX_H
#define TAGINDEX_H

#include <QString>
#include <QStringList>
#include <QMap>

class TagIndex
{
public:
    struct TagData {
        QMap<QString, QStringList> tagToFiles;  // tag → file paths
        QMap<QString, QStringList> fileToTags;  // file path → tags (for incremental)
    };

    TagIndex() = default;

    void setData(TagData data);

    static TagData buildFromPath(const QString &rootPath);

    QStringList filesForTag(const QString &tag) const;
    QStringList allTags() const;

    void rebuildFile(const QString &filePath);
    void onFileRenamed(const QString &oldPath, const QString &newPath);
    void onFileDeleted(const QString &path);

    // Shared utilities
    static QStringList extractTagsFromContent(const QString &content);
    static QString processTagsForPreview(const QString &markdown);

private:
    QMap<QString, QStringList> m_tagToFiles;  // tag → files
    QMap<QString, QStringList> m_fileToTags;  // file → tags

    void removeFile(const QString &path);
    void addFileTags(const QString &filePath);
};

#endif // TAGINDEX_H
