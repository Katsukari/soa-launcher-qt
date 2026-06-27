#include "../../../include/core/state/ViewRouter.hpp"

namespace core::state
{
    View view_for(const Stage stage)
    {
        switch (stage)
        {
            case Stage::Probing:         return View::Loading;
            case Stage::NeedsPrefix:     return View::WineInstall;
            case Stage::PrefixBroken:    return View::WineInstall;
            case Stage::SettingUpPrefix: return View::PrefixProgress;
            case Stage::NeedsDownload:   return View::GameInstall;
            case Stage::Downloading:     return View::DownloadProgress;
            case Stage::NeedsUpdate:     return View::GameInstall;
            case Stage::Updating:        return View::DownloadProgress;
            case Stage::NeedsAuth:       return View::PlaytestLogin;
            case Stage::Authenticating:  return View::PlaytestWaiting;
            case Stage::Ready:           return View::PlaytestReady;
            case Stage::Broken:          return View::Error;
        }
        return View::Loading;
    }
}