#pragma once

#include <QObject>
#include <QString>

#include "core/game/GameVersion.hpp"

class QFileSystemWatcher;

namespace util::config
{
    class Config : public QObject
    {
        Q_OBJECT

    public:
        static Config& instance();
        ~Config() override;

        QString wine_binary() const;
        QString winetricks_binary() const;






        QString wine_prefix() const;
        QString proton_compat_data_root() const;
        QString prefix_root() const;
        QString wine_arch() const;
        QString game_install_path() const;
        bool    use_dxvk() const;
        bool    runtime_selected() const;
        QString wine_args() const;

        bool    prerequisites_confirmed() const;
        bool    rules_accepted() const;
        bool    keep_signed_in() const;

        bool    launch_on_startup() const;
        QString after_game_start() const;
        QString launcher_size() const;

        core::game::GameVersion game_version() const;
        QString game_args() const;

        QString username() const;
        QString token() const;
        QString display_name() const;
        bool    has_auth() const;
        bool    game_installed() const;
        bool    path_inside_prefix(const QString& path) const;

        void set_wine_binary(const QString& value);
        void set_winetricks_binary(const QString& value);
        void set_wine_prefix(const QString& value);
        void set_game_install_path(const QString& value);
        void set_use_dxvk(bool value);
        void set_runtime_selected(bool value);
        void set_wine_args(const QString& value);

        void set_prerequisites_confirmed(bool value);
        void set_rules_accepted(bool value);
        void set_keep_signed_in(bool value);

        void set_launch_on_startup(bool value);
        void set_after_game_start(const QString& value);
        void set_launcher_size(const QString& value);

        void set_game_version(core::game::GameVersion value);
        void set_game_args(const QString& value);

        void set_auth(const QString& username, const QString& token,
                      const QString& display_name = {});
        bool reset_launcher_config();

        QString file_path() const;
        QString env_path() const;


    signals:
        void changed();

    private:
        explicit Config(QObject* parent = nullptr);
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        void load();
        bool save();
        void load_credentials();
        bool save_credentials();
        bool clear_saved_credentials();
        bool load_env_fallback();
        bool save_env_fallback();
        void apply_defaults();
        void probe_system_paths();
        void watch_files();
        void reload_from_disk();

        QString derive_game_path(const QString& prefix,
                                 core::game::GameVersion version) const;
        QString normalize_game_path(const QString& path) const;
        QString normalize_wine_prefix(const QString& path) const;
        QString normalize_proton_compat_root(const QString& path) const;
        bool runtime_is_proton() const;
        static QString game_install_path_key(core::game::GameVersion version);

        class Impl;
        Impl* d {};

        QFileSystemWatcher* watcher {};
        bool writing {};
    };
}
