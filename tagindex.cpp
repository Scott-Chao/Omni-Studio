#include "tagindex.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTextStream>
#include <QSet>

// Tag regex: # followed by word characters (Unicode-aware via (*UCP)).
// (*UCP) makes \w match Unicode letters/digits/underscore (Chinese, Japanese, etc.).
// Hyphen included explicitly for tags like #my-tag.
// (?!\s) negative lookahead prevents matching markdown headings (# Heading).
static const QRegularExpression &tagRegExp()
{
    static const QRegularExpression re(QStringLiteral("(*UCP)#(?!\\s)([\\w-]+)"));
    return re;
}

QStringList TagIndex::extractTagsFromContent(const QString &content)
{
    QSet<QString> tags;
    const QStringList lines = content.split(QLatin1Char('\n'));
    bool inCodeBlock = false;

    for (const QString &line : lines) {
        // Toggle code block state
        if (line.trimmed().startsWith(QStringLiteral("```"))) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock)
            continue;

        // Extract tags from non-code lines
        QRegularExpressionMatchIterator it = tagRegExp().globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString tag = match.captured(1);
            if (!tag.isEmpty())
                tags.insert(tag);
        }
    }

    return QStringList(tags.begin(), tags.end());
}

QString TagIndex::processTagsForPreview(const QString &markdown)
{
    // Replace #tag with clickable links.
    // Input is markdown with some <a> tags already injected (from processWikiLinks).
    // Strategy: line-based, skipping markdown code blocks (``` fences)
    // to avoid replacing #include, #define etc. inside code blocks.

    QStringList lines = markdown.split(QLatin1Char('\n'));
    QStringList result;
    bool inCodeBlock = false;

    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("```"))) {
            inCodeBlock = !inCodeBlock;
            result.append(line);
            continue;
        }
        if (inCodeBlock) {
            result.append(line);
            continue;
        }

        QString processed;
        int lastPos = 0;
        QStringView lineView(line);
        QRegularExpressionMatchIterator it = tagRegExp().globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            processed += lineView.mid(lastPos, match.capturedStart() - lastPos).toString();
            QString tagName = match.captured(1);
            processed += QStringLiteral("<a href=\"tag:") + tagName + QStringLiteral("\">#")
                         + tagName + QStringLiteral("</a>");
            lastPos = match.capturedEnd();
        }
        processed += lineView.mid(lastPos).toString();
        result.append(processed);
    }

    return result.join(QLatin1Char('\n'));
}

TagIndex::TagData TagIndex::buildFromPath(const QString &rootPath)
{
    TagData data;

    QStringList mdFilters = { QStringLiteral("*.md"), QStringLiteral("*.markdown") };
    QDirIterator it(rootPath, mdFilters,
                    QDir::Files | QDir::NoSymLinks | QDir::Readable,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();

        QStringList tags = extractTagsFromContent(content);
        if (tags.isEmpty())
            continue;

        data.fileToTags.insert(filePath, tags);
        for (const QString &tag : tags) {
            data.tagToFiles[tag].append(filePath);
        }
    }

    return data;
}

void TagIndex::setData(TagData data)
{
    m_tagToFiles = std::move(data.tagToFiles);
    m_fileToTags = std::move(data.fileToTags);
}

QStringList TagIndex::filesForTag(const QString &tag) const
{
    return m_tagToFiles.value(tag);
}

QStringList TagIndex::allTags() const
{
    return m_tagToFiles.keys();
}

void TagIndex::rebuildFile(const QString &filePath)
{
    removeFile(filePath);
    addFileTags(filePath);
}

void TagIndex::onFileRenamed(const QString &oldPath, const QString &newPath)
{
    // Migrate fileToTags key
    if (m_fileToTags.contains(oldPath)) {
        QStringList tags = m_fileToTags.take(oldPath);
        m_fileToTags.insert(newPath, tags);

        // Update tagToFiles references
        for (const QString &tag : tags) {
            QStringList &files = m_tagToFiles[tag];
            int idx = files.indexOf(oldPath);
            if (idx >= 0)
                files[idx] = newPath;
        }
    }
}

void TagIndex::onFileDeleted(const QString &path)
{
    removeFile(path);
}

void TagIndex::removeFile(const QString &path)
{
    if (!m_fileToTags.contains(path))
        return;

    QStringList tags = m_fileToTags.take(path);
    for (const QString &tag : tags) {
        QStringList &files = m_tagToFiles[tag];
        files.removeAll(path);
        if (files.isEmpty())
            m_tagToFiles.remove(tag);
    }
}

void TagIndex::addFileTags(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    if (!content.contains(QLatin1Char('#')))
        return;

    QStringList tags = extractTagsFromContent(content);
    if (tags.isEmpty())
        return;

    m_fileToTags.insert(filePath, tags);
    for (const QString &tag : tags) {
        m_tagToFiles[tag].append(filePath);
    }
}
