#include "widgets/GameInstall.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QGraphicsBlurEffect>

GameInstall::GameInstall(QWidget* parent) : ModalOverlay(parent)
{
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

void GameInstall::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    // Box
    const QRect box = layout::install_modal::box_rect(w);
    painter.drawPixmap(box, assets::images[assets::Image::BoxGameInstall]);

    // Title
    QFont title_font = assets::fonts[assets::Font::EurostileBlack];
    title_font.setPixelSize(layout::scaled(layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(layout::install_modal::title(w), Qt::AlignCenter, "GAME INSTALLATION");

    // Body
    QFont body_font = assets::fonts[assets::Font::Inter];
    body_font.setPixelSize(layout::scaled(layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(QColor(0x39, 0x25, 0x18));
    painter.drawText(layout::install_modal::body(w),
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     "The game will be installed in the selected directory. You can keep the default path or choose a custom one.");

    // Path field box
    const QRect path_rect = layout::install_modal::path_field(w);
    painter.drawPixmap(path_rect, assets::images[assets::Image::InstallPath]);

    // Caption
    QFont caption_font = assets::fonts[assets::Font::Inter];
    caption_font.setPixelSize(layout::scaled(layout::text::k_desc, w));
    caption_font.setWeight(QFont::Normal);
    painter.setFont(caption_font);
    painter.setPen(QColor(0x98, 0x87, 0x76));
    painter.drawText(path_rect.adjusted(10, 8, -10, 0), Qt::AlignTop | Qt::AlignLeft, "DEFAULT INSTALLATION PATH");

    // Path value
    QFont path_font = assets::fonts[assets::Font::EurostileBold];
    path_font.setPixelSize(layout::scaled(16, w));
    path_font.setWeight(QFont::ExtraBold);
    painter.setFont(path_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(path_rect.adjusted(14, 34, -14, 0), Qt::AlignTop | Qt::AlignLeft, "/home/user/games/soa");

    // Disk note
    QFont note_font = assets::fonts[assets::Font::Inter];
    note_font.setPixelSize(layout::scaled(layout::text::k_label, w));
    note_font.setWeight(QFont::Medium);
    painter.setFont(note_font);
    painter.setPen(QColor(0x98, 0x87, 0x76));
    painter.drawText(layout::install_modal::changepath_line(w), Qt::AlignRight | Qt::AlignVCenter, "~ 2 GB of free disk space required.");
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