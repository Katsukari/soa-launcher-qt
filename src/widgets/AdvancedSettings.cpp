#include "widgets/AdvancedSettings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Config.hpp"
#include <QLineEdit>

using util::config::Config;
namespace ls = util::layout::settings;
namespace aset = util::layout::advanced_settings;


AdvancedSettings::AdvancedSettings(QWidget* parent) : QWidget(parent)
{
    setup_game_args_option();
}

void AdvancedSettings::setup_game_args_option()
{
    const QSize w = window()->size();
    const int y = aset::row();
    util::simple_utils::make_label_block(this, w, y,
                            "GAME LAUNCH ARGUMENTS",
                            "Passed to Alicia.exe. Optional for most players.");

    auto* field = new QLineEdit(this);
    field->setPlaceholderText("Alicia.exe (default)");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::ctrl_pos(w, y).x(), ls::ctrl_pos(w, y).y(),
                       ls::ctrl_w(w), util::layout::scaled(34, w));
    field->setText(Config::instance().game_args());

    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        Config::instance().set_game_args(field->text());
    });
}