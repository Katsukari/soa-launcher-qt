#pragma once
#include <QWidget>

#include "util/Assets.hpp"
#include "util/ModalOverlay.hpp"

class QPushButton;
class QStackedWidget;
class LauncherSettings;
class WineSettings;
class AdvancedSettings;

namespace core::wine
{
    class Shell;
}

class Settings : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
        explicit Settings(core::wine::Shell* shell, QWidget* parent = nullptr);

    signals:
        void repair_requested();
        void launcher_size_changed();

    protected:
        void paint_content(QPainter& painter) override;

    private:
        void setup_close_button();
        void setup_pages();
        void setup_tabs();
        void set_tab(int index);
        void update_panel_geometry();
        int active_tab {};
        bool launcher_panel_expanded {};

        QPushButton* close_button {};
        QPushButton* tab_buttons[3] {};
        QStackedWidget* stack {};
        LauncherSettings* launcher_settings {};
        core::wine::Shell * shell {};
};
