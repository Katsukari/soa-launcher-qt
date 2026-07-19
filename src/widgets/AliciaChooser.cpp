#include "widgets/AliciaChooser.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"
#include "util/Config.hpp"
#include "core/auth/AuthHandler.hpp"
#include "core/wine/Shell.hpp"
#include "core/state/InstallState.hpp"

using util::config::Config;
using core::state::Stage;

namespace
{
    const char* k_note_box =
        "QLabel"
        "{"
        "    background: rgba(196, 150, 128, 0.12);"
        "    border-radius: 8px;"
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
        "QPushButton:hover { color: #7E6E5E; }";

    const char* k_checkbox =
        "QCheckBox { color: #392518; font-family: 'Inter'; font-size: 14px; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { image: url(:/assets/checkbox.png); }"
        "QCheckBox::indicator:checked { image: url(:/assets/checkbox-ticked.png); }";
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

    setup_title();
    setup_settings_button();
    setup_download_state();
    setup_login_state();
    setup_waiting_state();
    setup_signedin_state();
    refresh_game_text();

    reset_path_button = new QPushButton("RESET INSTALLATION PATH", this);
    reset_path_button->setCursor(Qt::PointingHandCursor);
    reset_path_button->setFocusPolicy(Qt::NoFocus);
    reset_path_button->setStyleSheet(k_reset_link);
    reset_path_button->setGeometry(util::layout::alicia_chooser::reset(w));
    reset_path_button->setToolTip("Resets launcher settings and sign-in without deleting the prefix or games");
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
}

AliciaChooser::State AliciaChooser::state_for(const Stage stage)
{
    switch (stage)
    {
        case Stage::NeedsAuth:      return State::Login;
        case Stage::Authenticating: return State::Waiting;
        case Stage::Ready:          return State::SignedIn;
        default:                    return State::Download;
    }
}

void AliciaChooser::on_stage_changed(const Stage stage)
{
    const State next = state_for(stage);

    if (next == State::SignedIn)
    {
        const QString name = Config::instance().display_name();
        signed_in_label->setText("  SIGNED IN AS " + (name.isEmpty() ? "PLAYER" : name.toUpper()));
    }

    set_state(next);
}

void AliciaChooser::setup_title()
{
    const QSize w = window()->size();

    title_label = new QLabel(this);
    title_label->setAlignment(Qt::AlignCenter);

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

    connect(settings_button, &QPushButton::clicked, this, [this] { emit settings_requested(); });
}

void AliciaChooser::setup_download_state()
{
    const QSize w = window()->size();

    message_label = new QLabel(this);
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);

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
    download_button->setStyleSheet("border: none; background: transparent;");

    const QPixmap& normal = util::assets::buttons[util::assets::Button::DownloadGame].normal;

    const int bw = util::layout::alicia_chooser::dl_button_w(w);
    const int bh = qRound(bw * static_cast<double>(normal.height()) / normal.width());
    const int bx = util::layout::alicia_chooser::dl_button_x(w);
    const int by = util::layout::alicia_chooser::dl_button_y(w);

    download_button->setIcon(QIcon(normal));
    download_button->setIconSize(QSize(bw, bh));
    download_button->setGeometry(bx, by, bw, bh);
    download_button->installEventFilter(this);
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
    discord_button->setStyleSheet("border: none; background: transparent;");
    discord_button->setIcon(QIcon(discord_normal));
    discord_button->setIconSize(QSize(dw, dh));
    discord_button->setGeometry(dr.x(), dr.y(), dw, dh);
    discord_button->installEventFilter(this);

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
}

void AliciaChooser::setup_waiting_state()
{
    const QSize w = window()->size();

    waiting_title = new QLabel("WAITING FOR BROWSER AUTHENTICATION", this);
    waiting_title->setAlignment(Qt::AlignCenter);
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
        "<b>1.</b>&nbsp; Sign in with Discord in the browser that just opened.<br>"
        "<b>2.</b>&nbsp; On the website, tick the checkboxes and click <b>Enter the Game</b>.<br>"
        "<b>3.</b>&nbsp; The launcher will update automatically once done.");
    steps_label->setStyleSheet(k_note_box);
    steps_label->setGeometry(util::layout::alicia_chooser::steps(w));
}

void AliciaChooser::setup_signedin_state()
{
    const QSize w = window()->size();

    check_bugs = new QCheckBox("I understand the game has bugs and is not the final version.", this);
    check_bugs->setCursor(Qt::PointingHandCursor);
    check_bugs->setStyleSheet(k_checkbox);
    check_bugs->setGeometry(util::layout::alicia_chooser::check_bugs(w));

    check_rules = new QCheckBox(this);
    check_rules->setCursor(Qt::PointingHandCursor);
    check_rules->setStyleSheet(k_checkbox);
    check_rules->setText("I have read and will obey the server rules.");
    check_rules->setGeometry(util::layout::alicia_chooser::check_rules(w));

    connect(check_bugs,  &QCheckBox::toggled, this, [this]() { refresh_enter_enabled(); });
    connect(check_rules, &QCheckBox::toggled, this, [this]() { refresh_enter_enabled(); });

    signed_in_label = new QLabel("  SIGNED IN", this);
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
    enter_button->setStyleSheet("border: none; background: transparent;");
    enter_button->setIcon(QIcon(enter_normal));
    enter_button->setIconSize(QSize(ew, eh));
    enter_button->setGeometry(er.x(), er.y(), ew, eh);
    enter_button->setEnabled(false);
    enter_button->installEventFilter(this);
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
    disclaimer_label->setVisible(login);

    waiting_title->setVisible(waiting);
    steps_label->setVisible(waiting);

    check_bugs->setVisible(signedin);
    check_rules->setVisible(signedin);
    signed_in_label->setVisible(signedin);
    enter_button->setVisible(signedin);

    reset_path_button->setVisible(!download);
}

void AliciaChooser::refresh_enter_enabled()
{
    enter_button->setEnabled(check_bugs->isChecked() && check_rules->isChecked());
}

void AliciaChooser::refresh_game_text()
{
    const bool alicia_2 = game_version == core::game::GameVersion::Alicia2;

    title_label->setText(alicia_2 ? "STORY OF ALICIA 2.0 PLAYTEST" : "PLAYTEST");
    message_label->setText(
        alicia_2
            ? "Welcome to Story of Alicia 2.0. To participate in the playtest, you\n"
              "have to first download the game."
            : "Welcome to Story of Alicia. To participate in the playtest, you\n"
              "have to first download the game.");
}

void AliciaChooser::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const QSize w = window()->size();

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.drawPixmap(rect(), util::assets::images[util::assets::Image::BoxCard]);

    if (state == State::Download)
        painter.drawPixmap(util::layout::alicia_chooser::message(w),
                           util::assets::images[util::assets::Image::BoxNote2]);
}

bool AliciaChooser::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == download_button)
    {
        const auto& button = util::assets::buttons[util::assets::Button::DownloadGame];
        util::simple_utils::apply_button_state(event, download_button, button.normal, button.hover, button.clicked);

        if (event->type() == QEvent::MouseButtonRelease && download_button->underMouse())
            emit download_triggered();
    }
    else if (obj == discord_button)
    {
        const auto& button = util::assets::buttons[util::assets::Button::Discord];
        util::simple_utils::apply_button_state(event, discord_button, button.normal, button.hover, button.clicked);

        if (event->type() == QEvent::MouseButtonRelease && discord_button->underMouse())
            auth->open_login();
    }
    else if (obj == enter_button)
    {
        const auto& button = util::assets::buttons[util::assets::Button::Enter];
        util::simple_utils::apply_button_state(event, enter_button, button.normal, button.hover, button.clicked);

        if (event->type() == QEvent::MouseButtonRelease && enter_button->underMouse() && enter_button->isEnabled())
            shell->run_game(Config::instance().username(), Config::instance().token());
    }
    return QWidget::eventFilter(obj, event);
}
