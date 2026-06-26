#include "widgets/LauncherSettings.hpp"
#include "widgets/ImageDropdown.hpp"
#include "widgets/LauncherLog.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Config.hpp"

#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>

using util::config::Config;

LauncherSettings::LauncherSettings(QWidget* parent) : QWidget(parent)
{
    setup_launch_on_startup_option();
    setup_after_game_start_option();
    setup_run_connectivity_test_option();
    setup_launcher_size_option();
    setup_github_button();
    setup_log_button();
}

void LauncherSettings::setup_launch_on_startup_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row1_y,
                            "LAUNCH ON STARTUP",
                            "Automatically open the launcher when you log in to your computer.");

    auto* slider = new QPushButton(this);
    slider->setFlat(true);
    slider->setCursor(Qt::PointingHandCursor);
    slider->setStyleSheet("border:none; background:transparent;");

    const QRect sr = util::layout::settings::slider_rect(w, util::layout::settings::k_row1_y);
    const QSize ssz = sr.size();
    slider->setIconSize(ssz);
    slider->setGeometry(sr);

    auto paint = [slider, ssz](bool on)
    {
        const auto& a = on ? util::assets::buttons[util::assets::Button::SliderOn]
                           : util::assets::buttons[util::assets::Button::SliderOff];
        slider->setIcon(QIcon(a.normal.scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    };
    paint(Config::instance().launch_on_startup());

    connect(slider, &QPushButton::clicked, this, [paint]()
    {
        const bool on = !Config::instance().launch_on_startup();
        Config::instance().set_launch_on_startup(on);
        paint(on);
    });
}

void LauncherSettings::setup_after_game_start_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row2_y,
                            "AFTER GAME START",
                            "Choose what the launcher does after the game starts up.");

    auto* dd = new ImageDropdown({ "Keep launcher open", "Minimize to tray" }, this);

    // Reflect the stored value ("keep" / "minimize").
    dd->set_index(Config::instance().after_game_start() == "minimize" ? 1 : 0);
    dd->move(util::layout::settings::ctrl_pos(w, util::layout::settings::k_row2_y));

    connect(dd, &ImageDropdown::changed, this, [](int idx)
    {
        Config::instance().set_after_game_start(idx == 1 ? "minimize" : "keep");
    });
}

void LauncherSettings::setup_run_connectivity_test_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row3_y,
                            "CONNECTIVITY CHECK",
                            "Diagnose issues connecting to the game and related servers.");

    auto* button = new QPushButton(this);
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet("border:none; background:transparent;");

    const QRect br = util::layout::settings::run_check(w, util::layout::settings::k_row3_y);
    button->setIcon(QIcon(util::assets::buttons[util::assets::Button::RunCheck].normal
        .scaled(br.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    button->setIconSize(br.size());
    button->setGeometry(br);
}

void LauncherSettings::setup_launcher_size_option()
{
    const QSize w = window()->size();
    util::simple_utils::make_label_block(this, w, util::layout::settings::k_row4_y,
                            "LAUNCHER SIZE",
                            "Choose your preferred window size for the launcher.");

    const QStringList sizes = { "1120x677", "1400x846", "1600x967", "1920x1160" };
    auto* dd = new ImageDropdown(
        { "Small (1120x677)", "Default (1400x846)", "Large (1600x967)", "4K (1920x1160)" }, this);

    // Select the entry matching the stored size (default to "Default").
    int idx = sizes.indexOf(Config::instance().launcher_size());
    if (idx < 0) idx = 1;
    dd->set_index(idx);
    dd->move(util::layout::settings::ctrl_pos(w, util::layout::settings::k_row4_y));
    dd->raise();

    connect(dd, &ImageDropdown::changed, this, [sizes](int i)
    {
        if (i >= 0 && i < sizes.size()) Config::instance().set_launcher_size(sizes[i]);
    });
}

void LauncherSettings::setup_github_button()
{
    const QSize w = window()->size();
    auto* gh = new QPushButton("GITHUB", this);
    gh->setCursor(Qt::PointingHandCursor);
    gh->setStyleSheet(util::styles::k_neutral_button);
    gh->setGeometry(util::layout::settings::footer_left(w));
    connect(gh, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl("https://github.com/Story-Of-Alicia/soa-launcher-qt"));
    });
}

void LauncherSettings::setup_log_button()
{
    const QSize w = window()->size();
    auto* show_log = new QPushButton("SHOW LOG", this);
    show_log->setCursor(Qt::PointingHandCursor);
    show_log->setStyleSheet(util::styles::k_neutral_button);
    show_log->setGeometry(util::layout::settings::footer_right(w));
    connect(show_log, &QPushButton::clicked, this, []()
    {
        LauncherLog::instance()->show();
        LauncherLog::instance()->raise();
    });
}