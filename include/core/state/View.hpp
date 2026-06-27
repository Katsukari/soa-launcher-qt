#pragma once

namespace core::state
{
    enum class View
    {
        Loading,
        WineInstall,
        GameInstall,
        Playtest,
        Error
    };

    inline const char* to_string(View v)
    {
        switch (v)
        {
            case View::Loading:     return "Loading";
            case View::WineInstall: return "WineInstall";
            case View::GameInstall: return "GameInstall";
            case View::Playtest:    return "Playtest";
            case View::Error:       return "Error";
        }
        return "Loading";
    }
}