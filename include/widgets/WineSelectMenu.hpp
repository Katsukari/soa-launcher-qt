#pragma once
#include <QVector>
#include "util/ModalOverlay.hpp"
#include "core/wine/WineRegistry.hpp"

class QScrollArea;
class QLabel;
class QPushButton;

class WineSelectMenu : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
        explicit WineSelectMenu(QWidget* parent = nullptr);
        signals:
            void runtime_chosen();
    protected:
        void paint_content(QPainter& painter) override;
    private:
        void build_ui();
        void populate();
        void rescan();
        void relayout();
        void select_row(int index);
        void confirm();
        QVector<core::wine::WineInstall> runtimes;
        int                              selected { -1 };
        QLabel*               tricks_status {};
        QScrollArea*          list {};
        QVector<QPushButton*> rows;
        QPushButton*          close_button {};
        QPushButton*          rescan_button {};
        QPushButton*          continue_button {};
};