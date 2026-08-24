#include "widgets/LauncherDialog.hpp"

#include "util/Assets.hpp"
#include "util/LanguageManager.hpp"
#include "util/Layout.hpp"

#include <QApplication>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <utility>

LauncherDialog::LauncherDialog(const Tone tone_, const QString& title,
                               const QString& message, const QString& details,
                               QWidget* parent)
    : QDialog(parent ? parent->window() : nullptr),
      tone(tone_),
      title_source(title),
      message_source(message),
      details_source(details)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setWindowModality(Qt::WindowModal);
    setSizeGripEnabled(false);
    setMinimumWidth(680);
    build_ui();
    connect(&util::i18n::LanguageManager::instance(),
            &util::i18n::LanguageManager::language_changed,
            this, [this]() { retranslate(); });
    retranslate();
}

void LauncherDialog::build_ui()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 22, 22, 22);

    panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("launcherDialogPanel"));
    panel->setMinimumWidth(636);
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setStyleSheet(QStringLiteral(
        "QFrame#launcherDialogPanel {"
        "background: rgba(249,244,240,252);"
        "border: 1px solid rgba(79,23,23,86);"
        "border-radius: 0px;"
        "}"
        "QFrame#launcherDialogDetails {"
        "background: rgba(238,224,215,178);"
        "border: 1px solid rgba(79,23,23,45);"
        "border-radius: 0px;"
        "}"
        "QLabel { background: transparent; }"
        "QPushButton#launcherDialogClose {"
        "background: transparent;"
        "border: none;"
        "padding: 0px;"
        "}"
        "QPushButton#launcherDialogClose:hover {"
        "background: transparent;"
        "border: none;"
        "}"
        "QPushButton#launcherDialogClose:pressed {"
        "background: transparent;"
        "border: none;"
        "}"));

    auto* shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(43, 28, 19, 100));
    panel->setGraphicsEffect(shadow);
    outer->addWidget(panel);

    auto* root = new QVBoxLayout(panel);
    root->setContentsMargins(28, 20, 28, 24);
    root->setSpacing(12);

    auto* top = new QHBoxLayout;
    // Clearance for the close button, which now sits further in from the
    // corner: its hit area starts 59px from the panel edge.
    // Clearance for the close button, whose 40x40 hit area now starts 67px
    // in from the panel edge (was 30px with the old hand-placed 24x24).
    top->setContentsMargins(0, 0, 52, 0);
    top->setSpacing(12);

    tone_badge = new QLabel(panel);
    tone_badge->setAlignment(Qt::AlignCenter);
    tone_badge->setFixedSize(42, 42);
    QFont badge_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    badge_font.setPixelSize(20);
    badge_font.setWeight(QFont::Black);
    tone_badge->setFont(badge_font);
    tone_badge->setStyleSheet(QStringLiteral(
        "color: #FFFFFF; background: #4F1717; border-radius: 21px;"));
    top->addWidget(tone_badge);

    auto* title_column = new QVBoxLayout;
    title_column->setSpacing(1);
    auto* category = new QLabel(panel);
    category->setObjectName(QStringLiteral("launcherDialogCategory"));
    QFont category_font = util::assets::fonts[util::assets::Font::EurostileBlack];
    category_font.setPixelSize(11);
    category_font.setWeight(QFont::Black);
    category->setFont(category_font);
    category->setStyleSheet(QStringLiteral("color: #9E8E7E; letter-spacing: 1px;"));
    title_column->addWidget(category);

    title_label = new QLabel(panel);
    title_label->setWordWrap(true);
    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(23);
    title_font.setWeight(QFont::Black);
    title_label->setFont(title_font);
    title_label->setStyleSheet(QStringLiteral("color: #4F1717;"));
    title_column->addWidget(title_label);
    top->addLayout(title_column, 1);

    close_button = new QPushButton(panel);
    close_button->setObjectName(QStringLiteral("launcherDialogClose"));
    close_button->setFixedSize(util::layout::modal_close::k_hit);
    close_button->setCursor(Qt::PointingHandCursor);
    close_button->setFlat(true);
    close_button->setAttribute(Qt::WA_TranslucentBackground);
    close_button->setIcon(QIcon(util::assets::images[util::assets::Image::CloseSettings]));
    close_button->setIconSize(util::layout::modal_close::k_icon);
    connect(close_button, &QPushButton::clicked, this, &QDialog::reject);
    root->addLayout(top);

    message_label = new QLabel(panel);
    message_label->setTextFormat(Qt::PlainText);
    message_label->setWordWrap(true);
    message_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    message_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont message_font = util::assets::fonts[util::assets::Font::Inter];
    message_font.setPixelSize(14);
    message_font.setWeight(QFont::Medium);
    message_label->setFont(message_font);
    message_label->setStyleSheet(QStringLiteral("color: #4F1717;"));
    message_label->setMinimumWidth(574);
    root->addWidget(message_label);

    details_card = new QFrame(panel);
    details_card->setObjectName(QStringLiteral("launcherDialogDetails"));
    auto* details_layout = new QVBoxLayout(details_card);
    details_layout->setContentsMargins(16, 12, 16, 12);
    details_label = new QLabel(details_card);
    details_label->setTextFormat(Qt::PlainText);
    details_label->setWordWrap(true);
    details_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont details_font = util::assets::fonts[util::assets::Font::Inter];
    details_font.setPixelSize(12);
    details_font.setWeight(QFont::Medium);
    details_label->setFont(details_font);
    details_label->setStyleSheet(QStringLiteral("color: #6C584A;"));
    details_layout->addWidget(details_label);
    root->addWidget(details_card);

    action_container = new QWidget(panel);
    action_container->setMinimumHeight(48);
    action_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    action_container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* actions = new QHBoxLayout(action_container);
    actions->setContentsMargins(0, 4, 0, 0);
    actions->setSpacing(12);
    actions->addStretch();
    root->addWidget(action_container);

    category->setProperty("dialogCategoryLabel", true);
}

QString LauncherDialog::tone_label() const
{
    switch (tone)
    {
        case Tone::Information: return QStringLiteral("INFORMATION");
        case Tone::Warning: return QStringLiteral("WARNING");
        case Tone::Error: return QStringLiteral("ERROR");
        case Tone::Question: return QStringLiteral("CONFIRMATION");
    }
    return {};
}

QString LauncherDialog::tone_symbol() const
{
    switch (tone)
    {
        case Tone::Information: return QStringLiteral("i");
        case Tone::Warning: return QStringLiteral("!");
        case Tone::Error: return QStringLiteral("×");
        case Tone::Question: return QStringLiteral("?");
    }
    return {};
}

void LauncherDialog::retranslate()
{
    setWindowTitle(util::i18n::translate(title_source));
    title_label->setText(util::i18n::translate(title_source));
    message_label->setText(util::i18n::translate(message_source));
    details_label->setText(util::i18n::translate(details_source));
    details_card->setVisible(!details_source.isEmpty());
    tone_badge->setText(tone_symbol());
    close_button->setAccessibleName(util::i18n::translate("Close window"));

    QString accent;
    switch (tone)
    {
        case Tone::Information: accent = QStringLiteral("#168EB8"); break;
        case Tone::Warning: accent = QStringLiteral("#C56A16"); break;
        case Tone::Error: accent = QStringLiteral("#B9352B"); break;
        case Tone::Question: accent = QStringLiteral("#4F1717"); break;
    }
    tone_badge->setStyleSheet(QStringLiteral(
        "color: #FFFFFF; background: %1; border-radius: 21px;").arg(accent));

    const auto category_labels = panel->findChildren<QLabel*>();
    for (QLabel* label : category_labels)
    {
        if (label->property("dialogCategoryLabel").toBool())
        {
            label->setText(util::i18n::translate(tone_label()));
            label->setStyleSheet(QStringLiteral(
                "color: %1; letter-spacing: 1px;").arg(accent));
        }
    }

    for (QPushButton* button : std::as_const(action_buttons))
    {
        const QString source = button->property("dialogTextSource").toString();
        const QString translated = util::i18n::translate(source);
        button->setText(translated);
        button->setAccessibleName(translated);
        button->setToolTip(translated);
    }

    panel->layout()->activate();
    layout()->activate();
    const int action_width = action_container->layout()->sizeHint().width();
    const int required_width = qBound(680, action_width + 112, 860);
    message_label->setMinimumWidth(required_width - 106);
    panel->setMinimumWidth(required_width - 44);
    panel->layout()->activate();
    layout()->activate();
    const int required_height = qMax(220, layout()->sizeHint().height());
    setFixedSize(required_width, required_height);
    layout()->activate();
    panel->layout()->activate();
    close_button->setGeometry(util::layout::modal_close::rect_in(panel->rect()));
    close_button->raise();
}

QPushButton* LauncherDialog::add_action(const Action& action)
{
    auto* layout = qobject_cast<QHBoxLayout*>(action_container->layout());
    auto* button = new QPushButton(action_container);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(42);
    button->setMinimumWidth(118);
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    QFont font = util::assets::fonts[util::assets::Font::EurostileBlack];
    font.setPixelSize(12);
    font.setWeight(QFont::Black);
    button->setFont(font);
    button->setProperty("dialogTextSource", action.text);
    switch (action.style)
    {
        case ActionStyle::Primary:
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #20ACDA; color: #FFFFFF;"
                "border: 1px solid #1198C5; border-radius: 6px; padding: 9px 18px; }"
                "QPushButton:hover { background: #39BCE7; border-color: #168BB0; }"
                "QPushButton:pressed { background: #168EB8; }"));
            break;
        case ActionStyle::Neutral:
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: rgba(255,255,255,210); color: #4F1717;"
                "border: 1px solid rgba(79,23,23,72); border-radius: 6px; padding: 9px 18px; }"
                "QPushButton:hover { background: #EEE0D7; border-color: rgba(79,23,23,125); }"
                "QPushButton:pressed { background: #DDC9BD; }"));
            break;
        case ActionStyle::Destructive:
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #E84B24; color: #FFFFFF;"
                "border: 1px solid #C83515; border-radius: 6px; padding: 9px 18px; }"
                "QPushButton:hover { background: #F15F38; border-color: #B92D10; }"
                "QPushButton:pressed { background: #C93616; }"));
            break;
    }
    connect(button, &QPushButton::clicked, this, [this, result = action.result]()
    {
        done(result);
    });
    layout->addWidget(button);
    action_buttons.push_back(button);
    if (action.is_default)
    {
        button->setDefault(true);
    }
    retranslate();
    return button;
}

void LauncherDialog::set_actions(const QVector<Action>& actions)
{
    auto* layout = qobject_cast<QHBoxLayout*>(action_container->layout());
    for (QPushButton* button : std::as_const(action_buttons))
    {
        layout->removeWidget(button);
        delete button;
    }
    action_buttons.clear();
    for (const Action& action : actions)
        add_action(action);
}

LauncherDialog* LauncherDialog::open_message(QWidget* parent, const Tone tone,
                                             const QString& title, const QString& message,
                                             const QString& details)
{
    auto* dialog = new LauncherDialog(tone, title, message, details, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->add_action({QStringLiteral("OK"), Primary, ActionStyle::Primary, true});
    dialog->open();
    return dialog;
}

void LauncherDialog::information(QWidget* parent, const QString& title,
                                 const QString& message, const QString& details)
{
    LauncherDialog dialog(Tone::Information, title, message, details, parent);
    dialog.add_action({QStringLiteral("OK"), Primary, ActionStyle::Primary, true});
    dialog.exec();
}

void LauncherDialog::warning(QWidget* parent, const QString& title,
                             const QString& message, const QString& details)
{
    LauncherDialog dialog(Tone::Warning, title, message, details, parent);
    dialog.add_action({QStringLiteral("OK"), Primary, ActionStyle::Primary, true});
    dialog.exec();
}

void LauncherDialog::error(QWidget* parent, const QString& title,
                           const QString& message, const QString& details)
{
    LauncherDialog dialog(Tone::Error, title, message, details, parent);
    dialog.add_action({QStringLiteral("OK"), Primary, ActionStyle::Primary, true});
    dialog.exec();
}

bool LauncherDialog::confirm(QWidget* parent, const Tone tone, const QString& title,
                             const QString& message, const QString& accept_text,
                             const QString& cancel_text, const bool destructive)
{
    const QVector<Action> actions {
        {cancel_text, Cancelled, ActionStyle::Neutral, true},
        {accept_text, Primary,
         destructive ? ActionStyle::Destructive : ActionStyle::Primary, false}
    };
    return choose(parent, tone, title, message, actions) == Primary;
}

int LauncherDialog::choose(QWidget* parent, const Tone tone, const QString& title,
                           const QString& message, const QVector<Action>& actions,
                           const QString& details)
{
    LauncherDialog dialog(tone, title, message, details, parent);
    dialog.set_actions(actions);
    return dialog.exec();
}

void LauncherDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && event->position().y() <= 78)
    {
        dragging = true;
        drag_offset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void LauncherDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging && event->buttons().testFlag(Qt::LeftButton))
    {
        move(event->globalPosition().toPoint() - drag_offset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void LauncherDialog::mouseReleaseEvent(QMouseEvent* event)
{
    dragging = false;
    QDialog::mouseReleaseEvent(event);
}
