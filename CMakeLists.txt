cmake_minimum_required(VERSION 3.21.1)

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum supported macOS version")
endif()

project(soa_launcher
        LANGUAGES C CXX
        DESCRIPTION "Story of Alicia Launcher"
        VERSION 1.0.0
)

if(APPLE)
    set(SOA_LAUNCHER_UPDATE_MANIFEST_URL
        "https://r2.storyofalicia.com/launcher/macos/manifest.json"
        CACHE STRING "HTTPS URL of the launcher update manifest")
    set(SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL
        "https://raw.githubusercontent.com/Story-Of-Alicia/soa-launcher-qt/launcher-updates/macos/manifest.json"
        CACHE STRING "Fallback HTTPS URL of the launcher update manifest")
else()
    set(SOA_LAUNCHER_UPDATE_MANIFEST_URL
        "https://r2.storyofalicia.com/launcher/linux/manifest.json"
        CACHE STRING "HTTPS URL of the launcher update manifest")
    set(SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL
        "https://raw.githubusercontent.com/Story-Of-Alicia/soa-launcher-qt/launcher-updates/linux/manifest.json"
        CACHE STRING "Fallback HTTPS URL of the launcher update manifest")
endif()
file(READ
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/soa-update-public-key.hex"
    SOA_DEFAULT_UPDATE_PUBLIC_KEY_HEX)
string(STRIP
    "${SOA_DEFAULT_UPDATE_PUBLIC_KEY_HEX}"
    SOA_DEFAULT_UPDATE_PUBLIC_KEY_HEX)
set(SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX
    "${SOA_DEFAULT_UPDATE_PUBLIC_KEY_HEX}"
    CACHE STRING "Raw Ed25519 public key used by SOA Seal v1 as 64 hexadecimal characters")

string(LENGTH
        "${SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX}"
        SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX_LENGTH
)

if(SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX)
    if(NOT SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX_LENGTH EQUAL 64
            OR NOT SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX MATCHES "^[0-9A-Fa-f]+$")
        message(FATAL_ERROR
                "SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX must be exactly 64 hexadecimal characters")
    endif()
endif()
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
option(SOA_REQUIRE_ALICIA_LOG_HOOK
       "Fail configuration unless the Windows x86 Alicia diagnostic hook can be built" OFF)
option(SOA_PORTABLE_BUILD
       "Use conservative x86-64 compiler settings for distributable Linux builds" ON)

set(SOA_PORTABLE_CXX_OPTIONS)

if(SOA_PORTABLE_BUILD
        AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

    add_library(soa_seal_crypto STATIC
            src/core/network/SOASeal.c
    )

    target_include_directories(soa_seal_crypto PRIVATE
            ${SOA_INCLUDE_DIRS}
    )

    target_link_libraries(soa_seal_crypto PUBLIC
            OpenSSL::Crypto
    )

    set_target_properties(soa_seal_crypto PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            AUTOMOC OFF
            AUTORCC OFF
            AUTOUIC OFF
    )

    target_link_libraries(soa_network PRIVATE
            soa_seal_crypto
    )
endif()

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
        src/core/wine/ProcessRunner.cpp
        src/core/wine/RuntimeLocator.cpp
        src/core/wine/RuntimeEnvironment.cpp
        src/core/wine/PrefixSetupJob.cpp
        src/core/wine/GameSession.cpp
        src/core/wine/GamePhase.cpp
        src/core/wine/AliciaLogHook.cpp
        src/core/wine/MacLaunchDiagnostics.cpp
        src/core/wine/WineProcess.cpp
        src/core/wine/MacWineRuntime.cpp
        src/core/system/SystemProfile.cpp
        src/core/integrity/GameIntegrityWatcher.cpp
        src/core/update/LauncherUpdateManager.cpp
        src/core/update/LauncherReleaseCatalogue.cpp
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
        include/core/update/LauncherReleaseCatalogue.hpp
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
        include/core/wine/ProcessRunner.hpp
        include/core/wine/RuntimeLocator.hpp
        include/core/wine/PrefixSetupJob.hpp
        include/core/wine/GameSession.hpp
        include/core/wine/AliciaLogHook.hpp
        include/core/wine/MacLaunchDiagnostics.hpp
        include/core/wine/WineProcess.hpp
        include/core/wine/MacWineRuntime.hpp
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

set(SOA_ALICIA_LOG_HOOK_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/alicia-log-hook")
set(SOA_ALICIA_LOG_HOOK_OUTPUT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/alicia-log-hook")
set(SOA_ALICIA_LOG_HOOK_INJECTOR
    "${SOA_ALICIA_LOG_HOOK_OUTPUT_DIR}/SoaAliciaLogInjector.exe")
set(SOA_ALICIA_LOG_HOOK_DLL
    "${SOA_ALICIA_LOG_HOOK_OUTPUT_DIR}/SoaAliciaLogHook.dll")

find_program(SOA_MINGW_C_COMPILER NAMES i686-w64-mingw32-gcc)
find_program(SOA_MINGW_CXX_COMPILER NAMES i686-w64-mingw32-g++)

if(SOA_MINGW_C_COMPILER AND SOA_MINGW_CXX_COMPILER)
    file(GLOB_RECURSE SOA_ALICIA_LOG_HOOK_INPUTS CONFIGURE_DEPENDS
            "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/src/*"
            "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/minhook/include/*"
            "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/minhook/src/*"
    )
    find_program(SOA_ALICIA_LOG_HOOK_BASH bash)
    if(NOT SOA_ALICIA_LOG_HOOK_BASH)
        set(SOA_ALICIA_LOG_HOOK_BASH "/bin/bash")
    endif()

    add_custom_command(
            OUTPUT
            "${SOA_ALICIA_LOG_HOOK_INJECTOR}"
            "${SOA_ALICIA_LOG_HOOK_DLL}"
            COMMAND
            # Invoked through bash rather than executed directly: zip archives
            # do not always carry the executable bit, and a stripped +x here
            # fails the whole build with a bare "Permission denied" (exit 126).
            "${SOA_ALICIA_LOG_HOOK_BASH}"
            "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/build-x86.sh"
            "${SOA_ALICIA_LOG_HOOK_OUTPUT_DIR}"
            "${SOA_MINGW_C_COMPILER}"
            "${SOA_MINGW_CXX_COMPILER}"
            DEPENDS
            ${SOA_ALICIA_LOG_HOOK_INPUTS}
            "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/build-x86.sh"
            COMMENT "Building the Windows x86 Alicia injector and compatibility hook"
            VERBATIM
    )
    if(APPLE)
        add_executable(soa_audio_host
            "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/src/soa-audio-host.c")
        set_target_properties(soa_audio_host PROPERTIES
            OUTPUT_NAME "soa-audio-host"
            RUNTIME_OUTPUT_DIRECTORY "${SOA_ALICIA_LOG_HOOK_OUTPUT_DIR}")
        target_link_libraries(soa_audio_host PRIVATE
            "-framework AudioToolbox" "-framework CoreFoundation")
    endif()

    add_custom_target(soa_alicia_log_hook ALL
            DEPENDS
            "${SOA_ALICIA_LOG_HOOK_INJECTOR}"
            "${SOA_ALICIA_LOG_HOOK_DLL}"
    )
    add_dependencies(${PROJECT_NAME} soa_alicia_log_hook)
    set(SOA_ALICIA_LOG_HOOK_AVAILABLE ON)
else()
    set(SOA_ALICIA_LOG_HOOK_AVAILABLE OFF)
    if(SOA_REQUIRE_ALICIA_LOG_HOOK)
        message(FATAL_ERROR
                "The Alicia diagnostic hook requires i686-w64-mingw32-gcc and "
                "i686-w64-mingw32-g++")
    endif()
    message(WARNING
            "Windows x86 MinGW was not found; this developer build will not include "
            "the optional Alicia diagnostic log hook")
endif()

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
        SOA_LAUNCHER_UPDATE_MANIFEST_URL="${SOA_LAUNCHER_UPDATE_MANIFEST_URL}"
        SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL="${SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL}"
        SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX="${SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX}"
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
    if(SOA_ALICIA_LOG_HOOK_AVAILABLE)
        if(TARGET soa_audio_host)
            add_dependencies(${PROJECT_NAME} soa_audio_host)
        endif()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/alicia-log-hook"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOA_ALICIA_LOG_HOOK_INJECTOR}"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/alicia-log-hook/SoaAliciaLogInjector.exe"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOA_ALICIA_LOG_HOOK_DLL}"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/alicia-log-hook/SoaAliciaLogHook.dll"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:soa_audio_host>"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/alicia-log-hook/soa-audio-host"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/alicia-log-hook/README.md"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/alicia-log-hook/README.md"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/minhook/LICENSE.txt"
                "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/alicia-log-hook/MINHOOK_LICENSE.txt"
                VERBATIM
        )
    endif()
else()
    set_target_properties(soa_network PROPERTIES
            INSTALL_RPATH "$ORIGIN"
    )

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

    install(FILES packaging/linux/soa-launcher.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES packaging/linux/soa-launcher.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    )
    if(SOA_ALICIA_LOG_HOOK_AVAILABLE)
        install(PROGRAMS "${SOA_ALICIA_LOG_HOOK_INJECTOR}"
                DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/soa-launcher/alicia-log-hook
        )
        install(FILES "${SOA_ALICIA_LOG_HOOK_DLL}"
                DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/soa-launcher/alicia-log-hook
        )
        install(FILES
                third_party/alicia-log-hook/README.md
                "${SOA_ALICIA_LOG_HOOK_SOURCE_DIR}/minhook/LICENSE.txt"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/soa-launcher/alicia-log-hook
        )
    endif()
endif()

set(CPACK_PACKAGE_NAME "Story Of Alicia Launcher")
set(CPACK_PACKAGE_VENDOR "Story Of Alicia")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Story Of Alicia Launcher")
    set(CPACK_PACKAGE_FILE_NAME "Story_Of_Alicia-macos")
    include(CPack)
endif()

if(BUILD_TESTING)
    enable_testing()

    find_package(Qt6 REQUIRED COMPONENTS Test)

    set(SOA_LAUNCHER_TEST_SOURCES
            src/util/LaunchArguments.cpp
            src/util/DesktopEntry.cpp
            src/core/wine/PrefixInspector.cpp
            src/core/wine/WineProcess.cpp
            src/core/wine/MacWineRuntime.cpp
            src/core/wine/ProcessRunner.cpp
            src/core/wine/RuntimeEnvironment.cpp
            src/core/wine/GamePhase.cpp
    )

    set(SOA_LAUNCHER_TEST_TARGETS
            environment_tests
            launch_argument_tests
            prefix_path_tests
            process_detection_tests
            prefix_inspector_tests
            runtime_locator_tests
            game_profile_tests
    )

    foreach(soa_test IN LISTS SOA_LAUNCHER_TEST_TARGETS)
        qt_add_executable(${soa_test}
                tests/${soa_test}.cpp
                ${SOA_LAUNCHER_TEST_SOURCES}
        )

        target_include_directories(${soa_test} PRIVATE
                ${SOA_INCLUDE_DIRS}
        )

        if(SOA_PORTABLE_CXX_OPTIONS)
            target_compile_options(${soa_test} PRIVATE
                    ${SOA_PORTABLE_CXX_OPTIONS}
            )
        endif()

        target_link_libraries(${soa_test} PRIVATE
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
                Qt6::Test
                spdlog::spdlog
        )

        set_target_properties(${soa_test} PROPERTIES
                LINKER_LANGUAGE CXX
        )

        target_compile_definitions(${soa_test} PRIVATE
                SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
        )

        add_test(
                NAME ${soa_test}
                COMMAND ${soa_test}
        )
    endforeach()

    add_executable(courier_path_tests
            tests/courier_paths_tests.swift
            src/core/network/CourierPaths.swift
    )

    target_compile_options(courier_path_tests PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
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
            "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
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
            "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
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

    target_compile_options(courier_hash_tests PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
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

    qt_add_executable(launcher_release_catalogue_tests
            tests/launcher_release_catalogue_tests.cpp
            src/core/update/LauncherReleaseCatalogue.cpp
    )

    target_include_directories(launcher_release_catalogue_tests PRIVATE
            ${SOA_INCLUDE_DIRS}
    )

    target_link_libraries(launcher_release_catalogue_tests PRIVATE
            Qt6::Core
    )

    add_test(
            NAME launcher_release_catalogue_tests
            COMMAND launcher_release_catalogue_tests
    )

    add_executable(soa_seal_tests
            tests/soa_seal_tests.cpp
    )

    target_include_directories(soa_seal_tests PRIVATE
            ${SOA_INCLUDE_DIRS}
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(soa_seal_tests PRIVATE
                soa_seal_crypto
        )
    else()
        target_link_libraries(soa_seal_tests PRIVATE
                soa_network
        )
    endif()

    set_target_properties(soa_seal_tests PROPERTIES
            AUTOMOC OFF
            AUTORCC OFF
            AUTOUIC OFF
    )

    add_test(
            NAME soa_seal_tests
            COMMAND soa_seal_tests
    )

endif()
