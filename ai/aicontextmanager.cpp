#include "aicontextmanager.h"
#include "editorwidget.h"
#include "smd/smdeditor.h"
#include "smd/smdcell.h"
#include "languageutils.h"

#include <QFileInfo>

ContextBundle AiContextManager::collectContext(EditorWidget *editor)
{
    ContextBundle bundle;
    if (!editor)
        return bundle;

    bundle.filePath = editor->currentFilePath();
    bundle.selectedText = editor->selectedText();

    // Only read full content when there's no selection (avoids unnecessary allocation)
    if (bundle.selectedText.isEmpty())
        bundle.fileContent = editor->toPlainText();

    bundle.cursorLine = editor->cursorLine();
    bundle.cursorColumn = editor->cursorColumn();
    bundle.mode = currentEditorMode(editor);

    if (editor->isCodeEdit()) {
        QString lang = languageForFile(bundle.filePath);
        if (lang.isEmpty())
            lang = QStringLiteral("cpp");   // default for code mode
        bundle.language = lang;
    } else if (editor->isSmdEdit()) {
        // Detect language from active SMD cell
        SmdEditor *smd = editor->smdEditor();
        if (smd) {
            int idx = smd->activeCellIndex();
            if (idx >= 0) {
                SmdCell *cell = smd->cellAt(idx);
                if (cell->cellType() == SmdCell::Markdown) {
                    bundle.language = QStringLiteral("markdown");
                } else {
                    bundle.language = SmdCell::langIdFromType(cell->cellType());
                }
            }
        }
        if (bundle.language.isEmpty())
            bundle.language = QStringLiteral("markdown");
    } else if (editor->isPdfView()) {
        bundle.language = QStringLiteral("pdf");
    } else {
        bundle.language = QStringLiteral("markdown");
    }

    return bundle;
}

AiEditorMode AiContextManager::currentEditorMode(EditorWidget *editor)
{
    if (!editor)
        return AiEditorMode::Unknown;

    if (editor->isSmdEdit()) {
        SmdEditor *smd = editor->smdEditor();
        if (smd) {
            int idx = smd->activeCellIndex();
            if (idx >= 0) {
                SmdCell *cell = smd->cellAt(idx);
                return cell->cellType() == SmdCell::Markdown
                    ? AiEditorMode::Markdown
                    : AiEditorMode::Code;
            }
        }
        return AiEditorMode::Code; // fallback
    }

    if (editor->isCodeEdit())
        return AiEditorMode::Code;

    // MarkdownEdit, Preview, PdfView → Markdown mode
    return AiEditorMode::Markdown;
}

QString AiContextManager::languageForFile(const QString &filePath)
{
    if (filePath.isEmpty())
        return {};

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    static const QStringList markdownExts = {
        QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("txt"),
        QStringLiteral("rst"), QStringLiteral("tex")
    };
    if (markdownExts.contains(ext))
        return QStringLiteral("markdown");

    QString lang = LanguageUtils::languageForExtension(ext);
    if (!lang.isEmpty())
        return lang;

    static const QMap<QString, QString> extraLangs = {
        {QStringLiteral("html"),  QStringLiteral("html")},
        {QStringLiteral("htm"),   QStringLiteral("html")},
        {QStringLiteral("css"),   QStringLiteral("css")},
        {QStringLiteral("js"),    QStringLiteral("javascript")},
        {QStringLiteral("jsx"),   QStringLiteral("javascript")},
        {QStringLiteral("ts"),    QStringLiteral("typescript")},
        {QStringLiteral("tsx"),   QStringLiteral("typescript")},
        {QStringLiteral("json"),  QStringLiteral("json")},
        {QStringLiteral("xml"),   QStringLiteral("xml")},
        {QStringLiteral("yaml"),  QStringLiteral("yaml")},
        {QStringLiteral("yml"),   QStringLiteral("yaml")},
        {QStringLiteral("java"),  QStringLiteral("java")},
        {QStringLiteral("go"),    QStringLiteral("go")},
        {QStringLiteral("rs"),    QStringLiteral("rust")},
        {QStringLiteral("rb"),    QStringLiteral("ruby")},
        {QStringLiteral("php"),   QStringLiteral("php")},
        {QStringLiteral("sql"),   QStringLiteral("sql")},
        {QStringLiteral("sh"),    QStringLiteral("bash")},
        {QStringLiteral("bash"),  QStringLiteral("bash")},
        {QStringLiteral("ps1"),   QStringLiteral("powershell")},
    };
    auto it = extraLangs.constFind(ext);
    if (it != extraLangs.constEnd())
        return it.value();

    return {};
}
