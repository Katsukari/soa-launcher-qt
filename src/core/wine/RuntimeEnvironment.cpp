#include "core/wine/RuntimeLocator.hpp"

#include <QProcess>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

namespace core::wine
{
    void RuntimeLocator::apply_wine_environment_entries(QProcessEnvironment& environment,
                                                        const QString& entries)
    {
        apply_runtime_environment_entries(environment, QProcess::splitCommand(entries));
    }

    void RuntimeLocator::apply_runtime_environment_entries(QProcessEnvironment& environment,
                                                           const QStringList& entries)
    {
        static const QRegularExpression key_pattern(QStringLiteral(R"(^[A-Za-z_][A-Za-z0-9_]*$)"));
        for (const QString& token : entries)
        {
            const int equals = token.indexOf(QLatin1Char('='));
            if (equals <= 0)
            {
                SPDLOG_WARN("ignoring runtime environment entry "
                            "(expected KEY=VALUE): {}",
                            token.toStdString());
                continue;
            }

            const QString key = token.left(equals);
            const QString value = token.mid(equals + 1);
            if (!key_pattern.match(key).hasMatch())
            {
                SPDLOG_WARN("ignoring invalid runtime environment key: {}", key.toStdString());
                continue;
            }

            const QString upper = key.toUpper();
            const bool protected_key =
                upper == QStringLiteral("PATH") || upper == QStringLiteral("HOME") ||
                upper == QStringLiteral("WINE") || upper == QStringLiteral("WINESERVER") ||
                upper == QStringLiteral("WINEPREFIX") || upper == QStringLiteral("WINEARCH") ||
                upper == QStringLiteral("WINEDEBUG") ||
                upper == QStringLiteral("WINEDLLOVERRIDES") ||
                upper == QStringLiteral("LD_PRELOAD") || upper == QStringLiteral("GAMEID") ||
                upper == QStringLiteral("STORE") || upper.startsWith(QStringLiteral("DYLD_")) ||
                upper.startsWith(QStringLiteral("CX_")) ||
                upper.startsWith(QStringLiteral("PROTON_")) ||
                upper.startsWith(QStringLiteral("STEAM_COMPAT_"));
            if (protected_key)
            {
                SPDLOG_WARN("ignoring launcher-owned runtime "
                            "environment key: {}",
                            key.toStdString());
                continue;
            }

            environment.insert(key, value);
            const bool sensitive = key.contains(QStringLiteral("TOKEN"), Qt::CaseInsensitive) ||
                                   key.contains(QStringLiteral("PASSWORD"), Qt::CaseInsensitive) ||
                                   key.contains(QStringLiteral("SECRET"), Qt::CaseInsensitive);
            SPDLOG_DEBUG("runtime env: {}={}", key.toStdString(),
                         sensitive ? "[REDACTED]" : value.toStdString());
        }
    }
}
