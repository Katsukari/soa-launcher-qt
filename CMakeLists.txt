cmake_minimum_required(VERSION 3.21.1)

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum supported macOS version")
endif()

project(soa_launcher
        LANGUAGES CXX
        DESCRIPTION "Story of Alicia Launcher"
        VERSION 0.1.0
)

if(NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "soa_launcher currently supports Linux and macOS only")
endif()

if(NOT CMAKE_GENERATOR MATCHES "^Ninja" AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "The Swift Courier library requires the Ninja or Xcode generator")
endif()

enable_language(Swift)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTS "Build tests" ON)

set(SOA_PORTABLE_CXX_OPTIONS)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
        AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
        AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND SOA_PORTABLE_CXX_OPTIONS
            -march=x86-64
            -mtune=generic
    )
endif()

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
        GIT_TAG v1.16.0
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(spdlog)

if(SOA_PORTABLE_CXX_OPTIONS AND TARGET spdlog)
    target_compile_options(spdlog PRIVATE
            ${SOA_PORTABLE_CXX_OPTIONS}
    )
endif()

add_library(soa_network SHARED
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
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_CURRENT_SOURCE_DIR}/src/core/network/module.modulemap>"
)

target_include_directories(soa_network PRIVATE
        include
)

set_target_properties(soa_network PROPERTIES
        AUTOMOC OFF
        AUTORCC OFF
        AUTOUIC OFF
        Swift_MODULE_NAME SoaCourier
)

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
        src/core/integrity/GameIntegrityWatcher.cpp
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
        include/core/integrity/GameIntegrityWatcher.hpp
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

set(SOA_PLATFORM_RESOURCES)

if(APPLE)
    set(SOA_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/soa-launcher.icns")

    set_source_files_properties(${SOA_MACOS_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )

    list(APPEND SOA_PLATFORM_RESOURCES
            ${SOA_MACOS_ICON}
    )
endif()

qt_add_executable(${PROJECT_NAME}
        ${LAUNCHER_SOURCES}
        ${LAUNCHER_HEADERS}
        src/res/resources.qrc
        ${SOA_PLATFORM_RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${SOA_INCLUDE_DIRS}
)

if(SOA_PORTABLE_CXX_OPTIONS)
    target_compile_options(${PROJECT_NAME} PRIVATE
            ${SOA_PORTABLE_CXX_OPTIONS}
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SOA_QT_LIBS}
        spdlog::spdlog
        soa_network
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        LINKER_LANGUAGE CXX
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
        SOA_LAUNCHER_VERSION="${PROJECT_VERSION}"
)

include(GNUInstallDirs)

if(APPLE)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
            ${SECURITY_FRAMEWORK}
    )

    set_target_properties(soa_network PROPERTIES
            MACOSX_RPATH TRUE
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_NAME_DIR TRUE
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Story Of Alicia Launcher"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.storyofalicia.launcher"
            MACOSX_BUNDLE_ICON_FILE "soa-launcher.icns"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:soa_network>"
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/$<TARGET_FILE_NAME:soa_network>"
            VERBATIM
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
endif()

install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT APPLE)
    install(TARGETS soa_network
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(FILES packaging/soa-launcher.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES packaging/soa-launcher.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    )
endif()

set(CPACK_PACKAGE_NAME "Story Of Alicia Launcher")
set(CPACK_PACKAGE_VENDOR "Story Of Alicia")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Story Of Alicia Launcher")
    set(CPACK_PACKAGE_FILE_NAME "Story_Of_Alicia-${CMAKE_SYSTEM_PROCESSOR}")
    include(CPack)
endif()

if(BUILD_TESTS)
    enable_testing()

    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(launcher_tests
            tests/main.cpp
    )

    target_include_directories(launcher_tests PRIVATE
            ${SOA_INCLUDE_DIRS}
    )

    if(SOA_PORTABLE_CXX_OPTIONS)
        target_compile_options(launcher_tests PRIVATE
                ${SOA_PORTABLE_CXX_OPTIONS}
        )
    endif()

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

    target_compile_definitions(launcher_tests PRIVATE
            SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    )

    add_test(
            NAME launcher_tests
            COMMAND launcher_tests
    )
endif()