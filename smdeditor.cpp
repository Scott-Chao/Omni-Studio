#include "smdeditor.h"
#include "smdcell.h"
#include "smdformat.h"
#include "smdoutputwidget.h"
#include "processrunner.h"
#include "smdlspmanager.h"
#include "smddiagnosticspanel.h"
#include "codeeditor.h"
#include "compilerutils.h"
#include "configmanager.h"
#include "settingsmanager.h"
#include "languageutils.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QCoreApplication>
#include <QScrollBar>
#include <QApplication>
#include <QMessageBox>
#include <QLabel>
#include <QRegularExpression>

// ============================================================
// LangSelectorPopup — language selection popup for cell type
// ============================================================

class LangSelectorPopup : public QFrame
{
    Q_OBJECT
public:
    LangSelectorPopup(QWidget *parent,
                      std::function<void(SmdCell::CellType)> onSelected,
                      std::function<void()> onCancelled = nullptr)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_onSelected(std::move(onSelected))
        , m_onCancelled(std::move(onCancelled))
    {
        // Qt::Popup implies WA_DeleteOnClose which can fire deleteLater()
        // twice when combined with explicit close() + window-system hide.
        // Disable it — we call deleteLater() exactly once in confirm().
        setAttribute(Qt::WA_DeleteOnClose, false);
        setStyleSheet(QStringLiteral(
            "LangSelectorPopup { background: #2d2d30; border: 1px solid #555555; "
            "border-radius: 4px; }"
        ));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        struct Opt { SmdCell::CellType type; QString text; };
        const QList<Opt> opts = {
            {SmdCell::Markdown, QStringLiteral("1  Markdown")},
            {SmdCell::Cpp,      QStringLiteral("2  C++")},
            {SmdCell::Python,   QStringLiteral("3  Python")},
        };

        for (const auto &opt : opts) {
            auto *item = new QLabel(opt.text, this);
            item->setStyleSheet(QStringLiteral(
                "QLabel { color: #e0e0e0; padding: 6px 16px; font-size: 12px; }"
            ));
            item->setCursor(Qt::PointingHandCursor);
            item->installEventFilter(this);
            item->setProperty("cellType", static_cast<int>(opt.type));
            layout->addWidget(item);
            m_items.append(item);
        }

        adjustSize();
        selectIndex(0);
    }

    void setOnSelected(std::function<void(SmdCell::CellType)> cb) { m_onSelected = std::move(cb); }
    void setOnCancelled(std::function<void()> cb) { m_onCancelled = std::move(cb); }

    void selectIndex(int idx)
    {
        if (idx < 0 || idx >= m_items.size()) return;
        for (int i = 0; i < m_items.size(); ++i)
            m_items[i]->setStyleSheet(i == idx
                ? QStringLiteral("QLabel { color: #e0e0e0; padding: 6px 16px; font-size: 12px; "
                                 "background: #094771; }")
                : QStringLiteral("QLabel { color: #e0e0e0; padding: 6px 16px; font-size: 12px; }"));
        m_selectedIndex = idx;
    }

    SmdCell::CellType selectedType() const
    {
        if (m_selectedIndex < 0 || m_selectedIndex >= m_items.size())
            return SmdCell::Markdown;
        return static_cast<SmdCell::CellType>(
            m_items[m_selectedIndex]->property("cellType").toInt());
    }

    void confirm()
    {
        m_confirmed = true;
        if (m_onSelected)
            m_onSelected(selectedType());
        // Defer close so the widget tree (modified by setCellType during
        // m_onSelected) stabilises before the focus-chain walk inside
        // QWidget::close() runs.
        QTimer::singleShot(0, this, [this]() { close(); });
    }

protected:
    void hideEvent(QHideEvent *event) override
    {
        if (!m_confirmed && m_onCancelled)
            m_onCancelled();
        QFrame::hideEvent(event);
    }

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            int idx = m_items.indexOf(qobject_cast<QLabel*>(obj));
            if (idx >= 0) {
                selectIndex(idx);
                confirm();
                return true;
            }
        }
        return QFrame::eventFilter(obj, event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Escape:
            close();
            return;
        case Qt::Key_Up:
            selectIndex(qMax(0, m_selectedIndex - 1));
            return;
        case Qt::Key_Down:
            selectIndex(qMin(m_items.size() - 1, m_selectedIndex + 1));
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            confirm();
            return;
        case Qt::Key_1:
            selectIndex(0);
            confirm();
            return;
        case Qt::Key_2:
            selectIndex(1);
            confirm();
            return;
        case Qt::Key_3:
            selectIndex(2);
            confirm();
            return;
        default:
            break;
        }
        QFrame::keyPressEvent(event);
    }

private:
    QList<QLabel*> m_items;
    int m_selectedIndex = 0;
    bool m_confirmed = false;
    std::function<void(SmdCell::CellType)> m_onSelected;
    std::function<void()> m_onCancelled;
};

// ============================================================
// SmdEditor
// ============================================================

SmdEditor::SmdEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Splitter so the user can drag the diagnostics panel top edge to resize.
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(4);
    m_splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #3c3c3c; }"
    ));

    m_scrollArea = new QScrollArea(m_splitter);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background-color: #1E1E1E; border: none; }"
    ));

    m_cellContainer = new QWidget(m_scrollArea);
    m_cellContainer->setStyleSheet(QStringLiteral("background-color: #1E1E1E;"));
    m_cellLayout = new QVBoxLayout(m_cellContainer);
    m_cellLayout->setContentsMargins(8, 8, 8, 8);
    m_cellLayout->setSpacing(4);
    m_cellLayout->addStretch();

    m_scrollArea->setWidget(m_cellContainer);
    m_splitter->addWidget(m_scrollArea);

    m_diagnosticsPanel = new SmdDiagnosticsPanel(this, m_splitter);
    m_diagnosticsPanel->setVisible(false);
    m_splitter->addWidget(m_diagnosticsPanel);
    m_splitter->setStretchFactor(0, 1);  // scroll area stretches
    m_splitter->setStretchFactor(1, 0);  // panel is fixed-size
    mainLayout->addWidget(m_splitter);

    m_processRunner = new ProcessRunner(this);

    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);

    // Global event filter: catches FocusIn/MousePress on any widget and
    // activates the parent SmdCell.  This is a back-up path independent of
    // SmdCell::eventFilter, which suppresses focusEntered during performGrab.
    QApplication::instance()->installEventFilter(this);

    // Watch top-level window for minimize/restore to preserve scroll position
    QTimer::singleShot(0, this, [this]() {
        if (QWidget *tlw = window())
            tlw->installEventFilter(this);
    });

    // Load configurable shortcuts
    reloadShortcuts();
}

SmdEditor::~SmdEditor()
{
    m_processRunner->stop();
    stopPythonExecProcess();
    if (m_autoRenderTimer) {
        m_autoRenderTimer->stop();
        delete m_autoRenderTimer;
        m_autoRenderTimer = nullptr;
    }
    // Remove global event filter from QApplication
    QApplication::instance()->removeEventFilter(this);
    // Remove event filter from top-level window
    if (QWidget *tlw = window())
        tlw->removeEventFilter(this);
}

void SmdEditor::reloadShortcuts()
{
    auto &sm = SettingsManager::instance();
    m_cellExecute       = QKeySequence(sm.value("shortcuts.cell_execute", "Ctrl+Return").toString());
    m_cellExecuteJump   = QKeySequence(sm.value("shortcuts.cell_execute_jump", "Shift+Return").toString());
    m_cellLanguage      = QKeySequence(sm.value("shortcuts.cell_language", "Ctrl+K").toString());
    m_cellTerminate     = QKeySequence(sm.value("shortcuts.cell_terminate", "Ctrl+C").toString());
    m_cellClearOutput   = QKeySequence(sm.value("shortcuts.cell_clear_output", "Ctrl+Shift+Z").toString());
    m_cellSplit         = QKeySequence(sm.value("shortcuts.cell_split", "Ctrl+Shift+-").toString());
    m_toggleDiagnostics = QKeySequence(sm.value("shortcuts.toggle_diagnostics", "Ctrl+E").toString());
    m_cellInsertAbove   = QKeySequence(sm.value("shortcuts.cell_insert_above", "A").toString());
    m_cellInsertBelow   = QKeySequence(sm.value("shortcuts.cell_insert_below", "B").toString());
    m_cellDelete        = QKeySequence(sm.value("shortcuts.cell_delete", "Delete").toString());
}

// ---- File I/O ----

bool SmdEditor::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    QString text = stream.readAll();
    file.close();

    m_filePath = QFileInfo(filePath).absoluteFilePath();
    setPlainText(text);
    m_originalContent = toPlainText();
    emit fileLoaded(m_filePath);
    return true;
}

bool SmdEditor::saveFile()
{
    if (m_filePath.isEmpty())
        return false;

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Cannot write file: ") + m_filePath);
        return false;
    }
    QString text = toPlainText();
    QTextStream stream(&file);
    stream << text;
    file.close();

    setModified(false);
    emit fileSaved(m_filePath);
    return true;
}

QString SmdEditor::toPlainText() const
{
    QList<SmdFormat::Cell> fmtCells;
    for (int i = 0; i < m_cells.size(); ++i) {
        SmdFormat::Cell fc;
        fc.type = SmdCell::langIdFromType(m_cells[i]->cellType());
        fc.content = m_cells[i]->content();
        fc.rendered = m_cells[i]->isRendered();
        fc.output = m_outputWidgets[i]->outputText();
        fmtCells.append(fc);
    }
    return SmdFormat::serialize(fmtCells);
}

QString SmdEditor::toPlainTextContentOnly() const
{
    QList<SmdFormat::Cell> fmtCells;
    for (int i = 0; i < m_cells.size(); ++i) {
        SmdFormat::Cell fc;
        fc.type = SmdCell::langIdFromType(m_cells[i]->cellType());
        fc.content = m_cells[i]->content();
        fc.rendered = m_cells[i]->isRendered();
        fmtCells.append(fc);
    }
    return SmdFormat::serialize(fmtCells);
}

void SmdEditor::setPlainText(const QString &text)
{

    // Stop any in-progress auto-render
    if (m_autoRenderTimer) {
        m_autoRenderTimer->stop();
        m_autoRenderTimer->deleteLater();
        m_autoRenderTimer = nullptr;
    }
    m_autoRenderQueue.clear();

    if (m_diagnosticsPanel)
        m_diagnosticsPanel->clear();

    // Shut down old LSP manager and Python executor
    if (m_lspManager) {
        m_lspManager->shutdown();
        m_lspManager->deleteLater();
        m_lspManager = nullptr;
    }
    stopPythonExecProcess();

    // Clear existing cells and output widgets
    for (SmdOutputWidget *w : m_outputWidgets) {
        m_cellLayout->removeWidget(w);
        w->deleteLater();
    }
    m_outputWidgets.clear();
    for (SmdCell *c : m_cells) {
        m_cellLayout->removeWidget(c);
        c->deleteLater();
    }
    m_cells.clear();
    m_activeCellIndex = -1;

    QList<SmdFormat::Cell> fmtCells = SmdFormat::parse(text);
    if (fmtCells.isEmpty()) {
        addCell(0, SmdCell::Markdown);
    } else {
        for (int i = 0; i < fmtCells.size(); ++i) {
            addCell(i, SmdCell::typeFromLangId(fmtCells[i].type), fmtCells[i].content);
            // Restore output
            if (!fmtCells[i].output.isEmpty())
                m_outputWidgets[i]->setOutput(fmtCells[i].output);
            // Queue for auto-render
            if (fmtCells[i].rendered
                && fmtCells[i].type == QStringLiteral("markdown")) {
                m_autoRenderQueue.append(m_cells[i]);
            }
        }
    }

    m_commandMode = false;
    if (!m_cells.isEmpty()) {
        for (SmdCell *c : m_cells)
            c->setActive(false);
        setActiveCell(0);
    }

    // Deferred height update: after all cells are created and laid out,
    // recalculate heights so text-wrapping uses the final widget width.
    QTimer::singleShot(0, this, [this]() {
        for (SmdCell *c : m_cells)
            c->updateEditorHeight();
    });

    // Pre-mark rendered flag so toPlainText() matches the file content.
    // Actual rendering happens async via auto-render — without this the
    // rendered→false→true transition would make isModified() trip falsely.
    for (SmdCell *cell : m_autoRenderQueue)
        cell->setRenderedState(true);

    // Start auto-render if there are cells to render
    if (!m_autoRenderQueue.isEmpty())
        startAutoRender();

    // Initialize LSP manager for code cells
    m_lspManager = new SmdLspManager(this);
    if (!m_filePath.isEmpty())
        m_lspManager->initialize(m_filePath);

    for (int i = 0; i < m_cells.size(); ++i) {
        SmdCell *cell = m_cells[i];
        QString langId = SmdCell::langIdFromType(cell->cellType());
        if (langId == QStringLiteral("cpp") || langId == QStringLiteral("python")) {
            m_lspManager->cellAdded(i, langId, cell->content());
            // Wire the shared LSP provider to code cells.  This normally
            // happens in connectCellSignals(), but m_lspManager was null
            // when addCell() ran during setPlainText().
            if (auto *codeEditor = qobject_cast<CodeEditor*>(cell->editorWidget())) {
                auto *provider = m_lspManager->providerForCell(i, langId);
                if (provider)
                    codeEditor->setCompletionProvider(provider);
            }
        }
    }

    connect(m_lspManager, &SmdLspManager::diagnosticsUpdated, this,
            [this](int cellIndex, QList<SmdDiagnostic> diags) {
                if (cellIndex >= 0 && cellIndex < m_cells.size()) {
                    m_cells[cellIndex]->setDiagnostics(diags);
                    if (auto *codeEditor = qobject_cast<CodeEditor*>(
                            m_cells[cellIndex]->editorWidget())) {
                        codeEditor->setDiagnostics(diags);
                    }
                }
            });

    // Set initial program group if the active cell is C++
    if (m_activeCellIndex >= 0
        && m_cells[m_activeCellIndex]->cellType() == SmdCell::Cpp)
        m_lspManager->focusCell(m_activeCellIndex);
}

bool SmdEditor::isModified() const
{
    return toPlainText() != m_originalContent;
}

void SmdEditor::setModified(bool modified)
{
    if (!modified)
        m_originalContent = toPlainText();
    emit modificationChanged(modified);
}

// ---- Zoom / Font ----

void SmdEditor::applyZoom(qreal factor, int baseFontSize)
{
    m_zoomFactor = factor;
    m_baseFontSize = baseFontSize;
    for (SmdCell *c : m_cells)
        c->applyZoom(factor, baseFontSize);
}

void SmdEditor::setEditorFont(const QString &family, int size)
{
    m_baseFontSize = size;
    for (SmdCell *c : m_cells)
        c->applyZoom(1.0, size);
}

void SmdEditor::reloadColors()
{
    // Cell colors are set at creation time from ConfigManager
}

// ---- Cell Management ----

SmdCell *SmdEditor::addCell(int index, SmdCell::CellType type, const QString &content)
{
    SmdCell *cell = new SmdCell(type, content, m_cellContainer);
    connectCellSignals(cell, index);

    int layoutIdx = qBound(0, index, m_cells.size());
    // Cells and outputs are interleaved: cell at 2*i, output at 2*i+1
    int cellLayoutPos = layoutIdx * 2;
    int outLayoutPos = cellLayoutPos + 1;

    m_cellLayout->insertWidget(cellLayoutPos, cell);
    m_cells.insert(layoutIdx, cell);

    SmdOutputWidget *outputWidget = new SmdOutputWidget(m_cellContainer);
    m_cellLayout->insertWidget(outLayoutPos, outputWidget);
    m_outputWidgets.insert(layoutIdx, outputWidget);

    for (int i = layoutIdx; i < m_cells.size(); ++i) {
        disconnect(m_cells[i], &SmdCell::focusEntered, nullptr, nullptr);
        disconnect(m_cells[i], &SmdCell::contentChanged, nullptr, nullptr);
        disconnect(m_cells[i], &SmdCell::cellTypeChanged, nullptr, nullptr);
        connectCellSignals(m_cells[i], i);
    }

    if (m_activeCellIndex >= layoutIdx)
        ++m_activeCellIndex;

    // Notify LSP manager
    if (m_lspManager) {
        QString langId = SmdCell::langIdFromType(type);
        if (langId == QStringLiteral("cpp") || langId == QStringLiteral("python"))
            m_lspManager->cellAdded(layoutIdx, langId, content);
        // Wire shared provider to code cell
        if (langId == QStringLiteral("cpp") || langId == QStringLiteral("python")) {
            if (auto *codeEditor = qobject_cast<CodeEditor*>(cell->editorWidget())) {
                auto *provider = m_lspManager->providerForCell(layoutIdx, langId);
                if (provider)
                    codeEditor->setCompletionProvider(provider);
            }
        }
    }

    // Re-check active group after cell insertion
    if (m_lspManager && m_activeCellIndex >= 0
        && m_cells[m_activeCellIndex]->cellType() == SmdCell::Cpp)
        m_lspManager->focusCell(m_activeCellIndex);

    cell->applyZoom(m_zoomFactor, m_baseFontSize);

    emit contentChanged();
    return cell;
}

void SmdEditor::removeCell(int index)
{
    if (index < 0 || index >= m_cells.size() || m_cells.size() <= 1)
        return;
    if (m_executingCell) return; // Block during execution

    // Detach CodeEditor from shared adapter BEFORE cellRemoved deletes the adapter
    SmdCell *cell = m_cells[index];
    if (auto *codeEditor = qobject_cast<CodeEditor*>(cell->editorWidget())) {
        codeEditor->setCompletionProvider(nullptr);
    }

    // Notify LSP manager before removal
    if (m_lspManager) {
        QString langId = SmdCell::langIdFromType(cell->cellType());
        if (langId == QStringLiteral("cpp") || langId == QStringLiteral("python"))
            m_lspManager->cellRemoved(index, langId);
    }

    SmdOutputWidget *output = m_outputWidgets[index];

    // Remove output widget first (higher layout index), then cell
    m_cellLayout->removeWidget(output);
    m_cellLayout->removeWidget(cell);
    m_cells.removeAt(index);
    m_outputWidgets.removeAt(index);
    output->deleteLater();
    cell->deleteLater();

    if (m_activeCellIndex >= m_cells.size())
        m_activeCellIndex = m_cells.size() - 1;
    if (m_activeCellIndex < 0)
        m_activeCellIndex = 0;

    setActiveCell(m_activeCellIndex);

    for (int i = 0; i < m_cells.size(); ++i) {
        disconnect(m_cells[i], &SmdCell::focusEntered, nullptr, nullptr);
        disconnect(m_cells[i], &SmdCell::contentChanged, nullptr, nullptr);
        disconnect(m_cells[i], &SmdCell::cellTypeChanged, nullptr, nullptr);
        connectCellSignals(m_cells[i], i);
    }
}

void SmdEditor::removeInsertScrollPad()
{
    if (m_insertScrollPad) {
        m_cellLayout->removeItem(m_insertScrollPad);
        delete m_insertScrollPad;
        m_insertScrollPad = nullptr;
    }
}

void SmdEditor::insertCellAbove()
{
    if (m_executingCell) return; // Block during execution
    int idx = m_activeCellIndex >= 0 ? m_activeCellIndex : 0;
    int originalIdx = m_activeCellIndex >= 0 ? m_activeCellIndex : 0;
    addCell(idx, SmdCell::Markdown);
    setActiveCell(idx);
    // Scroll after layout settles so the new cell is visible.
    QTimer::singleShot(0, this, [this, idx]() {
        if (idx < 0 || idx >= m_cells.size()) return;
        m_cellLayout->activate();
        int cellY = m_cells[idx]->mapTo(m_cellContainer, QPoint(0, 0)).y();
        int maxScroll = m_scrollArea->verticalScrollBar()->maximum();
        int target = cellY - 8;
        if (target < 0) target = 0;
        if (target > maxScroll) target = maxScroll;
        m_scrollArea->verticalScrollBar()->setValue(target);
    });
    showLanguageSelector(idx, true, originalIdx);
}

void SmdEditor::insertCellBelow()
{
    if (m_executingCell) return; // Block during execution
    int idx = m_activeCellIndex >= 0 ? m_activeCellIndex + 1 : m_cells.size();
    int originalIdx = m_activeCellIndex >= 0 ? m_activeCellIndex : 0;
    addCell(idx, SmdCell::Markdown);
    setActiveCell(idx);
    // Add temporary bottom padding so we have scroll room to show the
    // new cell even when it would land at the very bottom of the content.
    // Cleaned up by removeInsertScrollPad() called from language-popup
    // callbacks, with a 15 s timeout as fallback.
    removeInsertScrollPad();
    int vpH = m_scrollArea->viewport()->height();
    m_insertScrollPad = new QSpacerItem(0, vpH, QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_cellLayout->addSpacerItem(m_insertScrollPad);
    // Force layout NOW so that both setActiveCell's deferred scroll and
    // our own deferred scroll below see the updated content height.
    m_cellLayout->activate();
    QTimer::singleShot(15000, this, [this]() { removeInsertScrollPad(); });

    // Scroll after layout settles so the new cell is visible.
    QTimer::singleShot(0, this, [this, idx, vpH]() {
        if (idx < 0 || idx >= m_cells.size()) return;
        // ensureWidgetVisible forces the scroll area to update its internal
        // layout / scroll range.  activate() alone may not be enough because
        // QScrollArea reads the content size asynchronously.
        m_cellLayout->activate();
        m_scrollArea->ensureWidgetVisible(m_cells[idx], 0, 20);
        int cellTop = m_cells[idx]->mapTo(m_cellContainer, QPoint(0, 0)).y();
        int cellH = m_cells[idx]->height();
        int maxScroll = m_scrollArea->verticalScrollBar()->maximum();
        // Place the cell near the bottom of the viewport.
        int target = cellTop + cellH - vpH + 8;
        if (target < 0) target = 0;
        if (target > maxScroll) target = maxScroll;
        m_scrollArea->verticalScrollBar()->setValue(target);
    });
    showLanguageSelector(idx, true, originalIdx);
}

void SmdEditor::setActiveCell(int index)
{
    if (index < 0 || index >= m_cells.size())
        return;

    if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size()) {
        m_cells[m_activeCellIndex]->setActive(false);
        if (m_activeCellIndex < m_outputWidgets.size())
            m_outputWidgets[m_activeCellIndex]->clearSelection();
    }
    m_activeCellIndex = index;
    m_cells[index]->setActive(true);
    m_cells[index]->setCommandMode(m_commandMode);

    if (!m_clickSuppressScroll) {
        // Defer scroll by one event-cycle so that any pending layout
        // request (e.g. from addCell's insertWidget) is processed first.
        QTimer::singleShot(0, this, [this, idx = index]() {
            if (!m_clickSuppressScroll && idx >= 0 && idx < m_cells.size())
                m_scrollArea->ensureWidgetVisible(m_cells[idx], 0, 20);
        });
    }

    // Cancel pending post-render jump if user manually switched cells (Req 5)
    if (m_pendingRenderJumpIndex >= 0 && index != m_pendingRenderJumpIndex)
        m_pendingRenderJumpIndex = -1;

    if (m_lspManager && m_cells[index]->cellType() == SmdCell::Cpp)
        m_lspManager->focusCell(index);

    if (m_diagnosticsPanel && m_diagnosticsPanel->isVisible())
        m_diagnosticsPanel->scheduleRefresh();
}

SmdCell *SmdEditor::cellAt(int index) const
{
    if (index < 0 || index >= m_cells.size()) return nullptr;
    return m_cells[index];
}

int SmdEditor::activeCellCursorLine() const
{
    if (m_activeCellIndex < 0 || m_activeCellIndex >= m_cells.size()) return 0;
    return m_cells[m_activeCellIndex]->cursorLine();
}

int SmdEditor::activeCellCursorColumn() const
{
    if (m_activeCellIndex < 0 || m_activeCellIndex >= m_cells.size()) return 0;
    return m_cells[m_activeCellIndex]->cursorColumn();
}

void SmdEditor::setActiveCellCursor(int line, int column)
{
    if (m_activeCellIndex < 0 || m_activeCellIndex >= m_cells.size()) return;
    m_cells[m_activeCellIndex]->setCursorPosition(line, column);
}

QList<SmdFormat::Cell> SmdEditor::exportCells() const
{
    QList<SmdFormat::Cell> result;
    for (const auto *cell : m_cells) {
        SmdFormat::Cell fc;
        fc.type = SmdCell::langIdFromType(cell->cellType());
        fc.content = cell->content();
        fc.rendered = cell->isRendered();
        // output intentionally excluded
        result.append(fc);
    }
    return result;
}

// ---- Mode Management ----

void SmdEditor::enterCommandMode()
{
    m_commandMode = true;
    if (m_diagnosticsPanel)
        m_diagnosticsPanel->setVisible(false);
    for (int i = 0; i < m_cells.size(); ++i) {
        m_cells[i]->setCommandMode(true);
        m_cells[i]->setActive(i == m_activeCellIndex);
    }
    for (SmdOutputWidget *w : m_outputWidgets)
        w->clearSelection();
    setFocus();
}

void SmdEditor::enterEditMode()
{
    m_commandMode = false;
    if (m_activeCellIndex < 0 || m_activeCellIndex >= m_cells.size())
        return;

    for (SmdCell *c : m_cells) {
        c->setCommandMode(false);
        c->setActive(false);
    }
    m_cells[m_activeCellIndex]->setEditorFocus();
}

// ---- Language Selector ----

void SmdEditor::showLanguageSelector(int cellIndex, bool isNewCell, int originalCellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size())
        return;

    // Create popup first so we can capture it in both callbacks for
    // explicit deleteLater().  The popup's hideEvent no longer calls
    // deleteLater() itself — we handle deletion in these callbacks to
    // prevent double-delete from duplicate hideEvent delivery (common
    // with Qt::Popup widgets).
    auto *popup = new LangSelectorPopup(nullptr,
        [this, cellIndex](SmdCell::CellType type) {
            setFocus();
            if (cellIndex >= 0 && cellIndex < m_cells.size()) {
                m_cells[cellIndex]->setCellType(type);
            }
        },
        nullptr   // onCancelled set below after we know popup address
    );

    // Wire onCancelled now that popup exists.
    // Qt::Popup implies WA_DeleteOnClose, so close()/hide() already call
    // deleteLater() for us.  We must NOT call it again in these callbacks
    // or we get a double-delete (the DeferredDelete fires twice for the
    // same object → crash or heap corruption).
    if (isNewCell) {
        int capturedIdx = cellIndex;
        int restoreIdx = originalCellIndex >= 0 ? originalCellIndex : cellIndex;
        popup->setOnCancelled([this, capturedIdx, restoreIdx]() {
            removeInsertScrollPad();
            removeCell(capturedIdx);
            setActiveCell(qMin(restoreIdx, m_cells.size() - 1));
            setModified(false);
        });
    }

    // Wire onSelected.
    popup->setOnSelected([this, cellIndex](SmdCell::CellType type) {
        removeInsertScrollPad();
        setFocus();
        if (cellIndex >= 0 && cellIndex < m_cells.size()) {
            m_cells[cellIndex]->setCellType(type);
        }
        if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size())
            enterEditMode();
    });

    QWidget *vp = m_scrollArea->viewport();
    QPoint vpTopLeft = vp->mapToGlobal(QPoint(0, 0));
    int popupX = vpTopLeft.x() + (vp->width() - popup->width()) / 2;
    int popupY = vpTopLeft.y() + 8;
    popup->move(qMax(vpTopLeft.x() + 4, popupX), popupY);
    popup->show();
    popup->setFocus();
}

// ---- Cell Signal Connections ----

void SmdEditor::connectCellSignals(SmdCell *cell, int index)
{
    connect(cell, &SmdCell::focusEntered, this, [this, index]() {
        setActiveCell(index);
    });

    // Install event filter on cell's editor widget for Esc / Ctrl+Enter
    if (QWidget *ed = cell->editorWidget())
        ed->installEventFilter(this);
    // Also install on the render image widget so Esc/Ctrl+Enter work
    // on rendered cells (where editorWidget() may switch to m_renderImage).
    if (QWidget *ri = cell->renderImageWidget())
        ri->installEventFilter(this);

    // Re-install event filter when cell type changes (editor is recreated)
    connect(cell, &SmdCell::cellTypeChanged, this, [this, cell, index](SmdCell::CellType oldType) {
        if (QWidget *ed = cell->editorWidget())
            ed->installEventFilter(this);
        // Notify LSP manager of type change
        if (m_lspManager) {
            QString oldLangId = SmdCell::langIdFromType(oldType);
            QString newLangId = SmdCell::langIdFromType(cell->cellType());
            m_lspManager->cellTypeChanged(index, oldLangId, newLangId, cell->content());
            // Wire shared provider to new CodeEditor
            if (newLangId == QStringLiteral("cpp") || newLangId == QStringLiteral("python")) {
                if (auto *codeEditor = qobject_cast<CodeEditor*>(cell->editorWidget())) {
                    auto *provider = m_lspManager->providerForCell(index, newLangId);
                    if (provider)
                        codeEditor->setCompletionProvider(provider);
                }
            }
        }
    });

    // Notify LSP of content changes
    connect(cell, &SmdCell::contentChanged, this, [this, cell, index]() {
        if (m_lspManager) {
            QString langId = SmdCell::langIdFromType(cell->cellType());
            if (langId == QStringLiteral("cpp") || langId == QStringLiteral("python"))
                m_lspManager->cellContentChanged(index, langId, cell->content());
            if (langId == QStringLiteral("cpp") && m_activeCellIndex >= 0)
                m_lspManager->focusCell(m_activeCellIndex);
        }
        if (m_diagnosticsPanel && m_diagnosticsPanel->isVisible())
            m_diagnosticsPanel->scheduleRefresh();
    });

    // Wire shared LSP provider to code cells
    if (m_lspManager) {
        QString langId = SmdCell::langIdFromType(cell->cellType());
        if (langId == QStringLiteral("cpp") || langId == QStringLiteral("python")) {
            if (auto *codeEditor = qobject_cast<CodeEditor*>(cell->editorWidget())) {
                auto *provider = m_lspManager->providerForCell(index, langId);
                if (provider)
                    codeEditor->setCompletionProvider(provider);
            }
        }
    }

    // Auto-scroll to keep cursor visible when cell height grows (e.g., Enter)
    connect(cell, &SmdCell::contentChanged, this, [this, cell]() {
        emit contentChanged();
        if (m_commandMode || m_cells.indexOf(cell) != m_activeCellIndex)
            return;
        if (auto *pte = qobject_cast<QPlainTextEdit*>(cell->editorWidget())) {
            QRect cr = pte->cursorRect();
            // Map cursor bottom to scroll area content widget coordinates
            QPoint cursorBottom = pte->viewport()->mapTo(m_cellContainer, cr.bottomLeft());
            m_scrollArea->ensureVisible(cursorBottom.x(), cursorBottom.y(), 0, 30);
        }
    });
}

// ---- Auto-Render ----

void SmdEditor::startAutoRender()
{
    if (m_autoRenderQueue.isEmpty())
        return;

    m_autoRenderIndex = 0;
    m_autoRenderTimer = new QTimer(this);
    m_autoRenderTimer->setInterval(200);

    connect(m_autoRenderTimer, &QTimer::timeout, this, [this]() {
        if (m_autoRenderIndex >= m_autoRenderQueue.size()) {
            m_autoRenderTimer->stop();
            m_autoRenderTimer->deleteLater();
            m_autoRenderTimer = nullptr;
            m_autoRenderQueue.clear();
            return;
        }
        SmdCell *cell = m_autoRenderQueue[m_autoRenderIndex];
        if (cell && !cell->isRendered()) {
            cell->setRendered(true);
            cell->setCommandMode(true);
        }
        ++m_autoRenderIndex;
    });

    m_autoRenderTimer->start();
}

// ---- Execution ----

void SmdEditor::executeCurrentCell()
{
    if (m_activeCellIndex < 0 || m_activeCellIndex >= m_cells.size())
        return;

    SmdCell *cell = m_cells[m_activeCellIndex];
    if (cell->cellType() == SmdCell::Markdown)
        executeMarkdownCell(cell);
    else
        executeCodeCell(cell);
}

void SmdEditor::executeMarkdownCell(SmdCell *cell)
{
    if (cell->content().trimmed().isEmpty()) {
        if (m_jumpAfterExecute)
            jumpToNextCell();
        return;
    }

    if (!cell->isRendered()) {
        m_pendingRenderJumpIndex = m_cells.indexOf(cell);
        connect(cell, &SmdCell::renderFinished, this, [this, cell]() {
            disconnect(cell, &SmdCell::renderFinished, this, nullptr);
            onCellRenderFinished();
        });
        cell->setRendered(true);
    } else {
        if (m_jumpAfterExecute)
            jumpToNextCell();
    }
}

int SmdEditor::cppGroupForCell(int cellIndex) const
{
    static const QRegularExpression mainRe(QStringLiteral("\\bmain\\s*\\("));
    int group = 0;
    for (int i = 0; i < cellIndex; ++i) {
        SmdCell *c = m_cells[i];
        if (c->cellType() != SmdCell::Cpp)
            continue;
        if (mainRe.match(c->content()).hasMatch())
            ++group;
    }
    return group;
}

void SmdEditor::executeCodeCell(SmdCell *cell)
{
    int execIndex = m_cells.indexOf(cell);
    bool isPython = (cell->cellType() == SmdCell::Python);
    QString ext = QStringLiteral("cpp");

    // Hide signature help popup before execution
    if (CodeEditor *ce = qobject_cast<CodeEditor *>(cell->editorWidget()))
        ce->hideSignatureHelp();

    // Skip if current cell is truly empty (original behavior).
    if (cell->content().trimmed().isEmpty()) {
        if (m_jumpAfterExecute)
            jumpToNextCell();
        return;
    }

    // Python cells use a persistent process with shared namespace
    // (Jupyter-like), so only the current cell's output is captured.
    if (isPython) {
        executePythonCell(cell);
        return;
    }

    // Combine C++ cells from the same program group up to the executed
    // cell.  Program groups are split on cells that contain a main()
    // function, so that independent programs in the same .smd file do
    // not interfere with each other.
    QString combinedCode;
    int execGroup = cppGroupForCell(execIndex);
    for (int i = 0; i <= execIndex; ++i) {
        SmdCell *c = m_cells[i];
        if (c->cellType() != SmdCell::Cpp)
            continue;
        if (cppGroupForCell(i) != execGroup)
            continue;
        QString content = c->content();
        if (!combinedCode.isEmpty() && !content.isEmpty())
            combinedCode += QLatin1Char('\n');
        if (!content.isEmpty())
            combinedCode += content;
    }

    if (combinedCode.trimmed().isEmpty()) {
        if (m_jumpAfterExecute)
            jumpToNextCell();
        return;
    }

    // Detect whether the combined code has a main() function.
    // If not, compile-only (no link) to avoid spurious linker errors
    // about undefined reference to `WinMain` / `main`.
    bool hasMain = false;
    QRegularExpression mainRe(QStringLiteral("\\bmain\\s*\\("));
    hasMain = mainRe.match(combinedCode).hasMatch();

    QString tempPath = QDir::tempPath()
        + QStringLiteral("/smd_cell_")
        + QString::number(QCoreApplication::applicationPid())
        + QStringLiteral("_")
        + QString::number(m_executeCounter++)
        + QStringLiteral(".")
        + ext;

    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (execIndex >= 0 && execIndex < m_outputWidgets.size()) {
            m_outputWidgets[execIndex]->clearOutput();
            m_outputWidgets[execIndex]->appendText(tr("Error: Cannot create temp file.\n"), true);
        }
        return;
    }
    file.write(combinedCode.toUtf8());
    file.close();

    m_executingCell = cell;
    m_executingTempFile = tempPath;

    // Clear previous output (visibility is handled by appendText when output arrives)
    if (execIndex >= 0 && execIndex < m_outputWidgets.size())
        m_outputWidgets[execIndex]->clearOutput();

    // Connect output
    m_execOutputConn = connect(m_processRunner, &ProcessRunner::outputReceived,
                               this, &SmdEditor::onProcessOutput);

    if (!hasMain) {
        // Compile-only — no main(), so linking would fail with
        // undefined reference to `WinMain` / `main`.
        m_executingCompileOnly = true;
        m_execCompileConn = connect(m_processRunner, &ProcessRunner::compileFinished, this,
            [this](bool success) {
                disconnect(m_execOutputConn);
                disconnect(m_execCompileConn);
                onProcessFinished(success ? 0 : -1);
            });
        m_processRunner->startCompileOnly(tempPath);
    } else {
        m_executingCompileOnly = false;
        m_execCompileConn = connect(m_processRunner, &ProcessRunner::compileFinished, this,
            [this](bool success) {
                if (!success) {
                    disconnect(m_execOutputConn);
                    disconnect(m_execCompileConn);
                    onProcessFinished(-1);
                }
            });
        m_execRunConn = connect(m_processRunner, &ProcessRunner::runFinished, this,
            [this](int exitCode) {
                disconnect(m_execOutputConn);
                disconnect(m_execCompileConn);
                disconnect(m_execRunConn);
                onProcessFinished(exitCode);
            });
        m_processRunner->startCompileAndRun(tempPath);
    }
}

void SmdEditor::onProcessOutput(const QString &text, bool isStderr)
{
    if (!m_executingCell) return;
    int idx = m_cells.indexOf(m_executingCell);
    if (idx >= 0 && idx < m_outputWidgets.size()) {
        m_outputWidgets[idx]->appendText(text, isStderr);
    }
}

void SmdEditor::onProcessFinished(int exitCode)
{
    Q_UNUSED(exitCode);

    if (!m_executingCell) return;
    int finishedIdx = m_cells.indexOf(m_executingCell);

    // Clean up temp files
    QFile::remove(m_executingTempFile);
    QFileInfo fi(m_executingTempFile);
    QString exePath = fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName()
                      + QStringLiteral(".exe");
    QFile::remove(exePath);
    if (m_executingCompileOnly) {
        QString objPath = fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName()
                          + QStringLiteral(".o");
        QFile::remove(objPath);
    }
    m_executingTempFile.clear();
    m_executingCell = nullptr;
    m_executingCompileOnly = false;

    // Scroll output to top so user sees the beginning (Req 1)
    if (finishedIdx >= 0 && finishedIdx < m_outputWidgets.size())
        m_outputWidgets[finishedIdx]->scrollToTop();

    emit contentChanged();

    if (finishedIdx == m_activeCellIndex) {
        if (m_jumpAfterExecute && !m_userTerminated)
            jumpToNextCell();
        m_userTerminated = false;
    }
}

void SmdEditor::jumpToNextCell()
{
    if (m_activeCellIndex + 1 < m_cells.size()) {
        setActiveCell(m_activeCellIndex + 1);
    }
}

void SmdEditor::splitCellAtCursor()
{
    if (m_commandMode || m_executingCell)
        return;
    if (m_activeCellIndex < 0 || m_activeCellIndex >= m_cells.size())
        return;

    SmdCell *cell = m_cells[m_activeCellIndex];
    SmdCell::CellType type = cell->cellType();
    QString fullContent = cell->content();

    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit *>(cell->editorWidget());
    if (!editor)
        return;

    int pos = editor->textCursor().position();
    QString beforeText = fullContent.left(pos);
    QString afterText = fullContent.mid(pos);

    int oldIdx = m_activeCellIndex;
    cell->setContent(beforeText);
    addCell(oldIdx + 1, type, afterText);
    setActiveCell(oldIdx + 1);

    // Transfer output to the bottom cell after split
    if (oldIdx + 1 < m_outputWidgets.size()) {
        QString existingOutput = m_outputWidgets[oldIdx]->outputText();
        if (!existingOutput.isEmpty()) {
            m_outputWidgets[oldIdx + 1]->setOutput(existingOutput);
            m_outputWidgets[oldIdx]->clearOutput();
        }
    }

    // Focus the lower cell's editor with cursor at start
    SmdCell *newCell = m_cells[m_activeCellIndex];
    if (QWidget *ed = newCell->editorWidget()) {
        ed->setFocus();
        if (auto *pte = qobject_cast<QPlainTextEdit *>(ed))
            pte->moveCursor(QTextCursor::Start);
    }

    // Two-phase deferred height update: the outer layout assigns cell
    // widths in the first event-cycle, but the inner cascade (cell →
    // QStackedWidget → QPlainTextEdit → QTextDocument) needs a second
    // cycle to propagate the width and re-layout the document.
    QTimer::singleShot(0, this, [this, cell, newCell]() {
        cell->updateEditorHeight();
        newCell->updateEditorHeight();
        QTimer::singleShot(0, this, [this, cell, newCell]() {
            cell->updateEditorHeight();
            newCell->updateEditorHeight();
            // After height settles, scroll cursor into view with room
            // above for the cell header bar.
            QTimer::singleShot(0, this, [this, newCell]() {
                if (auto *pte = qobject_cast<QPlainTextEdit*>(newCell->editorWidget())) {
                    QRect cr = pte->cursorRect();
                    QPoint pt = pte->viewport()->mapTo(m_cellContainer, cr.topLeft());
                    m_scrollArea->ensureVisible(pt.x(), pt.y(), 0, 50);
                }
            });
        });
    });
}

// ---- Persistent Python Execution (Jupyter-like) ----

void SmdEditor::executePythonCell(SmdCell *cell)
{
    int execIndex = m_cells.indexOf(cell);

    // Start persistent process on first use
    if (!m_pyExecProcess || m_pyExecProcess->state() != QProcess::Running) {
        startPythonExecProcess();
        if (!m_pyExecProcess || m_pyExecProcess->state() != QProcess::Running) {
            if (execIndex >= 0 && execIndex < m_outputWidgets.size()) {
                m_outputWidgets[execIndex]->clearOutput();
                m_outputWidgets[execIndex]->appendText(
                    tr("Error: Python execution backend is not running.\n"), true);
            }
            return;
        }
    }

    m_executingCell = cell;
    m_executingTempFile.clear();

    // Clear previous output (visibility is handled by appendText when output arrives)
    if (execIndex >= 0 && execIndex < m_outputWidgets.size())
        m_outputWidgets[execIndex]->clearOutput();

    // Send code to persistent Python process.
    // Base64-encode to avoid JSON newline escaping issues.
    QString code = cell->content();
    code.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    code.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    for (int i = 0; i < code.size(); ++i) {
        QChar ch = code.at(i);
        if (ch.isLowSurrogate() && (i == 0 || !code.at(i - 1).isHighSurrogate()))
            code[i] = QChar(QChar::ReplacementCharacter);
        else if (ch.isHighSurrogate()
                 && (i + 1 >= code.size() || !code.at(i + 1).isLowSurrogate()))
            code[i] = QChar(QChar::ReplacementCharacter);
    }

    QJsonObject req;
    req[QStringLiteral("action")] = QStringLiteral("exec");
    req[QStringLiteral("code")] = QString::fromLatin1(code.toUtf8().toBase64());

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_pyExecProcess->write(payload);
}

void SmdEditor::startPythonExecProcess()
{
    if (m_pyExecProcess) return;

    // Look for python_executor.py next to the app, one dir up, or in CWD
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + QStringLiteral("/python_executor.py"),
        appDir + QStringLiteral("/../python_executor.py"),
        QStringLiteral("python_executor.py"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c)) {
            m_pyExecScriptPath = QFileInfo(c).absoluteFilePath();
            break;
        }
    }
    if (m_pyExecScriptPath.isEmpty()) {
        qWarning() << "SmdEditor: python_executor.py not found";
        return;
    }

    QString python = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (python.isEmpty())
        python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (python.isEmpty()) {
        qWarning() << "SmdEditor: python not found";
        return;
    }

    m_pyExecProcess = new QProcess(this);
    m_pyExecProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_pyExecProcess, &QProcess::readyReadStandardOutput,
            this, &SmdEditor::onPyExecReadyRead);
    connect(m_pyExecProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SmdEditor::onPyExecFinished);
    connect(m_pyExecProcess, &QProcess::errorOccurred,
            this, &SmdEditor::onPyExecError);

    m_pyExecBuffer.clear();
    m_pyExecProcess->start(python, {m_pyExecScriptPath});

    if (!m_pyExecProcess->waitForStarted(5000)) {
        qWarning() << "SmdEditor: failed to start Python executor";
        m_pyExecProcess->deleteLater();
        m_pyExecProcess = nullptr;
    }
}

void SmdEditor::stopPythonExecProcess()
{
    if (!m_pyExecProcess) return;

    // Send exit command
    if (m_pyExecProcess->state() == QProcess::Running) {
        QJsonObject req;
        req[QStringLiteral("action")] = QStringLiteral("exit");
        QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
        m_pyExecProcess->write(payload);
        m_pyExecProcess->waitForBytesWritten(500);
    }

    m_pyExecProcess->disconnect();
    if (m_pyExecProcess->state() != QProcess::NotRunning) {
        m_pyExecProcess->kill();
        m_pyExecProcess->waitForFinished(200);
    }
    m_pyExecProcess->deleteLater();
    m_pyExecProcess = nullptr;
    m_pyExecBuffer.clear();
}

void SmdEditor::onPyExecReadyRead()
{
    if (!m_pyExecProcess) return;

    m_pyExecBuffer.append(m_pyExecProcess->readAllStandardOutput());

    // Process complete lines
    while (true) {
        int newlineIdx = m_pyExecBuffer.indexOf('\n');
        if (newlineIdx < 0) break;

        QByteArray line = m_pyExecBuffer.left(newlineIdx).trimmed();
        m_pyExecBuffer.remove(0, newlineIdx + 1);

        if (line.isEmpty()) continue;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "SmdEditor: invalid JSON from Python executor:" << line;
            continue;
        }

        QJsonObject reply = doc.object();
        bool ok = reply.value(QStringLiteral("ok")).toBool();
        QString stdout_ = reply.value(QStringLiteral("stdout")).toString();
        QString stderr_ = reply.value(QStringLiteral("stderr")).toString();
        QString error = reply.value(QStringLiteral("error")).toString();

        // Route output to the executing cell
        if (m_executingCell) {
            int idx = m_cells.indexOf(m_executingCell);
            if (idx >= 0 && idx < m_outputWidgets.size()) {
                if (!stdout_.isEmpty())
                    m_outputWidgets[idx]->appendText(stdout_, false);
                if (!stderr_.isEmpty())
                    m_outputWidgets[idx]->appendText(stderr_, true);
                if (!ok && !error.isEmpty())
                    m_outputWidgets[idx]->appendText(error, true);
                m_outputWidgets[idx]->scrollToTop();
            }
        }

        // Finish execution
        int finishedIdx = m_executingCell ? m_cells.indexOf(m_executingCell) : -1;
        m_executingCell = nullptr;

        emit contentChanged();

        // Jump to next cell
        if (finishedIdx == m_activeCellIndex) {
            if (m_jumpAfterExecute && !m_userTerminated)
                jumpToNextCell();
            m_userTerminated = false;
        }
    }
}

void SmdEditor::onPyExecFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    if (!m_pyExecProcess) return;

    // If there's a pending cell, show error
    if (m_executingCell) {
        int idx = m_cells.indexOf(m_executingCell);
        if (idx >= 0 && idx < m_outputWidgets.size()) {
            if (status == QProcess::CrashExit)
                m_outputWidgets[idx]->appendText(
                    tr("Python execution backend crashed.\n"), true);
            else
                m_outputWidgets[idx]->appendText(
                    tr("Python execution backend exited unexpectedly.\n"), true);
        }
        m_executingCell = nullptr;
        emit contentChanged();
    }

    // Auto-restart after crash
    if (status == QProcess::CrashExit) {
        m_pyExecProcess->deleteLater();
        m_pyExecProcess = nullptr;
        m_pyExecBuffer.clear();
        QTimer::singleShot(1000, this, [this]() {
            if (m_lspManager)  // SmdEditor is still alive
                startPythonExecProcess();
        });
    }
}

void SmdEditor::onPyExecError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (!m_pyExecProcess) return;

    if (m_executingCell) {
        int idx = m_cells.indexOf(m_executingCell);
        if (idx >= 0 && idx < m_outputWidgets.size()) {
            m_outputWidgets[idx]->appendText(
                tr("Error communicating with Python execution backend.\n"), true);
        }
        m_executingCell = nullptr;
        emit contentChanged();
    }
}

// ---- Process Stop (Ctrl+C) ----

void SmdEditor::handleProcessStop()
{
    if (!m_executingCell) return;
    int stoppedIdx = m_cells.indexOf(m_executingCell);

    bool isPython = (m_executingCell->cellType() == SmdCell::Python);

    if (isPython) {
        // Kill and restart the persistent Python process
        if (m_pyExecProcess) {
            m_pyExecProcess->kill();
            m_pyExecProcess->waitForFinished(200);
            m_pyExecProcess->deleteLater();
            m_pyExecProcess = nullptr;
            m_pyExecBuffer.clear();
        }
    } else {
        // Clean up temp files for C++
        if (!m_executingTempFile.isEmpty()) {
            QFile::remove(m_executingTempFile);
            QFileInfo fi(m_executingTempFile);
            QString exePath = fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName()
                              + QStringLiteral(".exe");
            QFile::remove(exePath);
        }
        // Disconnect execution signal connections
        disconnect(m_execOutputConn);
        disconnect(m_execCompileConn);
        disconnect(m_execRunConn);
    }

    // Append termination message
    if (stoppedIdx >= 0 && stoppedIdx < m_outputWidgets.size()) {
        m_outputWidgets[stoppedIdx]->appendText(
            QStringLiteral("\n--- ") + tr("Terminated by user") + QStringLiteral(" ---\n"), true);
    }

    m_executingTempFile.clear();
    m_executingCell = nullptr;
    m_userTerminated = false;

    emit contentChanged();

    // Auto-restart Python process
    if (isPython) {
        QTimer::singleShot(500, this, [this]() {
            if (m_lspManager)
                startPythonExecProcess();
        });
    }
}

void SmdEditor::onCellRenderFinished()
{
    int jumpedIndex = m_pendingRenderJumpIndex;
    m_pendingRenderJumpIndex = -1;

    if (jumpedIndex == m_activeCellIndex && m_jumpAfterExecute)
        jumpToNextCell();
}

// ---- Event Handling ----

void SmdEditor::keyPressEvent(QKeyEvent *event)
{
    auto matchShortcut = [&](const QKeySequence &seq) -> bool {
        if (seq.isEmpty())
            return false;
        Qt::KeyboardModifiers mods = event->modifiers()
            & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
        return QKeySequence(mods | event->key()) == seq;
    };

    if (m_commandMode) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Plain Enter: enter edit mode. Ctrl/Shift+Enter are handled
            // by eventFilter in edit mode only.
            enterEditMode();
            return;

        case Qt::Key_Escape:
            return;

        default:
            break;
        }

        // Check configurable command-mode shortcuts
        if (matchShortcut(m_cellInsertAbove)) {
            insertCellAbove();
            return;
        }
        if (matchShortcut(m_cellInsertBelow)) {
            insertCellBelow();
            return;
        }
        if (matchShortcut(m_cellClearOutput)) {
            if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size()) {
                SmdCell *cell = m_cells[m_activeCellIndex];
                if (cell->cellType() != SmdCell::Markdown) {
                    m_outputWidgets[m_activeCellIndex]->clearOutput();
                    emit contentChanged();
                } else if (cell->isRendered()) {
                    cell->setRendered(false);
                }
            }
            return;
        }
        if (matchShortcut(m_cellDelete)) {
            if (m_activeCellIndex >= 0 && m_cells.size() > 1)
                removeCell(m_activeCellIndex);
            return;
        }

        // Legacy navigation keys (non-configurable)
        if (event->key() == Qt::Key_Up) {
            if (m_activeCellIndex > 0)
                setActiveCell(m_activeCellIndex - 1);
            return;
        }
        if (event->key() == Qt::Key_Down) {
            if (m_activeCellIndex < m_cells.size() - 1)
                setActiveCell(m_activeCellIndex + 1);
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

bool SmdEditor::eventFilter(QObject *obj, QEvent *event)
{
    // Any mouse click anywhere in the application suppresses scroll for a
    // short window.  This covers both clicks on cells (where we activate
    // without scrolling) and clicks on toolbars/chrome (where Qt may restore
    // focus to a cell and we want to suppress the ensuing FocusIn scroll).
    if (event->type() == QEvent::MouseButtonPress) {
        m_clickSuppressScroll = true;
        QTimer::singleShot(50, this, [this]() {
            m_clickSuppressScroll = false;
        });
    }

    // Activate the parent SmdCell on FocusIn / MouseButtonPress.
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
        if (auto *w = qobject_cast<QWidget*>(obj)) {
            for (QWidget *cur = w; cur; cur = cur->parentWidget()) {
                if (auto *cell = qobject_cast<SmdCell*>(cur)) {
                    int idx = m_cells.indexOf(cell);
                    if (idx >= 0 && idx != m_activeCellIndex) {
                        setActiveCell(idx);
                    }
                    break;
                }
            }
        }
    }

    // ── Keyboard shortcuts ──
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent*>(event);

        auto matchShortcut = [&](const QKeySequence &seq) -> bool {
            if (seq.isEmpty())
                return false;
            Qt::KeyboardModifiers mods = key->modifiers()
                & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
            return QKeySequence(mods | key->key()) == seq;
        };

        // Ctrl+Enter / Shift+Enter: execute current cell (edit mode only).
        if (!m_commandMode
            && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
            bool exec        = matchShortcut(m_cellExecute);
            bool execJump    = matchShortcut(m_cellExecuteJump);
            if (exec || execJump) {
                if (event->type() == QEvent::ShortcutOverride) {
                    event->accept();
                } else {
                    m_jumpAfterExecute = execJump;
                    executeCurrentCell();
                }
                return true;
            }
        }

        // Esc: always enter command mode, regardless of current mode.
        // Don't intercept when a Qt::Popup is active (e.g. language selector).
        if (key->key() == Qt::Key_Escape && !QApplication::activePopupWidget()) {
            if (event->type() == QEvent::ShortcutOverride)
                event->accept();
            else {
                enterCommandMode();
            }
            return true;
        }

        // Configurable shortcut checks
        auto handleOverrideOrAction = [&](const QKeySequence &seq, auto actionFn) -> bool {
            if (seq.isEmpty() || !matchShortcut(seq))
                return false;
            if (event->type() == QEvent::ShortcutOverride)
                event->accept();
            else
                actionFn();
            return true;
        };

        // Ctrl+K: show language selector in any mode (command or edit).
        if (handleOverrideOrAction(m_cellLanguage, [this]() {
            if (m_activeCellIndex >= 0)
                showLanguageSelector(m_activeCellIndex);
        })) return true;

        // Ctrl+C: terminate cell execution (only when a cell is executing).
        if (!m_cellTerminate.isEmpty() && matchShortcut(m_cellTerminate) && m_executingCell) {
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
            } else {
                m_userTerminated = true;
                m_processRunner->stop();
                handleProcessStop();
            }
            return true;
        }

        // Ctrl+Shift+Z: un-render MD cells, or clear output for code cells.
        // In command mode the event falls through to keyPressEvent.
        if (!m_cellClearOutput.isEmpty()) {
            bool matchClear = matchShortcut(m_cellClearOutput);
            if (matchClear) {
                if (event->type() == QEvent::ShortcutOverride) {
                    event->accept();
                    return true;
                }
                if (!m_commandMode) {
                    if (event->type() == QEvent::KeyPress) {
                        if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size()) {
                            SmdCell *cell = m_cells[m_activeCellIndex];
                            if (cell->cellType() != SmdCell::Markdown) {
                                m_outputWidgets[m_activeCellIndex]->clearOutput();
                                emit contentChanged();
                            } else if (cell->isRendered()) {
                                cell->setRendered(false);
                            }
                        }
                        return true;
                    }
                }
                // In command mode, KeyPress falls through to keyPressEvent
            }
        }

        // Ctrl+Shift+-: split cell at cursor (edit mode only).
        if (!m_commandMode
            && (key->key() == Qt::Key_Minus || key->key() == Qt::Key_Underscore)
            && matchShortcut(m_cellSplit)) {
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
                return true;
            }
            if (event->type() == QEvent::KeyPress) {
                splitCellAtCursor();
                return true;
            }
        }

        // Ctrl+E: toggle diagnostics panel (edit mode only).
        if (!m_commandMode && handleOverrideOrAction(m_toggleDiagnostics, [this]() {
            toggleDiagnosticsPanel();
        })) return true;

    }

    // ── Preserve scroll position across minimize/restore ──
    if (event->type() == QEvent::WindowStateChange) {
        auto *wsc = static_cast<QWindowStateChangeEvent*>(event);
        bool wasMinimized = (wsc->oldState() & Qt::WindowMinimized);
        bool nowMinimized = window()->isMinimized();

        if (!wasMinimized && nowMinimized) {
            m_savedScrollPos = m_scrollArea->verticalScrollBar()->value();
        } else if (wasMinimized && !nowMinimized) {
            int saved = m_savedScrollPos;
            m_scrollArea->setUpdatesEnabled(false);
            m_scrollArea->verticalScrollBar()->setValue(saved);
            QTimer::singleShot(10, this, [this, saved]() {
                m_scrollArea->verticalScrollBar()->setValue(saved);
                m_scrollArea->setUpdatesEnabled(true);
            });
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SmdEditor::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Check all cells for needed updates after layout settles
    QTimer::singleShot(0, this, [this]() {
        for (SmdCell *cell : m_cells) {
            if (cell->isRendered())
                cell->checkReRender();
            else
                cell->updateEditorHeight();
        }
    });
}

void SmdEditor::toggleDiagnosticsPanel()
{
    if (!m_diagnosticsPanel)
        return;
    bool currentlyVisible = m_diagnosticsPanel->isVisible();
    if (!currentlyVisible) {
        // Default panel height = ~1/3 of the total view.
        int total = m_splitter->height();
        int panelHeight = qMax(100, total / 3);
        m_splitter->setSizes({total - panelHeight, panelHeight});
    }
    m_diagnosticsPanel->setVisible(!currentlyVisible);
    if (!currentlyVisible)
        m_diagnosticsPanel->refresh();
}

#include "smdeditor.moc"
