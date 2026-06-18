#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include "widgets/Settings.hpp"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>

#include "widgets/LauncherSettings.hpp"
#include "widgets/WineSettings.hpp"
#include "widgets/AdvancedSettings.hpp"
#include "util/Layout.hpp"
#include "spdlog/spdlog.h"


Settings::Settings(QWidget* parent) : ModalOverlay(parent)
{
    set_keeps_chrome(false);
    setup_pages();
    setup_close_button();
    setup_tabs();
}

void Settings::setup_pages()
{
    const QSize w = size();
    stack = new QStackedWidget(this);
    stack->setGeometry(layout::settings::box_rect(w));
    stack->setStyleSheet("background: transparent;");

    stack->addWidget(new LauncherSettings(stack));
    stack->addWidget(new WineSettings(stack));
    stack->addWidget(new AdvancedSettings(stack));
    stack->setCurrentIndex(0);
}

void Settings::setup_close_button()
{
    const QSize w = size();

    close_button = new QPushButton(this);
    close_button->setFlat(true);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setText("");
    close_button->setStyleSheet("border: none; background: transparent;");

    const auto & close_px = assets::images[assets::Image::CloseSettings];
    close_button->setIcon(QIcon(close_px));
    close_button->setIconSize(layout::settings::close_icon(w));
    close_button->setGeometry(layout::settings::close(w));
    close_button->raise();

    connect(close_button, &QPushButton::clicked, this, [this]() { hide(); emit closed(); });
}

void Settings::setup_tabs()
{
    const QSize w = size();

    auto make_tab = [&](const int i)
    {
        auto* b = new QPushButton(this);
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("border:none; background:transparent;");
        b->setGeometry(layout::settings::tab_rect(w, i));
        return b;
    };

    auto tab_general  = make_tab(0);
    auto tab_wine     = make_tab(1);
    auto tab_advanced = make_tab(2);

    connect(tab_general,  &QPushButton::clicked, this, [this]() { set_tab(0); });
    connect(tab_wine,     &QPushButton::clicked, this, [this]() { set_tab(1); });
    connect(tab_advanced, &QPushButton::clicked, this, [this]() { set_tab(2); });

    tab_general->raise();
    tab_wine->raise();
    tab_advanced->raise();
    close_button->raise();
}

void Settings::set_tab(const int index)
{
    active_tab = index;
    stack->setCurrentIndex(index);
    update();
}

void Settings::paint_content(QPainter& painter)
{
    const QSize w = size();
    const QRect box = layout::settings::box_rect(w);
    painter.drawPixmap(box, assets::images[assets::Image::BoxSettings]);

    // Title (text per tab)
    {
        QFont tf = assets::fonts[assets::Font::EurostileExtraBlack];
        tf.setPixelSize(layout::scaled(layout::text::k_modal_header, w));
        tf.setWeight(QFont::Black);
        painter.setFont(tf);
        painter.setPen(QColor(0x4F, 0x17, 0x17));
        const char* title = (active_tab == 1) ? "WINE SETTINGS" : (active_tab == 2) ? "ADVANCED SETTINGS": "LAUNCHER SETTINGS";
        const QRect title_rect(box.left(), box.top() + layout::scaled(30, w), box.width(), layout::scaled(30, w));
        painter.drawText(title_rect, Qt::AlignCenter, title);
    }

    // Tabs
    constexpr QColor active   {0xFB, 0xF6, 0xF0};
    constexpr QColor inactive {0xD8, 0xCD, 0xC0};
    constexpr QColor textCol  {0x4F, 0x17, 0x17};

    QFont f = assets::fonts[assets::Font::EurostileBlack];
    f.setPixelSize(layout::scaled(layout::text::k_label, w));
    f.setWeight(QFont::Black);
    painter.setFont(f);
    const int radius = layout::scaled(8, w);

    for (int i = 0; i < 3; ++i)
    {
        const char* labels[] = { "LAUNCHER", "WINE", "ADVANCED" };
        const QRect r = layout::settings::tab_rect(w, i);
        const bool  on = i == active_tab;
        QPainterPath p;

        // Round only the top corners so the bottom melds into the box.
        p.addRoundedRect(QRectF(r), radius, radius);
        painter.fillPath(p, on ? active : inactive);
        painter.setPen(on ? textCol : textCol.lighter(140));
        painter.drawText(r, Qt::AlignCenter, labels[i]);
    }
}