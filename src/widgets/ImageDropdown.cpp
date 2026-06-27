#include "widgets/ImageDropdown.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>
#include <QMouseEvent>

namespace dd = util::layout::dropdown;

ImageDropdown::ImageDropdown(QStringList options, QWidget* parent) : QWidget(parent), items(std::move(options))
{
    const QSize w = window()->size();
    setFixedSize(dd::total_size(w, items.size()));
}

QRect ImageDropdown::option_rect(int slot) const
{
    return dd::option_rect(window()->size(), slot);
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
    if (dd::closed_rect(w).contains(event->pos()))
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

    const QPixmap& dropdown_px = util::assets::images[util::assets::Image::MenuDropdown];
    const int lip = dd::pad_bottom(w);
    const int pad = dd::text_pad(w);

    QFont f = util::assets::fonts[util::assets::Font::Inter];
    f.setPixelSize(util::layout::scaled(util::layout::text::k_label, w));
    f.setWeight(QFont::Medium);
    painter.setFont(f);
    const QColor text_col {0x4F, 0x17, 0x17};

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

    const QRect closed = dd::closed_rect(w);
    painter.drawPixmap(closed, dropdown_px);
    painter.setPen(text_col);
    painter.drawText(closed.adjusted(pad, 0, -pad, -lip),
                     Qt::AlignVCenter | Qt::AlignLeft, items[current]);

    painter.setPen(QPen(QColor(0xA8, 0x90, 0x78), util::layout::scaled(2, w)));
    const QPoint cc = dd::chevron_center(w);
    const int s = dd::chevron_arm(w);
    if (open)
    {
        painter.drawLine(cc.x() - s, cc.y() + s / 2, cc.x(), cc.y() - s / 2);
        painter.drawLine(cc.x(), cc.y() - s / 2, cc.x() + s, cc.y() + s / 2);
    }
    else
    {
        painter.drawLine(cc.x() - s, cc.y() - s / 2, cc.x(), cc.y() + s / 2);
        painter.drawLine(cc.x(), cc.y() + s / 2, cc.x() + s, cc.y() - s / 2);
    }
}