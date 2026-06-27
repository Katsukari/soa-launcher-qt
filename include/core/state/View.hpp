#pragma once

namespace core::state
{
    enum class View
    {
        Loading,
        WineInstall,
        PrefixProgress,
        GameInstall,
        DownloadProgress,
        PlaytestLogin,
        PlaytestWaiting,
        PlaytestReady,
        Error
    };

    inline const char* to_string(View v)
    {
        switch (v)
        {
            case View::Loading:          return "Loading";
            case View::WineInstall:      return "WineInstall";
            case View::PrefixProgress:   return "PrefixProgress";
            case View::GameInstall:      return "GameInstall";
            case View::DownloadProgress: return "DownloadProgress";
            case View::PlaytestLogin:    return "PlaytestLogin";
            case View::PlaytestWaiting:  return "PlaytestWaiting";
            case View::PlaytestReady:    return "PlaytestReady";
            case View::Error:            return "Error";
        }
        return "Loading";
    }
}