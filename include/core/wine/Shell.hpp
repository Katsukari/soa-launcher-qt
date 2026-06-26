#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

namespace core::wine
{
    struct command_result
    {
        bool    started   {false};   // did the process launch at all
        int     exit_code {-1};
        bool    crashed   {false};   // exited abnormally vs normal exit
        QString output;              // combined stdout+stderr
    };

    class Shell : public QObject
    {
        Q_OBJECT
        public:
            explicit Shell(QObject* parent = nullptr);

            void run_command(const QString& program, const QStringList& args,
                             const QProcessEnvironment& env = QProcessEnvironment::systemEnvironment());

            bool is_wine_installed() const;
            bool is_busy() const;            // a command is currently running

            void setup_wine();               // wineboot then winetricks (base packages)
            void setup_proton();             // stubbed for later
            void run_game(const QString& user, const QString& token);

        signals:
            void command_finished(const command_result& result);
            void wine_setup_finished(bool ok);
            void setup_status(const QString& message);

        private:
            // Resolve the wine / wineboot binaries from config (with PATH fallback)
            QString wine_binary() const;
            QString wineboot_binary() const;

            void start_winetricks();
            void handle_output();

            QProcess*      process {};
            command_result current;

            // Multi-step wine setup state machine
            enum class SetupStep
            {
                Idle,
                Wineboot,
                Winetricks,
            };
            SetupStep           setup_step { SetupStep::Idle };
            QProcessEnvironment setup_env;   // carried across the steps
    };
}