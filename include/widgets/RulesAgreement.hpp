#pragma once

#include <QEvent>
#include "util/ModalOverlay.hpp"

class QCheckBox;
class QShowEvent;
class QPushButton;
class QTextBrowser;

class RulesAgreement : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT

    public:
        explicit RulesAgreement(QWidget* parent = nullptr);

    signals:
        void accepted();

    protected:
        void paint_content(QPainter& painter) override;
        void showEvent(QShowEvent* event) override;
        bool eventFilter(QObject* object, QEvent* event) override;

    private:
        void setup_controls();
        void retranslate_content();
        void update_agree_button();

        QTextBrowser* rules_text {};
        QCheckBox* accepted_box {};
        QPushButton* agree_button {};
};
