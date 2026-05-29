#ifndef CPPKEYWORDS_H
#define CPPKEYWORDS_H

#include <QStringList>

inline const QStringList &cppKeywords()
{
    static const QStringList keywords = {
        // Storage / modifier keywords — blue
        QStringLiteral("alignas"), QStringLiteral("asm"), QStringLiteral("auto"),
        QStringLiteral("class"), QStringLiteral("const"),
        QStringLiteral("consteval"), QStringLiteral("constexpr"), QStringLiteral("constinit"),
        QStringLiteral("const_cast"), QStringLiteral("dynamic_cast"), QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
        QStringLiteral("false"), QStringLiteral("final"),
        QStringLiteral("friend"),
        QStringLiteral("inline"), QStringLiteral("mutable"),
        QStringLiteral("nullptr"), QStringLiteral("override"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("register"), QStringLiteral("reinterpret_cast"), QStringLiteral("signed"),
        QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("static_cast"),
        QStringLiteral("struct"), QStringLiteral("this"), QStringLiteral("thread_local"),
        QStringLiteral("true"), QStringLiteral("typedef"),
        QStringLiteral("union"),
        QStringLiteral("unsigned"), QStringLiteral("virtual"),
        QStringLiteral("void"), QStringLiteral("volatile"),
        // Type operators — blue (except using which is purple)
        QStringLiteral("sizeof"), QStringLiteral("alignof"), QStringLiteral("typeid"),
        QStringLiteral("noexcept"), QStringLiteral("decltype"),
        // Template / type keywords — blue (except using which is purple)
        QStringLiteral("template"), QStringLiteral("typename"), QStringLiteral("namespace"),
        // Alternative operator tokens — blue
        QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"),
        QStringLiteral("xor"), QStringLiteral("bitand"), QStringLiteral("bitor"),
        QStringLiteral("compl"),
        QStringLiteral("and_eq"), QStringLiteral("or_eq"),
        QStringLiteral("not_eq"), QStringLiteral("xor_eq"),
    };
    return keywords;
}

// Keywords that get preprocessor-purple in dark mode — same colour as #include.
// Applied after regular keywords so they override blue with purple.
inline const QStringList &cppControlKeywords()
{
    static const QStringList keywords = {
        // Control flow
        QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("for"),
        QStringLiteral("while"), QStringLiteral("do"), QStringLiteral("switch"),
        QStringLiteral("case"), QStringLiteral("default"), QStringLiteral("break"),
        QStringLiteral("continue"), QStringLiteral("return"), QStringLiteral("goto"),
        QStringLiteral("try"), QStringLiteral("catch"), QStringLiteral("throw"),
        // Coroutine control flow
        QStringLiteral("co_await"), QStringLiteral("co_return"), QStringLiteral("co_yield"),
        // Memory / object operators
        QStringLiteral("new"), QStringLiteral("delete"),
        // Type alias / using-directive (only using is purple — template/typename/namespace are blue)
        QStringLiteral("using"),
        // Operator overloading
        QStringLiteral("operator"),
        // C++20 concepts
        QStringLiteral("concept"), QStringLiteral("requires"),
    };
    return keywords;
}

// Built-in / primitive types — highlighted in keyword color (blue), no bold
inline const QStringList &cppPrimitiveTypes()
{
    static const QStringList types = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("char16_t"),
        QStringLiteral("char32_t"), QStringLiteral("char8_t"), QStringLiteral("double"),
        QStringLiteral("float"), QStringLiteral("int"), QStringLiteral("long"),
        QStringLiteral("short"), QStringLiteral("wchar_t"),
        QStringLiteral("size_t"), QStringLiteral("ssize_t"), QStringLiteral("ptrdiff_t"),
        QStringLiteral("int8_t"), QStringLiteral("int16_t"), QStringLiteral("int32_t"),
        QStringLiteral("int64_t"), QStringLiteral("uint8_t"), QStringLiteral("uint16_t"),
        QStringLiteral("uint32_t"), QStringLiteral("uint64_t"),
    };
    return types;
}

inline const QStringList &cppCommonTypes()
{
    static const QStringList types = {
        // Namespace
        QStringLiteral("std"),
        // Strings
        QStringLiteral("string"), QStringLiteral("wstring"), QStringLiteral("u16string"),
        QStringLiteral("u32string"), QStringLiteral("string_view"),
        // Containers
        QStringLiteral("vector"), QStringLiteral("map"), QStringLiteral("set"),
        QStringLiteral("list"), QStringLiteral("deque"), QStringLiteral("queue"),
        QStringLiteral("stack"), QStringLiteral("array"), QStringLiteral("tuple"),
        QStringLiteral("pair"), QStringLiteral("initializer_list"),
        // Optional / variant
        QStringLiteral("optional"), QStringLiteral("variant"),
        // Smart pointers
        QStringLiteral("unique_ptr"), QStringLiteral("shared_ptr"), QStringLiteral("weak_ptr"),
        // Functional
        QStringLiteral("function"),
        // Concurrency
        QStringLiteral("mutex"), QStringLiteral("lock_guard"), QStringLiteral("unique_lock"),
        QStringLiteral("shared_lock"), QStringLiteral("condition_variable"),
        QStringLiteral("promise"), QStringLiteral("future"), QStringLiteral("atomic"),
        // Thread
        QStringLiteral("thread"), QStringLiteral("jthread"),
        // Filesystem
        QStringLiteral("filesystem"), QStringLiteral("path"),
        QStringLiteral("error_code"), QStringLiteral("error_category"),
        // I/O
        QStringLiteral("istream"), QStringLiteral("ostream"), QStringLiteral("iostream"),
        QStringLiteral("fstream"), QStringLiteral("sstream"), QStringLiteral("stringstream"),
        QStringLiteral("ifstream"), QStringLiteral("ofstream"),
        // Qt core
        QStringLiteral("QString"), QStringLiteral("QStringList"), QStringLiteral("QVariant"),
        // Qt containers
        QStringLiteral("QList"), QStringLiteral("QVector"), QStringLiteral("QMap"),
        QStringLiteral("QSet"), QStringLiteral("QHash"), QStringLiteral("QPair"),
        // Qt smart pointers
        QStringLiteral("QSharedPointer"), QStringLiteral("QScopedPointer"),
        // Qt IO
        QStringLiteral("QDebug"), QStringLiteral("QFile"), QStringLiteral("QDir"),
        // Qt utilities
        QStringLiteral("QTimer"), QStringLiteral("QProcess"), QStringLiteral("QThread"),
        // Qt widgets
        QStringLiteral("QWidget"), QStringLiteral("QObject"),
    };
    return types;
}

#endif // CPPKEYWORDS_H
