#include "widgets/PrefixProgress.hpp"
#include "widgets/LauncherLog.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "../../include/util/ProgressBar.hpp"
#include <QPainter>
#include <QPushButton>
#include <QTimer>

#include "core/wine/Shell.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace dl = util::layout::progress_modal;

namespace
{
    constexpr int    k_anim_interval_ms = 16;
    constexpr double k_fill_per_tick    = 0.6;
    constexpr double k_pct_wineboot     = 33.0;
    constexpr double k_pct_winetricks   = 66.0;
    constexpr double k_pct_done         = 100.0;
}

PrefixProgress::PrefixProgress(core::wine::Shell* shell_, QWidget* parent)
    : ModalOverlay(parent), shell(shell_)
{
    setup_buttons();
    close_button->installEventFilter(this);

    anim = new QTimer(this);
    anim->setInterval(k_anim_interval_ms);
    connect(anim, &QTimer::timeout, this, [this]()
    {
        if (current_pct < target_pct)
        {
            current_pct += k_fill_per_tick;
            if (current_pct > target_pct) current_pct = target_pct;
            update();
        }
        else if (done && current_pct >= k_pct_done && !emitted)
        {
            emitted = true;
            anim->stop();
            update();
            QTimer::singleShot(800, this, [this]()
            {
                hide();
                emit prefix_complete();
            });
        }
    });

    connect(shell, &core::wine::Shell::setup_status, this, [this](const QString& message)
    {
        status = message;

        if (message.contains("prefix", Qt::CaseInsensitive))
        {
            step = 1;
            target_pct = k_pct_wineboot;
        }
        else if (message.contains("components", Qt::CaseInsensitive))
        {
            step = 2;
            target_pct = k_pct_winetricks;
        }
        update();
    });

    connect(shell, &core::wine::Shell::wine_setup_finished, this, [this](bool ok)
    {
        done   = ok;
        failed = !ok;
        if (ok) target_pct = k_pct_done;
        update();
    });
}

void PrefixProgress::setup_buttons()
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
        hide();
        emit closed();
    });
    close_button->raise();

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

void PrefixProgress::showEvent(QShowEvent* event)
{
    ModalOverlay::showEvent(event);
    current_pct = 0.0;
    target_pct  = 0.0;
    step        = 0;
    done        = false;
    failed      = false;
    emitted     = false;
    status      = "Starting...";
    anim->start();
}

void PrefixProgress::hideEvent(QHideEvent* event)
{
    ModalOverlay::hideEvent(event);
    anim->stop();
}

void PrefixProgress::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    painter.drawPixmap(dl::box_rect(w), util::assets::images[util::assets::Image::BoxDownload]);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_row_title, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(dl::title(w), Qt::AlignCenter, "INSTALLING WINE PREFIX");

    QFont label_font = util::assets::fonts[util::assets::Font::Inter];
    label_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    label_font.setWeight(QFont::Medium);
    painter.setFont(label_font);
    painter.setPen(QColor(0x9E, 0x8E, 0x7E));

    const QRect info = dl::info_row(w);
    painter.drawText(info, Qt::AlignLeft | Qt::AlignVCenter, status);
    if (step > 0)
        painter.drawText(info, Qt::AlignRight | Qt::AlignVCenter, QString("Step %1 of 2").arg(step));

    util::progress_bar::draw(painter, dl::bar_rect(w), current_pct / 100.0);

    QFont pct_font = util::assets::fonts[util::assets::Font::Inter];
    pct_font.setPixelSize(util::layout::scaled(util::layout::text::k_label, w));
    pct_font.setWeight(QFont::DemiBold);
    painter.setFont(pct_font);
    painter.setPen(failed ? QColor(0xC0, 0x2A, 0x2A) : QColor(0x4F, 0x17, 0x17));
    painter.drawText(dl::under_row(w), Qt::AlignCenter, QString("%1%").arg(int(current_pct)));
}