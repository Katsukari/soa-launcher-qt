#include "core/wine/Shell.hpp"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include "util/Config.hpp"
#include "core/game/GameVersion.hpp"
#include "core/wine/WineRegistry.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    using util::config::Config;

    namespace
    {
        const QString k_installing_msg = "Installing components (this can take a while)...";

        QStringList redacted_command_args(const QStringList& args)
        {
            QStringList redacted = args;

            for (qsizetype i = 0; i < redacted.size(); ++i)
            {
                if (redacted[i].compare("-OP", Qt::CaseInsensitive) != 0)
                    continue;

                if (i + 1 < redacted.size())
                    redacted[i + 1] = "[REDACTED]";
            }

            return redacted;
        }

        void apply_wine_args(QProcessEnvironment& env)
        {
            const QStringList tokens = Config::instance().wine_args().split(' ', Qt::SkipEmptyParts);
            for (const QString& token : tokens)
            {
                const int eq = token.indexOf('=');
                if (eq <= 0)
                {
                    SPDLOG_WARN("ignoring wine arg (expected KEY=VALUE): {}", token.toStdString());
                    continue;
                }
                const QString key = token.left(eq);
                const QString value = token.mid(eq + 1);
                env.insert(key, value);
                SPDLOG_INFO("wine env: {}={}", key.toStdString(), value.toStdString());
            }
        }

        QString prefix_marker_path()
        {
            return QDir(Config::instance().prefix_root()).filePath(".soa-prefix-ready");
        }

        void remove_prefix_marker()
        {
            const QString marker = prefix_marker_path();
            if (QFileInfo::exists(marker) && !QFile::remove(marker))
                SPDLOG_WARN("could not remove stale prefix marker at {}", marker.toStdString());
        }

        bool write_prefix_marker()
        {
            QSaveFile marker(prefix_marker_path());
            if (!marker.open(QIODevice::WriteOnly | QIODevice::Text))
                return false;

            marker.write("1\n");
            marker.write(Config::instance().wine_binary().toUtf8());
            marker.write("\n");
            return marker.commit();
        }

        bool dxvk_in_prefix(const QString& prefix)
        {
            QFile log(QDir(prefix).filePath("winetricks.log"));
            if (!log.open(QIODevice::ReadOnly | QIODevice::Text))
                return false;
            while (!log.atEnd())
            {
                if (QString::fromUtf8(log.readLine()).trimmed() == "dxvk")
                    return true;
            }
            return false;
        }

        bool prefix_is_win64(const QString& prefix)
        {
            QFile registry(QDir(prefix).filePath("system.reg"));
            if (!registry.open(QIODevice::ReadOnly | QIODevice::Text))
                return false;

            return QString::fromUtf8(registry.readLine()).contains("win64", Qt::CaseInsensitive);
        }

        QString game_dll_directory(const QString& prefix)
        {
            const QString windows = QDir(prefix).filePath("drive_c/windows");
            return QDir(windows).filePath(prefix_is_win64(prefix) ? "syswow64" : "system32");
        }

        bool component_dll_exists(const QString& prefix, const QString& name)
        {
            return QFileInfo::exists(QDir(game_dll_directory(prefix)).filePath(name));
        }
    }

    Shell::Shell(QObject* parent) : StatusReporter("wine", parent)
    {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);

        connect(process, &QProcess::readyReadStandardOutput, this, &Shell::handle_output);

        connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err)
            {
                if (err == QProcess::FailedToStart)
                {
                    current.started = false;
                    SPDLOG_ERROR("command failed to start: {}", process->program().toStdString());
                }
            });

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status)
            {
                current.exit_code = code;
                current.crashed   = (status == QProcess::CrashExit);

                if (current.crashed) SPDLOG_ERROR("command crashed (exit {})", code);
                else if (code != 0) SPDLOG_WARN("command exited non-zero: {}", code);
                else SPDLOG_DEBUG("command finished ok");

                emit command_finished(current);
            });

        connect(this, &Shell::command_finished, this, [this](const command_result& r)
            {
                if (setup_index < 0) return;

                const bool ok = r.started && !r.crashed && r.exit_code == 0;
                if (!ok)
                {
                    SPDLOG_ERROR("setup step failed (started={}, crashed={}, exit={})",
                                 r.started, r.crashed, r.exit_code);
                    setup_index = -1;
                    setup_queue.clear();
                    const QString msg = "Setup failed. Check the log for details.";
                    emit setup_status(msg);
                    fail(msg);
                    emit wine_setup_finished(false);
                    return;
                }

                if (setup_queue[setup_index].inspect_components_after && !queue_missing_components())
                {
                    setup_index = -1;
                    setup_queue.clear();
                    const QString msg = "Required components are missing and could not be installed.";
                    emit setup_status(msg);
                    fail(msg);
                    emit wine_setup_finished(false);
                    return;
                }

                ++setup_index;
                advance_setup();
            });
    }

    bool Shell::is_busy() const
    {
        return process->state() != QProcess::NotRunning;
    }

    QString Shell::wine_binary() const
    {
        const QString w = Config::instance().wine_binary();
        return w.isEmpty() ? QStringLiteral("wine") : w;
    }

    QString Shell::wineboot_binary() const
    {
        const QString wine = wine_binary();
        if (QFileInfo(wine).isAbsolute())
            return QFileInfo(wine).dir().filePath("wineboot");
        return QStringLiteral("wineboot");
    }

    bool Shell::runtime_is_proton() const
    {
        return WineRegistry::identify(wine_binary()) == RuntimeType::Proton;
    }

    QString Shell::proton_binary() const
    {
        return QDir(wine_binary()).filePath("proton");
    }

    QString Shell::steam_root() const
    {
        const QString home = QDir::homePath();
        for (const QString& root : { home + "/.local/share/Steam",
                                     home + "/.steam/steam",
                                     home + "/.steam/root",
                                     home + "/.var/app/com.valvesoftware.Steam/data/Steam" })
        {
            if (QFileInfo::exists(root)) return root;
        }
        return home + "/.steam";
    }

    QProcessEnvironment Shell::proton_env() const
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("STEAM_COMPAT_DATA_PATH", Config::instance().wine_prefix());
        env.insert("STEAM_COMPAT_CLIENT_INSTALL_PATH", steam_root());
        env.insert("WINEDLLOVERRIDES", "winegstreamer=");
        if (!Config::instance().use_dxvk())
            env.insert("PROTON_USE_WINED3D", "1");
        return env;
    }

    void Shell::run_command(const QString& program, const QStringList& args,
                            const QProcessEnvironment& env)
    {
        if (is_busy())
        {
            SPDLOG_WARN("run_command called while a command is already running; ignoring");
            return;
        }

        current = command_result{};
        current.started = true;

        process->setProcessEnvironment(env);
        const QStringList logged_args = redacted_command_args(args);
        SPDLOG_INFO("running: {} {}", program.toStdString(), logged_args.join(' ').toStdString());
        process->start(program, args);
    }

    void Shell::handle_output()
    {
        const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
        current.output += chunk;
        for (const QString& line : chunk.split('\n', Qt::SkipEmptyParts)) SPDLOG_DEBUG("[cmd] {}", line.toStdString());
    }

    bool Shell::is_wine_installed() const
    {
        const QString wine = wine_binary();

        if (QFileInfo(wine).isAbsolute())
        {
            const bool ok = QFileInfo::exists(wine);
            SPDLOG_DEBUG("wine binary {}: {}", wine.toStdString(), ok ? "found" : "NOT found");
            return ok;
        }

        const QString found = QStandardPaths::findExecutable(wine);
        if (found.isEmpty())
        {
            SPDLOG_DEBUG("wine not found on PATH");
            return false;
        }
        SPDLOG_DEBUG("wine found: {}", found.toStdString());
        return true;
    }

    void Shell::run_setup(QVector<SetupCommand> commands)
    {
        if (is_busy())
        {
            SPDLOG_WARN("run_setup: shell busy, ignoring");
            emit wine_setup_finished(false);
            return;
        }

        setup_queue = std::move(commands);
        setup_index = 0;
        advance_setup();
    }

    QStringList Shell::missing_component_packages() const
    {
        const QString prefix = Config::instance().prefix_root();
        QStringList packages;

        if (!component_dll_exists(prefix, "d3dx9_43.dll"))
            packages << "d3dx9";

        if (!component_dll_exists(prefix, "d3dcompiler_47.dll"))
            packages << "d3dcompiler_47";

        if (!runtime_is_proton()
            && (!component_dll_exists(prefix, "msvcp140.dll")
                || !component_dll_exists(prefix, "vcruntime140.dll")))
        {
            packages << "vcrun2019";
        }

        if (!runtime_is_proton() && Config::instance().use_dxvk() && !dxvk_in_prefix(prefix))
            packages << "dxvk";

        return packages;
    }

    bool Shell::required_components_present() const
    {
        const QString prefix = Config::instance().prefix_root();
        const bool d3dx9 = component_dll_exists(prefix, "d3dx9_43.dll");
        const bool compiler = component_dll_exists(prefix, "d3dcompiler_47.dll");
        const bool runtime = runtime_is_proton()
            || (component_dll_exists(prefix, "msvcp140.dll")
                && component_dll_exists(prefix, "vcruntime140.dll"));

        SPDLOG_INFO("prefix components: d3dx9_43={}, d3dcompiler_47={}, runtime={}",
                    d3dx9 ? "found" : "missing",
                    compiler ? "found" : "missing",
                    runtime ? "found" : "missing");

        return d3dx9 && compiler && runtime;
    }

    QProcessEnvironment Shell::winetricks_environment() const
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", Config::instance().prefix_root());

        if (runtime_is_proton())
        {
            env.insert("WINE", QDir(wine_binary()).filePath("files/bin/wine"));
            env.insert("WINESERVER", QDir(wine_binary()).filePath("files/bin/wineserver"));
        }
        else
        {
            env.insert("WINE", wine_binary());
            env.insert("WINEARCH", Config::instance().wine_arch());
        }

        return env;
    }

    bool Shell::queue_missing_components()
    {
        const QStringList packages = missing_component_packages();
        if (packages.isEmpty())
        {
            SPDLOG_INFO("prefix already has all required components; skipping winetricks");
            return true;
        }

        const QString tricks = winetricks_path();
        if (tricks.isEmpty())
        {
            SPDLOG_ERROR("missing components require winetricks: {}",
                         packages.join(", ").toStdString());
            return false;
        }

        QStringList args { "-q" };
        args.append(packages);

        SPDLOG_INFO("installing missing prefix components: {}",
                    packages.join(", ").toStdString());

        setup_queue.insert(setup_index + 1,
            { k_installing_msg, tricks, args, winetricks_environment(), false });
        return true;
    }

    void Shell::advance_setup()
    {
        if (setup_index < 0) return;

        if (setup_index >= setup_queue.size())
        {
            setup_index = -1;
            setup_queue.clear();

            if (!required_components_present())
            {
                const QString msg = "Prefix setup finished, but required components are still missing.";
                SPDLOG_ERROR("prefix component verification failed");
                emit setup_status(msg);
                fail(msg);
                emit wine_setup_finished(false);
                return;
            }

            if (!write_prefix_marker())
            {
                const QString msg = "Setup completed, but the prefix could not be marked ready.";
                SPDLOG_ERROR("could not write prefix marker at {}", prefix_marker_path().toStdString());
                emit setup_status(msg);
                fail(msg);
                emit wine_setup_finished(false);
                return;
            }

            SPDLOG_INFO("setup sequence complete");
            emit setup_status("Done!");
            done();
            emit wine_setup_finished(true);
            return;
        }

        const SetupCommand& cmd = setup_queue[setup_index];
        emit setup_status(cmd.message);
        working(cmd.message);
        run_command(cmd.program, cmd.args, cmd.env);
    }

    void Shell::setup()
    {
        remove_prefix_marker();

        if (runtime_is_proton())
        {
            SPDLOG_INFO("runtime is Proton; using proton setup");
            setup_proton();
        }
        else
        {
            SPDLOG_INFO("runtime is Wine; using wine setup");
            setup_wine();
        }
    }

    void Shell::setup_wine()
    {
        if (!is_wine_installed())
        {
            SPDLOG_ERROR("setup_wine: wine is not installed");
            fail("Wine is not installed");
            emit wine_setup_finished(false);
            return;
        }

        const QString prefix = Config::instance().wine_prefix();
        if (prefix.isEmpty())
        {
            SPDLOG_ERROR("setup_wine: no wine prefix configured");
            fail("No Wine prefix configured");
            emit wine_setup_finished(false);
            return;
        }

        const QString arch = Config::instance().wine_arch();
        SPDLOG_INFO("setting up wine prefix at {} (arch {})", prefix.toStdString(), arch.toStdString());

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", prefix);
        env.insert("WINEARCH", arch);
        env.insert("WINE", wine_binary());

        if (Config::instance().use_dxvk())
            SPDLOG_INFO("DXVK: enabled for Wine; it will only be installed if missing");
        else
            SPDLOG_INFO("DXVK: disabled, using wined3d");

        QVector<SetupCommand> commands;
        commands.push_back({ "Creating Wine prefix...", wineboot_binary(), { "--init" }, env, true });
        run_setup(std::move(commands));
    }

    void Shell::setup_proton()
    {
        const QString proton = proton_binary();
        if (!QFileInfo::exists(proton))
        {
            SPDLOG_ERROR("setup_proton: proton not found at {}", proton.toStdString());
            fail("Proton runtime not found");
            emit wine_setup_finished(false);
            return;
        }

        const QString compat = Config::instance().wine_prefix();
        if (compat.isEmpty())
        {
            SPDLOG_ERROR("setup_proton: no prefix configured");
            fail("No prefix configured");
            emit wine_setup_finished(false);
            return;
        }

        if (!QDir().mkpath(compat))
        {
            SPDLOG_ERROR("setup_proton: could not create compat dir {}", compat.toStdString());
            fail("Could not create prefix directory");
            emit wine_setup_finished(false);
            return;
        }

        const QString pfx = Config::instance().prefix_root();
        SPDLOG_INFO("setting up proton prefix at {} (pfx {})", compat.toStdString(), pfx.toStdString());
        if (Config::instance().use_dxvk())
            SPDLOG_INFO("DXVK: using Proton's built-in DXVK (no separate install needed)");
        else
            SPDLOG_INFO("DXVK: off, Proton will use wined3d");

        QVector<SetupCommand> commands;
        commands.push_back({ "Creating Proton prefix...", proton,
                             { "run", "wineboot", "--init" }, proton_env(), true });
        run_setup(std::move(commands));
    }

    void Shell::sync_dxvk()
    {
        if (runtime_is_proton())
        {
            SPDLOG_INFO("dxvk: Proton runtime ships its own DXVK, nothing to install");
            return;
        }

        const QString prefix = Config::instance().wine_prefix();
        if (!QFileInfo::exists(QDir(prefix).filePath("drive_c")))
        {
            SPDLOG_INFO("dxvk: no wine prefix yet, it will be installed during setup");
            return;
        }

        if (!Config::instance().use_dxvk())
        {
            SPDLOG_INFO("dxvk: disabled, wine will use wined3d at launch");
            return;
        }

        if (dxvk_in_prefix(prefix))
        {
            SPDLOG_INFO("dxvk: already installed in {}", prefix.toStdString());
            return;
        }

        if (!is_wine_installed())
        {
            SPDLOG_ERROR("dxvk: wine is not installed");
            return;
        }

        SPDLOG_INFO("dxvk: installing into {}", prefix.toStdString());

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", prefix);
        env.insert("WINE", wine_binary());

        QVector<SetupCommand> commands;
        commands.push_back({ "Installing DXVK...", winetricks_path(), { "-q", "dxvk" }, env });
        run_setup(std::move(commands));
    }

    command_result Shell::run_blocking_command(const QString& program, const QStringList& args,
                                                     const QProcessEnvironment& env) const
    {
        command_result result;
        QProcess command;
        command.setProcessChannelMode(QProcess::MergedChannels);
        command.setProcessEnvironment(env);

        const QStringList logged_args = redacted_command_args(args);
        SPDLOG_INFO("running: {} {}", program.toStdString(), logged_args.join(' ').toStdString());

        command.start(program, args);
        result.started = command.waitForStarted(10000);
        if (!result.started)
        {
            SPDLOG_ERROR("command failed to start: {}", program.toStdString());
            return result;
        }

        if (!command.waitForFinished(30000))
        {
            SPDLOG_ERROR("command timed out: {}", program.toStdString());
            command.kill();
            command.waitForFinished();
            result.crashed = true;
            result.output = QString::fromLocal8Bit(command.readAllStandardOutput());
            return result;
        }

        result.exit_code = command.exitCode();
        result.crashed = command.exitStatus() == QProcess::CrashExit;
        result.output = QString::fromLocal8Bit(command.readAllStandardOutput());

        for (const QString& line : result.output.split('\n', Qt::SkipEmptyParts))
            SPDLOG_DEBUG("[cmd] {}", line.toStdString());

        if (result.crashed)
            SPDLOG_ERROR("command crashed (exit {})", result.exit_code);
        else if (result.exit_code != 0)
            SPDLOG_DEBUG("command exited non-zero: {}", result.exit_code);
        else
            SPDLOG_DEBUG("command finished ok");

        return result;
    }

    bool Shell::apply_first_launch_settings()
    {
        const auto version = Config::instance().game_version();
        const auto& game = core::game::profile(version);
        const bool proton = runtime_is_proton();

        QProcessEnvironment env = proton
            ? proton_env()
            : QProcessEnvironment::systemEnvironment();

        if (!proton)
            env.insert("WINEPREFIX", Config::instance().wine_prefix());

        const QString program = proton ? proton_binary() : wine_binary();
        QStringList query_args;
        if (proton)
            query_args << "run";
        query_args << "reg.exe" << "query" << "HKCU\\Software\\Story of Alicia\\Launcher";

        const command_result query = run_blocking_command(program, query_args, env);
        if (!query.started || query.crashed)
        {
            SPDLOG_ERROR("first launch: could not inspect launcher registry state");
            return false;
        }

        const QString base_flag = QString::fromLatin1(game.first_launch_registry_value);
        const QString audio_flag = QString::fromLatin1(game.audio_settings_setup_registry_value);
        const bool base_done = query.exit_code == 0
            && query.output.contains(base_flag, Qt::CaseInsensitive);
        const bool audio_done = audio_flag.isEmpty()
            || (query.exit_code == 0 && query.output.contains(audio_flag, Qt::CaseInsensitive));

        if (base_done && audio_done)
        {
            SPDLOG_DEBUG("first launch: defaults already applied for game {}",
                         core::game::to_string(version).toStdString());
            return true;
        }

        QString registry = QStringLiteral("REGEDIT4\r\n\r\n");

        if (!base_done && game.video_settings_registry_key[0] != '\0')
        {
            registry += QStringLiteral("[HKEY_CURRENT_USER\\%1]\r\n")
                .arg(QString::fromLatin1(game.video_settings_registry_key));
            registry += QStringLiteral("\"screenResolutionID\"=\"0\"\r\n");
            registry += QStringLiteral("\"screenWindowType\"=\"1\"\r\n");
            registry += QStringLiteral("\"Width\"=\"0\"\r\n");
            registry += QStringLiteral("\"Height\"=\"0\"\r\n\r\n");
        }

        if (!audio_done && game.audio_settings_registry_key[0] != '\0')
        {
            registry += QStringLiteral("[HKEY_CURRENT_USER\\%1]\r\n")
                .arg(QString::fromLatin1(game.audio_settings_registry_key));
            registry += QStringLiteral("\"VolBGM\"=\"30\"\r\n");
            registry += QStringLiteral("\"VolSFX\"=\"30\"\r\n\r\n");
        }

        registry += QStringLiteral("[HKEY_CURRENT_USER\\Software\\Story of Alicia\\Launcher]\r\n");
        if (!base_done)
            registry += QStringLiteral("\"%1\"=dword:00000001\r\n").arg(base_flag);
        if (!audio_done && !audio_flag.isEmpty())
            registry += QStringLiteral("\"%1\"=dword:00000001\r\n").arg(audio_flag);

        const QString registry_file = QDir(Config::instance().prefix_root())
            .filePath("drive_c/.soa-first-launch.reg");
        QSaveFile file(registry_file);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            SPDLOG_ERROR("first launch: could not write {}", registry_file.toStdString());
            return false;
        }

        file.write(registry.toLatin1());
        if (!file.commit())
        {
            SPDLOG_ERROR("first launch: could not commit {}", registry_file.toStdString());
            return false;
        }

        QStringList import_args;
        if (proton)
            import_args << "run";
        import_args << "regedit.exe" << "/S" << "C:\\.soa-first-launch.reg";

        SPDLOG_INFO("first launch: applying defaults for game {}",
                    core::game::to_string(version).toStdString());
        const command_result imported = run_blocking_command(program, import_args, env);
        QFile::remove(registry_file);

        if (!imported.started || imported.crashed || imported.exit_code != 0)
        {
            SPDLOG_ERROR("first launch: registry import failed for game {}",
                         core::game::to_string(version).toStdString());
            return false;
        }

        SPDLOG_INFO("first launch: defaults applied for game {}",
                    core::game::to_string(version).toStdString());
        return true;
    }

    void Shell::run_game(const QString& user, const QString& token)
    {
        const bool proton = runtime_is_proton();

        if (proton ? !QFileInfo::exists(proton_binary()) : !is_wine_installed())
        {
            SPDLOG_ERROR("run_game: runtime is not installed");
            return;
        }

        const auto version = Config::instance().game_version();
        const auto& game = core::game::profile(version);
        const QString gameDir = Config::instance().game_install_path();
        if (gameDir.isEmpty())
        {
            SPDLOG_ERROR("run_game: no game install path configured");
            return;
        }

        if (is_busy())
        {
            SPDLOG_WARN("run_game: shell busy, ignoring");
            return;
        }

        const QString executable_name = QString::fromLatin1(game.executable_name);
        const QString exePath = QDir(gameDir).filePath(executable_name);
        if (!QFileInfo::exists(exePath))
        {
            SPDLOG_ERROR("run_game: {} not found at {}",
                         executable_name.toStdString(), exePath.toStdString());
            return;
        }

        if (!apply_first_launch_settings())
        {
            fail("First-time game setup failed");
            return;
        }

        const QString prefix = Config::instance().wine_prefix();
        const QString configured_game_id = Config::instance().game_id();
        const QString gameId = configured_game_id.isEmpty()
            ? QString::fromLatin1(game.launch_game_id)
            : configured_game_id;
        SPDLOG_INFO("launching game {} as user {} (prefix {})",
                    core::game::to_string(version).toStdString(),
                    user.toStdString(), prefix.toStdString());

        const bool dxvk = Config::instance().use_dxvk();
        if (proton)
            SPDLOG_INFO("renderer: {}", dxvk ? "Proton built-in DXVK (Vulkan)" : "Proton wined3d (OpenGL)");
        else if (dxvk && !dxvk_in_prefix(prefix))
            SPDLOG_WARN("renderer: wined3d (OpenGL) - DXVK is on but not installed in this prefix; re-run setup to install it");
        else
            SPDLOG_INFO("renderer: {}", dxvk ? "DXVK (Vulkan)" : "wined3d (OpenGL)");

        QStringList gameArgs;
        gameArgs << "-GameID" << gameId
                 << "-ID" << QString("[%1]").arg(user)
                 << "-OP" << QString("[%1]").arg(token);

        const QString extra = Config::instance().game_args();
        if (!extra.isEmpty())
            gameArgs << extra.split(' ', Qt::SkipEmptyParts);

        process->setWorkingDirectory(gameDir);

        if (proton)
        {
            const QString umu = umu_path();
            if (umu.isEmpty())
            {
                SPDLOG_ERROR("run_game: umu-run not found; Proton launch requires UMU");
                fail("UMU is not installed");
                return;
            }

            SPDLOG_INFO("launching via umu-run (Steam Linux Runtime): {}", umu.toStdString());

            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("GAMEID", "0");
            env.insert("STORE", "none");
            env.insert("PROTONPATH", wine_binary());
            env.insert("WINEPREFIX", Config::instance().prefix_root());
            env.insert("STEAM_COMPAT_LIBRARY_PATHS", gameDir + ":" + prefix);
            env.insert("WINEDLLOVERRIDES", "winegstreamer=");
            if (!dxvk)
                env.insert("PROTON_USE_WINED3D", "1");
            apply_wine_args(env);

            QStringList args;
            args << exePath << gameArgs;
            run_command(umu, args, env);
        }
        else
        {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("WINEPREFIX", prefix);
            env.insert("WINEDLLOVERRIDES", dxvk ? "d3d9,d3d10core,d3d11,dxgi=n"
                                                : "d3d9,d3d10core,d3d11,dxgi=b");
            apply_wine_args(env);

            QStringList args;
            args << exePath << gameArgs;
            run_command(wine_binary(), args, env);
        }
    }
}