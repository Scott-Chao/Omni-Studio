#ifndef SMDDIAGNOSTIC_H
#define SMDDIAGNOSTIC_H

#include <QString>

struct SmdDiagnostic {
    int cellIndex = -1;
    int startLine = 0;
    int startCol = 0;
    int endLine = 0;
    int endCol = 0;
    QString message;
    int severity = 0; // 1=Error, 2=Warning, 3=Info, 4=Hint
};

#endif // SMDDIAGNOSTIC_H
