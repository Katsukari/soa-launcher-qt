#include "util/ProgressBar.hpp"
#include "util/Assets.hpp"
#include <QPainter>
#include <QRect>

namespace util::progress_bar
{
    void draw(QPainter& painter, const QRect& bar, double fraction)
    {
        painter.drawPixmap(bar, util::assets::images[util::assets::Image::ProgressBarTrack]);

        if (fraction <= 0.0) return;

        const QPixmap& start_px = util::assets::images[util::assets::Image::ProgressBarStart];
        const QPixmap& mid_px   = util::assets::images[util::assets::Image::ProgressBarMiddle];
        const QPixmap& end_px   = util::assets::images[util::assets::Image::ProgressBarEnd];

        const int h  = bar.height();
        const int sw = (start_px.height() > 0) ? start_px.width() * h / start_px.height() : h;
        const int ew = (end_px.height()   > 0) ? end_px.width()   * h / end_px.height()   : h;

        const int fill_w = int(bar.width() * qBound(0.0, fraction, 1.0));
        if (fill_w <= 0) return;

        painter.save();
        painter.setClipRect(QRect(bar.left(), bar.top(), fill_w, h));

        const int mid_left  = bar.left() + sw;
        const int mid_right = bar.left() + fill_w - ew;
        const int mid_w     = qMax(0, mid_right - mid_left);

        painter.drawPixmap(QRect(bar.left(), bar.top(), sw, h), start_px);
        if (mid_w > 0) painter.drawPixmap(QRect(mid_left, bar.top(), mid_w, h), mid_px);
        painter.drawPixmap(QRect(bar.left() + fill_w - ew, bar.top(), ew, h), end_px);

        painter.restore();
    }
}