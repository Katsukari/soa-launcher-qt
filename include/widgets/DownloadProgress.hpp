#pragma once

#include "util/ModalOverlay.hpp"
#include <QString>

#include "core/network/Courier.h"
#include "core/network/DownloadStatus.hpp"

class QPushButton;

class DownloadProgress : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT

public:
    enum class Mode
    {
        Download,
        Repair
    };

    explicit DownloadProgress(QWidget* parent = nullptr);
    explicit DownloadProgress(Mode mode, QWidget* parent = nullptr);
    ~DownloadProgress() override;

signals:
    void closed();
    void download_started();
    void download_finished(bool ok);

protected:
    void paint_content(QPainter& painter) override;
    void showEvent(QShowEvent* event) override;

private:
    void setup_buttons();
    void start_download();
    void cancel_download();
    void set_terminal_error(const QString& message);

    static QString human_size(qulonglong bytes);
    static QString human_speed(qulonglong bytes_per_sec);
    static QString human_eta(qulonglong remaining, qulonglong throughput);

    Mode mode {Mode::Download};
    courier* downloader {};
    qulonglong active_operation_id {};
    core::network::DownloadStatus current;

    QPushButton* close_button {};
    QPushButton* retry_button {};
};
