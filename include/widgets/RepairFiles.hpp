#pragma once

#include <QEvent>
#include <QString>
#include "core/game/GameVersion.hpp"
#include "util/ModalOverlay.hpp"

class QPushButton;

class RepairFiles : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT

public:
    explicit RepairFiles(QWidget* parent = nullptr);
    void refresh();

signals:
    void repair_requested();
    void closed();

protected:
    void paint_content(QPainter& painter) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void setup_buttons();

    core::game::GameVersion game_version {core::game::GameVersion::Playtest};
    QString install_path;
    QPushButton* close_button {};
    QPushButton* cancel_button {};
    QPushButton* repair_button {};
};
