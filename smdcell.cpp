#include "smdcell.h"
#include "codeeditor.h"
#include "languageutils.h"
#include "configmanager.h"
#include "settingsmanager.h"
#include <QTimer>
#include <QTextBlock>
#include <QTextLayout>
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QThread>

// Minimal HTML page that embeds KaTeX + Mermaid + marked.js for
// rendering Markdown cells with full math / diagram support.
// Uses %1 as placeholder for the base64-encoded markdown content.
static QString smdRenderTemplate()
{
    return QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<link rel=\"stylesheet\" href=\"katex.min.css\">"
        "<style>"
        "body{background:#1E1E1E;color:#D4D4D4;"
        "font-family:-apple-system,\"Microsoft YaHei\",sans-serif;"
        "line-height:1.7;padding:8px 12px;margin:0}"
        "h1,h2,h3,h4,h5,h6{color:#569CD6}"
        "a{color:#4EC9B0}"
        "pre{background:#2D2D30;padding:12px;border-radius:4px;overflow-x:auto}"
        "code{background:#2D2D30;padding:2px 6px;border-radius:3px;"
        "font-family:Consolas,\"Courier New\",monospace;font-size:0.9em}"
        "pre code{background:none;padding:0}"
        "table{border-collapse:collapse}"
        "th,td{border:1px solid #555;padding:6px 10px}"
        "th{background:#3c3c3c}"
        "blockquote{border-left:4px solid #569CD6;padding-left:16px;color:#aaa;margin-left:0}"
        "hr{border:none;border-top:1px solid #444}"
        "img{max-width:100%}"
        ".katex{color:#D4D4D4}"
        ".mermaid{text-align:center;margin:1em 0}"
        ".mermaid svg{max-width:100%;background:#fff;border-radius:4px;padding:4px}"
        ".task-list-item{list-style:none;margin-left:-1.5em}"
        "</style></head><body>"
        "<div id=\"preview\"></div>"
        "<script src=\"marked.min.js\"></script>"
        "<script src=\"katex.min.js\"></script>"
        "<script src=\"mermaid.min.js\"></script>"
        "<script>"
        "mermaid.initialize({startOnLoad:false,theme:'default',securityLevel:'loose'});"
        "marked.use({breaks:true,gfm:true});"
        "var r={code:function(t){"
        "if(t.lang==='mermaid')return '<div class=\"mermaid\">'+t.text+'</div>';"
        "var c=t.text.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');"
        "return '<pre><code>'+c+'</code></pre>';"
        "}};marked.use({renderer:r});"
        "var b64=\"%1\";"
        "var bin=atob(b64),bytes=new Uint8Array(bin.length);"
        "for(var i=0;i<bin.length;i++)bytes[i]=bin.charCodeAt(i);"
        "var md=new TextDecoder('utf-8').decode(bytes);"
        "var mb=[];"
        "function sm(latex,dm){"
        "var idx=mb.length;"
        "try{mb[idx]=katex.renderToString(latex.trim(),{displayMode:dm,throwOnError:false});}"
        "catch(e){mb[idx]='<span style=\"color:#f88\">KaTeX: '+(e.message||e)+'</span>';}"
        "return '\\x00MATH'+idx+'\\x00';"
        "}"
        "md=md.replace(/\\$\\$([\\s\\S]*?)\\$\\$/g,function(m,latex){return sm(latex,true);});"
        "md=md.replace(/\\\\\\[([\\s\\S]*?)\\\\\\]/g,function(m,latex){return sm(latex,true);});"
        "md=md.replace(/\\\\\\(([\\s\\S]*?)\\\\\\)/g,function(m,latex){return sm(latex,false);});"
        "md=md.replace(/\\$([^$\\n]+?)\\$/g,function(m,latex){return sm(latex,false);});"
        "var html=marked.parse(md);"
        "for(var i=0;i<mb.length;i++)html=html.split('\\x00MATH'+i+'\\x00').join(mb[i]);"
        "document.getElementById('preview').innerHTML=html;"
        "(async function(){"
        "var els=document.querySelectorAll('.mermaid');"
        "for(var i=0;i<els.length;i++){"
        "try{"
        "var mr=await mermaid.render('mermaid-svg-'+i,els[i].textContent.trim());"
        "els[i].innerHTML=mr.svg;"
        "}catch(e){els[i].innerHTML='<span style=\"color:#f88\">Mermaid: '+(e.message||e)+'</span>';}"
        "}"
        "})();"
        "</script></body></html>"
    );
}

// Write debug log to release/smd_render_debug.log
static void smdDebugLog(const QString &msg)
{
    QFile file(QCoreApplication::applicationDirPath()
               + QStringLiteral("/smd_render_debug.log"));
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&file);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
           << QStringLiteral(" [%1] ").arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16)
           << msg << QStringLiteral("\n");
        file.close();
    }
}

SmdCell::SmdCell(CellType type, const QString &content, QWidget *parent)
    : QFrame(parent)
    , m_type(type)
    , m_languageId(langIdFromType(type))
{
    setupUi(type);
    if (!content.isEmpty())
        setContent(content);
}

void SmdCell::setupUi(CellType type)
{
    setFrameStyle(QFrame::NoFrame);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(24);
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(8, 2, 8, 2);
    headerLayout->setSpacing(6);

    m_typeLabel = new QLabel(m_headerBar);
    m_typeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; font-size: 10px; padding: 1px 6px; "
        "border-radius: 3px; background: %1; }"
    ).arg(m_type == Markdown ? QStringLiteral("#3a6ea5")
          : m_type == Cpp ? QStringLiteral("#2d8a56") : QStringLiteral("#b8952e")));
    headerLayout->addWidget(m_typeLabel);

    headerLayout->addStretch();

    m_executeHint = new QLabel(m_headerBar);
    m_executeHint->setStyleSheet(QStringLiteral("color: #858585; font-size: 10px;"));
    m_executeHint->setVisible(false);
    headerLayout->addWidget(m_executeHint);

    mainLayout->addWidget(m_headerBar);

    // Editor/View stack — no stretch, height driven by content
    m_editorStack = new QStackedWidget(this);
    m_editorStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (type == Markdown)
        setupMarkdownEditor();
    else
        setupCodeEditor(m_languageId);

    // Render view — page 1 = QWebEngineView placeholder, page 2 = QLabel
    // for grabbed pixmap (no native window → no scroll ghosting).
    // Both are created lazily in ensureRenderView().
    m_renderPlaceholder = new QWidget(this);
    m_renderPlaceholder->setStyleSheet(QStringLiteral("background-color: #1E1E1E;"));
    m_editorStack->addWidget(m_renderPlaceholder);   // page 1

    // Page 2: QLabel that replaces QWebEngineView after grab (no native HWND)
    m_renderImage = new QLabel(this);
    m_renderImage->setStyleSheet(QStringLiteral("background-color: #1E1E1E;"));
    m_renderImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_renderImage->installEventFilter(this);
    m_editorStack->addWidget(m_renderImage);          // page 2

    mainLayout->addWidget(m_editorStack);

    // Output area (hidden by default)
    m_outputArea = new QPlainTextEdit(this);
    m_outputArea->setReadOnly(true);
    m_outputArea->setMaximumBlockCount(
        ConfigManager::instance().outputPanelMaxBlocks());
    m_outputArea->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; "
        "selection-background-color: #264F78; border: none; "
        "border-top: 1px solid #3c3c3c; }"
    ));
    QFont outFont(QStringLiteral("Consolas"), 10);
    outFont.setStyleHint(QFont::Monospace);
    m_outputArea->setFont(outFont);
    m_outputArea->setVisible(false);
    m_outputArea->setMinimumHeight(40);
    m_outputArea->setMaximumHeight(200);
    mainLayout->addWidget(m_outputArea);

    updateTypeLabel();
    updateBorderStyle();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Catch Move events on self to repaint QWebEngineView during scroll
    installEventFilter(this);
}

void SmdCell::setupMarkdownEditor()
{
    m_markdownEditor = new QPlainTextEdit(this);
    m_markdownEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_markdownEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_markdownEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_markdownEditor->document()->setDocumentMargin(0);
    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();
    QString family = sm.value("editor.font.family", cfg.editorFontFamily()).toString();
    int size = sm.value("editor.font.size", cfg.editorFontSize()).toInt();
    QFont f(family, size);
    f.setStyleHint(QFont::Monospace);
    m_markdownEditor->setFont(f);
    m_markdownEditor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; "
        "selection-background-color: #264F78; border: none; padding: 8px; }"
    ));
    m_markdownEditor->setTabChangesFocus(false);

    connect(m_markdownEditor->document(), &QTextDocument::blockCountChanged,
            this, &SmdCell::updateEditorHeight);
    connect(m_markdownEditor->document(), &QTextDocument::contentsChanged,
            this, &SmdCell::updateEditorHeight);
    m_markdownEditor->installEventFilter(this);
    m_editorStack->insertWidget(0, m_markdownEditor);
    QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
}

void SmdCell::setupCodeEditor(const QString &langId)
{
    m_codeEditor = new CodeEditor(this);
    m_codeEditor->setLanguage(langId);
    m_codeEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_codeEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_codeEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_codeEditor->setTabChangesFocus(false);
    m_codeEditor->document()->setDocumentMargin(0);

    connect(m_codeEditor->document(), &QTextDocument::blockCountChanged,
            this, &SmdCell::updateEditorHeight);
    connect(m_codeEditor->document(), &QTextDocument::contentsChanged,
            this, &SmdCell::updateEditorHeight);
    m_codeEditor->installEventFilter(this);
    m_editorStack->insertWidget(0, m_codeEditor);
    QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
}

void SmdCell::setCellType(CellType type)
{
    if (m_type == type)
        return;

    QString oldContent = content();
    m_type = type;
    m_languageId = langIdFromType(type);
    m_rendered = false;

    // Remove old editor from stack
    if (m_markdownEditor) {
        m_editorStack->removeWidget(m_markdownEditor);
        m_markdownEditor->deleteLater();
        m_markdownEditor = nullptr;
    }
    if (m_codeEditor) {
        m_editorStack->removeWidget(m_codeEditor);
        m_codeEditor->deleteLater();
        m_codeEditor = nullptr;
    }

    if (type == Markdown) {
        setupMarkdownEditor();
    } else {
        setupCodeEditor(m_languageId);
    }

    m_editorStack->setCurrentIndex(0);
    if (!oldContent.isEmpty())
        setContent(oldContent);
    updateEditorHeight();

    updateTypeLabel();
    updateBorderStyle();
    emit cellTypeChanged();
}

QString SmdCell::content() const
{
    if (m_type == Markdown)
        return m_markdownEditor ? m_markdownEditor->toPlainText() : QString();
    return m_codeEditor ? m_codeEditor->toPlainText() : QString();
}

void SmdCell::setContent(const QString &text)
{
    if (m_type == Markdown && m_markdownEditor)
        m_markdownEditor->setPlainText(text);
    else if (m_codeEditor)
        m_codeEditor->setPlainText(text);
    QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
}

bool SmdCell::isModified() const
{
    if (m_type == Markdown && m_markdownEditor)
        return m_markdownEditor->document()->isModified();
    if (m_codeEditor)
        return m_codeEditor->document()->isModified();
    return false;
}

void SmdCell::setCommandMode(bool cmd)
{
    m_commandMode = cmd;
    updateBorderStyle();
    if (cmd) {
        if (m_rendered) {
            m_executeHint->setText(QStringLiteral("Ctrl+Shift+Z: 编辑"));
            m_executeHint->setVisible(true);
        } else if (m_type == Markdown) {
            m_executeHint->setText(QStringLiteral("Ctrl+Enter: 渲染"));
            m_executeHint->setVisible(true);
        } else {
            m_executeHint->setVisible(false);
        }
    } else {
        m_executeHint->setVisible(false);
    }
}

void SmdCell::setActive(bool active)
{
    m_active = active;
    updateBorderStyle();
    if (m_codeEditor) {
        if (active)
            m_codeEditor->refreshCurrentLineHighlight();
        else
            m_codeEditor->clearCurrentLineHighlight();
    }
}

void SmdCell::ensureRenderView()
{
    if (m_renderView)
        return;

    smdDebugLog(QStringLiteral("ensureRenderView — creating QWebEngineView"));

    m_renderView = new QWebEngineView(this);
    m_renderView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_renderView->setAttribute(Qt::WA_NoSystemBackground, true);
    m_renderView->page()->setBackgroundColor(QColor(QStringLiteral("#1E1E1E")));
    m_renderView->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    connect(m_renderView->page(), &QWebEnginePage::loadFinished,
            this, &SmdCell::onRenderLoadFinished);
    m_renderView->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() {
        if (QWidget *fp = m_renderView->focusProxy())
            fp->installEventFilter(this);
    });

    // Replace placeholder at page 1 with the live view
    if (m_renderPlaceholder) {
        m_editorStack->removeWidget(m_renderPlaceholder);
        m_renderPlaceholder->deleteLater();
        m_renderPlaceholder = nullptr;
    }
    m_editorStack->insertWidget(1, m_renderView);
}

void SmdCell::setRendered(bool rendered)
{
    if (m_type != Markdown)
        return;
    m_rendered = rendered;
    if (rendered) {
        ensureRenderView();

        int viewW = m_editorStack->width();
        if (viewW < 100) viewW = 600;
        int initH = 60;
        if (m_markdownEditor && m_markdownEditor->height() > initH)
            initH = m_markdownEditor->height();

        smdDebugLog(QStringLiteral("setRendered(true) — stackW=%1, editorH=%2, initSize=%3x%4")
            .arg(m_editorStack->width()).arg(m_markdownEditor ? m_markdownEditor->height() : -1)
            .arg(viewW).arg(initH));

        m_renderView->setFixedSize(viewW, initH);
        m_editorStack->setFixedHeight(initH);

        m_renderView->setAttribute(Qt::WA_NativeWindow, true);
        m_renderView->winId();

        // Use the same preview template that powers the full Markdown preview.
        // It already handles KaTeX + Mermaid + marked.js and is proven to work.
        QFile tmplFile(QStringLiteral(":/preview/template.html"));
        QString tmpl;
        if (tmplFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            tmpl = QString::fromUtf8(tmplFile.readAll());
            tmplFile.close();
        }
        if (tmpl.isEmpty()) {
            // Fallback: inline template (should never happen)
            smdDebugLog(QStringLiteral("setRendered — template read FAILED"));
            tmpl = smdRenderTemplate().arg(
                QString::fromLatin1(content().toUtf8().toBase64()));
        } else {
            QString safeContent = content();
            safeContent.replace(QStringLiteral("</script>"), QStringLiteral("<\\/script>"));
            tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), safeContent);
            // Tighten padding for cell display (full preview uses 24px 32px)
            tmpl.replace(QStringLiteral("padding: 24px 32px;"),
                         QStringLiteral("padding: 8px 12px;"));
            // Remove max-width so content fills the cell width
            tmpl.replace(QStringLiteral("max-width: 960px;"),
                         QStringLiteral(""));
        }

        smdDebugLog(QStringLiteral("setRendered(true) — after winId, size=%1x%2, htmlLen=%3")
            .arg(m_renderView->width()).arg(m_renderView->height()).arg(tmpl.size()));

        m_renderView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));
        m_editorStack->setCurrentIndex(1);
    } else {
        smdDebugLog(QStringLiteral("setRendered(false) — stop+switch to editor"));
        if (m_renderView)
            m_renderView->stop();
        if (m_renderImage)
            m_renderImage->clear();
        m_editorStack->setCurrentIndex(0);
        if (m_markdownEditor)
            m_markdownEditor->setFocus();
        updateEditorHeight();
    }
    if (m_commandMode) {
        if (rendered) {
            m_executeHint->setText(QStringLiteral("Ctrl+Shift+Z: 编辑"));
        } else {
            m_executeHint->setText(QStringLiteral("Ctrl+Enter: 渲染"));
        }
        m_executeHint->setVisible(true);
    }
}

void SmdCell::applyRenderHeight(int contentH)
{
    if (!m_rendered || !m_renderView || contentH <= 0)
        return;
    int totalH = contentH + 4;
    smdDebugLog(QStringLiteral("applyRenderHeight — contentH=%1, totalH=%2")
        .arg(contentH).arg(totalH));

    m_renderView->setFixedHeight(totalH);
    m_editorStack->setFixedHeight(totalH);
    updateGeometry();
    emit contentChanged();
}

void SmdCell::onRenderLoadFinished(bool ok)
{
    smdDebugLog(QStringLiteral("onRenderLoadFinished — ok=%1, m_rendered=%2")
        .arg(ok).arg(m_rendered));

    if (!m_rendered) {
        smdDebugLog(QStringLiteral("onRenderLoadFinished — cell no longer rendered, ignoring"));
        return;
    }

    if (!ok) {
        smdDebugLog(QStringLiteral("onRenderLoadFinished — load FAILED, using fallback"));
        int lineCount = content().count(QLatin1Char('\n')) + 1;
        applyRenderHeight(qMax(60, lineCount * 22 + 32));
        return;
    }

    // --- Immediate measurement ---
    m_renderView->page()->runJavaScript(
        QStringLiteral("(function(){"
        "  var h=document.body.scrollHeight;"
        "  if(!h) h=document.documentElement.scrollHeight;"
        "  if(!h){var el=document.getElementById('preview');if(el)h=el.offsetHeight||el.scrollHeight;}"
        "  return h||0;"
        "})()"),
        [this](const QVariant &v) {
            smdDebugLog(QStringLiteral("runJS immediate — raw=%1").arg(v.toInt()));
            if (v.isValid()) applyRenderHeight(v.toInt());
        });

    // --- Delayed re-measure for async Mermaid rendering ---
    for (int delayMs : {600, 1500}) {
        QTimer::singleShot(delayMs, this, [this]() {
            if (!m_rendered) return;
            m_renderView->page()->runJavaScript(
                QStringLiteral("(function(){"
                "  var h=document.body.scrollHeight;"
                "  if(!h) h=document.documentElement.scrollHeight;"
                "  if(!h){var el=document.getElementById('preview');if(el)h=el.offsetHeight||el.scrollHeight;}"
                "  return h||0;"
                "})()"),
                [this](const QVariant &v) {
                    smdDebugLog(QStringLiteral("runJS delayed — raw=%1").arg(v.toInt()));
                    if (v.isValid()) applyRenderHeight(v.toInt());
                });
        });
    }

    // --- One-time grab at 1800ms: replace QWebEngineView with QLabel ---
    // By now Mermaid has rendered. QLabel has no native HWND → no ghosting.
    QTimer::singleShot(1800, this, [this]() {
        if (!m_rendered || !m_renderView || !m_renderImage) return;
        int vh = m_renderView->height();
        if (vh < 40) return;
        smdDebugLog(QStringLiteral("grab — view=%1x%2").arg(m_renderView->width()).arg(vh));
        QPixmap pm = m_renderView->grab();
        if (pm.isNull() || pm.height() <= 0) { smdDebugLog(QStringLiteral("grab FAILED")); return; }
        qreal dpr = m_renderView->devicePixelRatioF();
        int lw = qRound(pm.width() / dpr);
        int lh = qRound(pm.height() / dpr);
        smdDebugLog(QStringLiteral("grab OK — %1x%2").arg(lw).arg(lh));
        m_renderImage->setPixmap(pm);
        m_renderImage->setScaledContents(true);
        m_renderImage->setFixedSize(lw, lh);
        m_editorStack->setFixedHeight(lh);
        m_editorStack->setCurrentIndex(2);
        updateGeometry();
        emit contentChanged();
    });
}

void SmdCell::showOutput(const QString &text, bool isStderr)
{
    m_outputArea->setVisible(true);
    m_outputArea->clear();
    appendOutput(text, isStderr);
}

void SmdCell::appendOutput(const QString &text, bool isStderr)
{
    m_outputArea->setVisible(true);
    QTextCursor cursor = m_outputArea->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (isStderr) {
        QTextCharFormat fmt;
        fmt.setForeground(QColor(QStringLiteral("#F48771")));
        cursor.insertText(text, fmt);
    } else {
        cursor.insertText(text);
    }
    m_outputArea->ensureCursorVisible();
}

void SmdCell::clearOutput()
{
    m_outputArea->clear();
}

void SmdCell::hideOutput()
{
    m_outputArea->setVisible(false);
    m_outputArea->clear();
}

QWidget *SmdCell::editorWidget() const
{
    if (m_rendered) {
        // After one-time grab, QLabel (page 2) replaces QWebEngineView
        if (m_renderImage && !m_renderImage->pixmap().isNull())
            return m_renderImage;
        return m_renderView;
    }
    if (m_type == Markdown)
        return m_markdownEditor;
    return m_codeEditor;
}

void SmdCell::setEditorFocus()
{
    if (m_rendered) {
        setRendered(false);
        return;
    }
    QWidget *w = editorWidget();
    if (w) {
        w->setFocus();
        if (auto *pte = qobject_cast<QPlainTextEdit*>(w))
            pte->moveCursor(QTextCursor::End);
    }
}

void SmdCell::applyZoom(qreal factor, int baseFontSize)
{
    const auto &cfg = ConfigManager::instance();
    int pointSize = qBound(cfg.fontMinPointSize(),
                           qRound(baseFontSize * factor),
                           cfg.fontMaxPointSize());

    if (m_markdownEditor) {
        QFont f = m_markdownEditor->font();
        f.setPointSize(pointSize);
        m_markdownEditor->setFont(f);
    }
    if (m_codeEditor) {
        QFont f = m_codeEditor->font();
        f.setPointSize(pointSize);
        m_codeEditor->setFont(f);
        m_codeEditor->refreshLineNumberArea();
    }

    // repaint() only lays out blocks visible in the current viewport. After
    // setFont() the viewport is sized for the old font — blocks outside the
    // visible area won't be relaid out, causing layout->boundingRect() to
    // return 0 and forcing fallback to font metrics. Temporarily stretch the
    // editor so repaint() covers ALL blocks, then measure the correct height.
    QPlainTextEdit *ed = m_markdownEditor
        ? static_cast<QPlainTextEdit*>(m_markdownEditor)
        : static_cast<QPlainTextEdit*>(m_codeEditor);
    if (ed) {
        ed->setFixedHeight(50000);
        m_editorStack->setFixedHeight(50000);
        ed->repaint();
    }

    updateEditorHeight();
}

void SmdCell::updateEditorHeight()
{
    QPlainTextEdit *ed = nullptr;
    if (m_rendered) {
        if (!m_renderView) {
            updateGeometry();
            return;
        }
        // The WebEngine view height is set asynchronously by
        // onRenderLoadFinished() after the page loads + Mermaid renders.
        // Keep whatever height is currently set and just sync the stack.
        smdDebugLog(QStringLiteral("updateEditorHeight(rendered) — viewSize=%1x%2, stackH=%3")
            .arg(m_renderView->width()).arg(m_renderView->height())
            .arg(m_editorStack->height()));
        m_editorStack->setFixedHeight(m_renderView->height());
        updateGeometry();
        return;
    }
    if (m_type == Markdown)
        ed = m_markdownEditor;
    else if (m_codeEditor)
        ed = m_codeEditor;

    if (!ed)
        return;

    // Measure actual content height by summing each block's QTextLayout
    // bounding rect. QFontMetrics::lineSpacing() is integer-truncated and
    // differs from QPlainTextEdit's internal layout by ~0.1px/line, which
    // accumulates to visible scroll overflow on multi-line blocks.
    QFontMetricsF fmf(ed->font());
    qreal fallbackLH = fmf.lineSpacing();
    qreal totalDocH = 0;
    int visualLines = 0;

    for (QTextBlock block = ed->document()->begin(); block.isValid(); block = block.next()) {
        QTextLayout *layout = block.layout();
        int lc = 1;
        if (layout) {
            lc = layout->lineCount();
            if (lc < 1) lc = 1;
            qreal h = layout->boundingRect().height();
            if (h > 0)
                totalDocH += h;
            else
                totalDocH += fallbackLH * lc;
        } else {
            totalDocH += fallbackLH * lc;
        }
        visualLines += lc;
    }
    if (visualLines < 1)
        visualLines = 1;

    QMargins cm = ed->contentsMargins();
    int marginH = cm.top() + cm.bottom();
    int contentH = qCeil(totalDocH) + marginH + 2;
    int minH = qCeil(fallbackLH) + marginH + 2;
    if (contentH < minH)
        contentH = minH;

    ed->setFixedHeight(contentH);
    m_editorStack->setFixedHeight(contentH);
    updateGeometry();
    emit contentChanged();
}

void SmdCell::updateTypeLabel()
{
    QString text;
    QString color;
    switch (m_type) {
    case Markdown:
        text = QStringLiteral("MD");
        color = QStringLiteral("#3a6ea5");
        break;
    case Cpp:
        text = QStringLiteral("C++");
        color = QStringLiteral("#2d8a56");
        break;
    case Python:
        text = QStringLiteral("Python");
        color = QStringLiteral("#b8952e");
        break;
    }
    m_typeLabel->setText(text);
    m_typeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; font-size: 10px; padding: 1px 6px; "
        "border-radius: 3px; background: %1; }"
    ).arg(color));
}

void SmdCell::updateBorderStyle()
{
    if (m_active) {
        setStyleSheet(QStringLiteral(
            "SmdCell { border: 2px solid #0078d4; "
            "background-color: #252526; }"
        ));
    } else if (m_commandMode) {
        setStyleSheet(QStringLiteral(
            "SmdCell { border: 2px solid #3c3c3c; "
            "background-color: #1E1E1E; }"
        ));
    } else {
        setStyleSheet(QStringLiteral(
            "SmdCell { border: 2px solid transparent; "
            "background-color: #1E1E1E; }"
        ));
    }
}

SmdCell::CellType SmdCell::typeFromLangId(const QString &langId)
{
    if (langId == QStringLiteral("cpp"))
        return Cpp;
    if (langId == QStringLiteral("python"))
        return Python;
    return Markdown;
}

bool SmdCell::eventFilter(QObject *obj, QEvent *event)
{
    // Repaint the QWebEngineView when the cell moves (scroll area scrolled)
    // to minimize native-window ghosting artifacts.
    if (obj == this && event->type() == QEvent::Move && m_rendered && m_renderView) {
        m_renderView->repaint();
    }

    // Log events on the render view / render image for debugging
    if ((m_renderView && (obj == m_renderView || obj == m_renderView->focusProxy()))
        || (m_renderImage && obj == m_renderImage)) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease) {
            const QString oName = (m_renderImage && obj == m_renderImage) ? QStringLiteral("renderImage")
                : (obj == m_renderView ? QStringLiteral("renderView") : QStringLiteral("focusProxy"));
            smdDebugLog(QStringLiteral("eventFilter — obj=%1, event=%2, m_rendered=%3")
                .arg(oName)
                .arg(event->type() == QEvent::FocusIn ? QStringLiteral("FocusIn")
                     : event->type() == QEvent::MouseButtonPress ? QStringLiteral("MousePress")
                     : QStringLiteral("MouseRelease"))
                .arg(m_rendered));
        }
    }

    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
        emit focusEntered();
    }
    return QFrame::eventFilter(obj, event);
}

QString SmdCell::langIdFromType(CellType type)
{
    switch (type) {
    case Markdown: return QStringLiteral("markdown");
    case Cpp:      return QStringLiteral("cpp");
    case Python:   return QStringLiteral("python");
    }
    return QStringLiteral("markdown");
}
