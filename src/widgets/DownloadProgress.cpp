#include "widgets/DownloadProgress.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Colors.hpp"
#include "util/Config.hpp"
#include "core/game/GameVersion.hpp"
#include "util/ProgressBar.hpp"
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>

#include "core/network/CourierBridge.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace dl = util::layout::progress_modal;
using util::config::Config;
using core::network::CourierBridge;
using core::network::DownloadStatus;
using core::status::State;

DownloadProgress::DownloadProgress(QWidget* parent)
    : DownloadProgress(Mode::Download, parent)
{
}

DownloadProgress::DownloadProgress(const Mode mode_, QWidget* parent)
    : ModalOverlay(parent), mode(mode_)
{
    setup_buttons();

    connect(&CourierBridge::instance(), &CourierBridge::download_status, this,
        [this](const DownloadStatus& status)
        {
            if (status.operation_id != active_operation_id)
                return;

            const bool finished = status.base.state == State::Done || status.base.state == State::Failed;
            current = status;

            const char* operation = mode == Mode::Repair ? "repair" : "download";
            if (status.base.state == State::Done)
                SPDLOG_INFO("{}: finished - {}", operation, status.base.message.toStdString());
            else if (status.base.state == State::Failed)
                SPDLOG_ERROR("{}: failed - {}", operation, status.base.message.toStdString());

            const bool retryable_failure = status.base.state == State::Failed
                && status.base.message.compare("Cancelled.", Qt::CaseInsensitive) != 0;
            retry_button->setVisible(retryable_failure);
            update();

            if (finished)
            {
                CourierBridge::instance().clear_operation(active_operation_id);
                active_operation_id = 0;
                emit download_finished(status.base.state == State::Done);
            }
        });
}

DownloadProgress::~DownloadProgress()
{
    if (downloader)
    {
        courier_cancel(downloader);
        CourierBridge::instance().clear_operation(active_operation_id);
        courier_destroy(downloader);
        downloader = nullptr;
    }
}

void DownloadProgress::setup_buttons()
{
    const QSize window_size = window()->size();

    close_button = util::simple_utils::make_flat_button(this);
    close_button->setAccessibleName("Cancel or close download");
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseNormal]));
    close_button->setIconSize(dl::close_icon(window_size));
    close_button->setGeometry(dl::close(window_size));
    connect(close_button, &QPushButton::clicked, this, &DownloadProgress::cancel_download);
    close_button->raise();

    const QRect action_rect = dl::retry_button(window_size);
    retry_button = new QPushButton("RETRY", this);
    retry_button->setCursor(Qt::PointingHandCursor);
    retry_button->setStyleSheet(util::styles::k_link_blue);
    retry_button->setGeometry(action_rect);
    retry_button->setVisible(false);
    connect(retry_button, &QPushButton::clicked, this, &DownloadProgress::start_download);
    retry_button->raise();
}

void DownloadProgress::showEvent(QShowEvent* event)
{
    ModalOverlay::showEvent(event);
    if (active_operation_id == 0)
        start_download();
}

void DownloadProgress::cancel_download()
{
    if (current.base.state == State::Working && current.base.progress > 0.0)
    {
        const auto answer = QMessageBox::question(
            this,
            mode == Mode::Repair ? "Cancel Repair" : "Cancel Download",
            mode == Mode::Repair
                ? "Cancel the current repair? Verified and partial files will be kept so a later retry can continue."
                : "Cancel the current game download? Verified and partial files will be kept so a later retry can continue.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    if (downloader)
        courier_cancel(downloader);
    CourierBridge::instance().clear_operation(active_operation_id);
    active_operation_id = 0;
    current = DownloadStatus{};
    hide();
    emit closed();
}

void DownloadProgress::set_terminal_error(const QString& message)
{
    CourierBridge::instance().clear_operation(active_operation_id);
    active_operation_id = 0;
    current = DownloadStatus{};
    current.base.state = State::Failed;
    current.base.message = message;
    current.base.progress = -1.0;
    retry_button->show();
    update();
    emit download_finished(false);
}

void DownloadProgress::start_download()
{
    auto& config = Config::instance();
    const auto version = config.game_version();
    const auto& game = core::game::profile(version);
    const QString install = config.game_install_path();

    retry_button->hide();
    current = DownloadStatus{};
    current.base.state = State::Working;
    current.base.message = mode == Mode::Repair
        ? QStringLiteral("Preparing repair...")
        : QStringLiteral("Preparing download...");
    current.base.progress = 0.0;
    update();

    if (downloader)
    {
        courier_cancel(downloader);
        CourierBridge::instance().clear_operation(active_operation_id);
        courier_destroy(downloader);
        downloader = nullptr;
        active_operation_id = 0;
    }

    downloader = courier_create(
        game.cdn_base_url,
        CourierBridge::progress_callback(),
        CourierBridge::done_callback(),
        &CourierBridge::instance());

    if (!downloader)
    {
        SPDLOG_ERROR("download: failed to create courier for game {}",
                     core::game::to_string(version).toStdString());
        set_terminal_error(mode == Mode::Repair
            ? QStringLiteral("The repair service could not be created.")
            : QStringLiteral("The downloader could not be created."));
        return;
    }

    SPDLOG_INFO("{}: starting game {} update from {} to {}",
                mode == Mode::Repair ? "repair" : "download",
                core::game::to_string(version).toStdString(),
                game.cdn_base_url,
                install.toStdString());
    active_operation_id = courier_update(downloader, install.toUtf8().constData());
    if (active_operation_id == 0)
    {
        set_terminal_error(mode == Mode::Repair
            ? QStringLiteral("The repair could not be started.")
            : QStringLiteral("The download could not be started."));
        return;
    }
    CourierBridge::instance().begin_operation(active_operation_id);
    emit download_started();
}

QString DownloadProgress::human_size(const qulonglong bytes)
{
    constexpr double kb = 1'000.0;
    constexpr double mb = 1'000'000.0;
    constexpr double gb = 1'000'000'000.0;
    if (bytes >= gb) return QString::number(bytes / gb, 'f', 1) + " GB";
    if (bytes >= mb) return QString::number(bytes / mb, 'f', 1) + " MB";
    if (bytes >= kb) return QString::number(bytes / kb, 'f', 0) + " KB";
    return QString::number(bytes) + " B";
}

QString DownloadProgress::human_speed(const qulonglong bytes_per_sec)
{
    if (bytes_per_sec == 0) return "--";
    return human_size(bytes_per_sec) + "/s";
}

QString DownloadProgress::human_eta(const qulonglong remaining, const qulonglong throughput)
{
    if (throughput == 0) return "Estimating...";

    const qulonglong total_seconds = (remaining + throughput - 1) / throughput;
    const qulonglong hours = total_seconds / 3600;
    const qulonglong minutes = (total_seconds % 3600) / 60;
    const qulonglong seconds = total_seconds % 60;

    if (hours > 0)
        return minutes > 0 ? QString("%1h %2m").arg(hours).arg(minutes)
                           : QString("%1h").arg(hours);
    if (minutes > 0)
        return seconds > 0 ? QString("%1m %2s").arg(minutes).arg(seconds)
                           : QString("%1m").arg(minutes);
    return QString("%1s").arg(seconds);
}

void DownloadProgress::paint_content(QPainter& painter)
{
    const QSize window_size = window()->size();
    painter.drawPixmap(dl::box_rect(window_size), util::assets::images[util::assets::Image::BoxDownload]);

    const bool done = current.base.state == State::Done;
    const bool failed = current.base.state == State::Failed;
    const int files = current.file_count;

    QString title_text;
    if (done)
        title_text = mode == Mode::Repair ? "REPAIR COMPLETE" : "DOWNLOAD COMPLETE";
    else if (failed)
        title_text = mode == Mode::Repair ? "REPAIR FAILED" : "DOWNLOAD FAILED";
    else
    {
        switch (current.phase)
        {
            case courier_phase_preparing:
                title_text = mode == Mode::Repair ? "PREPARING REPAIR" : "PREPARING";
                break;
            case courier_phase_checking:
                title_text = files > 0
                    ? QString("CHECKING FILES (%1/%2)").arg(current.file_index).arg(files)
                    : "CHECKING FILES";
                break;
            case courier_phase_verifying:
                title_text = files > 0
                    ? QString("VERIFYING FILES (%1/%2)").arg(current.file_index).arg(files)
                    : "VERIFYING FILES";
                break;
            case courier_phase_downloading:
            default:
            {
                const bool resuming = current.base.message.startsWith(
                    QStringLiteral("Resuming"), Qt::CaseInsensitive);
                const QString action = mode == Mode::Repair
                    ? (resuming ? QStringLiteral("RESUMING REPAIR") : QStringLiteral("REPAIRING"))
                    : (resuming ? QStringLiteral("RESUMING") : QStringLiteral("DOWNLOADING"));
                title_text = files > 0
                    ? QStringLiteral("%1 FILES (%2/%3)")
                        .arg(action).arg(current.file_index).arg(files)
                    : action;
                break;
            }
        }
    }

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_row_title, window_size));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(util::colors::k_text_maroon);
    painter.drawText(dl::title(window_size), Qt::AlignCenter, title_text);

    QFont label_font = util::assets::fonts[util::assets::Font::Inter];
    label_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, window_size));
    label_font.setWeight(QFont::Medium);
    painter.setFont(label_font);
    painter.setPen(failed ? util::colors::k_warning : util::colors::k_text_label);

    const QRect info = dl::info_row(window_size);
    const qulonglong remaining = current.total > current.received ? current.total - current.received : 0;
    if (failed)
    {
        const QString message = painter.fontMetrics().elidedText(
            current.base.message, Qt::ElideRight, info.width());
        painter.drawText(info, Qt::AlignCenter, message);
    }
    else if (done)
    {
        painter.drawText(info, Qt::AlignCenter, current.base.message);
    }
    else if (current.phase == courier_phase_downloading)
    {
        painter.drawText(info, Qt::AlignLeft | Qt::AlignVCenter,
                         "Time remaining: " + human_eta(remaining, current.speed));
        painter.drawText(info, Qt::AlignRight | Qt::AlignVCenter,
                         human_speed(current.speed));
    }

    util::progress_bar::draw(painter, dl::bar_rect(window_size), current.base.progress);

    QFont percent_font = util::assets::fonts[util::assets::Font::NanumExtraBold];
    percent_font.setPixelSize(util::layout::scaled(util::layout::text::k_label, window_size));
    percent_font.setWeight(QFont::ExtraBold);
    painter.setFont(percent_font);
    painter.setPen(failed ? util::colors::k_warning : util::colors::k_text_maroon);
    const int shown = current.base.progress < 0.0 ? 0 : qRound(current.base.progress * 100.0);
    painter.drawText(dl::under_row(window_size), Qt::AlignCenter,
                     failed ? "Retry continues from saved files"
                            : QString("%1%").arg(qBound(0, shown, 100)));
}
