#include "core/discord/DiscordRpc.hpp"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include <spdlog/spdlog.h>

namespace core::discord
{
    namespace
    {
        constexpr auto k_application_id = "1431526738881548369";
        constexpr int k_proxy_poll_interval_ms = 5'000;
        constexpr int k_min_update_interval_ms = 10'000;
        constexpr int k_retry_after_error_ms = 2'000;
        constexpr int k_log_throttle_ms = 60'000;
        constexpr int k_connect_timeout_ms = 400;
        constexpr quint32 k_max_frame_size = 1024 * 1024;

        QString non_empty(const QJsonValue& value)
        {
            if (!value.isString())
                return {};
            return value.toString().trimmed();
        }

        QString bounded(QString value, const qsizetype limit)
        {
            value = value.trimmed();
            if (value.size() > limit)
                value.truncate(limit);
            return value;
        }
    }

    DiscordRpc::DiscordRpc(QObject* parent)
        : QObject(parent)
    {
        socket = new QLocalSocket(this);
        network = new QNetworkAccessManager(this);
        flush_timer = new QTimer(this);
        reconnect_timer = new QTimer(this);
        connection_timer = new QTimer(this);
        proxy_poll_timer = new QTimer(this);

        flush_timer->setSingleShot(true);
        reconnect_timer->setSingleShot(true);
        connection_timer->setSingleShot(true);
        proxy_poll_timer->setInterval(k_proxy_poll_interval_ms);

        connect(flush_timer, &QTimer::timeout, this, [this]() { flush_presence(); });
        connect(reconnect_timer, &QTimer::timeout, this, [this]() { connect_to_discord(); });
        connect(connection_timer, &QTimer::timeout, this, [this]()
        {
            if (socket->state() != QLocalSocket::ConnectedState)
            {
                socket->abort();
                schedule_next_socket();
            }
        });
        connect(proxy_poll_timer, &QTimer::timeout, this, [this]() { poll_proxy(); });
        connect(socket, &QLocalSocket::connected, this, [this]() { handle_socket_connected(); });
        connect(socket, &QLocalSocket::disconnected, this, [this]() { handle_socket_disconnected(); });
        connect(socket, &QLocalSocket::readyRead, this, [this]() { handle_socket_data(); });
        connect(socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError)
        {
            handle_socket_error();
        });
    }

    DiscordRpc::~DiscordRpc()
    {
        shutdown();
    }

    void DiscordRpc::set_launcher_presence()
    {
        if (shutting_down)
            return;

        mode = Mode::Launcher;
        ++mode_generation;
        stop_proxy_polling();
        proxy_username.clear();

        Presence presence;
        presence.details = QStringLiteral("In Launcher");
        queue_presence(presence);
    }

    void DiscordRpc::set_game_presence(const QString& username)
    {
        if (shutting_down)
            return;

        mode = Mode::Game;
        ++mode_generation;
        stop_proxy_polling();
        proxy_username = username.trimmed();
        queue_presence(default_game_presence());
        start_proxy_polling();
    }

    void DiscordRpc::shutdown()
    {
        if (shutting_down)
            return;

        shutting_down = true;
        stop_proxy_polling();
        flush_timer->stop();
        reconnect_timer->stop();
        connection_timer->stop();

        if (ready && socket->state() == QLocalSocket::ConnectedState)
        {
            send_activity(QJsonValue(QJsonValue::Null));
            socket->flush();
            socket->waitForBytesWritten(150);
        }

        socket->disconnectFromServer();
        socket->abort();
        ready = false;
        has_desired = false;
        has_pending = false;
    }

    void DiscordRpc::queue_presence(const Presence& presence)
    {
        const QJsonObject activity = make_activity(presence);
        const QByteArray serialized = QJsonDocument(activity).toJson(QJsonDocument::Compact);

        desired_activity = activity;
        desired_serialized = serialized;
        has_desired = true;

        if (serialized == last_sent_serialized || serialized == pending_serialized)
            return;

        pending_activity = activity;
        pending_serialized = serialized;
        has_pending = true;
        flush_presence();
    }

    QJsonObject DiscordRpc::make_activity(const Presence& presence)
    {
        QJsonObject identity;
        identity.insert(QStringLiteral("details"), bounded(presence.details, 128));
        if (!presence.state.trimmed().isEmpty())
            identity.insert(QStringLiteral("state"), bounded(presence.state, 128));
        if (!presence.large_image_key.trimmed().isEmpty())
            identity.insert(QStringLiteral("large_image"), bounded(presence.large_image_key, 256));
        if (!presence.large_image_text.trimmed().isEmpty())
            identity.insert(QStringLiteral("large_text"), bounded(presence.large_image_text, 128));
        if (!presence.small_image_key.trimmed().isEmpty())
            identity.insert(QStringLiteral("small_image"), bounded(presence.small_image_key, 256));
        if (!presence.small_image_text.trimmed().isEmpty())
            identity.insert(QStringLiteral("small_text"), bounded(presence.small_image_text, 128));

        const QByteArray key = QJsonDocument(identity).toJson(QJsonDocument::Compact);
        if (presence.started_at > 0)
        {
            current_activity_key = key;
            current_activity_started_at = presence.started_at;
        }
        else if (current_activity_key != key || current_activity_started_at <= 0)
        {
            current_activity_key = key;
            current_activity_started_at = QDateTime::currentSecsSinceEpoch();
        }

        QJsonObject activity;
        activity.insert(QStringLiteral("type"), 0);
        activity.insert(QStringLiteral("details"), identity.value(QStringLiteral("details")));
        if (identity.contains(QStringLiteral("state")))
            activity.insert(QStringLiteral("state"), identity.value(QStringLiteral("state")));

        QJsonObject timestamps;
        timestamps.insert(QStringLiteral("start"), QJsonValue(current_activity_started_at));
        activity.insert(QStringLiteral("timestamps"), timestamps);

        QJsonObject assets;
        if (identity.contains(QStringLiteral("large_image")))
            assets.insert(QStringLiteral("large_image"), identity.value(QStringLiteral("large_image")));
        if (identity.contains(QStringLiteral("large_text")))
            assets.insert(QStringLiteral("large_text"), identity.value(QStringLiteral("large_text")));
        if (identity.contains(QStringLiteral("small_image")))
            assets.insert(QStringLiteral("small_image"), identity.value(QStringLiteral("small_image")));
        if (identity.contains(QStringLiteral("small_text")))
            assets.insert(QStringLiteral("small_text"), identity.value(QStringLiteral("small_text")));
        if (!assets.isEmpty())
            activity.insert(QStringLiteral("assets"), assets);

        return activity;
    }

    void DiscordRpc::flush_presence()
    {
        if (shutting_down || !has_pending)
            return;

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (last_sent_at_ms > 0 && now - last_sent_at_ms < k_min_update_interval_ms)
        {
            flush_timer->start(static_cast<int>(k_min_update_interval_ms - (now - last_sent_at_ms)));
            return;
        }

        if (!ready || socket->state() != QLocalSocket::ConnectedState)
        {
            connect_to_discord();
            return;
        }

        if (!send_activity(pending_activity))
        {
            schedule_reconnect();
            return;
        }

        last_sent_at_ms = now;
        last_sent_serialized = pending_serialized;
        pending_serialized.clear();
        pending_activity = {};
        has_pending = false;
    }

    bool DiscordRpc::send_activity(const QJsonValue& activity)
    {
        QJsonObject args;
        args.insert(QStringLiteral("pid"), QJsonValue(QCoreApplication::applicationPid()));
        args.insert(QStringLiteral("activity"), activity);

        const QJsonObject payload{
            {QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY")},
            {QStringLiteral("args"), args},
            {QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces)}
        };
        return send_packet(1, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    }

    bool DiscordRpc::send_packet(const quint32 opcode, const QByteArray& payload)
    {
        if (socket->state() != QLocalSocket::ConnectedState || payload.size() > k_max_frame_size)
            return false;

        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << opcode << static_cast<quint32>(payload.size());
        frame.append(payload);
        return socket->write(frame) == frame.size();
    }

    void DiscordRpc::connect_to_discord()
    {
        if (shutting_down || ready || socket->state() != QLocalSocket::UnconnectedState)
            return;

        reconnect_timer->stop();
        candidates = socket_candidates();
        candidate_index = 0;
        try_next_socket();
    }

    void DiscordRpc::try_next_socket()
    {
        if (shutting_down || ready)
            return;

        connection_timer->stop();
        socket->abort();

        if (candidate_index >= candidates.size())
        {
            schedule_reconnect();
            return;
        }

        const QString candidate = candidates.at(candidate_index++);
        socket->connectToServer(candidate, QIODevice::ReadWrite);
        connection_timer->start(k_connect_timeout_ms);
    }

    void DiscordRpc::schedule_next_socket()
    {
        if (shutting_down || ready || next_socket_scheduled)
            return;

        next_socket_scheduled = true;
        QTimer::singleShot(0, this, [this]()
        {
            next_socket_scheduled = false;
            if (!shutting_down && !ready)
                try_next_socket();
        });
    }

    void DiscordRpc::schedule_reconnect()
    {
        if (shutting_down || reconnect_timer->isActive())
            return;
        reconnect_timer->start(k_retry_after_error_ms);
    }

    QStringList DiscordRpc::socket_candidates() const
    {
        QStringList directories;
        for (const char* name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"})
        {
            const QString value = qEnvironmentVariable(name).trimmed();
            if (!value.isEmpty())
                directories.append(value);
        }
        directories.append(QStringLiteral("/tmp"));

        QSet<QString> seen;
        QStringList result;
        for (const QString& directory : directories)
        {
            const QString clean = QDir::cleanPath(directory);
            if (clean.isEmpty() || seen.contains(clean))
                continue;
            seen.insert(clean);

            for (int index = 0; index < 10; ++index)
            {
                const QString path = QDir(clean).filePath(
                    QStringLiteral("discord-ipc-%1").arg(index));
                if (QFileInfo::exists(path))
                    result.append(path);
            }
        }
        return result;
    }

    void DiscordRpc::handle_socket_connected()
    {
        connection_timer->stop();
        socket_buffer.clear();
        ready = false;

        const QJsonObject handshake{
            {QStringLiteral("v"), 1},
            {QStringLiteral("client_id"), QString::fromLatin1(k_application_id)}
        };
        if (!send_packet(0, QJsonDocument(handshake).toJson(QJsonDocument::Compact)))
        {
            socket->abort();
            schedule_next_socket();
        }
    }

    void DiscordRpc::handle_socket_disconnected()
    {
        connection_timer->stop();
        const bool was_ready = ready;
        ready = false;
        socket_buffer.clear();
        last_sent_serialized.clear();
        last_sent_at_ms = 0;

        if (has_desired)
        {
            pending_activity = desired_activity;
            pending_serialized = desired_serialized;
            has_pending = true;
        }

        if (!shutting_down)
        {
            if (was_ready)
                log_rpc_warning(QStringLiteral("Discord RPC disconnected; retrying."));
            schedule_reconnect();
        }
    }

    void DiscordRpc::handle_socket_error()
    {
        if (shutting_down || ready)
            return;
        connection_timer->stop();
        schedule_next_socket();
    }

    void DiscordRpc::handle_socket_data()
    {
        socket_buffer.append(socket->readAll());

        while (socket_buffer.size() >= 8)
        {
            QByteArray header = socket_buffer.left(8);
            QDataStream stream(&header, QIODevice::ReadOnly);
            stream.setByteOrder(QDataStream::LittleEndian);
            quint32 opcode = 0;
            quint32 length = 0;
            stream >> opcode >> length;

            if (length > k_max_frame_size)
            {
                log_rpc_warning(QStringLiteral("Discord RPC sent an oversized frame."));
                socket->abort();
                return;
            }

            const qsizetype frame_size = 8 + static_cast<qsizetype>(length);
            if (socket_buffer.size() < frame_size)
                return;

            const QByteArray payload = socket_buffer.mid(8, static_cast<qsizetype>(length));
            socket_buffer.remove(0, frame_size);
            handle_rpc_frame(opcode, payload);
        }
    }

    void DiscordRpc::handle_rpc_frame(const quint32 opcode, const QByteArray& payload)
    {
        if (opcode == 3)
        {
            send_packet(4, payload);
            return;
        }
        if (opcode == 2)
        {
            socket->disconnectFromServer();
            return;
        }
        if (opcode != 1)
            return;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return;

        const QJsonObject object = document.object();
        const QString event = object.value(QStringLiteral("evt")).toString();
        if (event == QStringLiteral("READY"))
        {
            ready = true;
            reconnect_timer->stop();
            flush_presence();
            return;
        }
        if (event == QStringLiteral("ERROR"))
        {
            const QJsonObject data = object.value(QStringLiteral("data")).toObject();
            log_rpc_warning(QStringLiteral("Discord RPC error %1: %2")
                .arg(data.value(QStringLiteral("code")).toInt())
                .arg(data.value(QStringLiteral("message")).toString()));
        }
    }

    void DiscordRpc::start_proxy_polling()
    {
        if (mode != Mode::Game || proxy_username.isEmpty())
            return;
        poll_proxy();
        proxy_poll_timer->start();
    }

    void DiscordRpc::stop_proxy_polling()
    {
        proxy_poll_timer->stop();
        if (proxy_reply)
        {
            proxy_reply->abort();
            proxy_reply->deleteLater();
            proxy_reply.clear();
        }
    }

    void DiscordRpc::poll_proxy()
    {
        if (shutting_down || mode != Mode::Game || proxy_username.isEmpty() || proxy_reply)
            return;

        QUrl url(proxy_base_url());
        QString path = url.path();
        if (!path.endsWith(QLatin1Char('/')))
            path += QLatin1Char('/');
        path += QStringLiteral("presence");
        url.setPath(path);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("username"), proxy_username);
        query.addQueryItem(QStringLiteral("t"), QString::number(QDateTime::currentMSecsSinceEpoch()));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setRawHeader("Cache-Control", "no-store");

        const quint64 generation = mode_generation;
        QNetworkReply* reply = network->get(request);
        proxy_reply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, generation]()
        {
            if (proxy_reply == reply)
                proxy_reply.clear();

            const bool current = !shutting_down
                && generation == mode_generation
                && mode == Mode::Game;
            if (!current)
            {
                reply->deleteLater();
                return;
            }

            if (reply->error() != QNetworkReply::NoError)
            {
                log_proxy_warning(QStringLiteral("Discord presence proxy request failed: %1")
                    .arg(reply->errorString()));
                queue_presence(default_game_presence());
                reply->deleteLater();
                return;
            }

            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
            {
                log_proxy_warning(QStringLiteral("Discord presence proxy returned invalid JSON."));
                queue_presence(default_game_presence());
                reply->deleteLater();
                return;
            }

            bool found = false;
            const Presence presence = parse_proxy_presence(document.object(), found);
            queue_presence(found ? presence : default_game_presence());
            reply->deleteLater();
        });
    }

    DiscordRpc::Presence DiscordRpc::parse_proxy_presence(const QJsonObject& payload, bool& found) const
    {
        found = payload.value(QStringLiteral("found")).toBool(false);

        Presence presence;
        presence.details = non_empty(payload.value(QStringLiteral("details")));
        if (presence.details.isEmpty())
            presence.details = QStringLiteral("In Game");
        presence.state = non_empty(payload.value(QStringLiteral("state")));

        const QJsonObject timestamps = payload.value(QStringLiteral("timestamps")).toObject();
        const double started = timestamps.value(QStringLiteral("started_at")).toDouble();
        if (started > 0)
            presence.started_at = static_cast<qint64>(started);

        const QJsonObject assets = payload.value(QStringLiteral("assets")).toObject();
        presence.large_image_key = non_empty(assets.value(QStringLiteral("large_image")));
        presence.large_image_text = non_empty(assets.value(QStringLiteral("large_text")));
        presence.small_image_key = non_empty(assets.value(QStringLiteral("small_image")));
        presence.small_image_text = non_empty(assets.value(QStringLiteral("small_text")));
        return presence;
    }

    DiscordRpc::Presence DiscordRpc::default_game_presence() const
    {
        Presence presence;
        presence.details = QStringLiteral("In Game");
        return presence;
    }

    QString DiscordRpc::proxy_base_url() const
    {
        const QString configured = qEnvironmentVariable("SOA_RPC_PROXY_URL").trimmed();
        return configured.isEmpty()
            ? QStringLiteral("http://127.0.0.1:18080")
            : configured;
    }

    void DiscordRpc::log_rpc_warning(const QString& message)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - last_rpc_log_at_ms < k_log_throttle_ms)
            return;
        last_rpc_log_at_ms = now;
        SPDLOG_WARN("discord-rpc: {}", message.toStdString());
    }

    void DiscordRpc::log_proxy_warning(const QString& message)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - last_proxy_log_at_ms < k_log_throttle_ms)
            return;
        last_proxy_log_at_ms = now;
        SPDLOG_WARN("discord-rpc: {}", message.toStdString());
    }
}
