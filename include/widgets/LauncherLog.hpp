#pragma once
#include <QDialog>
#include <QVector>

class QPlainTextEdit;
class QComboBox;

// Single shared log window for the whole app.
//
// Previously each settings page constructed its own LauncherLog and connected it
// to the LogBridge, so every log line was stored twice in two windows. Use
// LauncherLog::instance() everywhere instead: it lazily creates one window,
// connects it to the bridge exactly once, and hands the same pointer to every
// caller.
class LauncherLog : public QDialog
{
    Q_OBJECT
    public:
        static LauncherLog* instance();

    public slots:
        void append_line(int level, const QString& text);

    private:
        explicit LauncherLog(QWidget* parent = nullptr);

        struct Entry { int level; QString text; };
        QVector<Entry> entries;
        int  min_level {2};
        bool autoscroll {true};

        QPlainTextEdit* output {};
        QComboBox*      verbosity {};

        void rerender();
        bool passes(int level) const { return level >= min_level; }
};