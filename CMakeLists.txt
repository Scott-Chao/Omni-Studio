cmake_minimum_required(VERSION 3.22)
project(SmartMarkdown LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    Network
    Pdf
    PdfWidgets
)

set(SOURCES
    core/thememanager.cpp
    widgets/titlebarbutton.cpp
    ai/actionbar.cpp
    ai/aicontextmanager.cpp
    ai/aipanel.cpp
    ai/aiproviders.cpp
    ai/chatarea.cpp
    ai/chatbubble.cpp
    ai/errorjournal.cpp
    ai/errorlistpanel.cpp
    ai/aihistorylistwidget.cpp
    ai/aihistorymanager.cpp
    ai/airequesthandler.cpp
    config/configmanager.cpp
    core/crashrecoverymanager.cpp
    index/backlinkindex.cpp
    editor/brackethighlighter.cpp
    editor/codeeditor.cpp
    runner/codeblockrunner.cpp
    runner/compilerunmanager.cpp
    editor/cppsyntaxhighlighter.cpp
    judge/crawler.cpp
    editor/diagnosticsection.cpp
    editor/editorwidget.cpp
    widgets/flowlayout.cpp
    core/languageutils.cpp
    editor/completionpopup.cpp
    lsp/cppcompletionprovider.cpp
    editor/hovermanager.cpp
    editor/keywordcompletionprovider.cpp
    lsp/lspclient.cpp
    lsp/pythoncompletionprovider.cpp
    editor/signaturehelpmanager.cpp
    lsp/smdlspmanager.cpp
    editor/wikilinktextedit.cpp
    judge/judgeengine.cpp
    config/keyrecorder.cpp
    judge/logindialog.cpp
    judge/openjudgemanager.cpp
    panels/activitybar.cpp
    panels/bottompanel.cpp
    panels/fileexplorerwidget.cpp
    panels/helppanel.cpp
    panels/historypanel.cpp
    panels/judgepanel.cpp
    panels/openjudgewidget.cpp
    panels/outlinepanel.cpp
    panels/outputpanel.cpp
    panels/rightpanelcontainer.cpp
    panels/searchpanel.cpp
    panels/settingspanel.cpp
    panels/sidebarpanels.cpp
    panels/submissionpanel.cpp
    panels/smddiagnosticspanel.cpp
    panels/smdoutputwidget.cpp
    widgets/tabbuttongroup.cpp
    panels/windowdraghelper.cpp
    runner/processrunner.cpp
    editor/pythonsyntaxhighlighter.cpp
    widgets/scrollbarhider.cpp
    config/settingschangehandler.cpp
    index/indexmanager.cpp
    smd/smdcell.cpp
    smd/smdeditor.cpp
    index/tagindex.cpp
    main.cpp
    core/mainwindow.cpp
    config/settingsmanager.cpp
    editor/tabmanager.cpp
)

set(HEADERS
    core/thememanager.h
    widgets/titlebarbutton.h
    ai/actionbar.h
    ai/aicontextmanager.h
    ai/aipanel.h
    ai/aiproviders.h
    ai/chatarea.h
    ai/chatbubble.h
    ai/aiconversation.h
    ai/errorjournal.h
    ai/errorlistpanel.h
    ai/aihistorylistwidget.h
    ai/aihistorymanager.h
    ai/airequesthandler.h
    ai/prompttemplates.h
    config/configmanager.h
    core/crashrecoverymanager.h
    core/utilities.h
    lsp/keywords.h
    index/backlinkindex.h
    editor/codeeditor.h
    runner/codeblockrunner.h
    runner/compilererrorparser.h
    runner/compilerunmanager.h
    runner/compilerutils.h
    editor/brackethighlighter.h
    editor/cppsyntaxhighlighter.h
    judge/crawler.h
    editor/diagnosticsection.h
    editor/editorwidget.h
    widgets/flowlayout.h
    core/languageutils.h
    editor/completionpopup.h
    lsp/completionprovider.h
    lsp/cppcompletionprovider.h
    editor/hovermanager.h
    editor/keywordcompletionprovider.h
    lsp/lspclient.h
    lsp/pythoncompletionprovider.h
    editor/signaturehelpmanager.h
    lsp/smdlspmanager.h
    editor/wikilinktextedit.h
    judge/judgeengine.h
    config/keyrecorder.h
    judge/logindialog.h
    judge/openjudgemanager.h
    panels/activitybar.h
    panels/bottompanel.h
    panels/fileexplorerwidget.h
    panels/helppanel.h
    panels/historypanel.h
    panels/judgepanel.h
    panels/openjudgewidget.h
    panels/outlinepanel.h
    panels/outputpanel.h
    panels/rightpanelcontainer.h
    panels/searchpanel.h
    panels/settingspanel.h
    panels/sidebarpanels.h
    panels/submissionpanel.h
    panels/smddiagnosticspanel.h
    panels/smdoutputwidget.h
    widgets/tabbuttongroup.h
    panels/windowdraghelper.h
    smd/smddiagnostic.h
    runner/processrunner.h
    editor/pythonsyntaxhighlighter.h
    widgets/scrollbarhider.h
    config/settingschangehandler.h
    index/indexmanager.h
    smd/smdcell.h
    smd/smdeditor.h
    smd/smdformat.h
    index/tagindex.h
    core/mainwindow.h
    config/settingsmanager.h
    editor/tabmanager.h
)

qt_add_executable(SmartMarkdown
    ${SOURCES}
    ${HEADERS}
    core/mainwindow.ui
    resources/resources.qrc
)

target_include_directories(SmartMarkdown PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SmartMarkdown PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Network
    Qt6::Pdf
    Qt6::PdfWidgets
)

if(WIN32)
    target_compile_options(SmartMarkdown PRIVATE /utf-8)
    target_link_libraries(SmartMarkdown PRIVATE user32 dwmapi)
    find_program(WINDEPLOYQT windeployqt
        HINTS "${CMAKE_PREFIX_PATH}/bin"
    )
    if(WINDEPLOYQT)
        add_custom_command(TARGET SmartMarkdown POST_BUILD
            COMMAND "${WINDEPLOYQT}" "$<TARGET_FILE:SmartMarkdown>" --qmldir .
            COMMENT "Deploying Qt runtime DLLs..."
        )
    endif()
endif()
