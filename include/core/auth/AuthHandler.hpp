#pragma once

#include <QObject>
#include <QString>

namespace core::wine { class Shell; }

// Handles the Discord auth flow
class AuthHandler : public QObject
{
    Q_OBJECT

public:
    explicit AuthHandler(core::wine::Shell* shell, QObject* parent = nullptr);

    // Open the Discord OAuth login page in the user's browser.
    void open_login();

    // Parse a soa:// callback URL and launch the game
    void handle_url(const QString& url);

private:
    core::wine::Shell* shell {};   // borrowed
};