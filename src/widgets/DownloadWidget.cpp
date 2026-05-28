#include "widgets/DownloadWidget.hpp"

DownloadWidget::DownloadWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(640, 320);
    setup_title();
    setup_settings_button();
    setup_message();
    setup_download_button();
}

void DownloadWidget::setup_title()
{
    title_label = new QLabel("PLAYTEST", this);
    title_label->setAlignment(Qt::AlignCenter);
    QFont title_font = assets::fonts[assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(25);
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet("color: #4F1717; background: transparent;");
    title_label->setGeometry(0, 38, width(), 30);
}

void DownloadWidget::setup_settings_button()
{
    settings_button = new QPushButton(this);
    settings_button->setFlat(true);
    settings_button->setCursor(Qt::PointingHandCursor);
    settings_button->setStyleSheet("border: none; background: transparent;");
    const auto& icon = assets::images[assets::Image::SettingsButton];
    const QPixmap scaled = icon.scaled(37, 37, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    settings_button->setIcon(QIcon(scaled));
    settings_button->setIconSize(QSize(37, 37));
    settings_button->setGeometry(width() - 25 - 37, 23, 37, 37);
}

void DownloadWidget::setup_message()
{
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
    // Background drawn in paintEvent
    message_label->setStyleSheet("color: #4F1717; background: transparent; padding: 0px 12px;");
    message_label->setGeometry(105, 103, 440, 70);
}

void DownloadWidget::setup_download_button()
{
    download_button = new QPushButton(this);
    download_button->setFlat(true);
    download_button->setCursor(Qt::PointingHandCursor);
    download_button->setText("");
    download_button->setStyleSheet("border: none; background: transparent;");

    const QPixmap& normal = assets::buttons[assets::Button::DownloadGame].normal;
    const int w = 440;
    const int h = qRound(w * static_cast<double>(normal.height()) / normal.width());

    download_button->setIcon(QIcon(normal));
    download_button->setIconSize(QSize(w, h));
    download_button->setGeometry(105, 198, w, h);
    download_button->installEventFilter(this);
}

void DownloadWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.drawPixmap(0, 0, width(), height(), assets::images[assets::Image::BoxCard]);
    // Note box behind the message — same rect as message_label.
    painter.drawPixmap(105, 103, 440, 70, assets::images[assets::Image::BoxNote2]);
}

bool DownloadWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == download_button) {
        const auto& btn = assets::buttons[assets::Button::DownloadGame];
        switch (event->type()) {
            case QEvent::Enter:              download_button->setIcon(QIcon(btn.hover));   break;
            case QEvent::Leave:              download_button->setIcon(QIcon(btn.normal));  break;
            case QEvent::MouseButtonPress:   download_button->setIcon(QIcon(btn.clicked)); break;
            case QEvent::MouseButtonRelease:
                download_button->setIcon(QIcon(download_button->underMouse() ? btn.hover : btn.normal));
                break;
            default: break;
        }
    }
    return QWidget::eventFilter(obj, event);
}
