#include "widgets/WineTerminal.hpp"
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QScrollBar>

WineTerminal::WineTerminal(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Wine Log");
    resize(640, 400);

    output = new QPlainTextEdit(this);
    output->setReadOnly(true);
    output->setStyleSheet(
        "QPlainTextEdit { background:#1E1B17; color:#D8C9B8;"
        " font-family:'monospace'; font-size:12px; border:none; }");
    output->setPlainText("> wine log ready.");

    auto* clear = new QPushButton("Clear", this);
    auto* copy  = new QPushButton("Copy", this);
    auto* lock  = new QPushButton("Autoscroll: On", this);

    connect(clear, &QPushButton::clicked, this, [this]() { output->clear(); });
    connect(copy,  &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(output->toPlainText());
    });
    connect(lock, &QPushButton::clicked, this, [this, lock]()
    {
        autoscroll = !autoscroll;
        lock->setText(autoscroll ? "Autoscroll: On" : "Autoscroll: Off");
        if (autoscroll) output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
    });

    auto* bar = new QHBoxLayout;
    bar->addWidget(clear);
    bar->addWidget(copy);
    bar->addWidget(lock);
    bar->addStretch();

    auto* root = new QVBoxLayout(this);
    root->addLayout(bar);
    root->addWidget(output);
}

void WineTerminal::append_line(const QString& text)
{
    output->appendPlainText(text);
    if (autoscroll) output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
}