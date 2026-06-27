#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Layout.hpp"
#include "util/Assets.hpp"

#include <QIcon>
#include <QPushButton>
#include <QEvent>

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

    QPushButton* make_flat_button(QWidget* parent)
    {
        auto* b = new QPushButton(parent);
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(styles::k_flat_transparent);
        return b;
    }

    bool apply_button_state(QEvent* event, QPushButton* button,
                            const QPixmap& normal, const QPixmap& hover, const QPixmap& clicked)
    {
        switch (event->type())
        {
            case QEvent::Enter:
                button->setIcon(QIcon(hover));
                return true;
            case QEvent::Leave:
                button->setIcon(QIcon(normal));
                return true;
            case QEvent::MouseButtonPress:
                button->setIcon(QIcon(clicked));
                return true;
            case QEvent::MouseButtonRelease:
                button->setIcon(QIcon(button->underMouse() ? hover : normal));
                return true;
            default:
                return false;
        }
    }
}