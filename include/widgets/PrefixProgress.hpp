#pragma once

#include "util/ModalOverlay.hpp"
#include <QString>

class QTimer;
class QPushButton;

namespace core::wine
{
    class Shell;
}

// PrefixProgress: shown on top of WineInstall while the wine prefix is set up.
// Mirrors DownloadProgress visually. Wine setup has no real % (wineboot/winetricks
// don't report it), so the bar fills by STEP - 33% wineboot, 66% winetricks, 100%
// done - easing smoothly toward each target. Driven by Shell's setup_status signal.
class PrefixProgress : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
    explicit PrefixProgress(core::wine::Shell* shell, QWidget* parent = nullptr);

protected:
    void paint_content(QPainter& painter) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    signals:
        void prefix_complete();   // emitted after the bar fills to 100% and setup succeeded

private:
    void setup_buttons();
    void draw_fill(QPainter& painter, const QRect& track) const;

    core::wine::Shell* shell {};

    QString status { "Starting..." };
    int     step   {0};      // 0 none, 1 wineboot, 2 winetricks
    bool    done   {};
    bool    failed {};
    bool    emitted {};

    QTimer* anim {};
    double  current_pct {0.0};
    double  target_pct  {0.0};

    QPushButton* close_button {};
    QPushButton* log_button {};
};