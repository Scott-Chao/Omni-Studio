#ifndef OUTLINEPANEL_H
#define OUTLINEPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QVector>
#include <QRegularExpression>
#include <QStringList>

struct HeadingItem {
    QString text;
    int level = 1;       // 1-6
    int lineNumber = 0;  // 1-based
};

inline QVector<HeadingItem> extractHeadingsFromContent(const QString &content)
{
    QVector<HeadingItem> headings;

    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.+)$"),
                                              QRegularExpression::MultilineOption);

    const QStringList lines = content.split(QLatin1Char('\n'));
    bool inCodeBlock = false;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        if (line.trimmed().startsWith(QStringLiteral("```"))) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock)
            continue;

        QRegularExpressionMatch match = headingRe.match(line);
        if (match.hasMatch()) {
            HeadingItem item;
            item.level = match.captured(1).length();
            item.text = match.captured(2).trimmed();
            item.lineNumber = i + 1;
            headings.append(item);
        }
    }

    return headings;
}

class OutlinePanel : public QWidget
{
    Q_OBJECT
public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void showHeadings(const QVector<HeadingItem> &headings);
    void clear();
    void refreshStyle();

signals:
    void headingClicked(int lineNumber, const QString &headingText);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget *m_listWidget;
    QVector<HeadingItem> m_headings;
};

#endif // OUTLINEPANEL_H
