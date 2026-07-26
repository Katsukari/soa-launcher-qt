#pragma once

#include <QWidget>

class QLineEdit;

class AdvancedSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedSettings(QWidget* parent = nullptr);

private:
    void setup_game_args_option();
    void setup_game_path_option();
    void setup_umu_runner_option();
    void setup_macos_compatibility_option();
    void setup_macos_deep_diagnostics_option();

    QLineEdit* game_path_field {};
    QLineEdit* umu_path_field {};
};
