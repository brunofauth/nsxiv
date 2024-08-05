#pragma once


#include <stdbool.h>
#include <X11/Xutil.h>
#include "nsxiv.h"

#if HAVE_LIBFONTS
#include <X11/Xft/Xft.h>
#endif


typedef enum {
    CURSOR_ARROW,
    CURSOR_DRAG_ABSOLUTE,
    CURSOR_DRAG_RELATIVE,
    CURSOR_WATCH,
    CURSOR_LEFT,
    CURSOR_RIGHT,
    CURSOR_NONE,

    CURSOR_COUNT
} cursor_t;


enum {
    ATOM_WM_DELETE_WINDOW,
    ATOM__NET_WM_NAME,
    ATOM__NET_WM_ICON_NAME,
    ATOM__NET_WM_ICON,
    ATOM__NET_WM_STATE,
    ATOM__NET_WM_PID,
    ATOM__NET_WM_STATE_FULLSCREEN,
    ATOM_UTF8_STRING,
    ATOM_WM_NAME,
    ATOM_WM_ICON_NAME,
    ATOM_COUNT
};


typedef struct {
    Display *dpy;
    int scr;
    int scrw, scrh;
    Visual *vis;
    Colormap cmap;
    int depth;
} win_env_t;


typedef struct {
    size_t size;
    char *p;
    char *buf;
} win_bar_t;


typedef struct {
    Window xwin;
    win_env_t env;

    XColor win_bg;
    XColor win_fg;
    XColor tn_mark_fg;
#if HAVE_LIBFONTS
    XftColor bar_bg;
    XftColor bar_fg;
#endif

    int x;
    int y;
    unsigned int w;
    unsigned int h; /* = win height - bar height */
    unsigned int bw;

    struct {
        unsigned int w;
        unsigned int h;
        Pixmap pm;
    } buf;

    struct {
        unsigned int h;
        bool top;
        win_bar_t l;
        win_bar_t r;
    } bar;
} win_t;


extern Atom atoms[ATOM_COUNT];


// {{{

void win_init(win_t*);
void win_open(win_t*);
CLEANUP void win_close(win_t*);
bool win_configure(win_t*, const XConfigureEvent*);
void win_toggle_fullscreen(win_t*);
void win_toggle_bar(win_t*);
void win_clear(win_t*);
void win_draw(win_t*);
void win_draw_rect(win_t *window, int x, int y, int w, int h, bool fill, int line_width, unsigned long color);
void win_set_title(win_t*, const char *title, size_t length);
void win_set_cursor(win_t*, cursor_t);
void win_cursor_pos(win_t*, int *x, int *y);

// }}}
