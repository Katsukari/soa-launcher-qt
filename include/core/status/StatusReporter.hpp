#pragma once

#include <QObject>
#include "core/status/Status.hpp"

namespace core::status
{
    class StatusReporter : public QObject
    {
        Q_OBJECT

        public:
            explicit StatusReporter(QString name, QObject* parent = nullptr);
            ~StatusReporter() override;

            const Status& status() const { return current; }
            const QString& reporter_name() const { return name; }

            bool is_working() const { return current.state == State::Working; }

            signals:
                void status_changed(const Status & now);

        protected:
            void set_status(Status next);

            void working(const QString& phase, double progress = -1.0);
            void done(const QString& message = {});
            void fail(const QString& message);
            void retry(const QString& message);
            void idle();

        private:
            QString name;
            Status  current;
    };
}