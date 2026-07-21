#include "core/wine/WineRegistry.hpp"

#include <QtGlobal>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSet>

#include "core/Log.hpp"
#include "util/Config.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    QVector<QString>& WineRegistry::extra_search_dirs()
    {
        static QVector<QString> dirs;
        return dirs;
    }

    RuntimeType WineRegistry::identify(const QString& path, bool* ok)
    {
        const QFileInfo info(path);
        if (ok) *ok = false;

        if (info.isFile())
        {
            const bool executable = info.isExecutable();
            if (ok) *ok = executable;
            return info.fileName().compare(QStringLiteral("proton"), Qt::CaseInsensitive) == 0
                ? RuntimeType::Proton
                : RuntimeType::Wine;
        }

        if (info.isDir())
        {
            const QDir dir(path);
            const QFileInfo proton(dir.filePath(QStringLiteral("proton")));
            if (proton.isFile())
            {
                if (ok) *ok = proton.isExecutable();
                return RuntimeType::Proton;
            }

            for (const QString& relative : {
                     QStringLiteral("files/bin/wine"),
                     QStringLiteral("files/bin/wine64"),
                     QStringLiteral("dist/bin/wine"),
                     QStringLiteral("dist/bin/wine64"),
                     QStringLiteral("bin/wine"),
                     QStringLiteral("bin/wine64"),
                     QStringLiteral("Contents/Resources/wine/bin/wine"),
                     QStringLiteral("Contents/Resources/wine/bin/wine64"),
                     QStringLiteral("Contents/SharedSupport/CrossOver/bin/wine"),
                     QStringLiteral("Contents/SharedSupport/CrossOver/bin/wine64")})
            {
                const QFileInfo wine(dir.filePath(relative));
                if (wine.isFile())
                {
                    if (ok) *ok = wine.isExecutable();
                    return RuntimeType::Wine;
                }
            }
        }

        return RuntimeType::Wine;
    }

    namespace
    {
        QString home() { return QDir::homePath(); }

        void add_install(QVector<WineInstall>& out, QSet<QString>& seen,
                         const QString& path, RuntimeType type, const QString& name)
        {
            const QFileInfo supplied(path);
            QString executable = path;
            if (type == RuntimeType::Proton && supplied.isDir())
                executable = QDir(path).filePath(QStringLiteral("proton"));

            const QFileInfo executableInfo(executable);
            if (!executableInfo.isFile() || !executableInfo.isExecutable())
                return;

            const QString canonical = supplied.canonicalFilePath();
            const QString key = canonical.isEmpty() ? path : canonical;
            if (seen.contains(key)) return;
            seen.insert(key);

            WineInstall wi;
            wi.name = name;
            wi.path = path;
            wi.type = type;
            out.push_back(wi);
        }

        QString runtime_name_for_path(const QString& path)
        {
            const int app_suffix = path.indexOf(QStringLiteral(".app/"), 0, Qt::CaseInsensitive);
            if (app_suffix >= 0)
                return QFileInfo(path.left(app_suffix + 4)).baseName();
            if (path.contains(QStringLiteral("game-porting-toolkit"), Qt::CaseInsensitive))
                return QStringLiteral("Game Porting Toolkit");
            if (path.startsWith(QStringLiteral("/opt/homebrew/"))
                || path.startsWith(QStringLiteral("/usr/local/")))
            {
                return QStringLiteral("Homebrew Wine");
            }
            const QString name = QFileInfo(path).completeBaseName();
            return name.isEmpty() ? QStringLiteral("Wine") : name;
        }

        QString wine_in(const QString& folder)
        {
            for (const QString& rel : { QStringLiteral("/files/bin/wine"),
                                        QStringLiteral("/files/bin/wine64"),
                                        QStringLiteral("/dist/bin/wine"),
                                        QStringLiteral("/dist/bin/wine64"),
                                        QStringLiteral("/bin/wine"),
                                        QStringLiteral("/bin/wine64"),
                                        QStringLiteral("/Contents/Resources/wine/bin/wine"),
                                        QStringLiteral("/Contents/Resources/wine/bin/wine64"),
                                        QStringLiteral("/Contents/SharedSupport/CrossOver/bin/wine"),
                                        QStringLiteral("/Contents/SharedSupport/CrossOver/bin/wine64") })
            {
                const QFileInfo candidate(folder + rel);
                if (candidate.isFile() && candidate.isExecutable()) return candidate.absoluteFilePath();
            }
            return {};
        }

        void scan_runtime_dir(QVector<WineInstall>& out, QSet<QString>& seen, const QString& dir)
        {
            QDir d(dir);
            if (!d.exists()) return;

            for (const QFileInfo& entry : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
            {
                const QString folder = entry.absoluteFilePath();
                if (QFileInfo::exists(folder + "/proton"))
                {
                    add_install(out, seen, folder, RuntimeType::Proton, entry.completeBaseName());
                    continue;
                }
                const QString wine = wine_in(folder);
                if (!wine.isEmpty())
                    add_install(out, seen, wine, RuntimeType::Wine, entry.completeBaseName());
            }
        }

        QStringList runtime_search_dirs()
        {
            QStringList dirs;

#if defined(Q_OS_MACOS)
            dirs << "/Applications"
                 << home() + "/Applications"
                 << home() + "/Library/Application Support/com.isaacmarovitz.Whisky/Libraries"
                 << "/opt/homebrew/Caskroom"
                 << "/usr/local/Caskroom";
#else
            const QStringList steam_roots
            {
                home() + "/.steam/steam",
                home() + "/.steam/root",
                home() + "/.local/share/Steam",
                home() + "/.var/app/com.valvesoftware.Steam/data/Steam"
            };
            for (const QString& root : steam_roots)
            {
                dirs << root + "/steamapps/common";
                dirs << root + "/compatibilitytools.d";
            }
            dirs << home() + "/.local/share/lutris/runners/wine";
            dirs << "/opt" << "/usr/lib" << "/usr/local";
#endif
            return dirs;
        }

        QStringList direct_binaries()
        {
            QStringList bins;
#if defined(Q_OS_MACOS)
            const QStringList application_roots {QStringLiteral("/Applications"),
                                                  home() + QStringLiteral("/Applications")};
            bins << "/opt/homebrew/bin/wine"
                 << "/opt/homebrew/bin/wine64"
                 << "/usr/local/bin/wine"
                 << "/usr/local/bin/wine64"
                 << "/usr/local/opt/game-porting-toolkit/bin/wine64"
                 << "/opt/homebrew/opt/game-porting-toolkit/bin/wine64";
            for (const QString& root : application_roots)
            {
                bins << root + "/Wine Stable.app/Contents/Resources/wine/bin/wine"
                     << root + "/Wine Stable.app/Contents/Resources/wine/bin/wine64"
                     << root + "/Wine Staging.app/Contents/Resources/wine/bin/wine"
                     << root + "/Wine Staging.app/Contents/Resources/wine/bin/wine64"
                     << root + "/Wine Devel.app/Contents/Resources/wine/bin/wine"
                     << root + "/Wine Devel.app/Contents/Resources/wine/bin/wine64"
                     << root + "/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"
                     << root + "/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine64"
                     << root + "/Whisky.app/Contents/Resources/Libraries/Wine/bin/wine"
                     << root + "/Whisky.app/Contents/Resources/Libraries/Wine/bin/wine64";
            }
#endif
            return bins;
        }
    }

    QVector<WineInstall> WineRegistry::scan()
    {
        QVector<WineInstall> out;
        QSet<QString> seen;

        QString system_wine = QStandardPaths::findExecutable(QStringLiteral("wine"));
        if (system_wine.isEmpty())
            system_wine = QStandardPaths::findExecutable(QStringLiteral("wine64"));
        if (!system_wine.isEmpty())
            add_install(out, seen, system_wine, RuntimeType::Wine, QStringLiteral("System Wine"));

        for (const QString& bin : direct_binaries())
            add_install(out, seen, bin, RuntimeType::Wine, runtime_name_for_path(bin));

        for (const QString& dir : runtime_search_dirs())
        {
#if defined(Q_OS_MACOS)
            scan_runtime_dir(out, seen, dir);
#else
            if (dir == "/opt" || dir.startsWith("/usr"))
            {
                QDir d(dir);
                if (!d.exists()) continue;
                for (const QFileInfo& entry : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    const QString name = entry.fileName();
                    if (!name.contains("wine", Qt::CaseInsensitive) &&
                        !name.contains("proton", Qt::CaseInsensitive))
                        continue;

                    if (const QString folder = entry.absoluteFilePath(); QFileInfo::exists(folder + "/proton"))
                        add_install(out, seen, folder, RuntimeType::Proton, name);
                    else
                    {
                        const QString wine = wine_in(folder);
                        if (!wine.isEmpty())
                            add_install(out, seen, wine, RuntimeType::Wine, name);
                    }
                }
            }
            else
            {
                scan_runtime_dir(out, seen, dir);
            }
#endif
        }

        for (const QString& dir : extra_search_dirs())
            scan_runtime_dir(out, seen, dir);

        SPDLOG_INFO("wine scan: found {} runtime(s)", out.size());
        for (const WineInstall& wi : out)
            SPDLOG_DEBUG("  [{}] {} -> {}",
                         wi.type == RuntimeType::Proton ? "proton" : "wine",
                         wi.name.toStdString(), wi.path.toStdString());

        return out;
    }
    QString winetricks_path()
    {
        const QString configured = util::config::Config::instance().winetricks_binary();
        if (!configured.isEmpty())
        {
            if (QFileInfo(configured).isAbsolute())
                return QFileInfo(configured).isExecutable() ? configured : QString {};

            const QString found = QStandardPaths::findExecutable(configured);
            if (!found.isEmpty()) return found;
        }

        return QStandardPaths::findExecutable("winetricks");
    }

    QString umu_path()
    {
        QString found = QStandardPaths::findExecutable("umu-run");
        if (!found.isEmpty()) return found;

        const QString local = QDir::home().filePath(".local/bin/umu-run");
        return QFileInfo(local).isExecutable() ? local : QString {};
    }

    bool winetricks_available()
    {
        const QString path = winetricks_path();
        return !path.isEmpty() && QFileInfo(path).isExecutable();
    }

    bool umu_available()
    {
        const QString path = umu_path();
        return !path.isEmpty() && QFileInfo(path).isExecutable();
    }

}
