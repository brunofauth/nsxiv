// vim: foldmethod=marker foldlevel=0
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

#ifndef NSXIV_H
#define NSXIV_H

#if !defined(DEBUG) && !defined(NDEBUG)
    #define NDEBUG
#endif

#include <stdbool.h>
#include <X11/Xlib.h>

/*
 * Annotation for functions called in cleanup().
 * These functions are not allowed to call error(!0, ...) or exit().
 */
#define CLEANUP

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define ABS(a) ((a) > 0 ? (a) : -(a))

#define ARRLEN(a) (sizeof(a) / sizeof((a)[0]))
#define STREQ(s1,s2) (strcmp((s1), (s2)) == 0)


typedef enum {
    MODE_ALL,
    MODE_IMAGE,
    MODE_THUMB
} appmode_t;

typedef enum {
    DIR_LEFT  = 1,
    DIR_RIGHT = 2,
    DIR_UP    = 4,
    DIR_DOWN  = 8
} direction_t;

typedef enum {
    DEGREE_90  = 1,
    DEGREE_180 = 2,
    DEGREE_270 = 3
} degree_t;

typedef enum {
    SCALE_DOWN,
    SCALE_FIT,
    SCALE_FILL,
    SCALE_WIDTH,
    SCALE_HEIGHT,
    SCALE_ZOOM
} scalemode_t;

typedef enum {
    FLIP_HORIZONTAL = 1,
    FLIP_VERTICAL   = 2
} flipdir_t;

typedef enum {
    DRAG_RELATIVE,
    DRAG_ABSOLUTE
} dragmode_t;

typedef enum {
    FF_WARN    = 1,
    FF_MARK    = 2,
    FF_TN_IS_INIT = 4
} fileflags_t;

typedef struct {
    const char *name; /* as given by user */
    const char *path; /* always absolute, result of realpath(3) */
    fileflags_t flags;
} fileinfo_t;

/* timeouts in milliseconds: */
enum {
    TO_AUTORELOAD    = 128,
    TO_REDRAW_RESIZE = 75,
    TO_REDRAW_THUMBS = 200,
    TO_CURSOR_HIDE   = 1200,
    TO_DOUBLE_CLICK  = 300
};

typedef void (*timeout_f)(void);

// main.c {{{

/* timeout handler functions: */
void redraw(void);
void reset_cursor(void);
void animate(void);
void slideshow(void);
void clear_resize(void);

void remove_file(int, bool);
void set_timeout(timeout_f, int, bool);
void reset_timeout(timeout_f);
void close_info(void);
void open_info(void);
void load_image(int);
bool mark_image(int, bool);
int nav_button(void);
void handle_key_handler(bool);

extern appmode_t g_mode;
extern const XButtonEvent *g_xbutton_ev;
extern fileinfo_t *g_files;
extern int g_filecnt;
extern int g_fileidx;
extern int g_alternate;
extern int g_markcnt;
extern int g_markidx;
extern int g_prefix;

// }}}

#endif /* NSXIV_H */
