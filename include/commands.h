// vim: foldmethod=marker foldlevel=0
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include <X11/Xlib.h>
#include "nsxiv.h"


typedef int CommandArg;
typedef bool (*CommandFp)(CommandArg);

typedef struct {
    CommandFp func;
    appmode_t mode;
} Command;

typedef struct {
    unsigned int mask;
    KeySym ksym_or_button;
    Command cmd;
    CommandArg arg;
} keymap_t;

typedef keymap_t button_t;


// global {{{1
bool cg_change_gamma(CommandArg);
bool cg_change_brightness(CommandArg);
bool cg_change_contrast(CommandArg);
bool cg_first(CommandArg);
bool cg_mark_range(CommandArg);
bool cg_n_or_last(CommandArg);
bool cg_navigate_marked(CommandArg);
bool cg_prefix_external(CommandArg);
bool cg_quit(CommandArg);
bool cg_pick_quit(CommandArg);
bool cg_reload_image(CommandArg);
bool cg_remove_image(CommandArg);
bool cg_reverse_marks(CommandArg);
bool cg_scroll_screen(CommandArg);
bool cg_switch_mode(CommandArg);
bool cg_toggle_bar(CommandArg);
bool cg_toggle_fullscreen(CommandArg);
bool cg_toggle_image_mark(CommandArg);
bool cg_unmark_all(CommandArg);
bool cg_zoom(CommandArg);
// image mode {{{1
bool ci_alternate(CommandArg);
bool ci_cursor_navigate(CommandArg);
bool ci_drag(CommandArg);
bool ci_fit_to_win(CommandArg);
bool ci_flip(CommandArg);
bool ci_navigate(CommandArg);
bool ci_navigate_frame(CommandArg);
bool ci_rotate(CommandArg);
bool ci_scroll(CommandArg);
bool ci_scroll_to_center(CommandArg);
bool ci_scroll_to_edge(CommandArg);
bool ci_set_zoom(CommandArg);
bool ci_slideshow(CommandArg);
bool ci_toggle_alpha(CommandArg);
bool ci_toggle_animation(CommandArg);
bool ci_toggle_antialias(CommandArg);
// thumbnails mode {{{1
bool ct_move_sel(CommandArg);
bool ct_reload_all(CommandArg);
bool ct_scroll(CommandArg);
bool ct_drag_mark_image(CommandArg);
bool ct_select(CommandArg);
bool ct_toggle_squared(CommandArg);
// }}}


#ifdef INCLUDE_MAPPINGS_CONFIG

// global {{{1
#define g_change_gamma { cg_change_gamma, MODE_ALL }
#define g_change_brightness { cg_change_brightness, MODE_ALL }
#define g_change_contrast { cg_change_contrast, MODE_ALL }
#define g_first { cg_first, MODE_ALL }
#define g_mark_range { cg_mark_range, MODE_ALL }
#define g_n_or_last { cg_n_or_last, MODE_ALL }
#define g_navigate_marked { cg_navigate_marked, MODE_ALL }
#define g_prefix_external { cg_prefix_external, MODE_ALL }
#define g_quit { cg_quit, MODE_ALL }
#define g_pick_quit { cg_pick_quit, MODE_ALL }
#define g_reload_image { cg_reload_image, MODE_ALL }
#define g_remove_image { cg_remove_image, MODE_ALL }
#define g_reverse_marks { cg_reverse_marks, MODE_ALL }
#define g_scroll_screen { cg_scroll_screen, MODE_ALL }
#define g_switch_mode { cg_switch_mode, MODE_ALL }
#define g_toggle_bar { cg_toggle_bar, MODE_ALL }
#define g_toggle_fullscreen { cg_toggle_fullscreen, MODE_ALL }
#define g_toggle_image_mark { cg_toggle_image_mark, MODE_ALL }
#define g_unmark_all { cg_unmark_all, MODE_ALL }
#define g_zoom { cg_zoom, MODE_ALL }

// image mode {{{1
#define i_alternate { ci_alternate, MODE_IMAGE }
#define i_cursor_navigate { ci_cursor_navigate, MODE_IMAGE }
#define i_drag { ci_drag, MODE_IMAGE }
#define i_fit_to_win { ci_fit_to_win, MODE_IMAGE }
#define i_flip { ci_flip, MODE_IMAGE }
#define i_navigate { ci_navigate, MODE_IMAGE }
#define i_navigate_frame { ci_navigate_frame, MODE_IMAGE }
#define i_rotate { ci_rotate, MODE_IMAGE }
#define i_scroll { ci_scroll, MODE_IMAGE }
#define i_scroll_to_center { ci_scroll_to_center, MODE_IMAGE }
#define i_scroll_to_edge { ci_scroll_to_edge, MODE_IMAGE }
#define i_set_zoom { ci_set_zoom, MODE_IMAGE }
#define i_slideshow { ci_slideshow, MODE_IMAGE }
#define i_toggle_alpha { ci_toggle_alpha, MODE_IMAGE }
#define i_toggle_animation { ci_toggle_animation, MODE_IMAGE }
#define i_toggle_antialias { ci_toggle_antialias, MODE_IMAGE }

// thumbnails mode {{{1
#define t_move_sel { ct_move_sel, MODE_THUMB }
#define t_reload_all { ct_reload_all, MODE_THUMB }
#define t_scroll { ct_scroll, MODE_THUMB }
#define t_drag_mark_image { ct_drag_mark_image, MODE_THUMB }
#define t_select { ct_select, MODE_THUMB }
#define t_toggle_squared { ct_toggle_squared, MODE_THUMB }
// }}}

#endif // INCLUDE_MAPPINGS_CONFIG


#endif // COMMANDS_H
