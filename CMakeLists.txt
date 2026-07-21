cmake_minimum_required(VERSION 3.25)
project(soa_launcher
        LANGUAGES CXX
        DESCRIPTION "Story of Alicia Launcher"
        VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_language(Swift)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTS "Build tests" ON)

set(SOA_QT_COMPONENTS
        Core
        Gui
        Widgets
        Network
        Concurrent
)

set(SOA_INCLUDE_DIRS
        src
        include
)

find_package(Qt6 REQUIRED COMPONENTS ${SOA_QT_COMPONENTS})

list(TRANSFORM SOA_QT_COMPONENTS PREPEND Qt6:: OUTPUT_VARIABLE SOA_QT_LIBS)

include(FetchContent)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.16.0
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

add_library(soa_network STATIC
        src/core/network/Courier.swift
        src/core/network/CourierUpdate.swift
        src/core/network/CourierDownload.swift
        src/core/network/CourierIntegrity.swift
        src/core/network/CourierPaths.swift
        src/core/network/CourierTransfer.swift
        src/core/network/CourierTypes.swift
        src/core/network/CourierABI.swift
        src/core/network/CourierPing.swift
        src/core/network/MD5.swift
)

target_compile_options(soa_network PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_SOURCE_DIR}/src/core/network/module.modulemap>"
)

target_include_directories(soa_network PRIVATE include)

set(LAUNCHER_SOURCES
        src/main.cpp
        src/MainWindow.cpp
        src/util/Assets.cpp
        src/widgets/AliciaChooser.cpp
        src/widgets/PrerequisitesIntro.cpp
        src/widgets/RulesAgreement.cpp
        src/widgets/RepairFiles.cpp
        src/widgets/Settings.cpp
        src/util/Layout.cpp
        src/widgets/LauncherSettings.cpp
        src/widgets/WineSettings.cpp
        src/widgets/ImageDropdown.cpp
        src/widgets/WineInstall.cpp
        src/util/ModalOverlay.cpp
        src/util/SimpleUtils.cpp
        src/util/LauncherLog.cpp
        src/widgets/AdvancedSettings.cpp
        src/core/Log.cpp
        src/core/wine/Shell.cpp
        src/core/system/SystemProfile.cpp
        src/core/wine/PrefixInspector.cpp
        src/core/network/Courier.cpp
        src/core/auth/AuthHandler.cpp
        src/util/Config.cpp
        src/util/CredentialStore.cpp
        src/widgets/GameInstall.cpp
        src/widgets/DownloadProgress.cpp
        src/widgets/PrefixProgress.cpp
        src/core/wine/WineRegistry.cpp
        src/util/ProgressBar.cpp
        src/core/status/StatusReporter.cpp
        src/core/status/StatusBus.cpp
        src/core/state/InstallState.cpp
        src/core/state/ViewRouter.cpp
        src/widgets/WineSelectMenu.cpp
)

set(LAUNCHER_HEADERS
        include/MainWindow.hpp
        include/util/Assets.hpp
        include/widgets/AliciaChooser.hpp
        include/widgets/PrerequisitesIntro.hpp
        include/widgets/RulesAgreement.hpp
        include/widgets/RepairFiles.hpp
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
        include/core/system/SystemProfile.hpp
        include/core/wine/PrefixInspector.hpp
        include/core/QtLogSink.hpp
        include/core/Log.hpp
        include/util/Styles.hpp
        include/core/network/Courier.h
        include/core/network/CourierBridge.hpp
        include/core/auth/AuthHandler.hpp
        include/util/Config.hpp
        include/util/CredentialStore.hpp
        include/widgets/GameInstall.hpp
        include/widgets/DownloadProgress.hpp
        include/widgets/PrefixProgress.hpp
        include/core/wine/WineRegistry.hpp
        include/util/ProgressBar.hpp
        include/util/Colors.hpp
        include/core/status/Status.hpp
        include/core/status/StatusReporter.hpp
        include/core/status/StatusBus.hpp
        include/core/state/View.hpp
        include/core/state/ViewRouter.hpp
        include/core/state/Stage.hpp
        include/core/state/InstallState.hpp
        include/core/network/DownloadStatus.hpp
        include/core/game/GameVersion.hpp
        include/widgets/WineSelectMenu.hpp
)

set(LAUNCHER_RESOURCES
        src/res/resources.qrc
)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
        ${LAUNCHER_SOURCES}
        ${LAUNCHER_HEADERS}
        ${LAUNCHER_RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${SOA_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SOA_QT_LIBS}
        spdlog::spdlog
        soa_network
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
        SOA_LAUNCHER_VERSION="${PROJECT_VERSION}"
)

if(APPLE)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_FRAMEWORK})

    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Story Of Alicia Launcher"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.storyofalicia.launcher"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
    )
endif()

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT APPLE)
    install(FILES packaging/soa-launcher.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES packaging/soa-launcher.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()

set(CPACK_PACKAGE_NAME "Story Of Alicia Launcher")
set(CPACK_PACKAGE_VENDOR "Story Of Alicia")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Story Of Alicia Launcher")
    include(CPack)
endif()

if(BUILD_TESTS)
    enable_testing()

    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(launcher_tests
            tests/main.cpp
    )

    target_include_directories(launcher_tests PRIVATE ${SOA_INCLUDE_DIRS})

    target_link_libraries(launcher_tests PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
            spdlog::spdlog
    )

    set_target_properties(launcher_tests PROPERTIES
            LINKER_LANGUAGE CXX
    )

    target_compile_definitions(launcher_tests PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

    add_test(NAME launcher_tests COMMAND launcher_tests)
endif()