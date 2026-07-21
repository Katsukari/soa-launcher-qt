#pragma once

#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "core/game/GameVersion.hpp"
#include "core/status/StatusReporter.hpp"

class QTimer;

namespace core::wine
{
    enum class CommandOutcome
    {
        Success,
        NonZeroExit,
        FailedToStart,
        Crashed,
        TimedOut,
        Cancelled
    };

    struct command_result
    {
        CommandOutcome outcome {CommandOutcome::FailedToStart};
        bool started {};
        int exit_code {-1};
        bool crashed {};
        bool output_truncated {};
        QString output;
        QString error_message;

        [[nodiscard]] bool ok() const { return outcome == CommandOutcome::Success; }
    };

    class Shell : public status::StatusReporter
    {
        Q_OBJECT

    public:
        explicit Shell(QObject* parent = nullptr);

        void run_command(const QString& program, const QStringList& args,
                         const QProcessEnvironment& env = QProcessEnvironment::systemEnvironment());
        bool is_wine_installed() const;
        bool is_busy() const;
        bool is_game_running() const;
        void cancel_current();
        void setup();
        void setup_wine();
        void setup_proton();
        void sync_dxvk();
        void run_game(const QString& user, const QString& token);

    signals:
        void command_finished(const command_result& result);
        void wine_setup_finished(bool ok);
        void setup_status(const QString& message);
        void user_error(const QString& title, const QString& message);
        void user_notice(const QString& message);
        void game_starting(core::game::GameVersion version);
        void game_started(core::game::GameVersion version);
        void game_exited(core::game::GameVersion version, int exit_code, bool crashed);

    private:
        enum class OperationKind
        {
            None,
            Generic,
            Setup,
            Dxvk,
            FirstLaunchQuery,
            FirstLaunchImport,
            Game
        };

        struct SetupCommand
        {
            QString message;
            QString program;
            QStringList args;
            QProcessEnvironment env;
            int timeout_ms {15 * 60 * 1000};
            bool inspect_components_after {};
            bool invalidates_marker {};
        };

        struct PendingLaunch
        {
            core::game::GameVersion version {core::game::GameVersion::Playtest};
            QString user;
            QString token;
            QString game_directory;
            QString executable_path;
        };

        QString wine_binary() const;
        QString wineboot_binary() const;
        bool runtime_is_proton() const;
        QString proton_root() const;
        QString proton_binary() const;
        QString proton_wine_binary() const;
        QString proton_wineserver_binary() const;
        QString steam_root() const;
        QProcessEnvironment proton_env() const;
        QProcessEnvironment winetricks_environment() const;
        void apply_wine_environment(QProcessEnvironment& env) const;
        void handle_output();

        bool start_process(const QString& program, const QStringList& args,
                           const QProcessEnvironment& env, int timeout_ms,
                           OperationKind kind,
                           std::function<void(const command_result&)> completion = {});
        void finish_process(CommandOutcome outcome, int exit_code,
                            QProcess::ExitStatus exit_status,
                            const QString& error_message = {});
        void fail_user(const QString& title, const QString& message);
        QString resolved_executable(const QString& program) const;

        void run_setup(QVector<SetupCommand> commands, OperationKind kind);
        void advance_setup();
        void finish_setup_failure(const QString& message);
        bool queue_missing_components();
        bool required_components_present() const;
        QStringList missing_component_packages() const;

        void begin_first_launch_setup(PendingLaunch launch);
        void handle_first_launch_query(const command_result& result);
        bool write_first_launch_registry(const PendingLaunch& launch,
                                         const QString& query_output,
                                         QString& registry_file,
                                         bool& needs_import) const;
        void launch_pending_game();
        void launch_game_process(const PendingLaunch& launch);

        QProcess* process {};
        QTimer* command_timer {};
        command_result current;
        OperationKind operation_kind {OperationKind::None};
        std::function<void(const command_result&)> completion_handler;
        bool terminal_emitted {};
        CommandOutcome forced_outcome {CommandOutcome::Success};

        QVector<SetupCommand> setup_queue;
        int setup_index {-1};
        OperationKind setup_kind {OperationKind::None};
        bool marker_invalidated {};

        PendingLaunch pending_launch;
        bool has_pending_launch {};
        QString pending_registry_file;
    };
}
