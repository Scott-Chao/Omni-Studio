#include "judgepanel.h"
#include "thememanager.h"
#include "judge/judgeengine.h"
#include "config/configmanager.h"
#include "ai/errorjournal.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QColor>

JudgePanel::JudgePanel(QWidget *parent)
    : QWidget(parent)
{
    m_engine = new JudgeEngine(this);

    connect(m_engine, &JudgeEngine::testStarted,
            this, &JudgePanel::onTestStarted);
    connect(m_engine, &JudgeEngine::testFinished,
            this, &JudgePanel::onTestFinished);
    connect(m_engine, &JudgeEngine::allTestsFinished,
            this, &JudgePanel::onAllTestsFinished);
    connect(m_engine, &JudgeEngine::compileFinished,
            this, &JudgePanel::onCompileFinished);
    connect(m_engine, &JudgeEngine::judgeOutput,
            this, &JudgePanel::onJudgeOutput);
    connect(m_engine, &JudgeEngine::judgeStopped,
            this, &JudgePanel::onJudgeStopped);

    setupUi();

    ThemeManager::watchTheme(this, &JudgePanel::refreshStyle);
    refreshStyle();
}

void JudgePanel::setupUi()
{
    // ---- Folder selector ----
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(tr("选择评测用例文件夹..."));

    m_browseBtn = new QPushButton(tr("浏览..."), this);
    m_browseBtn->setFixedWidth(ConfigManager::instance().judgeBrowseButtonWidth());

    m_openJudgeBtn = new QPushButton(tr("从OpenJudge获取"), this);

    m_submitOJBtn = new QPushButton(tr("提交到OpenJudge"), this);

    QVBoxLayout *folderLayout = new QVBoxLayout;
    folderLayout->setContentsMargins(4, 4, 4, 0);
    folderLayout->setSpacing(4);

    QHBoxLayout *folderRow1 = new QHBoxLayout;
    folderRow1->setContentsMargins(0, 0, 0, 0);
    folderRow1->addWidget(m_folderEdit, 1);
    folderRow1->addWidget(m_browseBtn);

    QHBoxLayout *folderRow2 = new QHBoxLayout;
    folderRow2->setContentsMargins(0, 0, 0, 0);
    folderRow2->addWidget(m_openJudgeBtn, 1);

    QHBoxLayout *folderRow3 = new QHBoxLayout;
    folderRow3->setContentsMargins(0, 0, 0, 0);
    folderRow3->addWidget(m_submitOJBtn, 1);

    folderLayout->addLayout(folderRow1);
    folderLayout->addLayout(folderRow2);
    folderLayout->addLayout(folderRow3);

    // ---- Results table ----
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("#"),
        tr("测试用例"),
        tr("结果"),
        tr("耗时(ms)"),
        tr("内存(KB)")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(true);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setMinimumHeight(ConfigManager::instance().judgeTableMinHeight());

    connect(m_table, &QTableWidget::cellClicked,
            this, &JudgePanel::onTableItemClicked);

    // ---- Detail area ----
    m_detailEdit = new QPlainTextEdit(this);
    m_detailEdit->setReadOnly(true);
    m_detailEdit->setMaximumBlockCount(1000);
    m_detailEdit->setMinimumHeight(ConfigManager::instance().judgeDetailMinHeight());

    QFont monoFont(QStringLiteral("Consolas"), 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_detailEdit->setFont(monoFont);
    m_detailEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_detailEdit->setPlaceholderText(tr("点击失败的测试用例查看对比详情"));

    // ---- Bottom bar ----
    m_summaryLabel = new QLabel(tr("就绪"), this);

    m_runAllBtn = new QPushButton(tr("运行全部"), this);
    m_stopBtn = new QPushButton(tr("停止"), this);
    m_stopBtn->setEnabled(false);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(4, 2, 4, 4);
    buttonLayout->addWidget(m_summaryLabel);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_runAllBtn);
    buttonLayout->addWidget(m_stopBtn);

    // ---- Main layout ----
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);
    mainLayout->addLayout(folderLayout);
    mainLayout->addWidget(m_table, 1);
    mainLayout->addWidget(m_detailEdit);
    mainLayout->addLayout(buttonLayout);

    // ---- Internal connections ----
    connect(m_browseBtn, &QPushButton::clicked,
            this, &JudgePanel::onBrowseClicked);
    connect(m_openJudgeBtn, &QPushButton::clicked,
            this, [this]() { emit openJudgeRequested(); });
    connect(m_submitOJBtn, &QPushButton::clicked,
            this, [this]() { emit submitToOpenJudgeRequested(); });
    connect(m_runAllBtn, &QPushButton::clicked,
            this, &JudgePanel::onRunAllClicked);
    connect(m_stopBtn, &QPushButton::clicked,
            this, &JudgePanel::onStopClicked);
}

void JudgePanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QString("JudgePanel { %1 }")
        .arg(tm.colorStyle("background-color", "editor.background")));

    m_folderEdit->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 3px 6px; }")
        .arg(tm.color("input.background").name(),
             tm.color("input.foreground").name(),
             tm.color("input.border").name()));

    const QString btnStyle = QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 4px 12px; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { color: %5; background: %6; }"
    ).arg(tm.color("input.background").name(),
          tm.color("input.foreground").name(),
          tm.color("input.border").name(),
          tm.color("button.hoverBackground").name(),
          tm.color("tab.inactiveForeground").name(),
          tm.color("tab.inactiveBackground").name());

    m_browseBtn->setStyleSheet(btnStyle);
    m_openJudgeBtn->setStyleSheet(btnStyle);
    m_submitOJBtn->setStyleSheet(btnStyle);
    m_runAllBtn->setStyleSheet(btnStyle);
    m_stopBtn->setStyleSheet(btnStyle);

    m_table->setStyleSheet(QString(
        "QTableWidget { background: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; }"
        "QTableWidget::item { padding: 2px 4px; }"
        "QHeaderView::section { background: %4; color: %2;"
        "  border: none; border-bottom: 1px solid %3; padding: 4px; }")
        .arg(tm.color("editor.background").name(),
             tm.color("sideBar.foreground").name(),
             tm.color("sideBar.border").name(),
             tm.color("editorLineNumber.background").name()));

    m_detailEdit->setStyleSheet(QString(
        "QPlainTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  selection-background-color: %3;"
        "  border: 1px solid %4;"
        "}")
        .arg(tm.color("output.background").name(),
             tm.color("output.foreground").name(),
             tm.color("output.selectionBackground").name(),
             tm.color("sideBar.border").name()));

    m_summaryLabel->setStyleSheet(QString("color: %1; padding: 4px 8px; font-size: 12px;")
        .arg(tm.color("sideBar.foreground").name()));
}

QString JudgePanel::testFolder() const
{
    return m_folderEdit->text().trimmed();
}

void JudgePanel::setTestFolder(const QString &path)
{
    m_folderEdit->setText(path);
    clearResults();
}

void JudgePanel::runJudge(const QString &sourceFile)
{
    if (sourceFile.isEmpty())
        return;

    const QString folder = testFolder();
    if (folder.isEmpty() || !QDir(folder).exists()) {
        m_detailEdit->setPlainText(tr("请先在评测面板中选择有效的测试用例文件夹。"));
        return;
    }

    clearResults();
    m_detailEdit->clear();
    m_engine->setSourceFile(sourceFile);
    m_engine->setTestFolder(folder);
    m_engine->start();
}

void JudgePanel::clearResults()
{
    m_results.clear();
    m_table->setRowCount(0);
    m_detailEdit->clear();
    m_summaryLabel->setText(tr("就绪"));
    setInteractive(true);
}

void JudgePanel::setInteractive(bool enabled)
{
    m_runAllBtn->setEnabled(enabled);
    m_stopBtn->setEnabled(!enabled);
    m_folderEdit->setEnabled(enabled);
    m_browseBtn->setEnabled(enabled);
    m_openJudgeBtn->setEnabled(enabled);
    m_submitOJBtn->setEnabled(enabled);
}

// ---- Slots ----

void JudgePanel::onRunAllClicked()
{
    emit runAllRequested();
}

void JudgePanel::onStopClicked()
{
    m_engine->stop();
}

void JudgePanel::onBrowseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择评测用例文件夹"),
        m_folderEdit->text().isEmpty() ? QDir::homePath() : m_folderEdit->text());
    if (!dir.isEmpty()) {
        m_folderEdit->setText(dir);
        clearResults();
    }
}

void JudgePanel::onCompileFinished(bool success, const QString &errorOutput)
{
    if (!success) {
        m_detailEdit->setPlainText(
            tr("=== 编译错误 ===\n\n") + errorOutput);
    }
}

void JudgePanel::onTestStarted(int index, const QString &testName)
{
    Q_UNUSED(index);
    Q_UNUSED(testName);
    m_summaryLabel->setText(tr("评测中..."));
}

void JudgePanel::onTestFinished(int index, const JudgeEngine::TestResult &result)
{
    if (index >= m_results.size())
        m_results.resize(index + 1);
    m_results[index] = result;

    // Insert or update table row
    if (m_table->rowCount() <= index)
        m_table->insertRow(index);

    // # column
    QTableWidgetItem *numItem = new QTableWidgetItem(QString::number(index + 1));
    numItem->setData(Qt::UserRole, index);
    numItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(index, 0, numItem);

    // Test name column
    m_table->setItem(index, 1, new QTableWidgetItem(result.name));

    // Status column
    const auto &cfg = ConfigManager::instance();
    const QString &code = result.statusCode;
    QTableWidgetItem *statusItem = new QTableWidgetItem(code);
    statusItem->setTextAlignment(Qt::AlignCenter);
    if (code == QStringLiteral("AC")) {
        statusItem->setForeground(cfg.judgeColorAc());
    } else if (code == QStringLiteral("WA")) {
        statusItem->setForeground(cfg.judgeColorWa());
    } else if (code == QStringLiteral("TLE")) {
        statusItem->setForeground(cfg.judgeColorTle());
    } else if (code == QStringLiteral("MLE")) {
        statusItem->setForeground(cfg.judgeColorMle());
    } else {
        statusItem->setForeground(cfg.judgeColorRe());
    }
    m_table->setItem(index, 2, statusItem);

    // Time column
    QTableWidgetItem *timeItem = new QTableWidgetItem(QString::number(result.elapsedMs));
    timeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(index, 3, timeItem);

    // Memory column
    QTableWidgetItem *memItem = new QTableWidgetItem(QString::number(result.memoryKb));
    memItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(index, 4, memItem);

    updateSummaryLabel();

    // Record non-AC results to ErrorJournal
    if (result.statusCode != QStringLiteral("AC")) {
        ErrorJournal::instance().recordFailure(
            result, m_engine->sourceFile(), testFolder());
    }
}

void JudgePanel::onAllTestsFinished(int passed, int total)
{
    Q_UNUSED(passed);
    Q_UNUSED(total);
    setInteractive(true);
    updateSummaryLabel();
}

void JudgePanel::onJudgeStopped()
{
    setInteractive(true);
    m_summaryLabel->setText(tr("已停止"));
}

void JudgePanel::onJudgeOutput(const QString &text, bool isStderr)
{
    Q_UNUSED(text);
    Q_UNUSED(isStderr);
}

void JudgePanel::updateSummaryLabel()
{
    int passed = 0;
    int total = m_results.size();
    for (const auto &r : m_results) {
        if (r.passed)
            ++passed;
    }
    if (total == 0) {
        m_summaryLabel->setText(tr("就绪"));
    } else {
        m_summaryLabel->setText(tr("AC %1/%2").arg(passed).arg(total));
    }
}

void JudgePanel::onTableItemClicked(int row, int column)
{
    Q_UNUSED(column);
    showDetailForRow(row);
}

void JudgePanel::showDetailForRow(int row)
{
    if (row < 0 || row >= m_results.size())
        return;

    const JudgeEngine::TestResult &result = m_results[row];

    if (result.statusCode == QStringLiteral("AC")) {
        m_detailEdit->setPlainText(
            tr("AC\n\n测试用例 \"%1\" 在 %2 ms 内通过，峰值内存 %3 KB。")
                .arg(result.name).arg(result.elapsedMs).arg(result.memoryKb));
        return;
    }

    QString detail;
    detail += tr("--- %1 ---\n\n").arg(result.statusCode);
    if (!result.detail.isEmpty()) {
        detail += result.detail + QStringLiteral("\n\n");
    }
    detail += tr("峰值内存: %1 KB\n\n").arg(result.memoryKb);

    detail += tr("--- 预期输出 (%1.out) ---\n").arg(result.name);
    detail += result.expectedOutput;
    if (!detail.endsWith(QLatin1Char('\n')))
        detail += QLatin1Char('\n');

    detail += tr("\n--- 实际输出 (stdout) ---\n");
    detail += result.actualOutput;
    if (!detail.endsWith(QLatin1Char('\n')))
        detail += QLatin1Char('\n');

    m_detailEdit->setPlainText(detail);
}
