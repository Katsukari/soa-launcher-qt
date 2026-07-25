#include "util/SimpleUtils.hpp"

#include "util/LanguageManager.hpp"
#include "util/Layout.hpp"
#include "util/Styles.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QPalette>
#include <QVariant>

#include <initializer_list>

namespace util::simple_utils
{
    void make_label_block(QWidget* parent, const QSize window_size, const int y,
                          const QString& title, const QString& description)
    {
        auto* title_label = new QLabel(i18n::translate(title), parent);
        QFont title_font = assets::fonts[assets::Font::EurostileBlack];
        title_font.setPixelSize(layout::scaled(layout::text::k_row_title, window_size));
        title_font.setWeight(QFont::Black);
        title_label->setFont(title_font);
        title_label->setStyleSheet(QStringLiteral("color: #4F1717; background: transparent;"));
        title_label->setGeometry(layout::settings::row_title(window_size, y));
        title_label->setProperty("soa_i18n_text_source", title);

        auto* description_label = new QLabel(i18n::translate(description), parent);
        description_label->setWordWrap(true);
        QFont description_font = assets::fonts[assets::Font::Inter];
        description_font.setPixelSize(layout::scaled(layout::text::k_desc, window_size));
        description_font.setWeight(QFont::Medium);
        description_label->setFont(description_font);
        description_label->setStyleSheet(QStringLiteral("color: #4F1717; background: transparent;"));
        description_label->setGeometry(layout::settings::row_desc(window_size, y));
        description_label->setProperty("soa_i18n_text_source", description);
    }

    QPushButton* make_flat_button(QWidget* parent)
    {
        auto* button = new QPushButton(parent);
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(styles::k_flat_transparent);
        return button;
    }

    namespace
    {
        constexpr auto k_button_text_name = "soa_button_text_overlay";
        constexpr auto k_text_source_property = "soa_i18n_text_source";
        constexpr auto k_base_pixel_size_property = "soa_button_text_base_pixel_size";
        constexpr auto k_asset_property = "soa_button_asset";
        constexpr auto k_loading_property = "soa_button_loading";
        constexpr auto k_connected_property = "soa_button_language_connected";

        QLabel* button_text_label(QPushButton* button)
        {
            return button
                ? button->findChild<QLabel*>(QString::fromLatin1(k_button_text_name),
                                             Qt::FindDirectChildrenOnly)
                : nullptr;
        }

        void enforce_white_button_text(QLabel* label)
        {
            if (!label)
                return;

            QPalette palette = label->palette();
            for (const QPalette::ColorGroup group :
                 {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
            {
                palette.setColor(group, QPalette::WindowText, Qt::white);
                palette.setColor(group, QPalette::Text, Qt::white);
            }
            label->setPalette(palette);
            label->setStyleSheet(QStringLiteral(
                "QLabel { background: transparent; color: #FFFFFF; }"
                "QLabel:disabled { color: #FFFFFF; }"));
        }

        void fit_button_text(QLabel* label)
        {
            if (!label)
                return;

            QFont font = label->font();
            const int base_size = label->property(k_base_pixel_size_property).toInt();
            const int maximum_width = qMax(1, label->width() - 12);
            int pixel_size = qMax(8, base_size);
            while (pixel_size > 8)
            {
                font.setPixelSize(pixel_size);
                if (QFontMetrics(font).horizontalAdvance(label->text()) <= maximum_width)
                    break;
                --pixel_size;
            }
            font.setPixelSize(pixel_size);
            label->setFont(font);
        }

        assets::Button button_asset_key(QPushButton* button)
        {
            return static_cast<assets::Button>(button->property(k_asset_property).toInt());
        }
    }

    void refresh_button(QPushButton* button)
    {
        if (!button || !button->property(k_asset_property).isValid())
            return;

        const assets::Button key = button_asset_key(button);
        const assets::ButtonAsset& asset = assets::button(key);
        const bool loading = button->property(k_loading_property).toBool();
        const QPixmap& pixmap = loading && !asset.loading.isNull() ? asset.loading : asset.normal;
        QIcon icon;
        icon.addPixmap(pixmap, QIcon::Normal);
        icon.addPixmap(pixmap, QIcon::Disabled);
        button->setIcon(icon);

        if (QLabel* label = button_text_label(button))
        {
            const QString source = label->property(k_text_source_property).toString();
            label->setText(i18n::translate(source));
            enforce_white_button_text(label);
            fit_button_text(label);
            label->setVisible(assets::translated_button_assets_active());
            label->raise();
        }
    }

    void set_button_asset(QPushButton* button, const assets::Button asset)
    {
        if (!button)
            return;
        button->setProperty(k_asset_property, static_cast<int>(asset));
        refresh_button(button);
    }

    void set_button_loading(QPushButton* button, const bool loading)
    {
        if (!button)
            return;
        button->setProperty(k_loading_property, loading);
        refresh_button(button);
    }

    QLabel* add_button_text(QPushButton* button, const assets::Button asset,
                            const QString& source, const QFont& font,
                            const QColor&, const QPoint offset)
    {
        if (!button)
            return nullptr;

        QLabel* label = button_text_label(button);
        if (!label)
        {
            label = new QLabel(button);
            label->setObjectName(QString::fromLatin1(k_button_text_name));
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            label->setAlignment(Qt::AlignCenter);
            label->setTextFormat(Qt::PlainText);
        }

        label->setGeometry(button->rect().adjusted(offset.x(), offset.y(), 0, 0));
        label->setFont(font);
        label->setProperty(k_base_pixel_size_property, font.pixelSize());
        label->setProperty(k_text_source_property, source);
        enforce_white_button_text(label);
        button->setProperty(k_asset_property, static_cast<int>(asset));
        button->setProperty(k_loading_property, false);

        if (!button->property(k_connected_property).toBool())
        {
            button->setProperty(k_connected_property, true);
            QObject::connect(&i18n::LanguageManager::instance(),
                             &i18n::LanguageManager::language_changed,
                             button, [button]()
            {
                refresh_button(button);
            });
        }

        refresh_button(button);
        return label;
    }

    void set_button_text(QPushButton* button, const QString& source)
    {
        QLabel* label = button_text_label(button);
        if (!label)
            return;
        label->setProperty(k_text_source_property, source);
        refresh_button(button);
    }

    bool apply_button_state(QEvent* event, QPushButton* button,
                            const QPixmap& normal, const QPixmap& hover,
                            const QPixmap& clicked)
    {
        switch (event->type())
        {
            case QEvent::Enter:
                button->setIcon(QIcon(hover));
                return true;
            case QEvent::Leave:
                button->setIcon(QIcon(normal));
                return true;
            case QEvent::MouseButtonPress:
                button->setIcon(QIcon(clicked));
                return true;
            case QEvent::MouseButtonRelease:
                button->setIcon(QIcon(button->underMouse() ? hover : normal));
                return true;
            default:
                return false;
        }
    }
}
