#include "core/status/StatusBus.hpp"
#include "core/status/StatusReporter.hpp"

#include <QTimer>

#include "core/Log.hpp"
#include <spdlog/spdlog.h>

namespace core::status
{
    namespace
    {
        constexpr int   k_watchdog_interval_ms = 5000;
        constexpr qint64 k_stall_threshold_s   = 120;
    }

    StatusBus& StatusBus::instance()
    {
        static StatusBus bus;
        return bus;
    }

    StatusBus::StatusBus(QObject* parent) : QObject(parent)
    {
        watchdog = new QTimer(this);
        watchdog->setInterval(k_watchdog_interval_ms);
        connect(watchdog, &QTimer::timeout, this, [this]() { check_for_stalls(); });
        watchdog->start();
    }

    void StatusBus::register_reporter(StatusReporter* r)
    {
        if (!r || items.contains(r)) return;
        items.append(r);

        connect(r, &StatusReporter::status_changed, this, [this, r](const Status& now)
        {
            emit reporter_status_changed(r, now);
        });

        SPDLOG_DEBUG("status bus: registered '{}' ({} total)",
                     r->reporter_name().toStdString(), items.size());
    }

    void StatusBus::unregister_reporter(StatusReporter* r)
    {
        items.removeAll(r);
    }

    bool StatusBus::any_working() const
    {
        for (const auto* r : items)
            if (r->status().state == State::Working) return true;
        return false;
    }

    StatusReporter* StatusBus::find(const QString& name) const
    {
        for (auto* r : items)
            if (r->reporter_name() == name) return r;
        return nullptr;
    }

    void StatusBus::check_for_stalls()
    {
        const QDateTime now = QDateTime::currentDateTime();
        for (auto* r : items)
        {
            const Status& s = r->status();
            if (s.state != State::Working) continue;

            const qint64 secs = s.last_changed.secsTo(now);
            if (secs >= k_stall_threshold_s)
            {
                SPDLOG_WARN("status bus: '{}' stalled in Working for {}s (phase '{}')",
                            r->reporter_name().toStdString(), secs, s.phase.toStdString());
                emit stalled(r, secs);
            }
        }
    }
}