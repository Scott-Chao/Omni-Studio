#ifndef COMPILERUTILS_H
#define COMPILERUTILS_H

#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QProcessEnvironment>

struct CompilerInfo {
    QString id;
    QString displayName;
    QString compilerPath;
    bool available = false;
};

namespace CompilerUtils {

inline QList<CompilerInfo> findCompilers()
{
    QList<CompilerInfo> compilers;

    // Detect g++ (MinGW / standalone)
    {
        CompilerInfo info;
        info.id = QStringLiteral("gcc");
        info.displayName = QStringLiteral("g++ (MinGW)");
        info.compilerPath = QStandardPaths::findExecutable(QStringLiteral("g++"));
        info.available = !info.compilerPath.isEmpty();
        compilers.append(info);
    }

    // Detect MSVC cl.exe (only if VS dev prompt is active)
    {
        CompilerInfo info;
        info.id = QStringLiteral("msvc");
        info.displayName = QStringLiteral("MSVC (cl.exe)");
        info.compilerPath = QStandardPaths::findExecutable(QStringLiteral("cl.exe"));
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        info.available = !info.compilerPath.isEmpty()
                         && !env.value(QStringLiteral("VSCMD_VER")).isEmpty();
        compilers.append(info);
    }

    return compilers;
}

inline CompilerInfo findPython()
{
    CompilerInfo info;
    info.id = QStringLiteral("python");
    info.displayName = QStringLiteral("Python");
    info.compilerPath = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (info.compilerPath.isEmpty())
        info.compilerPath = QStandardPaths::findExecutable(QStringLiteral("python3"));
    info.available = !info.compilerPath.isEmpty();
    return info;
}

inline CompilerInfo defaultCompiler()
{
    QList<CompilerInfo> compilers = findCompilers();
    for (const auto &c : compilers) {
        if (c.available)
            return c;
    }
    return CompilerInfo();
}

inline QStringList getCompileArgs(const QString &compilerId,
                                   const QString &sourceFile,
                                   const QString &outputFile)
{
    QStringList args;
    if (compilerId == QStringLiteral("gcc")) {
        args << QStringLiteral("-std=c++17")
             << QStringLiteral("-Wall")
             << QStringLiteral("-Wextra")
             << sourceFile
             << QStringLiteral("-o") << outputFile;
    } else if (compilerId == QStringLiteral("msvc")) {
        args << QStringLiteral("/std:c++17")
             << QStringLiteral("/W4")
             << QStringLiteral("/EHsc")
             << sourceFile
             << QStringLiteral("/Fe") + outputFile;
    }
    return args;
}

inline QString getOutputPath(const QString &sourceFile)
{
    QFileInfo fi(sourceFile);
    return fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName() + QStringLiteral(".exe");
}

} // namespace CompilerUtils

#endif // COMPILERUTILS_H
