#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

#include "core/status/StatusReporter.hpp"

namespace core::wine
{
    struct command_result
    {
        bool    started   {false};
        int     exit_code {-1};
        bool    crashed   {false};
        QString output;
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

        void setup_wine();
        void setup_proton();
        void run_game(const QString& user, const QString& token);

        signals:
            void command_finished(const command_result& result);
        void wine_setup_finished(bool ok);
        void setup_status(const QString& message);

    private:
        QString wine_binary() const;
        QString wineboot_binary() const;

        void start_winetricks();
        void handle_output();

        QProcess*      process {};
        command_result current;

        enum class SetupStep
        {
            Idle,
            Wineboot,
            Winetricks,
        };
        SetupStep           setup_step { SetupStep::Idle };
        QProcessEnvironment setup_env;
    };
}