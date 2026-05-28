#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include "util/Assets.hpp"


class DownloadWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit DownloadWidget(QWidget* parent = nullptr);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        void setup_title();
        void setup_settings_button();
        void setup_message();
        void setup_download_button();

        QLabel* title_label = nullptr;
        QPushButton* settings_button = nullptr;
        QLabel* message_label = nullptr;
        QPushButton* download_button = nullptr;
};