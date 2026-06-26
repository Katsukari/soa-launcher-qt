#include "util/Config.hpp"

#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QTimer>
#include <QTextStream>

#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace util::config
{
    class Config::Impl
    {
        public:
            QVariantMap values;
            QString     username;
            QString     token;
            QString     display_name;
    };

    Config& Config::instance()
    {
        static Config inst;
        return inst;
    }

    Config::Config(QObject* parent) : QObject(parent)
    {
        d = new Impl();

        const QString dir = QFileInfo(file_path()).absolutePath();
        QDir().mkpath(dir);

        load();
        load_env();
        apply_defaults();
        save();

        watcher = new QFileSystemWatcher(this);
        watcher->addPath(file_path());
        connect(watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path)
        {
            if (writing) return;

            SPDLOG_INFO("config.json changed externally, reloading");
            QTimer::singleShot(100, this, [this, path]
            {
                load();
                if (!watcher->files().contains(path)) watcher->addPath(path);
                emit changed();
            });
        });
    }

    QString Config::file_path() const
    {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return QDir(base).filePath("config.json");
    }

    QString Config::env_path() const
    {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return QDir(base).filePath(".env");
    }

    void Config::load()
    {
        QFile f(file_path());
        if (!f.open(QIODevice::ReadOnly))
        {
            SPDLOG_DEBUG("config: no existing file at {}, will create", file_path().toStdString());
            return;
        }

        const QByteArray raw = f.readAll();
        f.close();

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
        {
            SPDLOG_ERROR("config: failed to parse config.json: {}", err.errorString().toStdString());
            return;
        }

        d->values = doc.object().toVariantMap();
        SPDLOG_DEBUG("config: loaded {} keys", d->values.size());
    }

    void Config::save()
    {
        const QJsonObject obj = QJsonObject::fromVariantMap(d->values);
        const QJsonDocument doc(obj);

        QFile f(file_path());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            SPDLOG_ERROR("config: cannot write {}", file_path().toStdString());
            return;
        }

        writing = true;
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();

        QTimer::singleShot(150, this, [this]() { writing = false; });
    }

    void Config::load_env()
    {
        QFile f(env_path());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            SPDLOG_DEBUG("config: no .env, user not logged in yet");
            return;
        }

        while (!f.atEnd())
        {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;

            const int eq = line.indexOf('=');
            if (eq < 0) continue;

            const QString key = line.left(eq).trimmed();
            QString       val = line.mid(eq + 1).trimmed();

            if (val.size() >= 2 && val.startsWith('"') && val.endsWith('"')) val = val.mid(1, val.size() - 2);

            if (key == "SOA_USER") d->username = val;
            else if (key == "SOA_TOKEN") d->token = val;
            else if (key == "SOA_USERNAME") d->display_name = val;
        }
        f.close();

        SPDLOG_DEBUG("config: loaded auth from .env (user {})", d->username.isEmpty() ? "<none>" : "set");
    }

    void Config::save_env()
    {
        QFile f(env_path());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        {
            SPDLOG_ERROR("config: cannot write .env at {}", env_path().toStdString());
            return;
        }

        QTextStream out(&f);
        out << "# Story Of Alicia launcher - auth credentials\n";
        out << "# This file contains your login token. Do NOT share it.\n";
        out << "SOA_USER=" << d->username << "\n";
        out << "SOA_TOKEN=" << d->token << "\n";
        out << "SOA_USERNAME=" << d->display_name << "\n";
        f.close();

        // Owner-only permissions so others can't read the token.
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        SPDLOG_DEBUG("config: wrote .env (auth)");
    }

    QString Config::derive_game_path(const QString& prefix) const
    {
        const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
        return QDir(prefix).filePath(QString("drive_c/users/%1/AppData/Roaming/Story Of Alicia/game").arg(user));
    }

    void Config::probe_system_paths()
    {
        auto probe_into = [this](const QString& key, const char* exe)
        {
            if (d->values.value(key).toString().isEmpty())
            {
                const QString found = QStandardPaths::findExecutable(exe);
                if (!found.isEmpty())
                {
                    d->values[key] = found;
                    SPDLOG_DEBUG("config: probed {} at {}", exe, found.toStdString());
                }
            }
        };

        probe_into("wine_binary",       "wine");
        probe_into("winetricks_binary", "winetricks");
    }

    void Config::apply_defaults()
    {
        probe_system_paths();

        auto set_if_missing = [this](const QString& key, const QVariant& val)
        {
            if (!d->values.contains(key)) d->values[key] = val;
        };

        const QString default_prefix = QDir(QDir::homePath()).filePath("soa-launcher");

        set_if_missing("wine_prefix",      default_prefix);
        set_if_missing("wine_arch",        "win64");
        set_if_missing("use_dxvk",         false);
        set_if_missing("wine_args",        "");
        set_if_missing("rosetta_x87_path", "");

        set_if_missing("launch_on_startup", false);
        set_if_missing("after_game_start",  "keep");
        set_if_missing("launcher_size",     "1400x846");

        set_if_missing("game_id",   "4");
        set_if_missing("game_args", "");

        set_if_missing("game_install_path", derive_game_path(default_prefix));
    }

    void Config::reload()
    {
        load();
        load_env();
        emit changed();
    }

    QString Config::wine_binary() const       { return d->values.value("wine_binary").toString(); }
    QString Config::winetricks_binary() const { return d->values.value("winetricks_binary").toString(); }
    QString Config::rosetta_x87_path() const  { return d->values.value("rosetta_x87_path").toString(); }
    QString Config::wine_arch() const         { const QString v = d->values.value("wine_arch").toString(); return v.isEmpty() ? "win64" : v; }
    bool    Config::use_dxvk() const          { return d->values.value("use_dxvk").toBool(); }
    QString Config::wine_args() const         { return d->values.value("wine_args").toString(); }

    bool    Config::launch_on_startup() const { return d->values.value("launch_on_startup").toBool(); }
    QString Config::after_game_start() const  { const QString v = d->values.value("after_game_start").toString(); return v.isEmpty() ? "keep" : v; }
    QString Config::launcher_size() const     { const QString v = d->values.value("launcher_size").toString(); return v.isEmpty() ? "1400x846" : v; }

    QString Config::game_id() const           { const QString v = d->values.value("game_id").toString(); return v.isEmpty() ? "4" : v; }
    QString Config::game_args() const         { return d->values.value("game_args").toString(); }

    QString Config::wine_prefix() const
    {
        const QString v = d->values.value("wine_prefix").toString();
        return v.isEmpty() ? QDir(QDir::homePath()).filePath("soa-launcher") : v;
    }

    QString Config::game_install_path() const
    {
        const QString v = d->values.value("game_install_path").toString();
        return v.isEmpty() ? derive_game_path(wine_prefix()) : v;
    }

    QString Config::username() const          { return d->username; }
    QString Config::token() const             { return d->token; }
    QString Config::display_name() const      { return d->display_name; }
    bool    Config::has_auth() const          { return !d->username.isEmpty() && !d->token.isEmpty(); }

    void Config::set_wine_binary(const QString& v)       { d->values["wine_binary"] = v;       save(); emit changed(); }
    void Config::set_winetricks_binary(const QString& v) { d->values["winetricks_binary"] = v; save(); emit changed(); }
    void Config::set_rosetta_x87_path(const QString& v)  { d->values["rosetta_x87_path"] = v;  save(); emit changed(); }
    void Config::set_wine_arch(const QString& v)         { d->values["wine_arch"] = v;         save(); emit changed(); }
    void Config::set_use_dxvk(bool v)                    { d->values["use_dxvk"] = v;          save(); emit changed(); }
    void Config::set_wine_args(const QString& v)         { d->values["wine_args"] = v;         save(); emit changed(); }

    void Config::set_launch_on_startup(bool v)           { d->values["launch_on_startup"] = v; save(); emit changed(); }
    void Config::set_after_game_start(const QString& v)  { d->values["after_game_start"] = v;  save(); emit changed(); }
    void Config::set_launcher_size(const QString& v)     { d->values["launcher_size"] = v;     save(); emit changed(); }

    void Config::set_game_id(const QString& v)           { d->values["game_id"] = v;           save(); emit changed(); }
    void Config::set_game_args(const QString& v)         { d->values["game_args"] = v;         save(); emit changed(); }

    void Config::set_wine_prefix(const QString& v)
    {
        d->values["wine_prefix"] = v;
        save();
        emit changed();
    }

    void Config::set_game_install_path(const QString& v)
    {
        d->values["game_install_path"] = v;
        save();
        emit changed();
    }

    bool Config::game_installed() const
    {
        const QDir dir(game_install_path());
        if (!dir.exists()) return false;
        return !dir.isEmpty(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    }

    void Config::set_auth(const QString& username, const QString& token, const QString& display_name)
    {
        d->username     = username;
        d->token        = token;
        d->display_name = display_name;
        save_env();
        emit changed();
    }

    void Config::clear_auth()
    {
        d->username.clear();
        d->token.clear();
        d->display_name.clear();
        save_env();
        emit changed();
    }
}