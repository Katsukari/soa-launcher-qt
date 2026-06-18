#pragma once
#include <QWidget>

class QPlainTextEdit;
class WineTerminal;

class WineSettings : public QWidget
{
    Q_OBJECT
public:
    explicit WineSettings(QWidget* parent = nullptr);

private:
    void setup_dxvk_option();
    void setup_prefix_option();
    void setup_wine_binary_option();
    void setup_wine_args_option();
    void setup_game_args_option();
    void setup_log_button();

    WineTerminal* log_window {};
};