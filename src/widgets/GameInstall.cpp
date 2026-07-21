#include "widgets/GameInstall.hpp"
#include "widgets/DownloadProgress.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/Styles.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Colors.hpp"
#include <QPainter>
#include <QApplication>
#include <QFileDialog>
#include <QDir>

#include "core/wine/Shell.hpp"
#include "util/Config.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

using util::config::Config;

GameInstall::GameInstall(core::wine::Shell* shell_, QWidget* parent) : ModalOverlay(parent), shell(shell_)
{
    refresh_game_path();

    setup_close_button();
    setup_buttons();

    close_button->installEventFilter(this);
    cancel_button->installEventFilter(this);
    install_button->installEventFilter(this);
}

void GameInstall::refresh_game_path()
{
    game_path = Config::instance().game_install_path();
    show_warning = false;
    update();
}

void GameInstall::setup_close_button()
{
    const QSize w = window()->size();
    close_button = util::simple_utils::make_flat_button(this);

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

    cancel_button = util::simple_utils::make_flat_button(this);
    cancel_button->setIcon(QIcon(util::assets::buttons[util::assets::Button::Cancel].normal));
    const QRect cancel_rect = util::layout::install_modal::cancel_button(w);
    cancel_button->setIconSize(cancel_rect.size());
    cancel_button->setGeometry(cancel_rect);

    connect(cancel_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    install_button = util::simple_utils::make_flat_button(this);
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
    change_path_button->setStyleSheet(util::styles::k_link_blue_lg);
    change_path_button->setGeometry(util::layout::install_modal::change_path_button(w));

    connect(change_path_button, &QPushButton::clicked, this, [this]
    {
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            "Select Game Install Location",
            Config::instance().prefix_root());
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


void GameInstall::set_installing(const bool value)
{
    installing = value;

    const auto& assets = util::assets::buttons[util::assets::Button::Install];
    const QPixmap& pixmap = value && !assets.loading.isNull() ? assets.loading : assets.normal;
    QIcon icon;
    icon.addPixmap(pixmap, QIcon::Normal);
    icon.addPixmap(pixmap, QIcon::Disabled);
    install_button->setIcon(icon);
    install_button->setEnabled(!value);
    change_path_button->setEnabled(!value);
    update();
}

bool GameInstall::path_inside_prefix() const
{
    return Config::instance().path_inside_prefix(game_path);
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
        SPDLOG_ERROR("game install: chosen path {} is outside prefix root {}, refusing",
                     game_path.toStdString(),
                     Config::instance().prefix_root().toStdString());
        show_warning = true;
        update();
        return;
    }

    set_installing(true);
    show_warning = false;
    update();

    SPDLOG_INFO("game install: starting download to {}", game_path.toStdString());

    if (!download)
    {
        download = new DownloadProgress(this);
        connect(download, &DownloadProgress::download_started, this, [this]()
        {
            set_installing(true);
        });
        connect(download, &DownloadProgress::download_finished, this, [this](const bool ok)
        {
            set_installing(false);
            if (ok && download)
                download->hide();
        });
        connect(download, &DownloadProgress::closed, this, [this]()
        {
            set_installing(false);
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
    painter.setPen(util::colors::k_text_maroon);
    painter.drawText(util::layout::install_modal::title(w), Qt::AlignCenter, "GAME INSTALLATION");

    QFont body_font = util::assets::fonts[util::assets::Font::Inter];
    body_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(util::colors::k_text_body);
    painter.drawText(util::layout::install_modal::body(w), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
    "The game will be downloaded into the selected directory inside your Wine prefix. You can keep the default path or choose a custom one.");

    const QRect path_rect = util::layout::install_modal::path_field(w);
    painter.drawPixmap(path_rect, util::assets::images[util::assets::Image::InstallPath]);

    QFont caption_font = util::assets::fonts[util::assets::Font::Inter];
    caption_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
    caption_font.setWeight(QFont::Normal);
    painter.setFont(caption_font);
    painter.setPen(util::colors::k_text_caption);
    painter.drawText(path_rect.adjusted(10, 8, -10, 0), Qt::AlignTop | Qt::AlignLeft, "GAME INSTALLATION PATH");

    QFont path_font = util::assets::fonts[util::assets::Font::EurostileBold];
    path_font.setPixelSize(util::layout::scaled(16, w));
    path_font.setWeight(QFont::ExtraBold);
    painter.setFont(path_font);
    painter.setPen(util::colors::k_text_maroon);
    const QString elided = painter.fontMetrics().elidedText(
        game_path, Qt::ElideMiddle, path_rect.width() - 28);
    painter.drawText(path_rect.adjusted(14, 34, -14, 0), Qt::AlignTop | Qt::AlignLeft, elided);

    QFont note_font = util::assets::fonts[util::assets::Font::Inter];
    note_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
    note_font.setWeight(QFont::Medium);
    painter.setFont(note_font);
    painter.setPen(util::colors::k_text_caption);
    painter.drawText(util::layout::install_modal::changepath_line(w),
                     Qt::AlignRight | Qt::AlignVCenter, "~ 2 GB of free disk space required.");

    if (show_warning)
    {
        QFont warn_font = util::assets::fonts[util::assets::Font::Inter];
        warn_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
        warn_font.setWeight(QFont::DemiBold);
        painter.setFont(warn_font);
        painter.setPen(util::colors::k_warning);
        painter.drawText(util::layout::install_modal::warning_line(w),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         "The game must be installed inside the Wine prefix.");
    }
}

bool GameInstall::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == cancel_button || obj == install_button)
    {
        const auto& cancel  = util::assets::buttons[util::assets::Button::Cancel];
        const auto& install = util::assets::buttons[util::assets::Button::Install];
        if (obj == cancel_button)
            util::simple_utils::apply_button_state(event, cancel_button, cancel.normal, cancel.hover, cancel.clicked);
        else
            util::simple_utils::apply_button_state(event, install_button, install.normal, install.hover, install.clicked);
    }
    return QWidget::eventFilter(obj, event);
}
