#pragma once

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPainter>
#include "util/Assets.hpp"
#include "core/state/Stage.hpp"

class AuthHandler;
namespace core::wine { class Shell; }
namespace core::state { class InstallState; }

class Playtest : public QWidget
{
    Q_OBJECT
    public:
        explicit Playtest(AuthHandler* auth, core::wine::Shell* shell,
                          core::state::InstallState* install_state, QWidget* parent = nullptr);
        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject* obj, QEvent* event) override;
        signals:
            void settings_requested();
        void download_triggered();
    private:
        enum class State { Download, Login, Waiting, SignedIn };
        static State state_for(core::state::Stage s);
        void on_stage_changed(core::state::Stage s);
        void setup_title();
        void setup_settings_button();
        void setup_download_state();
        void setup_login_state();
        void setup_waiting_state();
        void setup_signedin_state();
        void set_state(State s);
        void apply_state_visibility();
        void refresh_enter_enabled();
        AuthHandler*               auth          {};
        core::wine::Shell*         shell         {};
        core::state::InstallState* install_state {};
        State state { State::Download };
        QLabel*      title_label {};
        QPushButton* settings_button {};
        QPushButton* download_button {};
        QLabel*      message_label {};
        QPushButton* discord_button {};
        QLabel*      disclaimer_label {};
        QLabel*      waiting_title {};
        QLabel*      steps_label {};
        QCheckBox*   check_bugs {};
        QCheckBox*   check_rules {};
        QLabel*      signed_in_label {};
        QPushButton* enter_button {};
        QPushButton* reset_path_button {};
};