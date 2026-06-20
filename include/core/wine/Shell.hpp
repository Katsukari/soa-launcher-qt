#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>
#include <string>
#include <functional>

namespace core::wine
{
    struct wine_config
    {
        std::string proton_version {};
        std::string wine_version {};
        std::string wine_arch {};        // "win64" / "win32"
        std::string wine_prefix {};
        std::string wine_binary {};
        std::string game_install_path {};
        std::string launcher_root_path {};
    };

    struct command_result
    {
        bool        started   {false};   // did the process launch at all
        int         exit_code {-1};
        bool        crashed   {false};   // exited abnormally vs normal exit
        QString     output;              // combined stdout+stderr
    };

    class Shell : public QObject
    {
        Q_OBJECT
        public:
            explicit Shell(QObject* parent = nullptr);

            void set_root_path(const QString& root);
            QString wine_prefix() const { return QString::fromStdString(config.wine_prefix); }
            QString game_install_path() const { return QString::fromStdString(config.game_install_path); }

        void set_wine_binary(const QString& path);
        QString wine_binary() const;

        void run_command(const QString& program, const QStringList& args,
        const QProcessEnvironment& env = QProcessEnvironment::systemEnvironment());

            bool is_wine_installed() const;

            bool is_busy() const; // a command is currently running

            void setup_wine();

            // stubbed for later
            void setup_proton();
            void run_game(const QString& user, const QString& token);

            signals:
                void command_finished(const command_result& result);
                void wine_setup_finished(bool ok);

        private:
            QProcess*      process {};
            command_result current;
            wine_config    config;

            void handle_output();

            bool setting_up_wine {false};
            bool stream_to_terminal {false};
    };
}