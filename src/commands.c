/* Copyright 2011-2020 Bert Muennich
 * Copyright 2021-2023 nsxiv contributors
 *
 * This file is a part of nsxiv.
 *
 * nsxiv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * nsxiv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with nsxiv.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "commands.h"

#include "cli_options.h"
#include "image.h"
#include "nsxiv.h"
#include "thumbs.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


extern SxivImage g_img;
extern ThumbnailState g_tns;
extern win_t g_win;
extern opt_t *g_options;


static bool navigate_to(CommandArg n)
{
    if (n < 0 || n >= g_filecnt || n == g_fileidx)
        return false;

    switch (g_mode) {
        case MODE_ALL: error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");
        case MODE_IMAGE: load_image(n); break;
        case MODE_THUMB: g_fileidx = n; g_tns.dirty = true; break;
    }

    return true;
}


bool cg_quit(CommandArg status)
{
    if (g_options->to_stdout && g_markcnt > 0) {
        for (size_t i = 0; i < (size_t)g_filecnt; i++) {
            if (g_files[i].flags & FF_MARK)
                printf("%s%c", g_files[i].name, g_options->using_null ? '\0' : '\n');
        }
    }
    exit(status);
}


bool cg_pick_quit(CommandArg status)
{
    if (g_options->to_stdout && g_markcnt == 0)
        printf("%s%c", g_files[g_fileidx].name, g_options->using_null ? '\0' : '\n');
    cg_quit(status);
}


bool cg_switch_mode(CommandArg _)
{
    switch (g_mode) {
    case MODE_ALL:
        error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");

    case MODE_IMAGE: 
        if (g_tns.thumbs == NULL)
            tns_init(&g_tns, g_files, &g_filecnt, &g_fileidx, &g_win);
        img_close(&g_img, false);
        reset_timeout(reset_cursor);
        if (g_img.slideshow_settings.is_enabled) {
            g_img.slideshow_settings.is_enabled = false;
            reset_timeout(slideshow);
        }
        g_tns.dirty = true;
        g_mode = MODE_THUMB;
        break;

    case MODE_THUMB:
        load_image(g_fileidx);
        g_mode = MODE_IMAGE;
        break;
    }

    return true;
}


bool cg_toggle_fullscreen(CommandArg _)
{
    win_toggle_fullscreen(&g_win);
    /* redraw after next ConfigureNotify event */
    set_timeout(redraw, TO_REDRAW_RESIZE, false);

    switch (g_mode) {
        case MODE_ALL: error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");
        case MODE_IMAGE: g_img.flags |= IF_CHECKPAN | IF_IS_DIRTY; break;
        case MODE_THUMB: g_tns.dirty = true; break;
    }
        
    return false;
}


bool cg_toggle_bar(CommandArg _)
{
    win_toggle_bar(&g_win);

    switch (g_mode) {
        case MODE_ALL: error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");
        case MODE_IMAGE: g_img.flags |= IF_CHECKPAN | IF_IS_DIRTY; break;
        case MODE_THUMB: g_tns.dirty = true; break;
    }
        
    if (g_win.bar.h > 0)
        open_info();
    else
        close_info();

    return true;
}


bool cg_prefix_external(CommandArg _)
{
    handle_key_handler(true);
    return false;
}


bool cg_reload_image(CommandArg _)
{
    switch (g_mode) {
        case MODE_ALL:
            error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");

        case MODE_IMAGE:
            load_image(g_fileidx);
            break;

        case MODE_THUMB:
            win_set_cursor(&g_win, CURSOR_WATCH);
            if (!tns_load(&g_tns, g_fileidx, true, false)) {
                remove_file(g_fileidx, false);
                g_tns.dirty = true;
            }
            break;
    }

    return true;
}


bool cg_remove_image(CommandArg _)
{
    remove_file(g_fileidx, true);

    switch (g_mode) {
        case MODE_ALL: error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");
        case MODE_IMAGE: load_image(g_fileidx); break;
        case MODE_THUMB: g_tns.dirty = true; break;
    }

    return true;
}


bool cg_first(CommandArg _)
{
    return navigate_to(0);
}


bool cg_n_or_last(CommandArg _)
{
    int n = g_prefix != 0 && g_prefix - 1 < g_filecnt ? g_prefix - 1 : g_filecnt - 1;
    return navigate_to(n);
}


bool cg_scroll_screen(CommandArg dir)
{
    switch (g_mode) {
        case MODE_ALL: error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");
        case MODE_IMAGE: return img_pan(&g_img, dir, -1);
        case MODE_THUMB: return tns_scroll(&g_tns, dir, true);
    }
    return None;
}


bool cg_zoom(CommandArg d)
{
    switch (g_mode) {
        case MODE_ALL: error_quit(EXIT_FAILURE, 0, "unexpected mode 'ALL'");
        case MODE_IMAGE: return img_zoom(&g_img, d);
        case MODE_THUMB: return tns_zoom(&g_tns, d);
    }
    return None;
}


bool cg_toggle_image_mark(CommandArg _)
{
    return mark_image(g_fileidx, !(g_files[g_fileidx].flags & FF_MARK));
}


bool cg_reverse_marks(CommandArg _)
{
    int i;

    for (i = 0; i < g_filecnt; i++) {
        g_files[i].flags ^= FF_MARK;
        g_markcnt += g_files[i].flags & FF_MARK ? 1 : -1;
    }
    if (g_mode == MODE_THUMB)
        g_tns.dirty = true;
    return true;
}


bool cg_mark_range(CommandArg _)
{
    int d = g_markidx < g_fileidx ? 1 : -1, end, i;
    bool dirty = false, on = !!(g_files[g_markidx].flags & FF_MARK);

    for (i = g_markidx + d, end = g_fileidx + d; i != end; i += d)
        dirty |= mark_image(i, on);
    return dirty;
}


bool cg_unmark_all(CommandArg _)
{
    int i;

    for (i = 0; i < g_filecnt; i++)
        g_files[i].flags &= ~FF_MARK;
    g_markcnt = 0;
    if (g_mode == MODE_THUMB)
        g_tns.dirty = true;
    return true;
}


bool cg_navigate_marked(CommandArg n)
{
    int d, i;
    int new = g_fileidx;

    if (g_prefix > 0)
        n *= g_prefix;
    d = n > 0 ? 1 : -1;
    for (i = g_fileidx + d; n != 0 && i >= 0 && i < g_filecnt; i += d) {
        if (g_files[i].flags & FF_MARK) {
            n -= d;
            new = i;
        }
    }
    return navigate_to(new);
}

static bool change_color_modifier(CommandArg d, int *target)
{
    if (!img_change_color_modifier(&g_img, d * (g_prefix > 0 ? g_prefix : 1), target))
        return false;
    if (g_mode == MODE_THUMB)
        g_tns.dirty = true;
    return true;
}


bool cg_change_gamma(CommandArg d)
{
    return change_color_modifier(d, &g_img.gamma);
}


bool cg_change_brightness(CommandArg d)
{
    return change_color_modifier(d, &g_img.brightness);
}


bool cg_change_contrast(CommandArg d)
{
    return change_color_modifier(d, &g_img.contrast);
}


bool ci_navigate(CommandArg n)
{
    if (g_prefix > 0)
        n *= g_prefix;
    n += g_fileidx;
    n = MAX(0, MIN(n, g_filecnt - 1));

    if (n != g_fileidx) {
        load_image(n);
        return true;
    }

    return false;
}


bool ci_cursor_navigate(CommandArg _)
{
    return ci_navigate(nav_button() - 1);
}


bool ci_alternate(CommandArg _)
{
    load_image(g_alternate);
    return true;
}


bool ci_navigate_frame(CommandArg d)
{
    if (g_prefix > 0)
        d *= g_prefix;
    return !g_img.multi.animate && img_frame_navigate(&g_img, d);
}


bool ci_toggle_animation(CommandArg _)
{
    bool dirty = false;

    if (g_img.multi.cnt > 0) {
        g_img.multi.animate = !g_img.multi.animate;
        if (g_img.multi.animate) {
            dirty = img_frame_animate(&g_img);
            set_timeout(animate, g_img.multi.frames[g_img.multi.sel].delay, true);
        } else {
            reset_timeout(animate);
        }
    }
    return dirty;
}


bool ci_scroll(CommandArg dir)
{
    return img_pan(&g_img, dir, g_prefix);
}


bool ci_scroll_to_center(CommandArg _)
{
    return img_pan_center(&g_img);
}


bool ci_scroll_to_edge(CommandArg dir)
{
    return img_pan_edge(&g_img, dir);
}


bool ci_drag(CommandArg drag_mode)
{
    int x, y, ox, oy;
    float px, py;
    XEvent e;

    if ((int)(g_img.w * g_img.zoom) <= (int)g_win.w && (int)(g_img.h * g_img.zoom) <= (int)g_win.h)
        return false;

    win_set_cursor(&g_win, drag_mode == DRAG_ABSOLUTE ? CURSOR_DRAG_ABSOLUTE : CURSOR_DRAG_RELATIVE);
    win_cursor_pos(&g_win, &x, &y);
    ox = x;
    oy = y;

    while (true) {
        if (drag_mode == DRAG_ABSOLUTE) {
            px = MIN(MAX(0.0, x - g_win.w * 0.1), g_win.w * 0.8) /
                 (g_win.w * 0.8) * (g_win.w - g_img.w * g_img.zoom);
            py = MIN(MAX(0.0, y - g_win.h * 0.1), g_win.h * 0.8) /
                 (g_win.h * 0.8) * (g_win.h - g_img.h * g_img.zoom);
        } else {
            px = g_img.x + x - ox;
            py = g_img.y + y - oy;
        }

        if (img_set_position(&g_img, px, py)) {
            img_render(&g_img);
            win_draw(&g_win);
        }
        XMaskEvent(g_win.env.dpy,
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &e);
        if (e.type == ButtonPress || e.type == ButtonRelease)
            break;
        while (XCheckTypedEvent(g_win.env.dpy, MotionNotify, &e))
            ;
        ox = x;
        oy = y;
        x = e.xmotion.x;
        y = e.xmotion.y;
    }
    set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
    reset_cursor();

    return true;
}


bool ci_set_zoom(CommandArg zl)
{
    return img_zoom_to(&g_img, (g_prefix ? g_prefix : zl) / 100.0);
}


bool ci_fit_to_win(CommandArg sm)
{
    return img_fit_win(&g_img, sm);
}


bool ci_rotate(CommandArg degree)
{
    img_rotate(&g_img, degree);
    return true;
}


bool ci_flip(CommandArg dir)
{
    img_flip(&g_img, dir);
    return true;
}


bool ci_toggle_antialias(CommandArg _)
{
    img_toggle_antialias(&g_img);
    return true;
}


bool ci_toggle_alpha(CommandArg _)
{
    g_img.flags = (g_img.flags & ~IF_HAS_ALPHA_LAYER) | (~g_img.flags & IF_HAS_ALPHA_LAYER);
    g_img.flags |= IF_IS_DIRTY;
    return true;
}


bool ci_slideshow(CommandArg _)
{
    if (g_prefix > 0) {
        g_img.slideshow_settings.is_enabled = true;
        g_img.slideshow_settings.delay = g_prefix * 10;
        set_timeout(slideshow, g_img.slideshow_settings.delay * 100, true);
    } else if (g_img.slideshow_settings.is_enabled) {
        g_img.slideshow_settings.is_enabled = false;
        reset_timeout(slideshow);
    } else {
        g_img.slideshow_settings.is_enabled = true;
    }
    return true;
}


bool ct_move_sel(CommandArg dir)
{
    return tns_move_selection(&g_tns, dir, g_prefix);
}


bool ct_reload_all(CommandArg flags)
{
    tns_replace(&g_tns, g_files, &g_filecnt, &g_fileidx, &g_win, flags);
    return true;
}


bool ct_scroll(CommandArg dir)
{
    return tns_scroll(&g_tns, dir, false);
}


bool ct_drag_mark_image(CommandArg _)
{
    int sel;

    if ((sel = tns_translate(&g_tns, g_xbutton_ev->x, g_xbutton_ev->y)) >= 0) {
        XEvent e;
        bool on = !(g_files[sel].flags & FF_MARK);

        while (true) {
            if (sel >= 0 && mark_image(sel, on))
                redraw();
            XMaskEvent(g_win.env.dpy,
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &e);
            if (e.type == ButtonPress || e.type == ButtonRelease)
                break;
            while (XCheckTypedEvent(g_win.env.dpy, MotionNotify, &e))
                ;
            sel = tns_translate(&g_tns, e.xbutton.x, e.xbutton.y);
        }
    }

    return false;
}


bool ct_select(CommandArg _)
{
    int sel;
    bool dirty = false;
    static Time firstclick;

    if ((sel = tns_translate(&g_tns, g_xbutton_ev->x, g_xbutton_ev->y)) >= 0) {
        if (sel != g_fileidx) {
            tns_highlight(&g_tns, g_fileidx, false);
            tns_highlight(&g_tns, sel, true);
            g_fileidx = sel;
            firstclick = g_xbutton_ev->time;
            dirty = true;
        } else if (g_xbutton_ev->time - firstclick <= TO_DOUBLE_CLICK) {
            g_mode = MODE_IMAGE;
            set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
            load_image(g_fileidx);
            dirty = true;
        } else {
            firstclick = g_xbutton_ev->time;
        }
    }

    return dirty;
}


bool ct_toggle_squared(CommandArg _)
{
    tns_toggle_squared();
    ct_reload_all(RF_KEEP_ZOOM_LEVEL & RF_KEEP_MARK_COLOR_MOD);

    return true;
}
