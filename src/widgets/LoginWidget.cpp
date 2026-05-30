#include "widgets/LoginWidget.hpp"

LoginWidget::LoginWidget(QWidget* parent) : QWidget(parent)
{
    setup_logo();
    setup_close_buttons();
    setup_version_label();
    setup_download_widget();
}

void LoginWidget::setup_logo()
{
    const auto& logo_px = assets::images[assets::Image::Logo];
    QPixmap scaled = logo_px.scaledToWidth(390, Qt::SmoothTransformation);

    auto* logo = new QLabel(this);
    logo->setPixmap(scaled);
    logo->setAlignment(Qt::AlignHCenter);
    logo->setGeometry(30, 59, 1350, scaled.height());
}

void LoginWidget::setup_close_buttons()
{
    // Close button — top-right on the frame edge.
    close_button = new QPushButton(this);
    close_button->setFlat(true);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setStyleSheet("border: none; background: transparent;");

    const auto& close_px = assets::images[assets::Image::CloseIcon];
    close_button->setIcon(QIcon(close_px));
    close_button->setIconSize(QSize(35, 35));
    close_button->setGeometry(1315, 40, 40, 40);

    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        window()->close();
    });

    // Minimize button — to the left of close.
    minimize_button = new QPushButton(this);
    minimize_button->setFlat(true);
    minimize_button->setCursor(Qt::PointingHandCursor);
    minimize_button->setStyleSheet("border: none; background: transparent;");

    const auto& min_px = assets::images[assets::Image::Minimize];
    minimize_button->setIcon(QIcon(min_px));
    minimize_button->setIconSize(QSize(50, 50));
    minimize_button->setStyleSheet("border:none; background:transparent; padding:0; margin:0;");
    minimize_button->setGeometry(1270, 37, 50, 50);

    connect(minimize_button, &QPushButton::clicked, this, [this]()
    {
        window()->showMinimized();
    });
}

void LoginWidget::setup_version_label()
{
    version_label = new QLabel("VERSION 0.1.0", this);

    QFont version_font = assets::fonts[assets::Font::EurostileExtraBlack];
    version_font.setPixelSize(16);
    version_font.setWeight(QFont::Black);
    version_font.setLetterSpacing(QFont::PercentageSpacing, 108);

    version_label->setFont(version_font);
    version_label->setStyleSheet("color: #E4E8EA; background: transparent;");
    version_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    version_label->setGeometry(1400 - 300, 846 - 55 - 20, 245, 20);
}

void LoginWidget::setup_download_widget()
{
    download_widget = new DownloadWidget(this);
    download_widget->move(388, 365);
}

void LoginWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.fillRect(rect(), Qt::black);

    // Background, rounded corners. top 22, bottom 30, left 30, right 20, radius 35.
    QRect bg_rect(30, 22, width() - 50, height() - 52);
    QPainterPath bg_path;
    bg_path.addRoundedRect(bg_rect, 35, 35);
    painter.setClipPath(bg_path);
    painter.drawPixmap(bg_rect, assets::images[assets::Image::Background]);
    painter.setClipping(false);

    QPixmap left = assets::images[assets::Image::LeftFrame]
        .scaledToHeight(height(), Qt::SmoothTransformation);
    painter.drawPixmap(0, 0, left);

    QPixmap right = assets::images[assets::Image::RightFrame]
        .scaledToHeight(height(), Qt::SmoothTransformation);
    painter.drawPixmap(width() - right.width(), 0, right);

    // Sidebar icons, on top of the frame.
    // TOP = camel "PT" icon (PT ICON.png), BOTTOM = lock (2.0 ICON.png).
    painter.drawPixmap(56, 495, assets::images[assets::Image::IconPT]);
    painter.drawPixmap(58, 613, assets::images[assets::Image::IconMain]);
}