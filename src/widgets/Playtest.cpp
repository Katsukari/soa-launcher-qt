#include "widgets/Playtest.hpp"
#include "util/Layout.hpp"
#include "util/Config.hpp"
#include "core/auth/AuthHandler.hpp"
#include "core/wine/Shell.hpp"

using util::config::Config;

namespace
{
    const char* k_blue_button =
        "QPushButton"
        "{"
        "    border: none;"
        "    border-radius: 8px;"
        "    color: white;"
        "    font-family: 'Eurostile';"
        "    font-weight: 900;"
        "    font-size: 17px;"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "        stop:0 #56C8EE, stop:1 #1FA6DE);"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #6FD4F2, stop:1 #2FB4E8); }"
        "QPushButton:disabled { background: #BFD4DC; color: #EAF2F5; }";

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
        "    font-family: 'Eurostile';"
        "    font-weight: 800;"
        "    font-size: 13px;"
        "}"
        "QPushButton:hover { color: #7E6E5E; }";

    const char* k_checkbox =
        "QCheckBox { color: #392518; font-family: 'Inter'; font-size: 14px; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px;"
        "    border: 1px solid #C9BBAA; background: white; }"
        "QCheckBox::indicator:checked { background: #2FB4E0; border: 1px solid #2FB4E0;"
        "    image: url(:/assets/check.png); }";
}

Playtest::Playtest(AuthHandler* auth_, core::wine::Shell* shell_, QWidget* parent)
    : QWidget(parent), auth(auth_), shell(shell_)
{
    const QSize w = window()->size();
    setFixedSize(util::layout::playtest::box(w));

    setup_title();
    setup_settings_button();
    setup_download_state();
    setup_login_state();
    setup_waiting_state();
    setup_signedin_state();

    reset_path_button = new QPushButton("RESET INSTALLATION PATH", this);
    reset_path_button->setCursor(Qt::PointingHandCursor);
    reset_path_button->setFocusPolicy(Qt::NoFocus);
    reset_path_button->setStyleSheet(k_reset_link);
    reset_path_button->setGeometry(util::layout::playtest::reset(w));
    connect(reset_path_button, &QPushButton::clicked, this, []()
    {
        Config::instance().set_game_install_path("");
    });

    connect(auth, &AuthHandler::authenticated, this,
        [this](const QString&, const QString&, const QString& username)
        {
            signed_in_label->setText("  SIGNED IN AS " + username.toUpper());
            set_state(State::SignedIn);
        });

    if (Config::instance().has_auth() && Config::instance().game_installed())
    {
        const QString name = Config::instance().display_name();
        signed_in_label->setText("  SIGNED IN AS " + (name.isEmpty() ? "PLAYER" : name.toUpper()));
        set_state(State::SignedIn);
    }
    else
    {
        set_state(State::Download);
    }
}

void Playtest::setup_title()
{
    const QSize w = window()->size();

    title_label = new QLabel("PLAYTEST", this);
    title_label->setAlignment(Qt::AlignCenter);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(util::layout::scaled(util::layout::text::k_modal_header, w));
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet("color: #4F1717; background: transparent;");
    title_label->setGeometry(util::layout::playtest::title(w));
}

void Playtest::setup_settings_button()
{
    const QSize w = window()->size();
    const QRect button = util::layout::playtest::settings_button(w);

    settings_button = new QPushButton(this);
    settings_button->setFlat(true);
    settings_button->setCursor(Qt::PointingHandCursor);
    settings_button->setStyleSheet("outline:none; border: none; background: transparent;");

    const QPixmap scaled = util::assets::images[util::assets::Image::SettingsButton]
        .scaled(button.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    settings_button->setIcon(QIcon(scaled));
    settings_button->setIconSize(button.size());
    settings_button->setGeometry(button);

    connect(settings_button, &QPushButton::clicked, this, [this] { emit settings_requested(); });
}

void Playtest::setup_download_state()
{
    const QSize w = window()->size();

    message_label = new QLabel(this);
    message_label->setText(
        "Welcome to Story of Alicia. To participate in the playtest, you\n"
        "have to first create the wine prefix, and download the game.");
    message_label->setAlignment(Qt::AlignCenter);
    message_label->setWordWrap(true);

    QFont msg_font = util::assets::fonts[util::assets::Font::Inter];
    msg_font.setPixelSize(util::layout::scaled(util::layout::text::k_body, w));
    msg_font.setWeight(QFont::Medium);
    message_label->setFont(msg_font);
    message_label->setStyleSheet("color: #4F1717; background: transparent; padding: 0px 12px;");
    message_label->setGeometry(util::layout::playtest::message(w));

    download_button = new QPushButton(this);
    download_button->setFlat(true);
    download_button->setCursor(Qt::PointingHandCursor);
    download_button->setText("");
    download_button->setStyleSheet("border: none; background: transparent;");

    const QPixmap& normal = util::assets::buttons[util::assets::Button::DownloadGame].normal;

    const int bw = util::layout::playtest::dl_button_w(w);
    const int bh = qRound(bw * static_cast<double>(normal.height()) / normal.width());
    const int bx = util::layout::playtest::dl_button_x(w);
    const int by = util::layout::playtest::dl_button_y(w);

    download_button->setIcon(QIcon(normal));
    download_button->setIconSize(QSize(bw, bh));
    download_button->setGeometry(bx, by, bw, bh);
    download_button->installEventFilter(this);
}

void Playtest::setup_login_state()
{
    const QSize w = window()->size();

    discord_button = new QPushButton("  PROCEED WITH DISCORD", this);
    discord_button->setCursor(Qt::PointingHandCursor);
    discord_button->setFocusPolicy(Qt::NoFocus);
    discord_button->setStyleSheet(k_blue_button);
    discord_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseIcon]));
    discord_button->setIconSize(util::layout::playtest::discord_icon(w));
    discord_button->setGeometry(util::layout::playtest::discord_button(w));
    connect(discord_button, &QPushButton::clicked, this, [this]()
    {
        auth->open_login();
        set_state(State::Waiting);
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
    disclaimer_label->setGeometry(util::layout::playtest::disclaimer(w));
}

void Playtest::setup_waiting_state()
{
    const QSize w = window()->size();

    waiting_title = new QLabel("WAITING FOR BROWSER AUTHENTICATION", this);
    waiting_title->setAlignment(Qt::AlignCenter);
    QFont wf = util::assets::fonts[util::assets::Font::EurostileBlack];
    wf.setPixelSize(util::layout::scaled(16, w));
    wf.setWeight(QFont::Black);
    waiting_title->setFont(wf);
    waiting_title->setStyleSheet("color: #4F1717; background: transparent;");
    waiting_title->setGeometry(util::layout::playtest::waiting_title(w));

    steps_label = new QLabel(this);
    steps_label->setWordWrap(true);
    steps_label->setTextFormat(Qt::RichText);
    steps_label->setText(
        "<b>1.</b>&nbsp; Sign in with Discord in the browser that just opened.<br>"
        "<b>2.</b>&nbsp; On the website, tick the checkboxes and click <b>Enter the Game</b>.<br>"
        "<b>3.</b>&nbsp; The launcher will update automatically once done.");
    steps_label->setStyleSheet(k_note_box);
    steps_label->setGeometry(util::layout::playtest::steps(w));
}

void Playtest::setup_signedin_state()
{
    const QSize w = window()->size();

    check_bugs = new QCheckBox("I understand the game has bugs and is not the final version.", this);
    check_bugs->setCursor(Qt::PointingHandCursor);
    check_bugs->setStyleSheet(k_checkbox);
    check_bugs->setGeometry(util::layout::playtest::check_bugs(w));

    check_rules = new QCheckBox(this);
    check_rules->setCursor(Qt::PointingHandCursor);
    check_rules->setStyleSheet(k_checkbox);
    check_rules->setText("I have read and will obey the server rules.");
    check_rules->setGeometry(util::layout::playtest::check_rules(w));

    connect(check_bugs,  &QCheckBox::toggled, this, [this]() { refresh_enter_enabled(); });
    connect(check_rules, &QCheckBox::toggled, this, [this]() { refresh_enter_enabled(); });

    signed_in_label = new QLabel("  SIGNED IN", this);
    signed_in_label->setStyleSheet(k_banner_box);
    signed_in_label->setGeometry(util::layout::playtest::signed_in_banner(w));

    enter_button = new QPushButton("ENTER THE PLAYTEST", this);
    enter_button->setCursor(Qt::PointingHandCursor);
    enter_button->setFocusPolicy(Qt::NoFocus);
    enter_button->setStyleSheet(k_blue_button);
    enter_button->setGeometry(util::layout::playtest::enter_button(w));
    enter_button->setEnabled(false);
    connect(enter_button, &QPushButton::clicked, this, [this]()
    {
        shell->run_game(Config::instance().username(), Config::instance().token());
    });
}

void Playtest::on_download_complete()
{
    if (state == State::Download)
        set_state(State::Login);
}

void Playtest::set_state(State s)
{
    state = s;
    apply_state_visibility();
    if (s == State::SignedIn) refresh_enter_enabled();
}

void Playtest::apply_state_visibility()
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

void Playtest::refresh_enter_enabled()
{
    enter_button->setEnabled(check_bugs->isChecked() && check_rules->isChecked());
}

void Playtest::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const QSize w = window()->size();

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.drawPixmap(rect(), util::assets::images[util::assets::Image::BoxCard]);

    if (state == State::Download)
        painter.drawPixmap(util::layout::playtest::message(w),
                           util::assets::images[util::assets::Image::BoxNote2]);
}

bool Playtest::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == download_button)
    {
        const auto& button = util::assets::buttons[util::assets::Button::DownloadGame];
        switch (event->type())
        {
            case QEvent::Enter:              download_button->setIcon(QIcon(button.hover));   break;
            case QEvent::Leave:              download_button->setIcon(QIcon(button.normal));  break;
            case QEvent::MouseButtonPress:   download_button->setIcon(QIcon(button.clicked)); break;
            case QEvent::MouseButtonRelease:
                download_button->setIcon(QIcon(download_button->underMouse() ? button.hover : button.normal));
                if (download_button->underMouse()) emit download_triggered();
                break;
            default: break;
        }
    }
    return QWidget::eventFilter(obj, event);
}