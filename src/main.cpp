#include <QApplication>
#include "MainWindow.hpp"
#include <spdlog/spdlog.h>
#include "util/Assets.hpp"

// TODO list:
// - add shell class
// - add descriptive comments as most design is raw code (especially Layout.hpp)
// - add the ability to make the wine prefix for people (no packages, and they have to install wine first)
// - add settings so the user can customise wine path, which wine, env variables, etc
// - In the end do recheck of every widget and make sure it's aligned as possible to the webview2 launcher

int main(int argc, char *argv[])
{
    spdlog::info("Running Story Of Alicia for Linux and macOS");
    spdlog::info("Version: 0.1.0");

    QApplication app(argc, argv);

    util::assets::load_all();

    MainWindow window;
    window.show();
    return QApplication::exec();
}
