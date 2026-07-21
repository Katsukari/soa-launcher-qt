#include "core/auth/AuthHandler.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include "core/Log.hpp"
#include "core/wine/Shell.hpp"
#include "util/Config.hpp"
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
    constexpr int k_login_timeout_ms = 10 * 60 * 1000;

#ifdef Q_OS_LINUX
    QString desktop_exec_value(const QString& executable)
    {
        QString escaped = executable;
        escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
        escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
        return QStringLiteral("\"") + escaped + QStringLiteral("\" %u");
    }

    QByteArray url_handler_desktop_contents(const QString& executable)
    {
        QString text;
        QTextStream stream(&text);
        stream << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=Story Of Alicia URL Handler\n"
               << "Comment=Handles Story Of Alicia authentication callbacks\n"
               << "Exec=" << desktop_exec_value(executable) << "\n"
               << "Icon=soa-launcher\n"
               << "Terminal=false\n"
               << "NoDisplay=true\n"
               << "MimeType=x-scheme-handler/soa;\n";
        return text.toUtf8();
    }

    void register_linux_url_handler()
    {
        const QString applications_dir =
            QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        if (applications_dir.isEmpty() || !QDir().mkpath(applications_dir))
        {
            SPDLOG_WARN("could not prepare the user applications directory for URL registration");
            return;
        }

        QString executable = qEnvironmentVariable("APPIMAGE");
        if (executable.isEmpty())
            executable = QCoreApplication::applicationFilePath();

        const QString desktop_name = QStringLiteral("soa-launcher-url-handler.desktop");
        const QString desktop_path = QDir(applications_dir).filePath(desktop_name);
        const QByteArray desired = url_handler_desktop_contents(executable);

        QFile existing(desktop_path);
        bool needs_write = true;
        if (existing.open(QIODevice::ReadOnly))
            needs_write = existing.readAll() != desired;

        if (needs_write)
        {
            QSaveFile desktop_file(desktop_path);
            if (!desktop_file.open(QIODevice::WriteOnly | QIODevice::Text)
                || desktop_file.write(desired) != desired.size()
                || !desktop_file.commit())
            {
                SPDLOG_WARN("could not write soa URL handler desktop file: {}",
                            desktop_path.toStdString());
                return;
            }
        }

        if (!needs_write)
            return;

        const QString xdg_mime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
        if (xdg_mime.isEmpty())
        {
            SPDLOG_WARN("xdg-mime is unavailable; soa URL handler was not selected");
            return;
        }


        if (!QProcess::startDetached(
                xdg_mime,
                {QStringLiteral("default"), desktop_name, QStringLiteral("x-scheme-handler/soa")}))
        {
            SPDLOG_WARN("could not start xdg-mime for soa URL registration");
        }
    }
#endif
}

AuthHandler::AuthHandler(core::wine::Shell* shell_, QObject* parent)
    : core::status::StatusReporter(QStringLiteral("auth"), parent), shell(shell_)
{
    QDesktopServices::setUrlHandler(QStringLiteral("soa"), this, "handle_url");
    timeout_timer = new QTimer(this);
    timeout_timer->setSingleShot(true);
    connect(timeout_timer, &QTimer::timeout, this, [this]()
    {
        if (!pending)
            return;
        pending = false;
        fail(QStringLiteral("Discord login timed out. Try again."));
    });

#ifdef Q_OS_LINUX
    register_linux_url_handler();
#endif
}

void AuthHandler::open_login()
{
    SPDLOG_INFO("opening Discord login in browser");
    pending = true;
    pending_since = QDateTime::currentDateTimeUtc();
    timeout_timer->start(k_login_timeout_ms);
    working(QStringLiteral("discord-login"));

    if (!QDesktopServices::openUrl(QUrl(QString::fromLatin1(k_discord_oauth_url))))
    {
        pending = false;
        timeout_timer->stop();
        if (QGuiApplication::clipboard())
            QGuiApplication::clipboard()->setText(QString::fromLatin1(k_discord_oauth_url));
        fail(QStringLiteral(
            "The browser could not be opened. The Discord login link was copied to your clipboard."));
    }
}

void AuthHandler::cancel_login()
{
    if (!pending && status().state != core::status::State::Working)
        return;
    pending = false;
    timeout_timer->stop();
    idle();
    emit login_cancelled();
}

bool AuthHandler::callback_is_expected(const QUrl& url) const
{
    return url.isValid()
        && url.scheme().compare(QStringLiteral("soa"), Qt::CaseInsensitive) == 0
        && url.host().compare(QStringLiteral("launcher"), Qt::CaseInsensitive) == 0
        && (url.path().isEmpty() || url.path() == QStringLiteral("/"));
}

void AuthHandler::handle_url(const QString& url)
{
    SPDLOG_INFO("received soa login callback");
    const QUrl parsed(url, QUrl::StrictMode);

    if (!callback_is_expected(parsed))
    {
        SPDLOG_WARN("ignored unexpected soa callback target");
        return;
    }
    if (!pending || !pending_since.isValid())
    {
        SPDLOG_WARN("ignored unsolicited soa login callback");
        return;
    }
    if (pending_since.msecsTo(QDateTime::currentDateTimeUtc()) > k_login_timeout_ms)
    {
        SPDLOG_WARN("ignored expired soa login callback");
        pending = false;
        timeout_timer->stop();
        fail(QStringLiteral("This login response has expired. Start sign-in again."));
        return;
    }

    const QUrlQuery query(parsed);
    const QString user = query.queryItemValue(QStringLiteral("user"), QUrl::FullyDecoded).trimmed();
    const QString token = query.queryItemValue(QStringLiteral("token"), QUrl::FullyDecoded).trimmed();
    const QString username = query.queryItemValue(QStringLiteral("username"), QUrl::FullyDecoded).trimmed();

    if (user.isEmpty() || token.isEmpty() || user.size() > 256 || token.size() > 8192
        || username.size() > 256)
    {
        SPDLOG_ERROR("soa login callback contained missing or oversized credentials");
        fail(QStringLiteral("The login response was malformed."));
        return;
    }

    pending = false;
    timeout_timer->stop();
    Config::instance().set_auth(user, token, username);
    SPDLOG_INFO("auth ok for Discord user {}", user.toStdString());

    done(QStringLiteral("Logged in successfully."));
    emit authenticated(user, token, username);
}
