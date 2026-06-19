#pragma once

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include "util/Assets.hpp"
#include "Settings.hpp"


class DownloadBox : public QWidget
{
    Q_OBJECT

    public:
        explicit DownloadBox(QWidget * parent = nullptr);
        void paintEvent(QPaintEvent * event) override;
        bool eventFilter(QObject * obj, QEvent* event) override;

    signals:
        void settings_requested();
        void download_triggered();

    private:
        void setup_title();
        void setup_settings_button();
        void setup_message();
        void setup_download_button();

        QPushButton * download_button{};
};