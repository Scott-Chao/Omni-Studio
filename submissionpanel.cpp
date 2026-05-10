#include "submissionpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

SubmitResultPanel::SubmitResultPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SubmitResultPanel::setupUi()
{
    setStyleSheet(QStringLiteral(
        "QWidget { background: #1E1E1E; color: #D4D4D4; }"
    ));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMaximumHeight(80);
    QFont statusFont(QStringLiteral("Microsoft YaHei"), 24, QFont::Bold);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setMinimumHeight(60);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; padding: 8px;"));

    m_detailLabel = new QLabel(this);
    m_detailLabel->setAlignment(Qt::AlignCenter);
    QFont detailFont(QStringLiteral("Microsoft YaHei"), 11);
    m_detailLabel->setFont(detailFont);
    m_detailLabel->setStyleSheet(QStringLiteral("color: #D4D4D4; padding: 4px;"));

    m_ceEdit = new QPlainTextEdit(this);
    m_ceEdit->setReadOnly(true);
    m_ceEdit->setMaximumBlockCount(500);
    m_ceEdit->setMinimumHeight(60);
    m_ceEdit->setMaximumHeight(150);
    QFont monoFont(QStringLiteral("Consolas"), 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_ceEdit->setFont(monoFont);
    m_ceEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background-color: #2D2D30;"
        "  color: #F48771;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 8px;"
        "}"));
    m_ceEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_ceEdit->hide();

    m_hideBtn = new QPushButton(tr("隐藏"), this);
    m_hideBtn->setFixedWidth(60);
    m_hideBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3E3E42; color: #D4D4D4; border: 1px solid #555; "
        "border-radius: 3px; padding: 4px 12px; } "
        "QPushButton:hover { background: #505050; }"));

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(4, 2, 4, 4);
    btnLayout->addStretch();
    btnLayout->addWidget(m_hideBtn);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 4);
    mainLayout->setSpacing(6);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_detailLabel);
    mainLayout->addWidget(m_ceEdit, 1);
    mainLayout->addLayout(btnLayout);

    connect(m_hideBtn, &QPushButton::clicked, this, &SubmitResultPanel::hideRequested);
}

void SubmitResultPanel::showResult(const SubmissionResult &result)
{
    // Status text and color
    QString statusText = result.status;
    QString color;
    if (result.status == QStringLiteral("Accepted") || result.status == QStringLiteral("AC")) {
        color = QStringLiteral("#52C41A"); // green
    } else if (result.status == QStringLiteral("Wrong Answer") || result.status == QStringLiteral("WA")) {
        color = QStringLiteral("#E74C3C"); // red
    } else if (result.status == QStringLiteral("Compile Error") || result.status == QStringLiteral("CE")) {
        color = QStringLiteral("#F39C12"); // orange
    } else if (result.status == QStringLiteral("Time Limit Exceeded") || result.status == QStringLiteral("TLE")) {
        color = QStringLiteral("#3498DB"); // blue
    } else if (result.status == QStringLiteral("Memory Limit Exceeded") || result.status == QStringLiteral("MLE")) {
        color = QStringLiteral("#9B59B6"); // purple
    } else if (result.status == QStringLiteral("Runtime Error") || result.status == QStringLiteral("RE")) {
        color = QStringLiteral("#E74C3C"); // red
    } else if (result.status == QStringLiteral("Presentation Error") || result.status == QStringLiteral("PE")) {
        color = QStringLiteral("#E67E22"); // dark orange
    } else if (result.status == QStringLiteral("Output Limit Exceeded") || result.status == QStringLiteral("OLE")) {
        color = QStringLiteral("#FF6B6B"); // salmon pink
    } else {
        color = QStringLiteral("#D4D4D4");
    }

    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; padding: 8px;").arg(color));
    m_statusLabel->setText(statusText);

    // Detail line
    QString detail;
    if (result.timeMs >= 0 && result.memoryKb > 0) {
        detail = tr("用时: %1 ms    内存: %2 KB").arg(result.timeMs).arg(result.memoryKb);
    } else if (result.timeMs >= 0) {
        detail = tr("用时: %1 ms").arg(result.timeMs);
    } else if (result.memoryKb > 0) {
        detail = tr("内存: %1 KB").arg(result.memoryKb);
    }

    // Run ID
    if (!result.runId.isEmpty()) {
        if (!detail.isEmpty()) detail += QStringLiteral("    ");
        detail += tr("Run ID: %1").arg(result.runId);
    }

    m_detailLabel->setText(detail);

    // CE log
    if (!result.compileError.isEmpty()) {
        m_ceEdit->setPlainText(result.compileError);
        m_ceEdit->show();
    } else {
        m_ceEdit->hide();
    }
}
