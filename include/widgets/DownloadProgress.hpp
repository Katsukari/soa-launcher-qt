#pragma once

#include "util/ModalOverlay.hpp"
#include <QString>

#include "core/network/Courier.h"
#include "core/network/DownloadStatus.hpp"

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

        static QString human_size(qulonglong bytes);
        static QString human_speed(qulonglong bytes_per_sec);
        static QString human_eta(qulonglong remaining, qulonglong throughput);

        courier* downloader {};

        core::network::DownloadStatus current;

        QPushButton* close_button {};
        QPushButton* log_button {};
};