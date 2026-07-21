#include "widgets/WineSelectMenu.hpp"

#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFrame>
#include <QPainter>

#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/Styles.hpp"
#include "util/Colors.hpp"
#include "util/SimpleUtils.hpp"
#include "core/wine/WineRegistry.hpp"
#include "util/Config.hpp"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

using util::config::Config;
namespace cw = core::wine;
namespace im = util::layout::install_modal;

namespace
{
    const char* k_row =
        "QPushButton { text-align: left; padding: 10px 14px; border: 1px solid #C9BBAA;"
        "    border-radius: 8px; background: #FFFFFF; color: #392518;"
        "    font-family: 'Inter'; font-size: 14px; }"
        "QPushButton:hover { border-color: #2FB4E0; }"
        "QPushButton:checked { border: 2px solid #2FB4E0; background: #EAF7FC; }";
    const char* k_scroll =
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 8px; background: transparent; margin: 0; }"
        "QScrollBar::handle:vertical { background: #C9BBAA; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }";
    const char* k_status =
        "QLabel { color: #6B5B4D; font-family: 'Inter'; font-size: 12px; background: transparent; }";
    const char* k_empty =
        "QLabel { color: #8A7A6B; font-family: 'Inter'; font-size: 13px; padding: 24px;"
        "    background: transparent; }";
}

WineSelectMenu::WineSelectMenu(QWidget* parent) : ModalOverlay(parent)
{
    build_ui();
    runtimes = cw::WineRegistry::scan();
    populate();
    relayout();
}

void WineSelectMenu::build_ui()
{
    close_button = util::simple_utils::make_flat_button(this);
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    connect(close_button, &QPushButton::clicked, this, [this]() { hide(); emit closed(); });

    tricks_status = new QLabel(this);
    tricks_status->setStyleSheet(k_status);
    tricks_status->setWordWrap(true);

    list = new QScrollArea(this);
    list->setWidgetResizable(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setFrameShape(QFrame::NoFrame);
    list->setStyleSheet(k_scroll);

    rescan_button = new QPushButton("Rescan", this);
    rescan_button->setFlat(true);
    rescan_button->setCursor(Qt::PointingHandCursor);
    rescan_button->setStyleSheet(util::styles::k_link_blue_lg);
    connect(rescan_button, &QPushButton::clicked, this, &WineSelectMenu::rescan);

    continue_button = new QPushButton("Continue", this);
    continue_button->setFlat(true);
    continue_button->setCursor(Qt::PointingHandCursor);
    continue_button->setStyleSheet(util::styles::k_link_blue_lg);
    continue_button->setEnabled(false);
    connect(continue_button, &QPushButton::clicked, this, &WineSelectMenu::confirm);

    close_button->raise();
    rescan_button->raise();
    continue_button->raise();
}

void WineSelectMenu::populate()
{
    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    rows.clear();
    selected = -1;
    continue_button->setEnabled(false);

    for (int i = 0; i < runtimes.size(); ++i)
    {
        const cw::WineInstall& wi = runtimes[i];
        const QString type = wi.type == cw::RuntimeType::Proton ? "Proton" : "Wine";

        auto* row = new QPushButton(QString("%1   ·   %2\n%3").arg(wi.name, type, wi.path), content);
        row->setCheckable(true);
        row->setCursor(Qt::PointingHandCursor);
        row->setStyleSheet(k_row);
        connect(row, &QPushButton::clicked, this, [this, i]() { select_row(i); });

        lay->addWidget(row);
        rows.push_back(row);
    }

    if (runtimes.isEmpty())
    {
        auto* empty = new QLabel("No Wine or Proton runtimes were found on this system.", content);
        empty->setStyleSheet(k_empty);
        empty->setWordWrap(true);
        lay->addWidget(empty);
    }

    lay->addStretch(1);
    list->setWidget(content);

    const bool tricks = cw::winetricks_available();
    tricks_status->setText(tricks
        ? "winetricks: ready"
        : "winetricks not found - required components will be installed manually");
}

void WineSelectMenu::rescan()
{
    runtimes = cw::WineRegistry::scan();
    populate();
}

void WineSelectMenu::select_row(int index)
{
    selected = index;
    for (int i = 0; i < rows.size(); ++i) rows[i]->setChecked(i == index);
    continue_button->setEnabled(index >= 0 && index < runtimes.size());
}

void WineSelectMenu::confirm()
{
    if (selected < 0 || selected >= runtimes.size()) return;

    const cw::WineInstall& wi = runtimes[selected];
    Config::instance().set_wine_binary(wi.path);
    Config::instance().set_runtime_selected(true);
    SPDLOG_INFO("runtime selected: {} ({})", wi.name.toStdString(), wi.path.toStdString());
    emit runtime_chosen();
}

void WineSelectMenu::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    const QRect box = im::box_rect(w);
    painter.drawPixmap(box, util::assets::images[util::assets::Image::BoxGameInstall]);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(util::colors::k_text_maroon);
    painter.drawText(im::title(w), Qt::AlignCenter, "SELECT RUNTIME");

    QFont body_font = util::assets::fonts[util::assets::Font::Inter];
    body_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(util::colors::k_text_body);
    painter.drawText(im::body(w), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
        "Choose the Wine or Proton version used to run the game.");
}

void WineSelectMenu::relayout()
{
    const QSize w = window()->size();
    const QRect box = im::box_rect(w);

    close_button->setIconSize(im::close_icon(w));
    close_button->setGeometry(im::close(w));

    const int inset = util::layout::scaled(34, w);
    const QRect body = im::body(w);
    const int top = body.bottom() + util::layout::scaled(16, w);

    const int btn_h = util::layout::scaled(30, w);
    const int btn_y = box.bottom() - util::layout::scaled(30, w) - btn_h;

    const int status_h = util::layout::scaled(18, w);
    const int status_y = btn_y - util::layout::scaled(12, w) - status_h;

    list->setGeometry(box.left() + inset, top,
                      box.width() - 2 * inset,
                      qMax(0, status_y - top - util::layout::scaled(8, w)));

    tricks_status->setGeometry(box.left() + inset, status_y, box.width() - 2 * inset, status_h);

    const int btn_w = util::layout::scaled(150, w);
    continue_button->setGeometry(box.right() - inset - btn_w, btn_y, btn_w, btn_h);
    rescan_button->setGeometry(box.left() + inset, btn_y, btn_w, btn_h);
}
