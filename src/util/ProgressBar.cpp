#include "util/ProgressBar.hpp"
#include "util/Assets.hpp"

#include <QPainter>
#include <QRect>
#include <QtGlobal>

namespace util::progress_bar
{
    void draw(QPainter& painter, const QRect& bar, const double fraction)
    {
        const QPixmap& track = assets::images[assets::Image::ProgressBarTrack];
        painter.drawPixmap(bar, track);

        if (fraction <= 0.0 || bar.isEmpty()) return;




        const int inset_x = qMax(1, qRound(9.0 * bar.width() / 430.0));
        const int inset_y = qMax(1, qRound(1.0 * bar.height() / 19.0));
        const QRect fill_area = bar.adjusted(inset_x, inset_y, -inset_x, -inset_y);
        if (fill_area.isEmpty()) return;

        const QPixmap& start_px = assets::images[assets::Image::ProgressBarStart];
        const QPixmap& mid_px   = assets::images[assets::Image::ProgressBarMiddle];
        const QPixmap& end_px   = assets::images[assets::Image::ProgressBarEnd];

        const int h  = fill_area.height();
        const int sw = start_px.height() > 0 ? qRound(start_px.width() * h / double(start_px.height())) : h;
        const int ew = end_px.height() > 0 ? qRound(end_px.width() * h / double(end_px.height())) : h;
        const int fill_w = qRound(fill_area.width() * qBound(0.0, fraction, 1.0));
        if (fill_w <= 0) return;

        painter.save();
        painter.setClipRect(QRect(fill_area.left(), fill_area.top(), fill_w, h));

        painter.drawPixmap(
            QRect(fill_area.left(), fill_area.top(), sw, h),
            start_px);

        const int mid_left = fill_area.left() + sw;
        const int mid_right = fill_area.left() + fill_w - ew;
        const int mid_w = qMax(0, mid_right - mid_left);
        if (mid_w > 0)
            painter.drawPixmap(QRect(mid_left, fill_area.top(), mid_w, h), mid_px);

        if (fill_w > sw)
        {
            const int end_w = qMin(ew, fill_w);
            painter.drawPixmap(
                QRect(fill_area.left() + fill_w - end_w, fill_area.top(), end_w, h),
                end_px);
        }

        painter.restore();
    }
}
