#include "widgets/WineSettings.hpp"
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <functional>

#include "core/wine/Shell.hpp"
#include "core/wine/WineRegistry.hpp"
#include "core/runtime/RuntimeProvider.hpp"
#include "util/Assets.hpp"
#include "util/Config.hpp"
#include "util/Layout.hpp"
#include "util/LanguageManager.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"

using util::config::Config;
namespace ls = util::layout::settings;
namespace wset = util::layout::wine_settings;
namespace cw = core::wine;

namespace
{
    void sync_field(QLineEdit* field, const std::function<QString()>& getter)
    {
        QObject::connect(&Config::instance(), &Config::changed, field, [field, getter]()
        {
            if (!field->hasFocus()) field->setText(getter());
        });
    }

#if defined(Q_OS_MACOS)
    QString runtime_override_display()
    {
        const QString selected = Config::instance().wine_binary().trimmed();
        const QString launcherDefault =
            core::runtime::RuntimeProvider::default_selector().trimmed();
        return selected == launcherDefault ? QString() : selected;
    }
#endif
}

WineSettings::WineSettings(core::wine::Shell* shell_, QWidget* parent)
    : QWidget(parent), shell(shell_)
{
    setup_dxvk_option();
    setup_prefix_option();
    setup_wine_binary_option();
#if defined(Q_OS_MACOS)
    setup_macos_components_option();
#else
    setup_tricks_option();
#endif
    setup_wine_args_option();
}

void WineSettings::setup_dxvk_option()
{
    const QSize w = window()->size();
    const int y = wset::row(0);
#if defined(Q_OS_MACOS)
    util::simple_utils::make_label_block(this, w, y,
        "USE DXVK",
        "Unavailable in the current macOS runtime. The launcher keeps this off.");

    auto* slider = util::simple_utils::make_flat_button(this);
    const QRect sr = ls::slider_rect(w, y);
    slider->setIconSize(sr.size());
    slider->setGeometry(sr);
    slider->setIcon(QIcon(
        util::assets::button(util::assets::Button::SliderOff).normal.scaled(
            sr.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    slider->setEnabled(false);
    slider->setAccessibleName(QStringLiteral("Use DXVK"));
    slider->setAccessibleDescription(
        QStringLiteral("Unavailable for the current macOS runtime"));
    slider->setToolTip(QStringLiteral(
        "DXVK is not included in the current macOS runtime and cannot be enabled."));
#else
    util::simple_utils::make_label_block(this, w, y,
        "USE DXVK", "Translate Direct3D to Vulkan for better performance.");

    auto* slider = util::simple_utils::make_flat_button(this);
    const QRect sr = ls::slider_rect(w, y);
    const QSize ssz = sr.size();
    slider->setIconSize(ssz);
    slider->setGeometry(sr);
    auto paint = [slider, ssz](const bool on)
    {
        const auto& a = on ? util::assets::button(util::assets::Button::SliderOn)
                           : util::assets::button(util::assets::Button::SliderOff);
        slider->setIcon(QIcon(a.normal.scaled(ssz, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    };
    paint(Config::instance().use_dxvk());
    connect(slider, &QPushButton::clicked, this, [this, paint]()
    {
        const bool on = !Config::instance().use_dxvk();
        Config::instance().set_use_dxvk(on);
        paint(on);
        shell->sync_dxvk();
    });
    connect(&Config::instance(), &Config::changed, slider,
            [paint]() { paint(Config::instance().use_dxvk()); });
#endif
}

void WineSettings::setup_prefix_option()
{
    const QSize w = window()->size();
    const int y = wset::row(1);
#if defined(Q_OS_MACOS)
    const QString description = QStringLiteral(
        "Launcher-owned 64-bit compatibility prefix. Keep it under Application Support unless testing a clean prefix.");
    const QString title = QStringLiteral("RUNTIME PREFIX");
#else
    const QString description = QStringLiteral("The isolated Wine environment the game runs in.");
    const QString title = QStringLiteral("WINE PREFIX");
#endif
    util::simple_utils::make_label_block(this, w, y, title, description);

    auto* field = new QLineEdit(this);
    field->setText(Config::instance().wine_prefix());
    field->setStyleSheet(util::styles::k_field);
#if defined(Q_OS_MACOS)
    const int chooseWidth = util::layout::scaled(76, w);
    const int inputGap = util::layout::scaled(6, w);
    const int inputHeight = util::layout::scaled(34, w);
    const int controlX = ls::ctrl_x(w);
    const int controlWidth = ls::ctrl_w(w);
    field->setGeometry(controlX, util::layout::scaled(y, w),
                       controlWidth - chooseWidth - inputGap, inputHeight);
#else
    field->setGeometry(ls::field_rect(w, y));
#endif
    sync_field(field, []() { return Config::instance().wine_prefix(); });

#if defined(Q_OS_MACOS)
    auto* browse = new QPushButton(QStringLiteral("Choose"), this);
#else
    auto* browse = new QPushButton(QStringLiteral("..."), this);
#endif
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
#if defined(Q_OS_MACOS)
    browse->setGeometry(controlX + controlWidth - chooseWidth,
                        util::layout::scaled(y, w), chooseWidth, inputHeight);
    browse->setAccessibleName(QStringLiteral("Choose runtime prefix folder"));
#else
    browse->setGeometry(ls::browse_rect(w, y));
#endif
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
#if defined(Q_OS_MACOS)
        const QString title = QStringLiteral("Select Runtime Prefix Folder");
#else
        const QString title = QStringLiteral("Select Wine Prefix Folder");
#endif
        const QString dir = QFileDialog::getExistingDirectory(
            this, util::i18n::translate(title));
        if (!dir.isEmpty())
        {
            field->setText(dir);
            Config::instance().set_wine_prefix(dir);
        }
    });
    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        if (field->text() != Config::instance().wine_prefix())
            Config::instance().set_wine_prefix(field->text());
    });
}

void WineSettings::setup_wine_binary_option()
{
    const QSize w = window()->size();
    const int y = wset::row(2);
#if defined(Q_OS_MACOS)
    util::simple_utils::make_label_block(this, w, y,
        "CUSTOM RUNTIME OVERRIDE",
        "Optional. Leave blank to use the runtime included with the launcher.");
#else
    util::simple_utils::make_label_block(this, w, y,
        "CUSTOM WINE / PROTON",
        "A wine binary, or a Proton folder's \"proton\" script. Blank uses system wine.");
#endif

    auto* field = new QLineEdit(this);
#if defined(Q_OS_MACOS)
    field->setPlaceholderText(QStringLiteral("Bundled runtime (default)"));
#else
    field->setPlaceholderText(QStringLiteral("system wine"));
#endif
    field->setStyleSheet(util::styles::k_field);
#if defined(Q_OS_MACOS)
    const int chooseWidth = util::layout::scaled(76, w);
    const int inputGap = util::layout::scaled(6, w);
    const int inputHeight = util::layout::scaled(34, w);
    const int controlX = ls::ctrl_x(w);
    const int controlWidth = ls::ctrl_w(w);
    field->setGeometry(controlX, util::layout::scaled(y, w),
                       controlWidth - chooseWidth - inputGap, inputHeight);
#else
    field->setGeometry(ls::field_rect(w, y));
#endif
#if defined(Q_OS_MACOS)
    field->setText(runtime_override_display());
    sync_field(field, []() { return runtime_override_display(); });
#else
    field->setText(Config::instance().wine_binary());
    sync_field(field, []() { return Config::instance().wine_binary(); });
#endif

    auto apply = [this, field](const QString& raw)
    {
        QString path = raw.trimmed();
#if defined(Q_OS_MACOS)
        if (path.isEmpty())
            path = core::runtime::RuntimeProvider::default_selector();
#else
        const QFileInfo info(path);
        if (info.isFile() && info.fileName() == QStringLiteral("proton"))
            path = info.absolutePath();
#endif
        if (!path.isEmpty())
        {
            cw::WineInstall install;
            QString error;
            if (!cw::WineRegistry::inspect_path(path, install, &error))
            {
                QMessageBox::warning(this, QStringLiteral("Runtime Not Usable"),
                    error.isEmpty() ? QStringLiteral("The selected runtime could not be used.") : error);
#if defined(Q_OS_MACOS)
                field->setText(runtime_override_display());
#else
                field->setText(Config::instance().wine_binary());
#endif
                return;
            }
        }
        if (path == Config::instance().wine_binary())
        {
#if defined(Q_OS_MACOS)
            field->setText(runtime_override_display());
#endif
            return;
        }
        Config::instance().set_wine_binary(path);
        Config::instance().set_runtime_selected(!path.isEmpty());
#if defined(Q_OS_MACOS)
        Config::instance().set_setup_runtime_preference(QStringLiteral("wine"));
#endif
#if defined(Q_OS_MACOS)
        field->setText(runtime_override_display());
#else
        field->setText(path);
#endif
    };

#if defined(Q_OS_MACOS)
    auto* browse = new QPushButton(QStringLiteral("Choose"), this);
#else
    auto* browse = new QPushButton(QStringLiteral("..."), this);
#endif
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
#if defined(Q_OS_MACOS)
    browse->setGeometry(controlX + controlWidth - chooseWidth,
                        util::layout::scaled(y, w), chooseWidth, inputHeight);
    browse->setAccessibleName(QStringLiteral("Choose custom macOS runtime"));
#else
    browse->setGeometry(ls::browse_rect(w, y));
#endif
    connect(browse, &QPushButton::clicked, this, [this, apply]()
    {
#if defined(Q_OS_MACOS)
        QMessageBox choice(QMessageBox::Question, QStringLiteral("Select Custom macOS Runtime"),
            QStringLiteral("Select a runtime application, executable, or folder. Clear the field to restore the bundled default."),
            QMessageBox::NoButton, this);
        QPushButton* folderButton = choice.addButton(QStringLiteral("Runtime Folder"), QMessageBox::AcceptRole);
        QPushButton* fileButton = choice.addButton(QStringLiteral("Runtime App or Executable"), QMessageBox::ActionRole);
        choice.addButton(QMessageBox::Cancel);
        choice.exec();
        QString path;
        if (choice.clickedButton() == folderButton)
            path = QFileDialog::getExistingDirectory(
                this, util::i18n::translate("Select Runtime Folder"));
        else if (choice.clickedButton() == fileButton)
            path = QFileDialog::getOpenFileName(
                this,
                util::i18n::translate("Select Runtime App or Executable"),
                QStringLiteral("/Applications"),
                util::i18n::translate("Applications (*.app);;All Files (*)"));
        if (!path.isEmpty()) apply(path);
#else
        const QString file = QFileDialog::getOpenFileName(
            this, util::i18n::translate("Select Wine Binary or Proton Script"));
        if (!file.isEmpty()) apply(file);
#endif
    });
    connect(field, &QLineEdit::editingFinished, this, [field, apply]() { apply(field->text()); });
}

void WineSettings::setup_tricks_option()
{
#if !defined(Q_OS_MACOS)
    const QSize w = window()->size();
    const int y = wset::row(3);
    util::simple_utils::make_label_block(this, w, y,
        "WINETRICKS", "Path to Winetricks. Blank uses the one on PATH.");

    auto* field = new QLineEdit(this);
    field->setPlaceholderText(QStringLiteral("from PATH"));
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::field_rect(w, y));
    field->setText(Config::instance().winetricks_binary());
    sync_field(field, []() { return Config::instance().winetricks_binary(); });

    auto* browse = new QPushButton(QStringLiteral("..."), this);
    browse->setCursor(Qt::PointingHandCursor);
    browse->setStyleSheet(util::styles::k_neutral_button);
    browse->setGeometry(ls::browse_rect(w, y));
    connect(browse, &QPushButton::clicked, this, [this, field]()
    {
        const QString file = QFileDialog::getOpenFileName(
            this, util::i18n::translate("Select Winetricks"));
        if (!file.isEmpty())
        {
            Config::instance().set_winetricks_binary(file);
            field->setText(file);
        }
    });
    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        if (field->text() != Config::instance().winetricks_binary())
            Config::instance().set_winetricks_binary(field->text());
    });
#endif
}

void WineSettings::setup_macos_components_option()
{
#if defined(Q_OS_MACOS)
    const QSize w = window()->size();
    const int y = wset::row(3);
    util::simple_utils::make_label_block(this, w, y,
        "RUNTIME COMPONENTS",
        "Game-local DirectX, Visual C++ and PhysX components are checked before every launch.");

    auto* status = new QLabel(QStringLiteral("Managed automatically"), this);
    status->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    status->setStyleSheet(
        QStringLiteral("QLabel { background: rgba(216,205,192,0.62); "
                       "border: 1px solid #C9BBAA; border-radius: 6px; "
                       "padding: 0 8px; color: #6B5148; font-family: 'Inter'; }"));
    status->setGeometry(ls::ctrl_x(w), util::layout::scaled(y, w),
                        ls::ctrl_w(w), util::layout::scaled(34, w));
    status->setAccessibleName(QStringLiteral("Runtime components"));
    status->setAccessibleDescription(
        QStringLiteral("Managed automatically and verified before launch"));
#endif
}


void WineSettings::setup_wine_args_option()
{
    const QSize w = window()->size();
    const int y = wset::row(4);
#if defined(Q_OS_MACOS)
    const QString title = QStringLiteral("RUNTIME ENVIRONMENT VARIABLES");
    const QString description = QStringLiteral(
        "Optional non-reserved KEY=VALUE entries. Launcher-owned runtime, translation, graphics and prefix variables cannot be overridden.");
#else
    const QString title = QStringLiteral("WINE ENVIRONMENT VARIABLES");
    const QString description = QStringLiteral(
        "Space-separated KEY=VALUE entries, for example WINEDEBUG=-all.");
#endif
    util::simple_utils::make_label_block(this, w, y, title, description);

    auto* field = new QLineEdit(this);
#if defined(Q_OS_MACOS)
    field->setPlaceholderText(QStringLiteral("CUSTOM_OPTION=1"));
#else
    field->setPlaceholderText(QStringLiteral("WINEESYNC=1"));
#endif
    field->setStyleSheet(util::styles::k_field);
    field->setGeometry(ls::ctrl_pos(w, y).x(), ls::ctrl_pos(w, y).y(),
                       ls::ctrl_w(w), util::layout::scaled(34, w));
    field->setText(Config::instance().wine_args());
    sync_field(field, []() { return Config::instance().wine_args(); });
    connect(field, &QLineEdit::editingFinished, this, [field]()
    {
        if (field->text() != Config::instance().wine_args())
            Config::instance().set_wine_args(field->text());
    });
}
