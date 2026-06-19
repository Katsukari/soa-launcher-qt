#pragma once

#include <QPixmap>
#include <optional>
#include <QString>
#include <QFontDatabase>
#include <QPixmap>
#include <QFont>
#include <unordered_map>
#include "spdlog/spdlog.h"

namespace util::assets
{
    enum class Image
    {
        Background,
        BackgroundBox,
        BoxCard,
        BoxDisclaimer,
        BoxDownload,
        BoxDownloadWindow,
        BoxGameInstall,
        BoxModal,
        BoxNote,
        BoxNote2,
        BoxSettings,
        BoxUpdate,
        BoxWaitingForAuth,
        Checkbox,
        CheckboxTicked,
        CloseClicked,
        CloseIcon,
        CloseNormal,
        CloseSettings,
        InstallPath,
        IntegrityCheckFile,
        LeftFrame,
        MenuDropdown,
        Minimize,
        ProgressBarEnd,
        ProgressBarLoading,
        ProgressBarMiddle,
        ProgressBarStart,
        ProgressBarTrack,
        RightFrame,
        RulesFrame,
        SignedInAs,
        Logo,
        IconLock,
        IconPT,
        SettingsButton,
    };

    enum class Button
    {
        Agree,
        Enter,
        Cancel,
        Discord,
        DownloadGame,
        Install,
        RunCheck,
        UpdateAvailable,
        Update,
        Repair,
        Settings,
        SliderOn,
        SliderOff
    };

    enum class Font
    {
        EurostileRegular,
        EurostileBold,
        EurostileBlack,
        EurostileExtraBlack,
        Inter,
        NanumRegular,
        NanumBold,
        NanumExtraBold
    };

    struct ButtonAsset
    {
        QPixmap normal;
        QPixmap hover;
        QPixmap clicked;
        QPixmap loading;
    };

    void load_fonts();
    void load_buttons();
    void load_images();
    void load_all();

    inline std::unordered_map<Image, QPixmap> images;
    inline std::unordered_map<Button, ButtonAsset> buttons;
    inline std::unordered_map<Font, QFont> fonts;
}
