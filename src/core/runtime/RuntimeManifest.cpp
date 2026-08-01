#include "core/runtime/RuntimeManifest.hpp"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <utility>

namespace core::runtime
{
    namespace
    {
        bool fail(QString* error, const QString& message)
        {
            if (error)
                *error = message;
            return false;
        }

        bool read_required_string(const QJsonObject& object,
                                  const QString& key,
                                  QString& value,
                                  QString* error)
        {
            const QJsonValue raw = object.value(key);
            if (!raw.isString())
                return fail(error, QStringLiteral("Runtime manifest field '%1' must be a string.").arg(key));
            value = raw.toString().trimmed();
            if (value.isEmpty())
                return fail(error, QStringLiteral("Runtime manifest field '%1' cannot be empty.").arg(key));
            return true;
        }

        bool read_required_positive_int(const QJsonObject& object,
                                        const QString& key,
                                        int& value,
                                        QString* error)
        {
            const QJsonValue raw = object.value(key);
            if (!raw.isDouble())
                return fail(error, QStringLiteral("Runtime manifest field '%1' must be an integer.").arg(key));
            value = raw.toInt(-1);
            if (value < 1 || double(value) != raw.toDouble())
                return fail(error, QStringLiteral("Runtime manifest field '%1' must be a positive integer.").arg(key));
            return true;
        }
    }

    bool is_safe_runtime_component(const QString& value)
    {
        static const QRegularExpression pattern(
            QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$)"));
        return pattern.match(value).hasMatch();
    }

    bool is_safe_relative_runtime_path(const QString& value)
    {
        if (value.isEmpty() || QDir::isAbsolutePath(value) || value.contains(QLatin1Char('\\')))
            return false;

        const QString clean = QDir::cleanPath(value);
        if (clean != value || clean == QStringLiteral(".")
            || clean == QStringLiteral("..") || clean.startsWith(QStringLiteral("../")))
        {
            return false;
        }

        const QStringList components = clean.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        for (const QString& component : components)
        {
            if (component.isEmpty() || component == QStringLiteral(".")
                || component == QStringLiteral(".."))
            {
                return false;
            }
        }
        return true;
    }

    bool RuntimeManifest::valid() const
    {
        return schema_version == 1
            && launcher_contract >= 1
            && prefix_schema >= 1
            && is_safe_runtime_component(runtime_id)
            && is_safe_runtime_component(build_id)
            && !display_name.isEmpty()
            && !runtime_version.isEmpty()
            && !channel.isEmpty()
            && !platform.isEmpty()
            && !host_arch.isEmpty()
            && !wine_version.isEmpty()
            && is_safe_relative_runtime_path(entrypoints.wine);
    }

    QString RuntimeManifest::identity() const
    {
        return QStringLiteral("%1/%2").arg(runtime_id, build_id);
    }

    bool RuntimeManifest::parse(const QByteArray& json,
                                RuntimeManifest& result,
                                QString* error)
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            return fail(error, QStringLiteral("Runtime manifest JSON is invalid: %1")
                                   .arg(parseError.errorString()));
        }
        return parse(document.object(), result, error);
    }

    bool RuntimeManifest::parse(const QJsonObject& object,
                                RuntimeManifest& result,
                                QString* error)
    {
        RuntimeManifest parsed;
        if (!read_required_positive_int(object, QStringLiteral("schema_version"),
                                        parsed.schema_version, error)
            || !read_required_positive_int(object, QStringLiteral("launcher_contract"),
                                           parsed.launcher_contract, error)
            || !read_required_positive_int(object, QStringLiteral("prefix_schema"),
                                           parsed.prefix_schema, error)
            || !read_required_string(object, QStringLiteral("runtime_id"),
                                     parsed.runtime_id, error)
            || !read_required_string(object, QStringLiteral("display_name"),
                                     parsed.display_name, error)
            || !read_required_string(object, QStringLiteral("runtime_version"),
                                     parsed.runtime_version, error)
            || !read_required_string(object, QStringLiteral("build_id"),
                                     parsed.build_id, error)
            || !read_required_string(object, QStringLiteral("channel"),
                                     parsed.channel, error)
            || !read_required_string(object, QStringLiteral("platform"),
                                     parsed.platform, error)
            || !read_required_string(object, QStringLiteral("host_arch"),
                                     parsed.host_arch, error)
            || !read_required_string(object, QStringLiteral("wine_version"),
                                     parsed.wine_version, error))
        {
            return false;
        }

        parsed.wine_commit = object.value(QStringLiteral("wine_commit")).toString().trimmed();
        parsed.requires_rosetta_on_arm64 = object
            .value(QStringLiteral("requires_rosetta_on_arm64")).toBool(false);

        const QJsonValue graphicsValue = object.value(QStringLiteral("graphics_backends"));
        if (!graphicsValue.isArray())
            return fail(error, QStringLiteral("Runtime manifest field 'graphics_backends' must be an array."));
        for (const QJsonValue& value : graphicsValue.toArray())
        {
            if (!value.isString() || value.toString().trimmed().isEmpty())
                return fail(error, QStringLiteral("Runtime graphics backend names must be non-empty strings."));
            parsed.graphics_backends.append(value.toString().trimmed());
        }

        const QJsonValue entrypointsValue = object.value(QStringLiteral("entrypoints"));
        if (!entrypointsValue.isObject())
            return fail(error, QStringLiteral("Runtime manifest field 'entrypoints' must be an object."));
        const QJsonObject entrypoints = entrypointsValue.toObject();
        if (!read_required_string(entrypoints, QStringLiteral("wine"),
                                  parsed.entrypoints.wine, error))
        {
            return false;
        }
        parsed.entrypoints.wineserver = entrypoints.value(QStringLiteral("wineserver")).toString().trimmed();
        parsed.entrypoints.wineboot = entrypoints.value(QStringLiteral("wineboot")).toString().trimmed();
        parsed.entrypoints.self_test = entrypoints.value(QStringLiteral("self_test")).toString().trimmed();

        if (parsed.schema_version != 1)
            return fail(error, QStringLiteral("Unsupported runtime manifest schema version %1.")
                                   .arg(parsed.schema_version));
        if (parsed.launcher_contract > k_launcher_runtime_contract)
        {
            return fail(error, QStringLiteral("Runtime requires launcher contract %1, but this launcher supports %2.")
                                   .arg(parsed.launcher_contract)
                                   .arg(k_launcher_runtime_contract));
        }
        if (!is_safe_runtime_component(parsed.runtime_id)
            || !is_safe_runtime_component(parsed.build_id))
        {
            return fail(error, QStringLiteral("Runtime ID and build ID may contain only letters, numbers, '.', '_' and '-'."));
        }

        for (const auto& [label, path] : {
                 std::pair<QString, QString>{QStringLiteral("wine"), parsed.entrypoints.wine},
                 {QStringLiteral("wineserver"), parsed.entrypoints.wineserver},
                 {QStringLiteral("wineboot"), parsed.entrypoints.wineboot},
                 {QStringLiteral("self_test"), parsed.entrypoints.self_test}})
        {
            if (!path.isEmpty() && !is_safe_relative_runtime_path(path))
            {
                return fail(error, QStringLiteral("Runtime entrypoint '%1' is not a safe relative path.")
                                       .arg(label));
            }
        }

        result = std::move(parsed);
        if (error)
            error->clear();
        return true;
    }
}
