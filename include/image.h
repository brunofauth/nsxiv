#pragma once

#include <stdbool.h>
#include <Imlib2.h>
#include "window.h"


#ifdef IMLIB2_VERSION // UPGRADE: Imlib2 v1.8.0: remove all HAVE_IMLIB2_MULTI_FRAME ifdefs
    #if IMLIB2_VERSION >= IMLIB2_VERSION_(1, 8, 0)
        #define HAVE_IMLIB2_MULTI_FRAME 1
    #endif
#endif
#ifndef HAVE_IMLIB2_MULTI_FRAME
    #define HAVE_IMLIB2_MULTI_FRAME 0
#endif


typedef struct {
    Imlib_Image im;
    unsigned int delay;
} ImageFrame;


// Used for animated images (GIFs...)
typedef struct {
    // TODO: can't this be a flexible array member?
    ImageFrame *frames;
    unsigned int cap;
    unsigned int cnt;
    unsigned int sel;
    bool animate;
    int framedelay;
    int length;
} ImageFrameSet;


typedef enum {
    IF_CHECKPAN = 1,
    IF_IS_DIRTY = 2,
    IF_ANTI_ALIAS_ENABLED = 4,
    IF_HAS_ALPHA_LAYER = 8,
    IF_IS_AUTORELOAD_PENDING = 16,
} ImageFlags;


typedef struct {
    Imlib_Image im;
    int w;
    int h;

    win_t *win;
    float x;
    float y;

    Imlib_Color_Modifier cmod;
    int gamma;
    int brightness;
    int contrast;

    scalemode_t scalemode;
    float zoom;

    ImageFlags flags;

    struct {
        bool is_enabled;
        int delay;
    } slideshow_settings;

    ImageFrameSet multi;
} SxivImage;


void img_init(SxivImage*, win_t*)
    __attribute__((nonnull(1)));

bool img_load(SxivImage*, const fileinfo_t*)
    __attribute__((nonnull(1)));

CLEANUP void img_free(Imlib_Image, const bool decache);

CLEANUP void img_close(SxivImage*, const bool decache)
    __attribute__((nonnull(1)));

void img_render(SxivImage*)
    __attribute__((nonnull(1)));

bool img_fit_win(SxivImage*, scalemode_t)
    __attribute__((nonnull(1)));

bool img_zoom(SxivImage*, int)
    __attribute__((nonnull(1)));

bool img_zoom_to(SxivImage*, float)
    __attribute__((nonnull(1)));

bool img_pos(SxivImage*, float, float)
    __attribute__((nonnull(1)));

bool img_pan(SxivImage*, direction_t, int)
    __attribute__((nonnull(1)));

bool img_pan_center(SxivImage*)
    __attribute__((nonnull(1)));

bool img_pan_edge(SxivImage*, direction_t)
    __attribute__((nonnull(1)));

void img_rotate(SxivImage*, degree_t)
    __attribute__((nonnull(1)));

void img_flip(SxivImage*, flipdir_t)
    __attribute__((nonnull(1)));

void img_toggle_antialias(SxivImage*)
    __attribute__((nonnull(1)));

void img_update_color_modifiers(SxivImage*)
    __attribute__((nonnull(1)));

bool img_change_color_modifier(SxivImage*, int, int*)
    __attribute__((nonnull(1)));

bool img_frame_navigate(SxivImage*, int)
    __attribute__((nonnull(1)));

bool img_frame_animate(SxivImage*)
    __attribute__((nonnull(1)));

Imlib_Image img_open(const fileinfo_t*)
    __attribute__((nonnull(1)));

#if HAVE_LIBEXIF
void exif_auto_orientate(const fileinfo_t*)
    __attribute__((nonnull(1)));
#endif

