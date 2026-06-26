#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QFileSystemWatcher;

namespace util::config
{
    // Config - the util class that mirrors and updates config.json
    class Config : public QObject
    {
        Q_OBJECT

        public:
            static Config& instance();

            // System paths
            QString wine_binary() const;
            QString winetricks_binary() const;
            QString rosetta_x87_path() const; // macOS Rosetta

            // Wine settings
            QString wine_prefix() const;
            QString wine_arch() const;          // "win64" or "win32"
            QString game_install_path() const;  // <prefix>/drive_c/.../game
            bool    use_dxvk() const;           // whether to install/use DXVK
            QString wine_args() const;          // extra wine env or args

            // Launcher settings
            bool    launch_on_startup() const;
            QString after_game_start() const;   // "keep" / "close" / "minimize"
            QString launcher_size() const;      // "1400x846" or other presets

            // Advanced game arguments
            QString game_id() const;
            QString game_args() const;

            // Auth (stored in .env, not config.json)
            QString username() const;
            QString token() const;
            // true if both username and token are set
            bool    has_auth() const;

            // Setters: write through to disk + emit changed()
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

            // Auth setters write to .env and emit changed()
            void set_auth(const QString& username, const QString& token);
            void clear_auth();

            // Paths on disk (for logging / "open config" buttons)
            QString file_path() const;
            QString env_path() const;

            // Force a re-read from disk (normally automatic via the watcher)
            void reload();

        signals:
            // Emitted whenever config changes
            void changed();

        private:
            explicit Config(QObject* parent = nullptr);
            Config(const Config&) = delete;
            Config & operator=(const Config&) = delete;

            void load();
            void save();
            void load_env();
            void save_env();
            void apply_defaults();
            void probe_system_paths();
            QString derive_game_path(const QString& prefix) const;

            // The in-memory mirror of config.json + the auth pair from .env
            class Impl;
            Impl* d {};

            QFileSystemWatcher* watcher {};
            bool writing {};   // guard: ignore our own writes in the watcher
    };
}