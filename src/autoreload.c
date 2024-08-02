/* Copyright 2017 Max Voit, Bert Muennich
 * Copyright 2022-2023 nsxiv contributors
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

#include "nsxiv.h"

#if HAVE_INOTIFY

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

static struct {
    char *buf;
    size_t len;
} scratch;

void autoreload_init(arl_t *arl)
{
    arl->fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    arl->wd_dir = arl->wd_file = -1;
    if (arl->fd == -1)
        error(0, 0, "Could not initialize inotify, no automatic image reloading");
}

CLEANUP void autoreload_cleanup(arl_t *arl)
{
    if (arl->fd != -1)
        close(arl->fd);
    free(scratch.buf);
}

static void rm_watch(int fd, int *wd)
{
    if (*wd != -1) {
        inotify_rm_watch(fd, *wd);
        *wd = -1;
    }
}

static void add_watch(int fd, int *wd, const char *path, uint32_t mask)
{
    *wd = inotify_add_watch(fd, path, mask);
    if (*wd == -1)
        error(0, errno, "inotify: %s", path);
}

static char *arl_scratch_push(const char *filepath, size_t len)
{
    if (scratch.len < len + 1) {
        scratch.len = len + 1;
        scratch.buf = erealloc(scratch.buf, scratch.len);
    }
    scratch.buf[len] = '\0';
    return memcpy(scratch.buf, filepath, len);
}

void autoreload_add(arl_t *arl, const char *filepath)
{
    if (arl->fd == -1)
        return;

    rm_watch(arl->fd, &arl->wd_dir);
    rm_watch(arl->fd, &arl->wd_file);
    add_watch(arl->fd, &arl->wd_file, filepath, IN_CLOSE_WRITE | IN_DELETE_SELF);

    const char *base = strrchr(filepath, '/');
    assert(base != NULL && "filepath must be result of realpath(3)");
    const char *dir = arl_scratch_push(filepath, MAX(base - filepath, 1));
    add_watch(arl->fd, &arl->wd_dir, dir, IN_CREATE | IN_MOVED_TO);
    arl->filename = arl_scratch_push(base + 1, strlen(base + 1));
}

bool autoreload_handle_events(arl_t *arl)
{
    bool reload = false;
    const struct inotify_event *inotify_event;

    while (true) {
        /* inotify_event aligned buffer */
        static union {
            char d[4096];
            struct inotify_event e;
        } buf;

        ssize_t len = read(arl->fd, buf.d, sizeof(buf.d));
        if (len == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        for (char *ptr = buf.d; ptr < buf.d + len; ptr += sizeof(*inotify_event) + inotify_event->len) {
            inotify_event = (const struct inotify_event *)ptr;
            if (inotify_event->wd == arl->wd_file && (inotify_event->mask & IN_CLOSE_WRITE)) {
                reload = true;
            } else if (inotify_event->wd == arl->wd_file && (inotify_event->mask & IN_DELETE_SELF)) {
                rm_watch(arl->fd, &arl->wd_file);
            } else if (inotify_event->wd == arl->wd_dir && (inotify_event->mask & (IN_CREATE | IN_MOVED_TO))) {
                if (STREQ(inotify_event->name, arl->filename))
                    reload = true;
            }
        }
    }
    return reload;
}

#else

void arl_init(arl_t *arl)
{
    arl->fd = -1;
}

void arl_cleanup(arl_t *arl)
{
    (void)arl;
}

void arl_add(arl_t *arl, const char *filepath)
{
    (void)arl;
    (void)filepath;
}

bool arl_handle(arl_t *arl)
{
    (void)arl;
    return false;
}

#endif /* HAVE_INOTIFY */
