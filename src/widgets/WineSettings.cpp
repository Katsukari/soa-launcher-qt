#include "widgets/WineSettings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"

#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>

#include "widgets/WineTerminal.hpp"

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

    const char* k_browse_style =
        "QPushButton"
        "{"
        "    background: #D8CDC0;"
        "    border: none;"
        "    border-radius: 6px;"
        "    color: #4F1717;"
        "    font-family: 'Eurostile';"
        "    font-weight: 900;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background: #E6DCD0; }";

    const char* k_primary_style =
        "QPushButton"
        "{"
        "    background: #2FB4E0;"
        "    border: none;"
        "    border-radius: 6px;"
        "    color: #FFFFFF;"
        "    font-family: 'Eurostile';"
        "    font-weight: 900;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background: #4FC4EF; }";

    const char* k_term_style =
        "QPlainTextEdit"
        "{"
        "    background: #1E1B17;"
        "    border: 1px solid #3A332B;"
        "    border-radius: 6px;"
        "    color: #D8C9B8;"
        "    font-family: 'monospace';"
        "    font-size: 12px;"
        "}";
}

WineSettings::WineSettings(QWidget* parent) : QWidget(parent)
{
    log_window = new WineTerminal(this);
    setup_dxvk_option();
    setup_prefix_option();
    setup_wine_binary_option();
    setup_wine_args_option();
    setup_log_button();
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
        const auto& a = on ? util::assets::buttons[util::assets::Button::SliderOn] : util::assets::buttons[util::assets::Button::SliderOff];
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
    const int    bw  = util::layout::scaled(34, w);   // browse
    const int    gw  = util::layout::scaled(74, w);   // generate
    const int    field_w = cw - bw - gw - 2 * gap;

    auto* field = new QLineEdit(this);
    field->setText("~/.wine-alicia");
    field->setStyleSheet(k_field_style);
    field->setGeometry(cp.x(), cp.y(), field_w, h);

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(k_browse_style);
    browse->setGeometry(cp.x() + field_w + gap, cp.y(), bw, h);
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select Wine Prefix Folder");
        if (!dir.isEmpty()) field->setText(dir);
    });

    auto* generate = new QPushButton("GENERATE", this);
    generate->setCursor(Qt::PointingHandCursor);
    generate->setStyleSheet(k_primary_style);
    generate->setGeometry(cp.x() + field_w + bw + 2 * gap, cp.y(), gw, h);

    connect(generate, &QPushButton::clicked, this, [this]
    {
        log_window->show();
        log_window->raise();
        log_window->append_line("> generating wine prefix... (not yet wired to backend)");
        // TODO: QProcess wineboot/winetricks, stream stdout via log_window->append_line(...)
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
    field->setStyleSheet(k_field_style);
    field->setGeometry(cp.x(), cp.y(), field_w, h);

    auto* browse = new QPushButton("...", this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(k_browse_style);
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
    field->setStyleSheet(k_field_style);
    field->setGeometry(cp.x(), cp.y(), util::layout::settings::ctrl_w(w), util::layout::scaled(34, w));
}

void WineSettings::setup_log_button()
{
    const QSize w = window()->size();
    auto* show_log = new QPushButton("SHOW LOG", this);
    show_log->setCursor(Qt::PointingHandCursor);
    show_log->setStyleSheet(k_browse_style);

    const QPoint cp = util::layout::settings::ctrl_pos(w, 496);
    show_log->setGeometry(util::layout::scaled(util::layout::settings::k_text_x, w), util::layout::scaled(496, w),
        util::layout::scaled(110, w), util::layout::scaled(30, w));
    connect(show_log, &QPushButton::clicked, this, [this]()
    {
        log_window->show();
        log_window->raise();
    });
}