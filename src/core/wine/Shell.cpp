#include "core/wine/Shell.hpp"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "util/Config.hpp"
#include "core/wine/WineRegistry.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    using util::config::Config;

    namespace
    {
        const QString k_installing_msg = "Installing components (this can take a while)...";

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
        SPDLOG_INFO("running: {} {}", program.toStdString(), args.join(' ').toStdString());
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

    void Shell::advance_setup()
    {
        if (setup_index < 0) return;

        if (setup_index >= setup_queue.size())
        {
            SPDLOG_INFO("setup sequence complete");
            setup_index = -1;
            setup_queue.clear();
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

        QStringList packages = { "-q", "vcrun2019", "d3dx9", "d3dcompiler_47" };
        if (Config::instance().use_dxvk())
        {
            packages << "dxvk";
            SPDLOG_INFO("winetricks: DXVK enabled, installing it");
        }
        else
            SPDLOG_INFO("winetricks: DXVK disabled, using wined3d");

        QVector<SetupCommand> commands;
        commands.push_back({ "Creating Wine prefix...", wineboot_binary(), { "--init" }, env });
        commands.push_back({ k_installing_msg, "winetricks", packages, env });
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

        const QString pfx = QDir(compat).filePath("pfx");
        SPDLOG_INFO("setting up proton prefix at {} (pfx {})", compat.toStdString(), pfx.toStdString());
        if (Config::instance().use_dxvk())
            SPDLOG_INFO("DXVK: using Proton's built-in DXVK (no separate install needed)");
        else
            SPDLOG_INFO("DXVK: off, Proton will use wined3d");

        QProcessEnvironment tricks_env = QProcessEnvironment::systemEnvironment();
        tricks_env.insert("WINEPREFIX", pfx);

        QStringList packages = { "-q", "d3dx9", "d3dcompiler_47" };

        QVector<SetupCommand> commands;
        commands.push_back({ "Creating Proton prefix...", proton, { "run", "wineboot", "--init" }, proton_env() });
        commands.push_back({ k_installing_msg, "winetricks", packages, tricks_env });
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
        commands.push_back({ "Installing DXVK...", "winetricks", { "-q", "dxvk" }, env });
        run_setup(std::move(commands));
    }

    void Shell::run_game(const QString& user, const QString& token)
    {
        const bool proton = runtime_is_proton();

        if (proton ? !QFileInfo::exists(proton_binary()) : !is_wine_installed())
        {
            SPDLOG_ERROR("run_game: runtime is not installed");
            return;
        }

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

        const QString exePath = QDir(gameDir).filePath("Alicia.exe");
        if (!QFileInfo::exists(exePath))
        {
            SPDLOG_ERROR("run_game: Alicia.exe not found at {}", exePath.toStdString());
            return;
        }

        const QString prefix = Config::instance().wine_prefix();
        const QString gameId = Config::instance().game_id();
        SPDLOG_INFO("launching game as user {} (prefix {})", user.toStdString(), prefix.toStdString());

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
            const QString umu = QStandardPaths::findExecutable("umu-run");
            if (!umu.isEmpty())
            {
                SPDLOG_INFO("launching via umu-run (Steam Linux Runtime): {}", umu.toStdString());

                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert("GAMEID", "0");
                env.insert("STORE", "none");
                env.insert("PROTONPATH", wine_binary());
                env.insert("WINEPREFIX", prefix);
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
                SPDLOG_WARN("umu-run not found; falling back to bare Proton. Assets may fail to load "
                            "or the screen may go black mid-game. Install umu-launcher for the Steam Linux Runtime.");

                QProcessEnvironment env = proton_env();
                apply_wine_args(env);

                QStringList args;
                args << "run" << exePath << gameArgs;
                run_command(proton_binary(), args, env);
            }
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