#pragma once

#include <QObject>
#include "core/state/Stage.hpp"
#include "core/status/Status.hpp"

namespace core::state
{
    class InstallState : public QObject
    {
        Q_OBJECT

        public:
            explicit InstallState(QObject* parent = nullptr);

            Stage stage() const { return current; }

            void probe();

            void set_prefix_exists(bool v);
            void set_prefix_ready(bool v);
            void set_game_installed(bool v);
            void set_update_needed(bool v);
            void set_authed(bool v);
            void set_broken(bool v);

            signals:
                void stage_changed(core::state::Stage now);

        private:
            void on_reporter_changed(const QString& name, const status::Status& s);
            void recompute();
            Stage compute() const;

            bool probed         {false};
            bool broken         {false};
            bool runtime_chosen {false};
            bool prefix_exists  {false};
            bool prefix_ready   {false};
            bool game_installed {false};
            bool update_needed  {false};
            bool authed         {false};

            core::status::State wine_state    {status::State::Idle};
            core::status::State courier_state {status::State::Idle};
            core::status::State auth_state    {status::State::Idle};

            Stage current {Stage::Probing};
    };
}
