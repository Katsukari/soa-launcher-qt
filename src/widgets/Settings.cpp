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
#include "util/Layout.hpp"

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

void Settings::show_over(QWidget* background)
{
    // Snapshot everything currently drawn in the background widget
    const QPixmap snap = background->grab();
    blurred_bg = blur_pixmap(snap, layout::scaled(10, size()));
    show();
    raise();
}

Settings::Settings(QWidget* parent) : QWidget(parent)
{
    setFixedSize(window()->size());
    setup_pages();
    setup_close_button();
    setup_tabs();
}

void Settings::setup_pages()
{
    const QSize w = size();
    stack = new QStackedWidget(this);

    QRect box = layout::settings::box_rect(w);
    box.setHeight(box.height() + layout::scaled(160, w));
    stack->setGeometry(box);
    stack->setStyleSheet("background: transparent;");

    stack->addWidget(new LauncherSettings(stack));
    stack->addWidget(new WineSettings(stack));
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

    auto tab_general = make_tab(0);
    auto tab_wine    = make_tab(1);

    connect(tab_general, &QPushButton::clicked, this, [this]() { set_tab(0); });
    connect(tab_wine,    &QPushButton::clicked, this, [this]() { set_tab(1); });

    tab_general->raise();
    tab_wine->raise();
    close_button->raise();
}

void Settings::set_tab(int index)
{
    active_tab = index;
    stack->setCurrentIndex(index);
    update();
}

void Settings::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const QSize w = size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Clip to the same rounded region MainWindow uses, so the black gutter
    // in the corners stays black instead of getting the frost wash.
    const QRect bg = layout::region::rect(w);
    const int bg_radius = layout::scaled(layout::region::k_radius, w);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(bg), bg_radius, bg_radius);
    painter.setClipPath(clip);

    if (!blurred_bg.isNull()) painter.drawPixmap(rect(), blurred_bg);
    painter.fillRect(rect(), QColor(255, 255, 255, 70));
    painter.setClipping(false);

    // Settings box
    painter.drawPixmap(layout::settings::box_rect(w), assets::images[assets::Image::BoxSettings]);

    // Tabs
    constexpr QColor active   {0xFB, 0xF6, 0xF0};
    constexpr QColor inactive {0xD8, 0xCD, 0xC0};
    constexpr QColor textCol  {0x4F, 0x17, 0x17};

    QFont f = assets::fonts[assets::Font::EurostileBlack];
    f.setPixelSize(layout::scaled(layout::text::k_label, w));
    f.setWeight(QFont::Black);
    painter.setFont(f);
    const int radius = layout::scaled(8, w);

    for (int i = 0; i < 2; ++i)
    {
        const char* labels[] = { "LAUNCHER", "WINE" };
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