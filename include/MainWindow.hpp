#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <spdlog/spdlog.h>
#include "widgets/LoginWidget.hpp"

class LoginWidget;

class MainWindow : public QMainWindow
{
        Q_OBJECT
    public:
        explicit MainWindow(QWidget * parent = nullptr);
        ~MainWindow() override = default;

    private:
        QStackedWidget *stack;
        LoginWidget *login;
};
