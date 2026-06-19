cmake_minimum_required(VERSION 4.3)
project(soa_launcher
        LANGUAGES CXX
        DESCRIPTION "Story of Alicia Launcher"
        VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Swift is only used for the networking library (core/network). Enabling it as
# its own language lets that library build with Swift while the rest stays C++.
enable_language(Swift)

# Qt auto-generators for MOC and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Options
option(BUILD_TESTS "Build tests" ON)

# Dependencies

# Qt6
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Network
        Concurrent
)

# spdlog — fetched automatically, no submodule needed
include(FetchContent)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.16.0
        GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(spdlog)

# ---------------------------------------------------------------------------
# Swift networking library (isolated)
#
# Swift lives in its own static library so the C++ executable's compile/link
# flags (-fPIC, -static-libstdc++, etc.) never leak into swiftc, and so Swift
# doesn't synthesise its own main(). The executable just links this.
# ---------------------------------------------------------------------------
add_library(soa_network STATIC
        src/core/network/soa_bridge.swift
)

# Swift needs the module map to import the C bridge header (soa_log, etc.).
target_compile_options(soa_network PRIVATE
        -Xcc -fmodule-map-file=${CMAKE_SOURCE_DIR}/src/core/network/module.modulemap
)

# The bridge header lives under include/.
target_include_directories(soa_network PRIVATE
        include
)

# Sources (C++ only - the Swift file is in soa_network above)

set(LAUNCHER_SOURCES
        src/main.cpp
        src/MainWindow.cpp
        src/util/Assets.cpp
        src/widgets/DownloadBox.cpp
        src/widgets/Settings.cpp
        src/util/Layout.cpp
        src/widgets/LauncherSettings.cpp
        src/widgets/WineSettings.cpp
        src/widgets/ImageDropdown.cpp
        src/widgets/WineInstall.cpp
        src/util/ModalOverlay.cpp
        src/util/SimpleUtils.cpp
        src/widgets/LauncherLog.cpp
        src/widgets/AdvancedSettings.cpp
        src/core/Log.cpp
        src/core/wine/Shell.cpp
        src/core/network/soa_bridge.cpp
)

set(LAUNCHER_HEADERS
        include/MainWindow.hpp
        include/util/Assets.hpp
        include/widgets/DownloadBox.hpp
        include/widgets/Settings.hpp
        include/util/Layout.hpp
        include/widgets/LauncherSettings.hpp
        include/widgets/WineSettings.hpp
        include/widgets/ImageDropdown.hpp
        include/widgets/WineInstall.hpp
        include/util/ModalOverlay.hpp
        include/util/SimpleUtils.hpp
        include/widgets/LauncherLog.hpp
        include/widgets/AdvancedSettings.hpp
        include/core/wine/Shell.hpp
        include/core/QtLogSink.hpp
        include/core/Log.hpp
        include/util/Styles.hpp
        include/core/network/soa_bridge.h
)

# Resources (.qrc files)
set(LAUNCHER_RESOURCES
        src/res/resources.qrc
)

# Executable

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
        ${LAUNCHER_SOURCES}
        ${LAUNCHER_HEADERS}
        ${LAUNCHER_RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Concurrent
        spdlog::spdlog
        soa_network            # the Swift networking library
)

target_compile_definitions(${PROJECT_NAME} PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Platform-specific configuration

# macOS: build as a proper .app bundle.
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Story Of Alicia Launcher"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.storyofalicia.launcher"
    )
endif()

# Statically link the C++ runtime so the binary doesn't depend on the user's
# installed libstdc++/libgcc version. Now that the executable is a pure C++
# link target (Swift is in soa_network), g++ does the linking and these flags
# are valid again.
# On macOS with AppleClang, libc++ is part of the OS - no static linking needed.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${PROJECT_NAME} PRIVATE
            -static-libstdc++
            -static-libgcc
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT APPLE)
    target_link_options(${PROJECT_NAME} PRIVATE -static-libstdc++)
endif()

# Install + Qt deploy

install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
)

# Packaging (generates .dmg on macOS, .tar.gz on Linux)

set(CPACK_PACKAGE_NAME "Story Of Alicia Launcher")
set(CPACK_PACKAGE_VENDOR "Story Of Alicia")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Story Of Alicia Launcher")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# Tests

if(BUILD_TESTS)
    enable_testing()

    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(launcher_tests
            tests/main.cpp
    )

    target_include_directories(launcher_tests PRIVATE
            src
            include
    )

    target_link_libraries(launcher_tests PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
            spdlog::spdlog
    )

    add_test(NAME launcher_tests COMMAND launcher_tests)
endif()