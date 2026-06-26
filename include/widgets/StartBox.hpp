#pragma once

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPainter>
#include "util/Assets.hpp"

class AuthHandler;

namespace core::wine { class Shell; }

class StartBox : public QWidget
{
    Q_OBJECT
    public:
        explicit StartBox(AuthHandler* auth, core::wine::Shell* shell, QWidget* parent = nullptr);
        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject* obj, QEvent* event) override;

    public slots:
        void on_download_complete();

        signals:
            void settings_requested();
        void download_triggered();

    private:
        enum class State { Download, Login, Waiting, SignedIn };

        void setup_title();
        void setup_settings_button();
        void setup_download_state();
        void setup_login_state();
        void setup_waiting_state();
        void setup_signedin_state();

        void set_state(State s);
        void apply_state_visibility();
        void refresh_enter_enabled();

        AuthHandler*       auth  {};
        core::wine::Shell* shell {};

        State state { State::Login };

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