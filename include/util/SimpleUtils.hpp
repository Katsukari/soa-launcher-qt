#pragma once

#include <QEvent>
#include <QLabel>
#include <QPushButton>

namespace util::simple_utils
{
    void make_label_block(QWidget* parent, const QSize w, int y, const QString& title, const QString& desc);
    QPushButton * make_flat_button(QWidget* parent);
    bool apply_button_state(QEvent* event, QPushButton* button, const QPixmap& normal, const QPixmap& hover, const QPixmap& clicked);
}