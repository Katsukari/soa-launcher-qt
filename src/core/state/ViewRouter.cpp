#include "core/state/ViewRouter.hpp"

namespace core::state
{
    View view_for(const Stage stage)
    {
        switch (stage)
        {
            case Stage::Probing:         return View::Loading;

            case Stage::NeedsRuntime:    return View::WineSelect;

            case Stage::NeedsPrefix:     return View::WineInstall;
            case Stage::PrefixBroken:    return View::WineInstall;
            case Stage::SettingUpPrefix: return View::WineInstall;

            case Stage::NeedsDownload:   return View::GameInstall;
            case Stage::NeedsUpdate:     return View::GameInstall;
            case Stage::Downloading:     return View::GameInstall;
            case Stage::Updating:        return View::GameInstall;

            case Stage::NeedsAuth:       return View::AliciaChooser;
            case Stage::Authenticating:  return View::AliciaChooser;
            case Stage::Ready:           return View::AliciaChooser;

            case Stage::Broken:          return View::Error;
        }
        return View::Loading;
    }
}