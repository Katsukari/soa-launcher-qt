#include "MainWindow.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , stack(new QStackedWidget(this))
    , login(new LoginWidget)
{

    stack->addWidget(login);
    setCentralWidget(stack);

    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(1400, 846);
}
