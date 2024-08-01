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

#include "nsxiv.h"
#include <Imlib2.h>
#define INCLUDE_THUMBS_CONFIG
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#if HAVE_LIBEXIF
#include <libexif/exif-data.h>
#endif

static char *cache_dir;
static char *cache_tmpfile, *cache_tmpfile_base;
static const char TMP_NAME[] = "/nsxiv-XXXXXX";

static char *tns_cache_translate_fp(const char *filepath)
{
	size_t len;
	char *cfile = NULL;

	assert(*filepath == '/' && "filepath must be result of realpath(3)");

	if (strncmp(filepath, cache_dir, strlen(cache_dir)) != 0) {
		/* don't cache images inside the cache directory! */
		len = strlen(cache_dir) + strlen(filepath) + 2;
		cfile = emalloc(len);
		snprintf(cfile, len, "%s/%s", cache_dir, filepath + 1);
	}
	return cfile;
}

static Imlib_Image tns_cache_load(const char *filepath, bool *outdated)
{
	char *cfile;
	struct stat cstats, fstats;
	Imlib_Image im = NULL;

	if (stat(filepath, &fstats) < 0)
		return NULL;

	if ((cfile = tns_cache_translate_fp(filepath)) != NULL) {
		if (stat(cfile, &cstats) == 0) {
			if (cstats.st_mtime == fstats.st_mtime)
				im = imlib_load_image(cfile);
			else
				*outdated = true;
		}
		free(cfile);
	}
	return im;
}

static void tns_cache_write(Imlib_Image im, const char *filepath, bool force)
{
	char *cfile, *dirend;
	int tmpfd;
	struct stat cstats, fstats;
	struct utimbuf times;
	Imlib_Load_Error err;

	if (options->private_mode)
		return;

	if (stat(filepath, &fstats) < 0)
		return;

	if ((cfile = tns_cache_translate_fp(filepath)) != NULL) {
		if (force || stat(cfile, &cstats) < 0 ||
		    cstats.st_mtime != fstats.st_mtime)
		{
			if ((dirend = strrchr(cfile, '/')) != NULL) {
				*dirend = '\0';
				if (r_mkdir(cfile) < 0)
					goto end;
				*dirend = '/';
			}
			imlib_context_set_image(im);
			if (imlib_image_has_alpha()) {
				imlib_image_set_format("png");
			} else {
				imlib_image_set_format("jpg");
				imlib_image_attach_data_value("quality", NULL, 90, NULL);
			}
			memcpy(cache_tmpfile_base, TMP_NAME, sizeof(TMP_NAME));
			if ((tmpfd = mkstemp(cache_tmpfile)) < 0)
				goto end;
			close(tmpfd);
			/* UPGRADE: Imlib2 v1.11.0: use imlib_save_image_fd() */
			imlib_save_image_with_error_return(cache_tmpfile, &err);
			times.actime = fstats.st_atime;
			times.modtime = fstats.st_mtime;
			utime(cache_tmpfile, &times);
			if (err || rename(cache_tmpfile, cfile) < 0)
				unlink(cache_tmpfile);
		}
end:
		free(cfile);
	}
}

void tns_clean_cache(void)
{
	int dirlen;
	char *cfile, *filename;
	r_dir_t dir;

	if (r_opendir(&dir, cache_dir, true) < 0) {
		error(0, errno, "%s", cache_dir);
		return;
	}

	dirlen = strlen(cache_dir);

	while ((cfile = r_readdir(&dir, false)) != NULL) {
		filename = cfile + dirlen;
		if (access(filename, F_OK) < 0) {
			if (unlink(cfile) < 0)
				error(0, errno, "%s", cfile);
		}
		free(cfile);
	}
	r_closedir(&dir);
}

float clamp(float n, float min, float max) {
	const float t = n < min ? min : n;
	return t > max ? max : t;
}

void _transform_mark_color_modifier(tns_t *tns) {
	float af[256], rf[256], gf[256], bf[256];
	for (int i = 255; i >= 0; i--)
		rf [i] = gf [i] = bf [i] = af [i] = (float) i / 255;

	// Could have more blocks like this for filters other than tint
	if (MCM_TINT[MCM_R] != 1.0)
		for (int i = 255; i >= 0; i--)
			rf[i] *= MCM_TINT[MCM_R];
	if (MCM_TINT[MCM_G] != 1.0)
		for (int i = 255; i >= 0; i--)
			gf[i] *= MCM_TINT[MCM_G];
	if (MCM_TINT[MCM_B] != 1.0)
		for (int i = 255; i >= 0; i--)
			bf[i] *= MCM_TINT[MCM_B];
	if (MCM_TINT[MCM_A] != 1.0)
		for (int i = 255; i >= 0; i--)
			af[i] *= MCM_TINT[MCM_A];

	for (int i = 255; i != 0 ; i--) {
	    tns->mcm->r[i] = clamp(rf[i], 0, 1) * 255;
	    tns->mcm->g[i] = clamp(gf[i], 0, 1) * 255;
	    tns->mcm->b[i] = clamp(bf[i], 0, 1) * 255;
	    tns->mcm->a[i] = clamp(af[i], 0, 1) * 255;
	}
}

void tns_init(tns_t *tns, fileinfo_t *tns_files, const int *cnt, int *sel, win_t *win)
{
	int len;
	const char *homedir, *dsuffix = "";

	if (cnt != NULL && *cnt > 0)
		tns->thumbs = ecalloc(*cnt, sizeof(*tns->thumbs));
	else
		tns->thumbs = NULL;
	tns->files = tns_files;
	tns->cnt = cnt;
	tns->next_to_init = tns->next_to_load_in_view = 0;
	tns->visible_thumbs_first = tns->visible_thumbs_end = tns->loaded_thumbs_first = tns->loaded_thumbs_end = 0;
	tns->sel = sel;
	tns->win = win;
	tns->dirty = false;

	tns->zoom_level = THUMB_SIZE;
	tns_zoom(tns, 0);

	markcolormod_t *table = emalloc(sizeof(markcolormod_t));
	for (int i = 255; i >= 0; i--)
		table->a[i] = table->r[i] = table->g[i] = table->b[i] = i;
	tns->mcm = table;
	_transform_mark_color_modifier(tns);

	if ((homedir = getenv("XDG_CACHE_HOME")) == NULL || homedir[0] == '\0') {
		homedir = getenv("HOME");
		dsuffix = "/.cache";
	}
	if (homedir == NULL)
		error(EXIT_FAILURE, 0, "Cache directory not found");

	const char *s = "/nsxiv";
	free(cache_dir);
	len = strlen(homedir) + strlen(dsuffix) + strlen(s) + 1;
	cache_dir = emalloc(len);
	snprintf(cache_dir, len, "%s%s%s", homedir, dsuffix, s);
	cache_tmpfile = emalloc(len + sizeof(TMP_NAME));
	memcpy(cache_tmpfile, cache_dir, len - 1);
	cache_tmpfile_base = cache_tmpfile + len - 1;
}

CLEANUP void tns_free(tns_t *tns)
{
	int i;

	if (tns->thumbs != NULL) {
		for (i = 0; i < *tns->cnt; i++)
			img_free(tns->thumbs[i].im, false);
		free(tns->thumbs);
		tns->thumbs = NULL;
	}

	free(tns->mcm);
	free(cache_dir);
	cache_dir = NULL;
	free(cache_tmpfile);
	cache_tmpfile = cache_tmpfile_base = NULL;
}

CLEANUP void tns_replace(tns_t *tns, fileinfo_t *tns_files, const int *cnt, int *sel, win_t *win, replaceflags_t flags)
{
	int zoom_level = THUMB_SIZE;
	if (flags & RF_KEEP_ZOOM_LEVEL)
		zoom_level = tns->zoom_level;
	markcolormod_t *mark_mod = NULL;
	if (flags & RF_KEEP_MARK_COLOR_MOD) {
		mark_mod = tns->mcm;
		tns->mcm = NULL;
	}

	tns_free(tns);

	int len;
	const char *homedir, *dsuffix = "";

	if (cnt != NULL && *cnt > 0)
		tns->thumbs = ecalloc(*cnt, sizeof(*tns->thumbs));
	else
		tns->thumbs = NULL;
	tns->files = tns_files;
	tns->cnt = cnt;
	tns->next_to_init = tns->next_to_load_in_view = 0;
	tns->visible_thumbs_first = tns->visible_thumbs_end = tns->loaded_thumbs_first = tns->loaded_thumbs_end = 0;
	tns->sel = sel;
	tns->win = win;
	tns->dirty = true;

	tns->zoom_level = zoom_level;
	tns_zoom(tns, 0);

	if (mark_mod != NULL) {
		tns->mcm = mark_mod;
	} else {
		markcolormod_t *table = emalloc(sizeof(markcolormod_t));
		for (int i = 255; i >= 0; i--)
			table->a[i] = table->r[i] = table->g[i] = table->b[i] = i;
		tns->mcm = table;
		_transform_mark_color_modifier(tns);
	}

	if ((homedir = getenv("XDG_CACHE_HOME")) == NULL || homedir[0] == '\0') {
		homedir = getenv("HOME");
		dsuffix = "/.cache";
	}
	if (homedir == NULL) 
		error(EXIT_FAILURE, 0, "Cache directory not found");

	const char *s = "/nsxiv";
	free(cache_dir);
	len = strlen(homedir) + strlen(dsuffix) + strlen(s) + 1;
	cache_dir = emalloc(len);
	snprintf(cache_dir, len, "%s%s%s", homedir, dsuffix, s);
	cache_tmpfile = emalloc(len + sizeof(TMP_NAME));
	memcpy(cache_tmpfile, cache_dir, len - 1);
	cache_tmpfile_base = cache_tmpfile + len - 1;
}

static Imlib_Image tns_scale_down(Imlib_Image im, int max_side_size)
{
	int w, h;

	imlib_context_set_image(im);
	w = imlib_image_get_width();
	h = imlib_image_get_height();

	float scale = (w < h) // Changed this to store a thumbnail that looks best in squared mode
		? (float) max_side_size / (float) w
		: (float) max_side_size / (float) h;
	scale = MIN(scale, 1.0);

	// This means the image is smaller than the requested maximum size
	if (scale >= 1.0)
		return im;

	imlib_context_set_anti_alias(1);
	im = imlib_create_cropped_scaled_image(
		0, 0, w, h,
		MAX(scale * w, 1), MAX(scale * h, 1));
	if (im == NULL)
		error(EXIT_FAILURE, ENOMEM, NULL);
	imlib_free_image_and_decache();

	return im;
}

// Besides loading thumbnails, this function also advances `next_to_init` and `next_to_load_in_view`
// Returns true if thumbnail was successfully loaded
bool tns_load(tns_t *tns, int n, bool force, bool cache_only)
{
	int maxwh = thumb_sizes[ARRLEN(thumb_sizes) - 1];
	bool cache_hit = false;
	char *cfile;
	thumb_t *t;
	fileinfo_t *file;
	Imlib_Image im = NULL;

	if (n < 0 || n >= *tns->cnt)
		return false;
	file = &tns->files[n];
	if (file->name == NULL || file->path == NULL)
		return false;

	t = &tns->thumbs[n];
	img_free(t->im, false);
	t->im = NULL;

	if (!force) {
		if ((im = tns_cache_load(file->path, &force)) != NULL) {
			imlib_context_set_image(im);
			if (imlib_image_get_width() < maxwh &&
			    imlib_image_get_height() < maxwh)
			{
				if ((cfile = tns_cache_translate_fp(file->path)) != NULL) {
					unlink(cfile);
					free(cfile);
				}
				imlib_free_image_and_decache();
				im = NULL;
			} else {
				cache_hit = true;
			}
#if HAVE_LIBEXIF
		} else if (!force && !options->private_mode) {
			int pw = 0, ph = 0, w, h, x = 0, y = 0;
			bool err;
			float zw, zh;
			ExifData *ed;
			ExifEntry *entry;
			ExifContent *ifd;
			ExifByteOrder byte_order;
			int tmpfd;
			char tmppath[] = "/tmp/nsxiv-XXXXXX";
			Imlib_Image tmpim;

			/* UPGRADE: Imlib2 v1.10.0: avoid tempfile and use imlib_load_image_mem() */
			if ((ed = exif_data_new_from_file(file->path)) != NULL) {
				if (ed->data != NULL && ed->size > 0 &&
				    (tmpfd = mkstemp(tmppath)) >= 0)
				{
					err = write(tmpfd, ed->data, ed->size) != ed->size;
					close(tmpfd);

					if (!err && (tmpim = imlib_load_image(tmppath)) != NULL) {
						byte_order = exif_data_get_byte_order(ed);
						ifd = ed->ifd[EXIF_IFD_EXIF];
						entry = exif_content_get_entry(ifd, EXIF_TAG_PIXEL_X_DIMENSION);
						if (entry != NULL)
							pw = exif_get_long(entry->data, byte_order);
						entry = exif_content_get_entry(ifd, EXIF_TAG_PIXEL_Y_DIMENSION);
						if (entry != NULL)
							ph = exif_get_long(entry->data, byte_order);

						imlib_context_set_image(tmpim);
						w = imlib_image_get_width();
						h = imlib_image_get_height();

						if (pw > w && ph > h && (pw - ph >= 0) == (w - h >= 0)) {
							zw = (float)pw / (float)w;
							zh = (float)ph / (float)h;
							if (zw < zh) {
								pw /= zh;
								x = (w - pw) / 2;
								w = pw;
							} else if (zw > zh) {
								ph /= zw;
								y = (h - ph) / 2;
								h = ph;
							}
						}
						if (w >= maxwh || h >= maxwh) {
							if ((im = imlib_create_cropped_image(x, y, w, h)) == NULL)
								error(0, 0, "%s: error generating thumbnail", file->name);
						}
						imlib_free_image_and_decache();
					}
					unlink(tmppath);
				}
				exif_data_unref(ed);
			}
#endif /* HAVE_LIBEXIF */
		}
	}

	if (im == NULL) {
		if ((im = img_open(file)) == NULL)
			return false;
	}
	imlib_context_set_image(im);

	if (!cache_hit) {
#if HAVE_LIBEXIF
		exif_auto_orientate(file);
#endif
		im = tns_scale_down(im, maxwh);
		imlib_context_set_image(im);
                // If the image is smaller than maxwh in both dims, we dont even cache it
		if (imlib_image_get_width() == maxwh || imlib_image_get_height() == maxwh)
			tns_cache_write(im, file->path, true);
	}

	if (cache_only) {
		imlib_free_image_and_decache();
	} else {
		t->im = tns_scale_down(im, thumb_sizes[tns->zoom_level]);
		imlib_context_set_image(t->im);
		t->w = imlib_image_get_width();
		t->h = imlib_image_get_height();
		tns->dirty = true;
	}
	file->flags |= FF_TN_IS_INIT;

	if (n == tns->next_to_init) {
		while (++tns->next_to_init < *tns->cnt && ((++file)->flags & FF_TN_IS_INIT))
			;
	}
	if (n == tns->next_to_load_in_view && !cache_only) {
		while (++tns->next_to_load_in_view < tns->visible_thumbs_end && (++t)->im != NULL)
			;
	}

	return true;
}

void tns_unload(tns_t *tns, int n)
{
	thumb_t *t;

	assert(n >= 0 && n < *tns->cnt);
	t = &tns->thumbs[n];

	img_free(t->im, false);
	t->im = NULL;
}

static void tns_check_view(tns_t *tns, bool scrolled)
{
	int r;

	assert(tns != NULL);
	tns->visible_thumbs_first -= tns->visible_thumbs_first % tns->cols;
	r = *tns->sel % tns->cols;

	if (scrolled) {
		/* move selection into visible area */
		if (*tns->sel >= tns->visible_thumbs_first + tns->cols * tns->rows)
			*tns->sel = tns->visible_thumbs_first + r + tns->cols * (tns->rows - 1);
		else if (*tns->sel < tns->visible_thumbs_first)
			*tns->sel = tns->visible_thumbs_first + r;
	} else {
		/* scroll to selection */
		if (tns->visible_thumbs_first + tns->cols * tns->rows <= *tns->sel) {
			tns->visible_thumbs_first = *tns->sel - r - tns->cols * (tns->rows - 1);
			tns->dirty = true;
		} else if (tns->visible_thumbs_first > *tns->sel) {
			tns->visible_thumbs_first = *tns->sel - r;
			tns->dirty = true;
		}
	}
}

void tns_render(tns_t *tns)
{
	thumb_t *t;
	win_t *win;
	int i, cnt, r, grid_x, grid_y, grid_capacity, cell_side;

	if (!tns->dirty)
		return;

	win = tns->win;
	win_clear(win);
	imlib_context_set_drawable(win->buf.pm);

	tns->cols = MAX(1, win->w / tns->dim);
	tns->rows = MAX(1, win->h / tns->dim);
	grid_capacity = tns->cols * tns->rows;
	cell_side = thumb_sizes[tns->zoom_level];

	if (*tns->cnt < grid_capacity) {
		tns->visible_thumbs_first = 0;
		cnt = *tns->cnt;
	} else {
		tns_check_view(tns, false);
		cnt = grid_capacity;
		if ((r = tns->visible_thumbs_first + cnt - *tns->cnt) >= tns->cols)
			tns->visible_thumbs_first -= r - r % tns->cols;
		if (r > 0)
			cnt -= r % tns->cols;
	}
	r = cnt % tns->cols ? 1 : 0;
	tns->x = grid_x = (win->w - MIN(cnt, tns->cols) * tns->dim) / 2 + tns->border_width + 3;
	tns->y = grid_y = (win->h - (cnt / tns->cols + r) * tns->dim) / 2 + tns->border_width + 3 +
	             (win->bar.top ? win->bar.h : 0);
	tns->next_to_load_in_view = *tns->cnt;
	tns->visible_thumbs_end = tns->visible_thumbs_first + cnt;

	// Unload thumbs from other views/pages
	for (i = tns->loaded_thumbs_first; i < tns->loaded_thumbs_end; i++) {
                // Check if said thumb is outside of the current view
		if ((i < tns->visible_thumbs_first || i >= tns->visible_thumbs_end) && tns->thumbs[i].im != NULL)
			tns_unload(tns, i);
	}
	tns->loaded_thumbs_first = tns->visible_thumbs_first;
	tns->loaded_thumbs_end = tns->visible_thumbs_end;

	for (i = tns->visible_thumbs_first; i < tns->visible_thumbs_end; i++) {
		t = &tns->thumbs[i];
		// TODO: figure out at which times t->im can be NULL here
		if (t->im == NULL) {
			tns->next_to_load_in_view = MIN(tns->next_to_load_in_view, i);
		} else {
			imlib_context_set_image(t->im);
			if (square_thumbs) {
				int size = MIN(t->w, t->h);
				int tn_x = (t->w < t->h) ? 0 : (t->w - t->h) / 2;
				int tn_y = (t->w > t->h) ? 0 : (t->h - t->w) / 2;
				t->x = grid_x;
				t->y = grid_y;
				imlib_render_image_part_on_drawable_at_size(
					tn_x, tn_y, size, size,
					t->x, t->y, cell_side, cell_side
				);
			} else {
				t->scale = (t->w > t->h)
					? ((float) cell_side / (float) t->w)
					: ((float) cell_side / (float) t->h);
				int scaled_w = (int) (t->scale * t->w);
				int scaled_h = (int) (t->scale * t->h);
				t->x = grid_x + (cell_side - scaled_w) / 2;
				t->y = grid_y + (cell_side - scaled_h) / 2;
				imlib_render_image_on_drawable_at_size(t->x, t->y, scaled_w, scaled_h);
			}
			if (tns->files[i].flags & FF_MARK)
				tns_mark(tns, i, true);
		}
		if ((i + 1) % tns->cols == 0) {
			grid_x = tns->x;
			grid_y += tns->dim;
		} else {
			grid_x += tns->dim;
		}
	}
	tns->dirty = false;
	tns_highlight(tns, *tns->sel, true);
}

// Imlib has actual filters, but I couldn't figure out how they work, so I just
// reimplemented that functionality (probably in a worse way)
Imlib_Image apply_filters(Imlib_Image src, markcolormod_t *table) {
	imlib_context_set_image(src);
	Imlib_Image clone;
	if (!(clone = imlib_clone_image()))
		error(EXIT_FAILURE, 0, "Couldn't apply filters to image");
	imlib_context_set_image(clone);
	Imlib_Color_Modifier color_modifier;
	if (!(color_modifier = imlib_create_color_modifier()))
		error(EXIT_FAILURE, 0, "Couldn't apply filters to image");
	imlib_context_set_color_modifier(color_modifier);
	imlib_set_color_modifier_tables(table->r, table->g, table->b, table->a);
	imlib_apply_color_modifier();
	imlib_free_color_modifier();
	return clone;
}

void tns_mark(tns_t *tns, int n, bool mark)
{
	if (n < 0 || n >= *tns->cnt || tns->thumbs[n].im == NULL)
	    return;

	win_t *win = tns->win;
	thumb_t *t = &tns->thumbs[n];
	unsigned long color;
	int cell_side = thumb_sizes[tns->zoom_level];
	Imlib_Image filtered = NULL;

	int mark_w = cell_side / 3;
	int mark_h = cell_side / 3;
	int mark_x, mark_y;

	if (mark) {
		filtered = apply_filters(t->im, tns->mcm);
		imlib_context_set_image(filtered);
	} else {
		imlib_context_set_image(t->im);
	}

	if (square_thumbs) {
		mark_x = t->x - mark_w/2 + cell_side/2;
		mark_y = t->y - mark_h/2 + cell_side/2;
		int size = MIN(t->w, t->h);
		int tn_x = (t->w < t->h) ? 0 : (t->w - t->h) / 2;
		int tn_y = (t->w > t->h) ? 0 : (t->h - t->w) / 2;
		imlib_render_image_part_on_drawable_at_size(
			tn_x, tn_y, size, size,
			t->x, t->y, cell_side, cell_side
		);
	} else {
		int scaled_w = (int) (t->scale * t->w);
		int scaled_h = (int) (t->scale * t->h);
		mark_x = t->x - mark_w/2 + scaled_w/2;
		mark_y = t->y - mark_h/2 + scaled_h/2;
		imlib_render_image_on_drawable_at_size(t->x, t->y, scaled_w, scaled_h);
	}

	if (mark) {
		color = win->win_bg.pixel;
		win_draw_rect(win, mark_x, mark_y, mark_w, mark_h, true, 1, color);
		color = win->tn_mark_fg.pixel;
		win_draw_rect(win, mark_x + MARK_BORDER_SIZE, mark_y + MARK_BORDER_SIZE, mark_w - 2 * MARK_BORDER_SIZE, mark_h - 2 * MARK_BORDER_SIZE, true, 1, color);
	}

	if (filtered != NULL)
		free(filtered);
}

void tns_highlight(tns_t *tns, int n, bool hl)
{
	if (n < 0 || n >= *tns->cnt || tns->thumbs[n].im == NULL)
	    return;

	win_t *win = tns->win;
	thumb_t *t = &tns->thumbs[n];
	unsigned long color = hl ? win->win_fg.pixel : win->win_bg.pixel;
	int offset_xy = (tns->border_width + 1) / 2 + 1;
	int offset_wh = tns->border_width + 2;
	int cell_side = thumb_sizes[tns->zoom_level];

	if (square_thumbs) {
		int w = t->w + offset_wh;
		int h = t->h + offset_wh;
		int size = MAX(MIN(w, h), cell_side);
	    	win_draw_rect(win, t->x - offset_xy, t->y - offset_xy, size, size, false, tns->border_width, color);
	} else {
		float scale = (t->w > t->h) ? ((float) cell_side / (float) t->w) : ((float) cell_side / (float) t->h);
		int scaled_w = (int) (scale * t->w);
		int scaled_h = (int) (scale * t->h);
		int w = scaled_w + offset_wh;
		int h = scaled_h + offset_wh;
	    	win_draw_rect(win, t->x - offset_xy, t->y - offset_xy, w, h, false, tns->border_width, color);
	}
}

bool tns_move_selection(tns_t *tns, const direction_t dir, int cnt)
{
	int old, max;

	old = *tns->sel;
	cnt = cnt > 1 ? cnt : 1;

	switch (dir) {
	case DIR_UP:
		*tns->sel = MAX(*tns->sel - cnt * tns->cols, *tns->sel % tns->cols);
		break;
	case DIR_DOWN:
		max = tns->cols * ((*tns->cnt - 1) / tns->cols) +
		      MIN((*tns->cnt - 1) % tns->cols, *tns->sel % tns->cols);
		*tns->sel = MIN(*tns->sel + cnt * tns->cols, max);
		break;
	case DIR_LEFT:
		*tns->sel = MAX(*tns->sel - cnt, 0);
		break;
	case DIR_RIGHT:
		*tns->sel = MIN(*tns->sel + cnt, *tns->cnt - 1);
		break;
	}

	if (*tns->sel != old) {
		tns_highlight(tns, old, false);
		tns_check_view(tns, false);
		if (!tns->dirty)
			tns_highlight(tns, *tns->sel, true);
	}
	return *tns->sel != old;
}

bool tns_scroll(tns_t *tns, const direction_t dir, const bool screen)
{
	int d, max, old;

	old = tns->visible_thumbs_first;
	d = tns->cols * (screen ? tns->rows : 1);

	if (dir == DIR_DOWN) {
		max = *tns->cnt - tns->cols * tns->rows;
		if (*tns->cnt % tns->cols != 0)
			max += tns->cols - *tns->cnt % tns->cols;
		tns->visible_thumbs_first = MIN(tns->visible_thumbs_first + d, max);
	} else if (dir == DIR_UP) {
		tns->visible_thumbs_first = MAX(tns->visible_thumbs_first - d, 0);
	}

	if (tns->visible_thumbs_first != old) {
		tns_check_view(tns, true);
		tns->dirty = true;
	}
	return tns->visible_thumbs_first != old;
}

bool tns_zoom(tns_t *tns, const int d)
{
	int i, oldzl, tn_cell_size;

	oldzl = tns->zoom_level;
	tns->zoom_level += -(d < 0) + (d > 0);
	tns->zoom_level = MAX(tns->zoom_level, 0);
	tns->zoom_level = MIN(tns->zoom_level, (int)ARRLEN(thumb_sizes) - 1);
	tn_cell_size = thumb_sizes[tns->zoom_level];

	tns->border_width = ((tn_cell_size - 1) >> 5) + 1;
	tns->border_width = MIN(tns->border_width, MAX_BORDER_SIZE_HL);
	tns->dim = tn_cell_size + GRID_GAP_SIZE;

	if (tns->zoom_level != oldzl) {
		for (i = 0; i < *tns->cnt; i++)
			tns_unload(tns, i);
		tns->dirty = true;
	}
	return tns->zoom_level != oldzl;
}

int tns_translate(tns_t *tns, const int x, const int y)
{
	int n;

	if (x < tns->x || y < tns->y)
		return -1;

	n = tns->visible_thumbs_first + (y - tns->y) / tns->dim * tns->cols +
	    (x - tns->x) / tns->dim;
	if (n >= *tns->cnt)
		n = -1;

	return n;
}

bool tns_toggle_squared(void)
{
	square_thumbs = !square_thumbs;
	return true;
}
