#include "widgets/LauncherInfoDialog.hpp"

#include "util/Assets.hpp"
#include "util/LanguageManager.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

LauncherInfoDialog::LauncherInfoDialog(const Page page_, QWidget* parent)
    : QDialog(parent),
      page(page_)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(680, 580);
    setup_ui();
    retranslate();

    connect(&util::i18n::LanguageManager::instance(),
            &util::i18n::LanguageManager::language_changed,
            this,
            [this]()
    {
        retranslate();
    });
}

void LauncherInfoDialog::setup_ui()
{
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("launcherInfoPanel"));
    panel->setGeometry(18, 18, 644, 544);
    panel->setStyleSheet(QStringLiteral(
        "QFrame#launcherInfoPanel {"
        "background: rgba(249,244,240,252);"
        "border: 1px solid rgba(79,23,23,78);"
        "border-radius: 15px;"
        "}"
        "QFrame#launcherInfoCard {"
        "background: rgba(238,224,215,150);"
        "border: 1px solid rgba(79,23,23,38);"
        "border-radius: 10px;"
        "}"
        "QPushButton#launcherInfoLink {"
        "background: rgba(255,255,255,156);"
        "color: #4F1717;"
        "border: 1px solid rgba(79,23,23,52);"
        "border-radius: 8px;"
        "padding: 9px 12px;"
        "}"
        "QPushButton#launcherInfoLink:hover {"
        "background: rgba(235,220,211,235);"
        "border-color: rgba(79,23,23,105);"
        "}"
        "QPushButton#launcherInfoLink:pressed {"
        "background: rgba(219,198,186,242);"
        "}"
        "QPushButton#launcherInfoClose {"
        "background: rgba(255,255,255,112);"
        "border: 1px solid rgba(79,23,23,45);"
        "border-radius: 9px;"
        "padding: 0px;"
        "}"
        "QPushButton#launcherInfoClose:hover {"
        "background: rgba(235,220,211,225);"
        "border-color: rgba(79,23,23,95);"
        "}"
        "QLabel { background: transparent; }"));

    auto* shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(43, 28, 19, 94));
    panel->setGraphicsEffect(shadow);

    auto* root = new QVBoxLayout(panel);
    root->setContentsMargins(34, 22, 34, 28);
    root->setSpacing(8);

    auto* top_row = new QHBoxLayout;
    top_row->setContentsMargins(0, 0, 0, 0);

    section_label = new QLabel(panel);
    QFont section_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    section_font.setPixelSize(13);
    section_font.setWeight(QFont::Black);
    section_label->setFont(section_font);
    section_label->setStyleSheet(QStringLiteral("color: #9E8E7E; letter-spacing: 1px;"));
    top_row->addWidget(section_label);
    top_row->addStretch();

    close_button = new QPushButton(panel);
    close_button->setObjectName(QStringLiteral("launcherInfoClose"));
    close_button->setFixedSize(40, 40);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    close_button->setIconSize(QSize(16, 16));
    connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
    top_row->addWidget(close_button);
    root->addLayout(top_row);

    logo_label = new QLabel(panel);
    logo_label->setAlignment(Qt::AlignCenter);
    logo_label->setPixmap(util::assets::images[util::assets::Image::SoaLogo]
        .scaled(132, 91, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo_label->setFixedHeight(91);
    root->addWidget(logo_label);

    title_label = new QLabel(panel);
    title_label->setAlignment(Qt::AlignCenter);
    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(26);
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet(QStringLiteral("color: #4F1717;"));
    root->addWidget(title_label);

    subtitle_label = new QLabel(panel);
    subtitle_label->setAlignment(Qt::AlignCenter);
    subtitle_label->setWordWrap(true);
    QFont subtitle_font = util::assets::fonts[util::assets::Font::Inter];
    subtitle_font.setPixelSize(14);
    subtitle_font.setWeight(QFont::Medium);
    subtitle_label->setFont(subtitle_font);
    subtitle_label->setStyleSheet(QStringLiteral("color: #5A4636;"));
    root->addWidget(subtitle_label);

    badge_label = new QLabel(panel);
    badge_label->setAlignment(Qt::AlignCenter);
    badge_label->setFixedHeight(28);
    QFont badge_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    badge_font.setPixelSize(12);
    badge_font.setWeight(QFont::Black);
    badge_label->setFont(badge_font);
    badge_label->setStyleSheet(QStringLiteral(
        "color: #4F1717; background: rgba(216,205,192,128);"
        "border-radius: 8px; padding: 2px 12px;"));
    root->addWidget(badge_label, 0, Qt::AlignHCenter);

    auto* info_card = new QFrame(panel);
    info_card->setObjectName(QStringLiteral("launcherInfoCard"));
    auto* info_layout = new QVBoxLayout(info_card);
    info_layout->setContentsMargins(22, 16, 22, 16);
    info_label = new QLabel(info_card);
    info_label->setTextFormat(Qt::RichText);
    info_label->setWordWrap(true);
    info_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    info_label->setOpenExternalLinks(true);
    info_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont info_font = util::assets::fonts[util::assets::Font::Inter];
    info_font.setPixelSize(13);
    info_font.setWeight(QFont::Medium);
    info_label->setFont(info_font);
    info_label->setStyleSheet(QStringLiteral("color: #4F1717;"));
    info_label->setMinimumHeight(112);
    info_layout->addWidget(info_label);
    root->addWidget(info_card, 1);

    auto* link_row = new QHBoxLayout;
    link_row->setSpacing(10);
    const auto make_link_button = [panel]()
    {
        auto* button = new QPushButton(panel);
        button->setObjectName(QStringLiteral("launcherInfoLink"));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(40);
        QFont button_font = util::assets::fonts[util::assets::Font::EurostileBlack];
        button_font.setPixelSize(12);
        button_font.setWeight(QFont::Black);
        button->setFont(button_font);
        return button;
    };

    website_button = make_link_button();
    source_button = make_link_button();
    contact_button = make_link_button();
    link_row->addWidget(website_button);
    link_row->addWidget(source_button);
    link_row->addWidget(contact_button);
    root->addLayout(link_row);

    connect(website_button, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://storyofalicia.com")));
    });
    connect(source_button, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Story-Of-Alicia")));
    });
    connect(contact_button, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(QStringLiteral("mailto:dev@storyofalicia.com")));
    });

    footer_label = new QLabel(panel);
    footer_label->setAlignment(Qt::AlignCenter);
    footer_label->setWordWrap(true);
    QFont footer_font = util::assets::fonts[util::assets::Font::Inter];
    footer_font.setPixelSize(11);
    footer_font.setWeight(QFont::Medium);
    footer_label->setFont(footer_font);
    footer_label->setStyleSheet(QStringLiteral("color: #8D7768;"));
    root->addWidget(footer_label);
}

void LauncherInfoDialog::retranslate()
{
    using util::i18n::translate;

    close_button->setAccessibleName(translate("Close window"));
    website_button->setText(translate("Official Website"));
    source_button->setText(translate("Source Code"));
    contact_button->setText(translate("Contact"));
    website_button->setAccessibleName(website_button->text());
    source_button->setAccessibleName(source_button->text());
    contact_button->setAccessibleName(contact_button->text());

    if (page == Page::About)
    {
        setWindowTitle(translate("About Story of Alicia Launcher"));
        section_label->setText(translate("ABOUT"));
        title_label->setText(translate("Story of Alicia Launcher"));
        subtitle_label->setText(translate("The official launcher for Linux and macOS."));
        badge_label->setText(translate("VERSION %1 · QT %2")
            .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())));
        info_label->setText(QStringLiteral("<b>%1</b><br>%2<br><br><b>%3</b><br>%4")
            .arg(translate("Everything in one place"),
                 translate("Install, update, verify, repair, and launch both supported Story of Alicia game profiles."),
                 translate("Linux and macOS"),
                 translate("Linux uses Wine or Proton. macOS uses the bundled Story of Alicia runtime.")));
        footer_label->setText(translate(
            "Game and artwork remain owned by their respective copyright holders."));
    }
    else
    {
        setWindowTitle(translate("Credits"));
        section_label->setText(translate("CREDITS"));
        title_label->setText(translate("Credits"));
        subtitle_label->setText(translate("Built with help from the Story of Alicia community."));
        badge_label->setText(translate("THANK YOU"));
        info_label->setText(QStringLiteral(
            "<b>%1</b><br>%2<br><br>"
            "<b>%3</b><br>%4<br><br>"
            "<b>%5</b><br>%6<br><br>"
            "<b>%7</b><br>%8")
            .arg(translate("Launcher development"),
                 translate("Story of Alicia team and contributors"),
                 translate("Artwork and branding"),
                 translate("Respective Story of Alicia artists and copyright holders"),
                 translate("Translations and testing"),
                 translate("Community translators, testers, and players"),
                 translate("Open-source technology"),
                 translate("Qt, Wine, spdlog, fmt, and their contributors")));
        footer_label->setText(translate(
            "Thank you to everyone who helps players enjoy Story of Alicia."));
    }
}

void LauncherInfoDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && event->position().y() <= 74)
    {
        dragging = true;
        drag_offset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void LauncherInfoDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging && event->buttons().testFlag(Qt::LeftButton))
    {
        move(event->globalPosition().toPoint() - drag_offset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void LauncherInfoDialog::mouseReleaseEvent(QMouseEvent* event)
{
    dragging = false;
    QDialog::mouseReleaseEvent(event);
}
