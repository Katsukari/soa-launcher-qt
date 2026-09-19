#include "ui/RepairFiles.hpp"
#include "i18n/LanguageManager.hpp"

#include "common/GameVersion.hpp"
#include "ui/Assets.hpp"
#include "ui/Colors.hpp"
#include "config/Config.hpp"
#include "ui/Layout.hpp"
#include "ui/SimpleUtils.hpp"

#include <QIcon>
#include <QPainter>
#include <QFileInfo>
#include <QPushButton>

using soa::config::Config;

namespace
{
    constexpr QSize k_box_size {560, 392};

    QRect box_rect(const QSize window_size)
    {
        return soa::ui::layout::centered(k_box_size, window_size, 0, 35);
    }

    QRect local_rect(const QSize window_size, const QRect source)
    {
        return soa::ui::layout::scaled(source, window_size).translated(box_rect(window_size).topLeft());
    }
}

RepairFiles::RepairFiles(QWidget* parent)
    : ModalOverlay(parent)
{
    setup_buttons();
    refresh();
}

void RepairFiles::refresh()
{
    detected_message.clear();
    game_version = Config::instance().game_version();
    install_path = Config::instance().game_install_path(game_version);
    repair_button->setEnabled(QFileInfo::exists(install_path)
        && Config::instance().path_inside_prefix(install_path));
    update();
}

void RepairFiles::set_game_version(const soa::common::game::GameVersion version)
{
    game_version = version;
    install_path = Config::instance().game_install_path(version);
    repair_button->setEnabled(QFileInfo::exists(install_path)
        && Config::instance().path_inside_prefix(install_path));
    update();
}

void RepairFiles::set_detected_changes(const QStringList& paths)
{
    if (paths.isEmpty())
    {
        detected_message.clear();
    }
    else if (paths.size() == 1)
    {
        detected_message = QStringLiteral("The launcher detected a protected file change: %1")
            .arg(paths.front());
    }
    else
    {
        detected_message = QStringLiteral("The launcher detected %1 protected file changes.")
            .arg(paths.size());
    }
    update();
}

void RepairFiles::setup_buttons()
{
    const QSize w = window()->size();

    close_button = soa::ui::simple_utils::make_flat_button(this);
    close_button->setIcon(QIcon(soa::ui::assets::images[soa::ui::assets::Image::CloseSettings]));
    close_button->setIconSize(soa::ui::layout::scaled(soa::ui::layout::modal_close::k_icon, w));
    close_button->setGeometry(
        local_rect(w, soa::ui::layout::modal_close::rect_in({0, 0, k_box_size.width(),
                                                          k_box_size.height()})));
    close_button->setAccessibleName(QStringLiteral("Close repair window"));
    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    const auto& cancel = soa::ui::assets::button(soa::ui::assets::Button::Cancel);
    const QSize cancel_size = soa::ui::layout::scaled(cancel.normal.size(), w);
    cancel_button = soa::ui::simple_utils::make_flat_button(this);
    cancel_button->setIcon(QIcon(cancel.normal));
    cancel_button->setIconSize(cancel_size);
    cancel_button->setGeometry(local_rect(w, {70, 315, 193, 41}));
    QFont cancel_font = soa::ui::assets::fonts[soa::ui::assets::Font::EurostileExtraBlack];
    cancel_font.setPixelSize(soa::ui::layout::scaled(12, w));
    cancel_font.setWeight(QFont::Black);
    soa::ui::simple_utils::add_button_text(cancel_button, soa::ui::assets::Button::Cancel, QStringLiteral("CANCEL"), cancel_font);
    cancel_button->setAccessibleName(QStringLiteral("Cancel repair"));
    cancel_button->installEventFilter(this);
    connect(cancel_button, &QPushButton::clicked, this, [this]()
    {
        hide();
        emit closed();
    });

    const auto& repair = soa::ui::assets::button(soa::ui::assets::Button::Repair);
    const QSize repair_size = soa::ui::layout::scaled(repair.normal.size(), w);
    repair_button = soa::ui::simple_utils::make_flat_button(this);
    repair_button->setIcon(QIcon(repair.normal));
    repair_button->setIconSize(repair_size);
    repair_button->setGeometry(local_rect(w, {298, 315, 193, 41}));
    QFont repair_font = soa::ui::assets::fonts[soa::ui::assets::Font::EurostileExtraBlack];
    repair_font.setPixelSize(soa::ui::layout::scaled(12, w));
    repair_font.setWeight(QFont::Black);
    soa::ui::simple_utils::add_button_text(repair_button, soa::ui::assets::Button::Repair, QStringLiteral("REPAIR FILES"), repair_font);
    repair_button->setAccessibleName(QStringLiteral("Verify and repair files"));
    repair_button->installEventFilter(this);
    connect(repair_button, &QPushButton::clicked, this, [this]()
    {
        if (!repair_button->isEnabled())
            return;
        emit repair_requested();
    });
}

void RepairFiles::paint_content(QPainter& painter)
{
    const QSize w = window()->size();
    const QRect box = box_rect(w);
    painter.drawPixmap(box, soa::ui::assets::images[soa::ui::assets::Image::BoxModal]);

    QFont title_font = soa::ui::assets::fonts[soa::ui::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(soa::ui::layout::scaled(25, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(soa::ui::colors::k_text_maroon);
    painter.drawText(local_rect(w, {30, 36, 500, 34}), Qt::AlignCenter,
                     soa::i18n::translate("VERIFY AND REPAIR GAME"));

    QFont body_font = soa::ui::assets::fonts[soa::ui::assets::Font::Inter];
    body_font.setPixelSize(soa::ui::layout::scaled(14, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(soa::ui::colors::k_text_body);
    const QString game_name = QString::fromLatin1(soa::common::game::profile(game_version).display_name);
    painter.drawText(local_rect(w, {45, 80, 470, 44}),
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     soa::i18n::translate("The launcher will verify every %1 file against the current CDN manifest.")
                         .arg(game_name));

    const QRect path_box = local_rect(w, {45, 139, 470, 57});
    painter.drawPixmap(path_box, soa::ui::assets::images[soa::ui::assets::Image::IntegrityCheckFile]);

    QFont label_font = soa::ui::assets::fonts[soa::ui::assets::Font::Inter];
    label_font.setPixelSize(soa::ui::layout::scaled(13, w));
    label_font.setWeight(QFont::DemiBold);
    painter.setFont(label_font);
    painter.setPen(soa::ui::colors::k_text_maroon);
    const QString elided = painter.fontMetrics().elidedText(
        install_path, Qt::ElideMiddle, path_box.width() - soa::ui::layout::scaled(36, w));
    painter.drawText(path_box.adjusted(soa::ui::layout::scaled(18, w), 0,
                                       -soa::ui::layout::scaled(18, w), 0),
                     Qt::AlignVCenter | Qt::AlignLeft, elided);

    const QRect note_box = local_rect(w, {45, 220, 470, 72});
    painter.drawPixmap(note_box, soa::ui::assets::images[soa::ui::assets::Image::BoxNote]);
    QFont note_font = soa::ui::assets::fonts[soa::ui::assets::Font::Inter];
    note_font.setPixelSize(soa::ui::layout::scaled(13, w));
    note_font.setWeight(QFont::Medium);
    painter.setFont(note_font);
    painter.setPen(soa::ui::colors::k_text_body);
    painter.drawText(note_box.adjusted(soa::ui::layout::scaled(18, w), soa::ui::layout::scaled(12, w),
                                       -soa::ui::layout::scaled(18, w), -soa::ui::layout::scaled(10, w)),
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                     detected_message.isEmpty()
                         ? soa::i18n::translate("Missing or damaged files will be downloaded again. Valid files and resumable partial downloads are kept, so the repair does not restart the whole game.")
                         : soa::i18n::translate("%1 Verify and repair before launching again.").arg(detected_message));
}

bool RepairFiles::eventFilter(QObject* object, QEvent* event)
{
    if (object == cancel_button)
    {
        const auto& asset = soa::ui::assets::button(soa::ui::assets::Button::Cancel);
        soa::ui::simple_utils::apply_button_state(
            event, cancel_button, asset.normal, asset.hover, asset.clicked);
    }
    else if (object == repair_button && repair_button->isEnabled())
    {
        const auto& asset = soa::ui::assets::button(soa::ui::assets::Button::Repair);
        soa::ui::simple_utils::apply_button_state(
            event, repair_button, asset.normal, asset.hover, asset.clicked);
    }
    return QWidget::eventFilter(object, event);
}
