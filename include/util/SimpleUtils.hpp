#pragma once

#include <QWidget>
#include <QLabel>
#include <QFont>
#include "util/Layout.hpp"
#include "util/Assets.hpp"

namespace util::simple_utils
{
    void make_label_block(QWidget* parent, const QSize w, int y, const QString& title, const QString& desc);
}