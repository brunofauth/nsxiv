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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <X11/XF86keysym.h>
#include <X11/keysym.h>

#include "autoreload.h"
#include "image.h"
#include "thumbs.h"
#include "window.h"
#include "cli_options.h"
#include "util.h"

#define INCLUDE_MAPPINGS_CONFIG
#include "commands.h"
#include "config.h"

#define MODMASK(mask) (USED_MODMASK & (mask))
#define BAR_SEP "  "

#define TV_DIFF(t1,t2) (((t1)->tv_sec  - (t2)->tv_sec ) * 1000 + \
                        ((t1)->tv_usec - (t2)->tv_usec) / 1000)
#define TV_ADD_MSEC(tv, t)                  \
    do {                                    \
        (tv)->tv_sec  += (t) / 1000;        \
        (tv)->tv_usec += (t) % 1000 * 1000; \
    } while (0)


typedef struct {
    int err;
    char *cmd;
} extcmd_t;


AutoreloadState g_state_autoreload;
SxivImage g_img;
ThumbnailState g_tns;
win_t g_win;
extern opt_t *g_options;


appmode_t g_mode;
fileinfo_t *g_files;
int g_filecnt, g_fileidx;
int g_alternate;
int g_markcnt;
int g_markidx;
int g_prefix;
const XButtonEvent *g_xbutton_ev;

static void autoreload(void);

static bool extprefix;
static bool resized = false;

static struct {
    extcmd_t f, ft;
    int fd;
    pid_t pid;
} info, wintitle;

static struct {
    extcmd_t f;
    bool warned;
} keyhandler;

static struct {
    timeout_f handler;
    struct timeval when;
    bool active;
} timeouts[] = {
    { .handler = autoreload   },
    { .handler = redraw       },
    { .handler = reset_cursor },
    { .handler = slideshow    },
    { .handler = animate      },
    { .handler = clear_resize },
};

/*
 * function implementations
 */

static void cleanup(void)
{
    img_close(&g_img, false);
    autoreload_cleanup(&g_state_autoreload);
    tns_free(&g_tns);
    win_close(&g_win);
}


static bool xgetline(char **lineptr, size_t *n)
{
    ssize_t len = getdelim(lineptr, n, g_options->using_null ? '\0' : '\n', stdin);
    if (!g_options->using_null && len > 0 && (*lineptr)[len - 1] == '\n')
        (*lineptr)[len - 1] = '\0';
    return len > 0;
}


static int fncmp(const void *a, const void *b)
{
    return strcoll(((fileinfo_t *)a)->name, ((fileinfo_t *)b)->name);
}


static void check_add_file(const char *filename, bool given)
{
    char *path;

    if (*filename == '\0')
        return;

    if (access(filename, R_OK) < 0 ||
        (path = realpath(filename, NULL)) == NULL)
    {
        if (given)
            error_log(errno, "%s", filename);
        return;
    }

    if (g_fileidx == g_filecnt) {
        g_filecnt *= 2;
        g_files = erealloc(g_files, g_filecnt * sizeof(*g_files));
        memset(&g_files[g_filecnt / 2], 0, g_filecnt / 2 * sizeof(*g_files));
    }

    g_files[g_fileidx].name = estrdup(filename);
    g_files[g_fileidx].path = path;
    if (given)
        g_files[g_fileidx].flags |= FF_WARN;
    g_fileidx++;
}


static void add_entry(const char *entry_name)
{
    int start;
    char *filename;
    struct stat fstats;
    r_dir_t dir;

    if (stat(entry_name, &fstats) < 0) {
        error_log(errno, "%s", entry_name);
        return;
    }
    if (!S_ISDIR(fstats.st_mode)) {
        check_add_file(entry_name, true);
    } else {
        if (r_opendir(&dir, entry_name, g_options->recursive) < 0) {
            error_log(errno, "%s", entry_name);
            return;
        }
        start = g_fileidx;
        while ((filename = r_readdir(&dir, true)) != NULL) {
            check_add_file(filename, false);
            free(filename);
        }
        r_closedir(&dir);
        if (g_fileidx - start > 1)
            qsort(g_files + start, g_fileidx - start, sizeof(*g_files), fncmp);
    }
}


void remove_file(int n, bool manual)
{
    if (n < 0 || n >= g_filecnt)
        return;

    if (g_filecnt == 1) {
        if (!manual)
            fprintf(stderr, "%s: no more files to display, aborting\n", progname);
        exit(manual ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    if (g_files[n].flags & FF_MARK)
        g_markcnt--;

    if (g_files[n].path != g_files[n].name)
        free((void *)g_files[n].path);
    free((void *)g_files[n].name);
    if (g_tns.thumbs != NULL)
        tns_unload(&g_tns, n);

    if (n + 1 < g_filecnt) {
        if (g_tns.thumbs != NULL) {
            memmove(g_tns.thumbs + n, g_tns.thumbs + n + 1,
                    (g_filecnt - n - 1) * sizeof(*g_tns.thumbs));
            memset(g_tns.thumbs + g_filecnt - 1, 0, sizeof(*g_tns.thumbs));
        }
        memmove(g_files + n, g_files + n + 1, (g_filecnt - n - 1) * sizeof(*g_files));
    }
    g_filecnt--;
    if (g_fileidx > n || g_fileidx == g_filecnt)
        g_fileidx--;
    if (g_alternate > n || g_alternate == g_filecnt)
        g_alternate--;
    if (g_markidx > n || g_markidx == g_filecnt)
        g_markidx--;
}


void set_timeout(timeout_f handler, int time, bool overwrite)
{
    unsigned int i;

    for (i = 0; i < ARRLEN(timeouts); i++) {
        if (timeouts[i].handler == handler) {
            if (!timeouts[i].active || overwrite) {
                gettimeofday(&timeouts[i].when, 0);
                TV_ADD_MSEC(&timeouts[i].when, time);
                timeouts[i].active = true;
            }
            return;
        }
    }
}


void reset_timeout(timeout_f handler)
{
    unsigned int i;

    for (i = 0; i < ARRLEN(timeouts); i++) {
        if (timeouts[i].handler == handler) {
            timeouts[i].active = false;
            return;
        }
    }
}


static bool check_timeouts(int *t)
{
    int i = 0, tdiff, tmin;
    struct timeval now;

    for (i = 0; i < (int)ARRLEN(timeouts); ++i) {
        if (timeouts[i].active) {
            // TODO: does this syscall **need** to be inside a loop?
            gettimeofday(&now, 0);
            tdiff = TV_DIFF(&timeouts[i].when, &now);
            if (tdiff <= 0) {
                timeouts[i].active = false;
                if (timeouts[i].handler != NULL)
                    timeouts[i].handler();
            }
        }
    }

    tmin = INT_MAX;
    // TODO: would it be innaproppriate to just call this at the top of the
    // function (so that we may call it only a single time... [thinking
    // about the loop above])
    gettimeofday(&now, 0);
    for (i = 0; i < (int)ARRLEN(timeouts); ++i) {
        if (timeouts[i].active) {
            tdiff = TV_DIFF(&timeouts[i].when, &now);
            tmin = MIN(tmin, tdiff);
        }
    }

    if (tmin != INT_MAX && t != NULL)
        *t = MAX(tmin, 0);
    return tmin != INT_MAX;
}


static void autoreload(void)
{
    if (g_img.flags & IF_IS_AUTORELOAD_PENDING) {
        img_close(&g_img, true);
        /* load_image() sets autoreload_pending to false */
        load_image(g_fileidx);
        redraw();
    } else {
        assert(!"unreachable");
    }
}


static void kill_close(pid_t pid, int *fd)
{
    if (fd != NULL && *fd != -1) {
        kill(pid, SIGTERM);
        close(*fd);
        *fd = -1;
    }
}


static void close_title(void)
{
    kill_close(wintitle.pid, &wintitle.fd);
}


static void read_title(void)
{
    ssize_t n;
    char buf[512];

    if ((n = read(wintitle.fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        win_set_title(&g_win, buf, n);
    }
    close_title();
}


static void open_title(void)
{
    char *argv[8];
    char w[12] = "", h[12] = "", z[12] = "", fidx[12], fcnt[12];

    if (wintitle.f.err)
        return;

    close_title();
    if (g_mode == MODE_IMAGE) {
        snprintf(w, ARRLEN(w), "%d", g_img.w);
        snprintf(h, ARRLEN(h), "%d", g_img.h);
        snprintf(z, ARRLEN(z), "%d", (int)(g_img.zoom * 100));
    }
    snprintf(fidx, ARRLEN(fidx), "%d", g_fileidx + 1);
    snprintf(fcnt, ARRLEN(fcnt), "%d", g_filecnt);
    construct_argv(argv, ARRLEN(argv), wintitle.f.cmd, g_files[g_fileidx].path,
                   fidx, fcnt, w, h, z, NULL);
    wintitle.pid = spawn(&wintitle.fd, NULL, O_NONBLOCK, argv);
}


void close_info(void)
{
    kill_close(info.pid, &info.fd);
}


void open_info(void)
{
    char *argv[6], w[12] = "", h[12] = "";
    char *cmd = g_mode == MODE_IMAGE ? info.f.cmd : info.ft.cmd;
    bool ferr = g_mode == MODE_IMAGE ? info.f.err : info.ft.err;

    if (ferr || info.fd >= 0 || g_win.bar.h == 0)
        return;
    g_win.bar.l.buf[0] = '\0';
    if (g_mode == MODE_IMAGE) {
        snprintf(w, sizeof(w), "%d", g_img.w);
        snprintf(h, sizeof(h), "%d", g_img.h);
    }
    construct_argv(argv, ARRLEN(argv), cmd, g_files[g_fileidx].name, w, h,
                   g_files[g_fileidx].path, NULL);
    info.pid = spawn(&info.fd, NULL, O_NONBLOCK, argv);
}


static void read_info(void)
{
    ssize_t n = read(info.fd, g_win.bar.l.buf, g_win.bar.l.size - 1);
    if (n <= 0) {
        close_info();
        return;
    }

    g_win.bar.l.buf[n] = '\0';
    for (ssize_t i = 0; i < n; ++i) {
        if (g_win.bar.l.buf[i] == '\n')
            g_win.bar.l.buf[i] = ' ';
    }
    win_draw(&g_win);
    close_info();
}


void load_image(int new)
{
    bool prev = new < g_fileidx;
    static int current;

    if (new < 0 || new >= g_filecnt)
        return;

    if (g_win.xwin != None)
        win_set_cursor(&g_win, CURSOR_WATCH);
    reset_timeout(autoreload);
    reset_timeout(slideshow);

    if (new != current) {
        g_alternate = current;
        g_img.flags &= ~IF_IS_AUTORELOAD_PENDING;
    }

    img_close(&g_img, false);
    while (!img_load(&g_img, &g_files[new])) {
        remove_file(new, false);
        if (new >= g_filecnt)
            new = g_filecnt - 1;
        else if (new > 0 && prev)
            new -= 1;
    }
    g_files[new].flags &= ~FF_WARN;
    g_fileidx = current = new;

    autoreload_add(&g_state_autoreload, g_files[g_fileidx].path);

    if (g_img.multi.cnt > 0 && g_img.multi.animate)
        set_timeout(animate, g_img.multi.frames[g_img.multi.sel].delay, true);
    else
        reset_timeout(animate);
}


bool mark_image(int n, bool on)
{
    g_markidx = n;
    if (!!(g_files[n].flags & FF_MARK) != on) {
        g_files[n].flags ^= FF_MARK;
        g_markcnt += on ? 1 : -1;
        if (g_mode == MODE_THUMB)
            tns_mark(&g_tns, n, on);
        return true;
    }
    return false;
}


static void bar_put(win_bar_t *bar, const char *fmt, ...)
{
    size_t len = bar->size - (bar->p - bar->buf), n;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(bar->p, len, fmt, ap);
    bar->p += MIN(len, n);
    va_end(ap);
}


static void update_info(void)
{
    const char *mark;
    win_bar_t *l = &g_win.bar.l, *r = &g_win.bar.r;

    static struct {
        const char *filepath;
        int fileidx;
        float zoom;
        appmode_t mode;
    } prev;

    if (prev.fileidx != g_fileidx || prev.mode != g_mode ||
        (prev.filepath == NULL || !STREQ(prev.filepath, g_files[g_fileidx].path)))
    {
        close_info();
        open_info();
        open_title();
    } else if (g_mode == MODE_IMAGE && prev.zoom != g_img.zoom) {
        open_title();
    }

    if (g_win.bar.h == 0 || extprefix)
        return;

    free((char *)prev.filepath);
    prev.filepath = estrdup(g_files[g_fileidx].path);
    prev.fileidx = g_fileidx;
    prev.zoom = g_img.zoom;
    prev.mode = g_mode;

    uint32_t fw = 0;
    for (uint32_t i = g_filecnt; i > 0; fw++, i /= 10)
        ;

    mark = g_files[g_fileidx].flags & FF_MARK ? "* " : "";
    l->p = l->buf;
    r->p = r->buf;
    if (g_mode == MODE_THUMB) {
        if (g_tns.next_to_load_in_view < g_tns.visible_thumbs.end)
            bar_put(r, "Loading... %0*d | ", fw, g_tns.next_to_load_in_view + 1);
        else if (g_tns.next_to_init < g_filecnt)
            bar_put(r, "Caching... %0*d | ", fw, g_tns.next_to_init + 1);
        bar_put(r, "%s%0*d/%d", mark, fw, g_fileidx + 1, g_filecnt);
        if (info.ft.err)
            strncpy(l->buf, g_files[g_fileidx].name, l->size);
    } else {
        bar_put(r, "%s", mark);
        if (g_img.slideshow_settings.is_enabled) {
            if (g_img.slideshow_settings.delay % 10 != 0)
                bar_put(r, "%2.1fs" BAR_SEP, (float)g_img.slideshow_settings.delay / 10);
            else
                bar_put(r, "%ds" BAR_SEP, g_img.slideshow_settings.delay / 10);
        }
        if (g_img.gamma)
            bar_put(r, "G%+d" BAR_SEP, g_img.gamma);
        if (g_img.brightness)
            bar_put(r, "B%+d" BAR_SEP, g_img.brightness);
        if (g_img.contrast)
            bar_put(r, "C%+d" BAR_SEP, g_img.contrast);
        bar_put(r, "%3d%%" BAR_SEP, (int)(g_img.zoom * 100.0));

        if (g_img.multi.cnt > 0) {
            uint32_t fn = 0;
            for (uint32_t i = g_img.multi.cnt; i > 0; fn++, i /= 10)
                ;
            bar_put(r, "%0*d/%d" BAR_SEP, fn, g_img.multi.sel + 1, g_img.multi.cnt);
        }

        bar_put(r, "%0*d/%d", fw, g_fileidx + 1, g_filecnt);
        if (info.f.err)
            strncpy(l->buf, g_files[g_fileidx].name, l->size);
    }
}


int nav_button(void)
{
    int x, y, nw;

    if (NAV_WIDTH == 0)
        return 1;

    win_cursor_pos(&g_win, &x, &y);
    nw = NAV_IS_REL ? g_win.w * NAV_WIDTH / 100 : NAV_WIDTH;
    nw = MIN(nw, ((int)g_win.w + 1) / 2);

    if (x < nw)
        return 0;
    if (x < (int)g_win.w - nw)
        return 1;
    return 2;
}


void redraw(void)
{
    if (g_mode == MODE_IMAGE) {
        img_render(&g_img);
        if (g_img.slideshow_settings.is_enabled) {
            int t = g_img.slideshow_settings.delay * 100;
            if (g_img.multi.cnt > 0 && g_img.multi.animate)
                t = MAX(t, g_img.multi.length);
            set_timeout(slideshow, t, false);
        }
    } else {
        tns_render(&g_tns);
    }
    update_info();
    win_draw(&g_win);
    reset_timeout(redraw);
    reset_cursor();
}


void reset_cursor(void)
{
    if (g_mode != MODE_IMAGE) {
        win_set_cursor(&g_win, 
            (g_tns.next_to_load_in_view < g_tns.visible_thumbs.end || g_tns.next_to_init < g_filecnt)
                ? CURSOR_WATCH : CURSOR_ARROW);
        return;
    }

    cursor_t cursor = CURSOR_NONE;
    for (uint32_t i = 0; i < ARRLEN(timeouts); i++) {
        if (timeouts[i].handler == reset_cursor) {
            if (timeouts[i].active) {
                int c = nav_button();
                c = MAX(g_fileidx > 0 ? 0 : 1, c);
                c = MIN(g_fileidx + 1 < g_filecnt ? 2 : 1, c);
                cursor = imgcursor[c];
            }
            break;
        }
    }
    win_set_cursor(&g_win, cursor);
}


void animate(void)
{
    if (img_frame_animate(&g_img)) {
        set_timeout(animate, g_img.multi.frames[g_img.multi.sel].delay, true);
        redraw();
    }
}


void slideshow(void)
{
    load_image(g_fileidx + 1 < g_filecnt ? g_fileidx + 1 : 0);
    redraw();
}


void clear_resize(void)
{
    resized = false;
}


static Bool is_input_ev(Display *dpy, XEvent *ev, XPointer arg)
{
    return ev->type == ButtonPress || ev->type == KeyPress;
}


void handle_key_handler(bool init)
{
    extprefix = init;
    if (g_win.bar.h == 0)
        return;
    if (init) {
        snprintf(g_win.bar.r.buf, g_win.bar.r.size,
                 "Getting key handler input (%s to abort)...",
                 XKeysymToString(KEYHANDLER_ABORT));
    } else { /* abort */
        update_info();
    }
    win_draw(&g_win);
}


static bool run_key_handler(const char *key, unsigned int mask)
{
    FILE *pfs;
    bool marked = g_mode == MODE_THUMB && g_markcnt > 0;
    bool changed = false;
    pid_t pid;
    int writefd, f, i;
    int fcnt = marked ? g_markcnt : 1;
    char kstr[32];
    struct stat *oldst, st;
    XEvent dump;
    char *argv[3];

    if (keyhandler.f.err) {
        if (!keyhandler.warned) {
            error_log(keyhandler.f.err, "%s", keyhandler.f.cmd);
            keyhandler.warned = true;
        }
        return false;
    }
    if (key == NULL)
        return false;

    strncpy(g_win.bar.r.buf, "Running key handler...", g_win.bar.r.size);
    win_draw(&g_win);
    win_set_cursor(&g_win, CURSOR_WATCH);
    setenv("NSXIV_USING_NULL", g_options->using_null ? "1" : "0", 1);

    snprintf(kstr, sizeof(kstr), "%s%s%s%s",
             mask & ControlMask ? "C-" : "",
             mask & Mod1Mask    ? "M-" : "",
             mask & ShiftMask   ? "S-" : "", key);
    construct_argv(argv, ARRLEN(argv), keyhandler.f.cmd, kstr, NULL);
    if ((pid = spawn(NULL, &writefd, 0x0, argv)) < 0)
        return false;
    if ((pfs = fdopen(writefd, "w")) == NULL) {
        error_log(errno, "open pipe");
        close(writefd);
        return false;
    }

    oldst = emalloc(fcnt * sizeof(*oldst));
    for (f = i = 0; f < fcnt; i++) {
        if ((marked && (g_files[i].flags & FF_MARK)) || (!marked && i == g_fileidx)) {
            stat(g_files[i].path, &oldst[f]);
            fprintf(pfs, "%s%c", g_files[i].name, g_options->using_null ? '\0' : '\n');
            f++;
        }
    }
    fclose(pfs);
    while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
        ;

    for (f = i = 0; f < fcnt; i++) {
        if ((marked && (g_files[i].flags & FF_MARK)) || (!marked && i == g_fileidx)) {
            if (stat(g_files[i].path, &st) != 0 ||
                memcmp(&oldst[f].st_mtime, &st.st_mtime, sizeof(st.st_mtime)) != 0)
            {
                if (g_tns.thumbs != NULL) {
                    tns_unload(&g_tns, i);
                    g_tns.next_to_load_in_view = MIN(g_tns.next_to_load_in_view, i);
                }
                changed = true;
            }
            f++;
        }
    }
    /* drop user input events that occurred while running the key handler */
    while (XCheckIfEvent(g_win.env.dpy, &dump, is_input_ev, NULL))
        ;

    if (g_mode == MODE_IMAGE && changed) {
        img_close(&g_img, true);
        load_image(g_fileidx);
    } else {
        update_info();
    }
    free(oldst);
    reset_cursor();
    return true;
}


static bool process_bindings(const keymap_t *bindings, unsigned int len, KeySym ksym_or_button,
                             unsigned int state, unsigned int implicit_mod)
{
    unsigned int i;
    bool dirty = false;

    for (i = 0; i < len; i++) {
        if (bindings[i].ksym_or_button == ksym_or_button &&
            MODMASK(bindings[i].mask | implicit_mod) == MODMASK(state) &&
            bindings[i].cmd.func != NULL &&
            (bindings[i].cmd.mode == MODE_ALL || bindings[i].cmd.mode == g_mode))
        {
            if (bindings[i].cmd.func(bindings[i].arg))
                dirty = true;
        }
    }
    return dirty;
}


static void on_keypress(XKeyEvent *kev)
{
    unsigned int sh = 0;
    KeySym ksym, shksym;
    char dummy, key;
    bool dirty = false;

    XLookupString(kev, &key, 1, &ksym, NULL);

    if (kev->state & ShiftMask) {
        kev->state &= ~ShiftMask;
        XLookupString(kev, &dummy, 1, &shksym, NULL);
        kev->state |= ShiftMask;
        if (ksym != shksym)
            sh = ShiftMask;
    }
    if (IsModifierKey(ksym))
        return;
    if (extprefix && ksym == KEYHANDLER_ABORT && MODMASK(kev->state) == 0) {
        handle_key_handler(false);
    } else if (extprefix) {
        if ((dirty = run_key_handler(XKeysymToString(ksym), kev->state & ~sh)))
            extprefix = false;
        else
            handle_key_handler(false);
    } else if (key >= '0' && key <= '9') {
        /* number prefix for commands */
        g_prefix = g_prefix * 10 + (int)(key - '0');
        return;
    } else {
        dirty = process_bindings(keys, ARRLEN(keys), ksym, kev->state, sh);
    }
    if (dirty)
        redraw();
    g_prefix = 0;
}


static void on_buttonpress(const XButtonEvent *bev)
{
    bool dirty = false;

    if (g_mode == MODE_IMAGE) {
        set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
        reset_cursor();
        dirty = process_bindings(buttons_img, ARRLEN(buttons_img), bev->button, bev->state, 0);
    } else { /* thumbnail mode */
        dirty = process_bindings(buttons_tns, ARRLEN(buttons_tns), bev->button, bev->state, 0);
    }
    if (dirty)
        redraw();
    g_prefix = 0;
}


static void run(void)
{
    int32_t timeout = 0;
    XEvent ev;
    g_xbutton_ev = &ev.xbutton;

    while (true) {
        bool to_set = check_timeouts(&timeout);
        bool should_init_thumb = g_mode == MODE_THUMB && g_tns.next_to_init < g_filecnt;
        bool should_load_thumb = g_mode == MODE_THUMB && g_tns.next_to_load_in_view < g_tns.visible_thumbs.end;

        // "Only do heavy processing while there are no events to process"
        if (XPending(g_win.env.dpy) == 0) {
            if (should_load_thumb) {
                set_timeout(redraw, TO_REDRAW_THUMBS, false);
                if (!tns_load(&g_tns, g_tns.next_to_load_in_view, false, false)) {
                    remove_file(g_tns.next_to_load_in_view, false);
                    g_tns.dirty = true;
                }
                if (g_tns.next_to_load_in_view >= g_tns.visible_thumbs.end) {
                    open_info();
                    redraw();
                }
                continue;
            }
            if (should_init_thumb) {
                set_timeout(redraw, TO_REDRAW_THUMBS, false);
                if (!tns_load(&g_tns, g_tns.next_to_init, false, true))
                    remove_file(g_tns.next_to_init, false);
                continue;
            }
            if (to_set || info.fd != -1 || g_state_autoreload.fd != -1) {
                enum { FD_X, FD_INFO, FD_TITLE, FD_ARL, FD_CNT };
                // This needs to be reinitialized in every loop... might as well declare it here
                struct pollfd pfd[FD_CNT];

                pfd[FD_X].fd = ConnectionNumber(g_win.env.dpy);
                pfd[FD_INFO].fd = info.fd;
                pfd[FD_TITLE].fd = wintitle.fd;
                pfd[FD_ARL].fd = g_state_autoreload.fd;

                pfd[FD_X].events = pfd[FD_ARL].events = POLLIN;
                pfd[FD_INFO].events = pfd[FD_TITLE].events = 0;

                if (poll(pfd, ARRLEN(pfd), to_set ? timeout : -1) < 0)
                    continue;
                if (pfd[FD_INFO].revents & POLLHUP)
                    read_info();
                if (pfd[FD_TITLE].revents & POLLHUP)
                    read_title();
                if ((pfd[FD_ARL].revents & POLLIN) && autoreload_handle_events(&g_state_autoreload)) {
                    g_img.flags |= IF_IS_AUTORELOAD_PENDING;
                    set_timeout(autoreload, TO_AUTORELOAD, true);
                }
                continue;
            }
        }

        bool discard;
        do {
            XNextEvent(g_win.env.dpy, &ev);
            discard = false;
            if (XEventsQueued(g_win.env.dpy, QueuedAlready) > 0) {
                XEvent nextev;
                XPeekEvent(g_win.env.dpy, &nextev);
                switch (ev.type) {
                case ConfigureNotify:
                case MotionNotify:
                    discard = ev.type == nextev.type;
                    break;
                case KeyPress:
                    discard = (nextev.type == KeyPress || nextev.type == KeyRelease) &&
                              ev.xkey.keycode == nextev.xkey.keycode;
                    break;
                }
            }
        } while (discard);

        switch (ev.type) {
        case ButtonPress:
            on_buttonpress(&ev.xbutton);
            break;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == atoms[ATOM_WM_DELETE_WINDOW])
                cg_quit(EXIT_SUCCESS);
            break;
        case DestroyNotify:
            cg_quit(EXIT_FAILURE);
            break;
        case ConfigureNotify:
            if (win_configure(&g_win, &ev.xconfigure)) {
                if (g_mode == MODE_IMAGE) {
                    g_img.flags |= IF_IS_DIRTY;
                    g_img.flags |= IF_CHECKPAN;
                } else {
                    g_img.flags |= IF_IS_DIRTY;
                }
                if (!resized) {
                    redraw();
                    set_timeout(clear_resize, TO_REDRAW_RESIZE, false);
                    resized = true;
                } else {
                    set_timeout(redraw, TO_REDRAW_RESIZE, false);
                }
            }
            break;
        case KeyPress:
            on_keypress(&ev.xkey);
            break;
        case MotionNotify:
            if (g_mode == MODE_IMAGE) {
                set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
                reset_cursor();
            }
            break;
        }
    }
}


static void setup_signal(int sig, void (*handler)(int sig), int flags)
{
    struct sigaction sa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    if (sigaction(sig, &sa, NULL) < 0)
        error_quit(EXIT_FAILURE, errno, "signal %d", sig);
}


int main(int argc, char *argv[])
{
    int i;
    size_t n;
    const char *homedir, *dsuffix = "";

    setup_signal(SIGCHLD, SIG_DFL, SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT);
    setup_signal(SIGPIPE, SIG_IGN, 0);

    setlocale(LC_COLLATE, "");

    parse_options(argc, argv);

    if (g_options->clean_cache) {
        tns_init(&g_tns, NULL, NULL, NULL, NULL);
        tns_clean_cache();
        exit(EXIT_SUCCESS);
    }

    if (g_options->filecnt == 0 && !g_options->from_stdin) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    if (g_options->recursive || g_options->from_stdin)
        g_filecnt = 1024;
    else
        g_filecnt = g_options->filecnt;

    g_files = ecalloc(g_filecnt, sizeof(*g_files));
    g_fileidx = 0;

    if (g_options->from_stdin) {
        char *filename = NULL;
        n = 0;
        while (xgetline(&filename, &n))
            add_entry(filename);
        free(filename);
    }

    for (i = 0; i < g_options->filecnt; i++)
        add_entry(g_options->filenames[i]);

    if (g_fileidx == 0)
        error_quit(EXIT_FAILURE, 0, "No valid image file given, aborting");

    g_filecnt = g_fileidx;
    g_fileidx = g_options->startnum < g_filecnt ? g_options->startnum : 0;

    if (g_options->background_cache && !g_options->private_mode) {
        pid_t ppid = getpid(); /* to check if parent is still alive or not */
        switch (fork()) {
        case 0:
            tns_init(&g_tns, g_files, &g_filecnt, &g_fileidx, NULL);
            while (g_filecnt > 0 && getppid() == ppid) {
                tns_load(&g_tns, g_filecnt - 1, false, true);
                remove_file(g_filecnt - 1, true);
            }
            exit(0);
            break;
        case -1:
            error_log(errno, "fork failed");
            break;
        }
    }

    win_init(&g_win);
    img_init(&g_img, &g_win);
    autoreload_init(&g_state_autoreload);

    if ((homedir = getenv("XDG_CONFIG_HOME")) == NULL || homedir[0] == '\0') {
        homedir = getenv("HOME");
        dsuffix = "/.config";
    }
    if (homedir != NULL) {
        extcmd_t *cmd[] = { &info.f, &info.ft, &keyhandler.f, &wintitle.f };
        const char *name[] = { "image-info", "thumb-info", "key-handler", "win-title" };
        const char *s = "/nsxiv/exec/";

        for (i = 0; i < (int)ARRLEN(cmd); i++) {
            n = strlen(homedir) + strlen(dsuffix) + strlen(s) + strlen(name[i]) + 1;
            cmd[i]->cmd = emalloc(n);
            snprintf(cmd[i]->cmd, n, "%s%s%s%s", homedir, dsuffix, s, name[i]);
            if (access(cmd[i]->cmd, X_OK) != 0)
                cmd[i]->err = errno;
        }
    } else {
        error_log(0, "Exec directory not found");
    }
    wintitle.fd = info.fd = -1;

    if (g_options->thumb_mode) {
        g_mode = MODE_THUMB;
        tns_init(&g_tns, g_files, &g_filecnt, &g_fileidx, &g_win);
        while (!tns_load(&g_tns, g_fileidx, false, false))
            remove_file(g_fileidx, false);
    } else {
        g_mode = MODE_IMAGE;
        g_tns.thumbs = NULL;
        load_image(g_fileidx);
    }
    win_open(&g_win);
    win_set_cursor(&g_win, CURSOR_WATCH);

    atexit(cleanup);

    set_timeout(redraw, 25, false);

    run();

    return 0;
}
