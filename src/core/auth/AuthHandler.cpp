#include "core/auth/AuthHandler.hpp"

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>

#include "core/wine/Shell.hpp"
#include "util/Config.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

using util::config::Config;

namespace
{
    const char* k_discord_oauth_url = "https://discord.com/oauth2/authorize"
        "?client_id=1272602862043795586"
        "&response_type=code"
        "&redirect_uri=https%3A%2F%2Fauthentication.storyofalicia.com%2F"
        "&scope=identify";
}

AuthHandler::AuthHandler(core::wine::Shell* shell_, QObject* parent)
    : QObject(parent), shell(shell_)
{
    QDesktopServices::setUrlHandler("soa", this, "handle_url");
}

void AuthHandler::open_login()
{
    SPDLOG_INFO("opening Discord login in browser");
    QDesktopServices::openUrl(QUrl(k_discord_oauth_url));
}

void AuthHandler::handle_url(const QString& url)
{
    SPDLOG_INFO("received soa url: {}", url.toStdString());

    const QUrl parsed(url);
    const QUrlQuery query(parsed);

    const QString user     = query.queryItemValue("user");
    const QString token    = query.queryItemValue("token");
    const QString username = query.queryItemValue("username");

    if (user.isEmpty() || token.isEmpty())
    {
        SPDLOG_ERROR("soa url missing user or token: {}", url.toStdString());
        return;
    }

    Config::instance().set_auth(user, token, username);
    SPDLOG_INFO("auth ok: user={} username={}", user.toStdString(), username.toStdString());

    emit authenticated(user, token, username);
}