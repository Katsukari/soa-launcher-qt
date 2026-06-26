#include "core/wine/Shell.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "util/Config.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    using util::config::Config;

    Shell::Shell(QObject* parent) : QObject(parent)
    {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);   // stdout+stderr together

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
        if (QFileInfo(wine).isAbsolute()) return QFileInfo(wine).dir().filePath("wineboot");
        return QStringLiteral("wineboot");
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

    void Shell::setup_wine()
    {
        if (!is_wine_installed())
        {
            SPDLOG_ERROR("setup_wine: wine is not installed");
            emit wine_setup_finished(false);
            return;
        }

        const QString prefix = Config::instance().wine_prefix();
        if (prefix.isEmpty())
        {
            SPDLOG_ERROR("setup_wine: no wine prefix configured");
            emit wine_setup_finished(false);
            return;
        }

        if (is_busy())
        {
            SPDLOG_WARN("setup_wine: shell busy, ignoring");
            emit wine_setup_finished(false);
            return;
        }

        const QString arch = Config::instance().wine_arch();

        SPDLOG_INFO("setting up wine prefix at {} (arch {})", prefix.toStdString(), arch.toStdString());

        // Env shared by both wineboot and winetricks.
        setup_env = QProcessEnvironment::systemEnvironment();
        setup_env.insert("WINEPREFIX", prefix);
        setup_env.insert("WINEARCH", arch);
        setup_env.insert("WINE", wine_binary());

        // One connection drives the whole chain, advancing setup_step on each
        auto* conn = new QMetaObject::Connection;
        *conn = connect(this, &Shell::command_finished, this,
            [this, conn](const command_result& r)
            {
                if (setup_step == SetupStep::Idle) return;

                const bool ok = r.started && !r.crashed && r.exit_code == 0;
                if (!ok)
                {
                    SPDLOG_ERROR("wine setup step failed (started={}, crashed={}, exit={})",
                                 r.started, r.crashed, r.exit_code);
                    setup_step = SetupStep::Idle;
                    emit setup_status("Setup failed. Check the log for details.");
                    emit wine_setup_finished(false);
                    disconnect(*conn);
                    delete conn;
                    return;
                }

                switch (setup_step)
                {
                    case SetupStep::Wineboot:
                        SPDLOG_INFO("wineboot complete; installing base packages via winetricks");
                        setup_step = SetupStep::Winetricks;
                        start_winetricks();
                        break;

                    case SetupStep::Winetricks:
                        SPDLOG_INFO("wine prefix setup complete (prefix + base packages)");
                        setup_step = SetupStep::Idle;
                        emit setup_status("Done!");
                        emit wine_setup_finished(true);
                        disconnect(*conn);
                        delete conn;
                        break;

                    default:
                        break;
                }
            });

        setup_step = SetupStep::Wineboot;
        emit setup_status("Creating Wine prefix...");
        run_command(wineboot_binary(), { "--init" }, setup_env);
    }

    void Shell::start_winetricks()
    {
        // Base packages the game needs. DXVK is added only if the user enabled it.
        //   vcrun2019 for convenience and d3dx9, d3dcompiler_47 - required for the game
        QStringList packages = { "-q", "vcrun2019", "d3dx9", "d3dcompiler_47" };

        if (Config::instance().use_dxvk())
        {
            packages << "dxvk";
            SPDLOG_INFO("winetricks: DXVK enabled, including it");
        }

        emit setup_status("Installing components (this can take a while)...");
        SPDLOG_INFO("running: winetricks {}", packages.join(' ').toStdString());
        run_command("winetricks", packages, setup_env);
    }

    void Shell::run_game(const QString& user, const QString& token)
    {
        if (!is_wine_installed())
        {
            SPDLOG_ERROR("run_game: wine is not installed");
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

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", prefix);

        // The game takes flags: Alicia.exe -GameID <id> -ID [user] -OP [token]
        QStringList args;
        args << exePath
             << "-GameID" << gameId
             << "-ID" << QString("[%1]").arg(user)
             << "-OP" << QString("[%1]").arg(token);

        // Append any freeform extra game args from config.
        const QString extra = Config::instance().game_args();
        if (!extra.isEmpty()) args << extra.split(' ', Qt::SkipEmptyParts);

        process->setWorkingDirectory(gameDir);
        run_command(wine_binary(), args, env);
    }

    // stub for later
    void Shell::setup_proton() { }
}