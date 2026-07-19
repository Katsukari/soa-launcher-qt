#pragma once

namespace core::state
{
    enum class View
    {
        Loading,
        WineSelect,
        WineInstall,
        GameInstall,
        AliciaChooser,
        Error
    };

    inline const char* to_string(View v)
    {
        switch (v)
        {
            case View::Loading:     return "Loading";
            case View::WineSelect:  return "WineSelect";
            case View::WineInstall: return "WineInstall";
            case View::GameInstall: return "GameInstall";
            case View::AliciaChooser: return "AliciaChooser";
            case View::Error:       return "Error";
        }
        return "Loading";
    }
}