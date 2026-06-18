#include "util/ModalOverlay.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>

namespace
{
    QPixmap blur_pixmap(const QPixmap & src, const qreal radius)
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

ModalOverlay::ModalOverlay(QWidget* parent) : QWidget(parent)
{
    setFixedSize(window()->size());
}

void ModalOverlay::show_over(QWidget* background)
{
    const QPixmap snap = background->grab();
    blurred_bg = blur_pixmap(snap, layout::scaled(10, size()));
    show();
    raise();
}

void ModalOverlay::paint_frames(QPainter& painter)
{
    const QSize w = window()->size();

    const QPixmap left = assets::images[assets::Image::LeftFrame].scaledToHeight(height(), Qt::SmoothTransformation);
    painter.drawPixmap(0, 0, left);

    const QPixmap right = assets::images[assets::Image::RightFrame].scaledToHeight(height(), Qt::SmoothTransformation);
    painter.drawPixmap(width() - right.width(), 0, right);

    painter.drawPixmap(layout::chrome::pt_icon(w),   assets::images[assets::Image::IconPT]);
    painter.drawPixmap(layout::chrome::lock_icon(w), assets::images[assets::Image::IconLock]);
}

void ModalOverlay::paintEvent(QPaintEvent*)
{
    const QSize w = window()->size();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Frosted backdrop (shared by every overlay)
    if (!blurred_bg.isNull())
    {
        const QRect bg = layout::region::rect(w);
        const int radius = layout::scaled(layout::region::k_radius, w);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(bg), radius, radius);
        painter.setClipPath(clip);

        painter.drawPixmap(rect(), blurred_bg);
        painter.fillRect(rect(), QColor(255, 255, 255, 70));

        painter.setClipping(false);
    }

    // Frames + sidebar icons, sharp on top of the blur - only if this overlay keeps chrome
    if (keep_chrome)
    {
        paint_frames(painter);
    }

    // Subclass draws its box + contents on top
    paint_content(painter);
}