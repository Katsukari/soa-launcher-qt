#pragma once

#include <QString>
#include <QVector>

namespace core::wine
{
    enum class RuntimeType
    {
        Wine,
        Proton
    };

    struct WineInstall
    {
        QString     name;
        QString     path;
        RuntimeType type { RuntimeType::Wine };
    };

    QString winetricks_path();
    QString umu_path();
    bool    winetricks_available();
    bool    umu_available();

    class WineRegistry
    {
        public:
            static QVector<WineInstall> scan();
            static QVector<QString>& extra_search_dirs();
            static RuntimeType identify(const QString& path, bool* ok = nullptr);
    };
}
