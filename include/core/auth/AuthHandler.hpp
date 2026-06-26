#pragma once

#include <QObject>
#include <QString>

namespace core::wine { class Shell; }

class AuthHandler : public QObject
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