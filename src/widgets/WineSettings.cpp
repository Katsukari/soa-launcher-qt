#include "widgets/WineSettings.hpp"
#include "widgets/LauncherLog.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Config.hpp"

#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>

#include "core/wine/Shell.hpp"

using util::config::Config;
namespace ls = util::layout::settings;
namespace wset = util::layout::wine_settings;


WineSettings::WineSettings(core::wine::Shell* shell_, QWidget* parent) : QWidget(parent), shell(shell_)
{
    setup_dxvk_option();
    setup_prefix_option();
    setup_wine_binary_option();
    setup_tricks_option();
    setup_wine_args_option();
}

void WineSettings::setup_dxvk_option()
{
    const QSize w = window()->size();
    const int y = wset::row(0);
    util::simple_utils::make_label_block(this, w, y,
                            "USE DXVK",
                            "Translate Direct3D to Vulkan for better performance.");

    auto* slider = new QPushButton(this);
    slider->setFlat(true);
    slider->setCursor(Qt::PointingHandCursor);
    slider->setStyleSheet("border:none; background:transparent;");
    const QRect sr = ls::slider_rect(w, y);
    const QSize ssz = sr.size();
    slider->setIconSize(ssz);
    slider->setGeometry(sr);

    auto paint = [slider, ssz](bool on)
    {
        const auto& a = on ? util::assets::buttons[util::assets::Button::SliderOn]
                           : util::assets::buttons[util::assets::Button::SliderOff];
        slider->setIcon(QIcon(a.normal.scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    };
    paint(Config::instance().use_dxvk());

    connect(slider, &QPushButton::clicked, this, [paint]()
    {
        const bool on = !Config::instance().use_dxvk();
        Config::instance().set_use_dxvk(on);
        paint(on);
    });
}

void WineSettings::setup_prefix_option()
{
    const QSize w = window()->size();
    const int y = wset::row(1);
    util::simple_utils::make_label_block(this, w, y,
                            "WINE PREFIX",
                            "The isolated Wine environment the game runs in.");

    auto* field = new QLineEdit(this);
    field->setText(Config::instance().wine_prefix());
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::field_rect(w, y));

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(ls::browse_rect(w, y));
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select Wine Prefix Folder");
        if (!dir.isEmpty())
        {
            field->setText(dir);
            Config::instance().set_wine_prefix(dir);
        }
    });
    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        Config::instance().set_wine_prefix(field->text());
    });
}

void WineSettings::setup_wine_binary_option()
{
    const QSize w = window()->size();
    const int y = wset::row(2);
    util::simple_utils::make_label_block(this, w, y,
                            "CUSTOM WINE / PROTON",
                            "Path to a wine or proton binary. Blank uses system wine.");

    auto* field = new QLineEdit(this);
    field->setPlaceholderText("system wine");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::field_rect(w, y));
    field->setText(Config::instance().wine_binary());

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(ls::browse_rect(w, y));
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString file = QFileDialog::getOpenFileName(this, "Select Wine / Proton Binary");
        if (!file.isEmpty())
        {
            Config::instance().set_wine_binary(file);
            field->setText(file);
        }
    });
    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        Config::instance().set_wine_binary(field->text());
    });
}

void WineSettings::setup_tricks_option()
{
    const QSize w = window()->size();
    const int y = wset::row(3);
    util::simple_utils::make_label_block(this, w, y,
                            "WINETRICKS / PROTONTRICKS",
                            "Path to the tricks tool. Blank uses the one on PATH.");

    auto* field = new QLineEdit(this);
    field->setPlaceholderText("from PATH");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::field_rect(w, y));
    field->setText(Config::instance().winetricks_binary());

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(ls::browse_rect(w, y));
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString file = QFileDialog::getOpenFileName(this, "Select Winetricks / Protontricks");
        if (!file.isEmpty())
        {
            Config::instance().set_winetricks_binary(file);
            field->setText(file);
        }
    });
    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        Config::instance().set_winetricks_binary(field->text());
    });
}

void WineSettings::setup_wine_args_option()
{
    const QSize w = window()->size();
    const int y = wset::row(4);
    util::simple_utils::make_label_block(this, w, y,
                            "WINE LAUNCH ARGUMENTS",
                            "Extra flags passed to Wine. Leave blank if unsure.");

    auto* field = new QLineEdit(this);
    field->setPlaceholderText("e.g. WINEDEBUG=-all");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::ctrl_pos(w, y).x(), ls::ctrl_pos(w, y).y(),
                       ls::ctrl_w(w), util::layout::scaled(34, w));
    field->setText(Config::instance().wine_args());

    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        Config::instance().set_wine_args(field->text());
    });
}