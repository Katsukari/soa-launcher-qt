#include "MainWindow.hpp"
#include "widgets/DownloadBox.hpp"
#include "widgets/Settings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"

#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWindow>

#include "util/ModalOverlay.hpp"
#include "widgets/GameInstall.hpp"

MainWindow::MainWindow(QWidget* parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(util::layout::win::k_default);
    setAttribute(Qt::WA_TranslucentBackground);

    setup_window_buttons();
    setup_logo();
    setup_version_label();
    setup_settings();
    setup_download_box();
    setup_game_install();
}

void MainWindow::setup_window_buttons()
{
    const QSize w = size();

    close_button = new QPushButton(this);
    close_button->setFlat(true);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setStyleSheet("outline:none; border: none; background: transparent;");
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseIcon]));
    close_button->setIconSize(util::layout::chrome::close_icon(w));
    close_button->setGeometry(util::layout::chrome::close(w));
    connect(close_button, &QPushButton::clicked, this, [this]() { close(); });

    minimize_button = new QPushButton(this);
    minimize_button->setFlat(true);
    minimize_button->setCursor(Qt::PointingHandCursor);
    minimize_button->setStyleSheet("outline:none; border:none; background:transparent; padding:0; margin:0;");
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
    settings = new Settings(this);
    settings->move(0, 0);
    settings->hide();
}

void MainWindow::setup_download_box()
{
    const QSize w = size();
    download_box = new DownloadBox(this);
    download_box->move(util::layout::download::pos(w));

    connect(download_box, &DownloadBox::settings_requested, this, [this]()
    {
        on_overlay_opened(settings);       // keeps_chrome()==false -> frames hidden, chrome hidden
        settings->show_over(this);
    });

    connect(download_box, &DownloadBox::download_triggered, this, [this]()
    {
        game_install->show_over(this);
        on_overlay_opened(game_install);
    });


    connect(settings, &Settings::closed, this, [this]()
    {
        on_overlay_closed(settings);
    });
}

void MainWindow::setup_game_install()
{
    game_install = new GameInstall(this);
    game_install->hide();

    connect(game_install, &GameInstall::closed, this, [this]()
    {
        on_overlay_closed(game_install);
    });
}

void MainWindow::on_overlay_opened(util::modal_overlay::ModalOverlay * m)
{
    if (m->keeps_chrome())
    {
        // GameInstall: keep frames painting, keep chrome buttons but raise them above the modal
        chrome_hidden = false;
        close_button->raise();
        minimize_button->raise();
    }
    else
    {
        // Settings: hide everything
        chrome_hidden = true;
        close_button->hide();
        minimize_button->hide();
    }
    update();   // repaint frames per the new gate
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

    // Background always drawn.
    const QRect bg = util::layout::region::rect(w);
    const int radius = util::layout::scaled(util::layout::region::k_radius, w);
    QPainterPath path;
    path.addRoundedRect(bg, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(bg, util::assets::images[util::assets::Image::Background]);
    painter.setClipping(false);

    // Frames + sidebar icons drawn unless a chrome-hiding overlay (Settings) is open.
    // GameInstall keeps chrome, so these stay painted under its blur and show through sharp
    // (GameInstall re-draws them on top via ModalOverlay::paint_frames).
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
