#include <QApplication>
#include <QUrl>
#include "MainWindow.hpp"
#include <spdlog/spdlog.h>
#include "util/Assets.hpp"
#include "core/Log.hpp"
#include "widgets/LauncherLog.hpp"
#include "core/network/Courier.h"
#include "core/auth/AuthHandler.hpp"

// TODO list:
// - add descriptive comments as most design is raw code (especially Layout.hpp)
// - In the end do recheck of every widget and make sure it's aligned as possible to the webview2 launcher
// - FIX COMMENTS IN EVERY FILE
// - Make swift handle the download logging and status

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    core::log::init();
    LauncherLog::instance();
    SPDLOG_INFO("Running Story Of Alicia for Linux and macOS");
    SPDLOG_INFO("Version: 0.1.0");
    util::assets::load_all();
    SPDLOG_DEBUG("loaded all assets successfully!");
    soa_ping();
    MainWindow window;
    window.show();

    // If launched via a soa:// auth callback (OS routes the URL to us as an
    // argument), hand it to the auth handler to parse + launch the game
    for (int i = 1; i < argc; ++i)
    {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a.startsWith("soa://"))
        {
            window.auth_handler()->handle_url(QString(a));
            break;
        }
    }

    return QApplication::exec();
}