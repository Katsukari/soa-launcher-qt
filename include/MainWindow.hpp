#pragma once
#include <QMainWindow>

class ModalOverlay;
class QPushButton;
class QLabel;
class DownloadBox;
class Settings;
class GameInstall;

class MainWindow : public QWidget
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent * event) override;

    private:
        void setup_window_buttons();
        void setup_logo();
        void setup_version_label();
        void setup_settings();
        void setup_download_box();
        void setup_game_install();
        void on_overlay_opened(ModalOverlay* m);
        void on_overlay_closed(ModalOverlay *);

        QPoint drag_offset;
        bool chrome_hidden {};
        QPushButton * close_button = nullptr;
        QPushButton * minimize_button = nullptr;
        DownloadBox * download_box = nullptr;
        Settings * settings = nullptr;
        GameInstall * game_install = nullptr;
};