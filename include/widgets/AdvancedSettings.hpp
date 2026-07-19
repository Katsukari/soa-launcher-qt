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
        QLineEdit* game_path_field {};
};
