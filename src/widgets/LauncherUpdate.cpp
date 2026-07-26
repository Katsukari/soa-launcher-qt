#include "widgets/LauncherUpdate.hpp"

#include "util/Assets.hpp"
#include "util/Colors.hpp"
#include "util/LanguageManager.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"

#include <QFontMetrics>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>

namespace
{
    constexpr QSize k_box_size {580, 326};

    QRect box_rect(const QSize window_size)
    {
        return util::layout::centered(k_box_size, window_size, 0, 0);
    }

    QRect local_rect(const QSize window_size, const QRect source)
    {
        return util::layout::scaled(source, window_size).translated(box_rect(window_size).topLeft());
    }

    void fit_label(QLabel* label, const int base_size, const int minimum_size)
    {
        if (!label)
            return;
        QFont font = label->font();
        const QRect available(0, 0, qMax(1, label->width()), qMax(1, label->height()));
        int size = base_size;
        while (size > minimum_size)
        {
            font.setPixelSize(size);
            const QRect bounds = QFontMetrics(font).boundingRect(
                available, Qt::AlignCenter | Qt::TextWordWrap, label->text());
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
    title_label->setGeometry(local_rect(w, {42, 30, 496, 40}));
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setWordWrap(true);
    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(util::layout::scaled(25, w));
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet(QStringLiteral("color:#4F1717; background:transparent;"));

    message_label = new QLabel(this);
    message_label->setGeometry(local_rect(w, {68, 82, 444, 62}));
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);
    QFont message_font = util::assets::fonts[util::assets::Font::Inter];
    message_font.setPixelSize(util::layout::scaled(16, w));
    message_font.setWeight(QFont::Medium);
    message_label->setFont(message_font);
    message_label->setStyleSheet(QStringLiteral("color:#392518; background:transparent;"));

    details_label = new QLabel(this);
    details_label->setGeometry(local_rect(w, {68, 142, 444, 54}));
    details_label->setAlignment(Qt::AlignCenter);
    details_label->setWordWrap(true);
    QFont details_font = util::assets::fonts[util::assets::Font::Inter];
    details_font.setPixelSize(util::layout::scaled(14, w));
    details_font.setWeight(QFont::Medium);
    details_label->setFont(details_font);
    details_label->setStyleSheet(QStringLiteral("color:#A08C7B; background:transparent;"));

    progress_bar = new QProgressBar(this);
    progress_bar->setGeometry(local_rect(w, {74, 188, 432, 18}));
    progress_bar->setRange(0, 1000);
    progress_bar->setTextVisible(false);
    progress_bar->setStyleSheet(QStringLiteral(
        "QProgressBar { background:#E4DED9; border:1px solid rgba(79,23,23,55); border-radius:5px; }"
        "QProgressBar::chunk { background:#23B4E1; border-radius:4px; }"));

    progress_label = new QLabel(this);
    progress_label->setGeometry(local_rect(w, {74, 208, 432, 20}));
    progress_label->setAlignment(Qt::AlignCenter);
    QFont progress_font = util::assets::fonts[util::assets::Font::Inter];
    progress_font.setPixelSize(util::layout::scaled(12, w));
    progress_font.setWeight(QFont::DemiBold);
    progress_label->setFont(progress_font);
    progress_label->setStyleSheet(QStringLiteral("color:#4F1717; background:transparent;"));

    update_button = util::simple_utils::make_flat_button(this);
    update_button->setGeometry(local_rect(w, {74, 199, 432, 67}));
    update_button->setIconSize(update_button->size());
    update_button->setProperty("soa_button_stretch_asset", true);
    update_button->installEventFilter(this);
    update_button->setAccessibleName(QStringLiteral("Update launcher now"));

    update_button_label = new QLabel(update_button);
    update_button_label->setGeometry(update_button->rect());
    update_button_label->setAlignment(Qt::AlignCenter);
    update_button_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont button_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    button_font.setPixelSize(util::layout::scaled(25, w));
    button_font.setWeight(QFont::Black);
    update_button_label->setFont(button_font);
    update_button_label->setStyleSheet(QStringLiteral(
        "QLabel { color:#FFFFFF; background:transparent; }"
        "QLabel:disabled { color:#FFFFFF; }"));
    update_button_label->raise();

    close_button = util::simple_utils::make_flat_button(this);
    close_button->setGeometry(local_rect(w, {536, 18, 26, 26}));
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    close_button->setIconSize(close_button->size());
    close_button->setAccessibleName(QStringLiteral("Postpone launcher update"));

    connect(update_button, &QPushButton::clicked, this, [this]()
    {
        if (!downloading_update && !starting_installer)
            emit update_requested();
    });
    connect(close_button, &QPushButton::clicked, this, [this]()
    {
        if (!required_update && !downloading_update && !starting_installer)
            emit postponed();
    });

    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::set_release(const QString& version, const bool required,
                                 const QString& message)
{
    release_version = version;
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
        progress_bar->setRange(0, 0);
        progress_label->setText(util::i18n::translate("Preparing download..."));
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
        progress_bar->setRange(0, 1000);
        const qint64 progress = qBound<qint64>(
            qint64{0},
            received * qint64{1000} / total,
            qint64{1000});
        progress_bar->setValue(static_cast<int>(progress));
        const double received_mb = static_cast<double>(received) / (1024.0 * 1024.0);
        const double total_mb = static_cast<double>(total) / (1024.0 * 1024.0);
        progress_label->setText(util::i18n::translate("%1 MB of %2 MB")
                                    .arg(QString::number(received_mb, 'f', 1),
                                         QString::number(total_mb, 'f', 1)));
    }
    else
    {
        progress_bar->setRange(0, 0);
        const double received_mb = static_cast<double>(received) / (1024.0 * 1024.0);
        progress_label->setText(util::i18n::translate("%1 MB downloaded")
                                    .arg(QString::number(received_mb, 'f', 1)));
    }
}

void LauncherUpdate::set_starting_installer()
{
    downloading_update = false;
    starting_installer = true;
    progress_bar->setRange(0, 0);
    progress_label->setText(util::i18n::translate("Starting installer..."));
    refresh_layout();
    retranslate_content();
}

void LauncherUpdate::refresh_layout()
{
    const QSize w = window()->size();
    const bool progress_visible = downloading_update || starting_installer;
    progress_bar->setVisible(progress_visible);
    progress_label->setVisible(progress_visible);
    update_button->setGeometry(local_rect(w, progress_visible
        ? QRect{74, 229, 432, 67}
        : QRect{74, 199, 432, 67}));
    update_button->setIconSize(update_button->size());
    update_button_label->setGeometry(update_button->rect());
    update_button->setEnabled(!progress_visible);
    update_button_label->setEnabled(true);
    close_button->setVisible(!required_update && !progress_visible);
    close_button->setEnabled(!progress_visible);
    set_button_pixmap(util::assets::translated_buttons[util::assets::Button::UpdateAvailable].normal);
    update_button_label->raise();
}

void LauncherUpdate::retranslate_content()
{
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
                                   .arg(release_version));
        details_label->setText(util::i18n::translate(
            "The update is verified before it is installed."));
        set_update_button_text(QStringLiteral("DOWNLOADING..."));
    }
    else
    {
        title_label->setText(util::i18n::translate(required_update
            ? "LAUNCHER UPDATE REQUIRED"
            : "LAUNCHER UPDATE AVAILABLE"));
        message_label->setText(util::i18n::translate(required_update
            ? "Version %1 is available. You must update the launcher before continuing."
            : "Version %1 is available for the launcher.").arg(release_version));
#if defined(Q_OS_MACOS)
        const QString default_details = util::i18n::translate(
            "The installer will open automatically. The launcher will close.");
#else
        const QString default_details = util::i18n::translate(
            "The AppImage will update and restart automatically.");
#endif
        details_label->setText(release_message.isEmpty() ? default_details : release_message);
        set_update_button_text(QStringLiteral("UPDATE NOW"));
    }

    fit_label(title_label, util::layout::scaled(25, window()->size()), 18);
    fit_label(message_label, util::layout::scaled(16, window()->size()), 12);
    fit_label(details_label, util::layout::scaled(14, window()->size()), 11);
    update_button->setAccessibleName(util::i18n::translate("Update launcher now"));
    close_button->setAccessibleName(util::i18n::translate("Postpone launcher update"));
}

void LauncherUpdate::set_update_button_text(const QString& source)
{
    update_button_source = source;
    update_button_label->setText(util::i18n::translate(source));
    fit_label(update_button_label, util::layout::scaled(25, window()->size()), 14);
}

void LauncherUpdate::set_button_pixmap(const QPixmap& pixmap)
{
    const QPixmap scaled = pixmap.scaled(update_button->iconSize(), Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);
    QIcon icon;
    icon.addPixmap(scaled, QIcon::Normal);
    icon.addPixmap(scaled, QIcon::Disabled);
    update_button->setIcon(icon);
}

void LauncherUpdate::paint_content(QPainter& painter)
{
    painter.drawPixmap(box_rect(window()->size()),
                       util::assets::images[util::assets::Image::BoxUpdate]);
}

bool LauncherUpdate::eventFilter(QObject* object, QEvent* event)
{
    if (object == update_button && update_button->isEnabled())
    {
        const auto& assets = util::assets::translated_buttons[util::assets::Button::UpdateAvailable];
        switch (event->type())
        {
            case QEvent::Enter:
                set_button_pixmap(assets.hover);
                break;
            case QEvent::Leave:
                set_button_pixmap(assets.normal);
                break;
            case QEvent::MouseButtonPress:
                set_button_pixmap(assets.clicked);
                break;
            case QEvent::MouseButtonRelease:
                set_button_pixmap(update_button->underMouse() ? assets.hover : assets.normal);
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(object, event);
}
