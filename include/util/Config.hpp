#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QFileSystemWatcher;

namespace util::config
{
    class Config : public QObject
    {
        Q_OBJECT

    public:
        static Config& instance();

        QString wine_binary() const;
        QString winetricks_binary() const;
        QString rosetta_x87_path() const;

        QString wine_prefix() const;
        QString wine_arch() const;
        QString game_install_path() const;
        bool    use_dxvk() const;
        QString wine_args() const;

        bool    launch_on_startup() const;
        QString after_game_start() const;
        QString launcher_size() const;

        QString game_id() const;
        QString game_args() const;

        QString username() const;
        QString token() const;
        QString display_name() const;
        bool    has_auth() const;
        bool    game_installed() const;

        void set_wine_binary(const QString& v);
        void set_winetricks_binary(const QString& v);
        void set_rosetta_x87_path(const QString& v);
        void set_wine_prefix(const QString& v);
        void set_game_install_path(const QString& v);
        void set_wine_arch(const QString& v);
        void set_use_dxvk(bool v);
        void set_wine_args(const QString& v);

        void set_launch_on_startup(bool v);
        void set_after_game_start(const QString& v);
        void set_launcher_size(const QString& v);

        void set_game_id(const QString& v);
        void set_game_args(const QString& v);

        void set_auth(const QString& username, const QString& token, const QString& display_name = {});
        void clear_auth();

        QString file_path() const;
        QString env_path() const;

        void reload();

    signals:
        void changed();

    private:
        explicit Config(QObject* parent = nullptr);
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        void load();
        void save();
        void load_env();
        void save_env();
        void apply_defaults();
        void probe_system_paths();
        QString derive_game_path(const QString& prefix) const;

        class Impl;
        Impl* d {};

        QFileSystemWatcher* watcher {};
        bool writing {};
    };
}