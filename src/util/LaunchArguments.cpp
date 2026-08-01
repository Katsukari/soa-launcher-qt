#include "util/LaunchArguments.hpp"

#include <QProcess>
#include <QRegularExpression>

namespace util::launch_arguments
{
    namespace
    {
        bool is_reserved(const QString& argument)
        {
            const QString normalized = argument.trimmed();
            for (const QString& name : {
                     QStringLiteral("-OP"),
                     QStringLiteral("-ID"),
                     QStringLiteral("-GameID")})
            {
                if (normalized.compare(name, Qt::CaseInsensitive) == 0
                    || normalized.startsWith(name + QLatin1Char('='), Qt::CaseInsensitive))
                {
                    return true;
                }
            }
            return false;
        }
    }

    ValidationResult validate(const QString& raw)
    {
        ValidationResult result;
        if (raw.size() > 4096)
        {
            result.error = QStringLiteral("Game launch arguments are too long.");
            return result;
        }

        result.arguments = QProcess::splitCommand(raw);
        if (result.arguments.size() > 64)
        {
            result.error = QStringLiteral("Too many game launch arguments were provided.");
            return result;
        }

        static const QRegularExpression controlCharacters(QStringLiteral("[\\x00-\\x1F\\x7F]"));
        for (const QString& argument : result.arguments)
        {
            if (argument.size() > 1024 || controlCharacters.match(argument).hasMatch())
            {
                result.error = QStringLiteral("A game launch argument is invalid or too long.");
                return result;
            }
            if (is_reserved(argument))
            {
                result.error = QStringLiteral(
                    "-OP, -ID, and -GameID are managed by the launcher and cannot be overridden.");
                return result;
            }
        }

        result.valid = true;
        return result;
    }
}
