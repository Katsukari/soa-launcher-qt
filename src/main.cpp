#include <QApplication>
#include "MainWindow.hpp"
#include <spdlog/spdlog.h>
#include "util/Assets.hpp"
#include "core/Log.hpp"
#include "widgets/LauncherLog.hpp"

// TODO list:
// - add shell class
// - add descriptive comments as most design is raw code (especially Layout.hpp)
// - add the ability to make the wine prefix for people (no packages, and they have to install wine first)
// - add settings so the user can customise wine path, which wine, env variables, etc
// - In the end do recheck of every widget and make sure it's aligned as possible to the webview2 launcher

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    core::log::init();
    LauncherLog::instance();

    SPDLOG_INFO("Running Story Of Alicia for Linux and macOS");
    SPDLOG_INFO("Version: 0.1.0");

    util::assets::load_all();

    SPDLOG_DEBUG("loaded all assets successfully!");

    MainWindow window;
    window.show();
    return QApplication::exec();
}
