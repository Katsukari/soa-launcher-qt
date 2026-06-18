#include "widgets/DownloadBox.hpp"
#include "util/Layout.hpp"

DownloadBox::DownloadBox(QWidget* parent) : QWidget(parent)
{
    const QSize w = window()->size();
    setFixedSize(layout::download::box(w));

    setup_title();
    setup_settings_button();
    setup_message();
    setup_download_button();
}

void DownloadBox::setup_title()
{
    const QSize w = window()->size();

    const auto title_label = new QLabel("PLAYTEST", this);
    title_label->setAlignment(Qt::AlignCenter);

    QFont title_font = assets::fonts[assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(layout::scaled(layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet("color: #4F1717; background: transparent;");
    title_label->setGeometry(layout::scaled(layout::download::k_title, w));
}

void DownloadBox::setup_settings_button()
{
    const QSize w = window()->size();
    const QRect btn = layout::scaled(layout::download::k_settings_button, w);

    auto settings_button = new QPushButton(this);
    settings_button->setFlat(true);
    settings_button->setCursor(Qt::PointingHandCursor);
    settings_button->setStyleSheet("outline:none; border: none; background: transparent;");

    const QPixmap scaled = assets::images[assets::Image::SettingsButton]
        .scaled(btn.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    settings_button->setIcon(QIcon(scaled));
    settings_button->setIconSize(btn.size());
    settings_button->setGeometry(btn);

    connect(settings_button, &QPushButton::clicked, this, [this] { emit settings_requested(); });
}

void DownloadBox::setup_message()
{
    const QSize w = window()->size();

    const auto message_label = new QLabel(this);
    message_label->setText(
        "Welcome to Story of Alicia. To participate in the playtest, you\n"
        "have to first download the game.");
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);

    QFont msg_font = assets::fonts[assets::Font::Inter];
    msg_font.setPixelSize(layout::scaled(layout::text::k_body, w));
    msg_font.setWeight(QFont::Medium);
    message_label->setFont(msg_font);

    // Background drawn in paintEvent
    message_label->setStyleSheet("color: #4F1717; background: transparent; padding: 0px 12px;");
    message_label->setGeometry(layout::scaled(layout::download::k_note, w));
}

void DownloadBox::setup_download_button()
{
    const QSize w = window()->size();

    download_button = new QPushButton(this);
    download_button->setFlat(true);
    download_button->setCursor(Qt::PointingHandCursor);
    download_button->setText("");
    download_button->setStyleSheet("border: none; background: transparent;");

    const QPixmap& normal = assets::buttons[assets::Button::DownloadGame].normal;

    // Width from layout (scaled); height keeps the PNG's aspect ratio.
    const int bw = layout::scaled(layout::download::k_button_w, w);
    const int bh = qRound(bw * static_cast<double>(normal.height()) / normal.width());
    const int bx = layout::scaled(layout::download::k_button_x, w);
    const int by = layout::scaled(layout::download::k_button_y, w);

    download_button->setIcon(QIcon(normal));
    download_button->setIconSize(QSize(bw, bh));
    download_button->setGeometry(bx, by, bw, bh);
    download_button->installEventFilter(this);
}

void DownloadBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const QSize w = window()->size();

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.drawPixmap(rect(), assets::images[assets::Image::BoxCard]);
    painter.drawPixmap(layout::scaled(layout::download::k_note, w), assets::images[assets::Image::BoxNote2]);
}

bool DownloadBox::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == download_button)
    {
        const auto& btn = assets::buttons[assets::Button::DownloadGame];
        switch (event->type())
        {
            case QEvent::Enter:              download_button->setIcon(QIcon(btn.hover));   break;
            case QEvent::Leave:              download_button->setIcon(QIcon(btn.normal));  break;
            case QEvent::MouseButtonPress:   download_button->setIcon(QIcon(btn.clicked)); break;
            case QEvent::MouseButtonRelease:
                download_button->setIcon(QIcon(download_button->underMouse() ? btn.hover : btn.normal));
                if (download_button->underMouse()) emit download_triggered();
                break;
            default: break;
        }
    }
    return QWidget::eventFilter(obj, event);
}