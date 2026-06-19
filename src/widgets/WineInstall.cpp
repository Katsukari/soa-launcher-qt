#include "widgets/WineInstall.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>
#include <QApplication>
#include <QFileDialog>
#include <QGraphicsBlurEffect>

#include "core/wine/Shell.hpp"

WineInstall::WineInstall(core::wine::Shell* shell_, QWidget* parent) : ModalOverlay(parent), shell(shell_)
{
    game_path = shell->wine_prefix();

    setup_close_button();
    setup_buttons();

    close_button->installEventFilter(this);
    cancel_button->installEventFilter(this);
    install_button->installEventFilter(this);
}

void WineInstall::setup_close_button()
{
    const QSize w = window()->size();
    close_button = new QPushButton(this);
    close_button->setFlat(true);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setStyleSheet("border:none; background:transparent;");

    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings])); 
    close_button->setIconSize(util::layout::install_modal::close_icon(w));
    close_button->setGeometry(util::layout::install_modal::close(w));
    
    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    close_button->raise();
}

void WineInstall::setup_buttons()
{
    const QSize w = window()->size();

    // Cancel button
    cancel_button = new QPushButton(this);
    cancel_button->setFlat(true);
    cancel_button->setCursor(Qt::PointingHandCursor);
    cancel_button->setStyleSheet("border:none; background:transparent;");
    cancel_button->setIcon(QIcon(util::assets::buttons[util::assets::Button::Cancel].normal));
    const QRect cancel_rect = util::layout::install_modal::cancel_button(w);
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
    install_button->setIcon(QIcon(util::assets::buttons[util::assets::Button::Install].normal));
    const QRect install_rect = util::layout::install_modal::install_button(w);
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
    const QRect cp_row = util::layout::install_modal::changepath_line(w);
    const int cp_hit_w = qMin(cp_row.width() / 3, 150);
    change_path_button->setGeometry(cp_row.left(), cp_row.top(), cp_hit_w, cp_row.height() + 4);

    connect(change_path_button, &QPushButton::clicked, this, [this]
    {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select Wine Prefix Location");
        if (!dir.isEmpty())
        {
            shell->set_root_path(dir);
            game_path = shell->wine_prefix();
            update();
        }
    });

    change_path_button->raise();
}

void WineInstall::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    // Box
    const QRect box = util::layout::install_modal::box_rect(w);
    painter.drawPixmap(box, util::assets::images[util::assets::Image::BoxGameInstall]);

    // Title
    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(util::layout::install_modal::title(w), Qt::AlignCenter, "WINE PREFIX INSTALLATION");

    // Body
    QFont body_font = util::assets::fonts[util::assets::Font::Inter];
    body_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(QColor(0x39, 0x25, 0x18));
    painter.drawText(util::layout::install_modal::body(w), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
    "The wine prefix will be installed in the selected directory. You can keep the default path or choose a custom one.");

    // Path field box
    const QRect path_rect = util::layout::install_modal::path_field(w);
    painter.drawPixmap(path_rect, util::assets::images[util::assets::Image::InstallPath]);

    // Caption
    QFont caption_font = util::assets::fonts[util::assets::Font::Inter];
    caption_font.setPixelSize(util::layout::scaled(util::layout::text::k_desc, w));
    caption_font.setWeight(QFont::Normal);
    painter.setFont(caption_font);
    painter.setPen(QColor(0x98, 0x87, 0x76));
    painter.drawText(path_rect.adjusted(10, 8, -10, 0), Qt::AlignTop | Qt::AlignLeft, "DEFAULT INSTALLATION PATH");

    // Path value
    QFont path_font = util::assets::fonts[util::assets::Font::EurostileBold];
    path_font.setPixelSize(util::layout::scaled(16, w));
    path_font.setWeight(QFont::ExtraBold);
    painter.setFont(path_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(path_rect.adjusted(14, 34, -14, 0), Qt::AlignTop | Qt::AlignLeft, game_path);

    // Disk note
    // QFont note_font = util::assets::fonts[util::assets::Font::Inter];
    // note_font.setPixelSize(util::layout::scaled(util::layout::text::k_label, w));
    // note_font.setWeight(QFont::Medium);
    // painter.setFont(note_font);
    // painter.setPen(QColor(0x98, 0x87, 0x76));
    // painter.drawText(util::layout::install_modal::changepath_line(w), Qt::AlignRight | Qt::AlignVCenter, "~ 2 GB of free disk space required.");
}

bool WineInstall::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == cancel_button || obj == install_button || obj == change_path_button)
    {
        const auto& cancel   = util::assets::buttons[util::assets::Button::Cancel];
        const auto& install  = util::assets::buttons[util::assets::Button::Install];

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
                    if (auto* button = dynamic_cast<QPushButton*>(obj); button->underMouse())
                        button->setIcon(QIcon(b.hover));
                    else
                        button->setIcon(QIcon(b.normal));
                }
                break;

            default: break;
        }
    }
    return QWidget::eventFilter(obj, event);
}