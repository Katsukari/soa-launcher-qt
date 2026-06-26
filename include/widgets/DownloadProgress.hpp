#pragma once

#include "util/ModalOverlay.hpp"
#include <QString>

#include "core/network/soa_bridge.h"

class QTimer;
class QPushButton;

class DownloadProgress : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
        explicit DownloadProgress(QWidget* parent = nullptr);
        ~DownloadProgress() override;

    signals:
        void closed();
        void download_finished(bool ok);

    protected:
        void paint_content(QPainter& painter) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        void setup_buttons();
        void start_download();
        void draw_fill(QPainter& painter, const QRect& bar) const;
        void update_pause_icon();

        static QString human_size(qulonglong bytes);
        static QString human_speed(qulonglong bytes_per_sec);
        static QString human_eta(qulonglong remaining, qulonglong throughput);

        soa_downloader* downloader {};

        QString status      { "Preparing download..." };
        int     percent     {0};
        qulonglong received {0};
        qulonglong total    {0};
        qulonglong speed    {0};
        int     file_index  {0};
        int     file_count  {0};
        bool    done        {};
        bool    failed      {};
        bool    paused      {};

        QPushButton* close_button {};
        QPushButton* pause_button {};
        QPushButton* log_button {};
};