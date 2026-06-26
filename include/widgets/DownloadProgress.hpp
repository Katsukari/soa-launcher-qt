#pragma once

#include "util/ModalOverlay.hpp"

class DownloadProgress : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT
    public:
        explicit DownloadProgress(QWidget* parent = nullptr);

    protected:
        void paint_content(QPainter& painter) override;
};