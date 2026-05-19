QT       += core gui webenginewidgets network pdf pdfwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Enable UTF-8 source file handling for MSVC
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

win32: LIBS += -luser32

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ai/actionbar.cpp \
    ai/aicontextmanager.cpp \
    ai/aipanel.cpp \
    ai/aiproviderfactory.cpp \
    ai/anthropicprovider.cpp \
    ai/chatarea.cpp \
    ai/chatbubble.cpp \
    ai/errorjournal.cpp \
    ai/openaiprovider.cpp \
    activitybar.cpp \
    configmanager.cpp \
    backlinkindex.cpp \
    backlinkspanel.cpp \
    codeeditor.cpp \
    completionpopup.cpp \
    compilerutils.h \
    cppcompletionprovider.cpp \
    cppsyntaxhighlighter.cpp \
    crawler.cpp \
    editorwidget.cpp \
    flowlayout.cpp \
    hovermanager.cpp \
    keywordcompletionprovider.cpp \
    languageutils.cpp \
    lspclient.cpp \
    wikilinktextedit.cpp \
    fileexplorerwidget.cpp \
    historypanel.cpp \
    judgeengine.cpp \
    judgepanel.cpp \
    logindialog.cpp \
    openjudgewindow.cpp \
    outlinepanel.cpp \
    outputpanel.cpp \
    processrunner.cpp \
    pythonsyntaxhighlighter.cpp \
    pythoncompletionprovider.cpp \
    rightpanelcontainer.cpp \
    searchpanel.cpp \
    settingspanel.cpp \
    smdcell.cpp \
    smdeditor.cpp \
    smdoutputwidget.cpp \
    submissionpanel.cpp \
    tagindex.cpp \
    tagpanel.cpp \
    main.cpp \
    mainwindow.cpp \
    settingsmanager.cpp \
    signaturehelpmanager.cpp \
    tabmanager.cpp

HEADERS += \
    ai/actionbar.h \
    ai/aicontextmanager.h \
    ai/aipanel.h \
    ai/aiprovider.h \
    ai/aiproviderfactory.h \
    ai/anthropicprovider.h \
    ai/chatarea.h \
    ai/chatbubble.h \
    ai/errorjournal.h \
    ai/openaiprovider.h \
    ai/prompttemplates.h \
    activitybar.h \
    configmanager.h \
    backlinkindex.h \
    backlinkspanel.h \
    codeeditor.h \
    completionpopup.h \
    completionprovider.h \
    compilerutils.h \
    cppcompletionprovider.h \
    cppsyntaxhighlighter.h \
    crawler.h \
    editorwidget.h \
    flowlayout.h \
    hovermanager.h \
    keywordcompletionprovider.h \
    languageutils.h \
    lspclient.h \
    wikilinktextedit.h \
    fileexplorerwidget.h \
    fileutils.h \
    historypanel.h \
    judgeengine.h \
    judgepanel.h \
    logindialog.h \
    openjudgewindow.h \
    outlinepanel.h \
    outlineutils.h \
    outputpanel.h \
    processrunner.h \
    pythonsyntaxhighlighter.h \
    pythoncompletionprovider.h \
    rightpanelcontainer.h \
    searchpanel.h \
    settingspanel.h \
    smdcell.h \
    smdeditor.h \
    smdformat.h \
    smdoutputwidget.h \
    submissionpanel.h \
    tagindex.h \
    tagpanel.h \
    mainwindow.h \
    settingsmanager.h \
    signaturehelpmanager.h \
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
