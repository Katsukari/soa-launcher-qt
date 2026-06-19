#pragma once
#include <QRect>
#include <QSize>
#include <QPoint>
#include <QtGlobal>

namespace util::layout
{
    // Preset window sizes
    namespace win
    {
        inline constexpr QSize k_small   {1120, 677};
        inline constexpr QSize k_default {1400, 846};
        inline constexpr QSize k_large   {1600, 967};
        inline constexpr QSize k_4k      {1920, 1160};
    }

    // Scale primitives (uniform, width-based)
    double scale(QSize win);
    int    scaled(int v, QSize win);
    QSize  scaled(QSize s, QSize win);
    QPoint scaled(QPoint p, QSize win);
    QRect  scaled(QRect r, QSize win);

    // Frame insets (constant px in DEFAULT space)
    namespace region
    {
        inline constexpr int k_left   = 30;
        inline constexpr int k_top    = 22;
        inline constexpr int k_right  = 20;
        inline constexpr int k_bottom = 30;
        inline constexpr int k_radius = 35;

        inline constexpr QRect k_default
        {
            k_left,
            k_top,
            win::k_default.width()  - k_left - k_right,
            win::k_default.height() - k_top  - k_bottom
        };

        QRect rect(QSize win);   // scaled content region
    }

    // Authoring helpers (constexpr - operate in DEFAULT space)
    constexpr QRect center_in_region(const QSize box, const int dx = 0, const int dy = 0)
    {
        return
        {
            region::k_default.x() + (region::k_default.width()  - box.width())  / 2 + dx,
            region::k_default.y() + (region::k_default.height() - box.height()) / 2 + dy,
            box.width(), box.height()
        };
    }

    constexpr QRect hcenter_in_region(const QSize box, const int y)
    {
        return
        {
            region::k_default.x() + (region::k_default.width() - box.width()) / 2,
            y,
            box.width(), box.height()
        };
    }

    constexpr QRect anchor_top_right(const QRect parent, const int from_right, const int from_top, const QSize sz)
    {
        return
        {
            parent.right() + 1 - from_right - sz.width(),
            parent.top() + from_top,
            sz.width(), sz.height()
        };
    }

    constexpr QRect anchor_bottom_right(const QRect parent, const int from_right, const int from_bottom, const QSize sz)
    {
        return
        {
            parent.right()  + 1 - from_right  - sz.width(),
            parent.bottom() + 1 - from_bottom - sz.height(),
            sz.width(), sz.height()
        };
    }

    QRect centered(QSize box, QSize win, int dx = 0, int dy = 0);

    // Recurring font sizes (default-space px; scale with scaled(px, win))
    namespace text
    {
        inline constexpr int k_modal_header = 25;
        inline constexpr int k_row_title    = 20;
        inline constexpr int k_banner       = 18;
        inline constexpr int k_label        = 15;
        inline constexpr int k_desc         = 14;
        inline constexpr int k_body         = 13;
        inline constexpr int k_version      = 16;
        inline constexpr int k_status       = 12;
    }

    // MainWindow chrome
    namespace chrome
    {
        inline constexpr int   k_logo_width = 390;
        inline constexpr int   k_logo_top   = 62;

        inline constexpr QRect k_close         {1315, 40, 40, 40};
        inline constexpr QSize k_close_icon    {35, 35};
        inline constexpr QRect k_minimize      {1270, 37, 50, 50};
        inline constexpr QSize k_minimize_icon {50, 50};

        inline constexpr QPoint k_pt_icon   {56, 495};
        inline constexpr QPoint k_lock_icon {58, 613};

        inline constexpr QRect k_version =
            anchor_bottom_right(region::k_default, 35, 33, {245, 20});

        int    logo_width(QSize win);
        int    logo_top(QSize win);
        QPoint logo_pos(QSize win);

        QRect  close(QSize win);
        QSize  close_icon(QSize win);
        QRect  minimize(QSize win);
        QSize  minimize_icon(QSize win);

        QPoint pt_icon(QSize win);
        QPoint lock_icon(QSize win);

        QRect  version(QSize win);
    }

    // DownloadWidget (central card)
    namespace download
    {
        inline constexpr QSize k_box        {620, 320};
        inline constexpr QSize k_box_auth   {620, 360};
        inline constexpr int   k_top_offset = 350;

        inline constexpr int k_button_x = 90;
        inline constexpr int k_button_y = 198;
        inline constexpr int k_button_w = 440;

        inline constexpr QRect k_rect      = hcenter_in_region(k_box, k_top_offset);
        inline constexpr QRect k_rect_auth = hcenter_in_region(k_box_auth, k_top_offset);

        inline constexpr QRect k_title  {0, 38, k_box.width(), 30};
        inline constexpr QRect k_note   {90, 103, 440, 70};
        inline constexpr QRect k_settings_button =
            anchor_top_right({0, 0, k_box.width(), k_box.height()}, 25, 23, {37, 37});

        inline constexpr QRect k_signed_in_banner {95, 0, 430, 48};
        inline constexpr QSize k_disclaimer       {430, 94};

        QRect  rect(QSize win);
        QRect  rect_auth(QSize win);
        QPoint pos(QSize win);
        QPoint pos_auth(QSize win);
        QSize  box(QSize win);
        QSize  box_auth(QSize win);
        QRect  title(QSize win);
        QRect  note(QSize win);
        QRect  settings_button(QSize win);
    }

    // SettingsWidget (full-window overlay + centered box)
    // Both tabs (Launcher + Wine) use the SAME box. The wine terminal is a
    // separate top-level window
    namespace settings
    {
        inline constexpr QSize k_box {630, 555};
        inline constexpr QSize k_box_results {630, 650};
        inline constexpr QSize k_close_icon {13, 13};
        inline constexpr QSize k_close_hit {22, 22};
        inline constexpr int   k_margin_top = 69;
        inline constexpr int   k_header_gap = 60;
        inline constexpr int   k_row_gap = 50;
        inline constexpr int   k_padding = 37;

        inline constexpr QSize k_tab        {150, 44};
        inline constexpr int   k_tab_gap    = 6;
        inline constexpr int   k_tab_inset  = 40;
        inline constexpr int   k_tab_overlap = 4;

        inline constexpr QRect k_rect  = center_in_region(k_box, 0, k_margin_top);
        inline constexpr QRect k_close = anchor_top_right(k_rect, 27, 22, k_close_hit);

        inline constexpr QRect k_page_title {0, 34, k_box.width(), 30};
        inline constexpr int   k_control_col = 227;
        inline constexpr QSize k_slider      {69, 34};
        inline constexpr int   k_slider_gap  = 10;
        inline constexpr int   k_desc_max_w  = 270;

        // Left text column (title + description), right control column.
        inline constexpr int   k_text_x    = 37;
        inline constexpr int   k_text_w    = 290;
        inline constexpr int   k_ctrl_x    = 366;
        inline constexpr int   k_ctrl_w    = 227;
        inline constexpr int   k_desc_dy   = 30;
        inline constexpr int   k_desc_h    = 40;
        inline constexpr int   k_title_h   = 26;

        inline constexpr int   k_row1_y = 78;
        inline constexpr int   k_row2_y = 182;
        inline constexpr int   k_row3_y = 286;
        inline constexpr int   k_row4_y = 390;

        QRect box_rect(QSize win);
        QSize box(QSize win);
        QRect close(QSize win);
        QSize close_icon(QSize win);
        int   header_gap(QSize win);
        int   row_gap(QSize win);
        int   control_col(QSize win);
        QSize slider(QSize win);
        int   desc_max_w(QSize win);

        QSize tab(QSize win);
        int   tab_gap(QSize win);
        int   tab_inset(QSize win);
        int   tab_overlap(QSize win);
        QRect tab_rect(QSize win, int i);

        QRect row_title(QSize win, int y);
        QRect row_desc(QSize win, int y);
        int   ctrl_x(QSize win);
        int   ctrl_w(QSize win);
        QPoint ctrl_pos(QSize win, int y);
        QRect run_check(QSize win, int y);
        QRect slider_rect(QSize win, int y);
        QRect page_title(QSize win);
    }

    // CustomSelect dropdown
    namespace select
    {
        inline constexpr QSize k_box            {227, 64};
        inline constexpr int   k_option_h       = 64;
        inline constexpr int   k_option_overlap = 21;
        inline constexpr int   k_pad_bottom     = 10;

        QSize box(QSize win);
        int   option_h(QSize win);
        int   option_overlap(QSize win);
    }

    // GameInstall modal overlay
    namespace install_modal
    {
        // Box dimensions & placement
        inline constexpr QSize k_box        {580, 382};
        inline constexpr int   k_margin_top = 66;  // dy offset from vertical center

        // Box-local positioning
        inline constexpr int   k_title_y      = 40;
        inline constexpr int   k_title_h      = 30;
        inline constexpr int   k_body_y       = 100;  // title_y + title_h + 30
        inline constexpr int   k_body_h       = 40;
        inline constexpr int   k_text_x       = 33;
        inline constexpr int   k_path_inset   = 33;
        inline constexpr int   k_path_h       = 69;
        inline constexpr int   k_path_y       = 150;  // body_y + body_h + 30
        inline constexpr int   k_changepath_y = 234;   // path_y + path_h + 15
        inline constexpr int   k_changepath_h = 20;
        inline constexpr int   k_button_row_y = 287;   // changepath_y + changepath_h + 40
        inline constexpr int   k_button_h     = 40;
        inline constexpr int   k_bottom_pad   = 55;
        inline constexpr int   k_button_outer = 70;
        inline constexpr int   k_button_gap   = 35;

        // Computed layout
        inline constexpr QRect k_rect  = center_in_region(k_box, 0, k_margin_top);
        inline constexpr QSize k_close_icon {13, 13};
        inline constexpr QSize k_close_hit  {22, 22};
        inline constexpr QRect k_close = anchor_top_right({0, 0, k_box.width(), k_box.height()}, 19, 17, k_close_hit);

        inline constexpr QRect k_title {0, k_title_y, k_box.width(), k_title_h};
        inline constexpr QRect k_body  {k_text_x, k_body_y, k_box.width() - 2 * k_text_x, k_body_h};
        inline constexpr QRect k_path  {k_path_inset, k_path_y, k_box.width() - 2 * k_path_inset, k_path_h};
        inline constexpr QRect k_changepath {k_path_inset, k_changepath_y, k_box.width() - 2 * k_path_inset, k_changepath_h};

        inline constexpr int   k_btn_w   = (k_box.width() - 2 * k_button_outer - k_button_gap) / 2;
        inline constexpr QRect k_cancel  {k_button_outer, k_button_row_y, k_btn_w, k_button_h};
        inline constexpr QRect k_install {k_button_outer + k_btn_w + k_button_gap, k_button_row_y,
                                          k_box.width() - k_button_outer - (k_button_outer + k_btn_w + k_button_gap), k_button_h};

        // Accessor functions
        QRect  box_rect(QSize win);
        QSize  box(QSize win);
        QRect  rect(QSize win);
        QRect  close(QSize win);
        QRect  title(QSize win);
        QRect  body(QSize win);
        QRect  path_field(QSize win);
        QRect  changepath_line(QSize win);
        QRect  cancel_button(QSize win);
        QRect  install_button(QSize win);
        QSize  close_icon(QSize win);
    }
}