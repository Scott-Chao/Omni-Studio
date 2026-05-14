#include "smdeditor.h"
#include "smdcell.h"
#include "smdformat.h"
#include "smdoutputwidget.h"
#include "processrunner.h"
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

// ============================================================
// LangSelectorPopup — language selection popup for cell type
// ============================================================

class LangSelectorPopup : public QFrame
{
    Q_OBJECT
public:
    LangSelectorPopup(QWidget *parent, std::function<void(SmdCell::CellType)> onSelected)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_onSelected(onSelected)
    {
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
        if (m_onSelected)
            m_onSelected(selectedType());
        close();
        deleteLater();
    }

protected:
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
            deleteLater();
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
    std::function<void(SmdCell::CellType)> m_onSelected;
};

// ============================================================
// SmdEditor
// ============================================================

SmdEditor::SmdEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(this);
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
    mainLayout->addWidget(m_scrollArea);

    m_processRunner = new ProcessRunner(this);

    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);

    // Watch top-level window for minimize/restore to preserve scroll position
    QTimer::singleShot(0, this, [this]() {
        if (QWidget *tlw = window())
            tlw->installEventFilter(this);
    });
}

SmdEditor::~SmdEditor()
{
    m_processRunner->stop();
    if (m_autoRenderTimer) {
        m_autoRenderTimer->stop();
        delete m_autoRenderTimer;
        m_autoRenderTimer = nullptr;
    }
    // Remove event filter from top-level window
    if (QWidget *tlw = window())
        tlw->removeEventFilter(this);
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

    m_originalContent = text;
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

void SmdEditor::setPlainText(const QString &text)
{
    // Stop any in-progress auto-render
    if (m_autoRenderTimer) {
        m_autoRenderTimer->stop();
        m_autoRenderTimer->deleteLater();
        m_autoRenderTimer = nullptr;
    }
    m_autoRenderQueue.clear();

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

    if (!m_cells.isEmpty()) {
        for (SmdCell *c : m_cells)
            c->setActive(false);
        setActiveCell(0);
    }
    m_commandMode = false;

    // Pre-mark rendered flag so toPlainText() matches the file content.
    // Actual rendering happens async via auto-render — without this the
    // rendered→false→true transition would make isModified() trip falsely.
    for (SmdCell *cell : m_autoRenderQueue)
        cell->setRenderedState(true);

    // Start auto-render if there are cells to render
    if (!m_autoRenderQueue.isEmpty())
        startAutoRender();
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
    for (SmdCell *c : m_cells)
        c->applyZoom(factor, baseFontSize);
}

void SmdEditor::setEditorFont(const QString &family, int size)
{
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
        connectCellSignals(m_cells[i], i);
    }

    if (m_activeCellIndex >= layoutIdx)
        ++m_activeCellIndex;

    return cell;
}

void SmdEditor::removeCell(int index)
{
    if (index < 0 || index >= m_cells.size() || m_cells.size() <= 1)
        return;

    SmdCell *cell = m_cells[index];
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
        connectCellSignals(m_cells[i], i);
    }
}

void SmdEditor::insertCellAbove()
{
    int idx = m_activeCellIndex >= 0 ? m_activeCellIndex : 0;
    addCell(idx, SmdCell::Markdown);
    setActiveCell(idx);
    showLanguageSelector(idx);
}

void SmdEditor::insertCellBelow()
{
    int idx = m_activeCellIndex >= 0 ? m_activeCellIndex + 1 : m_cells.size();
    addCell(idx, SmdCell::Markdown);
    setActiveCell(idx);
    showLanguageSelector(idx);
}

void SmdEditor::setActiveCell(int index)
{
    if (index < 0 || index >= m_cells.size())
        return;
    if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size())
        m_cells[m_activeCellIndex]->setActive(false);
    m_activeCellIndex = index;
    m_cells[index]->setActive(true);
    m_cells[index]->setCommandMode(m_commandMode);
    m_scrollArea->ensureWidgetVisible(m_cells[index], 0, 20);
}

// ---- Mode Management ----

void SmdEditor::enterCommandMode()
{
    m_commandMode = true;
    for (int i = 0; i < m_cells.size(); ++i) {
        m_cells[i]->setCommandMode(true);
        m_cells[i]->setActive(i == m_activeCellIndex);
    }
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

void SmdEditor::showLanguageSelector(int cellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size())
        return;

    SmdCell *cell = m_cells[cellIndex];
    auto *popup = new LangSelectorPopup(m_cellContainer, [this, cellIndex](SmdCell::CellType type) {
        if (cellIndex >= 0 && cellIndex < m_cells.size()) {
            m_cells[cellIndex]->setCellType(type);
        }
        setFocus();
        QTimer::singleShot(0, this, [this]() {
            if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size())
                enterEditMode();
        });
    });

    QPoint pos = cell->mapToGlobal(QPoint(20, cell->height() / 2));
    popup->move(pos);
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

    // Re-install event filter when cell type changes (editor is recreated)
    connect(cell, &SmdCell::cellTypeChanged, this, [this, cell]() {
        if (QWidget *ed = cell->editorWidget())
            ed->installEventFilter(this);
    });

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
    if (!cell->isRendered()) {
        cell->setRendered(true);
    }
    if (!m_commandMode)
        enterCommandMode();
    jumpToNextCell();
}

void SmdEditor::executeCodeCell(SmdCell *cell)
{
    QString code = cell->content();
    if (code.trimmed().isEmpty()) {
        // Empty cell — just jump to next without execution
        if (!m_commandMode)
            enterCommandMode();
        jumpToNextCell();
        return;
    }

    bool isPython = (cell->cellType() == SmdCell::Python);
    QString ext = isPython ? QStringLiteral("py") : QStringLiteral("cpp");

    QString tempPath = QDir::tempPath()
        + QStringLiteral("/smd_cell_")
        + QString::number(QCoreApplication::applicationPid())
        + QStringLiteral("_")
        + QString::number(m_executeCounter++)
        + QStringLiteral(".")
        + ext;

    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        int idx = m_cells.indexOf(cell);
        if (idx >= 0 && idx < m_outputWidgets.size()) {
            m_outputWidgets[idx]->clearOutput();
            m_outputWidgets[idx]->appendText(tr("Error: Cannot create temp file.\n"), true);
        }
        return;
    }
    file.write(code.toUtf8());
    file.close();

    m_executingCellIndex = m_cells.indexOf(cell);
    m_executingTempFile = tempPath;

    // Clear previous output
    m_outputWidgets[m_executingCellIndex]->clearOutput();
    m_outputWidgets[m_executingCellIndex]->setVisible(true);

    // Connect output
    m_execOutputConn = connect(m_processRunner, &ProcessRunner::outputReceived,
                               this, &SmdEditor::onProcessOutput);

    if (isPython) {
        m_execRunConn = connect(m_processRunner, &ProcessRunner::runFinished, this,
            [this](int exitCode) {
                disconnect(m_execOutputConn);
                disconnect(m_execRunConn);
                onProcessFinished(exitCode);
            });
        m_processRunner->startRunPython(tempPath);
    } else {
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
    if (m_executingCellIndex >= 0 && m_executingCellIndex < m_outputWidgets.size()) {
        m_outputWidgets[m_executingCellIndex]->appendText(text, isStderr);
    }
}

void SmdEditor::onProcessFinished(int exitCode)
{
    Q_UNUSED(exitCode);

    int finishedIdx = m_executingCellIndex;

    // Clean up temp files
    QFile::remove(m_executingTempFile);
    QFileInfo fi(m_executingTempFile);
    QString exePath = fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName()
                      + QStringLiteral(".exe");
    QFile::remove(exePath);
    m_executingTempFile.clear();
    m_executingCellIndex = -1;

    // Output may have changed — let the modification indicator update
    emit contentChanged();

    // Only jump if the user is still on the executed cell
    if (finishedIdx == m_activeCellIndex) {
        if (!m_commandMode)
            enterCommandMode();
        jumpToNextCell();
    }
}

void SmdEditor::jumpToNextCell()
{
    if (m_commandMode) {
        if (m_activeCellIndex + 1 < m_cells.size()) {
            setActiveCell(m_activeCellIndex + 1);
        }
    }
}

// ---- Event Handling ----

void SmdEditor::keyPressEvent(QKeyEvent *event)
{
    if (m_commandMode) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (event->modifiers() & Qt::ControlModifier) {
                executeCurrentCell();
            } else {
                enterEditMode();
            }
            return;

        case Qt::Key_Escape:
            return;

        case Qt::Key_A:
            insertCellAbove();
            return;

        case Qt::Key_B:
            insertCellBelow();
            return;

        case Qt::Key_Up:
            if (m_activeCellIndex > 0)
                setActiveCell(m_activeCellIndex - 1);
            return;

        case Qt::Key_Down:
            if (m_activeCellIndex < m_cells.size() - 1)
                setActiveCell(m_activeCellIndex + 1);
            return;

        case Qt::Key_K:
            if (event->modifiers() & Qt::ControlModifier) {
                if (m_activeCellIndex >= 0)
                    showLanguageSelector(m_activeCellIndex);
                return;
            }
            break;

        case Qt::Key_Z:
            if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
                if (m_activeCellIndex >= 0 && m_activeCellIndex < m_cells.size()) {
                    SmdCell *cell = m_cells[m_activeCellIndex];
                    if (cell->isRendered())
                        cell->setRendered(false);
                }
                return;
            }
            break;

        case Qt::Key_Delete:
            if (m_activeCellIndex >= 0 && m_cells.size() > 1)
                removeCell(m_activeCellIndex);
            return;

        default:
            break;
        }
    }
    QWidget::keyPressEvent(event);
}

bool SmdEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent*>(event);

        if (!m_commandMode) {
            if (key->key() == Qt::Key_Escape) {
                if (event->type() == QEvent::ShortcutOverride)
                    event->accept();
                else
                    enterCommandMode();
                return true;
            }
            if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
                && (key->modifiers() & Qt::ControlModifier)) {
                if (event->type() == QEvent::ShortcutOverride) {
                    event->accept();
                } else {
                    executeCurrentCell();
                }
                return true;
            }
        }
    }

    // Preserve scroll position across minimize/restore
    if (event->type() == QEvent::WindowStateChange) {
        auto *wsc = static_cast<QWindowStateChangeEvent*>(event);
        bool wasMinimized = (wsc->oldState() & Qt::WindowMinimized);
        bool nowMinimized = window()->isMinimized();

        if (!wasMinimized && nowMinimized) {
            // About to minimize: save scroll position
            m_savedScrollPos = m_scrollArea->verticalScrollBar()->value();
        } else if (wasMinimized && !nowMinimized) {
            // Just restored: block paints, set scroll position, then re-enable.
            // This prevents the first frame from showing the wrong scroll position.
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
    // Check all rendered cells for needed re-renders after layout settles
    QTimer::singleShot(0, this, [this]() {
        for (SmdCell *cell : m_cells) {
            if (cell->isRendered())
                cell->checkReRender();
        }
    });
}

#include "smdeditor.moc"
