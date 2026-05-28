#include <QApplication>
#include "MainWindow.hpp"
#include <spdlog/spdlog.h>
#include "util/Assets.hpp"

int main(int argc, char *argv[])
{
    spdlog::info("Running Story Of Alicia for Linux and macOS");
    spdlog::info("Version: 0.1.0");
    spdlog::info("Build date: {}", __DATE__);
    spdlog::info("Build time: {}", __TIME__);

    QApplication app(argc, argv);

    assets::load_all();

    MainWindow window;
    window.show();
    return QApplication::exec();
}
