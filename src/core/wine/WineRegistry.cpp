#include "core/wine/WineRegistry.hpp"

#include <QtGlobal>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcessEnvironment>
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
        if (ok) *ok = info.exists();

        if (info.isFile() && info.fileName().compare(QStringLiteral("proton"), Qt::CaseInsensitive) == 0)
            return RuntimeType::Proton;

        if (info.isDir())
        {
            const QDir dir(path);
            if (dir.exists("proton") || dir.exists("proton_dist.tar"))
                return RuntimeType::Proton;
            if (dir.exists("files/bin/wine") || dir.exists("dist/bin/wine") ||
                dir.exists("bin/wine") || dir.exists("Contents/Resources/wine/bin/wine") ||
                dir.exists("Contents/SharedSupport/CrossOver/bin/wine"))
                return RuntimeType::Wine;
            if (ok) *ok = false;
            return RuntimeType::Wine;
        }

        return RuntimeType::Wine;
    }

    namespace
    {
        QString home() { return QDir::homePath(); }

        void add_install(QVector<WineInstall>& out, QSet<QString>& seen,
                         const QString& path, RuntimeType type, const QString& name)
        {
            if (!QFileInfo::exists(path)) return;

            const QString canonical = QFileInfo(path).canonicalFilePath();
            const QString key = canonical.isEmpty() ? path : canonical;
            if (seen.contains(key)) return;
            seen.insert(key);

            WineInstall wi;
            wi.name = name;
            wi.path = path;
            wi.type = type;
            out.push_back(wi);
        }

        QString wine_in(const QString& folder)
        {
            for (const QString& rel : { QStringLiteral("/files/bin/wine"),
                                        QStringLiteral("/dist/bin/wine"),
                                        QStringLiteral("/bin/wine"),
                                        QStringLiteral("/Contents/Resources/wine/bin/wine"),
                                        QStringLiteral("/Contents/SharedSupport/CrossOver/bin/wine") })
            {
                if (QFileInfo::exists(folder + rel)) return folder + rel;
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
                    add_install(out, seen, folder, RuntimeType::Proton, entry.fileName());
                    continue;
                }
                const QString wine = wine_in(folder);
                if (!wine.isEmpty())
                    add_install(out, seen, wine, RuntimeType::Wine, entry.fileName());
            }
        }

        QStringList runtime_search_dirs()
        {
            QStringList dirs;

#if defined(Q_OS_MACOS)
            dirs << "/Applications"
                 << home() + "/Applications"
                 << home() + "/Library/Application Support/com.isaacmarovitz.Whisky/Libraries"
                 << home() + "/Library/Application Support/CrossOver/Bottles"
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
            bins << "/opt/homebrew/bin/wine"
                 << "/opt/homebrew/bin/wine64"
                 << "/usr/local/bin/wine"
                 << "/usr/local/bin/wine64"
                 << "/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine"
                 << "/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine"
                 << "/Applications/Wine Devel.app/Contents/Resources/wine/bin/wine"
                 << "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"
                 << "/Applications/Whisky.app/Contents/Resources/Libraries/Wine/bin/wine"
                 << "/usr/local/opt/game-porting-toolkit/bin/wine64"
                 << "/opt/homebrew/opt/game-porting-toolkit/bin/wine64";
#endif
            return bins;
        }
    }

    QVector<WineInstall> WineRegistry::scan()
    {
        QVector<WineInstall> out;
        QSet<QString> seen;

        const QString system_wine = QStandardPaths::findExecutable("wine");
        if (!system_wine.isEmpty())
            add_install(out, seen, system_wine, RuntimeType::Wine, "System Wine");

        for (const QString& bin : direct_binaries())
            add_install(out, seen, bin, RuntimeType::Wine, QFileInfo(bin).dir().dirName());

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
                return QFileInfo::exists(configured) ? configured : QString {};

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
        return QFileInfo::exists(local) ? local : QString {};
    }

    bool winetricks_available()
    {
        return !winetricks_path().isEmpty();
    }

    bool umu_available()
    {
        return !umu_path().isEmpty();
    }

}
