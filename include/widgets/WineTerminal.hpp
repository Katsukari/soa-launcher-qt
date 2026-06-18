#pragma once
#include <QDialog>

class QPlainTextEdit;

class WineTerminal : public QDialog
{
    Q_OBJECT
    public:
        explicit WineTerminal(QWidget* parent = nullptr);
        void append_line(const QString& text);

    private:
        QPlainTextEdit* output {};
        bool autoscroll {true};
};