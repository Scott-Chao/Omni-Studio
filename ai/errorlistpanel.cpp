#include "errorlistpanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════
// ErrorListItem — single clickable error record card
// ═══════════════════════════════════════════════════════════════════

static QString statusColor(const QString &code)
{
    if (code == "WA")  return "#ff8c00";
    if (code == "RE")  return "#e74c3c";
    if (code == "TLE") return "#f39c12";
    if (code == "MLE") return "#9b59b6";
    return "#888888";
}

ErrorListItem::ErrorListItem(const ErrorRecord &record, QWidget *parent)
    : QWidget(parent), m_record(record)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(64);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(2);

    // ── Row 1: status badge + problem name + arrow ──
    auto *row1 = new QHBoxLayout;
    row1->setSpacing(8);

    auto *badge = new QLabel(m_record.statusCode);
    badge->setFixedSize(36, 20);
    badge->setAlignment(Qt::AlignCenter);
    QString badgeColor = statusColor(m_record.statusCode);
    badge->setStyleSheet(
        QString("background-color: %1; color: #fff; font-size: 10px;"
                " font-weight: bold; border-radius: 3px;").arg(badgeColor));

    auto *nameLabel = new QLabel(m_record.problemName);
    nameLabel->setStyleSheet("color: #D4D4D4; font-size: 12px; font-weight: bold;");
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Allow text to ellipsis
    nameLabel->setTextFormat(Qt::PlainText);

    auto *arrow = new QLabel(">");
    arrow->setStyleSheet("color: #555; font-size: 14px;");
    arrow->setFixedWidth(12);

    row1->addWidget(badge);
    row1->addWidget(nameLabel, 1);
    row1->addWidget(arrow);

    // ── Row 2: source file + time ──
    auto *row2 = new QHBoxLayout;
    row2->setSpacing(12);

    auto *fileLabel = new QLabel(m_record.sourceFile.section('\\', -1).section('/', -1));
    fileLabel->setStyleSheet("color: #888; font-size: 10px;");

    auto *timeLabel = new QLabel(m_record.timestamp.toString("yyyy-MM-dd HH:mm"));
    timeLabel->setStyleSheet("color: #666; font-size: 10px;");

    row2->addWidget(fileLabel);
    row2->addWidget(timeLabel);
    row2->addStretch();

    // ── Row 3: tags (if any) ──
    QLabel *tagsLabel = nullptr;
    if (!m_record.tags.isEmpty()) {
        tagsLabel = new QLabel(m_record.tags.join("  "));
        tagsLabel->setStyleSheet("color: #0078d4; font-size: 10px;");
    }

    layout->addLayout(row1);
    layout->addLayout(row2);
    if (tagsLabel)
        layout->addWidget(tagsLabel);
}

void ErrorListItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_record.id);
    QWidget::mousePressEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ErrorListItem::enterEvent(QEnterEvent *event)
#else
void ErrorListItem::enterEvent(QEvent *event)
#endif
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void ErrorListItem::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void ErrorListItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_hovered) {
        painter.fillRect(rect(), QColor("#2d2d2d"));
    }

    // Bottom border line
    painter.setPen(QPen(QColor("#3c3c3c"), 1));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

// ═══════════════════════════════════════════════════════════════════
// ErrorListPanel — filter/search bar + scrollable list + bottom bar
// ═══════════════════════════════════════════════════════════════════

ErrorListPanel::ErrorListPanel(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #1E1E1E;");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Filter bar ──
    auto *filterBar = new QWidget(this);
    filterBar->setStyleSheet("background-color: #252525;");
    auto *filterLayout = new QVBoxLayout(filterBar);
    filterLayout->setContentsMargins(8, 6, 8, 6);
    filterLayout->setSpacing(6);

    m_statusFilter = new QComboBox;
    m_statusFilter->addItem(tr("全部状态"), QString());
    m_statusFilter->addItem("WA", "WA");
    m_statusFilter->addItem("RE", "RE");
    m_statusFilter->addItem("TLE", "TLE");
    m_statusFilter->addItem("MLE", "MLE");
    m_statusFilter->setStyleSheet(
        "QComboBox {"
        "  background-color: #3c3c3c;"
        "  color: #cccccc;"
        "  border: 1px solid #555;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 12px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 6px solid #888;"
        "  margin-right: 4px;"
        "}"
        "QComboBox:hover {"
        "  border-color: #0078d4;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #2d2d2d;"
        "  color: #cccccc;"
        "  border: 1px solid #555;"
        "  selection-background-color: #094771;"
        "  font-size: 12px;"
        "}"
    );

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("搜索错题..."));
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #3c3c3c;"
        "  color: #cccccc;"
        "  border: 1px solid #555;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #0078d4;"
        "}"
        "QLineEdit::placeholder {"
        "  color: #666;"
        "}"
    );

    filterLayout->addWidget(m_statusFilter);
    filterLayout->addWidget(m_searchEdit);

    // ── Scroll area for error list ──
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background-color: #1E1E1E; border: none; }"
        "QScrollBar:vertical {"
        "  background-color: #1E1E1E;"
        "  width: 8px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #424242;"
        "  min-height: 30px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background-color: #555555;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    m_listContainer = new QWidget;
    m_listContainer->setStyleSheet("background-color: #1E1E1E;");
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(4, 4, 4, 4);
    m_listLayout->setSpacing(2);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(m_listContainer);

    // ── Bottom bar ──
    auto *bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(32);
    bottomBar->setStyleSheet("background-color: #252525;");
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(8, 0, 8, 0);

    m_deleteAllBtn = new QPushButton(tr("全部删除"));
    m_deleteAllBtn->setFixedHeight(22);
    m_deleteAllBtn->setCursor(Qt::PointingHandCursor);
    m_deleteAllBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  border: 1px solid #555;"
        "  border-radius: 4px;"
        "  color: #e74c3c;"
        "  font-size: 11px;"
        "  padding: 0 8px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #3c1e1e;"
        "  border-color: #e74c3c;"
        "}"
    );

    m_countLabel = new QLabel(tr("共 0 条记录"));
    m_countLabel->setStyleSheet("color: #888; font-size: 11px;");

    bottomLayout->addWidget(m_deleteAllBtn);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_countLabel);

    // ── Assemble main layout ──
    mainLayout->addWidget(filterBar);
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(bottomBar);

    // ── Connections ──
    connect(m_statusFilter, &QComboBox::currentIndexChanged,
            this, &ErrorListPanel::onFilterChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ErrorListPanel::onSearchTextChanged);
    connect(m_deleteAllBtn, &QPushButton::clicked,
            this, &ErrorListPanel::deleteAllRequested);

    // Initial empty state
    update(); // Will be painted empty
}

void ErrorListPanel::loadRecords()
{
    setRecords(ErrorJournal::instance().allRecords());
}

void ErrorListPanel::setRecords(const QVector<ErrorRecord> &records)
{
    m_allRecords = records;
    // Sort by timestamp descending
    std::sort(m_allRecords.begin(), m_allRecords.end(),
              [](const ErrorRecord &a, const ErrorRecord &b) {
                  return a.timestamp > b.timestamp;
              });
    rebuildList();
}

void ErrorListPanel::onFilterChanged()
{
    rebuildList();
}

void ErrorListPanel::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    rebuildList();
}

void ErrorListPanel::rebuildList()
{
    // Remove all existing item widgets
    for (auto *item : findChildren<ErrorListItem*>()) {
        m_listLayout->removeWidget(item);
        item->deleteLater();
    }

    QVector<ErrorRecord> filtered = filteredRecords();
    // Update count
    m_countLabel->setText(tr("共 %1 条记录").arg(filtered.size()));

    for (const auto &record : filtered) {
        auto *item = new ErrorListItem(record, m_listContainer);
        connect(item, &ErrorListItem::clicked, this, &ErrorListPanel::errorClicked);
        // Insert before the trailing stretch
        m_listLayout->insertWidget(m_listLayout->count() - 1, item);
    }

    // Scroll to top after rebuild
    m_scrollArea->verticalScrollBar()->setValue(0);
}

QVector<ErrorRecord> ErrorListPanel::filteredRecords() const
{
    QString statusFilter = m_statusFilter->currentData().toString();
    QString keyword = m_searchEdit->text().trimmed().toLower();

    QVector<ErrorRecord> result;
    for (const auto &rec : m_allRecords) {
        // Status filter
        if (!statusFilter.isEmpty() && rec.statusCode != statusFilter)
            continue;

        // Keyword search — match problem name, source file, tags
        if (!keyword.isEmpty()) {
            bool match = rec.problemName.toLower().contains(keyword)
                      || rec.sourceFile.toLower().contains(keyword)
                      || rec.statusCode.toLower().contains(keyword);
            if (!match) {
                for (const auto &tag : rec.tags) {
                    if (tag.toLower().contains(keyword)) {
                        match = true;
                        break;
                    }
                }
            }
            if (!match)
                continue;
        }

        result.append(rec);
    }

    return result;
}
