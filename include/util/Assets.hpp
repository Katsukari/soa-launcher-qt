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
        BoxCard,
        BoxDownload,
        BoxGameInstall,
        BoxModal,
        BoxNote,
        BoxNote2,
        BoxSettings,
        BoxWaitingForAuth,
        CloseIcon,
        CloseNormal,
        CloseSettings,
        InstallPath,
        IntegrityCheckFile,
        LeftFrame,
        MenuDropdown,
        Minimize,
        ProgressBarEnd,
        ProgressBarMiddle,
        ProgressBarStart,
        ProgressBarTrack,
        RightFrame,
        RulesFrame,
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
        Repair,
        SliderOn,
        SliderOff,
        Count
    };

    enum class Font
    {
        EurostileBold,
        EurostileBlack,
        EurostileExtraBlack,
        Inter,
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
