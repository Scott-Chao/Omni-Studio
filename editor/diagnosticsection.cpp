#include "diagnosticsection.h"
#include "core/thememanager.h"

#include <algorithm>
#include <QMouseEvent>

DiagnosticSection::DiagnosticSection(const QString &title, const QString &borderColor,
                                     int severity, QWidget *parent)
    : QWidget(parent)
    , m_borderColor(borderColor)
    , m_title(title)
    , m_severity(severity)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_headerLabel = new QLabel(this);
    m_headerLabel->setCursor(Qt::PointingHandCursor);
    m_headerLabel->setTextFormat(Qt::RichText);

    m_contentWidget = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    layout->addWidget(m_headerLabel);
    layout->addWidget(m_contentWidget);

    connect(m_headerLabel, &QLabel::linkActivated, this, [this](const QString &) {
        setExpanded(!m_expanded);
    });

    refreshStyle();
    setExpanded(true);
}

void DiagnosticSection::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    m_headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold; "
        "padding: 4px 8px; border-bottom: 1px solid %2; }"
    ).arg(tm.color("cell.foreground").name(),
          tm.color("panel.border").name()));

    if (m_severity == 1)
        m_borderColor = tm.color(QStringLiteral("diagnostics.error")).name();
    else if (m_severity == 2)
        m_borderColor = tm.color(QStringLiteral("diagnostics.warning")).name();
}

void DiagnosticSection::setDiagnostics(int cellIndex, const QList<SmdDiagnostic> &diags)
{
    m_cellIndex = cellIndex;

    QLayoutItem *child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    QList<SmdDiagnostic> filtered;
    for (const auto &d : diags) {
        if (d.severity == m_severity)
            filtered.append(d);
    }
    m_count = filtered.size();

    std::sort(filtered.begin(), filtered.end(),
              [](const SmdDiagnostic &a, const SmdDiagnostic &b) {
                  return a.startLine < b.startLine;
              });

    auto &tm = ThemeManager::instance();
    QString lineFg = tm.color("editorLineNumber.foreground").name();
    QString msgFg = tm.color("editor.foreground").name();
    QString entryBg = tm.color("sideBar.background").name();
    QString hoverBg = tm.color("editor.lineHighlightBackground").name();

    for (const auto &d : filtered) {
        QString entryText = QStringLiteral("<span style=\"color:%1;\">行 %2:</span> "
                                           "<span style=\"color:%3;\">%4</span>")
                                .arg(lineFg)
                                .arg(d.startLine + 1)
                                .arg(msgFg)
                                .arg(d.message.toHtmlEscaped());

        auto *entry = new QLabel(entryText, m_contentWidget);
        entry->setWordWrap(true);
        entry->setCursor(Qt::PointingHandCursor);
        entry->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; padding: 3px 8px 3px 12px; "
            "border-left: 2px solid %2; background: %3; }"
            "QLabel:hover { background: %4; }")
            .arg(msgFg, m_borderColor, entryBg, hoverBg));
        entry->setTextFormat(Qt::RichText);

        entry->setProperty("_ci", cellIndex);
        entry->setProperty("_line", d.startLine);
        entry->installEventFilter(this);

        m_contentLayout->addWidget(entry);
    }

    setExpanded(m_expanded);
}

bool DiagnosticSection::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QVariant ci = obj->property("_ci");
        QVariant line = obj->property("_line");
        if (ci.isValid() && line.isValid()) {
            emit lineClicked(ci.toInt(), line.toInt());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DiagnosticSection::clear()
{
    QLayoutItem *child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_count = 0;
    m_cellIndex = -1;
    setExpanded(m_expanded);
}

void DiagnosticSection::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_contentWidget->setVisible(expanded);

    QString indicator = expanded ? QStringLiteral("▾")
                                 : QStringLiteral("▸");
    m_headerLabel->setText(QStringLiteral(
        "<a href=\"toggle\" style=\"color:%1;text-decoration:none;"
        "font-weight:bold;\">%2 %3 (%4)</a>")
        .arg(ThemeManager::instance().color("cell.foreground").name(),
             indicator, m_title)
        .arg(m_count));
}
