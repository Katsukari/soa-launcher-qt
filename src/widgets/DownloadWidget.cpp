#include "widgets/DownloadWidget.hpp"

DownloadWidget::DownloadWidget(QWidget* parent)
    : QWidget(parent)
{
    // React: width 640, height 320 (in download/install state).
    setFixedSize(640, 320);

    setup_title();
    setup_settings_button();
    setup_message();
    setup_download_button();
}

void DownloadWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // React: backgroundImage boxCardImg, backgroundSize '640px 320px'.
    const auto& box = assets::images[assets::Image::BoxCard];
    painter.drawPixmap(0, 0, width(), height(), box);
}

void DownloadWidget::setup_title()
{
    // React: Eurostile, weight 900, 25px, color #4F1717, uppercase,
    // centered, margin-bottom 35px.
    title_label = new QLabel("PLAYTEST", this);
    title_label->setAlignment(Qt::AlignCenter);

    QFont title_font = assets::fonts[assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(25);
    title_font.setWeight(QFont::Black); // Force weight 900

    title_label->setFont(title_font);
    title_label->setStyleSheet("color: #4F1717; background: transparent;");
    // Card padding-top is 38px. Title spans full card width.
    title_label->setGeometry(0, 38, width(), 30);
}

void DownloadWidget::setup_settings_button()
{
    // React: absolute top 23px, right 25px from card, 37x37.
    settings_button = new QPushButton(this);
    settings_button->setFlat(true);
    settings_button->setCursor(Qt::PointingHandCursor);
    settings_button->setStyleSheet("border: none; background: transparent;");

    const auto& icon = assets::images[assets::Image::SettingsButton];
    QPixmap scaled = icon.scaled(37, 37, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    settings_button->setIcon(QIcon(scaled));
    settings_button->setIconSize(QSize(37, 37));
    settings_button->setGeometry(width() - 25 - 37, 23, 37, 37);
}

void DownloadWidget::setup_message()
{
    // React: box-note2.png background, 430x70, Inter weight 500, 13px,
    // color #4F1717, centered, padding 0 12px.
    // Positioned inside card padding (95px left) + 10px margin = 105px from card left.
    // Vertically: after title (38 + 30) + margin-bottom 35 = 103.
    message_label = new QLabel(this);
    message_label->setText(
        "Welcome to Story of Alicia. To participate in the playtest, you\n"
        "have to first download the game.");
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);

    QFont msg_font = assets::fonts[assets::Font::Inter];
    msg_font.setPixelSize(13);
    msg_font.setWeight(QFont::Medium);

    message_label->setFont(msg_font);

    // Use box-note2.png as the background via stylesheet border-image.
    message_label->setStyleSheet(
        "color: #4F1717;"
        "background: transparent;"
        "border-image: url(:/assets/box-note2.png) 0 0 0 0 stretch stretch;"
        "padding: 0px 12px;"
    );
    message_label->setGeometry(105, 103, 440, 70);
}

void DownloadWidget::setup_download_button()
{
    download_button = new QPushButton(this);
    download_button->setCursor(Qt::PointingHandCursor);
    download_button->setText("");
    download_button->setStyleSheet(
        "QPushButton {"
        "   border: none; background: transparent;"
        "   border-image: url(:/assets/btn-download-game-normal.png) 0 0 0 0 stretch stretch;"
        "}"
        "QPushButton:hover {"
        "   border-image: url(:/assets/btn-download-game-hover.png) 0 0 0 0 stretch stretch;"
        "}"
        "QPushButton:pressed {"
        "   border-image: url(:/assets/btn-download-game-clicked.png) 0 0 0 0 stretch stretch;"
        "}"
    );

    QPixmap px(":/assets/btn-download-game-normal.png");
    const int w = 440;
    const int h = qRound(w * static_cast<double>(px.height()) / px.width());
    download_button->setGeometry(105, 198, w, h);
}
