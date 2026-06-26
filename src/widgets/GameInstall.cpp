#include "widgets/GameInstall.hpp"
#include "widgets/DownloadProgress.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>
#include <QApplication>
#include <QFileDialog>
#include <QDir>

#include "core/wine/Shell.hpp"
#include "util/Config.hpp"
#include "core/Log.hpp"
#include "widgets/LauncherLog.hpp"
#include <spdlog/spdlog.h>

using util::config::Config;

GameInstall::GameInstall(core::wine::Shell* shell_, QWidget* parent) : ModalOverlay(parent), shell(shell_)
{
    game_path = Config::instance().game_install_path();

    setup_close_button();
    setup_buttons();

    close_button->installEventFilter(this);
    cancel_button->installEventFilter(this);
    install_button->installEventFilter(this);
}

void GameInstall::setup_close_button()
{
    const QSize w = window()->size();
    close_button = new QPushButton(this);
    close_button->setFlat(true);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setStyleSheet("border:none; background:transparent;");

    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    close_button->setIconSize(util::layout::install_modal::close_icon(w));
    close_button->setGeometry(util::layout::install_modal::close(w));

    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    close_button->raise();
}

void GameInstall::setup_buttons()
{
    const QSize w = window()->size();

    cancel_button = new QPushButton(this);
    cancel_button->setFlat(true);
    cancel_button->setCursor(Qt::PointingHandCursor);
    cancel_button->setStyleSheet("border:none; background:transparent;");
    cancel_button->setIcon(QIcon(util::assets::buttons[util::assets::Button::Cancel].normal));
    const QRect cancel_rect = util::layout::install_modal::cancel_button(w);
    cancel_button->setIconSize(cancel_rect.size());
    cancel_button->setGeometry(cancel_rect);

    connect(cancel_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    install_button = new QPushButton(this);
    install_button->setFlat(true);
    install_button->setCursor(Qt::PointingHandCursor);
    install_button->setStyleSheet("border:none; background:transparent;");
    install_button->setIcon(QIcon(util::assets::buttons[util::assets::Button::Install].normal));
    const QRect install_rect = util::layout::install_modal::install_button(w);
    install_button->setIconSize(install_rect.size());
    install_button->setGeometry(install_rect);

    connect(install_button, &QPushButton::clicked, this, [this]()
    {
        start_install();
    });

    cancel_button->raise();
    install_button->raise();

    change_path_button = new QPushButton("Change path", this);
    change_path_button->setFlat(true);
    change_path_button->setCursor(Qt::PointingHandCursor);
    change_path_button->setStyleSheet(
        "QPushButton"
        "{"
        "    background: transparent;"
        "    color: #2FB4E0;"
        "    font-family: 'Inter';"
        "    font-size: 15px;"
        "    text-decoration: underline;"
        "}"
        "QPushButton:hover { color: #6FD4EF; outline: none; border: none; }"
    );
    const QRect cp_row = util::layout::install_modal::changepath_line(w);
    const int cp_hit_w = qMin(cp_row.width() / 3, 150);
    change_path_button->setGeometry(cp_row.left(), cp_row.top(), cp_hit_w, cp_row.height() + 4);

    connect(change_path_button, &QPushButton::clicked, this, [this]
    {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select Game Install Location");
        if (!dir.isEmpty())
        {
            Config::instance().set_game_install_path(dir);
            game_path = Config::instance().game_install_path();
            show_warning = false;
            update();
        }
    });

    change_path_button->raise();
}

bool GameInstall::path_inside_prefix() const
{
    const QString prefix = QDir(Config::instance().wine_prefix()).absolutePath();
    const QString game   = QDir(game_path).absolutePath();

    return game == prefix || game.startsWith(prefix + QDir::separator());
}

void GameInstall::start_install()
{
    if (installing)
    {
        SPDLOG_WARN("game install already in progress");
        return;
    }

    if (!path_inside_prefix())
    {
        SPDLOG_ERROR("game install: chosen path is outside the wine prefix, refusing");
        show_warning = true;
        update();
        return;
    }

    installing = true;
    show_warning = false;
    update();

    SPDLOG_INFO("game install: starting download to {}", game_path.toStdString());

    if (!download)
    {
        download = new DownloadProgress(this);
        connect(download, &DownloadProgress::download_finished, this, [this](bool ok)
        {
            installing = false;
            if (download) download->hide();
            hide();
            if (ok) emit install_complete();
            else    emit closed();
        });
    }

    download->show_over(this);
}

void GameInstall::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    const QRect box = util::layout::install_modal::box_rect(w);
    painter.drawPixmap(box, util::assets::images[util::assets::Image::BoxGameInstall]);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(util::layout::install_modal::title(w), Qt::AlignCenter, "GAME INSTALLATION");

    QFont body_font = util::assets::fonts[util::assets::Font::Inter];
    body_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(QColor(0x39, 0x25, 0x18));
    painter.drawText(util::layout::install_modal::body(w), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
    "The game will be downloaded into the selected directory inside your Wine prefix. You can keep the default path or choose a custom one.");

    const QRect path_rect = util::layout::install_modal::path_field(w);
    painter.drawPixmap(path_rect, util::assets::images[util::assets::Image::InstallPath]);

    QFont caption_font = util::assets::fonts[util::assets::Font::Inter];
    caption_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
    caption_font.setWeight(QFont::Normal);
    painter.setFont(caption_font);
    painter.setPen(QColor(0x98, 0x87, 0x76));
    painter.drawText(path_rect.adjusted(10, 8, -10, 0), Qt::AlignTop | Qt::AlignLeft, "GAME INSTALLATION PATH");

    QFont path_font = util::assets::fonts[util::assets::Font::EurostileBold];
    path_font.setPixelSize(util::layout::scaled(16, w));
    path_font.setWeight(QFont::ExtraBold);
    painter.setFont(path_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    const QString elided = painter.fontMetrics().elidedText(
        game_path, Qt::ElideMiddle, path_rect.width() - 28);
    painter.drawText(path_rect.adjusted(14, 34, -14, 0), Qt::AlignTop | Qt::AlignLeft, elided);

    QFont note_font = util::assets::fonts[util::assets::Font::Inter];
    note_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
    note_font.setWeight(QFont::Medium);
    painter.setFont(note_font);
    painter.setPen(QColor(0x98, 0x87, 0x76));
    painter.drawText(util::layout::install_modal::changepath_line(w),
                     Qt::AlignRight | Qt::AlignVCenter, "~ 2 GB of free disk space required.");

    if (show_warning)
    {
        QFont warn_font = util::assets::fonts[util::assets::Font::Inter];
        warn_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
        warn_font.setWeight(QFont::DemiBold);
        painter.setFont(warn_font);
        painter.setPen(QColor(0xC0, 0x2A, 0x2A));
        painter.drawText(util::layout::install_modal::warning_line(w),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         "The game must be installed inside the Wine prefix.");
    }
}

bool GameInstall::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == cancel_button || obj == install_button || obj == change_path_button)
    {
        const auto& cancel   = util::assets::buttons[util::assets::Button::Cancel];
        const auto& install  = util::assets::buttons[util::assets::Button::Install];

        switch (event->type())
        {
            case QEvent::Enter:
                if (obj == cancel_button) cancel_button->setIcon(QIcon(cancel.hover));
                else if (obj == install_button) install_button->setIcon(QIcon(install.hover));
                break;

            case QEvent::Leave:
                if (obj == cancel_button) cancel_button->setIcon(QIcon(cancel.normal));
                else if (obj == install_button) install_button->setIcon(QIcon(install.normal));
                break;

            case QEvent::MouseButtonPress:
                if (obj == cancel_button || obj == install_button)
                {
                    const auto& b = (obj == cancel_button) ? cancel : install;
                    dynamic_cast<QPushButton*>(obj)->setIcon(QIcon(b.clicked));
                }
                break;

            case QEvent::MouseButtonRelease:
                if (obj == cancel_button || obj == install_button)
                {
                    const auto& b = (obj == cancel_button) ? cancel : install;
                    if (auto* button = dynamic_cast<QPushButton*>(obj); button->underMouse())
                        button->setIcon(QIcon(b.hover));
                    else
                        button->setIcon(QIcon(b.normal));
                }
                break;

            default: break;
        }
    }
    return QWidget::eventFilter(obj, event);
}