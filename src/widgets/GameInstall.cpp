#include "widgets/GameInstall.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

namespace
{
    QPixmap blur_pixmap(const QPixmap& src, qreal radius)
    {
        QGraphicsScene scene;
        auto* item = new QGraphicsPixmapItem(src);
        auto* blur = new QGraphicsBlurEffect;
        blur->setBlurRadius(radius);
        item->setGraphicsEffect(blur);
        scene.addItem(item);

        QPixmap out(src.size());
        out.fill(QColor(0xEA, 0xF2, 0xF7));
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        scene.render(&p, QRectF(), QRectF(src.rect()));
        return out;
    }
}

void GameInstall::show_over(QWidget* background)
{
    const QPixmap snap = background->grab();
    blurred_bg = blur_pixmap(snap, layout::scaled(10, size()));
    show();
    raise();
}

GameInstall::GameInstall(QWidget* parent) : QWidget(parent)
{
    const QSize w = window()->size();
    setFixedSize(window()->size());
    //setFixedSize(layout::install_modal::box(w));
    //move(layout::install_modal::box_rect(w).topLeft());
    
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

    close_button->setIcon(QIcon(assets::images[assets::Image::CloseSettings])); 
    close_button->setIconSize(layout::install_modal::close_icon(w));
    close_button->setGeometry(layout::install_modal::close(w));
    
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

    // Cancel button
    cancel_button = new QPushButton(this);
    cancel_button->setFlat(true);
    cancel_button->setCursor(Qt::PointingHandCursor);
    cancel_button->setStyleSheet("border:none; background:transparent;");
    cancel_button->setIcon(QIcon(assets::buttons[assets::Button::Cancel].normal));
    const QRect cancel_rect = layout::install_modal::cancel_button(w);
    cancel_button->setIconSize(cancel_rect.size());
    cancel_button->setGeometry(cancel_rect);

    connect(cancel_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    // Install button
    install_button = new QPushButton(this);
    install_button->setFlat(true);
    install_button->setCursor(Qt::PointingHandCursor);
    install_button->setStyleSheet("border:none; background:transparent;");
    install_button->setIcon(QIcon(assets::buttons[assets::Button::Install].normal));
    const QRect install_rect = layout::install_modal::install_button(w);
    install_button->setIconSize(install_rect.size());
    install_button->setGeometry(install_rect);

    connect(install_button, &QPushButton::clicked, this, []()
    {
        // TODO: Trigger actual install logic here
    });

    cancel_button->raise();
    install_button->raise();

    // Change path link
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
    const QRect cp_row = layout::install_modal::changepath_line(w);
    const int cp_hit_w = qMin(cp_row.width() / 3, 150);
    change_path_button->setGeometry(cp_row.left(), cp_row.top(), cp_hit_w, cp_row.height() + 4);

    connect(change_path_button, &QPushButton::clicked, this, []
    {
        // TODO: Open path picker
    });

    change_path_button->raise();
}

void GameInstall::paintEvent(QPaintEvent*)
{
    const QSize w = window()->size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Blurred Background 
    if (!blurred_bg.isNull())
    {
        // Clip to the rounded region
        const QRect bg = layout::region::rect(w);
        const int radius = layout::scaled(layout::region::k_radius, w);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(bg), radius, radius);
        painter.setClipPath(clip);

        // Draw the blurred snapshot
        painter.drawPixmap(rect(), blurred_bg);

        // Draw the frosty wash (white with alpha)
        painter.fillRect(rect(), QColor(255, 255, 255, 70)); // Adjust alpha (70) if needed

        painter.setClipping(false);
    }

    // Frames + sidebar icons drawn SHARP on top of the blur (mirrors MainWindow, so they
    // stay crisp instead of being captured into the frosted snapshot).
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

    // Draw Background Box
    const QRect box = layout::install_modal::box_rect(w);
    painter.drawPixmap(box, assets::images[assets::Image::BoxGameInstall]);

    // Title
    QFont title_font = assets::fonts[assets::Font::EurostileBlack];
    title_font.setPixelSize(layout::scaled(layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));

    const QRect title_rect = layout::install_modal::title(w);
    painter.drawText(title_rect, Qt::AlignCenter, "GAME INSTALLATION");

    // Body Text
    QFont body_font = assets::fonts[assets::Font::Inter];
    body_font.setPixelSize(layout::scaled(layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(QColor(0x39, 0x25, 0x18));
    const QRect body_rect = layout::install_modal::body(w);
    const QString body_text = "The game will be installed in the selected directory. You can keep the default path or choose a custom one.";
    painter.drawText(body_rect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, body_text);

    // Drawing the path field background box
    const QPixmap& path_bg = assets::images[assets::Image::InstallPath]; 
    const QRect path_rect = layout::install_modal::path_field(w);
    painter.drawPixmap(path_rect, path_bg);

    // DEFAULT INSTALLATION PATH
    QFont caption_font = assets::fonts[assets::Font::Inter];
    caption_font.setPixelSize(layout::scaled(layout::text::k_desc, w));
    caption_font.setWeight(QFont::Normal);
    painter.setFont(caption_font);
    painter.setPen(QColor(0x98, 0x87, 0x76)); // #988776
    painter.drawText(path_rect.adjusted(10, 8, -10, 0), Qt::AlignTop | Qt::AlignLeft, "DEFAULT INSTALLATION PATH");

    // Path value
    QFont path_font = assets::fonts[assets::Font::EurostileBold];
    path_font.setPixelSize(layout::scaled(16, w));
    path_font.setWeight(QFont::ExtraBold);
    painter.setFont(path_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(path_rect.adjusted(14, 34, -14, 0), Qt::AlignTop | Qt::AlignLeft, "/home/user/games/soa");

    const QRect change_path_rect = layout::install_modal::changepath_line(w);

    // Disk space note
    QFont note_font = assets::fonts[assets::Font::Inter];
    note_font.setPixelSize(layout::scaled(layout::text::k_label, w));
    note_font.setWeight(QFont::Medium);
    painter.setFont(note_font);
    painter.setPen(QColor(0x98, 0x87, 0x76)); // #988776
    painter.drawText(change_path_rect, Qt::AlignRight | Qt::AlignVCenter, "~ 2 GB of free disk space required.");
}

bool GameInstall::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == cancel_button || obj == install_button || obj == change_path_button) // Removed close_button here
    {
        const auto& cancel   = assets::buttons[assets::Button::Cancel];
        const auto& install  = assets::buttons[assets::Button::Install];

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
                    if (auto* btn = dynamic_cast<QPushButton*>(obj); btn->underMouse())
                        btn->setIcon(QIcon(b.hover));
                    else
                        btn->setIcon(QIcon(b.normal));
                }
                break;

            default: break;
        }
    }
    return QWidget::eventFilter(obj, event);
}