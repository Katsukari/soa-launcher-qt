#include "widgets/LauncherUpdate.hpp"

#include "util/Assets.hpp"
#include "util/Colors.hpp"
#include "util/LanguageManager.hpp"
#include "util/Layout.hpp"
#include "util/ProgressBar.hpp"
#include "util/SimpleUtils.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVersionNumber>

namespace lu = util::layout::launcher_update;

namespace
{
    void fit_label(QLabel* label, const int base_size, const int minimum_size,
                   const bool word_wrap = true)
    {
        if (!label)
            return;
        QFont font = label->font();
        const QRect available(0, 0, qMax(1, label->width()), qMax(1, label->height()));
        int flags = Qt::AlignCenter;
        if (word_wrap)
            flags |= Qt::TextWordWrap;
        else
            flags |= Qt::TextSingleLine;
        int size = base_size;
        while (size > minimum_size)
        {
            font.setPixelSize(size);
            const QRect bounds = QFontMetrics(font).boundingRect(
                available, flags, label->text());
            if (bounds.height() <= available.height() && bounds.width() <= available.width())
                break;
            --size;
        }
        font.setPixelSize(size);
        label->setFont(font);
    }
}

LauncherUpdate::LauncherUpdate(QWidget* parent)
    : ModalOverlay(parent)
{
    set_keeps_chrome(false);
    setup_controls();
    connect(&util::i18n::LanguageManager::instance(),
            &util::i18n::LanguageManager::language_changed,
            this, [this]()
    {
        retranslate_content();
        update();
    });
}

void LauncherUpdate::setup_controls()
{
    const QSize w = window()->size();

    title_label = new QLabel(this);
    title_label->setGeometry(lu::title(w));
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setWordWrap(false);
    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(util::layout::scaled(
        util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet(QStringLiteral("color:#4F1717; background:transparent;"));

    message_label = new QLabel(this);
    message_label->setGeometry(lu::message(w));
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);
    QFont message_font = util::assets::fonts[util::assets::Font::Inter];
    message_font.setPixelSize(util::layout::scaled(
        util::layout::text::k_version, w));
    message_font.setWeight(QFont::Medium);
    message_label->setFont(message_font);
    message_label->setStyleSheet(QStringLiteral("color:#392518; background:transparent;"));

    details_label = new QLabel(this);
    details_label->setGeometry(lu::details(w, false));
    details_label->setAlignment(Qt::AlignCenter);
    details_label->setWordWrap(true);
    QFont details_font = util::assets::fonts[util::assets::Font::Inter];
    details_font.setPixelSize(util::layout::scaled(
        util::layout::text::k_desc, w));
    details_font.setWeight(QFont::Medium);
    details_label->setFont(details_font);
    details_label->setStyleSheet(QStringLiteral("color:#A08C7B; background:transparent;"));

    version_combo = new QComboBox(this);
    version_combo->setGeometry(lu::version_combo(w));
    version_combo->setCursor(Qt::PointingHandCursor);
    version_combo->setMaxVisibleItems(3);
    version_combo->setStyleSheet(QStringLiteral(
        "QComboBox { background:#F7F0EB; color:#4F1717; border:1px solid #A98678; "
        "border-radius:7px; padding:5px 12px; }"
        "QComboBox::drop-down { border:0; width:28px; }"
        "QComboBox QAbstractItemView { background:#F7F0EB; color:#4F1717; "
        "selection-background-color:#EBDCD3; selection-color:#4F1717; "
        "border:1px solid #A98678; outline:0; }"
        "QComboBox QAbstractItemView::item { min-height:34px; padding:3px 10px; }"));
    version_combo->hide();
    connect(version_combo, &QComboBox::currentTextChanged, this,
            [this](const QString& version)
    {
        if (!catalogue_mode || version.isEmpty())
            return;
        emit version_selected(version);
    });

    progress_label = new QLabel(this);
    progress_label->setGeometry(lu::progress_label(w));
    progress_label->setAlignment(Qt::AlignCenter);
    QFont progress_font = util::assets::fonts[util::assets::Font::Inter];
    progress_font.setPixelSize(util::layout::scaled(
        util::layout::text::k_status, w));
    progress_font.setWeight(QFont::DemiBold);
    progress_label->setFont(progress_font);
    progress_label->setStyleSheet(QStringLiteral("color:#4F1717; background:transparent;"));

    update_button = util::simple_utils::make_flat_button(this);
    update_button->setGeometry(lu::update_button(w, false, true));
    update_button->setIconSize(update_button->size());
    update_button->setProperty("soa_button_stretch_asset", true);
    update_button->installEventFilter(this);
    update_button->setAccessibleName(QStringLiteral("Update launcher now"));

    QFont button_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    button_font.setPixelSize(util::layout::scaled(
        util::layout::text::k_row_title, w));
    button_font.setWeight(QFont::Black);
    update_button_label = util::simple_utils::add_button_text(
        update_button, util::assets::Button::UpdateAvailable,
        QStringLiteral("UPDATE NOW"), button_font);

    cancel_button = util::simple_utils::make_flat_button(this);
    cancel_button->setGeometry(lu::cancel_button(w, false, false));
    cancel_button->setIconSize(cancel_button->size());
    cancel_button->setProperty("soa_button_stretch_asset", true);
    cancel_button->installEventFilter(this);
    QFont cancel_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    cancel_font.setPixelSize(util::layout::scaled(
        util::layout::text::k_status, w));
    cancel_font.setWeight(QFont::Black);
    util::simple_utils::add_button_text(
        cancel_button, util::assets::Button::Cancel,
        QStringLiteral("CANCEL"), cancel_font);

    close_button = util::simple_utils::make_flat_button(this);
    close_button->setGeometry(lu::close_button(w));
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    close_button->setIconSize(close_button->size());
    close_button->setAccessibleName(QStringLiteral("Postpone launcher update"));

    progress_timer = new QTimer(this);
    progress_timer->setInterval(40);
    connect(progress_timer, &QTimer::timeout, this, [this]()
    {
        progress_phase += 0.025;
        if (progress_phase > 1.0)
            progress_phase -= 1.0;
        update();
    });

    connect(update_button, &QPushButton::clicked, this, [this]()
    {
        if (!downloading_update && !starting_installer)
            emit update_requested();
    });
    connect(cancel_button, &QPushButton::clicked, this, [this]()
    {
        if (starting_installer || required_update)
            return;
        emit postponed();
    });
    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        if (!required_update && !downloading_update && !starting_installer)
            emit postponed();
    });

    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::set_versions(const QString& installed_version,
                                  const QStringList& versions,
                                  const bool catalogue_visible)
{
    current_version = installed_version;
    catalogue_mode = catalogue_visible && !versions.isEmpty();
    const QSignalBlocker blocker(version_combo);
    version_combo->clear();
    version_combo->addItems(versions);
    const int selected = version_combo->findText(release_version);
    if (selected >= 0)
        version_combo->setCurrentIndex(selected);
    else if (version_combo->count() > 0)
    {
        version_combo->setCurrentIndex(0);
        release_version = version_combo->currentText();
    }
    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::set_release(const QString& version, const bool required,
                                 const QString& message)
{
    release_version = version.trimmed();
    if (release_version.isEmpty() && version_combo && version_combo->count() > 0)
        release_version = version_combo->currentText().trimmed();
    if (version_combo && version_combo->count() > 0)
    {
        const QSignalBlocker blocker(version_combo);
        const int selected = version_combo->findText(release_version);
        if (selected >= 0)
            version_combo->setCurrentIndex(selected);
    }
    required_update = required;
    release_message = message.trimmed();
    downloading_update = false;
    starting_installer = false;
    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::set_downloading(const bool downloading)
{
    downloading_update = downloading;
    starting_installer = false;
    if (downloading_update)
    {
        progress_fraction = 0.0;
        progress_indeterminate = true;
        progress_phase = 0.0;
        progress_timer->start();
        progress_label->setText(util::i18n::translate("Preparing download..."));
    }
    else
    {
        progress_indeterminate = false;
        progress_timer->stop();
    }
    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::set_progress(const qint64 received, const qint64 total)
{
    if (!downloading_update)
        return;
    if (total > 0)
    {
        progress_indeterminate = false;
        progress_timer->stop();
        progress_fraction = qBound(0.0,
                                   static_cast<double>(received) / static_cast<double>(total),
                                   1.0);
        const double received_mb = static_cast<double>(received) / (1024.0 * 1024.0);
        const double total_mb = static_cast<double>(total) / (1024.0 * 1024.0);
        progress_label->setText(util::i18n::translate("%1 MB of %2 MB")
                                    .arg(QString::number(received_mb, 'f', 1),
                                         QString::number(total_mb, 'f', 1)));
    }
    else
    {
        progress_indeterminate = true;
        if (!progress_timer->isActive())
            progress_timer->start();
        const double received_mb = static_cast<double>(received) / (1024.0 * 1024.0);
        progress_label->setText(util::i18n::translate("%1 MB downloaded")
                                    .arg(QString::number(received_mb, 'f', 1)));
    }
    update();
}

void LauncherUpdate::set_starting_installer()
{
    downloading_update = false;
    starting_installer = true;
    progress_fraction = 1.0;
    progress_indeterminate = false;
    progress_timer->stop();
    progress_label->setText(util::i18n::translate("Starting installer..."));
    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::refresh_layout()
{
    const QSize w = window()->size();
    const bool progress_visible = downloading_update || starting_installer;
    details_label->setGeometry(lu::details(w, catalogue_mode));
    progress_label->setVisible(progress_visible);
    version_combo->setVisible(catalogue_mode && !progress_visible);
    const bool cancel_visible = !required_update && !starting_installer;
    cancel_button->setGeometry(
        lu::cancel_button(w, catalogue_mode, downloading_update));
    cancel_button->setIconSize(cancel_button->size());
    cancel_button->setVisible(cancel_visible);
    cancel_button->setEnabled(cancel_visible);
    update_button->setGeometry(
        lu::update_button(w, catalogue_mode, cancel_visible));
    update_button->setIconSize(update_button->size());
    update_button_label->setGeometry(update_button->rect());
    update_button->setVisible(!progress_visible);
    update_button->setEnabled(!progress_visible);
    close_button->setVisible(!required_update && !progress_visible);
    close_button->setEnabled(!progress_visible);
    util::simple_utils::refresh_button(update_button);
    util::simple_utils::refresh_button(cancel_button);
    update_button_label->raise();
    cancel_button->raise();
    update();
}

void LauncherUpdate::retranslate_content()
{
    QString shown_version = release_version.trimmed();
    if (shown_version.isEmpty() && version_combo)
        shown_version = version_combo->currentText().trimmed();
    if (shown_version.isEmpty())
        shown_version = current_version.trimmed();
    if (shown_version.isEmpty())
        shown_version = QCoreApplication::applicationVersion().trimmed();
    if (starting_installer)
    {
        title_label->setText(util::i18n::translate("STARTING LAUNCHER UPDATE"));
        message_label->setText(util::i18n::translate(
            "The installer is ready. The launcher will close automatically."));
        details_label->setText(util::i18n::translate(
            "Complete the installer, then open Story of Alicia again."));
        set_update_button_text(QStringLiteral("STARTING..."));
    }
    else if (downloading_update)
    {
        title_label->setText(util::i18n::translate("DOWNLOADING LAUNCHER UPDATE"));
        message_label->setText(util::i18n::translate("Downloading version %1...")
                                   .arg(shown_version));
        details_label->setText(util::i18n::translate(
            "The update is verified before it is installed."));
        set_update_button_text(QStringLiteral("DOWNLOADING..."));
    }
    else
    {
        title_label->setText(util::i18n::translate(catalogue_mode
            ? "LAUNCHER VERSIONS"
            : required_update ? "LAUNCHER UPDATE REQUIRED"
                              : "LAUNCHER UPDATE AVAILABLE"));
        if (catalogue_mode)
        {
            message_label->setText(util::i18n::translate(
                "Choose from up to three signed launcher releases."));
        }
        else if (required_update)
        {
            message_label->setText(util::i18n::translate(
                "Version %1 is available. You must update the launcher before continuing.",
                shown_version));
        }
        else
        {
            message_label->setText(util::i18n::translate(
                "Version %1 is available for the launcher.", shown_version));
        }
#if defined(Q_OS_MACOS)
        const QString default_details = util::i18n::translate(
            "The installer will open automatically. The launcher will close.");
#else
        const QString default_details = util::i18n::translate(
            "The AppImage will update and restart automatically.");
#endif
        details_label->setText(catalogue_mode
            ? util::i18n::translate("Installed: %1 · Selected: %2")
                .arg(current_version, shown_version)
            : release_message.isEmpty() ? default_details : release_message);
        if (!catalogue_mode)
            set_update_button_text(QStringLiteral("UPDATE NOW"));
        else if (shown_version == current_version)
            set_update_button_text(QStringLiteral("REINSTALL VERSION"));
        else if (QVersionNumber::compare(QVersionNumber::fromString(shown_version),
                                         QVersionNumber::fromString(current_version)) < 0)
            set_update_button_text(QStringLiteral("DOWNGRADE"));
        else
            set_update_button_text(QStringLiteral("UPDATE NOW"));
    }

    fit_label(title_label,
              util::layout::scaled(util::layout::text::k_modal_header,
                                   window()->size()),
              14, false);
    fit_label(message_label,
              util::layout::scaled(util::layout::text::k_version,
                                   window()->size()),
              12);
    fit_label(details_label,
              util::layout::scaled(util::layout::text::k_desc,
                                   window()->size()),
              11);
    update_button->setAccessibleName(util::i18n::translate("Update launcher now"));
    util::simple_utils::set_button_text(cancel_button, QStringLiteral("CANCEL"));
    cancel_button->setAccessibleName(util::i18n::translate(
        downloading_update ? "Cancel launcher update download" : "Cancel launcher update"));
    close_button->setAccessibleName(util::i18n::translate("Postpone launcher update"));
}

void LauncherUpdate::set_update_button_text(const QString& source)
{
    util::simple_utils::set_button_text(update_button, source);
}

void LauncherUpdate::paint_content(QPainter& painter)
{
    painter.drawPixmap(lu::box(window()->size()),
                       util::assets::images[util::assets::Image::BoxUpdate]);
    if (downloading_update || starting_installer)
    {
        const QRect bar = lu::progress_bar(window()->size());
        if (downloading_update && progress_indeterminate)
            util::progress_bar::draw_indeterminate(painter, bar, progress_phase);
        else
            util::progress_bar::draw(painter, bar, progress_fraction);
    }
}

bool LauncherUpdate::eventFilter(QObject* object, QEvent* event)
{
    if (object == cancel_button && cancel_button->isEnabled())
    {
        const auto& asset = util::assets::button(util::assets::Button::Cancel);
        util::simple_utils::apply_button_state(
            event, cancel_button, asset.normal, asset.hover, asset.clicked);
    }
    else if (object == update_button && update_button->isEnabled())
    {
        const auto& asset = util::assets::button(
            util::assets::Button::UpdateAvailable);
        util::simple_utils::apply_button_state(
            event, update_button, asset.normal, asset.hover, asset.clicked);
    }
    return QWidget::eventFilter(object, event);
}
