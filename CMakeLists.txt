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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include_directories(src)

set(COMMON_SOURCES
    src/CommandLineOptions.h
    src/CommandLineOptions.cpp
    src/MenuBuilder.h
    src/MenuBuilder.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/Theme.h
    src/Theme.cpp
    src/PlatformFrame.h
    src/HeaderBar.h
    src/HeaderBar.cpp
    resources/theme.qrc
)

if(APPLE)
    list(APPEND COMMON_SOURCES src/PlatformFrame_mac.mm)
elseif(WIN32)
    list(APPEND COMMON_SOURCES src/PlatformFrame_win.cpp)
else()
    list(APPEND COMMON_SOURCES src/PlatformFrame_generic.cpp)
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
)

if(APPLE)
    target_link_libraries(test_cli PRIVATE "-framework Cocoa")
endif()

add_test(NAME test_cli COMMAND test_cli)
