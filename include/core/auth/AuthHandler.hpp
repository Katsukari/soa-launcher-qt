#pragma once

#include <QString>

#include "core/status/StatusReporter.hpp"

namespace core::wine { class Shell; }

class AuthHandler : public core::status::StatusReporter
{
    Q_OBJECT

    public:
        explicit AuthHandler(core::wine::Shell* shell, QObject* parent = nullptr);

        void open_login();

    public slots:
        void handle_url(const QString& url);

        signals:
            void authenticated(const QString& user, const QString& token, const QString& username);

    private:
        core::wine::Shell* shell {};
};