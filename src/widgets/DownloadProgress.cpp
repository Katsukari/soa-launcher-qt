#include "widgets/DownloadProgress.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/Config.hpp"
#include "util/ProgressBar.hpp"
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>

#include "core/network/DownloadBridge.hpp"
#include "widgets/LauncherLog.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace dl = util::layout::progress_modal;
using util::config::Config;
using core::network::DownloadBridge;

namespace
{
    constexpr const char* k_cdn_base = "https://r2.storyofalicia.com/game";
}

DownloadProgress::DownloadProgress(QWidget* parent) : ModalOverlay(parent)
{
    setup_buttons();
    close_button->installEventFilter(this);

    connect(&DownloadBridge::instance(), &DownloadBridge::progress, this,
        [this](const QString& message, int pct, qulonglong rec, qulonglong tot, qulonglong tput)
        {
            status   = message;
            percent  = pct;
            received = rec;
            total    = tot;
            speed    = tput;

            static const QRegularExpression re("\\((\\d+)\\s*/\\s*(\\d+)\\)");
            const auto m = re.match(message);
            if (m.hasMatch())
            {
                const int idx = m.captured(1).toInt();
                if (idx != file_index)
                    SPDLOG_INFO("download: {} ({}%)", message.toStdString(), pct);
                file_index = idx;
                file_count = m.captured(2).toInt();
            }
            update();
        });

    connect(&DownloadBridge::instance(), &DownloadBridge::finished, this,
        [this](bool ok, const QString& message)
        {
            done    = ok;
            failed  = !ok;
            status  = message;
            if (ok) percent = 100;

            if (ok)
                SPDLOG_INFO("download: finished - {}", message.toStdString());
            else
                SPDLOG_ERROR("download: failed - {}", message.toStdString());

            update();
            emit download_finished(ok);
        });
}

DownloadProgress::~DownloadProgress()
{
    if (downloader)
    {
        soa_cancel(downloader);
        soa_downloader_destroy(downloader);
        downloader = nullptr;
    }
}

void DownloadProgress::setup_buttons()
{
    const QSize w = window()->size();

    close_button = new QPushButton(this);
    close_button->setFlat(true);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setStyleSheet("border:none; background:transparent;");
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    close_button->setIconSize(dl::close_icon(w));
    close_button->setGeometry(dl::close(w));
    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        if (downloader) soa_cancel(downloader);
        hide();
        emit closed();
    });
    close_button->raise();

    pause_button = new QPushButton(this);
    pause_button->setFlat(true);
    pause_button->setCursor(Qt::PointingHandCursor);
    pause_button->setFocusPolicy(Qt::NoFocus);
    pause_button->setStyleSheet("border:none; outline:none; background:transparent;");
    pause_button->setIconSize(dl::pause_icon(w));
    pause_button->setGeometry(dl::pause(w));
    connect(pause_button, &QPushButton::clicked, this, [this]()
    {
        paused = !paused;
        update_pause_icon();
        update();
    });
    update_pause_icon();
    pause_button->raise();

    log_button = new QPushButton("SHOW LOG", this);
    log_button->setCursor(Qt::PointingHandCursor);
    log_button->setFocusPolicy(Qt::NoFocus);
    log_button->setStyleSheet(
        "QPushButton"
        "{"
        "    background: transparent;"
        "    border: none;"
        "    outline: none;"
        "    color: #2FB4E0;"
        "    font-family: 'Inter';"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    text-decoration: underline;"
        "}"
        "QPushButton:hover { color: #6FD4EF; }"
        "QPushButton:focus { outline: none; border: none; }"
    );
    log_button->setGeometry(dl::log_button(w));
    connect(log_button, &QPushButton::clicked, this, []()
    {
        LauncherLog::instance()->show();
        LauncherLog::instance()->raise();
    });
    log_button->raise();
}

void DownloadProgress::update_pause_icon()
{
    const auto img = util::assets::Image::CloseIcon;
    pause_button->setIcon(QIcon(util::assets::images[img]));
}

void DownloadProgress::showEvent(QShowEvent* event)
{
    ModalOverlay::showEvent(event);

    status     = "Preparing download...";
    percent    = 0;
    received   = 0;
    total      = 0;
    speed      = 0;
    file_index = 0;
    file_count = 0;
    done       = false;
    failed     = false;
    paused     = false;
    update_pause_icon();

    start_download();
}

void DownloadProgress::hideEvent(QHideEvent* event)
{
    ModalOverlay::hideEvent(event);
}

void DownloadProgress::start_download()
{
    const QString install = Config::instance().game_install_path();

    if (!downloader)
    {
        downloader = soa_downloader_create(
            k_cdn_base,
            DownloadBridge::progress_callback(),
            DownloadBridge::done_callback(),
            this);
    }

    SPDLOG_INFO("download: starting update to {}", install.toStdString());
    soa_update(downloader, install.toUtf8().constData());
}

QString DownloadProgress::human_size(qulonglong bytes)
{
    constexpr double kb = 1024.0, mb = kb * 1024.0, gb = mb * 1024.0;
    if (bytes >= gb) return QString::number(bytes / gb, 'f', 1) + " GB";
    if (bytes >= mb) return QString::number(bytes / mb, 'f', 1) + " MB";
    if (bytes >= kb) return QString::number(bytes / kb, 'f', 1) + " KB";
    return QString::number(bytes) + " B";
}

QString DownloadProgress::human_speed(qulonglong bytes_per_sec)
{
    if (bytes_per_sec == 0) return "--";
    return human_size(bytes_per_sec) + "/s";
}

QString DownloadProgress::human_eta(qulonglong remaining, qulonglong throughput)
{
    if (throughput == 0) return "Estimating...";

    const qulonglong secs = remaining / throughput;
    if (secs >= 3600) return QString("%1 h %2 m").arg(secs / 3600).arg((secs % 3600) / 60);
    if (secs >= 60)   return QString("%1 min").arg((secs + 59) / 60);
    return QString("%1 sec").arg(secs);
}

void DownloadProgress::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    painter.drawPixmap(dl::box_rect(w), util::assets::images[util::assets::Image::BoxDownload]);

    QString title_text;
    if (file_count > 0)
        title_text = QString("DOWNLOADING %1 OF %2 FILES").arg(file_index).arg(file_count);
    else if (done)
        title_text = "DOWNLOAD COMPLETE";
    else
        title_text = "DOWNLOADING";

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_row_title, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(dl::title(w), Qt::AlignCenter, title_text);

    QFont label_font = util::assets::fonts[util::assets::Font::Inter];
    label_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    label_font.setWeight(QFont::Medium);
    painter.setFont(label_font);
    painter.setPen(QColor(0x9E, 0x8E, 0x7E));

    const QRect info = dl::info_row(w);
    const qulonglong remaining = (total > received) ? (total - received) : 0;

    QString left_text;
    if (paused)      left_text = "Paused";
    else if (done)   left_text = "Finished";
    else             left_text = "Time remaining: " + human_eta(remaining, speed);

    painter.drawText(info, Qt::AlignLeft | Qt::AlignVCenter, left_text);
    painter.drawText(info, Qt::AlignRight | Qt::AlignVCenter,
                     "Download Speed: " + human_speed(paused ? 0 : speed));

    util::progress_bar::draw(painter, dl::bar_rect(w), percent / 100.0);

    QFont pct_font = util::assets::fonts[util::assets::Font::Inter];
    pct_font.setPixelSize(util::layout::scaled(util::layout::text::k_label, w));
    pct_font.setWeight(QFont::DemiBold);
    painter.setFont(pct_font);
    painter.setPen(failed ? QColor(0xC0, 0x2A, 0x2A) : QColor(0x4F, 0x17, 0x17));
    painter.drawText(dl::under_row(w), Qt::AlignCenter, QString("%1%").arg(qBound(0, percent, 100)));
}