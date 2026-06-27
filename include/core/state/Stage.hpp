#pragma once

namespace core::state
{
    enum class Stage
    {
        Probing,
        NeedsPrefix,
        PrefixBroken,
        SettingUpPrefix,
        NeedsDownload,
        Downloading,
        NeedsUpdate,
        Updating,
        NeedsAuth,
        Authenticating,
        Ready,
        Broken
    };

    inline const char* to_string(Stage s)
    {
        switch (s)
        {
            case Stage::Probing:         return "Probing";
            case Stage::NeedsPrefix:     return "NeedsPrefix";
            case Stage::PrefixBroken:    return "PrefixBroken";
            case Stage::SettingUpPrefix: return "SettingUpPrefix";
            case Stage::NeedsDownload:   return "NeedsDownload";
            case Stage::Downloading:     return "Downloading";
            case Stage::NeedsUpdate:     return "NeedsUpdate";
            case Stage::Updating:        return "Updating";
            case Stage::NeedsAuth:       return "NeedsAuth";
            case Stage::Authenticating:  return "Authenticating";
            case Stage::Ready:           return "Ready";
            case Stage::Broken:          return "Broken";
        }
        return "Probing";
    }
}