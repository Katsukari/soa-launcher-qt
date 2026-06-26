#include "widgets/DownloadProgress.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include <QPainter>

DownloadProgress::DownloadProgress(QWidget* parent) : ModalOverlay(parent)
{
    // empty for now
}

void DownloadProgress::paint_content(QPainter& painter)
{
    const QSize w = window()->size();

    // Just the box for now - contents (progress bar, speed, file count) come later.
    const QRect box = util::layout::install_modal::box_rect(w);
    painter.drawPixmap(box, util::assets::images[util::assets::Image::BoxGameInstall]);

    // Placeholder title so it's visibly the download step.
    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(QColor(0x4F, 0x17, 0x17));
    painter.drawText(util::layout::install_modal::title(w), Qt::AlignCenter, "DOWNLOADING");
}