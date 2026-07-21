#pragma once

#include <QFont>
#include <QPixmap>
#include <unordered_map>

namespace util::assets
{
    enum class Image
    {
        BackgroundPlaytest,
        BackgroundAlicia2,
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
        VersionFrameActive,
        VersionFrameInactive,
        VersionIconPlaytest,
        VersionIconAlicia2,
        SettingsButton,
        Count
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
        SliderOff,
        Count
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
        NanumExtraBold,
        Count
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
