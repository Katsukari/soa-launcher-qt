#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include "core/status/StatusReporter.hpp"

class QTimer;
class QUrl;
namespace core::wine { class Shell; }

class AuthHandler : public core::status::StatusReporter
{
    Q_OBJECT

    public:
        explicit AuthHandler(core::wine::Shell* shell, QObject* parent = nullptr);

        void open_login();
        void cancel_login();

    public slots:
        void handle_url(const QString& url);

    signals:
        void authenticated(const QString& user, const QString& token, const QString& username);
        void login_cancelled();

    private:
        bool callback_is_expected(const QUrl& url) const;
        void begin_legacy_confirmation(const QString& user, const QString& token,
                                       const QString& username);
        void schedule_login_completion(const QString& user, const QString& token,
                                       const QString& username, bool legacy_callback);
        void reset_pending_login();

        core::wine::Shell* shell {};
        QTimer* timeout_timer {};
        bool pending {};
        bool awaiting_legacy_confirmation {};
        bool completion_scheduled {};
        QDateTime pending_since;
        QString pending_state;
        QByteArray last_callback_digest;
        QDateTime last_callback_seen;
};
