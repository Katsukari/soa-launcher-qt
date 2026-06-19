#include "util/SimpleUtils.hpp"

namespace util::simple_utils
{
    void make_label_block(QWidget* parent, const QSize w, int y, const QString& title, const QString& desc)
    {
        auto* t = new QLabel(title, parent);
        QFont tf = assets::fonts[assets::Font::EurostileBlack];
        tf.setPixelSize(layout::scaled(layout::text::k_row_title, w));
        tf.setWeight(QFont::Black);
        t->setFont(tf);
        t->setStyleSheet("color: #4F1717; background: transparent;");
        t->setGeometry(layout::settings::row_title(w, y));

        auto* d = new QLabel(desc, parent);
        d->setWordWrap(true);
        QFont df = assets::fonts[assets::Font::Inter];
        df.setPixelSize(layout::scaled(layout::text::k_desc, w));
        df.setWeight(QFont::Medium);
        d->setFont(df);
        d->setStyleSheet("color: #4F1717; background: transparent;");
        d->setGeometry(layout::settings::row_desc(w, y));
    }
}