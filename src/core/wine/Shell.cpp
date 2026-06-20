#include "core/wine/Shell.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    Shell::Shell(QObject* parent) : QObject(parent)
    {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);   // stdout+stderr together

        // Stream output as it arrives
        connect(process, &QProcess::readyReadStandardOutput, this, &Shell::handle_output);

        // Process failed to start / crashed / etc
        connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err)
            {
                if (err == QProcess::FailedToStart)
                {
                    current.started = false;
                    SPDLOG_ERROR("command failed to start: {}", process->program().toStdString());
                }
            });

        // Completion.
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
        set_root_path(QDir(QDir::homePath()).filePath("soa-launcher"));
    }

    void Shell::set_root_path(const QString& prefix_root)
    {
        // The root the user picks IS the wine prefix (or where to generate one)
        config.wine_prefix = prefix_root.toStdString();

        // Game lives inside the prefix, at the Windows-mirrored AppData path
        const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
        const QString game = QDir(prefix_root).filePath(QString("drive_c/users/%1/AppData/Roaming/Story Of Alicia/game").arg(user));
        config.game_install_path = game.toStdString();

        SPDLOG_DEBUG("prefix {} -> game {}", config.wine_prefix, config.game_install_path);
    }

    bool Shell::is_busy() const
    {
        return process->state() != QProcess::NotRunning;
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
        const QString path = QStandardPaths::findExecutable("wine");
        if (path.isEmpty())
        {
            SPDLOG_DEBUG("wine not found on PATH");
            return false;
        }
        SPDLOG_DEBUG("wine found: {}", path.toStdString());
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

        if (config.wine_prefix.empty())
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

        const QString prefix = QString::fromStdString(config.wine_prefix);
        const QString arch   = config.wine_arch.empty() ? QStringLiteral("win64") : QString::fromStdString(config.wine_arch);

        SPDLOG_INFO("setting up wine prefix at {} (arch {})", config.wine_prefix, arch.toStdString());

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", prefix);
        env.insert("WINEARCH", arch);

        setting_up_wine = true;
        auto* conn = new QMetaObject::Connection;
        *conn = connect(this, &Shell::command_finished, this,
            [this, conn](const command_result& r)
            {
                if (!setting_up_wine) return;
                setting_up_wine = false;

                const bool ok = r.started && !r.crashed && r.exit_code == 0;
                if (ok)
                {
                    SPDLOG_INFO("wine prefix setup complete");
                }
                else
                {
                    SPDLOG_ERROR("wine prefix setup failed (started={}, crashed={}, exit={})", r.started, r.crashed, r.exit_code);
                }

                emit wine_setup_finished(ok);
                disconnect(*conn);
                delete conn;
            });

        run_command("wineboot", { "--init" }, env);
    }

    void Shell::run_game(const QString& user, const QString& token)
    {
        if (!is_wine_installed())
        {
            SPDLOG_ERROR("run_game: wine is not installed");
            return;
        }

        if (config.game_install_path.empty())
        {
            SPDLOG_ERROR("run_game: no game install path configured");
            return;
        }

        if (is_busy())
        {
            SPDLOG_WARN("run_game: shell busy, ignoring");
            return;
        }

        const QString gameDir = QString::fromStdString(config.game_install_path);
        const QString exePath = QDir(gameDir).filePath("Alicia.exe");

        if (!QFileInfo::exists(exePath))
        {
            SPDLOG_ERROR("run_game: Alicia.exe not found at {}", exePath.toStdString());
            return;
        }

        const QString prefix = QString::fromStdString(config.wine_prefix);

        SPDLOG_INFO("launching game as user {} (prefix {})", user.toStdString(), prefix.toStdString());

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", prefix);
        // env.insert("DXVK_HUD", "fps");

        // The game takes flags: Alicia.exe -GameID 4 -ID [user] -OP [token]
        // Run through wine, with the working dir set to the game folder
        QStringList args;
        args << exePath
             << "-GameID" << "4"
             << "-ID" << QString("[%1]").arg(user)
             << "-OP" << QString("[%1]").arg(token);

        process->setWorkingDirectory(gameDir);
        run_command("wine", args, env);
    }

    // stub for later
    void Shell::setup_proton() { }
}