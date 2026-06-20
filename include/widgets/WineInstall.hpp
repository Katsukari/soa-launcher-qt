#pragma once
#include <QPushButton>
#include "util/ModalOverlay.hpp"
#include "core/network/soa_bridge.h"

namespace core::wine
{
    class Shell;
}

class WineInstall : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
        explicit WineInstall(core::wine::Shell* shell, QWidget* parent = nullptr);
        ~WineInstall();

    protected:
        void paint_content(QPainter& painter) override;
        bool eventFilter(QObject* obj, QEvent* event) override;

    private:
        void setup_close_button();
        void setup_buttons();

        QString game_path {};
        QPushButton* close_button {};
        QPushButton* install_button {};
        QPushButton* cancel_button {};
        QPushButton* change_path_button {};
        QPushButton* login_button {};
        core::wine::Shell *shell{};
        soa_downloader* downloader {};
        bool            installing {};
        void start_install();
        void start_download();
};