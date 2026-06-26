#pragma once
#include <QMainWindow>

#include "widgets/WineInstall.hpp"

namespace util::modal_overlay
{
    class ModalOverlay;
}

namespace core::wine
{
    class Shell;
}

class QPushButton;
class QLabel;
class StartBox;
class Settings;
class GameInstall;

class AuthHandler;

class MainWindow : public QWidget
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        [[nodiscard]] AuthHandler * auth_handler() const { return auth; }

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent * event) override;

    private:
        void setup_window_buttons();
        void setup_logo();
        void setup_version_label();
        void setup_settings();
        void setup_start_box();
        void setup_wine_install();
        void setup_game_install();
        void on_overlay_opened(util::modal_overlay::ModalOverlay * m);
        void on_overlay_closed(util::modal_overlay::ModalOverlay *);

        QPoint drag_offset;
        bool chrome_hidden {};
        QPushButton * close_button = nullptr;
        QPushButton * minimize_button = nullptr;
        StartBox * start_box = nullptr;
        Settings * settings = nullptr;
        WineInstall * wine_install = nullptr;
        GameInstall * game_install = nullptr;
        core::wine::Shell * shell {};
        AuthHandler * auth {};
};