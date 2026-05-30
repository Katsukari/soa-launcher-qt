#include "util/Layout.hpp"

namespace layout
{
    double scale(const QSize win)
    {
        return static_cast<double>(win.width()) / win::k_default.width();
    }

    int scaled(const int v, const QSize win)
    {
        return qRound(v * scale(win));
    }

    QSize scaled(const QSize s, const QSize win)
    {
        const double f = scale(win);
        return { qRound(s.width() * f), qRound(s.height() * f) };
    }

    QPoint scaled(const QPoint p, const QSize win)
    {
        const double f = scale(win);
        return { qRound(p.x() * f), qRound(p.y() * f) };
    }

    QRect scaled(const QRect r, const QSize win)
    {
        const double f = scale(win);
        return
        {
            qRound(r.x() * f),
            qRound(r.y() * f),
            qRound(r.width()  * f),
            qRound(r.height() * f)
        };
    }

    QRect centered(const QSize box, const QSize win, const int dx, const int dy)
    {
        return scaled(center_in_region(box, dx, dy), win);
    }

    namespace region
    {
        QRect rect(const QSize win) { return scaled(k_default, win); }
    }

    namespace chrome
    {
        int logo_width(const QSize win) { return scaled(k_logo_width, win); }
        int logo_top(const QSize win)   { return scaled(k_logo_top, win); }

        QPoint logo_pos(const QSize win)
        {
            const QRect r = region::rect(win);
            return { r.x() + (r.width() - logo_width(win)) / 2, logo_top(win) };
        }

        QRect close(const QSize win)         { return scaled(k_close, win); }
        QSize close_icon(const QSize win)    { return scaled(k_close_icon, win); }
        QRect minimize(const QSize win)      { return scaled(k_minimize, win); }
        QSize minimize_icon(const QSize win) { return scaled(k_minimize_icon, win); }

        QPoint pt_icon(const QSize win)   { return scaled(k_pt_icon, win); }
        QPoint lock_icon(const QSize win) { return scaled(k_lock_icon, win); }

        QRect version(const QSize win) { return scaled(k_version, win); }
    }

    namespace download
    {
        QRect  rect(const QSize win)            { return scaled(k_rect, win); }
        QRect  rect_auth(const QSize win)       { return scaled(k_rect_auth, win); }
        QPoint pos(const QSize win)             { return rect(win).topLeft(); }
        QPoint pos_auth(const QSize win)        { return rect_auth(win).topLeft(); }
        QSize  box(const QSize win)             { return scaled(k_box, win); }
        QSize  box_auth(const QSize win)        { return scaled(k_box_auth, win); }
        QRect  title(const QSize win)           { return scaled(k_title, win); }
        QRect  note(const QSize win)            { return scaled(k_note, win); }
        QRect  settings_button(const QSize win) { return scaled(k_settings_button, win); }
    }

    namespace settings
    {
        QRect box_rect(const QSize win)   { return scaled(k_rect, win); }
        QSize box(const QSize win)        { return scaled(k_box, win); }
        QRect close(const QSize win)      { return scaled(k_close, win); }
        QSize close_icon(const QSize win) { return scaled(k_close_icon, win); }
        int   header_gap(const QSize win) { return scaled(k_header_gap, win); }
        int   row_gap(const QSize win)    { return scaled(k_row_gap, win); }
        int   control_col(const QSize win){ return scaled(k_control_col, win); }
        QSize slider(const QSize win)     { return scaled(k_slider, win); }
        int   desc_max_w(const QSize win) { return scaled(k_desc_max_w, win); }

        QSize tab(const QSize win)        { return scaled(k_tab, win); }
        int   tab_gap(const QSize win)    { return scaled(k_tab_gap, win); }
        int   tab_inset(const QSize win)  { return scaled(k_tab_inset, win); }
        int   tab_overlap(const QSize win){ return scaled(k_tab_overlap, win); }

        QRect page_title(QSize win) { return scaled(k_page_title, win); }

        QRect tab_rect(const QSize win, int i)
        {
            const QRect box = box_rect(win);
            const QSize t   = tab(win);
            const int   x   = box.left() + tab_inset(win) + i * (t.width() + tab_gap(win));
            const int   y   = box.top() - t.height() + tab_overlap(win);
            return { x, y, t.width(), t.height() };
        }

        QRect row_title(const QSize win, const int y)
        {
            return scaled(QRect{ k_text_x, y, k_text_w, k_title_h }, win);
        }

        QRect row_desc(const QSize win, const int y)
        {
            return scaled(QRect{ k_text_x, y + k_desc_dy, k_text_w, k_desc_h }, win);
        }

        int ctrl_x(const QSize win) { return scaled(k_ctrl_x, win); }
        int ctrl_w(const QSize win) { return scaled(k_ctrl_w, win); }

        QPoint ctrl_pos(const QSize win, const int y)
        {
            return scaled(QPoint{ k_ctrl_x, y }, win);
        }

        QRect run_check(const QSize win, const int y)
        {
            return scaled(QRect{ k_ctrl_x, y, k_ctrl_w, 48 }, win);
        }

        QRect slider_rect(const QSize win, const int y)
        {
            constexpr int x = k_ctrl_x + k_ctrl_w - k_slider.width();
            return scaled(QRect{ x, y, k_slider.width(), k_slider.height() }, win);
        }
    }

    namespace select
    {
        QSize box(const QSize win)            { return scaled(k_box, win); }
        int   option_h(const QSize win)       { return scaled(k_option_h, win); }
        int   option_overlap(const QSize win) { return scaled(k_option_overlap, win); }
    }

    namespace install_modal
    {
        QRect box_rect(const QSize win)   { return scaled(k_rect, win); }
        QSize box(const QSize win)        { return scaled(k_box, win); }
        QRect close(const QSize win)      { return scaled(k_close, win); }
        QSize close_icon(const QSize win) { return scaled(k_close_icon, win); }
        int   path_h(const QSize win)     { return scaled(k_path_h, win); }
    }

    namespace progress_modal
    {
        QRect box_rect(const QSize win)   { return scaled(k_rect, win); }
        QSize box(const QSize win)        { return scaled(k_box, win); }
        QRect close(const QSize win)      { return scaled(k_close, win); }
        QSize close_icon(const QSize win) { return scaled(k_close_icon, win); }
    }
}