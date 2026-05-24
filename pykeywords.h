#ifndef PYKEYWORDS_H
#define PYKEYWORDS_H

#include <QStringList>

inline const QStringList &pyKeywords()
{
    static const QStringList keywords = {
        QStringLiteral("False"), QStringLiteral("None"), QStringLiteral("True"),
        QStringLiteral("and"), QStringLiteral("as"), QStringLiteral("assert"),
        QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"), QStringLiteral("elif"), QStringLiteral("else"),
        QStringLiteral("except"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("from"), QStringLiteral("global"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("is"),
        QStringLiteral("lambda"), QStringLiteral("match"), QStringLiteral("case"),
        QStringLiteral("nonlocal"), QStringLiteral("not"), QStringLiteral("or"),
        QStringLiteral("pass"), QStringLiteral("raise"), QStringLiteral("return"),
        QStringLiteral("try"), QStringLiteral("while"), QStringLiteral("with"),
        QStringLiteral("yield"),
    };
    return keywords;
}

inline const QStringList &pyBuiltins()
{
    static const QStringList builtins = {
        // Types
        QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("str"),
        QStringLiteral("list"), QStringLiteral("dict"), QStringLiteral("tuple"),
        QStringLiteral("set"), QStringLiteral("bool"), QStringLiteral("bytes"),
        QStringLiteral("bytearray"), QStringLiteral("complex"), QStringLiteral("frozenset"),
        QStringLiteral("range"), QStringLiteral("slice"), QStringLiteral("type"),
        QStringLiteral("super"), QStringLiteral("object"), QStringLiteral("property"),
        QStringLiteral("staticmethod"), QStringLiteral("classmethod"),
        // Functions
        QStringLiteral("enumerate"), QStringLiteral("zip"), QStringLiteral("map"),
        QStringLiteral("filter"), QStringLiteral("len"), QStringLiteral("print"),
        QStringLiteral("open"), QStringLiteral("isinstance"), QStringLiteral("hasattr"),
        QStringLiteral("getattr"), QStringLiteral("setattr"), QStringLiteral("sorted"),
        QStringLiteral("reversed"), QStringLiteral("iter"), QStringLiteral("next"),
        QStringLiteral("any"), QStringLiteral("all"), QStringLiteral("sum"),
        QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("abs"),
        QStringLiteral("round"), QStringLiteral("ord"), QStringLiteral("chr"),
        QStringLiteral("repr"), QStringLiteral("input"), QStringLiteral("format"),
        QStringLiteral("id"), QStringLiteral("dir"), QStringLiteral("vars"),
        QStringLiteral("callable"), QStringLiteral("issubclass"), QStringLiteral("eval"),
        QStringLiteral("exec"), QStringLiteral("compile"), QStringLiteral("locals"),
        QStringLiteral("globals"), QStringLiteral("hash"),
        // Exceptions
        QStringLiteral("ValueError"), QStringLiteral("TypeError"),
        QStringLiteral("KeyError"), QStringLiteral("IndexError"),
        QStringLiteral("AttributeError"), QStringLiteral("ImportError"),
        QStringLiteral("ModuleNotFoundError"), QStringLiteral("NameError"),
        QStringLiteral("FileNotFoundError"), QStringLiteral("ZeroDivisionError"),
        QStringLiteral("StopIteration"), QStringLiteral("RuntimeError"),
        QStringLiteral("OSError"), QStringLiteral("IOError"),
        QStringLiteral("Exception"), QStringLiteral("BaseException"),
        QStringLiteral("Warning"), QStringLiteral("UserWarning"),
        QStringLiteral("DeprecationWarning"),
    };
    return builtins;
}

#endif // PYKEYWORDS_H
