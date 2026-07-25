#pragma once

#include <QString>
#include <QStringList>

namespace util::launch_arguments
{
    struct ValidationResult
    {
        bool valid {};
        QString error;
        QStringList arguments;
    };

    ValidationResult validate(const QString& raw);
}
