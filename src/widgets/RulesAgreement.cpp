#include "widgets/RulesAgreement.hpp"

#include "util/Assets.hpp"
#include "util/Colors.hpp"
#include "util/Layout.hpp"
#include "util/SimpleUtils.hpp"

#include <QCheckBox>
#include <QFrame>
#include <QIcon>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBrowser>

namespace
{
    constexpr QSize k_box_size {667, 635};

    QRect box_rect(const QSize window_size)
    {
        return util::layout::centered(k_box_size, window_size, 0, 4);
    }

    QRect local_rect(const QSize window_size, const QRect source)
    {
        const QRect box = box_rect(window_size);
        return util::layout::scaled(source, window_size).translated(box.topLeft());
    }

    const char* k_checkbox_style =
        "QCheckBox { color:#4F1717; font-family:'Inter'; font-size:14px; spacing:10px; outline:none; }"
        "QCheckBox::indicator { width:24px; height:24px; }"
        "QCheckBox::indicator:unchecked { image:url(:/assets/checkbox.png); }"
        "QCheckBox::indicator:checked { image:url(:/assets/checkbox-ticked.png); }"
        "QCheckBox:focus { outline:none; }";
}

RulesAgreement::RulesAgreement(QWidget* parent)
    : ModalOverlay(parent)
{
    setup_controls();
}

void RulesAgreement::setup_controls()
{
    const QSize w = window()->size();

    rules_text = new QTextBrowser(this);
    rules_text->setGeometry(local_rect(w, {54, 94, 559, 390}));
    rules_text->setFrameShape(QFrame::NoFrame);
    rules_text->setOpenExternalLinks(true);
    rules_text->setStyleSheet(
        "QTextBrowser { background:rgba(255,255,255,0.22); color:#392518; padding:18px; "
        "font-family:'Inter'; font-size:13px; border-radius:8px; }"
        "QScrollBar:vertical { width:7px; background:#E4DED9; margin:2px; }"
        "QScrollBar::handle:vertical { background:#B0A297; border-radius:3px; min-height:24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }");
    rules_text->setHtml(QStringLiteral(
        "<h2 style='color:#4F1717; margin-top:0;'>Before entering the playtest</h2>"
        "<p>The authoritative Story of Alicia server rules are maintained in the linked document and may be updated by the team.</p>"
        "<p><a style='color:#2FB4E0; font-weight:700;' href='https://docs.google.com/document/d/1vry3ZuDtzdS_mX1P2udWlb8z2Q9Atr3p1THZdtZ2EHA/edit'>Open the complete Story of Alicia server rules</a></p>"
        "<hr>"
        "<h3 style='color:#4F1717;'>Your playtest acknowledgement</h3>"
        "<ul>"
        "<li>You have read the complete rules document and agree to obey it while using either game version.</li>"
        "<li>You understand that the game is unfinished, contains bugs, and may change during testing.</li>"
        "<li>You understand that access can be limited or removed when the rules are not followed.</li>"
        "<li>You will report serious problems to the team instead of intentionally abusing them.</li>"
        "</ul>"
        "<p>The launcher stores this acceptance in its local configuration so the rules step is not shown on every start. Resetting launcher settings will show it again.</p>"));

    accepted_box = new QCheckBox(
        QStringLiteral("I have read and agree to follow the Story of Alicia server rules."), this);
    accepted_box->setStyleSheet(k_checkbox_style);
    accepted_box->setGeometry(local_rect(w, {66, 500, 535, 34}));
    accepted_box->setAccessibleName(QStringLiteral("Accept Story of Alicia rules"));
    connect(accepted_box, &QCheckBox::toggled, this, [this]() { update_agree_button(); });

    agree_button = util::simple_utils::make_flat_button(this);
    const auto& agree = util::assets::buttons[util::assets::Button::Agree];
    const QSize agree_size = util::layout::scaled(agree.normal.size(), w);
    const QRect box = box_rect(w);
    agree_button->setGeometry(box.center().x() - agree_size.width() / 2,
                              box.bottom() - util::layout::scaled(64, w),
                              agree_size.width(), agree_size.height());
    agree_button->setIconSize(agree_size);
    agree_button->setAccessibleName(QStringLiteral("Agree with the rules"));
    agree_button->installEventFilter(this);
    connect(agree_button, &QPushButton::clicked, this, [this]()
    {
        if (!accepted_box->isChecked())
            return;
        hide();
        emit accepted();
    });

    update_agree_button();
}

void RulesAgreement::update_agree_button()
{
    const auto& agree = util::assets::buttons[util::assets::Button::Agree];
    const bool enabled = accepted_box && accepted_box->isChecked();
    const QPixmap& pixmap = enabled || agree.loading.isNull() ? agree.normal : agree.loading;

    QIcon icon;
    icon.addPixmap(pixmap, QIcon::Normal);
    icon.addPixmap(pixmap, QIcon::Disabled);
    agree_button->setIcon(icon);
    agree_button->setEnabled(enabled);
}


void RulesAgreement::showEvent(QShowEvent* event)
{
    accepted_box->setChecked(false);
    rules_text->verticalScrollBar()->setValue(0);
    update_agree_button();
    ModalOverlay::showEvent(event);
}

void RulesAgreement::paint_content(QPainter& painter)
{
    const QSize w = window()->size();
    painter.drawPixmap(box_rect(w), util::assets::images[util::assets::Image::RulesFrame]);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(util::layout::scaled(25, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(util::colors::k_text_maroon);
    painter.drawText(local_rect(w, {25, 35, 617, 38}), Qt::AlignCenter,
                     QStringLiteral("STORY OF ALICIA PLAYTEST RULES"));
}

bool RulesAgreement::eventFilter(QObject* object, QEvent* event)
{
    if (object == agree_button && agree_button->isEnabled())
    {
        const auto& agree = util::assets::buttons[util::assets::Button::Agree];
        util::simple_utils::apply_button_state(
            event, agree_button, agree.normal, agree.hover, agree.clicked);
    }
    return QWidget::eventFilter(object, event);
}
