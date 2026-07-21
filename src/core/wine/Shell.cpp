#include "core/wine/Shell.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

#include "core/Log.hpp"
#include "core/game/GameVersion.hpp"
#include "core/wine/PrefixInspector.hpp"
#include "core/wine/WineRegistry.hpp"
#include "util/Config.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    using util::config::Config;

    namespace
    {
        constexpr qsizetype k_max_captured_output = 1024 * 1024;
        constexpr int k_runtime_probe_timeout_ms = 15 * 1000;
        constexpr int k_registry_timeout_ms = 45 * 1000;
        const QString k_installing_message = QStringLiteral("Installing components (this can take a while)...");

        QStringList redacted_command_args(const QStringList& arguments)
        {
            QStringList redacted = arguments;
            for (qsizetype index = 0; index < redacted.size(); ++index)
            {
                if (redacted[index].compare(QStringLiteral("-OP"), Qt::CaseInsensitive) == 0
                    && index + 1 < redacted.size())
                {
                    redacted[index + 1] = QStringLiteral("[REDACTED]");
                }
            }
            return redacted;
        }


        QString redact_sensitive_text(QString text)
        {
            const QString token = Config::instance().token();
            if (!token.isEmpty())
                text.replace(token, QStringLiteral("[REDACTED]"));
            return text;
        }

        QString outcome_name(const CommandOutcome outcome)
        {
            switch (outcome)
            {
                case CommandOutcome::Success: return QStringLiteral("success");
                case CommandOutcome::NonZeroExit: return QStringLiteral("non-zero exit");
                case CommandOutcome::FailedToStart: return QStringLiteral("failed to start");
                case CommandOutcome::Crashed: return QStringLiteral("crashed");
                case CommandOutcome::TimedOut: return QStringLiteral("timed out");
                case CommandOutcome::Cancelled: return QStringLiteral("cancelled");
            }
            return QStringLiteral("unknown");
        }
    }

    Shell::Shell(QObject* parent) : StatusReporter(QStringLiteral("wine"), parent)
    {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);
        command_timer = new QTimer(this);
        command_timer->setSingleShot(true);

        connect(process, &QProcess::started, this, [this]()
        {
            current.started = true;
            if (operation_kind == OperationKind::Game && has_pending_launch)
            {
                working(QStringLiteral("game-running"));
                emit game_started(pending_launch.version);
            }
        });
        connect(process, &QProcess::readyReadStandardOutput, this, &Shell::handle_output);
        connect(process, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error)
        {
            if (error == QProcess::FailedToStart)
            {
                finish_process(CommandOutcome::FailedToStart, -1, QProcess::NormalExit,
                               process->errorString());
            }
            else
            {
                SPDLOG_WARN("process error: {}", process->errorString().toStdString());
            }
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](const int exitCode, const QProcess::ExitStatus exitStatus)
        {
            CommandOutcome outcome = forced_outcome;
            if (outcome == CommandOutcome::Success)
            {
                if (exitStatus == QProcess::CrashExit)
                    outcome = CommandOutcome::Crashed;
                else if (exitCode != 0)
                    outcome = CommandOutcome::NonZeroExit;
            }
            finish_process(outcome, exitCode, exitStatus);
        });
        connect(command_timer, &QTimer::timeout, this, [this]()
        {
            if (operation_kind == OperationKind::None || terminal_emitted)
                return;
            forced_outcome = CommandOutcome::TimedOut;
            SPDLOG_ERROR("command timed out: {}", process->program().toStdString());
            process->kill();
        });
    }

    bool Shell::is_busy() const
    {
        return operation_kind != OperationKind::None || process->state() != QProcess::NotRunning;
    }

    bool Shell::is_game_running() const
    {
        return operation_kind == OperationKind::Game && process->state() != QProcess::NotRunning;
    }

    QString Shell::wine_binary() const
    {
        const QString configured = Config::instance().wine_binary();
        return configured.isEmpty() ? QStringLiteral("wine") : configured;
    }

    QString Shell::proton_root() const
    {
        const QFileInfo info(wine_binary());
        return info.isFile() ? info.dir().absolutePath() : info.absoluteFilePath();
    }

    QString Shell::proton_binary() const
    {
        const QFileInfo info(wine_binary());
        if (info.isFile() && info.fileName().compare(QStringLiteral("proton"), Qt::CaseInsensitive) == 0)
            return info.absoluteFilePath();
        return QDir(info.absoluteFilePath()).filePath(QStringLiteral("proton"));
    }

    QString Shell::proton_wine_binary() const
    {
        const QString root = proton_root();
        for (const QString& relative : {
                 QStringLiteral("files/bin/wine"),
                 QStringLiteral("dist/bin/wine"),
                 QStringLiteral("bin/wine")})
        {
            const QString candidate = QDir(root).filePath(relative);
            if (QFileInfo(candidate).isExecutable())
                return candidate;
        }
        return {};
    }

    QString Shell::proton_wineserver_binary() const
    {
        const QString wine = proton_wine_binary();
        return wine.isEmpty() ? QString() : QFileInfo(wine).dir().filePath(QStringLiteral("wineserver"));
    }

    QString Shell::wineboot_binary() const
    {
        const QString wine = resolved_executable(wine_binary());
        if (QFileInfo(wine).isAbsolute())
        {
            const QString sibling = QFileInfo(wine).dir().filePath(QStringLiteral("wineboot"));
            if (QFileInfo(sibling).isExecutable())
                return sibling;
        }
        return QStringLiteral("wineboot");
    }

    bool Shell::runtime_is_proton() const
    {
        return WineRegistry::identify(wine_binary()) == RuntimeType::Proton;
    }

    QString Shell::steam_root() const
    {
        const QString home = QDir::homePath();
        for (const QString& root : {
                 home + QStringLiteral("/.local/share/Steam"),
                 home + QStringLiteral("/.steam/steam"),
                 home + QStringLiteral("/.steam/root"),
                 home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam")})
        {
            if (QFileInfo::exists(root))
                return root;
        }
        return home + QStringLiteral("/.steam");
    }

    QProcessEnvironment Shell::proton_env() const
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"),
                           Config::instance().proton_compat_data_root());
        environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), steam_root());
        environment.insert(QStringLiteral("WINEDLLOVERRIDES"), QStringLiteral("winegstreamer="));
        if (!Config::instance().use_dxvk())
            environment.insert(QStringLiteral("PROTON_USE_WINED3D"), QStringLiteral("1"));
        apply_wine_environment(environment);
        return environment;
    }

    QProcessEnvironment Shell::winetricks_environment() const
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
        if (runtime_is_proton())
        {
            const QString wine = proton_wine_binary();
            const QString wineserver = proton_wineserver_binary();
            if (!wine.isEmpty()) environment.insert(QStringLiteral("WINE"), wine);
            if (!wineserver.isEmpty()) environment.insert(QStringLiteral("WINESERVER"), wineserver);
        }
        else
        {
            environment.insert(QStringLiteral("WINE"), resolved_executable(wine_binary()));
            environment.insert(QStringLiteral("WINEARCH"), Config::instance().wine_arch());
        }
        apply_wine_environment(environment);
        return environment;
    }

    void Shell::apply_wine_environment(QProcessEnvironment& environment) const
    {
        static const QRegularExpression keyPattern(QStringLiteral(R"(^[A-Za-z_][A-Za-z0-9_]*$)"));
        const QStringList tokens = QProcess::splitCommand(Config::instance().wine_args());
        for (const QString& token : tokens)
        {
            const int equals = token.indexOf(QLatin1Char('='));
            if (equals <= 0)
            {
                SPDLOG_WARN("ignoring Wine environment entry (expected KEY=VALUE): {}",
                            token.toStdString());
                continue;
            }
            const QString key = token.left(equals);
            const QString value = token.mid(equals + 1);
            if (!keyPattern.match(key).hasMatch())
            {
                SPDLOG_WARN("ignoring invalid Wine environment key: {}", key.toStdString());
                continue;
            }
            environment.insert(key, value);
            const bool sensitive = key.contains(QStringLiteral("TOKEN"), Qt::CaseInsensitive)
                || key.contains(QStringLiteral("PASSWORD"), Qt::CaseInsensitive)
                || key.contains(QStringLiteral("SECRET"), Qt::CaseInsensitive);
            SPDLOG_DEBUG("wine env: {}={}", key.toStdString(),
                         sensitive ? "[REDACTED]" : value.toStdString());
        }
    }

    QString Shell::resolved_executable(const QString& program) const
    {
        const QFileInfo info(program);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        return QStandardPaths::findExecutable(program);
    }

    bool Shell::is_wine_installed() const
    {
        const QString program = runtime_is_proton() ? proton_binary() : resolved_executable(wine_binary());
        const QFileInfo info(program);
        const bool valid = info.isFile() && info.isExecutable();
        SPDLOG_DEBUG("runtime {}: {}", program.toStdString(), valid ? "usable" : "not executable");
        return valid;
    }

    bool Shell::start_process(const QString& program, const QStringList& arguments,
                              const QProcessEnvironment& environment, const int timeoutMs,
                              const OperationKind kind,
                              std::function<void(const command_result&)> completion)
    {
        if (is_busy())
        {
            SPDLOG_WARN("attempted to start {} while another process is active", program.toStdString());
            return false;
        }

        const QString executable = resolved_executable(program);
        if (executable.isEmpty() || !QFileInfo(executable).isFile() || !QFileInfo(executable).isExecutable())
        {
            SPDLOG_ERROR("executable not found or not runnable: {}", program.toStdString());
            return false;
        }

        current = command_result{};
        terminal_emitted = false;
        forced_outcome = CommandOutcome::Success;
        operation_kind = kind;
        completion_handler = std::move(completion);
        process->setProcessEnvironment(environment);

        const QStringList logged = redacted_command_args(arguments);
        SPDLOG_INFO("running: {} {}", executable.toStdString(), logged.join(QLatin1Char(' ')).toStdString());
        process->start(executable, arguments);
        if (timeoutMs > 0)
            command_timer->start(timeoutMs);
        return true;
    }

    void Shell::run_command(const QString& program, const QStringList& arguments,
                            const QProcessEnvironment& environment)
    {
        if (!start_process(program, arguments, environment, 15 * 60 * 1000,
                           OperationKind::Generic))
        {
            fail_user(QStringLiteral("Could Not Start Command"),
                      QStringLiteral("The selected executable could not be started."));
        }
    }

    void Shell::cancel_current()
    {
        if (!is_busy())
            return;
        forced_outcome = CommandOutcome::Cancelled;
        process->kill();
    }

    void Shell::handle_output()
    {
        QString chunk = redact_sensitive_text(
            QString::fromLocal8Bit(process->readAllStandardOutput()));
        if (chunk.isEmpty())
            return;

        if (current.output.size() + chunk.size() > k_max_captured_output)
        {
            const qsizetype keep = qMax<qsizetype>(0, k_max_captured_output - chunk.size());
            if (keep > 0)
                current.output = current.output.right(keep);
            else
                current.output.clear();
            if (chunk.size() > k_max_captured_output)
                chunk = chunk.right(k_max_captured_output);
            current.output_truncated = true;
        }
        current.output += chunk;

        int logged = 0;
        for (const QString& line : chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        {
            if (logged++ >= 100)
            {
                SPDLOG_DEBUG("[cmd] additional output omitted from this batch");
                break;
            }
            SPDLOG_DEBUG("[cmd] {}", line.left(4096).toStdString());
        }
    }

    void Shell::finish_process(const CommandOutcome outcome, const int exitCode,
                               const QProcess::ExitStatus exitStatus,
                               const QString& errorMessage)
    {
        if (terminal_emitted || operation_kind == OperationKind::None)
            return;
        terminal_emitted = true;
        command_timer->stop();
        handle_output();

        const OperationKind finishedKind = operation_kind;
        const auto completion = std::move(completion_handler);
        current.outcome = outcome;
        current.exit_code = exitCode;
        current.crashed = exitStatus == QProcess::CrashExit || outcome == CommandOutcome::Crashed;
        current.error_message = errorMessage;
        operation_kind = OperationKind::None;
        completion_handler = {};

        SPDLOG_INFO("command completed: {} (exit {})", outcome_name(outcome).toStdString(), exitCode);
        emit command_finished(current);

        if (finishedKind == OperationKind::Game && has_pending_launch)
        {
            const auto version = pending_launch.version;
            emit game_exited(version, exitCode, current.crashed);
            if (current.ok())
                done(QStringLiteral("Game exited."));
            else
                fail(QStringLiteral("The game exited unexpectedly."));
            has_pending_launch = false;
        }

        if (completion)
            completion(current);
    }

    void Shell::fail_user(const QString& title, const QString& message)
    {
        SPDLOG_ERROR("{}: {}", title.toStdString(), message.toStdString());
        fail(message);
        emit user_error(title, message);
    }

    QStringList Shell::missing_component_packages() const
    {
        return PrefixInspector::missing_packages(
            Config::instance().prefix_root(), runtime_is_proton(), Config::instance().use_dxvk());
    }

    bool Shell::required_components_present() const
    {
        const auto inspection = PrefixInspector::inspect(
            Config::instance().prefix_root(), Config::instance().wine_binary(), runtime_is_proton());
        return inspection.required_components_present(runtime_is_proton());
    }

    bool Shell::queue_missing_components()
    {
        const QStringList packages = missing_component_packages();
        if (packages.isEmpty())
            return true;

        const QString winetricks = winetricks_path();
        if (winetricks.isEmpty() || !QFileInfo(winetricks).isExecutable())
        {
            SPDLOG_ERROR("missing components require winetricks: {}", packages.join(QStringLiteral(", ")).toStdString());
            return false;
        }
        if (runtime_is_proton() && proton_wine_binary().isEmpty())
        {
            SPDLOG_ERROR("selected Proton runtime does not expose a Wine binary for winetricks");
            return false;
        }

        QStringList arguments {QStringLiteral("-q")};
        arguments.append(packages);
        setup_queue.insert(setup_index + 1,
            SetupCommand{k_installing_message, winetricks, arguments,
                         winetricks_environment(), 30 * 60 * 1000, false, false});
        return true;
    }

    void Shell::finish_setup_failure(const QString& message)
    {
        setup_index = -1;
        setup_queue.clear();
        setup_kind = OperationKind::None;
        emit setup_status(message);
        fail_user(QStringLiteral("Wine Setup Failed"), message);
        emit wine_setup_finished(false);
    }

    void Shell::run_setup(QVector<SetupCommand> commands, const OperationKind kind)
    {
        if (is_busy())
        {
            fail_user(QStringLiteral("Launcher Busy"),
                      QStringLiteral("Another Wine or game process is already running."));
            emit wine_setup_finished(false);
            return;
        }
        setup_queue = std::move(commands);
        setup_index = 0;
        setup_kind = kind;
        marker_invalidated = false;
        advance_setup();
    }

    void Shell::advance_setup()
    {
        if (setup_index < 0)
            return;
        if (setup_index >= setup_queue.size())
        {
            setup_index = -1;
            setup_queue.clear();
            const OperationKind completedKind = setup_kind;
            setup_kind = OperationKind::None;

            if (!required_components_present())
            {
                finish_setup_failure(QStringLiteral(
                    "Prefix setup finished, but required components are still missing."));
                return;
            }
            if (!PrefixInspector::write_marker(
                    Config::instance().prefix_root(), Config::instance().wine_binary()))
            {
                finish_setup_failure(QStringLiteral(
                    "Setup completed, but the prefix could not be marked ready."));
                return;
            }

            emit setup_status(QStringLiteral("Done!"));
            done(completedKind == OperationKind::Dxvk
                ? QStringLiteral("DXVK setup complete.")
                : QStringLiteral("Prefix setup complete."));
            emit wine_setup_finished(true);
            return;
        }

        const SetupCommand command = setup_queue[setup_index];
        if (command.invalidates_marker && !marker_invalidated)
        {
            if (!PrefixInspector::remove_marker(Config::instance().prefix_root()))
            {
                finish_setup_failure(QStringLiteral("Could not invalidate the old prefix setup marker."));
                return;
            }
            marker_invalidated = true;
        }

        emit setup_status(command.message);
        working(QStringLiteral("prefix-setup"));
        if (!start_process(command.program, command.args, command.env, command.timeout_ms,
                           setup_kind,
                           [this, command](const command_result& result)
        {
            if (!result.ok())
            {
                finish_setup_failure(QStringLiteral("%1 (%2). Check the launcher log for details.")
                    .arg(command.message, outcome_name(result.outcome)));
                return;
            }
            if (command.inspect_components_after && !queue_missing_components())
            {
                finish_setup_failure(QStringLiteral(
                    "Required components are missing, but Winetricks could not install them."));
                return;
            }
            ++setup_index;
            advance_setup();
        }))
        {
            finish_setup_failure(QStringLiteral("Could not start %1").arg(command.message));
        }
    }

    void Shell::setup()
    {
        if (runtime_is_proton())
            setup_proton();
        else
            setup_wine();
    }

    void Shell::setup_wine()
    {
        if (!is_wine_installed())
        {
            fail_user(QStringLiteral("Wine Not Available"),
                      QStringLiteral("The selected Wine runtime is missing or not executable."));
            emit wine_setup_finished(false);
            return;
        }
        const QString prefix = Config::instance().prefix_root();
        if (prefix.isEmpty() || !QDir().mkpath(prefix))
        {
            fail_user(QStringLiteral("Invalid Prefix"),
                      QStringLiteral("The Wine prefix directory could not be created."));
            emit wine_setup_finished(false);
            return;
        }

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), prefix);
        environment.insert(QStringLiteral("WINEARCH"), Config::instance().wine_arch());
        environment.insert(QStringLiteral("WINE"), resolved_executable(wine_binary()));
        apply_wine_environment(environment);

        QVector<SetupCommand> commands;
        commands.push_back({QStringLiteral("Validating Wine runtime..."), wine_binary(),
                            {QStringLiteral("--version")}, environment,
                            k_runtime_probe_timeout_ms, false, false});
        commands.push_back({QStringLiteral("Creating Wine prefix..."), wineboot_binary(),
                            {QStringLiteral("--init")}, environment,
                            5 * 60 * 1000, true, true});
        run_setup(std::move(commands), OperationKind::Setup);
    }

    void Shell::setup_proton()
    {
        if (!is_wine_installed())
        {
            fail_user(QStringLiteral("Proton Not Available"),
                      QStringLiteral("The selected Proton runtime is missing or not executable."));
            emit wine_setup_finished(false);
            return;
        }
        const QString compat = Config::instance().proton_compat_data_root();
        if (compat.isEmpty() || !QDir().mkpath(compat))
        {
            fail_user(QStringLiteral("Invalid Compatibility Data Path"),
                      QStringLiteral("The Proton compatibility-data directory could not be created."));
            emit wine_setup_finished(false);
            return;
        }

        const QString protonWine = proton_wine_binary();
        if (protonWine.isEmpty())
        {
            fail_user(QStringLiteral("Invalid Proton Runtime"),
                      QStringLiteral("The selected Proton installation does not contain a runnable Wine binary."));
            emit wine_setup_finished(false);
            return;
        }

        QVector<SetupCommand> commands;
        // Valve Proton is a verb-based wrapper, not a conventional CLI program.
        // `runinprefix` invokes Proton's own Wine binary inside STEAM_COMPAT_DATA_PATH
        // and is the appropriate mode for Wine utilities such as wineboot. The actual
        // prefix-creation command is also the runtime probe, so avoid a redundant
        // `proton --version`/bundled-Wine probe before it.
        commands.push_back({QStringLiteral("Creating Proton prefix..."), proton_binary(),
                            {QStringLiteral("runinprefix"), QStringLiteral("wineboot"),
                             QStringLiteral("--init")},
                            proton_env(), 5 * 60 * 1000, true, true});
        run_setup(std::move(commands), OperationKind::Setup);
    }

    void Shell::sync_dxvk()
    {
        if (runtime_is_proton())
        {
            emit user_notice(QStringLiteral("Proton provides its own DXVK; no separate installation is needed."));
            return;
        }
        if (!Config::instance().use_dxvk())
        {
            emit user_notice(QStringLiteral("DXVK is disabled; WineD3D will be used."));
            return;
        }
        if (PrefixInspector::dxvk_installed(Config::instance().prefix_root()))
        {
            emit user_notice(QStringLiteral("DXVK is already installed in this prefix."));
            return;
        }

        const QString winetricks = winetricks_path();
        if (winetricks.isEmpty() || !QFileInfo(winetricks).isExecutable())
        {
            fail_user(QStringLiteral("Winetricks Not Available"),
                      QStringLiteral("DXVK cannot be installed because Winetricks is unavailable."));
            return;
        }

        QVector<SetupCommand> commands;
        commands.push_back({QStringLiteral("Installing DXVK..."), winetricks,
                            {QStringLiteral("-q"), QStringLiteral("dxvk")},
                            winetricks_environment(), 30 * 60 * 1000, false, true});
        run_setup(std::move(commands), OperationKind::Dxvk);
    }

    void Shell::run_game(const QString& user, const QString& token)
    {
        if (is_busy())
        {
            fail_user(QStringLiteral("Launcher Busy"),
                      is_game_running()
                        ? QStringLiteral("Alicia is already running.")
                        : QStringLiteral("Wait for the current Wine operation to finish."));
            return;
        }
        if (!is_wine_installed())
        {
            fail_user(QStringLiteral("Runtime Missing"),
                      QStringLiteral("The selected Wine or Proton runtime is unavailable."));
            return;
        }
        if (user.isEmpty() || token.isEmpty())
        {
            fail_user(QStringLiteral("Sign In Required"),
                      QStringLiteral("Your login session is missing or expired. Sign in again."));
            return;
        }

        const auto version = Config::instance().game_version();
        const auto& profile = core::game::profile(version);
        const QString gameDirectory = Config::instance().game_install_path();
        const QString executable = QDir(gameDirectory).filePath(QString::fromLatin1(profile.executable_name));
        if (!QFileInfo(executable).isFile())
        {
            fail_user(QStringLiteral("Game Not Found"),
                      QStringLiteral("%1 was not found in the selected game folder.")
                          .arg(QString::fromLatin1(profile.executable_name)));
            return;
        }
        if (!Config::instance().path_inside_prefix(gameDirectory))
        {
            fail_user(QStringLiteral("Invalid Game Path"),
                      QStringLiteral("The configured game folder is outside the active Wine prefix."));
            return;
        }

        PendingLaunch launch;
        launch.version = version;
        launch.user = user;
        launch.token = token;
        launch.game_directory = gameDirectory;
        launch.executable_path = executable;
        begin_first_launch_setup(std::move(launch));
    }

    void Shell::begin_first_launch_setup(PendingLaunch launch)
    {
        pending_launch = std::move(launch);
        has_pending_launch = true;
        emit game_starting(pending_launch.version);
        working(QStringLiteral("game-first-launch"));

        const bool proton = runtime_is_proton();
        QProcessEnvironment environment = proton ? proton_env() : QProcessEnvironment::systemEnvironment();
        if (!proton)
        {
            environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
            apply_wine_environment(environment);
        }

        QStringList arguments;
        if (proton) arguments << QStringLiteral("run");
        arguments << QStringLiteral("reg.exe") << QStringLiteral("query")
                  << QStringLiteral("HKCU\\Software\\Story of Alicia\\Launcher");
        const QString program = proton ? proton_binary() : wine_binary();
        if (!start_process(program, arguments, environment, k_registry_timeout_ms,
                           OperationKind::FirstLaunchQuery,
                           [this](const command_result& result) { handle_first_launch_query(result); }))
        {
            has_pending_launch = false;
            fail_user(QStringLiteral("First-Launch Setup Failed"),
                      QStringLiteral("The Wine registry could not be inspected."));
        }
    }

    bool Shell::write_first_launch_registry(const PendingLaunch& launch,
                                            const QString& queryOutput,
                                            QString& registryFile,
                                            bool& needsImport) const
    {
        const auto& profile = core::game::profile(launch.version);
        const QString baseFlag = QString::fromLatin1(profile.first_launch_registry_value);
        const QString audioFlag = QString::fromLatin1(profile.audio_settings_setup_registry_value);
        const bool querySucceeded = !queryOutput.isNull();
        const bool baseDone = querySucceeded && queryOutput.contains(baseFlag, Qt::CaseInsensitive);
        const bool audioDone = audioFlag.isEmpty()
            || (querySucceeded && queryOutput.contains(audioFlag, Qt::CaseInsensitive));
        needsImport = !baseDone || !audioDone;
        if (!needsImport)
            return true;

        QString registry = QStringLiteral("REGEDIT4\r\n\r\n");
        if (!baseDone && profile.video_settings_registry_key[0] != '\0')
        {
            registry += QStringLiteral("[HKEY_CURRENT_USER\\%1]\r\n")
                .arg(QString::fromLatin1(profile.video_settings_registry_key));
            registry += QStringLiteral("\"screenResolutionID\"=\"0\"\r\n");
            registry += QStringLiteral("\"screenWindowType\"=\"1\"\r\n");
            registry += QStringLiteral("\"Width\"=\"0\"\r\n");
            registry += QStringLiteral("\"Height\"=\"0\"\r\n\r\n");
        }
        if (!audioDone && profile.audio_settings_registry_key[0] != '\0')
        {
            registry += QStringLiteral("[HKEY_CURRENT_USER\\%1]\r\n")
                .arg(QString::fromLatin1(profile.audio_settings_registry_key));
            registry += QStringLiteral("\"VolBGM\"=\"30\"\r\n");
            registry += QStringLiteral("\"VolSFX\"=\"30\"\r\n\r\n");
        }
        registry += QStringLiteral("[HKEY_CURRENT_USER\\Software\\Story of Alicia\\Launcher]\r\n");
        if (!baseDone)
            registry += QStringLiteral("\"%1\"=dword:00000001\r\n").arg(baseFlag);
        if (!audioDone && !audioFlag.isEmpty())
            registry += QStringLiteral("\"%1\"=dword:00000001\r\n").arg(audioFlag);

        registryFile = QDir(Config::instance().prefix_root())
            .filePath(QStringLiteral("drive_c/.soa-first-launch.reg"));
        QSaveFile file(registryFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        if (file.write(registry.toLatin1()) < 0)
            return false;
        return file.commit();
    }

    void Shell::handle_first_launch_query(const command_result& result)
    {
        if (!has_pending_launch)
            return;
        if (!result.started || result.outcome == CommandOutcome::TimedOut
            || result.outcome == CommandOutcome::Cancelled
            || result.outcome == CommandOutcome::FailedToStart
            || result.outcome == CommandOutcome::Crashed)
        {
            has_pending_launch = false;
            fail_user(QStringLiteral("First-Launch Setup Failed"),
                      QStringLiteral("The Wine registry could not be inspected."));
            return;
        }

        bool needsImport = false;
        QString registryFile;
        const QString queryOutput = result.exit_code == 0 ? result.output : QString();
        if (!write_first_launch_registry(pending_launch, queryOutput, registryFile, needsImport))
        {
            has_pending_launch = false;
            fail_user(QStringLiteral("First-Launch Setup Failed"),
                      QStringLiteral("The default game settings could not be prepared."));
            return;
        }
        if (!needsImport)
        {
            launch_pending_game();
            return;
        }

        pending_registry_file = registryFile;
        const bool proton = runtime_is_proton();
        QProcessEnvironment environment = proton ? proton_env() : QProcessEnvironment::systemEnvironment();
        if (!proton)
        {
            environment.insert(QStringLiteral("WINEPREFIX"), Config::instance().prefix_root());
            apply_wine_environment(environment);
        }
        QStringList arguments;
        if (proton) arguments << QStringLiteral("run");
        arguments << QStringLiteral("regedit.exe") << QStringLiteral("/S")
                  << QStringLiteral("C:\\.soa-first-launch.reg");
        const QString program = proton ? proton_binary() : wine_binary();
        if (!start_process(program, arguments, environment, k_registry_timeout_ms,
                           OperationKind::FirstLaunchImport,
                           [this](const command_result& importResult)
        {
            QFile::remove(pending_registry_file);
            pending_registry_file.clear();
            if (!importResult.ok())
            {
                has_pending_launch = false;
                fail_user(QStringLiteral("First-Launch Setup Failed"),
                          QStringLiteral("The default video and audio settings could not be applied."));
                return;
            }
            launch_pending_game();
        }))
        {
            QFile::remove(pending_registry_file);
            pending_registry_file.clear();
            has_pending_launch = false;
            fail_user(QStringLiteral("First-Launch Setup Failed"),
                      QStringLiteral("The registry import could not be started."));
        }
    }

    void Shell::launch_pending_game()
    {
        if (!has_pending_launch)
            return;
        launch_game_process(pending_launch);
    }

    void Shell::launch_game_process(const PendingLaunch& launch)
    {
        const auto& profile = core::game::profile(launch.version);
        const QString prefix = Config::instance().prefix_root();
        const bool proton = runtime_is_proton();
        const bool requestedDxvk = Config::instance().use_dxvk();
        const bool effectiveDxvk = requestedDxvk && (proton || PrefixInspector::dxvk_installed(prefix));
        if (requestedDxvk && !effectiveDxvk)
        {
            emit user_notice(QStringLiteral(
                "DXVK was requested but is not installed. Falling back to WineD3D for this launch."));
        }

        QStringList gameArguments;
        gameArguments << QStringLiteral("-GameID") << QString::fromLatin1(profile.launch_game_id)
                      << QStringLiteral("-ID") << QStringLiteral("[%1]").arg(launch.user)
                      << QStringLiteral("-OP") << QStringLiteral("[%1]").arg(launch.token);
        if (!Config::instance().game_args().trimmed().isEmpty())
            gameArguments.append(QProcess::splitCommand(Config::instance().game_args()));

        process->setWorkingDirectory(launch.game_directory);
        if (proton)
        {
            const QString umu = umu_path();
            if (umu.isEmpty() || !QFileInfo(umu).isExecutable())
            {
                has_pending_launch = false;
                fail_user(QStringLiteral("UMU Not Available"),
                          QStringLiteral("Proton launch requires a working umu-run installation."));
                return;
            }

            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("GAMEID"), QStringLiteral("0"));
            environment.insert(QStringLiteral("STORE"), QStringLiteral("none"));
            environment.insert(QStringLiteral("PROTONPATH"), proton_root());
            environment.insert(QStringLiteral("WINEPREFIX"), prefix);
            environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"),
                               Config::instance().proton_compat_data_root());
            environment.insert(QStringLiteral("STEAM_COMPAT_LIBRARY_PATHS"),
                               launch.game_directory + QLatin1Char(':') + prefix);
            environment.insert(QStringLiteral("WINEDLLOVERRIDES"), QStringLiteral("winegstreamer="));
            if (!effectiveDxvk)
                environment.insert(QStringLiteral("PROTON_USE_WINED3D"), QStringLiteral("1"));
            apply_wine_environment(environment);

            QStringList arguments {launch.executable_path};
            arguments.append(gameArguments);
            if (!start_process(umu, arguments, environment, 0, OperationKind::Game))
            {
                has_pending_launch = false;
                fail_user(QStringLiteral("Game Launch Failed"),
                          QStringLiteral("umu-run could not be started."));
            }
        }
        else
        {
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("WINEPREFIX"), prefix);
            environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
            environment.insert(QStringLiteral("WINEDLLOVERRIDES"), effectiveDxvk
                ? QStringLiteral("d3d9,d3d10core,d3d11,dxgi=n")
                : QStringLiteral("d3d9,d3d10core,d3d11,dxgi=b"));
            apply_wine_environment(environment);

            QStringList arguments {launch.executable_path};
            arguments.append(gameArguments);
            if (!start_process(wine_binary(), arguments, environment, 0, OperationKind::Game))
            {
                has_pending_launch = false;
                fail_user(QStringLiteral("Game Launch Failed"),
                          QStringLiteral("Wine could not start Alicia.exe."));
            }
        }
    }
}
