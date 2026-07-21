#pragma once

#include <QWidget>
#include <QStringList>

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

class ImageDropdown : public QWidget
{
    Q_OBJECT

public:
    explicit ImageDropdown(QStringList options, QWidget* parent = nullptr);

    void set_index(int i);
    int index() const { return current; }

signals:
    void changed(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void set_open(bool value);
    void select_relative(int delta);
    QRect option_rect(int slot) const;

    QStringList items;
    int current {};
    bool open {};
};
