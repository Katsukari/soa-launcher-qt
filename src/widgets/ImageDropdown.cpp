#include "widgets/ImageDropdown.hpp"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

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

QRect ImageDropdown::option_rect(const int slot) const
{
    return dd::option_rect(window()->size(), slot);
}

void ImageDropdown::set_open(const bool value)
{
    if (!isEnabled())
        return;
    open = value;
    setFixedSize(open
        ? dd::total_size(window()->size(), items.size())
        : dd::box(window()->size()));
    if (open)
        raise();
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
    if (dd::closed_rect(window()->size()).contains(event->pos()))
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

    const QRect closed = dd::closed_rect(w);
    painter.drawPixmap(closed, dropdown_px);
    painter.setPen(text_col);
    painter.drawText(closed.adjusted(pad, 0, -pad, -lip),
                     Qt::AlignVCenter | Qt::AlignLeft, items.value(current));

    painter.setPen(QPen(QColor(0xA8, 0x90, 0x78), util::layout::scaled(2, w)));
    const QPoint center = dd::chevron_center(w);
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
