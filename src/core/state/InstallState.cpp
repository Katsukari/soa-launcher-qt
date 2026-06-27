#include "../../../include/core/state/InstallState.hpp"
#include "core/status/StatusBus.hpp"
#include "core/status/StatusReporter.hpp"
#include "util/Config.hpp"
#include "core/wine/WineRegistry.hpp"

#include "core/Log.hpp"
#include <spdlog/spdlog.h>

#include <QDir>

namespace core::state
{
    namespace
    {
        const QString k_reporter_wine    = "wine";
        const QString k_reporter_courier = "courier";
        const QString k_reporter_auth    = "auth";
    }

    using core::status::State;
    using core::status::Status;
    using core::status::StatusBus;
    using core::status::StatusReporter;

    InstallState::InstallState(QObject* parent) : QObject(parent)
    {
        connect(&StatusBus::instance(), &StatusBus::reporter_status_changed, this,
            [this](StatusReporter* r, const Status& s)
            {
                on_reporter_changed(r->reporter_name(), s);
            });
    }

    void InstallState::probe()
    {
        auto& cfg = util::config::Config::instance();

        const QString prefix = cfg.wine_prefix();
        prefix_exists = !prefix.isEmpty() && QDir(prefix).exists("drive_c");
        prefix_ready  = prefix_exists && QDir(prefix).exists("drive_c/windows/system32/d3dx9_43.dll");

        game_installed = cfg.game_installed();
        authed         = cfg.has_auth();

        update_needed  = false;

        probed = true;
        recompute();
    }

    void InstallState::set_prefix_exists(bool v)  { if (prefix_exists  != v) { prefix_exists  = v; recompute(); } }
    void InstallState::set_prefix_ready(bool v)   { if (prefix_ready   != v) { prefix_ready   = v; recompute(); } }
    void InstallState::set_game_installed(bool v) { if (game_installed != v) { game_installed = v; recompute(); } }
    void InstallState::set_update_needed(bool v)  { if (update_needed  != v) { update_needed  = v; recompute(); } }
    void InstallState::set_authed(bool v)         { if (authed         != v) { authed         = v; recompute(); } }
    void InstallState::set_broken(bool v)         { if (broken         != v) { broken         = v; recompute(); } }

    void InstallState::on_reporter_changed(const QString& name, const Status& s)
    {
        if (name == k_reporter_wine)         wine_state    = s.state;
        else if (name == k_reporter_courier) courier_state = s.state;
        else if (name == k_reporter_auth)    auth_state    = s.state;
        else return;

        if (s.state == State::Done || s.state == State::Failed)
            probe();
        else
            recompute();
    }

    Stage InstallState::compute() const
    {
        if (!probed) return Stage::Probing;
        if (broken)  return Stage::Broken;

        if (wine_state == State::Working) return Stage::SettingUpPrefix;
        if (!prefix_exists)               return Stage::NeedsPrefix;
        if (!prefix_ready)                return Stage::PrefixBroken;

        if (courier_state == State::Working)
            return game_installed ? Stage::Updating : Stage::Downloading;
        if (!game_installed)              return Stage::NeedsDownload;
        if (update_needed)                return Stage::NeedsUpdate;

        if (auth_state == State::Working) return Stage::Authenticating;
        if (!authed)                      return Stage::NeedsAuth;

        return Stage::Ready;
    }

    void InstallState::recompute()
    {
        const Stage next = compute();
        if (next == current) return;

        SPDLOG_INFO("install state: {} -> {}", to_string(current), to_string(next));
        current = next;
        emit stage_changed(current);
    }
}