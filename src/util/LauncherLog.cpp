#include "widgets/LauncherLog.hpp"
#include "core/QtLogSink.hpp"
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QScrollBar>
#include <QTextDocument>

#include "util/Config.hpp"

LauncherLog* LauncherLog::instance()
{
    static LauncherLog* s_instance = nullptr;
    if (!s_instance)
    {
        s_instance = new LauncherLog();
        connect(&LogBridge::instance(), &LogBridge::message, s_instance, &LauncherLog::append_line);
    }
    return s_instance;
}

LauncherLog::LauncherLog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Launcher Log");
    resize(680, 420);

    output = new QPlainTextEdit(this);
    output->setReadOnly(true);
    output->document()->setMaximumBlockCount(5000);
    output->setStyleSheet(
        "QPlainTextEdit { background:#1E1B17; color:#D8C9B8;"
        " font-family:'monospace'; font-size:12px; border:none; }");

    auto* clear = new QPushButton("Clear", this);
    auto* copy  = new QPushButton("Copy", this);
    auto* lock  = new QPushButton("Autoscroll: On", this);

    verbosity = new QComboBox(this);
    verbosity->addItem("Errors only");
    verbosity->addItem("Normal");
    verbosity->addItem("Verbose");
    verbosity->setCurrentIndex(1);
    connect(verbosity, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int i)
        {
            min_level = (i == 0) ? 4 : (i == 1) ? 2 : 1;
            rerender();
        });

    connect(clear, &QPushButton::clicked, this, [this]()
    {
        entries.clear();
        output->clear();
    });
    connect(copy, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(output->toPlainText());
    });
    connect(lock, &QPushButton::clicked, this, [this, lock]()
    {
        autoscroll = !autoscroll;
        lock->setText(autoscroll ? "Autoscroll: On" : "Autoscroll: Off");
        if (autoscroll)
            output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
    });

    auto* bar = new QHBoxLayout;
    bar->addWidget(clear);
    bar->addWidget(copy);
    bar->addWidget(lock);
    bar->addStretch();
    bar->addWidget(verbosity);

    auto* root = new QVBoxLayout(this);
    root->addLayout(bar);
    root->addWidget(output);
}

void LauncherLog::append_line(int level, const QString& text)
{
    QString safeText = text;
    const QString token = util::config::Config::instance().token();
    if (!token.isEmpty())
        safeText.replace(token, QStringLiteral("[REDACTED]"));

    entries.push_back({ level, safeText });
    constexpr qsizetype k_max_entries = 5000;
    if (entries.size() > k_max_entries)
        entries.remove(0, entries.size() - k_max_entries);

    if (passes(level))
    {
        output->appendPlainText(safeText);
        if (autoscroll)
            output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
    }

    constexpr int k_error_level = 4; // spdlog::level::err
    if (level >= k_error_level)
    {
        showNormal();
        raise();
        activateWindow();
    }
}

void LauncherLog::rerender()
{
    output->clear();
    for (const Entry& e : entries)
        if (passes(e.level)) output->appendPlainText(e.text);
    if (autoscroll)
        output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
}