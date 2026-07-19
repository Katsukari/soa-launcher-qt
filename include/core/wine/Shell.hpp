#pragma once
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>
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
            void setup();
            void setup_wine();
            void setup_proton();
            void sync_dxvk();
            void run_game(const QString& user, const QString& token);
            signals:
                void command_finished(const command_result& result);
            void wine_setup_finished(bool ok);
            void setup_status(const QString& message);
        private:
            QString wine_binary() const;
            QString wineboot_binary() const;
            bool    runtime_is_proton() const;
            QString proton_binary() const;
            QString steam_root() const;
            QProcessEnvironment proton_env() const;
            void    handle_output();

            struct SetupCommand
            {
                QString             message;
                QString             program;
                QStringList         args;
                QProcessEnvironment env;
            };
            void run_setup(QVector<SetupCommand> commands);
            void advance_setup();

            QProcess*             process {};
            command_result        current;
            QVector<SetupCommand> setup_queue;
            int                   setup_index { -1 };
    };
}