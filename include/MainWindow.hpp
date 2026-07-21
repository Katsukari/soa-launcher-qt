#pragma once

#include <QWidget>

class QCloseEvent;

#include "core/game/GameVersion.hpp"
#include "core/state/Stage.hpp"
#include "core/state/View.hpp"

namespace util::modal_overlay
{
    class ModalOverlay;
}
namespace core::wine
{
    class Shell;
}
namespace core::state
{
    class InstallState;
}

class QPushButton;
class AliciaChooser;
class PrerequisitesIntro;
class RulesAgreement;
class RepairFiles;
class Settings;
class GameInstall;
class DownloadProgress;
class WineSelectMenu;
class WineInstall;
class AuthHandler;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    [[nodiscard]] AuthHandler* auth_handler() const
    {
        return auth;
    }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setup_window_buttons();
    void setup_version_label();
    void setup_settings();
    void setup_prerequisites();
    void setup_rules();
    void setup_repair_files();
    void setup_alicia_chooser();
    void setup_game_selector();
    void setup_wine_install();
    void setup_game_install();
    void setup_wine_select();
    void set_game_version(core::game::GameVersion version);
    void refresh_game_selector();
    void set_game_switching_enabled(core::state::Stage stage);
    void on_overlay_opened(util::modal_overlay::ModalOverlay* overlay);
    void on_overlay_closed(util::modal_overlay::ModalOverlay* overlay);
    void update_chrome_visibility();
    void open_overlay(util::modal_overlay::ModalOverlay* overlay);
    void close_overlay(util::modal_overlay::ModalOverlay* overlay);
    void on_stage_changed(core::state::Stage stage);
    void open_for_current_stage();

    bool chrome_hidden {};
    bool repair_active {};
    bool minimized_for_game {};
    QPushButton* close_button {};
    QPushButton* minimize_button {};
    QPushButton* playtest_button {};
    QPushButton* alicia_2_button {};
    AliciaChooser* alicia_chooser {};
    PrerequisitesIntro* prerequisites_intro {};
    RulesAgreement* rules_agreement {};
    RepairFiles* repair_files {};
    Settings* settings {};
    WineInstall* wine_install {};
    GameInstall* game_install {};
    DownloadProgress* repair_progress {};
    WineSelectMenu* wine_select {};
    core::wine::Shell* shell {};
    AuthHandler* auth {};
    core::state::InstallState* install_state {};
    core::game::GameVersion game_version {core::game::GameVersion::Playtest};
    core::state::View last_view {core::state::View::Loading};
};
