#include "core/auth/AuthHandler.hpp"
#include "util/LanguageManager.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWidget>

#include "core/Log.hpp"
#include "core/wine/Shell.hpp"
#include "util/Config.hpp"
#include "util/DesktopEntry.hpp"
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

    QString new_login_state()
    {
        QString state;
        state.reserve(64);
        for (int i = 0; i < 4; ++i)
            state += QStringLiteral("%1").arg(QRandomGenerator::system()->generate64(), 16, 16, QLatin1Char('0'));
        return state;
    }
    constexpr int k_login_timeout_ms = 10 * 60 * 1000;
    constexpr int k_duplicate_callback_window_ms = 30000;

#ifdef Q_OS_LINUX
    QString desktop_exec_value(const QString& executable)
    {
        return util::desktop_entry::quoted_exec_argument(executable) + QStringLiteral(" %u");
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

        const QString xdg_mime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
        if (xdg_mime.isEmpty())
        {
            SPDLOG_WARN("xdg-mime is unavailable; soa URL handler was not selected");
            return;
        }

        QProcess process;
        process.start(
            xdg_mime,
            {QStringLiteral("default"), desktop_name, QStringLiteral("x-scheme-handler/soa")});
        if (!process.waitForStarted(3000)
            || !process.waitForFinished(5000)
            || process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0)
        {
            SPDLOG_WARN("xdg-mime failed to register the soa URL handler");
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
        if (!pending || completion_scheduled)
            return;
        reset_pending_login();
        fail(QStringLiteral("Discord login timed out. Try again."));
    });

#ifdef Q_OS_LINUX
    register_linux_url_handler();
#endif
}

void AuthHandler::open_login()
{
    if (pending || awaiting_legacy_confirmation || completion_scheduled)
        return;
    QUrl login_url(QString::fromLatin1(k_discord_oauth_url));
    const QString login_state = new_login_state();
    QUrlQuery login_query(login_url);
    login_query.addQueryItem(QStringLiteral("state"), login_state);
    login_url.setQuery(login_query);

    QWidget* parent_widget = qobject_cast<QWidget*>(parent());
    QMessageBox choice(
        QMessageBox::Question,
        util::i18n::translate("Discord Login"),
        util::i18n::translate(
            "Choose how to open the Discord sign-in page.\n\n"
            "Copy Login Link lets you paste the exact sign-in link into any browser."),
        QMessageBox::NoButton,
        parent_widget);
    QPushButton* copy_button = choice.addButton(
        util::i18n::translate("Copy Login Link"), QMessageBox::AcceptRole);
    QPushButton* open_button = choice.addButton(
        util::i18n::translate("Open Default Browser"), QMessageBox::ActionRole);
    QPushButton* cancel_button = choice.addButton(QMessageBox::Cancel);
    choice.setDefaultButton(copy_button);
    choice.exec();

    if (choice.clickedButton() == cancel_button)
        return;

    pending = true;
    pending_since = QDateTime::currentDateTimeUtc();
    pending_state = login_state;
    last_callback_digest.clear();
    last_callback_seen = {};
    timeout_timer->start(k_login_timeout_ms);
    working(QStringLiteral("discord-login"), -1.0, true);

    const QString encoded_url = login_url.toString(QUrl::FullyEncoded);
    if (choice.clickedButton() == copy_button)
    {
        if (QGuiApplication::clipboard())
        {
            QGuiApplication::clipboard()->setText(encoded_url);
            SPDLOG_INFO("copied Discord login link to clipboard");
            QMessageBox::information(
                parent_widget,
                util::i18n::translate("Discord Login"),
                util::i18n::translate("The sign-in link was copied. Paste it into the browser you want to use."));
            return;
        }

        reset_pending_login();
        fail(QStringLiteral("The Discord login link could not be copied."));
        return;
    }

    SPDLOG_INFO("opening Discord login in the default browser");
    if (choice.clickedButton() == open_button && !QDesktopServices::openUrl(login_url))
    {
        if (QGuiApplication::clipboard())
        {
            QGuiApplication::clipboard()->setText(encoded_url);
            QMessageBox::warning(
                parent_widget,
                util::i18n::translate("Browser Could Not Be Opened"),
                util::i18n::translate(
                    "The default browser could not be opened. The exact sign-in link was copied "
                    "to your clipboard instead."));
            return;
        }

        reset_pending_login();
        fail(QStringLiteral("The browser could not be opened and the login link could not be copied."));
    }
}

void AuthHandler::cancel_login()
{
    if (!pending && !awaiting_legacy_confirmation && !completion_scheduled
        && status().state != core::status::State::Working)
    {
        return;
    }

    reset_pending_login();
    completion_scheduled = false;
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
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QByteArray digest = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256);
    const bool duplicate = !last_callback_digest.isEmpty()
        && digest == last_callback_digest
        && last_callback_seen.isValid()
        && last_callback_seen.msecsTo(now) <= k_duplicate_callback_window_ms;
    last_callback_digest = digest;
    last_callback_seen = now;

    if (duplicate)
        return;

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
    if (pending_since.msecsTo(now) > k_login_timeout_ms)
    {
        SPDLOG_WARN("ignored expired soa login callback");
        reset_pending_login();
        fail(QStringLiteral("This login response has expired. Start sign-in again."));
        return;
    }

    const QUrlQuery query(parsed);
    QString returned_state = query.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded).trimmed();
    if (returned_state.isEmpty())
        returned_state = query.queryItemValue(QStringLiteral("oauth_state"), QUrl::FullyDecoded).trimmed();
    if (returned_state.isEmpty())
        returned_state = query.queryItemValue(QStringLiteral("launcher_state"), QUrl::FullyDecoded).trimmed();

    const QString user = query.queryItemValue(QStringLiteral("user"), QUrl::FullyDecoded).trimmed();
    const QString token = query.queryItemValue(QStringLiteral("token"), QUrl::FullyDecoded).trimmed();
    const QString username = query.queryItemValue(QStringLiteral("username"), QUrl::FullyDecoded).trimmed();

    const auto containsUnsafeCredentialCharacter = [](const QString& value)
    {
        for (const QChar character : value)
        {
            if (character.isNull() || character.category() == QChar::Other_Control
                || character == QLatin1Char('\r') || character == QLatin1Char('\n'))
                return true;
        }
        return false;
    };

    if (user.isEmpty() || token.isEmpty() || user.size() > 256 || token.size() > 8192
        || username.size() > 256 || containsUnsafeCredentialCharacter(user)
        || containsUnsafeCredentialCharacter(token) || containsUnsafeCredentialCharacter(username))
    {
        SPDLOG_ERROR("soa login callback contained missing or oversized credentials");
        reset_pending_login();
        fail(QStringLiteral("The login response was malformed."));
        return;
    }

    if (pending_state.isEmpty())
    {
        SPDLOG_WARN("ignored soa login callback because no login state is pending");
        return;
    }

    if (completion_scheduled || awaiting_legacy_confirmation)
        return;

    if (returned_state.isEmpty())
    {
        begin_legacy_confirmation(user, token, username);
        return;
    }

    if (returned_state != pending_state)
    {
        SPDLOG_WARN("ignored soa login callback with mismatched state");
        return;
    }

    schedule_login_completion(user, token, username, false);
}

void AuthHandler::begin_legacy_confirmation(const QString& user, const QString& token,
                                            const QString& username)
{
    if (!pending || awaiting_legacy_confirmation || completion_scheduled)
        return;

    awaiting_legacy_confirmation = true;
    QWidget* parent_widget = qobject_cast<QWidget*>(parent());
    const QString display_name = username.isEmpty() ? user : username;
    auto* confirmation = new QMessageBox(
        QMessageBox::Warning,
        util::i18n::translate("Unverified Login Response"),
        util::i18n::translate(
            "The authentication service did not return the security state generated by the "
            "launcher. This is a compatibility fallback and is less secure.\n\n"
            "Only continue if the browser has just completed the login you started and this is the expected account.\n\n"
            "Continue signing in as %1?").arg(display_name.toHtmlEscaped()),
        QMessageBox::Yes | QMessageBox::Cancel,
        parent_widget);
    confirmation->setDefaultButton(QMessageBox::Cancel);
    confirmation->setAttribute(Qt::WA_DeleteOnClose);

    connect(confirmation, &QMessageBox::finished, this,
            [this, user, token, username](const int result)
    {
        awaiting_legacy_confirmation = false;
        if (!pending || completion_scheduled)
            return;

        if (result != QMessageBox::Yes)
        {
            reset_pending_login();
            fail(QStringLiteral(
                "Discord login was cancelled because the response could not be verified."));
            return;
        }

        SPDLOG_WARN("accepted a legacy soa login callback without state after user confirmation");
        schedule_login_completion(user, token, username, true);
    });

    confirmation->open();
}

void AuthHandler::schedule_login_completion(const QString& user, const QString& token,
                                            const QString& username,
                                            const bool legacy_callback)
{
    if (!pending || completion_scheduled)
        return;

    completion_scheduled = true;
    pending = false;
    awaiting_legacy_confirmation = false;
    pending_since = {};
    pending_state.clear();
    timeout_timer->stop();

    QTimer::singleShot(0, this, [this, user, token, username, legacy_callback]()
    {
        if (!completion_scheduled)
            return;

        QPointer<AuthHandler> self(this);
        Config::instance().set_auth(user, token, username);
        if (!self)
            return;

        SPDLOG_INFO("auth ok for Discord user {}", user.toStdString());
        if (legacy_callback)
            SPDLOG_DEBUG("completed Discord login through legacy callback compatibility mode");

        self->completion_scheduled = false;
        self->done(QStringLiteral("Logged in successfully."));
        if (self)
            emit self->authenticated(user, token, username);
    });
}

void AuthHandler::reset_pending_login()
{
    pending = false;
    awaiting_legacy_confirmation = false;
    pending_since = {};
    pending_state.clear();
    timeout_timer->stop();
}
