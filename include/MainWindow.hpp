#pragma once
#include <QMainWindow>
#include "widgets/WineInstall.hpp"

#include "core/state/Stage.hpp"
#include "core/state/View.hpp"

namespace util::modal_overlay
{
    class ModalOverlay;
}
namespace core::wine
{
    class Shell;
}
namespace core::state
{
    class InstallState;
}
class QPushButton;
class QLabel;
class Playtest;
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
        void setup_playtest();
        void setup_wine_install();
        void setup_game_install();
        void on_overlay_opened(util::modal_overlay::ModalOverlay * m);
        void on_overlay_closed(util::modal_overlay::ModalOverlay *);
        void on_stage_changed(core::state::Stage s);
        void open_for_current_stage();
        QPoint drag_offset;
        bool chrome_hidden {};
        QPushButton * close_button = nullptr;
        QPushButton * minimize_button = nullptr;
        Playtest * playtest = nullptr;
        Settings * settings = nullptr;
        WineInstall * wine_install = nullptr;
        GameInstall * game_install = nullptr;
        core::wine::Shell * shell {};
        AuthHandler * auth {};
        core::state::InstallState * install_state {};
        core::state::View last_view { core::state::View::Loading };
};