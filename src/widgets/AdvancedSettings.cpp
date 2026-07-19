#include "widgets/AdvancedSettings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Config.hpp"
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
using util::config::Config;
namespace ls = util::layout::settings;
namespace aset = util::layout::advanced_settings;
AdvancedSettings::AdvancedSettings(QWidget* parent) : QWidget(parent)
{
    setup_game_path_option();
    setup_game_args_option();
}
void AdvancedSettings::setup_game_path_option()
{
    const QSize w = window()->size();
    const int y = aset::row(0);
    util::simple_utils::make_label_block(this, w, y,
                            "GAME INSTALL PATH",
                            "Where the game is installed. Leave blank to use the default location.");
    game_path_field = new QLineEdit(this);
    game_path_field->setText(Config::instance().game_install_path());
    game_path_field->setStyleSheet(util::styles::k_field);
    game_path_field->setGeometry(ls::field_rect(w, y));

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(ls::browse_rect(w, y));
    connect(browse, &QPushButton::clicked, this, [this]()
    {
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            "Select Game Folder",
            Config::instance().prefix_root());
        if (!dir.isEmpty())
        {
            game_path_field->setText(dir);
            Config::instance().set_game_install_path(dir);
        }
    });
    connect(game_path_field, &QLineEdit::editingFinished, this, [this]()
    {
        if (game_path_field->text() != Config::instance().game_install_path())
            Config::instance().set_game_install_path(game_path_field->text());
    });
    connect(&Config::instance(), &Config::changed, game_path_field, [this]()
    {
        if (!game_path_field->hasFocus())
            game_path_field->setText(Config::instance().game_install_path());
    });
}
void AdvancedSettings::setup_game_args_option()
{
    const QSize w = window()->size();
    const int y = aset::row(1);
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
