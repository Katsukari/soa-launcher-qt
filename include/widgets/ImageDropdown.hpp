#pragma once

#include <QWidget>
#include <QStringList>

class ImageDropdown : public QWidget
{
    Q_OBJECT
    public:
        explicit ImageDropdown(QStringList options, QWidget* parent = nullptr);

        void set_index(int i);
        int  index() const { return current; }

        signals:
            void changed(int index);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:
        QStringList items;
        int  current {};
        bool open {};

        QRect option_rect(int slot) const;
};