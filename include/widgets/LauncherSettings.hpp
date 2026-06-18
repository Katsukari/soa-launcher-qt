#pragma once

#include <QWidget>

class LauncherSettings : public QWidget
{
    Q_OBJECT
    public:
        explicit LauncherSettings(QWidget* parent = nullptr);

    private:
        void setup_launch_on_startup_option();
        void setup_after_game_start_option();
        void setup_run_connectivity_test_option();
        void setup_launcher_size_option();
        void setup_github_button();
};
