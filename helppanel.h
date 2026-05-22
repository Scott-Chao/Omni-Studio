#ifndef HELPPANEL_H
#define HELPPANEL_H

#include <QWidget>
#include <QVector>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;
class QTextBrowser;
class QScrollBar;

class HelpPanel : public QWidget
{
    Q_OBJECT

public:
    explicit HelpPanel(QWidget *parent = nullptr);

signals:
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct SectionInfo {
        QString id;
        QString displayText; // shown in sidebar
        QString matchText;   // text to match against QTextBlock::text()
    };

    void loadContent();
    void computeSectionPositions();
    void onScrollChanged(int value);
    void refreshStyle();
    void applyThemeToContent();

    QLabel *m_titleLabel;
    QPushButton *m_closeButton;
    QListWidget *m_categoryList;
    QTextBrowser *m_contentBrowser;
    QScrollBar *m_scrollBar = nullptr;

    bool m_dragging = false;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;
    bool m_updatingCategory = false;

    QVector<SectionInfo> m_sectionInfo;
    QVector<qreal> m_sectionPositions;
    bool m_positionsComputed = false;
    QString m_rawHelpContent;

    static constexpr int kTitleBarHeight = 36;
    static constexpr int kCategoryWidth = 170;
};

#endif // HELPPANEL_H
