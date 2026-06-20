#pragma once

#include <QObject>
#include <QUrl>

namespace core::wine { class Shell; }

// Handles the Discord auth flow
class AuthHandler : public QObject
{
    Q_OBJECT

    public:
        explicit AuthHandler(core::wine::Shell* shell, QObject* parent = nullptr);

        // Open the Discord OAuth login page in the user's browser.
        void open_login();

    public slots:
        // Parse a soa:// callback URL and launch the game
        void handle_url(const QUrl& url) const;

    private:
        core::wine::Shell* shell {};   // borrowed
};