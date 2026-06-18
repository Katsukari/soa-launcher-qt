#include "widgets/WineSettings.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QScrollBar>

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

    const char* k_term_btn_style =
        "QPushButton"
        "{"
        "    background: transparent;"
        "    border: 1px solid #4A4138;"
        "    border-radius: 4px;"
        "    color: #B8A894;"
        "    font-family: 'Inter';"
        "    font-size: 10px;"
        "    padding: 1px 6px;"
        "}"
        "QPushButton:hover { color: #E8D8C4; border-color: #6A5E50; }";
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
    utils::make_label_block(this, w, layout::settings::k_row1_y,
                            "USE DXVK",
                            "Translate Direct3D to Vulkan for better performance.");

    auto* slider = new QPushButton(this);
    slider->setFlat(true);
    slider->setCursor(Qt::PointingHandCursor);
    slider->setStyleSheet("border:none; background:transparent;");
    const QRect sr = layout::settings::slider_rect(w, layout::settings::k_row1_y);
    const QSize ssz = sr.size();
    slider->setIcon(QIcon(assets::buttons[assets::Button::SliderOn].normal
        .scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));   // on by default
    slider->setIconSize(ssz);
    slider->setGeometry(sr);
    connect(slider, &QPushButton::clicked, this, [slider, ssz]()
    {
        static bool on = true;
        on = !on;
        const auto& a = on ? assets::buttons[assets::Button::SliderOn] : assets::buttons[assets::Button::SliderOff];
        slider->setIcon(QIcon(a.normal.scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    });
}

void WineSettings::setup_prefix_option()
{
    const QSize w = window()->size();
    utils::make_label_block(this, w, layout::settings::k_row2_y,
                            "WINE PREFIX",
                            "The isolated Wine environment the game runs in.");

    const QPoint cp  = layout::settings::ctrl_pos(w, layout::settings::k_row2_y);
    const int    cw  = layout::settings::ctrl_w(w);
    const int    h   = layout::scaled(34, w);
    const int    gap = layout::scaled(6, w);
    const int    bw  = layout::scaled(34, w);   // browse
    const int    gw  = layout::scaled(74, w);   // generate
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
    utils::make_label_block(this, w, layout::settings::k_row3_y,
                            "CUSTOM WINE / PROTON",
                            "Path to a wine or proton binary. Blank uses system wine.");

    const QPoint cp  = layout::settings::ctrl_pos(w, layout::settings::k_row3_y);
    const int    cw  = layout::settings::ctrl_w(w);
    const int    h   = layout::scaled(34, w);
    const int    gap = layout::scaled(6, w);
    const int    bw  = layout::scaled(34, w);
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
    utils::make_label_block(this, w, layout::settings::k_row4_y,
                            "WINE LAUNCH ARGUMENTS",
                            "Extra flags passed to Wine. Leave blank if unsure.");

    const QPoint cp = layout::settings::ctrl_pos(w, layout::settings::k_row4_y);
    auto* field = new QLineEdit(this);
    field->setPlaceholderText("e.g. WINEDEBUG=-all");
    field->setStyleSheet(k_field_style);
    field->setGeometry(cp.x(), cp.y(), layout::settings::ctrl_w(w), layout::scaled(34, w));
}

void WineSettings::setup_log_button()
{
    const QSize w = window()->size();
    auto* show_log = new QPushButton("SHOW LOG", this);
    show_log->setCursor(Qt::PointingHandCursor);
    show_log->setStyleSheet(k_browse_style);

    const QPoint cp = layout::settings::ctrl_pos(w, 496);
    show_log->setGeometry(layout::scaled(layout::settings::k_text_x, w), layout::scaled(496, w),
        layout::scaled(110, w), layout::scaled(30, w));
    connect(show_log, &QPushButton::clicked, this, [this]()
    {
        log_window->show();
        log_window->raise();
    });
}