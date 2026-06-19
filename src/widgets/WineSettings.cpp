#include "widgets/WineSettings.hpp"
#include "widgets/LauncherLog.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"

#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>

#include "core/wine/Shell.hpp"

WineSettings::WineSettings(core::wine::Shell* shell_, QWidget* parent)
    : QWidget(parent), shell(shell_)
{
    setup_dxvk_option();
    setup_prefix_option();
    setup_wine_binary_option();
    setup_wine_args_option();
    setup_generate_button();
}

void WineSettings::setup_dxvk_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row1_y,
                            "USE DXVK",
                            "Translate Direct3D to Vulkan for better performance.");

    auto* slider = new QPushButton(this);
    slider->setFlat(true);
    slider->setCursor(Qt::PointingHandCursor);
    slider->setStyleSheet("border:none; background:transparent;");
    const QRect sr = util::layout::settings::slider_rect(w, util::layout::settings::k_row1_y);
    const QSize ssz = sr.size();
    slider->setIcon(QIcon(util::assets::buttons[util::assets::Button::SliderOn].normal
        .scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));   // on by default
    slider->setIconSize(ssz);
    slider->setGeometry(sr);
    connect(slider, &QPushButton::clicked, this, [slider, ssz]()
    {
        static bool on = true;
        on = !on;
        const auto& a = on ? util::assets::buttons[util::assets::Button::SliderOn]
                           : util::assets::buttons[util::assets::Button::SliderOff];
        slider->setIcon(QIcon(a.normal.scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    });
}

void WineSettings::setup_prefix_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row2_y,
                            "WINE PREFIX",
                            "The isolated Wine environment the game runs in.");

    const QPoint cp  = util::layout::settings::ctrl_pos(w, util::layout::settings::k_row2_y);
    const int    cw  = util::layout::settings::ctrl_w(w);
    const int    h   = util::layout::scaled(34, w);
    const int    gap = util::layout::scaled(6, w);
    const int    bw  = util::layout::scaled(34, w);   // browse only (generate moved to footer)
    const int    field_w = cw - bw - gap;             // field gets the freed width

    auto* field = new QLineEdit(this);
    field->setText(shell->wine_prefix());
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(cp.x(), cp.y(), field_w, h);

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(cp.x() + field_w + gap, cp.y(), bw, h);
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select Wine Prefix Folder");
        if (!dir.isEmpty())
        {
            field->setText(dir);
            shell->set_root_path(dir);   // keep the shell's prefix in sync with the field
        }
    });
}

void WineSettings::setup_wine_binary_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row3_y,
                            "CUSTOM WINE / PROTON",
                            "Path to a wine or proton binary. Blank uses system wine.");

    const QPoint cp  = util::layout::settings::ctrl_pos(w, util::layout::settings::k_row3_y);
    const int    cw  = util::layout::settings::ctrl_w(w);
    const int    h   = util::layout::scaled(34, w);
    const int    gap = util::layout::scaled(6, w);
    const int    bw  = util::layout::scaled(34, w);
    const int    field_w = cw - bw - gap;

    auto* field = new QLineEdit(this);
    field->setPlaceholderText("system wine");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(cp.x(), cp.y(), field_w, h);

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(cp.x() + field_w + gap, cp.y(), bw, h);
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString file = QFileDialog::getOpenFileName(this, "Select Wine / Proton Binary");
        if (!file.isEmpty()) field->setText(file);
    });
}

void WineSettings::setup_wine_args_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row4_y,
                            "WINE LAUNCH ARGUMENTS",
                            "Extra flags passed to Wine. Leave blank if unsure.");

    const QPoint cp = util::layout::settings::ctrl_pos(w, util::layout::settings::k_row4_y);
    auto* field = new QLineEdit(this);
    field->setPlaceholderText("e.g. WINEDEBUG=-all");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(cp.x(), cp.y(), util::layout::settings::ctrl_w(w), util::layout::scaled(34, w));
}

void WineSettings::setup_generate_button()
{
    const QSize w = window()->size();
    auto* generate = new QPushButton("INSTALL WINE PREFIX", this);
    generate->setCursor(Qt::PointingHandCursor);
    generate->setStyleSheet(util::styles::k_primary_button);
    generate->setGeometry(util::layout::settings::footer_big(w));   // centered big footer button

    connect(generate, &QPushButton::clicked, this, [this]()
    {
        LauncherLog::instance()->show();
        LauncherLog::instance()->raise();
        shell->setup_wine();
    });
}