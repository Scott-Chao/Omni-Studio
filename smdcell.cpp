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

// Recursively find and log all widgets with native windows
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

    // Page 1: QLabel that replaces QWebEngineView after grab (no native HWND)
    m_renderImage = new QLabel(this);
    m_renderImage->setStyleSheet(QStringLiteral("background-color: #1E1E1E;"));
    m_renderImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_renderImage->installEventFilter(this);
    m_editorStack->addWidget(m_renderImage);          // page 1

    mainLayout->addWidget(m_editorStack);

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

    smdDebugLog(QStringLiteral("ensureRenderView — creating QWebEngineView as top-level window"));

    // Create as a SEPARATE TOP-LEVEL WINDOW, NOT a child of SmdCell.
    // This prevents Qt from cascading native windows to SmdCell and all ancestor
    // widgets (QScrollArea viewport, etc.) — the root cause of minimize/restore flash.
    m_renderView = new QWebEngineView(static_cast<QWidget*>(nullptr));
    m_renderView->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_renderView->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_renderView->setAttribute(Qt::WA_NoSystemBackground, true);
    m_renderView->page()->setBackgroundColor(QColor(QStringLiteral("#1E1E1E")));
    m_renderView->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    m_renderView->setVisible(false);
    connect(m_renderView->page(), &QWebEnginePage::loadFinished,
            this, &SmdCell::onRenderLoadFinished);
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

        m_editorStack->setFixedHeight(initH);

        // Position QWebEngineView at the cell's screen position.
        // It's a separate top-level window (no native cascade into SmdCell).
        // Show window at cell position so Chromium renders content.
        // Need a real visible window for proper paint — then immediately
        // lower behind main window. processEvents minimizes the visible time.
        QPoint cellPos = m_editorStack->mapToGlobal(QPoint(0, 0));
        m_renderView->setGeometry(cellPos.x(), cellPos.y(), viewW, initH);
        m_renderView->setFixedSize(viewW, initH);
        m_renderView->show();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        m_renderView->lower();

        // Non-native overlay (no WA_NativeWindow) as loading indicator
        if (!m_renderOverlay) {
            m_renderOverlay = new QWidget(this);
            m_renderOverlay->setStyleSheet(QStringLiteral("background-color: #1E1E1E;"));
        }
        m_renderOverlay->setGeometry(m_editorStack->geometry());
        m_renderOverlay->setVisible(true);
        m_renderOverlay->raise();
        m_editorStack->setCurrentIndex(0);

        // Load template
        QFile tmplFile(QStringLiteral(":/preview/template.html"));
        QString tmpl;
        if (tmplFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            tmpl = QString::fromUtf8(tmplFile.readAll());
            tmplFile.close();
        }
        if (tmpl.isEmpty()) {
            smdDebugLog(QStringLiteral("setRendered — template read FAILED"));
            tmpl = smdRenderTemplate().arg(
                QString::fromLatin1(content().toUtf8().toBase64()));
        } else {
            QString safeContent = content();
            safeContent.replace(QStringLiteral("</script>"), QStringLiteral("<\\/script>"));
            tmpl.replace(QStringLiteral("{{MARKDOWN_CONTENT}}"), safeContent);
            tmpl.replace(QStringLiteral("padding: 24px 32px;"),
                         QStringLiteral("padding: 8px 12px;"));
            tmpl.replace(QStringLiteral("max-width: 960px;"),
                         QStringLiteral(""));
        }

        smdDebugLog(QStringLiteral("setRendered(true) — top-level win at (%1,%2) size=%3x%4, htmlLen=%5")
            .arg(m_renderView->x()).arg(m_renderView->y())
            .arg(m_renderView->width()).arg(m_renderView->height()).arg(tmpl.size()));

        m_renderView->setHtml(tmpl, QUrl(QStringLiteral("qrc:/preview/")));
    } else {
        smdDebugLog(QStringLiteral("setRendered(false) — stop+cleanup+switch to editor"));
        if (m_grabTimer)
            m_grabTimer->stop();
        if (m_renderView)
            m_renderView->stop();
        if (m_renderImage)
            m_renderImage->clear();
        if (m_renderOverlay) {
            m_renderOverlay->hide();
            delete m_renderOverlay;
            m_renderOverlay = nullptr;
        }
        cleanupRenderView();
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
    // Keep overlay covering the full editor stack area as it grows
    if (m_renderOverlay && m_renderOverlay->isVisible()) {
        m_renderOverlay->setGeometry(m_editorStack->geometry());
    }
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
        // Force grab on load failure — show what we have after a short delay
        QTimer::singleShot(400, this, [this]() {
            if (m_rendered) performGrab();
        });
        return;
    }

    // --- Immediate height measurement ---
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

    // --- Adaptive polling: re-measure until height stable + Mermaid done ---
    startGrabPolling();
}

void SmdCell::startGrabPolling()
{
    if (!m_grabTimer) {
        m_grabTimer = new QTimer(this);
        m_grabTimer->setInterval(200);
        connect(m_grabTimer, &QTimer::timeout, this, &SmdCell::pollGrabReady);
    }
    m_pollCount = 0;
    m_polledHeights.clear();
    m_grabTimer->start();
    smdDebugLog(QStringLiteral("startGrabPolling — started 200ms timer"));
}

void SmdCell::pollGrabReady()
{
    if (!m_rendered || !m_renderView) {
        if (m_grabTimer) m_grabTimer->stop();
        return;
    }

    m_pollCount++;

    // JS: get scrollHeight + whether all Mermaid elements have been rendered
    m_renderView->page()->runJavaScript(
        QStringLiteral("(function(){"
        "  var h=document.body.scrollHeight||document.documentElement.scrollHeight||0;"
        "  var els=document.querySelectorAll('.mermaid');var done=true;"
        "  for(var i=0;i<els.length;i++){"
        "    if(!els[i].querySelector('svg')){done=false;break;}"
        "  }"
        "  return JSON.stringify({h:h,done:done,c:els.length});"
        "})()"),
        [this](const QVariant &v) {
            if (!m_rendered || !m_renderView) return;
            QString json = v.toString();
            // Parse simple JSON manually to avoid QStringView issues
            int h = 0;
            bool done = false;
            int mermaidCount = 0;
            // Extract h
            int hi = json.indexOf(QStringLiteral("\"h\":"));
            if (hi >= 0) {
                int hs = hi + 4;
                int he = json.indexOf(QLatin1Char(','), hs);
                if (he < 0) he = json.indexOf(QLatin1Char('}'), hs);
                if (he > hs) h = json.mid(hs, he - hs).toInt();
            }
            // Extract done
            int di = json.indexOf(QStringLiteral("\"done\":"));
            if (di >= 0) {
                int ds = di + 7;
                done = (json.mid(ds, 4) == QStringLiteral("true"));
            }
            // Extract count
            int ci = json.indexOf(QStringLiteral("\"c\":"));
            if (ci >= 0) {
                int cs = ci + 4;
                int ce = json.indexOf(QLatin1Char('}'), cs);
                if (ce > cs) mermaidCount = json.mid(cs, ce - cs).toInt();
            }

            smdDebugLog(QStringLiteral("pollGrabReady #%1 — h=%2, done=%3, mermaid=%4")
                .arg(m_pollCount).arg(h).arg(done).arg(mermaidCount));

            if (h > 0)
                applyRenderHeight(h);

            // Track height history for stability
            m_polledHeights.append(h);
            if (m_polledHeights.size() > 3)
                m_polledHeights.removeFirst();

            bool heightStable = (m_polledHeights.size() >= 3)
                && (m_polledHeights[0] == m_polledHeights[1])
                && (m_polledHeights[1] == m_polledHeights[2]);

            // Ready when height is stable AND all Mermaid rendered (or no Mermaid at all)
            bool ready = heightStable && (done || mermaidCount == 0);

            if (m_pollCount >= kMaxPollCount) {
                smdDebugLog(QStringLiteral("pollGrabReady — max polls reached, forcing grab"));
                if (m_grabTimer) m_grabTimer->stop();
                performGrab();
            } else if (ready) {
                smdDebugLog(QStringLiteral("pollGrabReady — ready! stable=%1, mermaid=%2 done=%3")
                    .arg(heightStable).arg(mermaidCount).arg(done));
                if (m_grabTimer) m_grabTimer->stop();
                performGrab();
            }
        });

    // Safety: force grab after max polls (checked in callback too for async safety)
    if (m_pollCount >= kMaxPollCount + 5) {
        smdDebugLog(QStringLiteral("pollGrabReady — force grab (overflow)"));
        if (m_grabTimer) m_grabTimer->stop();
        performGrab();
    }
}

void SmdCell::performGrab()
{
    if (!m_rendered || !m_renderView || !m_renderImage) {
        smdDebugLog(QStringLiteral("performGrab — aborted (state changed)"));
        return;
    }
    int vw = m_renderView->width();
    int vh = m_renderView->height();
    if (vh < 40) {
        smdDebugLog(QStringLiteral("performGrab — view too small (%1x%2)").arg(vw).arg(vh));
        return;
    }

    // Grab the top-level render window, then immediately hide it.
    smdDebugLog(QStringLiteral("performGrab — grabbing window %1x%2").arg(vw).arg(vh));
    m_grabbing = true; // suppress focusEntered during hide/cleanup
    QPixmap pm = m_renderView->grab();
    m_renderView->hide(); // hide immediately after grab — no linger
    if (pm.isNull() || pm.height() <= 0) {
        smdDebugLog(QStringLiteral("performGrab — grab FAILED"));
        return;
    }

    qreal dpr = m_renderView->devicePixelRatioF();
    int lw = qRound(pm.width() / dpr);
    int lh = qRound(pm.height() / dpr);
    smdDebugLog(QStringLiteral("performGrab — OK %1x%2 @dpr=%3").arg(lw).arg(lh).arg(dpr));

    // Set up QLabel with pixmap, switch stack, hide overlay
    m_renderImage->setPixmap(pm);
    m_renderImage->setScaledContents(true);
    m_renderImage->setFixedSize(lw, lh);
    m_editorStack->setFixedHeight(lh);
    m_editorStack->setCurrentIndex(1);
    if (m_renderOverlay) {
        m_renderOverlay->hide();
    }
    m_editorStack->repaint();

    // Close and delete the top-level QWebEngineView
    cleanupRenderView();
    if (m_renderOverlay) {
        delete m_renderOverlay;
        m_renderOverlay = nullptr;
    }

    updateGeometry();
    m_grabbing = false;
    emit contentChanged();
    smdDebugLog(QStringLiteral("performGrab — done"));
}

void SmdCell::cleanupRenderView()
{
    if (m_grabTimer) {
        m_grabTimer->stop();
    }
    if (m_renderView) {
        smdDebugLog(QStringLiteral("cleanupRenderView — closing top-level QWebEngineView"));
        m_renderView->stop();
        disconnect(m_renderView->page(), nullptr, this, nullptr);
        m_renderView->hide();
        m_renderView->close();
        delete m_renderView;
        m_renderView = nullptr;
        smdDebugLog(QStringLiteral("cleanupRenderView — done"));
    }
}

QWidget *SmdCell::editorWidget() const
{
    if (m_rendered) {
        // After grab, QLabel (page 1) holds the static pixmap
        if (m_renderImage && !m_renderImage->pixmap().isNull())
            return m_renderImage;
        // During rendering (before grab), return the markdown editor
        return m_markdownEditor;
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
        // After grab: QLabel determines height, no dynamic changes needed
        if (m_renderImage && !m_renderImage->pixmap().isNull()) {
            updateGeometry();
            return;
        }
        // Before grab: editor height used for the stack, QWebEngineView size updated elsewhere
        if (m_markdownEditor) {
            m_editorStack->setFixedHeight(m_markdownEditor->height());
        }
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
    // Log events on the render image for debugging
    if (m_renderImage && obj == m_renderImage) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease) {
            smdDebugLog(QStringLiteral("eventFilter — obj=renderImage, event=%1, m_rendered=%2")
                .arg(event->type() == QEvent::FocusIn ? QStringLiteral("FocusIn")
                     : event->type() == QEvent::MouseButtonPress ? QStringLiteral("MousePress")
                     : QStringLiteral("MouseRelease"))
                .arg(m_rendered));
        }
    }

    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
        if (!m_grabbing)
            emit focusEntered();
    }
    return QFrame::eventFilter(obj, event);
}

void SmdCell::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    // Keep overlay positioned over the editor stack (non-native, safe)
    if (m_renderOverlay && m_renderOverlay->isVisible()) {
        m_renderOverlay->setGeometry(m_editorStack->geometry());
    }
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
