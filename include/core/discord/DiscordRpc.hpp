#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class QLocalSocket;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace core::discord
{
    class DiscordRpc final : public QObject
    {
    public:
        explicit DiscordRpc(QObject* parent = nullptr);
        ~DiscordRpc() override;

        void set_launcher_presence();
        void set_game_presence(const QString& username);
        void shutdown();

    private:
        struct Presence
        {
            QString details;
            QString state;
            qint64 started_at {};
            QString large_image_key;
            QString large_image_text;
            QString small_image_key;
            QString small_image_text;
        };

        enum class Mode
        {
            Launcher,
            Game
        };

        void queue_presence(const Presence& presence);
        QJsonObject make_activity(const Presence& presence);
        void flush_presence();
        bool send_activity(const QJsonValue& activity);
        bool send_packet(quint32 opcode, const QByteArray& payload);

        void connect_to_discord();
        void try_next_socket();
        void schedule_next_socket();
        void schedule_reconnect();
        QStringList socket_candidates() const;
        void handle_socket_connected();
        void handle_socket_disconnected();
        void handle_socket_error();
        void handle_socket_data();
        void handle_rpc_frame(quint32 opcode, const QByteArray& payload);

        void start_proxy_polling();
        void stop_proxy_polling();
        void poll_proxy();
        Presence parse_proxy_presence(const QJsonObject& payload, bool& found) const;
        Presence default_game_presence() const;
        QString proxy_base_url() const;

        void log_rpc_warning(const QString& message);
        void log_proxy_warning(const QString& message);

        QLocalSocket* socket {};
        QNetworkAccessManager* network {};
        QTimer* flush_timer {};
        QTimer* reconnect_timer {};
        QTimer* connection_timer {};
        QTimer* proxy_poll_timer {};
        QPointer<QNetworkReply> proxy_reply;

        Mode mode {Mode::Launcher};
        QString proxy_username;
        QStringList candidates;
        qsizetype candidate_index {};
        QByteArray socket_buffer;
        QJsonObject desired_activity;
        QJsonObject pending_activity;
        QByteArray desired_serialized;
        QByteArray pending_serialized;
        QByteArray last_sent_serialized;
        QByteArray current_activity_key;
        qint64 current_activity_started_at {};
        qint64 last_sent_at_ms {};
        qint64 last_rpc_log_at_ms {};
        qint64 last_proxy_log_at_ms {};
        quint64 mode_generation {};
        bool ready {};
        bool has_desired {};
        bool has_pending {};
        bool next_socket_scheduled {};
        bool shutting_down {};
    };
}
