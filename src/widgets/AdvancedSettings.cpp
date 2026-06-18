// AdvancedSettings.cpp
#include "widgets/AdvancedSettings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include <QLineEdit>

namespace
{
    const char* k_field_style =
        "QLineEdit"
        "{"
        "    background: rgba(255,255,255,0.45);"
        "    border: 1px solid #C9BBAA;"
        "    border-radius: 6px;"
        "    padding: 0 8px;"
        "    color: #4F1717;"
        "    font-family: 'Inter';"
        "}";
}

AdvancedSettings::AdvancedSettings(QWidget* parent) : QWidget(parent)
{
    setup_game_args_option();
}

void AdvancedSettings::setup_game_args_option()
{
    const QSize w = window()->size();
    utils::make_label_block(this, w, layout::settings::k_row1_y,
                            "GAME LAUNCH ARGUMENTS",
                            "Passed to Alicia.exe. Optional for most players.");

    const QPoint cp = layout::settings::ctrl_pos(w, layout::settings::k_row1_y);
    auto* field = new QLineEdit(this);
    field->setPlaceholderText("Alicia.exe (default)");
    field->setStyleSheet(k_field_style);
    field->setGeometry(cp.x(), cp.y(), layout::settings::ctrl_w(w), layout::scaled(34, w));
}