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
#include "util/LanguageManager.hpp"

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
    resize(680, 420);

    output = new QPlainTextEdit(this);
    output->setReadOnly(true);
    output->document()->setMaximumBlockCount(5000);
    output->setStyleSheet(
        "QPlainTextEdit { background:#1E1B17; color:#D8C9B8;"
        " font-family:'monospace'; font-size:12px; border:none; }");

    clear_button = new QPushButton(this);
    copy_button = new QPushButton(this);
    autoscroll_button = new QPushButton(this);

    verbosity = new QComboBox(this);
    verbosity->addItems({QString(), QString(), QString()});
    verbosity->setCurrentIndex(1);
    connect(verbosity, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int i)
        {
            min_level = (i == 0) ? 4 : (i == 1) ? 2 : 1;
            rerender();
        });

    connect(clear_button, &QPushButton::clicked, this, [this]()
    {
        entries.clear();
        output->clear();
    });
    connect(copy_button, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(output->toPlainText());
    });
    connect(autoscroll_button, &QPushButton::clicked, this, [this]()
    {
        autoscroll = !autoscroll;
        retranslate();
        if (autoscroll)
            output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
    });

    connect(&util::i18n::LanguageManager::instance(),
            &util::i18n::LanguageManager::language_changed, this, [this]()
    {
        retranslate();
    });

    auto* bar = new QHBoxLayout;
    bar->addWidget(clear_button);
    bar->addWidget(copy_button);
    bar->addWidget(autoscroll_button);
    bar->addStretch();
    bar->addWidget(verbosity);

    auto* root = new QVBoxLayout(this);
    root->addLayout(bar);
    root->addWidget(output);
    retranslate();
}

void LauncherLog::retranslate()
{
    using util::i18n::translate;
    setWindowTitle(translate("Launcher Log"));
    clear_button->setText(translate("Clear"));
    copy_button->setText(translate("Copy"));
    autoscroll_button->setText(translate(
        autoscroll ? "Autoscroll: On" : "Autoscroll: Off"));
    const int selected = verbosity->currentIndex();
    verbosity->blockSignals(true);
    verbosity->setItemText(0, translate("Errors only"));
    verbosity->setItemText(1, translate("Normal"));
    verbosity->setItemText(2, translate("Verbose"));
    verbosity->setCurrentIndex(selected);
    verbosity->blockSignals(false);
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

    constexpr int k_error_level = 4;
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
