#include "widgets/LauncherSettings.hpp"
#include "widgets/ImageDropdown.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"

#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>

LauncherSettings::LauncherSettings(QWidget* parent) : QWidget(parent)
{
    setup_launch_on_startup_option();
    setup_after_game_start_option();
    setup_run_connectivity_test_option();
    setup_launcher_size_option();
    setup_github_button();
}

void LauncherSettings::setup_launch_on_startup_option()
{
    const QSize w = window()->size();
    utils::make_label_block(this, w, layout::settings::k_row1_y,
                            "LAUNCH ON STARTUP",
                            "Automatically open the launcher when you log in to your computer.");

    auto* slider = new QPushButton(this);
    slider->setFlat(true);
    slider->setCursor(Qt::PointingHandCursor);
    slider->setStyleSheet("border:none; background:transparent;");

    const QRect sr = layout::settings::slider_rect(w, layout::settings::k_row1_y);
    const QSize ssz = sr.size();
    slider->setIcon(QIcon(assets::buttons[assets::Button::SliderOff].normal
        .scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    slider->setIconSize(ssz);
    slider->setGeometry(sr);

    connect(slider, &QPushButton::clicked, this, [slider, ssz]()
    {
        static bool on = false;   // display-only toggle for now
        on = !on;
        const auto& a = on ? assets::buttons[assets::Button::SliderOn] : assets::buttons[assets::Button::SliderOff];
        slider->setIcon(QIcon(a.normal.scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    });
}

void LauncherSettings::setup_after_game_start_option()
{
    const QSize w = window()->size();
    utils::make_label_block(this, w, layout::settings::k_row2_y,
                            "AFTER GAME START",
                            "Choose what the launcher does after the game starts up.");

    auto* dd = new ImageDropdown({ "Keep launcher open", "Minimize to tray" }, this);
    dd->move(layout::settings::ctrl_pos(w, layout::settings::k_row2_y));
}

void LauncherSettings::setup_run_connectivity_test_option()
{
    const QSize w = window()->size();
    utils::make_label_block(this, w, layout::settings::k_row3_y,
                            "CONNECTIVITY CHECK",
                            "Diagnose issues connecting to the game and related servers.");

    auto* btn = new QPushButton(this);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet("border:none; background:transparent;");

    const QRect br = layout::settings::run_check(w, layout::settings::k_row3_y);
    btn->setIcon(QIcon(assets::buttons[assets::Button::RunCheck].normal
        .scaled(br.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    btn->setIconSize(br.size());
    btn->setGeometry(br);
}

void LauncherSettings::setup_launcher_size_option()
{
    const QSize w = window()->size();
    utils::make_label_block(this, w, layout::settings::k_row4_y,
                            "LAUNCHER SIZE",
                            "Choose your preferred window size for the launcher.");

    auto* dd = new ImageDropdown(
        { "Small (1120x677)", "Default (1400x846)", "Large (1600x967)", "4K (1920x1160)" }, this);
    dd->set_index(1);   // Default
    dd->move(layout::settings::ctrl_pos(w, layout::settings::k_row4_y));
    dd->raise();
}

void LauncherSettings::setup_github_button()
{
    const QSize w = window()->size();
    auto* gh = new QPushButton("GITHUB", this);
    gh->setCursor(Qt::PointingHandCursor);
    gh->setStyleSheet(
        "QPushButton { background:#D8CDC0; border:none; border-radius:6px;"
        " color:#4F1717; font-family:'Eurostile'; font-weight:900; font-size:11px; }"
        "QPushButton:hover { background:#E6DCD0; }");
    gh->setGeometry(layout::scaled(layout::settings::k_text_x, w),
                    layout::scaled(496, w),
                    layout::scaled(110, w), layout::scaled(30, w));
    connect(gh, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl("https://github.com/Story-Of-Alicia/soa-launcher-qt"));
    });
}

