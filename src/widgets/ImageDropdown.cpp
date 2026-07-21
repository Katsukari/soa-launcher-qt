#include "widgets/ImageDropdown.hpp"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>

#include <utility>

#include "util/Assets.hpp"
#include "util/Layout.hpp"

namespace dd = util::layout::dropdown;

ImageDropdown::ImageDropdown(QStringList options, QWidget* parent)
    : QWidget(parent), items(std::move(options))
{
    if (items.isEmpty())
    {
        items.push_back(QStringLiteral("Unavailable"));
        setEnabled(false);
    }

    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("Selection menu"));
    setFixedSize(dd::box(window()->size()));
}

QRect ImageDropdown::closed_rect() const
{
    const QSize w = window()->size();
    const QSize box = dd::box(w);
    if (!open || !opens_upward)
        return dd::closed_rect(w);
    const QSize total = dd::total_size(w, items.size());
    return {0, total.height() - box.height(), box.width(), box.height()};
}

QRect ImageDropdown::option_rect(const int slot) const
{
    if (!open || !opens_upward)
        return dd::option_rect(window()->size(), slot);
    const QSize w = window()->size();
    const QSize box = dd::box(w);
    const int step = dd::option_h(w) - dd::option_overlap(w);
    return {0, slot * step, box.width(), dd::option_h(w)};
}

void ImageDropdown::set_open_upwards(const bool value)
{
    if (open)
        set_open(false);
    opens_upward = value;
}

void ImageDropdown::set_open(const bool value)
{
    if (!isEnabled() || open == value)
        return;

    const QSize w = window()->size();
    const QSize box = dd::box(w);
    if (value)
    {
        closed_position = pos();
        const QSize total = dd::total_size(w, items.size());
        open = true;
        if (opens_upward)
            move(closed_position.x(), closed_position.y() - total.height() + box.height());
        setFixedSize(total);
        raise();
    }
    else
    {
        open = false;
        setFixedSize(box);
        if (opens_upward)
            move(closed_position);
    }
    update();
}

void ImageDropdown::set_index(const int i)
{
    if (i < 0 || i >= items.size() || i == current)
        return;
    current = i;
    update();
    emit changed(current);
}

void ImageDropdown::select_relative(const int delta)
{
    if (items.size() < 2)
        return;
    const int next = (current + delta + items.size()) % items.size();
    set_index(next);
}

void ImageDropdown::mousePressEvent(QMouseEvent* event)
{
    if (closed_rect().contains(event->pos()))
    {
        setFocus(Qt::MouseFocusReason);
        set_open(!open);
        event->accept();
        return;
    }

    if (open)
    {
        int slot = 0;
        for (int i = 0; i < items.size(); ++i)
        {
            if (i == current)
                continue;
            if (option_rect(slot).contains(event->pos()))
            {
                current = i;
                set_open(false);
                emit changed(current);
                event->accept();
                return;
            }
            ++slot;
        }
        set_open(false);
    }
    QWidget::mousePressEvent(event);
}

void ImageDropdown::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Up:
        case Qt::Key_Left:
            select_relative(-1);
            event->accept();
            return;
        case Qt::Key_Down:
        case Qt::Key_Right:
            select_relative(1);
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            set_open(!open);
            event->accept();
            return;
        case Qt::Key_Escape:
            set_open(false);
            event->accept();
            return;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
}

void ImageDropdown::focusOutEvent(QFocusEvent* event)
{
    set_open(false);
    QWidget::focusOutEvent(event);
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

    QFont font = util::assets::fonts[util::assets::Font::Inter];
    font.setPixelSize(util::layout::scaled(util::layout::text::k_label, w));
    font.setWeight(QFont::Medium);
    painter.setFont(font);
    const QColor text_col {0x4F, 0x17, 0x17};

    if (open)
    {
        int slot = 0;
        for (int i = 0; i < items.size(); ++i)
        {
            if (i == current)
                continue;
            const QRect rect = option_rect(slot++);
            painter.drawPixmap(rect, dropdown_px);
            painter.setPen(text_col);
            painter.drawText(rect.adjusted(pad, 0, -pad, -lip),
                             Qt::AlignVCenter | Qt::AlignLeft, items[i]);
        }
    }

    const QRect closed = closed_rect();
    painter.drawPixmap(closed, dropdown_px);
    painter.setPen(text_col);
    painter.drawText(closed.adjusted(pad, 0, -pad, -lip),
                     Qt::AlignVCenter | Qt::AlignLeft, items.value(current));

    painter.setPen(QPen(QColor(0xA8, 0x90, 0x78), util::layout::scaled(2, w)));
    const QPoint center = dd::chevron_center(w) + QPoint(0, closed.top());
    const int arm = dd::chevron_arm(w);
    if (open)
    {
        painter.drawLine(center.x() - arm, center.y() + arm / 2, center.x(), center.y() - arm / 2);
        painter.drawLine(center.x(), center.y() - arm / 2, center.x() + arm, center.y() + arm / 2);
    }
    else
    {
        painter.drawLine(center.x() - arm, center.y() - arm / 2, center.x(), center.y() + arm / 2);
        painter.drawLine(center.x(), center.y() + arm / 2, center.x() + arm, center.y() - arm / 2);
    }
}
