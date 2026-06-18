#pragma once
#include <QWidget>
#include <QPushButton>
#include "MainWindow.hpp"

class GameInstall : public QWidget
{
    Q_OBJECT

public:
        explicit GameInstall(QWidget* parent = nullptr);
        void show_over(QWidget* background);

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject * obj, QEvent* event) override;

        signals:
            void closed();

    private:
        void setup_close_button();
        void setup_buttons();

        QPushButton* close_button {};
        QPushButton* install_button {};
        QPushButton* cancel_button {};
        QPushButton * change_path_button {};
        QPixmap blurred_bg;
        bool open_state{false}; // For potential expand/collapse if needed later, currently just close
};
