#pragma once
#include <QWidget>
#include "util/Assets.hpp"

class QPushButton;
class QStackedWidget;
class LauncherSettings;
class WineSettings;

class Settings : public QWidget
{
    Q_OBJECT
    public:
        explicit Settings(QWidget* parent = nullptr);
        void show_over(QWidget* background);

    protected:
        void paintEvent(QPaintEvent* event) override;

    signals:
        void closed();

    private:
        void setup_close_button();
        void setup_pages();
        void setup_tabs();
        void set_tab(int index);
        int  active_tab {};

        QPixmap blurred_bg;
        QPushButton * close_button {};
        QStackedWidget * stack {};
        LauncherSettings * launcher_page {};
        WineSettings * wine_page {};
};