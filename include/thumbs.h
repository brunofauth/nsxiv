#pragma once

#include <stdint.h>
#include <Imlib2.h>
#include "range.h"
#include "window.h"


typedef struct {
    Imlib_Image im;
    int w;
    int h;
    int x;
    int y;
    float scale;
} thumb_t;


typedef struct {
    uint8_t r[256];
    uint8_t g[256];
    uint8_t b[256];
    uint8_t a[256];
} ColorModifier;


typedef enum {
    RF_KEEP_ZOOM_LEVEL = 1,
    RF_KEEP_MARK_COLOR_MOD = 2,
} replaceflags_t;


enum {
    MCM_R = 0,
    MCM_G = 1,
    MCM_B = 2,
    MCM_A = 3,
};


typedef struct {
    fileinfo_t *files;
    thumb_t *thumbs;
    const int *cnt;
    int *sel;
    int next_to_init;
    int next_to_load_in_view;
    IndexRange visible_thumbs;
    IndexRange loaded_thumbs;

    win_t *win;
    int x;
    int y;
    int cols;
    int rows;
    int zoom_level;
    int border_width;
    int dim;
    ColorModifier *mark_cm;

    bool dirty;
} ThumbnailState;


// {{{

void tns_init(
    ThumbnailState*,
    fileinfo_t*,
    const int* thumbnail_count,
    int* selected_thumbnail,
    win_t* window
) __attribute__((nonnull(1)));

CLEANUP void tns_replace(
    ThumbnailState*,
    fileinfo_t*,
    const int* thumbnail_count,
    int* selected_thumbnail,
    win_t* window,
    replaceflags_t flags
) __attribute__((nonnull(1)));

void tns_clean_cache(void);

CLEANUP void tns_free(ThumbnailState*)
    __attribute__((nonnull(1)));

bool tns_load(ThumbnailState*, int thumbnail_index, bool force, bool cache_only)
    __attribute__((nonnull(1)));

void tns_unload(ThumbnailState*, int thumbnail_index)
    __attribute__((nonnull(1)));

void tns_render(ThumbnailState*)
    __attribute__((nonnull(1)));

void tns_mark(ThumbnailState*, int thumbnail_index, bool should_mark)
    __attribute__((nonnull(1)));

void tns_highlight(ThumbnailState*, int thumbnail_index, bool should_highlight)
    __attribute__((nonnull(1)));

bool tns_move_selection(ThumbnailState*, direction_t, int grid_distance)
    __attribute__((nonnull(1)));

bool tns_scroll(ThumbnailState*, direction_t, bool whole_screen)
    __attribute__((nonnull(1)));

bool tns_zoom(ThumbnailState*, int zoom_level_index)
    __attribute__((nonnull(1)));

int tns_translate(ThumbnailState*, int grid_x, int grid_y)
    __attribute__((nonnull(1)));

bool tns_toggle_squared(void);

// }}}
