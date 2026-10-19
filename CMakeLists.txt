cmake_minimum_required(VERSION 3.16)
project(markd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Default to local Qt 6.5.3 on macOS if not specified
if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH)
    if(EXISTS "/Users/rajsekharan/Qt/6.5.3/macos")
        set(CMAKE_PREFIX_PATH "/Users/rajsekharan/Qt/6.5.3/macos")
    endif()
endif()

option(ENABLE_COVERAGE "Enable Clang source-based code coverage" OFF)
if(ENABLE_COVERAGE)
    message(STATUS "Enabling Clang source-based code coverage")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebChannel Test Network)

include_directories(src)

set(COMMON_SOURCES
    src/app/MarkdApplication.h
    src/app/MarkdApplication.cpp
    src/app/CommandLineOptions.h
    src/app/CommandLineOptions.cpp
    src/core/Version.h
    src/core/ProvenanceDetector.h
    src/core/ProvenanceDetector.cpp
    src/core/UpdateChecker.h
    src/core/UpdateChecker.cpp
    src/core/DocumentSession.h
    src/core/DocumentSession.cpp
    src/core/Converter.h
    src/core/Converter.cpp
    src/core/HtmlClipboardProvider.h
    src/core/HtmlClipboardProvider.cpp
    src/core/TelemetryEngine.h
    src/core/TelemetryEngine.cpp
    src/parser/TocItem.h
    src/parser/TocParser.h
    src/parser/TocParser.cpp
    src/bridge/WebBridge.h
    src/bridge/WebBridge.cpp
    src/ui/EditorView.h
    src/ui/EditorView.cpp
    src/ui/TocHeaderWidget.h
    src/ui/TocHeaderWidget.cpp
    src/ui/TocItemDelegate.h
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/PreviewView.h
    src/ui/PreviewView.cpp
    src/ui/TocWidget.h
    src/ui/TocWidget.cpp
    src/ui/HeaderBar.h
    src/ui/HeaderBar.cpp
    src/ui/ModeSwitcher.h
    src/ui/ModeSwitcher.cpp
    src/ui/StatusBarTelemetry.h
    src/ui/StatusBarTelemetry.cpp
    src/ui/FindBarOverlay.h
    src/ui/FindBarOverlay.cpp
    src/ui/AboutDialog.h
    src/ui/AboutDialog.cpp
    src/ui/UpdateDialog.h
    src/ui/UpdateDialog.cpp
    src/ui/MenuBuilder.h
    src/ui/MenuBuilder.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/platform/PlatformFrame.h
    resources/theme.qrc
    web/web.qrc
)

foreach(PLATFORM_SRC
    src/ui/platform/PlatformFrame_mac.mm
    src/ui/platform/PlatformFrame_win.cpp
    src/ui/platform/PlatformFrame_generic.cpp
)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PLATFORM_SRC}")
        message(FATAL_ERROR "Physical platform implementation source missing: ${PLATFORM_SRC}")
    endif()
endforeach()

if(APPLE)
    list(APPEND COMMON_SOURCES src/ui/platform/PlatformFrame_mac.mm)
elseif(WIN32)
    list(APPEND COMMON_SOURCES src/ui/platform/PlatformFrame_win.cpp)
else()
    list(APPEND COMMON_SOURCES src/ui/platform/PlatformFrame_generic.cpp)
endif()

set(SOURCES
    src/main.cpp
    ${COMMON_SOURCES}
)

add_executable(markd ${SOURCES})

target_link_libraries(markd PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Network
)

if(APPLE)
    target_link_libraries(markd PRIVATE "-framework Cocoa")
    set_target_properties(markd PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

# Enable Testing
enable_testing()

add_executable(test_cli
    tests/test_cli.cpp
    ${COMMON_SOURCES}
)

target_link_libraries(test_cli PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Test
    Qt6::Network
)

if(APPLE)
    target_link_libraries(test_cli PRIVATE "-framework Cocoa")
endif()

add_test(NAME test_cli COMMAND test_cli)

set_tests_properties(test_cli PROPERTIES RUN_SERIAL TRUE)


