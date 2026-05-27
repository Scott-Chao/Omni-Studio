#ifndef CPPKEYWORDS_H
#define CPPKEYWORDS_H

#include <QStringList>

inline const QStringList &cppKeywords()
{
    static const QStringList keywords = {
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("and"),
        QStringLiteral("and_eq"), QStringLiteral("asm"), QStringLiteral("auto"),
        QStringLiteral("bitand"), QStringLiteral("bitor"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("compl"), QStringLiteral("concept"), QStringLiteral("const"),
        QStringLiteral("consteval"), QStringLiteral("constexpr"), QStringLiteral("constinit"),
        QStringLiteral("const_cast"), QStringLiteral("continue"), QStringLiteral("co_await"),
        QStringLiteral("co_return"), QStringLiteral("co_yield"), QStringLiteral("decltype"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("dynamic_cast"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
        QStringLiteral("false"), QStringLiteral("final"), QStringLiteral("for"),
        QStringLiteral("friend"), QStringLiteral("goto"), QStringLiteral("if"),
        QStringLiteral("inline"), QStringLiteral("mutable"), QStringLiteral("namespace"),
        QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("not"),
        QStringLiteral("not_eq"), QStringLiteral("nullptr"), QStringLiteral("operator"),
        QStringLiteral("or"), QStringLiteral("or_eq"), QStringLiteral("override"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("register"), QStringLiteral("reinterpret_cast"), QStringLiteral("requires"),
        QStringLiteral("return"), QStringLiteral("signed"), QStringLiteral("sizeof"),
        QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("static_cast"),
        QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("template"),
        QStringLiteral("this"), QStringLiteral("thread_local"), QStringLiteral("throw"),
        QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
        QStringLiteral("typeid"), QStringLiteral("typename"), QStringLiteral("union"),
        QStringLiteral("unsigned"), QStringLiteral("using"), QStringLiteral("virtual"),
        QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("while"),
        QStringLiteral("xor"), QStringLiteral("xor_eq")
    };
    return keywords;
}

// Control-flow / branching keywords — highlighted differently from other keywords
// (e.g. purple instead of blue in dark mode)
inline const QStringList &cppControlKeywords()
{
    static const QStringList keywords = {
        QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("for"),
        QStringLiteral("while"), QStringLiteral("do"), QStringLiteral("switch"),
        QStringLiteral("case"), QStringLiteral("default"), QStringLiteral("break"),
        QStringLiteral("continue"), QStringLiteral("return"), QStringLiteral("goto"),
        QStringLiteral("try"), QStringLiteral("catch"), QStringLiteral("throw"),
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
