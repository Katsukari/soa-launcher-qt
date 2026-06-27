#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include "MainWindow.hpp"
#include <spdlog/spdlog.h>
#include "util/Assets.hpp"
#include "core/Log.hpp"
#include "widgets/LauncherLog.hpp"
#include "core/network/Courier.h"
#include "core/auth/AuthHandler.hpp"

namespace
{
    const char* k_instance_key = "soa_launcher";

    QString soa_url_from_args(const QStringList& args)
    {
        for (const QString& a : args)
            if (a.startsWith("soa://"))
                return a;
        return {};
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QString url = soa_url_from_args(app.arguments());

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

    QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window]()
    {
        QLocalSocket* client = server.nextPendingConnection();
        if (!client) return;

        QObject::connect(client, &QLocalSocket::readyRead, &window, [client, &window]()
        {
            const QString incoming = QString::fromUtf8(client->readAll());
            if (incoming.startsWith("soa://"))
                window.auth_handler()->handle_url(incoming);

            window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            window.show();
            window.raise();
            window.activateWindow();

            client->deleteLater();
        });
    });

    if (!url.isEmpty())
        window.auth_handler()->handle_url(url);

    return QApplication::exec();
}