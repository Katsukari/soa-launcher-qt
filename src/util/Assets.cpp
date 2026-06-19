#include "util/Assets.hpp"

namespace util::assets
{
    QPixmap load_pixmap(const QString& path)
    {
        QPixmap px(":/assets/" + path);
        if (px.isNull()) spdlog::warn("Failed to load image: {}", path.toStdString());
        return px;
    }

    void load_images()
    {
        const std::initializer_list<std::pair<Image, QString>> defs =
        {
            {Image::Background,          "bg.png"},
            {Image::BackgroundBox,       "bg-box1.png"},
            {Image::BoxCard,             "box-card.png"},
            {Image::BoxDisclaimer,       "box-disclaimer.png"},
            {Image::BoxDownload,         "box-download.png"},
            {Image::BoxDownloadWindow,   "box-download-window.png"},
            {Image::BoxGameInstall,      "box-game-install.png"},
            {Image::BoxModal,            "box-modal.png"},
            {Image::BoxNote,             "box-note.png"},
            {Image::BoxNote2,            "box-note2.png"},
            {Image::BoxSettings,         "box-settings.png"},
            {Image::BoxUpdate,           "box-update.png"},
            {Image::BoxWaitingForAuth,   "box-waiting-for-auth.png"},
            {Image::Checkbox,            "checkbox.png"},
            {Image::CheckboxTicked,      "checkbox-ticked.png"},
            {Image::CloseClicked,        "close-clicked.png"},
            {Image::CloseIcon,           "close-icon2.png"},
            {Image::CloseNormal,         "close-normal.png"},
            {Image::CloseSettings,       "close-settings.png"},
            {Image::InstallPath,         "install-path.png"},
            {Image::IntegrityCheckFile,  "integrity-check-file.png"},
            {Image::LeftFrame,           "left-frame.png"},
            {Image::MenuDropdown,        "menu-dopdown.png"},
            {Image::Minimize,            "Minimize.png"},
            {Image::ProgressBarEnd,      "progress-bar-end.png"},
            {Image::ProgressBarLoading,  "progress-bar- loading.png"},
            {Image::ProgressBarMiddle,   "progress-bar-middle.png"},
            {Image::ProgressBarStart,    "progress-bar-start.png"},
            {Image::ProgressBarTrack,    "progress-bar-track.png"},
            {Image::RightFrame,          "right-frame.png"},
            {Image::RulesFrame,          "rules-frame.png"},
            {Image::SignedInAs,          "signed-in-as.png"},
            {Image::Logo,                "soa-logo.png"},
            {Image::IconLock,            "2.0 ICON.png"},
            {Image::IconPT,              "PT ICON.png"},
            {Image::SettingsButton, "Settings Button.png"},
        };

        for (const auto& [key, path] : defs)
        {
            if (auto px = load_pixmap(path); !px.isNull()) images[key] = px;
        }
    }

    void load_buttons()
    {
        struct ButtonDef
        {
            Button key;
            QString normal;
            QString hover;
            QString clicked;
            QString loading;
        };

        const std::initializer_list<ButtonDef> defs =
        {
            {Button::Agree,          "agree_normal.png",               "agree_hover.png",               "agree_clicked.png",               "agree-loading.png"},
            {Button::Enter,          "enter_normal.png",               "enter_hover.png",               "enter_clicked.png",               "enter_loading.png"},
            {Button::Cancel,         "btn-cancel-normal.png",          "btn-cancel-hover.png",          "btn-cancel-clicked.png",          ""},
            {Button::Discord,        "btn-discord-normal.png",         "btn-discord-hover.png",         "btn-discord-clicked.png",         "btn-discord-loading.png"},
            {Button::DownloadGame,   "btn-download-game-normal.png",   "btn-download-game-hover.png",   "btn-download-game-clicked.png",   "btn-download-game-loading.png"},
            {Button::Install,        "btn-install-normal.png",         "btn-install-hover.png",         "btn-install-clicked.png",         "btn-install-loading.png"},
            {Button::RunCheck,       "btn-run-check-normal.png",       "btn-run-check-hover.png",       "btn-run-check-clicked.png",       "btn-run-check-loading.png"},
            {Button::UpdateAvailable,"btn-update-available.png",       "btn-update-available-hover.png", "btn-update-available-clicked.png", ""},
            {Button::Update,         "update-hover.png",               "update-hover.png",              "update-clicked.png",              ""},
            {Button::Repair,         "btn-repair.png",                 "btn-repair-hover.png",          "btn-repair-clicked.png",          ""},
            {Button::Settings,       "Settings Button.png",            "",                              "",                                ""},
            {Button::SliderOn,       "slider-toggle-on.png",           "",                              "",                                ""},
            {Button::SliderOff,      "slider-toggle-off.png",          "",                              "",                                ""},
        };

        for (const auto & [key, normal, hover, clicked, loading] : defs)
        {
            ButtonAsset asset;
            asset.normal  = normal.isEmpty()  ? QPixmap{} : load_pixmap(normal);
            asset.hover   = hover.isEmpty()   ? QPixmap{} : load_pixmap(hover);
            asset.clicked = clicked.isEmpty() ? QPixmap{} : load_pixmap(clicked);
            asset.loading = loading.isEmpty() ? QPixmap{} : load_pixmap(loading);
            buttons[key] = asset;
        }
    }

    void load_fonts()
    {
        const std::initializer_list<std::pair<Font, QString>> defs =
        {
            {Font::EurostileRegular,    "fonts/Eurostile-Regular.otf"},
            {Font::EurostileBold,  "fonts/Eurostile-Bold.otf"},
            {Font::EurostileBlack,      "fonts/Eurostile-Black.otf"},
            {Font::EurostileExtraBlack, "fonts/Eurostile-ExtraBlack.otf"},
            {Font::Inter,               "fonts/InterVariable.ttf"},
            {Font::NanumRegular,        "fonts/NanumGothic-Regular.ttf"},
            {Font::NanumBold,           "fonts/NanumGothic-Bold.ttf"},
            {Font::NanumExtraBold,      "fonts/NanumGothic-ExtraBold.ttf"},
        };

        for (const auto& [key, path] : defs)
        {
            const int id = QFontDatabase::addApplicationFont(":/assets/" + path);

            if (id == -1)
            {
                spdlog::warn("Failed to load font: {}", path.toStdString());
                continue;
            }

            QStringList families = QFontDatabase::applicationFontFamilies(id);

            if (families.isEmpty())
            {
                spdlog::warn("No font families found in: {}", path.toStdString());
                continue;
            }

            fonts[key] = QFont(families.first());
        }
    }



    void load_all()
    {
        load_images();
        load_buttons();
        load_fonts();
    }
}
