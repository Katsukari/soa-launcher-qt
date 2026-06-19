#pragma once
#include <QPushButton>
#include "util/ModalOverlay.hpp"

class GameInstall : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
        explicit GameInstall(QWidget* parent = nullptr);

    protected:
        void paint_content(QPainter& painter) override;
        bool eventFilter(QObject* obj, QEvent* event) override;

    private:
        void setup_close_button();
        void setup_buttons();

        QString game_path {"/home/<user>/soa-launcher/drive_c/Users/<user>/AppData/Roaming/Story Of Alicia/game"};
        QPushButton* close_button {};
        QPushButton* install_button {};
        QPushButton* cancel_button {};
        QPushButton* change_path_button {};
};