// include/LoginWidget.hpp

#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QPaintEvent>
#include "LoginWidget.hpp"
#include "util/Assets.hpp"
#include "DownloadWidget.hpp"


class DownloadWidget;

class LoginWidget : public QWidget
{
    Q_OBJECT
    public:
        explicit LoginWidget(QWidget* parent = nullptr);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
    void setup_logo();
    void setup_close_buttons();
    void setup_version_label();
    void setup_download_widget();

    QPushButton* close_button = nullptr;
    QPushButton* minimize_button = nullptr;
    QLabel* pt_icon = nullptr;
    QLabel* lock_icon = nullptr;
    QLabel* version_label = nullptr;
    DownloadWidget* download_widget = nullptr;
};