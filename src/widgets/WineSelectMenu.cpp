#include "widgets/WineSelectMenu.hpp"
#include "util/LanguageManager.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "core/Log.hpp"
#include "core/wine/MacWineRuntime.hpp"
#include "core/wine/WineRegistry.hpp"
#include "util/Assets.hpp"
#include "util/Colors.hpp"
#include "util/Config.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Styles.hpp"
#include <spdlog/spdlog.h>

using util::config::Config;
namespace cw = core::wine;
namespace im = util::layout::install_modal;

namespace
{
    const char* k_row =
        "QPushButton { text-align: left; padding: 10px 14px; border: 1px solid #C9BBAA;"
        "    border-radius: 8px; background: #FFFFFF; color: #392518;"
        "    font-family: 'Inter'; font-size: 14px; }"
        "QPushButton:hover { border-color: #2FB4E0; }"
        "QPushButton:checked { border: 2px solid #2FB4E0; background: #EAF7FC; }"
        "QPushButton:disabled { color: #8A7A6B; background: #F5F1ED; }";
    const char* k_scroll =
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 8px; background: transparent; margin: 0; }"
        "QScrollBar::handle:vertical { background: #C9BBAA; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }";
    const char* k_status =
        "QLabel { color: #6B5B4D; font-family: 'Inter'; font-size: 12px; background: transparent; }";
    const char* k_empty =
        "QLabel { color: #8A7A6B; font-family: 'Inter'; font-size: 13px; padding: 24px;"
        "    background: transparent; }";
}

WineSelectMenu::WineSelectMenu(QWidget* parent) : ModalOverlay(parent)
{
    build_ui();
    detector = new QFutureWatcher<QVector<cw::WineInstall>>(this);
    connect(detector, &QFutureWatcher<QVector<cw::WineInstall>>::finished,
            this, &WineSelectMenu::finish_scan);
    start_scan();
    relayout();
    connect(&util::i18n::LanguageManager::instance(),
            &util::i18n::LanguageManager::language_changed, this,
            [this]() { retranslate_dynamic_text(); });
}

void WineSelectMenu::build_ui()
{
    close_button = util::simple_utils::make_flat_button(this);
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    connect(close_button, &QPushButton::clicked, this, [this]() { hide(); emit closed(); });

    runtime_status = new QLabel(this);
    runtime_status->setStyleSheet(k_status);
    runtime_status->setWordWrap(true);

    list = new QScrollArea(this);
    list->setWidgetResizable(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setFrameShape(QFrame::NoFrame);
    list->setStyleSheet(k_scroll);

    rescan_button = new QPushButton(QStringLiteral("Rescan"), this);
    browse_button = new QPushButton(QStringLiteral("Add Runtime…"), this);
    continue_button = new QPushButton(QStringLiteral("Continue"), this);
    for (QPushButton* button : {rescan_button, browse_button, continue_button})
    {
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(util::styles::k_link_blue_lg);
    }
    connect(rescan_button, &QPushButton::clicked, this, &WineSelectMenu::rescan);
    connect(browse_button, &QPushButton::clicked, this, &WineSelectMenu::browse_runtime);
    continue_button->setEnabled(false);
    connect(continue_button, &QPushButton::clicked, this, &WineSelectMenu::confirm);

#if defined(Q_OS_MACOS)
    rosetta_button = new QPushButton(QStringLiteral("Request Rosetta…"), this);
    rosetta_button->setFlat(true);
    rosetta_button->setCursor(Qt::PointingHandCursor);
    rosetta_button->setStyleSheet(util::styles::k_link_blue_lg);
    connect(rosetta_button, &QPushButton::clicked, this, &WineSelectMenu::request_rosetta);
#endif

    close_button->raise();
    rescan_button->raise();
    browse_button->raise();
    if (rosetta_button) rosetta_button->raise();
    continue_button->raise();
}

void WineSelectMenu::populate()
{
    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    rows.clear();
    selected = -1;
    continue_button->setEnabled(false);

    for (int i = 0; i < runtimes.size(); ++i)
    {
        const cw::WineInstall& wi = runtimes[i];
#if defined(Q_OS_MACOS)
        const QString type = util::i18n::translate("Runtime");
#else
        const QString type = wi.type == cw::RuntimeType::Proton
            ? util::i18n::translate("Proton") : util::i18n::translate("Wine");
#endif
        QString details = wi.version.isEmpty() ? wi.issue : wi.version;
        if (details.isEmpty()) details = util::i18n::translate("Capability probe failed");
        if (wi.requires_rosetta)
        {
            details += wi.rosetta_available
                ? util::i18n::translate(" · Rosetta ready")
                : util::i18n::translate(" · Rosetta required");
        }
        const QString architecture = wi.architectures.isEmpty()
            ? util::i18n::translate("unknown architecture") : wi.architectures;
        auto* row = new QPushButton(
            QStringLiteral("%1   ·   %2   ·   %3\n%4\n%5")
                .arg(wi.name, type, architecture, wi.path, details), content);
        row->setCheckable(true);
        row->setEnabled(wi.usable);
        row->setCursor(Qt::PointingHandCursor);
        row->setStyleSheet(k_row);
        connect(row, &QPushButton::clicked, this, [this, i]() { select_row(i); });
        lay->addWidget(row);
        rows.push_back(row);
    }

    if (runtimes.isEmpty())
    {
#if defined(Q_OS_MACOS)
        const QString missingText = QStringLiteral(
            "No usable macOS runtime was found. Restore the bundled runtime or add a compatible app, executable, or runtime folder.");
#else
        const QString missingText = QStringLiteral(
            "No usable Wine or Proton runtimes were found on this system.");
#endif
#if defined(Q_OS_MACOS)
        const QString emptyText =
            scanning ? util::i18n::translate("Scanning macOS runtimes…")
                     : util::i18n::translate(missingText);
#else
        const QString emptyText =
            scanning ? util::i18n::translate("Scanning Wine runtimes…")
                     : util::i18n::translate(missingText);
#endif
        auto* empty = new QLabel(emptyText, content);
        empty->setStyleSheet(k_empty);
        empty->setWordWrap(true);
        lay->addWidget(empty);
    }

    lay->addStretch(1);
    list->setWidget(content);

#if defined(Q_OS_MACOS)
    const bool rosetta = cw::macos::rosetta_is_available();
    runtime_status->setText(
        util::i18n::translate("Graphics: built-in · Prefix: 64-bit · Rosetta: %1")
            .arg(rosetta ? util::i18n::translate("ready")
                         : util::i18n::translate("not detected")));
    if (rosetta_button) rosetta_button->setVisible(!rosetta);
#else
    const bool tricks = cw::winetricks_available();
    runtime_status->setText(tricks
        ? util::i18n::translate("winetricks: ready")
        : util::i18n::translate(
              "winetricks not found - required components will be installed manually"));
#endif
    relayout();
}

void WineSelectMenu::start_scan()
{
    if (detector->isRunning()) return;
    scanning = true;
    runtimes.clear();
    rescan_button->setEnabled(false);
    browse_button->setEnabled(false);
    continue_button->setEnabled(false);
    populate();
    detector->setFuture(QtConcurrent::run([]() { return cw::WineRegistry::scan(); }));
}

void WineSelectMenu::finish_scan()
{
    runtimes = detector->result();
    scanning = false;
    rescan_button->setEnabled(true);
    browse_button->setEnabled(true);
    populate();
}

void WineSelectMenu::rescan()
{
    start_scan();
}

void WineSelectMenu::browse_runtime()
{
#if defined(Q_OS_MACOS)
    QMessageBox choice(QMessageBox::Question, QStringLiteral("Add macOS Runtime"),
        QStringLiteral("Select a compatible runtime application, executable, or folder."),
        QMessageBox::NoButton, this);
    QPushButton* folderButton = choice.addButton(QStringLiteral("Select Runtime Folder"), QMessageBox::AcceptRole);
    QPushButton* fileButton = choice.addButton(QStringLiteral("Select Runtime App or Executable"), QMessageBox::ActionRole);
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
    else
        return;
#else
    const QString path = QFileDialog::getOpenFileName(
        this, util::i18n::translate("Select Wine Binary or Proton Script"));
#endif
    if (path.isEmpty()) return;

    cw::WineInstall install;
    QString error;
    if (!cw::WineRegistry::inspect_path(path, install, &error))
    {
        QMessageBox::warning(this, QStringLiteral("Runtime Not Usable"),
                             error.isEmpty() ? QStringLiteral("The selected runtime could not be used.") : error);
        return;
    }
    runtimes.append(install);
    populate();
    select_row(runtimes.size() - 1);
}

void WineSelectMenu::request_rosetta()
{
#if defined(Q_OS_MACOS)
    const auto answer = QMessageBox::question(this, QStringLiteral("Install Rosetta"),
        QStringLiteral("Some macOS runtimes are Intel applications. macOS may now show its system Rosetta installation prompt. Continue?"));
    if (answer != QMessageBox::Yes) return;
    if (!cw::macos::request_rosetta_install_prompt())
    {
        QMessageBox::warning(this, QStringLiteral("Rosetta Request Failed"),
                             QStringLiteral("macOS could not start the Rosetta installation request."));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Rosetta"),
        QStringLiteral("Complete the macOS prompt, then press Rescan."));
#endif
}

void WineSelectMenu::select_row(const int index)
{
    selected = index;
    for (int i = 0; i < rows.size(); ++i) rows[i]->setChecked(i == index);
    continue_button->setEnabled(index >= 0 && index < runtimes.size() && runtimes[index].usable);
}

void WineSelectMenu::retranslate_dynamic_text()
{
    const int previous_selection = selected;
    populate();
    if (previous_selection >= 0 && previous_selection < runtimes.size())
        select_row(previous_selection);
    update();
}

void WineSelectMenu::confirm()
{
    if (selected < 0 || selected >= runtimes.size() || !runtimes[selected].usable) return;
    const cw::WineInstall& wi = runtimes[selected];
    Config::instance().set_wine_binary(wi.path);
    Config::instance().set_runtime_selected(true);
    Config::instance().set_setup_runtime_preference(QStringLiteral("wine"));
#if defined(Q_OS_MACOS)
    Config::instance().set_use_dxvk(false);
    Config::instance().set_wine_arch(QStringLiteral("win64"));
#endif
    SPDLOG_INFO("runtime selected: {} ({})", wi.name.toStdString(), wi.path.toStdString());
    emit runtime_chosen();
}

void WineSelectMenu::paint_content(QPainter& painter)
{
    const QSize w = window()->size();
    const QRect box = im::box_rect(w);
    painter.drawPixmap(box, util::assets::images[util::assets::Image::BoxGameInstall]);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(util::colors::k_text_maroon);
    painter.drawText(im::title(w), Qt::AlignCenter, util::i18n::translate("SELECT RUNTIME"));

    QFont body_font = util::assets::fonts[util::assets::Font::Inter];
    body_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(util::colors::k_text_body);
#if defined(Q_OS_MACOS)
    const QString text = QStringLiteral("The bundled macOS runtime is used by default. Choose an override only for development or compatibility testing.");
#else
    const QString text = QStringLiteral("Choose the Wine or Proton version used to run the game.");
#endif
    painter.drawText(im::body(w), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     util::i18n::translate(text));
}

void WineSelectMenu::relayout()
{
    const QSize w = window()->size();
    const QRect box = im::box_rect(w);
    close_button->setIconSize(im::close_icon(w));
    close_button->setGeometry(im::close(w));

    const int inset = util::layout::scaled(34, w);
    const QRect body = im::body(w);
    const int top = body.bottom() + util::layout::scaled(16, w);
    const int btn_h = util::layout::scaled(30, w);
    const int btn_y = box.bottom() - util::layout::scaled(30, w) - btn_h;
    const int status_h = util::layout::scaled(34, w);
    const int status_y = btn_y - util::layout::scaled(8, w) - status_h;

    list->setGeometry(box.left() + inset, top, box.width() - 2 * inset,
                      qMax(0, status_y - top - util::layout::scaled(8, w)));
    runtime_status->setGeometry(box.left() + inset, status_y, box.width() - 2 * inset, status_h);

    const int btn_w = util::layout::scaled(132, w);
    continue_button->setGeometry(box.right() - inset - btn_w, btn_y, btn_w, btn_h);
    rescan_button->setGeometry(box.left() + inset, btn_y, btn_w, btn_h);
    browse_button->setGeometry(box.left() + inset + btn_w, btn_y, btn_w, btn_h);
    if (rosetta_button)
        rosetta_button->setGeometry(box.left() + inset + btn_w * 2, btn_y,
                                    util::layout::scaled(160, w), btn_h);
}
