#include <QtTest>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <utility>

#include "core/game/GameVersion.hpp"
#include "core/runtime/RuntimeManager.hpp"
#include "core/runtime/RuntimeManifest.hpp"
#include "core/wine/GameSession.hpp"
#include "core/wine/MacWineRuntime.hpp"
#include "core/wine/PrefixInspector.hpp"
#include "core/wine/ProcessRunner.hpp"
#include "core/wine/RuntimeLocator.hpp"
#include "core/wine/WineProcess.hpp"
#include "util/DesktopEntry.hpp"
#include "util/LaunchArguments.hpp"

namespace
{
    QJsonObject valid_runtime_manifest(const QString& runtimeId,
                                       const QString& buildId,
                                       const QString& wineEntrypoint)
    {
        QJsonObject entrypoints;
        entrypoints.insert(QStringLiteral("wine"), wineEntrypoint);
        entrypoints.insert(QStringLiteral("wineserver"), QString());
        entrypoints.insert(QStringLiteral("wineboot"), QString());
        entrypoints.insert(QStringLiteral("self_test"), QString());

        QJsonObject manifest;
        manifest.insert(QStringLiteral("schema_version"), 1);
        manifest.insert(QStringLiteral("launcher_contract"), 1);
        manifest.insert(QStringLiteral("prefix_schema"), 1);
        manifest.insert(QStringLiteral("runtime_id"), runtimeId);
        manifest.insert(QStringLiteral("display_name"),
                        QStringLiteral("Story of Alicia Wine 11 Stable"));
        manifest.insert(QStringLiteral("runtime_version"), QStringLiteral("11.0-soa.1"));
        manifest.insert(QStringLiteral("build_id"), buildId);
        manifest.insert(QStringLiteral("channel"), QStringLiteral("stable"));
#if defined(Q_OS_MACOS)
        manifest.insert(QStringLiteral("platform"), QStringLiteral("macos"));
#else
        manifest.insert(QStringLiteral("platform"), QStringLiteral("linux"));
#endif
        manifest.insert(QStringLiteral("host_arch"), QStringLiteral("x86_64"));
        manifest.insert(QStringLiteral("wine_version"), QStringLiteral("11.0"));
        manifest.insert(QStringLiteral("wine_commit"), QStringLiteral("wine-11.0"));
        manifest.insert(QStringLiteral("requires_rosetta_on_arm64"), true);
        manifest.insert(QStringLiteral("graphics_backends"),
                        QJsonArray {QStringLiteral("wined3d-opengl")});
        manifest.insert(QStringLiteral("entrypoints"), entrypoints);
        return manifest;
    }

    bool write_json(const QString& path, const QJsonObject& object)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
    }
}

class LauncherTests final : public QObject
{
    Q_OBJECT

private slots:
    void accepts_only_safe_runtime_environment_entries()
    {
        QProcessEnvironment environment;
        environment.insert(QStringLiteral("WINEPREFIX"), QStringLiteral("/safe/prefix"));
        environment.insert(QStringLiteral("PATH"), QStringLiteral("/safe/path"));

        core::wine::RuntimeLocator::apply_wine_environment_entries(
            environment, QStringLiteral("WINEPREFIX=/escape PATH=/unsafe "
                                        "SOA_RENDER_HINT=fast "
                                        "SOA_LABEL=\"hello world\" "
                                        "invalid-key=value"));

        QCOMPARE(environment.value(QStringLiteral("WINEPREFIX")),
                 QStringLiteral("/safe/prefix"));
        QCOMPARE(environment.value(QStringLiteral("PATH")), QStringLiteral("/safe/path"));
        QCOMPARE(environment.value(QStringLiteral("SOA_RENDER_HINT")), QStringLiteral("fast"));
        QCOMPARE(environment.value(QStringLiteral("SOA_LABEL")), QStringLiteral("hello world"));
        QVERIFY(!environment.contains(QStringLiteral("invalid-key")));
    }

    void redacts_process_arguments_and_output()
    {
        const QString secret = QStringLiteral("private-token");
        const QStringList arguments = core::wine::redacted_command_args(
            {QStringLiteral("-ID"), QStringLiteral("[user]"), QStringLiteral("-OP"),
             QStringLiteral("[private-token]")},
            {secret});
        QCOMPARE(arguments,
                 QStringList({QStringLiteral("-ID"), QStringLiteral("[user]"),
                              QStringLiteral("-OP"), QStringLiteral("[REDACTED]")}));
        QCOMPARE(
            core::wine::redact_sensitive_text(
                QStringLiteral("launch -OP [private-token] private-token"), {secret}),
            QStringLiteral("launch -OP [REDACTED] [REDACTED]"));
    }

    void process_runner_completes_once()
    {
        const QString shell = QStandardPaths::findExecutable(QStringLiteral("sh"));
        QVERIFY(!shell.isEmpty());

        core::wine::ProcessRunner runner;
        core::wine::ProcessRunner::Request request;
        request.program = shell;
        request.arguments = {QStringLiteral("-c"), QStringLiteral("printf runner-ok")};
        request.timeout_ms = 5000;

        QEventLoop loop;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        int completions = 0;
        core::wine::command_result result;
        connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        QVERIFY(runner.start(std::move(request),
                             [&](const core::wine::command_result& completed)
                             {
                                 ++completions;
                                 result = completed;
                                 loop.quit();
                             }));
        watchdog.start(8000);
        loop.exec();

        QCOMPARE(completions, 1);
        QVERIFY(result.ok());
        QVERIFY(result.started);
        QCOMPARE(result.exit_code, 0);
        QCOMPARE(result.output, QStringLiteral("runner-ok"));
        QVERIFY(!runner.is_busy());
        QTest::qWait(50);
        QCOMPARE(completions, 1);
    }

    void game_phase_transition_graph_is_explicit()
    {
        using core::wine::GamePhase;
        using core::wine::is_valid_game_transition;

        QVERIFY(is_valid_game_transition(GamePhase::Idle, GamePhase::Preflight));
        QVERIFY(is_valid_game_transition(GamePhase::Preflight, GamePhase::CleaningPrefix));
        QVERIFY(is_valid_game_transition(GamePhase::CleaningPrefix,
                                         GamePhase::FirstLaunchSetup));
        QVERIFY(is_valid_game_transition(GamePhase::FirstLaunchSetup, GamePhase::Launching));
        QVERIFY(is_valid_game_transition(GamePhase::Launching, GamePhase::Running));
        QVERIFY(is_valid_game_transition(GamePhase::Running, GamePhase::Finished));
        QVERIFY(is_valid_game_transition(GamePhase::Finished, GamePhase::Idle));
        QVERIFY(is_valid_game_transition(GamePhase::Running,
                                         GamePhase::MonitoringUncertain));

        QVERIFY(!is_valid_game_transition(GamePhase::Idle, GamePhase::Launching));
        QVERIFY(!is_valid_game_transition(GamePhase::Running, GamePhase::Launching));
        QVERIFY(!is_valid_game_transition(GamePhase::MonitoringUncertain, GamePhase::Idle));
    }

    void accepts_safe_game_arguments()
    {
        const auto result = util::launch_arguments::validate(
            QStringLiteral("-windowed -language \"English UK\""));
        QVERIFY(result.valid);
        QCOMPARE(result.arguments,
                 QStringList({QStringLiteral("-windowed"),
                              QStringLiteral("-language"),
                              QStringLiteral("English UK")}));
    }

    void rejects_reserved_game_arguments()
    {
        for (const QString& value : {
                 QStringLiteral("-OP stolen"),
                 QStringLiteral("-id=other"),
                 QStringLiteral("-GameID 99")})
        {
            const auto result = util::launch_arguments::validate(value);
            QVERIFY(!result.valid);
            QVERIFY(!result.error.isEmpty());
        }
    }

    void rejects_oversized_game_arguments()
    {
        const auto result = util::launch_arguments::validate(QString(4097, QLatin1Char('a')));
        QVERIFY(!result.valid);
    }

    void escapes_desktop_field_codes()
    {
        QCOMPARE(
            util::desktop_entry::quoted_exec_argument(QStringLiteral("/tmp/100%/launcher\"app")),
            QStringLiteral("\"/tmp/100%%/launcher\\\"app\""));
    }

    void escapes_desktop_control_characters()
    {
        QCOMPARE(
            util::desktop_entry::quoted_exec_argument(QStringLiteral("/tmp/a\nb\tapp")),
            QStringLiteral("\"/tmp/a\\nb\\tapp\""));
    }

    void maps_prefix_file_to_windows_c_path()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString gameDirectory = QDir(prefix).filePath(
            QStringLiteral("drive_c/users/test/Story Of Alicia"));
        QVERIFY(QDir().mkpath(gameDirectory));

        const QString executable = QDir(gameDirectory).filePath(QStringLiteral("Alicia.exe"));
        QFile file(executable);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("MZ") > 0);
        file.close();

        QString error;
        QCOMPARE(core::wine::windows_path_for_prefix_file(prefix, executable, &error),
                 QStringLiteral("C:\\users\\test\\Story Of Alicia\\Alicia.exe"));
        QVERIFY(error.isEmpty());
    }

    void rejects_executable_outside_wine_c_drive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        QVERIFY(QDir().mkpath(QDir(prefix).filePath(QStringLiteral("drive_c"))));

        const QString executable = directory.filePath(QStringLiteral("Alicia.exe"));
        QFile file(executable);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("MZ") > 0);
        file.close();

        QString error;
        QVERIFY(core::wine::windows_path_for_prefix_file(prefix, executable, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("outside"), Qt::CaseInsensitive));
    }

    void rejects_symlink_escape_from_wine_c_drive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString gameDirectory = QDir(prefix).filePath(QStringLiteral("drive_c/game"));
        QVERIFY(QDir().mkpath(gameDirectory));

        const QString outside = directory.filePath(QStringLiteral("outside-Alicia.exe"));
        QFile file(outside);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("MZ") > 0);
        file.close();

        const QString linked = QDir(gameDirectory).filePath(QStringLiteral("Alicia.exe"));
        QVERIFY(QFile::link(outside, linked));

        QString error;
        QVERIFY(core::wine::windows_path_for_prefix_file(prefix, linked, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("outside"), Qt::CaseInsensitive));
    }

    void parses_alicia_from_tasklist_csv()
    {
        const QString output = QStringLiteral(
            "\"services.exe\",\"52\",\"Services\",\"0\",\"8,000 K\"\n"
            "\"Alicia.exe\",\"184\",\"Console\",\"1\",\"220,000 K\"\n");
        const auto process = core::wine::find_windows_process(
            output, QStringLiteral("Alicia.exe"));
        QVERIFY(process.has_value());
        QCOMPARE(process->pid, qint64(184));
        QCOMPARE(process->image_name, QStringLiteral("Alicia.exe"));
    }

    void decodes_utf16le_tasklist_output()
    {
        const QString source = QStringLiteral(
            "\"services.exe\",\"52\"\r\n\"Alicia.exe\",\"4242\"\r\n");
        QByteArray utf16;
        utf16.append(char(0xff));
        utf16.append(char(0xfe));
        for (const QChar character : source)
        {
            const ushort unit = character.unicode();
            utf16.append(char(unit & 0xff));
            utf16.append(char((unit >> 8) & 0xff));
        }

        const QString decoded =
            core::wine::decode_windows_process_output(utf16);
        const auto process = core::wine::find_windows_process(
            decoded, QStringLiteral("Alicia.exe"));
        QVERIFY(process.has_value());
        QCOMPARE(process->pid, qint64(4242));
    }

    void decodes_utf16le_tasklist_after_host_warning()
    {
        const QString source = QStringLiteral(
            "\"services.exe\",\"52\"\r\n\"Alicia.exe\",\"4243\"\r\n");
        QByteArray mixed("runtime warning before tasklist\n");
        for (const QChar character : source)
        {
            const ushort unit = character.unicode();
            mixed.append(char(unit & 0xff));
            mixed.append(char((unit >> 8) & 0xff));
        }

        const QString decoded =
            core::wine::decode_windows_process_output(mixed);
        const auto process = core::wine::find_windows_process(
            decoded, QStringLiteral("Alicia.exe"));
        QVERIFY(process.has_value());
        QCOMPARE(process->pid, qint64(4243));
    }

    void parses_actual_host_alicia_process()
    {
        const QString output = QStringLiteral(
            "  912 /opt/wine/bin/wineserver\n"
            " 1001 /opt/wine/bin/wine explorer /desktop=StoryOfAlicia,1280x720 "
            "C:\\Story Of Alicia\\Alicia.exe -GameID 1\n"
            " 1017 /opt/wine/bin/winedbg --auto 1042 Alicia.exe\n"
            " 1042 /opt/wine/bin/wine64-preloader C:\\Story Of Alicia\\Alicia.exe -GameID 1\n");
        const auto process = core::wine::find_host_process(
            output, QStringLiteral("Alicia.exe"));
        QVERIFY(process.has_value());
        QCOMPARE(process->pid, qint64(1042));
        QVERIFY(process->source_line.contains(QStringLiteral("Alicia.exe")));
    }

    void prefix_marker_tracks_runtime_changes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString runtime = directory.filePath(QStringLiteral("wine"));
        QVERIFY(QDir().mkpath(prefix));
        QFile executable(runtime);
        QVERIFY(executable.open(QIODevice::WriteOnly));
        QCOMPARE(executable.write("runtime-a"), qint64(9));
        executable.close();

        QVERIFY(core::wine::PrefixInspector::write_marker(prefix, runtime));
        QVERIFY(core::wine::PrefixInspector::marker_valid(prefix, runtime));

        QVERIFY(executable.open(QIODevice::Append));
        QCOMPARE(executable.write("-changed"), qint64(8));
        executable.close();
        QVERIFY(!core::wine::PrefixInspector::marker_valid(prefix, runtime));
    }

    void resolves_active_managed_runtime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString runtimeId = QStringLiteral("soa-wine-macos-x86_64");
        const QString buildId = QStringLiteral("11.0-soa.1");
        const QString installation = QDir(directory.path()).filePath(
            QStringLiteral("installed/%1/%2").arg(runtimeId, buildId));
        const QString bin = QDir(installation).filePath(QStringLiteral("payload/bin"));
        QVERIFY(QDir().mkpath(bin));

        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\necho wine-11.0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QVERIFY(write_json(QDir(installation).filePath(QStringLiteral("runtime.json")),
                           valid_runtime_manifest(runtimeId, buildId,
                                                  QStringLiteral("payload/bin/wine"))));

        QJsonObject active;
        active.insert(QStringLiteral("schema_version"), 1);
        active.insert(QStringLiteral("runtime_id"), runtimeId);
        active.insert(QStringLiteral("build_id"), buildId);
        QVERIFY(write_json(QDir(directory.path()).filePath(QStringLiteral("active.json")),
                           active));

        const core::runtime::RuntimeManager manager(directory.path());
        const core::runtime::RuntimeInstallation resolved = manager.active();
        QVERIFY2(resolved.usable, qPrintable(resolved.failure));
        QCOMPARE(resolved.manifest.identity(), runtimeId + QLatin1Char('/') + buildId);
        QCOMPARE(resolved.wine_executable, QFileInfo(winePath).canonicalFilePath());
        QCOMPARE(manager.resolve_active_entrypoint(QStringLiteral("wine")),
                 QFileInfo(winePath).canonicalFilePath());
    }

    void validates_standalone_runtime_package()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString relativeWine = QStringLiteral(
            "payload/StoryOfAliciaRuntime.app/Contents/Resources/wine/bin/wine");
        const QString winePath = QDir(directory.path()).filePath(relativeWine);
        QVERIFY(QDir().mkpath(QFileInfo(winePath).absolutePath()));

        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\necho wine-11.0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QVERIFY(write_json(
            QDir(directory.path()).filePath(QStringLiteral("runtime.json")),
            valid_runtime_manifest(QStringLiteral("soa-runtime"),
                                   QStringLiteral("build-1"), relativeWine)));

        const auto package =
            core::runtime::RuntimeManager::inspect_package(directory.path());
        QVERIFY2(package.usable, qPrintable(package.failure));
        QCOMPARE(package.wine_executable,
                 QFileInfo(winePath).canonicalFilePath());
        QCOMPARE(core::wine::macos::resolve_wine_executable(directory.path()),
                 winePath);
    }

    void rejects_runtime_package_for_wrong_platform()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString relativeWine = QStringLiteral("payload/bin/wine");
        const QString winePath = QDir(directory.path()).filePath(relativeWine);
        QVERIFY(QDir().mkpath(QFileInfo(winePath).absolutePath()));

        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\nexit 0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QJsonObject manifest = valid_runtime_manifest(
            QStringLiteral("soa-runtime"), QStringLiteral("build-1"),
            relativeWine);
#if defined(Q_OS_MACOS)
        manifest.insert(QStringLiteral("platform"), QStringLiteral("linux"));
#else
        manifest.insert(QStringLiteral("platform"), QStringLiteral("macos"));
#endif
        QVERIFY(write_json(
            QDir(directory.path()).filePath(QStringLiteral("runtime.json")),
            manifest));

        const auto package =
            core::runtime::RuntimeManager::inspect_package(directory.path());
        QVERIFY(!package.usable);
        QVERIFY(package.failure.contains(
            QStringLiteral("not a"), Qt::CaseInsensitive));
    }

    void rejects_runtime_manifest_entrypoint_traversal()
    {
        core::runtime::RuntimeManifest manifest;
        QString error;
        QVERIFY(!core::runtime::RuntimeManifest::parse(
            valid_runtime_manifest(QStringLiteral("soa-wine"),
                                   QStringLiteral("build-1"),
                                   QStringLiteral("../outside/wine")),
            manifest, &error));
        QVERIFY(error.contains(QStringLiteral("safe relative path"), Qt::CaseInsensitive));
    }

    void rejects_unsafe_active_runtime_identity()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QJsonObject active;
        active.insert(QStringLiteral("schema_version"), 1);
        active.insert(QStringLiteral("runtime_id"), QStringLiteral("../escape"));
        active.insert(QStringLiteral("build_id"), QStringLiteral("build-1"));
        QVERIFY(write_json(QDir(directory.path()).filePath(QStringLiteral("active.json")),
                           active));

        const core::runtime::RuntimeInstallation resolved =
            core::runtime::RuntimeManager(directory.path()).active();
        QVERIFY(!resolved.usable);
        QVERIFY(resolved.failure.contains(QStringLiteral("unsafe"), Qt::CaseInsensitive));
    }

    void atomically_activates_and_rolls_back_runtime_builds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString runtimeId = QStringLiteral("soa-wine-test");

        for (const QString& buildId : {QStringLiteral("build-a"),
                                       QStringLiteral("build-b")})
        {
            const QString installation = QDir(directory.path()).filePath(
                QStringLiteral("installed/%1/%2").arg(runtimeId, buildId));
            const QString bin = QDir(installation).filePath(QStringLiteral("payload/bin"));
            QVERIFY(QDir().mkpath(bin));
            QFile wine(QDir(bin).filePath(QStringLiteral("wine")));
            QVERIFY(wine.open(QIODevice::WriteOnly));
            QVERIFY(wine.write("#!/bin/sh\nexit 0\n") > 0);
            wine.close();
            QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                        | QFileDevice::ExeOwner));
            QVERIFY(write_json(QDir(installation).filePath(QStringLiteral("runtime.json")),
                               valid_runtime_manifest(runtimeId, buildId,
                                                      QStringLiteral("payload/bin/wine"))));
        }

        core::runtime::RuntimeManager manager(directory.path());
        QString error;
        QVERIFY2(manager.activate(runtimeId, QStringLiteral("build-a"), &error),
                 qPrintable(error));
        QCOMPARE(manager.active().manifest.build_id, QStringLiteral("build-a"));
        QVERIFY2(manager.activate(runtimeId, QStringLiteral("build-b"), &error),
                 qPrintable(error));
        QCOMPARE(manager.active().manifest.build_id, QStringLiteral("build-b"));
        QVERIFY2(manager.rollback(&error), qPrintable(error));
        QCOMPARE(manager.active().manifest.build_id, QStringLiteral("build-a"));
    }

    void dxvk_requires_files_and_native_overrides()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString dllDirectory = QDir(prefix).filePath(
            QStringLiteral("drive_c/windows/syswow64"));
        QVERIFY(QDir().mkpath(dllDirectory));

        QFile systemRegistry(QDir(prefix).filePath(QStringLiteral("system.reg")));
        QVERIFY(systemRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(systemRegistry.write("#arch=win64\n") > 0);
        systemRegistry.close();

        for (const QString& dll : {QStringLiteral("d3d9.dll"), QStringLiteral("dxgi.dll")})
        {
            QFile file(QDir(dllDirectory).filePath(dll));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("dxvk") > 0);
        }

        QFile userRegistry(QDir(prefix).filePath(QStringLiteral("user.reg")));
        QVERIFY(userRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(userRegistry.write(
            "[Software\\Wine\\DllOverrides]\n"
            "\"*d3d9\"=\"native\"\n"
            "\"*dxgi\"=\"native,builtin\"\n") > 0);
        userRegistry.close();
        QVERIFY(core::wine::PrefixInspector::dxvk_installed(prefix));
        auto inspection = core::wine::PrefixInspector::inspect(
            prefix, QString(), false);
        QVERIFY(inspection.dxvk_files_present);
        QVERIFY(inspection.dxvk_overrides_present);

        QVERIFY(userRegistry.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
        QVERIFY(userRegistry.write(
            "\"*d3d9\"=\"builtin\"\n"
            "\"*dxgi\"=\"native\"\n") > 0);
        userRegistry.close();
        QVERIFY(!core::wine::PrefixInspector::dxvk_installed(prefix));
        inspection = core::wine::PrefixInspector::inspect(
            prefix, QString(), false);
        QVERIFY(inspection.dxvk_files_present);
        QVERIFY(!inspection.dxvk_overrides_present);
    }

    void dxvk_accepts_unstarred_manual_overrides()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString dllDirectory = QDir(prefix).filePath(
            QStringLiteral("drive_c/windows/syswow64"));
        QVERIFY(QDir().mkpath(dllDirectory));

        QFile systemRegistry(QDir(prefix).filePath(QStringLiteral("system.reg")));
        QVERIFY(systemRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(systemRegistry.write("#arch=win64\n") > 0);
        systemRegistry.close();

        for (const QString& dll : {QStringLiteral("d3d9.dll"), QStringLiteral("dxgi.dll")})
        {
            QFile file(QDir(dllDirectory).filePath(dll));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("dxvk") > 0);
        }

        QFile userRegistry(QDir(prefix).filePath(QStringLiteral("user.reg")));
        QVERIFY(userRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(userRegistry.write(
            "\"d3d9\"=\"native\"\n"
            "\"dxgi\"=\"native\"\n") > 0);
        userRegistry.close();
        QVERIFY(core::wine::PrefixInspector::dxvk_installed(prefix));
        const auto inspection = core::wine::PrefixInspector::inspect(
            prefix, QString(), false);
        QVERIFY(inspection.dxvk_files_present);
        QVERIFY(inspection.dxvk_overrides_present);
    }

    void dxvk_uses_a_reproducible_winetricks_verb()
    {
        QCOMPARE(core::wine::PrefixInspector::dxvk_winetricks_verb(),
                 QStringLiteral("dxvk2071"));
#if !defined(Q_OS_MACOS)
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QStringList packages =
            core::wine::PrefixInspector::missing_packages(
                directory.filePath(QStringLiteral("prefix")),
                false,
                true);
        QVERIFY(packages.contains(
            core::wine::PrefixInspector::dxvk_winetricks_verb()));
        QVERIFY(!packages.contains(QStringLiteral("dxvk")));
#endif
    }


    void reads_architecture_from_user_registry()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        QVERIFY(QDir().mkpath(prefix));

        QFile systemRegistry(QDir(prefix).filePath(QStringLiteral("system.reg")));
        QVERIFY(systemRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(systemRegistry.write("WINE REGISTRY Version 2\n") > 0);
        systemRegistry.close();

        QFile userRegistry(QDir(prefix).filePath(QStringLiteral("user.reg")));
        QVERIFY(userRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(userRegistry.write("#arch=win64\n") > 0);
        userRegistry.close();

        QCOMPARE(core::wine::PrefixInspector::architecture(prefix),
                 core::wine::PrefixArchitecture::Win64);
    }

#if defined(Q_OS_MACOS)
    void falls_back_to_system32_when_syswow64_is_absent()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString system32 = QDir(prefix).filePath(
            QStringLiteral("drive_c/windows/system32"));
        QVERIFY(QDir().mkpath(system32));

        QFile systemRegistry(QDir(prefix).filePath(QStringLiteral("system.reg")));
        QVERIFY(systemRegistry.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(systemRegistry.write("#arch=win64\n") > 0);
        systemRegistry.close();

        QCOMPARE(core::wine::PrefixInspector::game_dll_directory(prefix), system32);
    }

    void accepts_new_wow64_prefix_structure_without_arch_marker()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString driveC = QDir(prefix).filePath(QStringLiteral("drive_c"));
        QVERIFY(QDir().mkpath(QDir(driveC).filePath(QStringLiteral("windows/system32"))));
        QVERIFY(QDir().mkpath(QDir(prefix).filePath(QStringLiteral("dosdevices"))));
        QVERIFY(QFile::link(driveC, QDir(prefix).filePath(QStringLiteral("dosdevices/c:"))));

        for (const QString& name : {QStringLiteral("system.reg"), QStringLiteral("user.reg")})
        {
            QFile registry(QDir(prefix).filePath(name));
            QVERIFY(registry.open(QIODevice::WriteOnly | QIODevice::Text));
            QVERIFY(registry.write("WINE REGISTRY Version 2\n") > 0);
            registry.close();
        }

        const auto inspection = core::wine::PrefixInspector::inspect(
            prefix, QStringLiteral("/tmp/wine"), false);
        QVERIFY(inspection.exists);
        QVERIFY(inspection.structure_valid);
        QCOMPARE(inspection.architecture, core::wine::PrefixArchitecture::Win64);



        QVERIFY(inspection.required_components_present(false));
        QVERIFY(core::wine::PrefixInspector::missing_packages(
                    prefix, false, false).isEmpty());
        QCOMPARE(core::wine::PrefixInspector::game_dll_directory(prefix),
                 QDir(prefix).filePath(QStringLiteral("drive_c/windows/system32")));
    }

    void new_wow64_prefix_becomes_ready_after_legacy_components_are_present()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString prefix = directory.filePath(QStringLiteral("prefix"));
        const QString driveC = QDir(prefix).filePath(QStringLiteral("drive_c"));
        const QString system32 = QDir(driveC).filePath(QStringLiteral("windows/system32"));
        QVERIFY(QDir().mkpath(system32));
        QVERIFY(QDir().mkpath(QDir(prefix).filePath(QStringLiteral("dosdevices"))));
        QVERIFY(QFile::link(driveC, QDir(prefix).filePath(QStringLiteral("dosdevices/c:"))));

        for (const QString& name : {QStringLiteral("system.reg"), QStringLiteral("user.reg")})
        {
            QFile registry(QDir(prefix).filePath(name));
            QVERIFY(registry.open(QIODevice::WriteOnly | QIODevice::Text));
            QVERIFY(registry.write("WINE REGISTRY Version 2\n") > 0);
            registry.close();
        }

        const QStringList requiredDlls {
            QStringLiteral("d3dx9_31.dll"),
            QStringLiteral("d3dx9_42.dll"),
            QStringLiteral("d3dcompiler_42.dll"),
            QStringLiteral("msvcp100.dll"),
            QStringLiteral("msvcr100.dll")
        };
        for (const QString& dll : requiredDlls)
        {
            QFile file(QDir(system32).filePath(dll));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("test") > 0);
            file.close();
        }

        const auto inspection = core::wine::PrefixInspector::inspect(
            prefix, QStringLiteral("/tmp/wine"), false);
        QVERIFY(inspection.structure_valid);
        QCOMPARE(inspection.architecture, core::wine::PrefixArchitecture::Win64);
        QVERIFY(inspection.required_components_present(false));
        QVERIFY(!inspection.physx_runtime);
    }
#endif

    void resolves_runtime_folder_to_wine_entry_point()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString runtime = directory.filePath(QStringLiteral("runtime"));
        const QString bin = QDir(runtime).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));

        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\nexit 0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QCOMPARE(core::wine::macos::resolve_wine_executable(runtime), winePath);
        QCOMPARE(core::wine::macos::runtime_root_for_executable(winePath), runtime);
    }

    void prefers_wine_over_legacy_wine64()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString bin = directory.filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        for (const QString& name : {QStringLiteral("wine64"), QStringLiteral("wine")})
        {
            QFile file(QDir(bin).filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("#!/bin/sh\nexit 0\n") > 0);
            file.close();
            QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                        | QFileDevice::ExeOwner));
        }
        QCOMPARE(core::wine::macos::resolve_wine_executable(directory.path()),
                 QDir(bin).filePath(QStringLiteral("wine")));
    }

    void probes_script_runtime_without_creating_a_prefix()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString bin = directory.filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\necho wine-test-11.0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        const auto probe = core::wine::macos::probe_runtime(directory.path());
        QVERIFY2(probe.usable, qPrintable(probe.failure));
        QCOMPARE(probe.executable, winePath);
        QCOMPARE(probe.version, QStringLiteral("wine-test-11.0"));
    }

#if defined(Q_OS_MACOS)
    void configures_crossover_runtime_root()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString cxRoot = directory.filePath(
            QStringLiteral("CrossOver.app/Contents/SharedSupport/CrossOver"));
        const QString bin = QDir(cxRoot).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        const QString winePath = QDir(bin).filePath(QStringLiteral("wine"));
        QFile wine(winePath);
        QVERIFY(wine.open(QIODevice::WriteOnly));
        QVERIFY(wine.write("#!/bin/sh\nexit 0\n") > 0);
        wine.close();
        QVERIFY(wine.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        QProcessEnvironment environment;
        core::wine::macos::apply_runtime_environment(environment, winePath);
        QCOMPARE(environment.value(QStringLiteral("CX_ROOT")), cxRoot);
    }
#endif

    void game_profiles_remain_distinct()
    {
        const auto& first = core::game::profile(core::game::GameVersion::Playtest);
        const auto& second = core::game::profile(core::game::GameVersion::Alicia2);
        QVERIFY(QString::fromLatin1(first.default_install_directory)
                != QString::fromLatin1(second.default_install_directory));
        QVERIFY(QString::fromLatin1(first.video_settings_registry_key)
                != QString::fromLatin1(second.video_settings_registry_key));
        QCOMPARE(core::game::to_string(core::game::GameVersion::Playtest), QStringLiteral("1.0"));
        QCOMPARE(core::game::to_string(core::game::GameVersion::Alicia2), QStringLiteral("2.0"));
    }
};

QTEST_MAIN(LauncherTests)
#include "main.moc"
