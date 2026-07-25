#pragma once

#include <QEvent>
#include <QWidget>

class QLineEdit;
class QPushButton;

class AdvancedSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedSettings(QWidget* parent = nullptr);
    bool eventFilter(QObject* object, QEvent* event) override;

signals:
    void repair_requested();

private:
    void setup_game_args_option();
    void setup_game_path_option();
    void setup_macos_compatibility_option();
    void setup_macos_deep_diagnostics_option();
    void setup_repair_option();

    QLineEdit* game_path_field {};
    QPushButton* repair_button {};
};
