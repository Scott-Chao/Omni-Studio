#ifndef AICONTEXTMANAGER_H
#define AICONTEXTMANAGER_H

#include <QString>

class EditorWidget;

enum class AiEditorMode {
    Markdown,   // .md, .markdown files
    Code,       // code files (.cpp, .py, etc.) or SMD code cells
    Unknown
};

struct ContextBundle {
    AiEditorMode mode = AiEditorMode::Unknown;
    QString filePath;
    QString fileContent;
    QString selectedText;
    QString language;        // e.g., "cpp", "python", "markdown"
    int cursorLine = 0;
    int cursorColumn = 0;

    // Error analysis fields (for ErrorJournal / AiAction::ErrorAnalysis)
    QString errorStatusCode;     // "WA", "RE", "TLE", "MLE"
    QString errorDetail;         // Human-readable error description
    QString actualOutput;
    QString expectedOutput;
    QString inputData;
    qint64 elapsedMs = 0;
    quint64 memoryKb = 0;
};

class AiContextManager {
public:
    // Collect full context from the given editor widget
    static ContextBundle collectContext(EditorWidget *editor);

    // Quick mode detection (for action bar buttons)
    static AiEditorMode currentEditorMode(EditorWidget *editor);

    // Language string inferred from file path extension
    static QString languageForFile(const QString &filePath);
};

#endif // AICONTEXTMANAGER_H
