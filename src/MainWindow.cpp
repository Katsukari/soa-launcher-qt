#include "MainWindow.hpp"

#include "core/auth/AuthHandler.hpp"
#include "core/state/InstallState.hpp"
#include "core/state/ViewRouter.hpp"
#include "core/wine/Shell.hpp"
#include "util/Assets.hpp"
#include "util/Config.hpp"
#include "util/Layout.hpp"
#include "util/ModalOverlay.hpp"
#include "util/SimpleUtils.hpp"
#include "widgets/AliciaChooser.hpp"
#include "widgets/GameInstall.hpp"
#include "widgets/Settings.hpp"
#include "widgets/WineInstall.hpp"
#include "widgets/WineSelectMenu.hpp"

#include <QLabel>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QWindow>

using core::game::GameVersion;
using core::state::Stage;
using core::state::View;
using util::config::Config;

namespace
{
    QPixmap make_version_button(const QSize window_size, const QRect button_rect,
                                const QPixmap& frame, const QPixmap& icon,
                                const QPoint icon_offset)
    {
        QPixmap composed(button_rect.size());
        composed.fill(Qt::transparent);

        QPainter painter(&composed);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QSize icon_size = util::layout::scaled(icon.size(), window_size);
        painter.drawPixmap(icon_offset, icon.scaled(icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        painter.drawPixmap(QRect(QPoint(0, 0), button_rect.size()), frame);

        return composed;
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent),
      game_version(Config::instance().game_version())
{
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(util::layout::win::k_default);
    setAttribute(Qt::WA_TranslucentBackground);

    shell = new core::wine::Shell(this);
    auth = new AuthHandler(shell, this);
    install_state = new core::state::InstallState(this);

    setup_window_buttons();
    setup_version_label();
    setup_settings();
    setup_alicia_chooser();
    setup_game_selector();
    setup_wine_install();
    setup_game_install();
    setup_wine_select();

    connect(install_state, &core::state::InstallState::stage_changed,
            this, &MainWindow::on_stage_changed);

    install_state->probe();
}

void MainWindow::setup_window_buttons()
{
    const QSize window_size = size();

    close_button = util::simple_utils::make_flat_button(this);
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseIcon]));
    close_button->setIconSize(util::layout::chrome::close_icon(window_size));
    close_button->setGeometry(util::layout::chrome::close(window_size));
    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        close();
    });

    minimize_button = util::simple_utils::make_flat_button(this);
    minimize_button->setIcon(QIcon(util::assets::images[util::assets::Image::Minimize]));
    minimize_button->setIconSize(util::layout::chrome::minimize_icon(window_size));
    minimize_button->setGeometry(util::layout::chrome::minimize(window_size));
    connect(minimize_button, &QPushButton::clicked, this, [this]()
    {
        showMinimized();
    });
}

void MainWindow::setup_version_label()
{
    const QSize window_size = size();
    auto* version_label = new QLabel("VERSION 0.1.0", this);

    QFont font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    font.setPixelSize(util::layout::scaled(util::layout::text::k_version, window_size));
    font.setWeight(QFont::Black);
    font.setLetterSpacing(QFont::PercentageSpacing, 108);
    version_label->setFont(font);
    version_label->setStyleSheet("color: #747B82; background: transparent;");
    version_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    version_label->setGeometry(util::layout::chrome::version(window_size));
}

void MainWindow::setup_settings()
{
    settings = new Settings(shell, this);
    settings->move(0, 0);
    settings->hide();

    connect(settings, &Settings::closed, this, [this]()
    {
        on_overlay_closed(settings);
    });
}

void MainWindow::setup_alicia_chooser()
{
    wine_install = new WineInstall(shell, this);

    const QSize window_size = size();
    alicia_chooser = new AliciaChooser(auth, shell, install_state, this);
    alicia_chooser->set_game_version(game_version);
    alicia_chooser->move(util::layout::alicia_chooser::pos(window_size));

    connect(alicia_chooser, &AliciaChooser::settings_requested, this, [this]()
    {
        on_overlay_opened(settings);
        settings->show_over(this);
    });

    connect(alicia_chooser, &AliciaChooser::download_triggered, this, [this]()
    {
        open_for_current_stage();
    });

    connect(alicia_chooser, &AliciaChooser::reset_config_requested, this, [this]()
    {
        const auto answer = QMessageBox::question(
            this,
            "Reset Launcher Config",
            "This resets launcher settings and signs you out.\n\n"
            "The Wine prefix and both game installations will not be deleted.",
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);

        if (answer != QMessageBox::Yes) return;

        Config::instance().reset_launcher_config();
        game_version = Config::instance().game_version();
        alicia_chooser->set_game_version(game_version);
        game_install->refresh_game_path();
        refresh_game_selector();
        install_state->probe();
        update();
    });
}

void MainWindow::setup_game_selector()
{
    const QSize window_size = size();

    playtest_button = util::simple_utils::make_flat_button(this);
    playtest_button->setCursor(Qt::PointingHandCursor);
    playtest_button->setFocusPolicy(Qt::NoFocus);
    playtest_button->setAccessibleName("Story of Alicia Playtest");
    playtest_button->setGeometry(util::layout::chrome::playtest_button(window_size));
    connect(playtest_button, &QPushButton::clicked, this, [this]()
    {
        set_game_version(GameVersion::Playtest);
    });

    alicia_2_button = util::simple_utils::make_flat_button(this);
    alicia_2_button->setCursor(Qt::PointingHandCursor);
    alicia_2_button->setFocusPolicy(Qt::NoFocus);
    alicia_2_button->setAccessibleName("Story of Alicia 2.0");
    alicia_2_button->setGeometry(util::layout::chrome::alicia_2_button(window_size));
    connect(alicia_2_button, &QPushButton::clicked, this, [this]()
    {
        set_game_version(GameVersion::Alicia2);
    });

    refresh_game_selector();
}

void MainWindow::setup_wine_install()
{
    wine_install->hide();

    connect(wine_install, &WineInstall::closed, this, [this]()
    {
        on_overlay_closed(wine_install);
    });
}

void MainWindow::setup_game_install()
{
    game_install = new GameInstall(shell, this);
    game_install->hide();

    connect(game_install, &GameInstall::closed, this, [this]()
    {
        on_overlay_closed(game_install);
    });
}

void MainWindow::setup_wine_select()
{
    wine_select = new WineSelectMenu(this);
    wine_select->hide();

    connect(wine_select, &WineSelectMenu::runtime_chosen, this, [this]()
    {
        close_overlay(wine_select);
        install_state->probe();
        open_for_current_stage();
    });

    connect(wine_select, &WineSelectMenu::closed, this, [this]()
    {
        on_overlay_closed(wine_select);
    });
}

void MainWindow::set_game_version(const GameVersion version)
{
    if (game_version == version) return;

    game_version = version;
    Config::instance().set_game_version(version);
    alicia_chooser->set_game_version(version);
    game_install->refresh_game_path();
    refresh_game_selector();
    install_state->probe();
    update();
}

void MainWindow::refresh_game_selector()
{
    const QSize window_size = size();
    const QRect playtest_rect = util::layout::chrome::playtest_button(window_size);
    const QRect alicia_2_rect = util::layout::chrome::alicia_2_button(window_size);

    const QPixmap& active = util::assets::images[util::assets::Image::VersionFrameActive];
    const QPixmap& inactive = util::assets::images[util::assets::Image::VersionFrameInactive];
    const QPixmap& playtest_icon = util::assets::images[util::assets::Image::VersionIconPlaytest];
    const QPixmap& alicia_2_icon = util::assets::images[util::assets::Image::VersionIconAlicia2];

    const QPixmap playtest_pixmap = make_version_button(
        window_size,
        playtest_rect,
        game_version == GameVersion::Playtest ? active : inactive,
        playtest_icon,
        util::layout::chrome::playtest_icon_offset(window_size));

    const QPixmap alicia_2_pixmap = make_version_button(
        window_size,
        alicia_2_rect,
        game_version == GameVersion::Alicia2 ? active : inactive,
        alicia_2_icon,
        util::layout::chrome::alicia_2_icon_offset(window_size));

    QIcon playtest_icon_set;
    playtest_icon_set.addPixmap(playtest_pixmap, QIcon::Normal);
    playtest_icon_set.addPixmap(playtest_pixmap, QIcon::Disabled);

    QIcon alicia_2_icon_set;
    alicia_2_icon_set.addPixmap(alicia_2_pixmap, QIcon::Normal);
    alicia_2_icon_set.addPixmap(alicia_2_pixmap, QIcon::Disabled);

    playtest_button->setIcon(playtest_icon_set);
    playtest_button->setIconSize(playtest_rect.size());
    alicia_2_button->setIcon(alicia_2_icon_set);
    alicia_2_button->setIconSize(alicia_2_rect.size());
}

void MainWindow::set_game_switching_enabled(const Stage stage)
{
    const bool enabled =
        stage != Stage::SettingUpPrefix &&
        stage != Stage::Downloading &&
        stage != Stage::Updating &&
        stage != Stage::Authenticating;

    playtest_button->setEnabled(enabled);
    alicia_2_button->setEnabled(enabled);
}

void MainWindow::open_for_current_stage()
{
    const View view = core::state::view_for(install_state->stage());

    if (view == View::WineSelect) open_overlay(wine_select);
    else if (view == View::WineInstall) open_overlay(wine_install);
    else if (view == View::GameInstall)
    {
        game_install->refresh_game_path();
        open_overlay(game_install);
    }
}

void MainWindow::on_stage_changed(const Stage stage)
{
    set_game_switching_enabled(stage);

    const View view = core::state::view_for(stage);
    if (view == last_view) return;
    last_view = view;

    switch (view)
    {
        case View::WineSelect:
            close_overlay(wine_install);
            close_overlay(game_install);
            break;

        case View::WineInstall:
            close_overlay(wine_select);
            close_overlay(game_install);
            if (stage == Stage::NeedsPrefix) break;
            open_overlay(wine_install);
            break;

        case View::GameInstall:
            close_overlay(wine_select);
            close_overlay(wine_install);
            open_overlay(game_install);
            break;

        case View::AliciaChooser:
            close_overlay(wine_select);
            close_overlay(wine_install);
            close_overlay(game_install);
            break;

        case View::Loading:
        case View::Error:
            break;
    }
}

void MainWindow::on_overlay_opened(util::modal_overlay::ModalOverlay* overlay)
{
    if (overlay->keeps_chrome())
    {
        chrome_hidden = false;
        close_button->raise();
        minimize_button->raise();
    }
    else
    {
        chrome_hidden = true;
        close_button->hide();
        minimize_button->hide();
    }
    update();
}

void MainWindow::on_overlay_closed(util::modal_overlay::ModalOverlay*)
{
    chrome_hidden = false;
    close_button->show();
    minimize_button->show();
    update();
}

void MainWindow::open_overlay(util::modal_overlay::ModalOverlay* overlay)
{
    if (!overlay->isVisible())
    {
        overlay->show_over(this);
        on_overlay_opened(overlay);
    }
}

void MainWindow::close_overlay(util::modal_overlay::ModalOverlay* overlay)
{
    if (overlay->isVisible())
    {
        overlay->hide();
        on_overlay_closed(overlay);
    }
}

void MainWindow::paintEvent(QPaintEvent*)
{
    const QSize window_size = size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect background_rect = util::layout::region::rect(window_size);
    const int radius = util::layout::scaled(util::layout::region::k_radius, window_size);
    QPainterPath path;
    path.addRoundedRect(background_rect, radius, radius);
    painter.setClipPath(path);

    const util::assets::Image background = game_version == GameVersion::Alicia2
        ? util::assets::Image::BackgroundAlicia2
        : util::assets::Image::BackgroundPlaytest;
    painter.drawPixmap(background_rect, util::assets::images[background]);
    painter.setClipping(false);

    if (!chrome_hidden)
    {
        const QPixmap left = util::assets::images[util::assets::Image::LeftFrame]
            .scaledToHeight(height(), Qt::SmoothTransformation);
        painter.drawPixmap(0, 0, left);

        const QPixmap right = util::assets::images[util::assets::Image::RightFrame]
            .scaledToHeight(height(), Qt::SmoothTransformation);
        painter.drawPixmap(width() - right.width(), 0, right);
    }
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        windowHandle()->startSystemMove();
        event->accept();
    }
}
