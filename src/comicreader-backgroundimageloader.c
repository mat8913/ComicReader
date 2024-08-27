/* comicreader-backgroundimageloader.c
 *
 * Copyright 2024 Matthew Harm Bekkema
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "comicreader-backgroundimageloader.h"
#include "comicreader-debug.h"

#include <stdbool.h>

struct CacheItem {
	struct ComicReaderImage *image;
	size_t index;
};

struct ComicReaderBackgroundImageLoader {
	struct ComicReaderImageLoader parent;
	struct ComicReaderImageLoader *inner_loader;
	GThreadPool *thread_pool;
	struct CacheItem cache[3];
	size_t current_index;
	int ref_count;
	bool loading;
};

struct BackgroundLoadData {
	struct ComicReaderBackgroundImageLoader *self;
	struct CacheItem item;
};

/* interface implementations */
static size_t impl_get_num_images(struct ComicReaderImageLoader *image_loader);
static struct ComicReaderImage *impl_get_image(
	struct ComicReaderImageLoader *image_loader,
	size_t index);
static void impl_free(struct ComicReaderImageLoader *image_loader);

/* helper functions */
static void add_to_cache(struct ComicReaderBackgroundImageLoader *self, struct CacheItem item);
static void start_load_in_background(struct ComicReaderBackgroundImageLoader *self);
static void load_in_background(void *p, void *unused);
static gboolean finish_load_in_background(void *p);
static size_t get_prev_index(struct ComicReaderBackgroundImageLoader *self);
static size_t get_next_index(struct ComicReaderBackgroundImageLoader *self);

struct ComicReaderImageLoader *comicreader_background_image_loader_new(
	struct ComicReaderImageLoader *inner_loader)
{
	struct ComicReaderBackgroundImageLoader *ret;
	ret = calloc(1, sizeof(struct ComicReaderBackgroundImageLoader));
	debug_init("ComicReaderBackgroundImageLoader", ret);

	ret->parent.free = impl_free;
	ret->parent.get_num_images = impl_get_num_images;
	ret->parent.get_image = impl_get_image;

	ret->inner_loader = inner_loader;
	ret->thread_pool = g_thread_pool_new(&load_in_background, NULL, 1, FALSE, NULL);

	ret->ref_count = 1;
	ret->loading = false;

	g_assert((void *)ret == (void *)&ret->parent);
	return &ret->parent;
}

static size_t impl_get_num_images(struct ComicReaderImageLoader *image_loader)
{
	struct ComicReaderBackgroundImageLoader *self =
		(struct ComicReaderBackgroundImageLoader *)image_loader;

	return self->inner_loader->get_num_images(self->inner_loader);
}

static struct ComicReaderImage *impl_get_image(
	struct ComicReaderImageLoader *image_loader,
	size_t index)
{
	struct ComicReaderBackgroundImageLoader *self =
		(struct ComicReaderBackgroundImageLoader *)image_loader;
	struct ComicReaderImage *image = NULL;

	self->current_index = index;

	for (size_t i = 0; i < sizeof(self->cache) / sizeof(self->cache[0]); ++i) {
		if (self->cache[i].index == index && self->cache[i].image) {
			image = comicreader_image_dup(self->cache[i].image);
			break;
		}
	}

	if (!image) {
		debug_printf("cache miss for image index %zu\n", index);
		image = self->inner_loader->get_image(self->inner_loader, index);
		struct CacheItem item;
		item.index = index;
		item.image = comicreader_image_dup(image);
		add_to_cache(self, item);
	}

	if (!self->loading)
		start_load_in_background(self);

	return image;
}

static void impl_free(struct ComicReaderImageLoader *image_loader)
{
	struct ComicReaderBackgroundImageLoader *self =
		(struct ComicReaderBackgroundImageLoader *)image_loader;

	--self->ref_count;
	g_assert(self->ref_count >= 0);
	if (self->ref_count > 0)
		return;

	for (size_t i = 0; i < sizeof(self->cache) / sizeof(self->cache[0]); ++i) {
		comicreader_image_clear(&self->cache[i].image);
	}
	comicreader_image_loader_clear(&self->inner_loader);

	g_assert(g_thread_pool_unprocessed(self->thread_pool) == 0);
	g_thread_pool_free(self->thread_pool, TRUE, FALSE);

	debug_free("ComicReaderBackgroundImageLoader", self);
	free(image_loader);
}

static void add_to_cache(struct ComicReaderBackgroundImageLoader *self, struct CacheItem item)
{
	struct CacheItem *dest = NULL;

	size_t current = self->current_index;
	size_t next = get_next_index(self);
	size_t prev = get_prev_index(self);

	if (item.index != current && item.index != next && item.index != prev) {
		debug_printf("not caching index %zu\n", item.index);
		comicreader_image_clear(&item.image);
		return;
	}

	/* search for an unused or unwanted slot */
	for (size_t i = 0; i < sizeof(self->cache) / sizeof(self->cache[0]); ++i) {
		size_t index = self->cache[i].index;
		int wanted = index == current || index == next || index == prev;
		if (!self->cache[i].image || !wanted) {
			dest = &self->cache[i];
			break;
		}
	}

	/* search for a slot with same index */
	for (size_t i = 0; i < sizeof(self->cache) / sizeof(self->cache[0]); ++i) {
		if (self->cache[i].index == item.index && self->cache[i].image) {
			dest = &self->cache[i];
			break;
		}
	}

	g_assert(dest);

	debug_printf("caching index %zu\n", item.index);
	comicreader_image_clear(&dest->image);
	dest->image = item.image;
	dest->index = item.index;
}

static void start_load_in_background(struct ComicReaderBackgroundImageLoader *self)
{
	size_t next_index = get_next_index(self);
	size_t prev_index = get_prev_index(self);

	bool have_next = false;
	bool have_prev = false;

	for (size_t i = 0; i < sizeof(self->cache) / sizeof(self->cache[0]); ++i) {
		if (!self->cache[i].image)
			continue;
		size_t cindex = self->cache[i].index;
		if (cindex == next_index)
			have_next = true;
		if (cindex == prev_index)
			have_prev = true;
	}

	if (have_next && have_prev)
		return;

	struct BackgroundLoadData *data = calloc(1, sizeof(*data));
	data->self = self;
	++self->ref_count;
	data->item.index = have_next ? prev_index : next_index;

	self->loading = true;
	g_thread_pool_push(self->thread_pool, data, NULL);
}

/* called on background thread */
static void load_in_background(void *p, void *unused)
{
	struct BackgroundLoadData *data = p;
	struct ComicReaderBackgroundImageLoader *self = data->self;

	debug_printf("loading index %zu in bg\n", data->item.index);

	data->item.image = self->inner_loader->get_image(self->inner_loader, data->item.index);
	g_idle_add(&finish_load_in_background, data);
}

static gboolean finish_load_in_background(void *p)
{
	struct BackgroundLoadData *data = p;
	struct ComicReaderBackgroundImageLoader *self = data->self;

	self->loading = false;
	add_to_cache(self, data->item);

	free(data);

	/* skip background load if disposing */
	if (self->ref_count >= 2)
		start_load_in_background(self);

	impl_free(&self->parent);

	return G_SOURCE_REMOVE;
}

static size_t get_prev_index(struct ComicReaderBackgroundImageLoader *self)
{
	size_t current = self->current_index;
	size_t num_images = impl_get_num_images(&self->parent);
	if (current == 0)
		return num_images - 1;
	else
		return (current - 1) % num_images;
}

static size_t get_next_index(struct ComicReaderBackgroundImageLoader *self)
{
	size_t num_images = impl_get_num_images(&self->parent);
	return (self->current_index + 1) % num_images;
}
