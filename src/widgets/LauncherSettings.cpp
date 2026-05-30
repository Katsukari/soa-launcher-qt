#include "widgets/LauncherSettings.hpp"
#include "widgets/ImageDropdown.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"

#include <QLabel>
#include <QPushButton>

namespace
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
}

LauncherSettings::LauncherSettings(QWidget* parent) : QWidget(parent)
{
    setup_title();
    setup_launch_on_startup_option();
    setup_after_game_start_option();
    setup_run_connectivity_test_option();
    setup_launcher_size_option();
}

void LauncherSettings::setup_title()
{
    const QSize w = window()->size();
    auto* title = new QLabel("LAUNCHER SETTINGS", this);
    title->setAlignment(Qt::AlignCenter);
    QFont f = assets::fonts[assets::Font::EurostileExtraBlack];
    f.setPixelSize(layout::scaled(layout::text::k_modal_header, w));
    f.setWeight(QFont::Black);
    title->setFont(f);
    title->setStyleSheet("color: #4F1717; background: transparent;");
    title->setGeometry(layout::settings::page_title(w));
}

void LauncherSettings::setup_launch_on_startup_option()
{
    const QSize w = window()->size();
    make_label_block(this, w, layout::settings::k_row1_y,
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
    make_label_block(this, w, layout::settings::k_row2_y,
        "AFTER GAME START",
        "Choose what the launcher does after the game starts up.");

    auto* dd = new ImageDropdown({ "Keep launcher open", "Minimize to tray" }, this);
    dd->move(layout::settings::ctrl_pos(w, layout::settings::k_row2_y));
}

void LauncherSettings::setup_run_connectivity_test_option()
{
    const QSize w = window()->size();
    make_label_block(this, w, layout::settings::k_row3_y,
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
    make_label_block(this, w, layout::settings::k_row4_y,
        "LAUNCHER SIZE",
        "Choose your preferred window size for the launcher.");

    auto* dd = new ImageDropdown(
        { "Small (1120x677)", "Default (1400x846)", "Large (1600x967)", "4K (1920x1160)" }, this);
    dd->set_index(1);   // Default
    dd->move(layout::settings::ctrl_pos(w, layout::settings::k_row4_y));
    dd->raise();
}

