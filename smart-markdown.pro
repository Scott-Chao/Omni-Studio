QT       += core gui webenginewidgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Enable UTF-8 source file handling for MSVC
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    backlinkindex.cpp \
    backlinkspanel.cpp \
    codeeditor.cpp \
    compilerutils.h \
    cppsyntaxhighlighter.cpp \
    crawler.cpp \
    editorwidget.cpp \
    flowlayout.cpp \
    languageutils.cpp \
    wikilinktextedit.cpp \
    fileexplorerwidget.cpp \
    historypanel.cpp \
    judgeengine.cpp \
    judgepanel.cpp \
    logindialog.cpp \
    openjudgewindow.cpp \
    outputpanel.cpp \
    processrunner.cpp \
    pythonsyntaxhighlighter.cpp \
    searchpanel.cpp \
    main.cpp \
    mainwindow.cpp \
    settingsmanager.cpp \
    tabmanager.cpp

HEADERS += \
    backlinkindex.h \
    backlinkspanel.h \
    codeeditor.h \
    compilerutils.h \
    cppsyntaxhighlighter.h \
    crawler.h \
    editorwidget.h \
    flowlayout.h \
    languageutils.h \
    wikilinktextedit.h \
    fileexplorerwidget.h \
    fileutils.h \
    historypanel.h \
    judgeengine.h \
    judgepanel.h \
    logindialog.h \
    openjudgewindow.h \
    outputpanel.h \
    processrunner.h \
    pythonsyntaxhighlighter.h \
    searchpanel.h \
    mainwindow.h \
    settingsmanager.h \
    tabmanager.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources/resources.qrc

TRANSLATIONS += \
    smart-markdown_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

CONFIG(release, debug|release) {
    DESTDIR = $$PWD/release
    OBJECTS_DIR = $$PWD/build/release/obj
    MOC_DIR = $$PWD/build/release/moc
    RCC_DIR = $$PWD/build/release/rcc
    UI_DIR = $$PWD/build/release/ui
}
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/debug
    OBJECTS_DIR = $$PWD/build/debug/obj
    MOC_DIR = $$PWD/build/debug/moc
    RCC_DIR = $$PWD/build/debug/rcc
    UI_DIR = $$PWD/build/debug/ui
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
