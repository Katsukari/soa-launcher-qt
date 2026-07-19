#include <QApplication>
#include <QFileOpenEvent>
#include <utility>
#include <QLocalServer>
#include <QLocalSocket>

#include "MainWindow.hpp"
#include "core/Log.hpp"
#include "core/auth/AuthHandler.hpp"
#include "core/network/Courier.h"
#include "util/Assets.hpp"
#include "widgets/LauncherLog.hpp"

#include <spdlog/spdlog.h>

namespace
{
    const char* k_instance_key = "soa_launcher";

    QString soa_url_from_args(const QStringList& args)
    {
        for (const QString& arg : args)
        {
            if (arg.startsWith("soa://"))
                return arg;
        }

        return {};
    }

    class LauncherApplication final : public QApplication
    {
        Q_OBJECT

        public:
            LauncherApplication(int& argc, char** argv)
                : QApplication(argc, argv)
            {
            }

            QString take_pending_url()
            {
                return std::exchange(pending_url, {});
            }

        signals:
            void soa_url_opened(const QString& url);

        protected:
            bool event(QEvent* event) override
            {
                if (event->type() != QEvent::FileOpen)
                    return QApplication::event(event);

                const auto* open_event = static_cast<QFileOpenEvent*>(event);
                const QString url = open_event->url().toString();
                if (!url.startsWith("soa://"))
                    return QApplication::event(event);

                pending_url = url;
                emit soa_url_opened(url);
                return true;
            }

        private:
            QString pending_url;
    };
}

int main(int argc, char *argv[])
{
    LauncherApplication app(argc, argv);

    QString url = soa_url_from_args(app.arguments());
    if (url.isEmpty())
        url = app.take_pending_url();

    QLocalSocket probe;
    probe.connectToServer(k_instance_key);
    if (probe.waitForConnected(200))
    {
        probe.write(url.toUtf8());
        probe.flush();
        probe.waitForBytesWritten(200);
        probe.disconnectFromServer();
        return 0;
    }

    core::log::init();
    LauncherLog::instance();
    SPDLOG_INFO("Running Story Of Alicia for Linux and macOS");
    SPDLOG_INFO("Version: 0.1.0");
    util::assets::load_all();
    SPDLOG_DEBUG("loaded all assets successfully!");
    soa_ping();

    QLocalServer::removeServer(k_instance_key);
    QLocalServer server;
    if (!server.listen(k_instance_key))
        SPDLOG_WARN("single-instance server failed to listen: {}", server.errorString().toStdString());

    MainWindow window;
    window.show();

    const auto handle_url = [&window](const QString& incoming)
    {
        if (incoming.startsWith("soa://"))
            window.auth_handler()->handle_url(incoming);

        window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        window.show();
        window.raise();
        window.activateWindow();
    };

    QObject::connect(&app, &LauncherApplication::soa_url_opened, &window, handle_url);

    QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, handle_url]()
    {
        QLocalSocket* client = server.nextPendingConnection();
        if (!client)
            return;

        QObject::connect(client, &QLocalSocket::readyRead, client, [client, handle_url]()
        {
            handle_url(QString::fromUtf8(client->readAll()));
            client->deleteLater();
        });
    });

    if (!url.isEmpty())
        handle_url(url);

    return QApplication::exec();
}

#include "main.moc"
