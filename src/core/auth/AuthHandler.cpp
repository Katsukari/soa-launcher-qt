#include "core/auth/AuthHandler.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

#include "core/wine/Shell.hpp"
#include "util/Config.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

using util::config::Config;

namespace
{
    const char* k_discord_oauth_url =
        "https://discord.com/oauth2/authorize"
        "?client_id=1272602862043795586"
        "&response_type=code"
        "&redirect_uri=https%3A%2F%2Fauthentication.storyofalicia.com%2F"
        "&scope=identify";

    // https://discord.com/oauth2/authorize?client_id=1272602862043795586&response_type=code&redirect_uri=https%3A%2F%2Fauthentication.storyofalicia.com%2F&scope=identify

#ifdef Q_OS_LINUX
    QString desktop_exec_value(const QString& executable)
    {
        QString escaped = executable;
        escaped.replace("\\", "\\\\");
        escaped.replace("\"", "\\\"");
        return QStringLiteral("\"") + escaped + QStringLiteral("\" %u");
    }

    void register_linux_url_handler()
    {
        const QString applications_dir =
            QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        if (applications_dir.isEmpty())
        {
            SPDLOG_WARN("could not locate the user applications directory");
            return;
        }

        QDir dir;
        if (!dir.mkpath(applications_dir))
        {
            SPDLOG_WARN("could not create user applications directory: {}",
                        applications_dir.toStdString());
            return;
        }

        const QString desktop_path = applications_dir + QStringLiteral("/soa-launcher.desktop");
        QFile desktop_file(desktop_path);
        if (!desktop_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            SPDLOG_WARN("could not write soa URL handler desktop file: {}",
                        desktop_file.errorString().toStdString());
            return;
        }

        QString executable = qEnvironmentVariable("APPIMAGE");
        if (executable.isEmpty())
            executable = QCoreApplication::applicationFilePath();

        QTextStream stream(&desktop_file);
        stream << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=Story Of Alicia\n"
               << "Comment=Story Of Alicia game launcher\n"
               << "Exec=" << desktop_exec_value(executable) << "\n"
               << "Icon=soa-launcher\n"
               << "Categories=Game;\n"
               << "Terminal=false\n"
               << "NoDisplay=true\n"
               << "MimeType=x-scheme-handler/soa;\n";
        desktop_file.close();

        const int exit_code = QProcess::execute(
            QStringLiteral("xdg-mime"),
            {QStringLiteral("default"), QStringLiteral("soa-launcher.desktop"),
             QStringLiteral("x-scheme-handler/soa")});

        if (exit_code == 0)
            SPDLOG_INFO("registered soa URL handler for this user");
        else
            SPDLOG_WARN("could not register soa URL handler with xdg-mime, exit code {}", exit_code);
    }
#endif
}

AuthHandler::AuthHandler(core::wine::Shell* shell_, QObject* parent)
    : core::status::StatusReporter("auth", parent), shell(shell_)
{
    QDesktopServices::setUrlHandler("soa", this, "handle_url");

#ifdef Q_OS_LINUX
    register_linux_url_handler();
#endif
}

void AuthHandler::open_login()
{
    SPDLOG_INFO("opening Discord login in browser");
    working("Waiting for Discord login...");
    QDesktopServices::openUrl(QUrl(k_discord_oauth_url));
}

void AuthHandler::handle_url(const QString& url)
{
    SPDLOG_INFO("received soa login callback");

    const QUrl parsed(url);
    const QUrlQuery query(parsed);

    const QString user     = query.queryItemValue("user");
    const QString token    = query.queryItemValue("token");
    const QString username = query.queryItemValue("username");

    if (user.isEmpty() || token.isEmpty())
    {
        SPDLOG_ERROR("soa login callback missing user or token");
        fail("Login response was missing user or token.");
        return;
    }

    Config::instance().set_auth(user, token, username);
    SPDLOG_INFO("auth ok: user={} username={}", user.toStdString(), username.toStdString());

    done();
    emit authenticated(user, token, username);
}