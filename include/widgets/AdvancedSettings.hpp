#pragma once
#include <QWidget>

class AdvancedSettings : public QWidget
{
    Q_OBJECT
    public:
        explicit AdvancedSettings(QWidget* parent = nullptr);

    private:
        void setup_game_args_option();
};