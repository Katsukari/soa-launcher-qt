#include "widgets/AdvancedSettings.hpp"
#include "widgets/ImageDropdown.hpp"
#include "util/Assets.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include "util/Config.hpp"
#include "util/LaunchArguments.hpp"
#include <QIcon>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

#include "util/LanguageManager.hpp"
using util::config::Config;
namespace ls = util::layout::settings;
namespace aset = util::layout::advanced_settings;

namespace
{
#if defined(Q_OS_MACOS)
    constexpr int kAdvancedRowCount = 5;
#else
    constexpr int kAdvancedRowCount = 3;
#endif
}

AdvancedSettings::AdvancedSettings(QWidget* parent) : QWidget(parent)
{
    setup_game_path_option();
    setup_game_args_option();
#if defined(Q_OS_MACOS)
    setup_macos_compatibility_option();
    setup_macos_deep_diagnostics_option();
#endif
    setup_repair_option();
}
void AdvancedSettings::setup_game_path_option()
{
    const QSize w = window()->size();
    const int y = aset::row(0, kAdvancedRowCount);
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
            util::i18n::translate("Select Game Folder"),
            Config::instance().prefix_root());
        if (!dir.isEmpty())
        {
            if (!Config::instance().path_inside_prefix(dir))
            {
#if defined(Q_OS_MACOS)
                const QString message = QStringLiteral(
                    "The game folder must remain inside the selected runtime prefix.");
#else
                const QString message = QStringLiteral(
                    "The game folder must remain inside the selected Wine prefix.");
#endif
                QMessageBox::warning(
                    this,
                    QStringLiteral("Invalid Game Folder"),
                    message);
                return;
            }
            game_path_field->setText(dir);
            Config::instance().set_game_install_path(dir);
        }
    });
    connect(game_path_field, &QLineEdit::editingFinished, this, [this]()
    {
        const QString candidate = game_path_field->text().trimmed();
        if (candidate.isEmpty())
        {
            Config::instance().forget_game_install_path();
            game_path_field->setText(Config::instance().game_install_path());
            return;
        }
        if (!Config::instance().path_inside_prefix(candidate))
        {
#if defined(Q_OS_MACOS)
            const QString message = QStringLiteral(
                "The game folder must remain inside the selected runtime prefix.");
#else
            const QString message = QStringLiteral(
                "The game folder must remain inside the selected Wine prefix.");
#endif
            QMessageBox::warning(
                this,
                QStringLiteral("Invalid Game Folder"),
                message);
            game_path_field->setText(Config::instance().game_install_path());
            return;
        }
        if (candidate != Config::instance().game_install_path())
            Config::instance().set_game_install_path(candidate);
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
    const int y = aset::row(1, kAdvancedRowCount);
    util::simple_utils::make_label_block(this, w, y,
                            "GAME LAUNCH ARGUMENTS",
                            "Passed to Alicia.exe. Optional for most players.");
    auto* field = new QLineEdit(this);
    field->setPlaceholderText("Alicia.exe (default)");
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::ctrl_pos(w, y).x(), ls::ctrl_pos(w, y).y(),
                       ls::ctrl_w(w), util::layout::scaled(34, w));
    field->setText(Config::instance().game_args());
    connect(field, &QLineEdit::editingFinished, this, [this, field]()
    {
        const auto validation = util::launch_arguments::validate(field->text());
        if (!validation.valid)
        {
            QMessageBox::warning(
                this,
                QStringLiteral("Invalid Launch Arguments"),
                validation.error);
            field->setText(Config::instance().game_args());
            return;
        }
        Config::instance().set_game_args(field->text());
    });
}

void AdvancedSettings::setup_macos_compatibility_option()
{
#if defined(Q_OS_MACOS)
    const QSize w = window()->size();
    const int y = aset::row(2, kAdvancedRowCount);
    util::simple_utils::make_label_block(
        this, w, y,
        "COMPATIBILITY PROFILE",
        "Normal is recommended. Display fallbacks only change targeted graphics behavior.");

    const QStringList labels {
        QStringLiteral("Normal (recommended)"),
        QStringLiteral("Safe display"),
        QStringLiteral("Low graphics"),
        QStringLiteral("Mac GL fallback")
    };
    const QStringList ids {
        QStringLiteral("default"),
        QStringLiteral("safe-display"),
        QStringLiteral("low-graphics"),
        QStringLiteral("gl-behind")
    };

    auto* dropdown = new ImageDropdown(labels, this);
    dropdown->setAccessibleName(QStringLiteral("macOS compatibility profile"));
    dropdown->setAccessibleDescription(
        QStringLiteral("Select a targeted runtime compatibility profile"));
    dropdown->move(ls::ctrl_pos(w, y));
    const int current = ids.indexOf(Config::instance().macos_compatibility_profile());
    dropdown->set_index(current >= 0 ? current : 0);

    connect(dropdown, &ImageDropdown::changed, this, [ids](const int index)
    {
        if (index >= 0 && index < ids.size())
            Config::instance().set_macos_compatibility_profile(ids[index]);
    });
    connect(&Config::instance(), &Config::changed, dropdown, [dropdown, ids]()
    {
        const int index = ids.indexOf(
            Config::instance().macos_compatibility_profile());
        if (index >= 0)
            dropdown->set_index(index);
    });
#endif
}

void AdvancedSettings::setup_macos_deep_diagnostics_option()
{
#if defined(Q_OS_MACOS)
    const QSize w = window()->size();
    const int y = aset::row(3, kAdvancedRowCount);
    util::simple_utils::make_label_block(
        this, w, y,
        "DEVELOPER DEEP DIAGNOSTICS",
        "Adds verbose runtime traces and host sampling. Leave off for normal play.");

    auto* slider = util::simple_utils::make_flat_button(this);
    const QRect geometry = ls::slider_rect(w, y);
    slider->setGeometry(geometry);
    slider->setIconSize(geometry.size());
    slider->setAccessibleName(QStringLiteral("Developer deep diagnostics"));
    slider->setAccessibleDescription(
        QStringLiteral("Enable verbose launch traces and host sampling"));

    const auto paint = [slider, size = geometry.size()](const bool enabled)
    {
        const auto& asset = enabled
            ? util::assets::button(util::assets::Button::SliderOn)
            : util::assets::button(util::assets::Button::SliderOff);
        slider->setIcon(QIcon(asset.normal.scaled(
            size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    };
    paint(Config::instance().macos_deep_diagnostics());
    connect(slider, &QPushButton::clicked, this, [paint]()
    {
        Config::instance().set_macos_deep_diagnostics(
            !Config::instance().macos_deep_diagnostics());
        paint(Config::instance().macos_deep_diagnostics());
    });
    connect(&Config::instance(), &Config::changed, slider, [paint]()
    {
        paint(Config::instance().macos_deep_diagnostics());
    });
#endif
}

void AdvancedSettings::setup_repair_option()
{
    const QSize w = window()->size();
    const int y = aset::row(kAdvancedRowCount - 1, kAdvancedRowCount);
    util::simple_utils::make_label_block(
        this, w, y,
        "VERIFY AND REPAIR GAME",
        "Check the selected game's manifest and redownload missing or damaged files.");

    const auto& asset = util::assets::button(util::assets::Button::Repair);
    const QSize button_size = util::layout::scaled(asset.normal.size(), w);
    const QPoint control_pos = ls::ctrl_pos(w, y);
    const int x = control_pos.x() + (ls::ctrl_w(w) - button_size.width()) / 2;

    repair_button = util::simple_utils::make_flat_button(this);
    repair_button->setText(QString{});
    repair_button->setIcon(QIcon(asset.normal));
    repair_button->setIconSize(button_size);
    repair_button->setGeometry(x, control_pos.y(), button_size.width(), button_size.height());
    QFont repair_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    repair_font.setPixelSize(util::layout::scaled(11, w));
    repair_font.setWeight(QFont::Black);
    util::simple_utils::add_button_text(repair_button, util::assets::Button::Repair, QStringLiteral("REPAIR FILES"), repair_font);
    repair_button->setEnabled(Config::instance().game_installed());
    repair_button->setAccessibleName(QStringLiteral("Verify and repair selected game"));
    repair_button->installEventFilter(this);

    connect(repair_button, &QPushButton::clicked, this, &AdvancedSettings::repair_requested);
    connect(&Config::instance(), &Config::changed, repair_button, [this]()
    {
        repair_button->setEnabled(Config::instance().game_installed());
    });
}

bool AdvancedSettings::eventFilter(QObject* object, QEvent* event)
{
    if (object == repair_button && repair_button->isEnabled())
    {
        const auto& asset = util::assets::button(util::assets::Button::Repair);
        util::simple_utils::apply_button_state(
            event, repair_button, asset.normal, asset.hover, asset.clicked);
    }
    return QWidget::eventFilter(object, event);
}
