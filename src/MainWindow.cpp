#include "MainWindow.hpp"
#include "widgets/Playtest.hpp"
#include "widgets/Settings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "core/wine/Shell.hpp"
#include "core/auth/AuthHandler.hpp"

#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWindow>

#include "util/ModalOverlay.hpp"
#include "widgets/WineInstall.hpp"
#include "widgets/GameInstall.hpp"

#include "core/state/InstallState.hpp"
#include "core/state/ViewRouter.hpp"
#include "core/state/Stage.hpp"
#include "core/state/View.hpp"

using core::state::Stage;
using core::state::View;

MainWindow::MainWindow(QWidget* parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(util::layout::win::k_default);
    setAttribute(Qt::WA_TranslucentBackground);

    shell = new core::wine::Shell(this);
    auth  = new AuthHandler(shell, this);

    install_state = new core::state::InstallState(this);

    setup_window_buttons();
    setup_logo();
    setup_version_label();
    setup_settings();
    setup_playtest();
    setup_wine_install();
    setup_game_install();

    connect(install_state, &core::state::InstallState::stage_changed,
            this, &MainWindow::on_stage_changed);

    install_state->probe();
}

void MainWindow::setup_window_buttons()
{
    const QSize w = size();

    close_button = util::simple_utils::make_flat_button(this);
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseIcon]));
    close_button->setIconSize(util::layout::chrome::close_icon(w));
    close_button->setGeometry(util::layout::chrome::close(w));
    connect(close_button, &QPushButton::clicked, this, [this]() { close(); });

    minimize_button = util::simple_utils::make_flat_button(this);
    minimize_button->setIcon(QIcon(util::assets::images[util::assets::Image::Minimize]));
    minimize_button->setIconSize(util::layout::chrome::minimize_icon(w));
    minimize_button->setGeometry(util::layout::chrome::minimize(w));
    connect(minimize_button, &QPushButton::clicked, this, [this]() { showMinimized(); });
}

void MainWindow::setup_logo()
{
    const QSize w = size();
    const int width = util::layout::chrome::logo_width(w);

    const QPixmap scaled = util::assets::images[util::assets::Image::Logo]
        .scaledToWidth(width, Qt::SmoothTransformation);

    auto* logo = new QLabel(this);
    logo->setPixmap(scaled);
    logo->setStyleSheet("background: transparent;");

    const QPoint pos = util::layout::chrome::logo_pos(w);
    logo->setGeometry(pos.x(), pos.y(), width, scaled.height());
}

void MainWindow::setup_version_label()
{
    const QSize w = size();
    const auto version_label = new QLabel("VERSION 0.1.0", this);

    QFont f = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    f.setPixelSize(util::layout::scaled(util::layout::text::k_version, w));
    f.setWeight(QFont::Black);
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    version_label->setFont(f);
    version_label->setStyleSheet("color: #E4E8EA; background: transparent;");
    version_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    version_label->setGeometry(util::layout::chrome::version(w));
}

void MainWindow::setup_settings()
{
    settings = new Settings(shell, this);
    settings->move(0, 0);
    settings->hide();
}

void MainWindow::setup_playtest()
{
    wine_install = new WineInstall(shell, this);
    const QSize w = size();
    playtest = new Playtest(auth, shell, install_state, this);
    playtest->move(util::layout::playtest::pos(w));

    connect(playtest, &Playtest::settings_requested, this, [this]()
    {
        on_overlay_opened(settings);
        settings->show_over(this);
    });

    connect(playtest, &Playtest::download_triggered, this, [this]()
    {
        open_for_current_stage();
    });

    connect(settings, &Settings::closed, this, [this]()
    {
        on_overlay_closed(settings);
    });
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

void MainWindow::open_for_current_stage()
{
    const View v = core::state::view_for(install_state->stage());

    if (v == View::WineInstall && !wine_install->isVisible())
    {
        wine_install->show_over(this);
        on_overlay_opened(wine_install);
    }
    else if (v == View::GameInstall && !game_install->isVisible())
    {
        game_install->show_over(this);
        on_overlay_opened(game_install);
    }
}

void MainWindow::on_stage_changed(Stage s)
{
    const View v = core::state::view_for(s);
    if (v == last_view) return;
    last_view = v;

    switch (v)
    {
        case View::WineInstall:
            if (game_install->isVisible()) { game_install->hide(); on_overlay_closed(game_install); }
            if (s == Stage::NeedsPrefix) break;
            if (!wine_install->isVisible())
            {
                wine_install->show_over(this);
                on_overlay_opened(wine_install);
            }
            break;

        case View::GameInstall:
            if (wine_install->isVisible()) { wine_install->hide(); on_overlay_closed(wine_install); }
            if (!game_install->isVisible())
            {
                game_install->show_over(this);
                on_overlay_opened(game_install);
            }
            break;

        case View::Playtest:
            if (wine_install->isVisible()) { wine_install->hide(); on_overlay_closed(wine_install); }
            if (game_install->isVisible()) { game_install->hide(); on_overlay_closed(game_install); }
            break;

        case View::Loading:
        case View::Error:
            break;
    }
}

void MainWindow::on_overlay_opened(util::modal_overlay::ModalOverlay * m)
{
    if (m->keeps_chrome())
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

void MainWindow::on_overlay_closed(util::modal_overlay::ModalOverlay *)
{
    chrome_hidden = false;
    close_button->show();
    minimize_button->show();
    update();
}

void MainWindow::paintEvent(QPaintEvent*)
{
    const QSize w = size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect bg = util::layout::region::rect(w);
    const int radius = util::layout::scaled(util::layout::region::k_radius, w);
    QPainterPath path;
    path.addRoundedRect(bg, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(bg, util::assets::images[util::assets::Image::Background]);
    painter.setClipping(false);

    if (!chrome_hidden)
    {
        const QPixmap left = util::assets::images[util::assets::Image::LeftFrame]
            .scaledToHeight(height(), Qt::SmoothTransformation);
        painter.drawPixmap(0, 0, left);

        const QPixmap right = util::assets::images[util::assets::Image::RightFrame]
            .scaledToHeight(height(), Qt::SmoothTransformation);
        painter.drawPixmap(width() - right.width(), 0, right);

        painter.drawPixmap(util::layout::chrome::pt_icon(w),   util::assets::images[util::assets::Image::IconPT]);
        painter.drawPixmap(util::layout::chrome::lock_icon(w), util::assets::images[util::assets::Image::IconLock]);
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