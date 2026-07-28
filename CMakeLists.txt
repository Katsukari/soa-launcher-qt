cmake_minimum_required(VERSION 3.21.1)

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum supported macOS version")
endif()

project(soa_launcher
        LANGUAGES CXX
        DESCRIPTION "Story of Alicia Launcher"
        VERSION 1.0.0
)

if(DEFINED ENV{GITHUB_REPOSITORY} AND NOT "$ENV{GITHUB_REPOSITORY}" STREQUAL "")
    set(SOA_LAUNCHER_GITHUB_REPOSITORY_DEFAULT "$ENV{GITHUB_REPOSITORY}")
else()
    set(SOA_LAUNCHER_GITHUB_REPOSITORY_DEFAULT "Story-Of-Alicia/soa-launcher-qt")
endif()
set(SOA_LAUNCHER_GITHUB_REPOSITORY
    "${SOA_LAUNCHER_GITHUB_REPOSITORY_DEFAULT}"
    CACHE STRING "GitHub owner/repository used for launcher releases")
set(SOA_RULES_DOCUMENT_URL
    "https://docs.google.com/document/d/1vry3ZuDtzdS_mX1P2udWlb8z2Q9Atr3p1THZdtZ2EHA/export?format=html"
    CACHE STRING "HTTPS URL of the authoritative rules document")

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

include(CTest)
option(SOA_ALLOW_FETCHCONTENT "Allow downloading missing dependencies during configure" ON)

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

find_package(Qt6 REQUIRED COMPONENTS ${SOA_QT_COMPONENTS} LinguistTools)
list(TRANSFORM SOA_QT_COMPONENTS PREPEND Qt6:: OUTPUT_VARIABLE SOA_QT_LIBS)

find_package(spdlog 1.12 QUIET)
if(NOT spdlog_FOUND)
    if(NOT SOA_ALLOW_FETCHCONTENT)
        message(FATAL_ERROR "spdlog was not found and SOA_ALLOW_FETCHCONTENT is OFF")
    endif()
    include(FetchContent)
    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.16.0
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

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
        src/core/network/NetworkFoundation.swift
        src/core/network/SwiftHttpClient.swift
        src/core/network/LauncherUpdateService.swift
        src/core/network/DiscordRpcService.swift
        src/core/network/SwiftNetworkABI.swift
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
        src/util/LanguageManager.cpp
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
        src/util/LaunchArguments.cpp
        src/util/DesktopEntry.cpp
        src/util/LauncherLog.cpp
        src/widgets/AdvancedSettings.cpp
        src/core/Log.cpp
        src/core/discord/DiscordRpc.cpp
        src/core/wine/Shell.cpp
        src/core/wine/WineProcess.cpp
        src/core/wine/MacWineRuntime.cpp
        src/core/runtime/RuntimeManifest.cpp
        src/core/runtime/RuntimeManager.cpp
        src/core/runtime/RuntimeProvider.cpp
        src/core/system/SystemProfile.cpp
        src/core/integrity/GameIntegrityWatcher.cpp
        src/core/update/LauncherUpdateManager.cpp
        src/core/wine/PrefixInspector.cpp
        src/core/network/Courier.cpp
        src/core/network/SwiftHttpClient.cpp
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
        src/widgets/LauncherInfoDialog.cpp
        src/widgets/LauncherDialog.cpp
        src/widgets/LauncherUpdate.cpp
)

set(LAUNCHER_HEADERS
        include/MainWindow.hpp
        include/util/Assets.hpp
        include/util/LanguageManager.hpp
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
        include/util/LaunchArguments.hpp
        include/util/DesktopEntry.hpp
        include/widgets/LauncherLog.hpp
        include/widgets/AdvancedSettings.hpp
        include/core/wine/Shell.hpp
        include/core/wine/WineProcess.hpp
        include/core/wine/MacWineRuntime.hpp
        include/core/runtime/RuntimeManifest.hpp
        include/core/runtime/RuntimeManager.hpp
        include/core/runtime/RuntimeProvider.hpp
        include/core/system/SystemProfile.hpp
        include/core/integrity/GameIntegrityWatcher.hpp
        include/core/update/LauncherUpdateManager.hpp
        include/core/wine/PrefixInspector.hpp
        include/core/QtLogSink.hpp
        include/core/Log.hpp
        include/core/discord/DiscordRpc.hpp
        include/util/Styles.hpp
        include/core/network/Courier.h
        include/core/network/SwiftNetwork.h
        include/core/network/CourierBridge.hpp
        include/core/network/SwiftHttpClient.hpp
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
        include/widgets/LauncherInfoDialog.hpp
        include/widgets/LauncherDialog.hpp
        include/widgets/LauncherUpdate.hpp
)

set(SOA_PLATFORM_RESOURCES)

if(APPLE)
    set(SOA_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/soa-launcher.icns")
    set(SOA_MACOS_RUNTIME_PACKAGE "" CACHE PATH
        "Runtime package root containing runtime.json and payload/StoryOfAliciaRuntime.app")
    option(SOA_REQUIRE_BUNDLED_RUNTIME
        "Fail configuration when the launcher-owned macOS runtime package is unavailable"
        OFF)

    set_source_files_properties(${SOA_MACOS_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )

    list(APPEND SOA_PLATFORM_RESOURCES
            ${SOA_MACOS_ICON}
    )

    set(SOA_RESOLVED_MACOS_RUNTIME_PACKAGE "")
    if(SOA_MACOS_RUNTIME_PACKAGE)
        get_filename_component(SOA_RESOLVED_MACOS_RUNTIME_PACKAGE
            "${SOA_MACOS_RUNTIME_PACKAGE}" REALPATH)
    else()
        file(GLOB _SOA_RUNTIME_MANIFESTS
            LIST_DIRECTORIES FALSE
            "${CMAKE_CURRENT_SOURCE_DIR}/../soa_wine_runtime/runtime/out/packages/*/*/runtime.json")
        if(_SOA_RUNTIME_MANIFESTS)
            list(SORT _SOA_RUNTIME_MANIFESTS COMPARE NATURAL ORDER ASCENDING)
            list(GET _SOA_RUNTIME_MANIFESTS -1 _SOA_RUNTIME_MANIFEST)
            get_filename_component(SOA_RESOLVED_MACOS_RUNTIME_PACKAGE
                "${_SOA_RUNTIME_MANIFEST}" DIRECTORY)
        endif()
        unset(_SOA_RUNTIME_MANIFESTS)
        unset(_SOA_RUNTIME_MANIFEST)
    endif()

    if(SOA_RESOLVED_MACOS_RUNTIME_PACKAGE)
        set(_SOA_RUNTIME_MANIFEST
            "${SOA_RESOLVED_MACOS_RUNTIME_PACKAGE}/runtime.json")
        set(_SOA_RUNTIME_APP
            "${SOA_RESOLVED_MACOS_RUNTIME_PACKAGE}/payload/StoryOfAliciaRuntime.app")
        set(_SOA_RUNTIME_WINE
            "${_SOA_RUNTIME_APP}/Contents/Resources/wine/bin/wine")
        set(_SOA_RUNTIME_SELF_TEST
            "${_SOA_RUNTIME_APP}/Contents/Resources/tools/self-test-macos.sh")
        if(NOT EXISTS "${_SOA_RUNTIME_MANIFEST}"
            OR NOT IS_DIRECTORY "${_SOA_RUNTIME_APP}"
            OR NOT EXISTS "${_SOA_RUNTIME_WINE}"
            OR NOT EXISTS "${_SOA_RUNTIME_SELF_TEST}")
            message(FATAL_ERROR
                "SOA_MACOS_RUNTIME_PACKAGE is incomplete: ${SOA_RESOLVED_MACOS_RUNTIME_PACKAGE}")
        endif()
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/verify-runtime-package.py"
                    "${SOA_RESOLVED_MACOS_RUNTIME_PACKAGE}"
            RESULT_VARIABLE _SOA_RUNTIME_VERIFY_RESULT
            OUTPUT_VARIABLE _SOA_RUNTIME_VERIFY_OUTPUT
            ERROR_VARIABLE _SOA_RUNTIME_VERIFY_ERROR
        )
        if(NOT _SOA_RUNTIME_VERIFY_RESULT STREQUAL "0")
            message(FATAL_ERROR
                "SOA_MACOS_RUNTIME_PACKAGE failed immutable-payload verification:\n"
                "${_SOA_RUNTIME_VERIFY_OUTPUT}${_SOA_RUNTIME_VERIFY_ERROR}")
        endif()
        message(STATUS
            "Bundling Story of Alicia runtime package: ${SOA_RESOLVED_MACOS_RUNTIME_PACKAGE}")
    elseif(SOA_REQUIRE_BUNDLED_RUNTIME)
        message(FATAL_ERROR
            "A macOS distribution build requires the launcher-owned runtime package. "
            "Build soa_wine_runtime first or set -DSOA_MACOS_RUNTIME_PACKAGE=/path/to/package.")
    else()
        message(WARNING
            "Building a developer-only macOS launcher without its runtime. "
            "Distribution builds must set SOA_REQUIRE_BUNDLED_RUNTIME=ON.")
    endif()
endif()

qt_add_executable(${PROJECT_NAME}
        ${LAUNCHER_SOURCES}
        ${LAUNCHER_HEADERS}
        src/res/resources.qrc
        ${SOA_PLATFORM_RESOURCES}
)

set(SOA_TRANSLATION_FILES
        translations/soa_launcher_nb.ts
        translations/soa_launcher_nl.ts
)

if(Qt6_VERSION VERSION_GREATER_EQUAL "6.9")
    qt_add_translations(${PROJECT_NAME}
            TS_FILES ${SOA_TRANSLATION_FILES}
            RESOURCE_PREFIX "/i18n"
            QT_TRANSLATION_CATALOGS qtbase
    )
else()
    qt_add_translations(${PROJECT_NAME}
            TS_FILES ${SOA_TRANSLATION_FILES}
            RESOURCE_PREFIX "/i18n"
    )
endif()

if(APPLE AND SOA_RESOLVED_MACOS_RUNTIME_PACKAGE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/Story of Alicia Runtime"
        COMMAND /usr/bin/ditto
                "${SOA_RESOLVED_MACOS_RUNTIME_PACKAGE}"
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/Story of Alicia Runtime"
        VERBATIM
        COMMENT "Bundling the verified Story of Alicia runtime package")
endif()

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
        SOA_LAUNCHER_GITHUB_REPOSITORY="${SOA_LAUNCHER_GITHUB_REPOSITORY}"
        SOA_RULES_DOCUMENT_URL="${SOA_RULES_DOCUMENT_URL}"
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

if(BUILD_TESTING)
    enable_testing()

    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(launcher_tests
            tests/main.cpp
            src/util/LaunchArguments.cpp
            src/util/DesktopEntry.cpp
            src/core/wine/PrefixInspector.cpp
            src/core/wine/WineProcess.cpp
            src/core/wine/MacWineRuntime.cpp
            src/core/runtime/RuntimeManifest.cpp
            src/core/runtime/RuntimeManager.cpp
            src/core/runtime/RuntimeProvider.cpp
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

    add_executable(courier_path_tests
            tests/courier_paths_tests.swift
            src/core/network/CourierPaths.swift
    )

    set_target_properties(courier_path_tests PROPERTIES
            AUTOMOC OFF
            AUTORCC OFF
            AUTOUIC OFF
            Swift_MODULE_NAME SoaCourierPathTests
    )

    add_test(
            NAME courier_path_tests
            COMMAND courier_path_tests
    )


    add_executable(courier_cancel_tests
            tests/courier_cancel_tests.swift
            src/core/network/Courier.swift
    )

    target_compile_options(courier_cancel_tests PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_CURRENT_SOURCE_DIR}/src/core/network/module.modulemap>"
    )

    target_include_directories(courier_cancel_tests PRIVATE
            include
    )

    set_target_properties(courier_cancel_tests PROPERTIES
            AUTOMOC OFF
            AUTORCC OFF
            AUTOUIC OFF
            Swift_MODULE_NAME SoaCourierCancelTests
    )

    add_test(
            NAME courier_cancel_tests
            COMMAND courier_cancel_tests
    )


    add_executable(swift_network_abi_tests
            tests/swift_network_abi_tests.swift
    )

    target_compile_options(swift_network_abi_tests PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_CURRENT_SOURCE_DIR}/src/core/network/module.modulemap>"
    )

    target_include_directories(swift_network_abi_tests PRIVATE
            include
    )

    target_link_libraries(swift_network_abi_tests PRIVATE
            soa_network
    )

    set_target_properties(swift_network_abi_tests PROPERTIES
            AUTOMOC OFF
            AUTORCC OFF
            AUTOUIC OFF
            Swift_MODULE_NAME SoaSwiftNetworkABITests
    )

    add_test(
            NAME swift_network_abi_tests
            COMMAND swift_network_abi_tests
    )

    add_executable(courier_hash_tests
            tests/courier_hash_tests.swift
            src/core/network/MD5.swift
    )

    set_target_properties(courier_hash_tests PROPERTIES
            AUTOMOC OFF
            AUTORCC OFF
            AUTOUIC OFF
            Swift_MODULE_NAME SoaCourierHashTests
    )

    add_test(
            NAME courier_hash_tests
            COMMAND courier_hash_tests
    )

endif()
