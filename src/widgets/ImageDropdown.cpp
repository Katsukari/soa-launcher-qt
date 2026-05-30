#include "widgets/ImageDropdown.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>
#include <QMouseEvent>

ImageDropdown::ImageDropdown(QStringList options, QWidget* parent) : QWidget(parent), items(std::move(options))
{
    const QSize w = window()->size();
    const QSize box = layout::select::box(w);
    const int   oh  = layout::select::option_h(w);
    const int   ov  = layout::select::option_overlap(w);
    const int   max_h = box.height() + (items.size() - 1) * (oh - ov);
    setFixedSize(box.width(), max_h);
}

QRect ImageDropdown::option_rect(int slot) const
{
    const QSize w = window()->size();
    const int oh  = layout::select::option_h(w);
    const int ov  = layout::select::option_overlap(w);
    const int y   = layout::select::box(w).height() - ov + slot * (oh - ov);
    return { 0, y, width(), oh };
}

void ImageDropdown::set_index(int i)
{
    if (i < 0 || i >= items.size()) return;
    current = i;
    update();
    emit changed(current);
}

void ImageDropdown::mousePressEvent(QMouseEvent* event)
{
    const QSize w = window()->size();
    const QRect closed { 0, 0, width(), layout::select::box(w).height() };

    if (closed.contains(event->pos()))
    {
        open = !open;
        update();
        return;
    }

    if (open)
    {
        int slot = 0;
        for (int i = 0; i < items.size(); ++i)
        {
            if (i == current) continue;
            if (option_rect(slot).contains(event->pos()))
            {
                current = i;
                open = false;
                update();
                emit changed(current);
                return;
            }
            ++slot;
        }
        open = false;
        update();
    }
}

void ImageDropdown::paintEvent(QPaintEvent*)
{
    const QSize w = window()->size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QPixmap& dropdown_px = assets::images[assets::Image::MenuDropdown];
    const QSize box = layout::select::box(w);
    const int   lip = layout::scaled(layout::select::k_pad_bottom, w);
    const int   pad = layout::scaled(20, w);

    QFont f = assets::fonts[assets::Font::Inter];
    f.setPixelSize(layout::scaled(layout::text::k_label, w));
    f.setWeight(QFont::Medium);
    painter.setFont(f);
    const QColor text_col {0x4F, 0x17, 0x17};

    // Open options first (below), closed box drawn on top after
    if (open)
    {
        int slot = 0;
        for (int i = 0; i < items.size(); ++i)
        {
            if (i == current) continue;
            const QRect r = option_rect(slot);
            painter.drawPixmap(r, dropdown_px);
            painter.setPen(text_col);
            painter.drawText(r.adjusted(pad, 0, -pad, -lip),
                             Qt::AlignVCenter | Qt::AlignLeft, items[i]);
            ++slot;
        }
    }

    const QRect closed { 0, 0, box.width(), box.height() };
    painter.drawPixmap(closed, dropdown_px);
    painter.setPen(text_col);
    painter.drawText(closed.adjusted(pad, 0, -pad, -lip),
                     Qt::AlignVCenter | Qt::AlignLeft, items[current]);

    // Chevron
    painter.setPen(QPen(QColor(0xA8, 0x90, 0x78), layout::scaled(2, w)));
    const int cx = closed.right() - layout::scaled(28, w);
    const int cy = closed.center().y();
    const int s  = layout::scaled(5, w);
    if (open)
    {
        painter.drawLine(cx - s, cy + s / 2, cx, cy - s / 2);
        painter.drawLine(cx, cy - s / 2, cx + s, cy + s / 2);
    }
    else
    {
        painter.drawLine(cx - s, cy - s / 2, cx, cy + s / 2);
        painter.drawLine(cx, cy + s / 2, cx + s, cy - s / 2);
    }
}