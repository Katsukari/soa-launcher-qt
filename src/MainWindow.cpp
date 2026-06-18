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

#include "widgets/GameInstall.hpp"

MainWindow::MainWindow(QWidget* parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(layout::win::k_default);
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
    close_button->setStyleSheet("border: none; background: transparent;");
    close_button->setIcon(QIcon(assets::images[assets::Image::CloseIcon]));
    close_button->setIconSize(layout::chrome::close_icon(w));
    close_button->setGeometry(layout::chrome::close(w));
    connect(close_button, &QPushButton::clicked, this, [this]() { close(); });

    minimize_button = new QPushButton(this);
    minimize_button->setFlat(true);
    minimize_button->setCursor(Qt::PointingHandCursor);
    minimize_button->setStyleSheet("border:none; background:transparent; padding:0; margin:0;");
    minimize_button->setIcon(QIcon(assets::images[assets::Image::Minimize]));
    minimize_button->setIconSize(layout::chrome::minimize_icon(w));
    minimize_button->setGeometry(layout::chrome::minimize(w));
    connect(minimize_button, &QPushButton::clicked, this, [this]() { showMinimized(); });
}

void MainWindow::setup_logo()
{
    const QSize w = size();
    const int width = layout::chrome::logo_width(w);

    const QPixmap scaled = assets::images[assets::Image::Logo]
        .scaledToWidth(width, Qt::SmoothTransformation);

    auto* logo = new QLabel(this);
    logo->setPixmap(scaled);
    logo->setStyleSheet("background: transparent;");

    const QPoint pos = layout::chrome::logo_pos(w);
    logo->setGeometry(pos.x(), pos.y(), width, scaled.height());
}

void MainWindow::setup_version_label()
{
    const QSize w = size();
    const auto version_label = new QLabel("VERSION 0.1.0", this);

    QFont f = assets::fonts[assets::Font::EurostileExtraBlack];
    f.setPixelSize(layout::scaled(layout::text::k_version, w));
    f.setWeight(QFont::Black);
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    version_label->setFont(f);
    version_label->setStyleSheet("color: #E4E8EA; background: transparent;");
    version_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    version_label->setGeometry(layout::chrome::version(w));
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
    download_box->move(layout::download::pos(w));

    connect(download_box, &DownloadBox::settings_requested, this, [this]()
    {
        settings_open = true;
        close_button->hide();
        minimize_button->hide();
        repaint();
        settings->hide();
        settings->show_over(this);
    });

    connect(download_box, &DownloadBox::download_triggered, this, [this]()
    {
        game_install->show_over(this);
        game_install->raise();
        close_button->raise();
        minimize_button->raise();
    });

    connect(settings, &Settings::closed, this, [this]()
    {
        settings_open = false;
        close_button->show();
        minimize_button->show();
        update();
    });
}

void MainWindow::setup_game_install()
{
    game_install = new GameInstall(this);
    game_install->hide();

    connect(game_install, &GameInstall::closed, this, [this]()
    {
        // Maybe re-enable download button if needed
    });

    connect(game_install, &GameInstall::closed, this, [this]()
    {
        game_install->hide();
        if (!settings_open)
        {
            close_button->show();
            minimize_button->show();
            update();
        }
    });
}

void MainWindow::paintEvent(QPaintEvent*)
{
    const QSize w = size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Background always drawn.
    const QRect bg = layout::region::rect(w);
    const int radius = layout::scaled(layout::region::k_radius, w);
    QPainterPath path;
    path.addRoundedRect(bg, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(bg, assets::images[assets::Image::Background]);
    painter.setClipping(false);

    // Frames + sidebar icons hidden while settings is open
    if (!settings_open)
    {
        const QPixmap left = assets::images[assets::Image::LeftFrame]
            .scaledToHeight(height(), Qt::SmoothTransformation);
        painter.drawPixmap(0, 0, left);

        const QPixmap right = assets::images[assets::Image::RightFrame]
            .scaledToHeight(height(), Qt::SmoothTransformation);
        painter.drawPixmap(width() - right.width(), 0, right);

        painter.drawPixmap(layout::chrome::pt_icon(w),   assets::images[assets::Image::IconPT]);
        painter.drawPixmap(layout::chrome::lock_icon(w), assets::images[assets::Image::IconLock]);
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
