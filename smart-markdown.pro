QT       += core gui webenginewidgets network pdf pdfwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Enable UTF-8 source file handling for MSVC
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

win32: LIBS += -luser32 -ldwmapi

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    thememanager.cpp \
    titlebarbutton.cpp \
    ai/actionbar.cpp \
    ai/aicontextmanager.cpp \
    ai/aipanel.cpp \
    ai/aiprovider.cpp \
    ai/aiproviderfactory.cpp \
    ai/anthropicprovider.cpp \
    ai/chatarea.cpp \
    ai/chatbubble.cpp \
    ai/errorjournal.cpp \
    ai/errorlistpanel.cpp \
    ai/aihistorylistwidget.cpp \
    ai/aihistorymanager.cpp \
    ai/airequesthandler.cpp \
    ai/openaiprovider.cpp \
    activitybar.cpp \
    configmanager.cpp \
    crashrecoverymanager.cpp \
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
    keyrecorder.cpp \
    logindialog.cpp \
    openjudgewidget.cpp \
    outlinepanel.cpp \
    bottompanel.cpp \
    outputpanel.cpp \
    processrunner.cpp \
    pythonsyntaxhighlighter.cpp \
    pythoncompletionprovider.cpp \
    rightpanelcontainer.cpp \
    scrollbarhider.cpp \
    searchpanel.cpp \
    settingspanel.cpp \
    helppanel.cpp \
    indexmanager.cpp \
    smdcell.cpp \
    smddiagnosticspanel.cpp \
    smdeditor.cpp \
    smdlspmanager.cpp \
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
    thememanager.h \
    titlebarbutton.h \
    ai/actionbar.h \
    ai/aicontextmanager.h \
    ai/aipanel.h \
    ai/aiprovider.h \
    ai/aiproviderfactory.h \
    ai/anthropicprovider.h \
    ai/chatarea.h \
    ai/chatbubble.h \
    ai/aiconversation.h \
    ai/errorjournal.h \
    ai/errorlistpanel.h \
    ai/aihistorylistwidget.h \
    ai/aihistorymanager.h \
    ai/airequesthandler.h \
    ai/openaiprovider.h \
    ai/prompttemplates.h \
    activitybar.h \
    configmanager.h \
    crashrecoverymanager.h \
    backlinkindex.h \
    backlinkspanel.h \
    codeeditor.h \
    completionpopup.h \
    completionprovider.h \
    compilererrorparser.h \
    compilerutils.h \
    cppcompletionprovider.h \
    cppkeywords.h \
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
    keyrecorder.h \
    logindialog.h \
    openjudgewidget.h \
    outlinepanel.h \
    outlineutils.h \
    bottompanel.h \
    smddiagnostic.h \
    outputpanel.h \
    processrunner.h \
    pythonsyntaxhighlighter.h \
    pykeywords.h \
    pythoncompletionprovider.h \
    rightpanelcontainer.h \
    scrollbarhider.h \
    searchpanel.h \
    settingspanel.h \
    helppanel.h \
    indexmanager.h \
    smdcell.h \
    smddiagnosticspanel.h \
    smdeditor.h \
    smdlspmanager.h \
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
