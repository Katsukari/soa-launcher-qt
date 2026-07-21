#include "widgets/AliciaChooser.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Config.hpp"
#include "core/auth/AuthHandler.hpp"
#include "core/wine/Shell.hpp"
#include "core/state/InstallState.hpp"

#include <QCheckBox>
#include <QGraphicsDropShadowEffect>
#include <QSignalBlocker>

using util::config::Config;
using core::state::Stage;

namespace
{
    const char* k_note_box =
        "QLabel"
        "{"
        "    background: rgba(246, 231, 223, 0.92);"
        "    border: 1px solid rgba(160, 119, 98, 0.42);"
        "    border-radius: 5px;"
        "    color: #5A4636;"
        "    font-family: 'Inter';"
        "    font-size: 13px;"
        "    padding: 12px 16px;"
        "}";

    const char* k_banner_box =
        "QLabel"
        "{"
        "    background: rgba(196, 150, 128, 0.12);"
        "    border-radius: 8px;"
        "    color: #4F1717;"
        "    font-family: 'Eurostile';"
        "    font-weight: 800;"
        "    font-size: 14px;"
        "    padding: 0px 16px;"
        "}";

    const char* k_reset_link =
        "QPushButton"
        "{"
        "    background: transparent;"
        "    border: none;"
        "    color: #9E8E7E;"
        "    font-family: 'Inter';"
        "    font-weight: 700;"
        "    font-size: 15px;"
        "}"
        "QPushButton:hover { color: #7E6E5E; }"
        "QPushButton:focus { outline: none; }";

    void add_soft_shadow(QWidget* widget, const qreal blur = 22.0, const qreal y = 7.0,
                         const QColor& color = QColor(65, 39, 25, 72))
    {
        auto* effect = new QGraphicsDropShadowEffect(widget);
        effect->setBlurRadius(blur);
        effect->setOffset(0.0, y);
        effect->setColor(color);
        widget->setGraphicsEffect(effect);
    }

    QString welcome_message(const core::game::GameVersion version, const QString& required_action)
    {
        const QString game_name = version == core::game::GameVersion::Alicia2
            ? QStringLiteral("Story of Alicia 2.0")
            : QStringLiteral("Story of Alicia");
        return QStringLiteral("Welcome to %1. To participate in the playtest, you have to first %2.")
            .arg(game_name, required_action);
    }


}

AliciaChooser::AliciaChooser(AuthHandler* auth_, core::wine::Shell* shell_,
                   core::state::InstallState* install_state_, QWidget* parent)
    : QWidget(parent),
      auth(auth_),
      shell(shell_),
      install_state(install_state_),
      game_version(Config::instance().game_version())
{
    const QSize w = window()->size();
    setFixedSize(util::layout::alicia_chooser::box(w));
    add_soft_shadow(this, 30.0, 9.0, QColor(43, 28, 19, 88));

    setup_title();
    setup_settings_button();
    setup_download_state();
    setup_login_state();
    setup_waiting_state();
    setup_signedin_state();
    refresh_game_text();
    refresh_keep_signed_in();
    connect(&Config::instance(), &Config::changed,
            this, &AliciaChooser::refresh_keep_signed_in);

    reset_path_button = new QPushButton("RESET LAUNCHER SETTINGS", this);
    reset_path_button->setCursor(Qt::PointingHandCursor);
    reset_path_button->setAccessibleName(QStringLiteral("Reset launcher settings"));
    reset_path_button->setStyleSheet(k_reset_link);
    reset_path_button->setGeometry(util::layout::alicia_chooser::reset(w));
    reset_path_button->setToolTip("Reset launcher settings and sign-in without deleting the shared prefix or either game");
    connect(reset_path_button, &QPushButton::clicked, this, [this]()
    {
        emit reset_config_requested();
    });

    on_stage_changed(install_state->stage());
    connect(install_state, &core::state::InstallState::stage_changed,
            this, &AliciaChooser::on_stage_changed);
}

void AliciaChooser::set_game_version(const core::game::GameVersion version)
{
    if (game_version == version) return;

    game_version = version;
    refresh_game_text();
    on_stage_changed(current_stage);
}

AliciaChooser::State AliciaChooser::state_for(const Stage stage)
{
    switch (stage)
    {
        case Stage::NeedsAuth:      return State::Login;
        case Stage::Authenticating: return State::Waiting;
        case Stage::Launching:
        case Stage::Running:
        case Stage::Ready:          return State::SignedIn;
        default:                    return State::Download;
    }
}

void AliciaChooser::on_stage_changed(const Stage stage)
{
    current_stage = stage;
    const State next = state_for(stage);

    QString message;
    switch (stage)
    {
        case Stage::Probing:         message = QStringLiteral("Checking launcher state..."); break;
        case Stage::NeedsPrerequisites:
            message = welcome_message(game_version, QStringLiteral("complete the easy setup assistant"));
            break;
        case Stage::NeedsRuntime:
            message = welcome_message(game_version, QStringLiteral("choose Wine or Proton"));
            break;
        case Stage::NeedsPrefix:     message = QStringLiteral("The shared Wine prefix needs to be created before installing the game."); break;
        case Stage::PrefixBroken:    message = QStringLiteral("The shared Wine prefix is incomplete and needs repair."); break;
        case Stage::SettingUpPrefix: message = QStringLiteral("Preparing the shared Wine prefix..."); break;
        case Stage::NeedsDownload:
            message = welcome_message(game_version, QStringLiteral("download the game"));
            break;
        case Stage::CheckingUpdate:  message = QStringLiteral("Checking this game for updates..."); break;
        case Stage::Downloading:     message = QStringLiteral("Downloading and verifying game files..."); break;
        case Stage::NeedsUpdate:     message = QStringLiteral("A game update is available and must be installed before launch."); break;
        case Stage::Updating:        message = QStringLiteral("Updating and verifying game files..."); break;
        case Stage::NeedsRules:      message = QStringLiteral("The game is installed. Review and accept the playtest rules before signing in."); break;
        case Stage::Failed:          message = install_state->error_message(); break;
        case Stage::Broken:          message = QStringLiteral("The launcher detected an invalid installation state."); break;
        default: break;
    }
    if (!message.isEmpty())
        message_label->setText(message);

    const bool actionable = stage == Stage::NeedsRuntime || stage == Stage::NeedsPrefix
        || stage == Stage::PrefixBroken || stage == Stage::NeedsDownload
        || stage == Stage::NeedsUpdate;
    download_button->setEnabled(actionable);
    const auto action = stage == Stage::NeedsUpdate
        ? util::assets::Button::UpdateAvailable
        : util::assets::Button::DownloadGame;
    download_button->setIcon(QIcon(util::assets::buttons[action].normal));

    if (next == State::SignedIn)
    {
        QString name = Config::instance().display_name().trimmed();
        if (name.isEmpty()) name = QStringLiteral("PLAYER");
        name = name.left(64).toUpper();
        signed_in_label->setText(QStringLiteral("  SIGNED IN AS ") + name);
    }

    set_state(next);
    refresh_enter_enabled();
}

void AliciaChooser::setup_title()
{
    const QSize w = window()->size();

    title_label = new QLabel(this);
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setTextFormat(Qt::PlainText);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet("color: #4F1717; background: transparent;");
    title_label->setGeometry(util::layout::alicia_chooser::title(w));
}

void AliciaChooser::setup_settings_button()
{
    const QSize w = window()->size();
    const QRect button = util::layout::alicia_chooser::settings_button(w);

    settings_button = util::simple_utils::make_flat_button(this);

    const QPixmap scaled = util::assets::images[util::assets::Image::SettingsButton]
        .scaled(button.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    settings_button->setIcon(QIcon(scaled));
    settings_button->setIconSize(button.size());
    settings_button->setGeometry(button);
    settings_button->setAccessibleName(QStringLiteral("Open launcher settings"));

    connect(settings_button, &QPushButton::clicked, this, [this] { emit settings_requested(); });
}

void AliciaChooser::setup_download_state()
{
    const QSize w = window()->size();

    message_label = new QLabel(this);
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);
    message_label->setTextFormat(Qt::PlainText);

    QFont msg_font = util::assets::fonts[util::assets::Font::Inter];
    msg_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    msg_font.setWeight(QFont::Medium);
    message_label->setFont(msg_font);
    message_label->setStyleSheet("color: #4F1717; background: transparent; padding: 0px 12px;");
    message_label->setGeometry(util::layout::alicia_chooser::message(w));

    download_button = new QPushButton(this);
    download_button->setFlat(true);
    download_button->setCursor(Qt::PointingHandCursor);
    download_button->setText("");
    download_button->setStyleSheet("border: none; outline: none; background: transparent;");

    const QPixmap& normal = util::assets::buttons[util::assets::Button::DownloadGame].normal;

    const int bw = util::layout::alicia_chooser::dl_button_w(w);
    const int bh = qRound(bw * static_cast<double>(normal.height()) / normal.width());
    const int bx = util::layout::alicia_chooser::dl_button_x(w);
    const int by = util::layout::alicia_chooser::dl_button_y(w);

    download_button->setIcon(QIcon(normal));
    download_button->setIconSize(QSize(bw, bh));
    download_button->setGeometry(bx, by, bw, bh);
    download_button->setAccessibleName(QStringLiteral("Open the required game setup action"));
    download_button->installEventFilter(this);
    connect(download_button, &QPushButton::clicked, this, [this]()
    {
        if (download_button->isEnabled()) emit download_triggered();
    });
}

void AliciaChooser::setup_login_state()
{
    const QSize w = window()->size();

    const QRect dr = util::layout::alicia_chooser::discord_button(w);
    const QPixmap& discord_normal = util::assets::buttons[util::assets::Button::Discord].normal;
    const int dw = dr.width();
    const int dh = qRound(dw * static_cast<double>(discord_normal.height()) / discord_normal.width());

    discord_button = new QPushButton(this);
    discord_button->setFlat(true);
    discord_button->setCursor(Qt::PointingHandCursor);
    discord_button->setText("");
    discord_button->setStyleSheet("border: none; outline: none; background: transparent;");
    discord_button->setIcon(QIcon(discord_normal));
    discord_button->setIconSize(QSize(dw, dh));
    discord_button->setGeometry(dr.x(), dr.y(), dw, dh);
    discord_button->setAccessibleName(QStringLiteral("Proceed with Discord"));
    discord_button->installEventFilter(this);
    connect(discord_button, &QPushButton::clicked, auth, &AuthHandler::open_login);

    keep_signed_button = new QCheckBox(QStringLiteral("Keep me signed in"), this);
    keep_signed_button->setCursor(Qt::PointingHandCursor);
    keep_signed_button->setAccessibleName(QStringLiteral("Keep me signed in after the launcher closes"));
    keep_signed_button->setGeometry(util::layout::alicia_chooser::keep_signed_in(w));
    QFont keep_font = util::assets::fonts[util::assets::Font::Inter];
    keep_font.setPixelSize(util::layout::scaled(13, w));
    keep_font.setWeight(QFont::Medium);
    keep_signed_button->setFont(keep_font);
    keep_signed_button->setStyleSheet(
        "QCheckBox { background: transparent; color: #4F1717; spacing: 5px; outline: none; }"
        "QCheckBox:hover { color: #321010; }"
        "QCheckBox:focus { outline: none; }"
        "QCheckBox::indicator { width: 19px; height: 18px; }"
        "QCheckBox::indicator:unchecked { image: url(:/assets/checkbox.png); }"
        "QCheckBox::indicator:checked { image: url(:/assets/checkbox-ticked.png); }");
    connect(keep_signed_button, &QCheckBox::toggled, this, [](const bool checked)
    {
        Config::instance().set_keep_signed_in(checked);
    });

    disclaimer_label = new QLabel(this);
    disclaimer_label->setWordWrap(true);
    disclaimer_label->setTextFormat(Qt::RichText);
    disclaimer_label->setOpenExternalLinks(true);
    disclaimer_label->setText(
        "By clicking the \u201CProceed with Discord\u201D button, you acknowledge that your "
        "Discord ID will be stored on our database servers indefinitely for identification "
        "and service purposes. This data can be removed upon request by emailing "
        "<a href=\"mailto:dev@storyofalicia.com\" style=\"color:#2FB4E0;\">dev@storyofalicia.com</a>.");
    disclaimer_label->setStyleSheet(k_note_box);
    disclaimer_label->setGeometry(util::layout::alicia_chooser::disclaimer(w));
    add_soft_shadow(disclaimer_label, 18.0, 6.0, QColor(64, 40, 27, 62));
}

void AliciaChooser::setup_waiting_state()
{
    const QSize w = window()->size();

    waiting_title = new QLabel("WAITING FOR BROWSER AUTHENTICATION", this);
    waiting_title->setAlignment(Qt::AlignCenter);
    waiting_title->setTextFormat(Qt::PlainText);
    QFont wf = util::assets::fonts[util::assets::Font::EurostileBlack];
    wf.setPixelSize(util::layout::scaled(16, w));
    wf.setWeight(QFont::Black);
    waiting_title->setFont(wf);
    waiting_title->setStyleSheet("color: #4F1717; background: transparent;");
    waiting_title->setGeometry(util::layout::alicia_chooser::waiting_title(w));

    steps_label = new QLabel(this);
    steps_label->setWordWrap(true);
    steps_label->setTextFormat(Qt::RichText);
    steps_label->setText(
        "<b>1.</b>&nbsp; Sign in with Discord via the app or browser.<br>"
        "<b>2.</b>&nbsp; On your <b>browser</b>, click <b>“Open Story of Alicia Launcher”</b>.<br>"
        "<b>3.</b>&nbsp; Allow your browser to always open the Launcher when prompted.<br>"
        "<b>4.</b>&nbsp; If login did not work, cancel and try again.");
    steps_label->setStyleSheet(k_note_box);
    steps_label->setGeometry(util::layout::alicia_chooser::steps(w));
    add_soft_shadow(steps_label, 18.0, 6.0, QColor(64, 40, 27, 62));

    try_again_button = new QPushButton(QStringLiteral("Cancel / Try again"), this);
    try_again_button->setCursor(Qt::PointingHandCursor);
    try_again_button->setAccessibleName(QStringLiteral("Cancel Discord login and try again"));
    try_again_button->setGeometry(util::layout::alicia_chooser::try_again(w));
    QFont retry_font = util::assets::fonts[util::assets::Font::Inter];
    retry_font.setPixelSize(util::layout::scaled(11, w));
    retry_font.setWeight(QFont::Medium);
    retry_font.setUnderline(true);
    try_again_button->setFont(retry_font);
    try_again_button->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: #988776; }"
        "QPushButton:hover { color: #6F5F50; }"
        "QPushButton:focus { outline: none; }");
    connect(try_again_button, &QPushButton::clicked, auth, &AuthHandler::cancel_login);
}

void AliciaChooser::setup_signedin_state()
{
    const QSize w = window()->size();

    signed_in_label = new QLabel("  SIGNED IN", this);
    signed_in_label->setTextFormat(Qt::PlainText);
    signed_in_label->setStyleSheet(k_banner_box);
    signed_in_label->setGeometry(util::layout::alicia_chooser::signed_in_banner(w));

    const QRect er = util::layout::alicia_chooser::enter_button(w);
    const QPixmap& enter_normal = util::assets::buttons[util::assets::Button::Enter].normal;
    const int ew = er.width();
    const int eh = qRound(ew * static_cast<double>(enter_normal.height()) / enter_normal.width());

    enter_button = new QPushButton(this);
    enter_button->setFlat(true);
    enter_button->setCursor(Qt::PointingHandCursor);
    enter_button->setText("");
    enter_button->setStyleSheet("border: none; outline: none; background: transparent;");
    enter_button->setIcon(QIcon(enter_normal));
    enter_button->setIconSize(QSize(ew, eh));
    enter_button->setGeometry(er.x(), er.y(), ew, eh);
    enter_button->setEnabled(false);
    enter_button->setAccessibleName(QStringLiteral("Enter the playtest"));
    enter_button->installEventFilter(this);
    connect(enter_button, &QPushButton::clicked, this, [this]()
    {
        if (enter_button->isEnabled())
            shell->run_game(Config::instance().username(), Config::instance().token());
    });
}

void AliciaChooser::set_state(const State next_state)
{
    state = next_state;
    apply_state_visibility();
    if (next_state == State::SignedIn) refresh_enter_enabled();
}

void AliciaChooser::apply_state_visibility()
{
    const bool download = state == State::Download;
    const bool login    = state == State::Login;
    const bool waiting  = state == State::Waiting;
    const bool signedin = state == State::SignedIn;

    download_button->setVisible(download);
    message_label->setVisible(download);

    discord_button->setVisible(login);
    keep_signed_button->setVisible(login);
    disclaimer_label->setVisible(login);

    waiting_title->setVisible(waiting);
    steps_label->setVisible(waiting);
    try_again_button->setVisible(waiting);

    signed_in_label->setVisible(signedin);
    enter_button->setVisible(signedin);

    reset_path_button->setVisible(!download);
}

void AliciaChooser::refresh_enter_enabled()
{
    const bool ready = current_stage == Stage::Ready;
    enter_button->setEnabled(ready);
    enter_button->setToolTip(current_stage == Stage::Running
        ? QStringLiteral("Alicia is already running")
        : current_stage == Stage::Launching
            ? QStringLiteral("Alicia is starting")
            : QString());
}

void AliciaChooser::refresh_keep_signed_in()
{
    if (!keep_signed_button)
        return;

    const bool keep = Config::instance().keep_signed_in();
    const QSignalBlocker blocker(keep_signed_button);
    keep_signed_button->setChecked(keep);
    keep_signed_button->setToolTip(keep
        ? QStringLiteral("Your Discord session will be restored next time the launcher starts")
        : QStringLiteral("Your Discord session will only last until this launcher closes"));
}

void AliciaChooser::refresh_game_text()
{
    const bool alicia_2 = game_version == core::game::GameVersion::Alicia2;

    title_label->setText(alicia_2 ? "STORY OF ALICIA 2.0 PLAYTEST" : "PLAYTEST");
}

void AliciaChooser::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const QSize w = window()->size();

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const auto card = state == State::Waiting
        ? util::assets::Image::BoxWaitingForAuth
        : util::assets::Image::BoxCard;
    painter.drawPixmap(rect(), util::assets::images[card]);

    if (state == State::Download)
        painter.drawPixmap(util::layout::alicia_chooser::message(w),
                           util::assets::images[util::assets::Image::BoxNote2]);
}

bool AliciaChooser::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == download_button)
    {
        const auto action = current_stage == Stage::NeedsUpdate
            ? util::assets::Button::UpdateAvailable
            : util::assets::Button::DownloadGame;
        const auto& button = util::assets::buttons[action];
        util::simple_utils::apply_button_state(event, download_button,
                                               button.normal, button.hover, button.clicked);
    }
    else if (obj == discord_button)
    {
        const auto& button = util::assets::buttons[util::assets::Button::Discord];
        util::simple_utils::apply_button_state(event, discord_button,
                                               button.normal, button.hover, button.clicked);
    }
    else if (obj == enter_button)
    {
        const auto& button = util::assets::buttons[util::assets::Button::Enter];
        util::simple_utils::apply_button_state(event, enter_button,
                                               button.normal, button.hover, button.clicked);
    }
    return QWidget::eventFilter(obj, event);
}
