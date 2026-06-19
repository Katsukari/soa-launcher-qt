#pragma once
#include <QWidget>

// I know having the namespace also named modal_overlay is duplication,
// but I want to extend this class later so it wont be a simple class.
namespace util::modal_overlay
{
    class ModalOverlay : public QWidget
    {
        Q_OBJECT
        public:
        explicit ModalOverlay(QWidget * parent = nullptr);

        void show_over(QWidget* background);   // grab -> blur -> show -> raise
        [[nodiscard]] bool keeps_chrome() const { return keep_chrome; }

        signals:
            void closed();

    protected:
        // Subclass draws its box + contents here. Called AFTER the blurred backdrop
        // (and frames, if kept) are already painted.
        virtual void paint_content(QPainter& painter) = 0;
        void set_keeps_chrome(const bool v) { keep_chrome = v; }
        void paintEvent(QPaintEvent* event) override;   // paints backdrop, then calls paint_content

    private:
        void paint_frames(QPainter& painter) const;   // left/right frame + PT/lock, sharp

        QPixmap blurred_bg;
        bool    keep_chrome {true};   // default = modal over live launcher (keeps frames/icons/chrome)
    };
}