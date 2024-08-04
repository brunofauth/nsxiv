#pragma once

#include <stdbool.h>
#include <X11/Xlib.h>
#include "nsxiv.h"


typedef struct {
    /* file list: */
    char **filenames;
    bool from_stdin;
    bool to_stdout;
    bool using_null;
    bool recursive;
    int filecnt;
    int startnum;

    /* image: */
    scalemode_t scalemode;
    float zoom;
    bool animate;
    bool anti_alias;
    bool alpha_layer;
    int gamma;
    unsigned int slideshow;
    int framerate;

    /* window: */
    bool fullscreen;
    bool hide_bar;
    Window embed;
    char *geometry;
    char *res_name;

    /* misc flags: */
    bool quiet;
    bool thumb_mode;
    bool clean_cache;
    bool private_mode;
    bool background_cache;
} opt_t;


// {{{
void print_usage(void);
void parse_options(int, char**);
// }}}
