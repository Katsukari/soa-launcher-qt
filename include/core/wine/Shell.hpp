#pragma once

#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <atomic>
#include <memory>
#include <optional>

#include "core/game/GameVersion.hpp"
#include "core/status/StatusReporter.hpp"
#include "core/wine/WineProcess.hpp"

class QTimer;
class QFile;

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
        void detect_existing_game();

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
            PrefixCleanup,
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
            bool optional_failure {};
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
        QString runtime_self_test_binary() const;
        bool runtime_is_proton() const;
        QString proton_root() const;
        QString proton_binary() const;
        QString proton_wine_binary() const;
        QString proton_wineserver_binary() const;
        QString wineserver_binary() const;
        QString steam_root() const;
        QProcessEnvironment proton_env() const;
        QProcessEnvironment winetricks_environment() const;
        void apply_wine_environment(QProcessEnvironment& env) const;
        void handle_output();
        bool prepare_host_invocation(const QString& program, const QStringList& arguments,
                                     const QProcessEnvironment& environment,
                                     QString& executable, QStringList& launch_arguments,
                                     QProcessEnvironment& launch_environment) const;

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
        void finalize_setup(OperationKind completed_kind, int attempts_remaining);
        void finish_setup_failure(const QString& message);
        void finish_optional_dxvk_failure(const QString& message);
        void skip_optional_setup_command(const QString& message);
        bool queue_missing_components();
        QStringList missing_component_packages() const;

        void begin_launch_preflight(PendingLaunch launch);
        void clean_stale_prefix_processes();
        void wait_for_prefix_shutdown();
        void begin_first_launch_setup(PendingLaunch launch);
        void handle_first_launch_query(const command_result& result);
        bool write_first_launch_registry(const PendingLaunch& launch,
                                         const QString& query_output,
                                         QString& registry_file,
                                         bool& needs_import) const;
        void launch_pending_game();
        void launch_game_process(const PendingLaunch& launch);

        bool start_game_probe(std::function<void(bool, std::optional<WindowsProcessInfo>)> completion);
        bool start_host_game_probe(
            std::function<void(bool, std::optional<WindowsProcessInfo>)> completion);
        bool start_game_lifecycle_probe(
            std::function<void(bool, std::optional<WindowsProcessInfo>)> completion);
        void capture_game_probe_context();
        void clear_game_probe_context();
        void handle_game_probe_output();
        void begin_game_verification();
        void poll_game_process();
        void attach_to_game_process(const WindowsProcessInfo& info, bool restored_session);
        void refresh_game_host_diagnostics();
#if defined(Q_OS_MACOS)
        void arm_macos_deep_diagnostic_sample();
        bool macos_trace_has_fatal_game_failure() const;
        void begin_macos_launch_timeline(const QString& profile,
                                         const PendingLaunch& launch);
        void append_macos_launch_timeline(const QString& event,
                                          const QString& details = {});
        void finish_macos_launch_timeline(const QString& outcome,
                                          int exit_code, bool crashed,
                                          const QString& details = {});
#endif
        void handle_game_launcher_finished(int exit_code, QProcess::ExitStatus exit_status);
        void fail_game_launch(const QString& message);
        void stop_game_monitoring_uncertain(const QString& message);
        void finish_game_session(int exit_code, bool crashed);


        QProcess* process {};
        QProcess* game_probe_process {};
        QTimer* command_timer {};
        QTimer* force_kill_timer {};
        QTimer* game_monitor_timer {};
        QTimer* game_probe_timeout_timer {};
#if defined(Q_OS_MACOS)
        std::shared_ptr<std::atomic_uint64_t> game_snapshot_generation;
#endif
        command_result current;
        OperationKind operation_kind {OperationKind::None};
        std::function<void(const command_result&)> completion_handler;
        bool terminal_emitted {};
        CommandOutcome forced_outcome {CommandOutcome::Success};
        qint64 force_kill_process_id {-1};

        QVector<SetupCommand> setup_queue;
        int setup_index {-1};
        OperationKind setup_kind {OperationKind::None};
        bool marker_invalidated {};

        PendingLaunch pending_launch;
        bool has_pending_launch {};
        QString pending_registry_file;

        QByteArray game_probe_output;
        QString game_probe_program_snapshot;
        QString game_probe_prefix_snapshot;
        QProcessEnvironment game_probe_environment_snapshot;
        std::function<void(bool, std::optional<WindowsProcessInfo>)> game_probe_completion;
        bool game_probe_host_mode {};
        bool game_probe_context_valid {};
        WindowsProcessInfo tracked_game_process;
        WindowsProcessInfo tracked_game_host_process;
        core::game::GameVersion tracked_game_version {core::game::GameVersion::Playtest};
        bool game_running_confirmed {};
        bool game_lifecycle_uncertain {};
        bool game_preflight_active {};
        bool game_launcher_finished {};
        bool restored_game_session {};
        int game_launcher_exit_code {-1};
        bool game_launcher_crashed {};
        qint64 game_wrapper_finished_elapsed_ms {-1};
#if defined(Q_OS_MACOS)
        QFile* game_diagnostic_file {};
        QString game_diagnostic_path;
        QString game_timeline_path;
        QString game_launch_session_id;
        QElapsedTimer game_launch_clock;
        quint64 game_launch_generation {};
        bool game_timeline_finished {};
        bool game_timeline_seen_create_device {};
        bool game_timeline_seen_present {};
        bool game_timeline_seen_draw {};
        bool game_timeline_seen_exception {};
        bool game_snapshot_armed {};
#endif
        int game_verification_attempts {};
        int game_probe_failures {};
        int missing_game_probes {};
    };
}
