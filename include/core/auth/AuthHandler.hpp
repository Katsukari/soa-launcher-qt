#pragma once

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
    [[nodiscard]] bool login_pending() const { return pending; }

public slots:
    void handle_url(const QString& url);

signals:
    void authenticated(const QString& user, const QString& token, const QString& username);
    void login_cancelled();

private:
    bool callback_is_expected(const QUrl& url) const;

    core::wine::Shell* shell {};
    QTimer* timeout_timer {};
    bool pending {};
    QDateTime pending_since;
};
