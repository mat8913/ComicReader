/* comicreader-directoryimageloader.c
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

#include "comicreader-directoryimageloader.h"
#include "comicreader-debug.h"

struct ComicReaderDirectoryImageLoader {
	struct ComicReaderImageLoader parent;
	GFile *directory;
	char **child_filenames;
	size_t child_filenames_length;
};

/* interface implementations */
static size_t impl_get_num_images(struct ComicReaderImageLoader *image_loader);
static struct ComicReaderImage *impl_get_image(
	struct ComicReaderImageLoader *image_loader,
	size_t index);
static void impl_free(struct ComicReaderImageLoader *image_loader);

/* helper functions */
static void strarray_append(char ***strarray, size_t *length, size_t *capacity, char *str);
static int strcmpp(const void *str1p, const void *str2p);

struct ComicReaderImageLoader *comicreader_directory_image_loader_new(GFile *directory)
{
	struct ComicReaderDirectoryImageLoader *ret;
	ret = calloc(1, sizeof(struct ComicReaderDirectoryImageLoader));
	debug_init("ComicReaderDirectoryImageLoader", ret);

	ret->parent.free = impl_free;
	ret->parent.get_num_images = impl_get_num_images;
	ret->parent.get_image = impl_get_image;

	ret->directory = directory;

	GError *error = NULL;
	GFileEnumerator *direnum = g_file_enumerate_children(
		directory,
		G_FILE_ATTRIBUTE_STANDARD_NAME,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&error);
	if (error) {
		abort_printf("Error: %i %s\n", error->code, error->message);
	}

	char **filenames = NULL;
	size_t filenames_length = 0;
	size_t filenames_capacity = 0;

	for (;;) {
		GFileInfo *info;
		if (!g_file_enumerator_iterate(direnum, &info, NULL, NULL, &error)) {
			abort_printf("Error: %i %s\n", error->code, error->message);
		}
		if (!info)
			break;
		const char *name =
			g_file_info_get_attribute_file_path(info, G_FILE_ATTRIBUTE_STANDARD_NAME);
		strarray_append(&filenames, &filenames_length, &filenames_capacity, strdup(name));
	}

	filenames = reallocarray(filenames, filenames_length, sizeof(char *));

	qsort(filenames, filenames_length, sizeof(char *), strcmpp);

	ret->child_filenames = filenames;
	ret->child_filenames_length = filenames_length;

	g_assert((void *)ret == (void *)&ret->parent);
	return &ret->parent;
}

static size_t impl_get_num_images(struct ComicReaderImageLoader *image_loader)
{
	struct ComicReaderDirectoryImageLoader *self =
		(struct ComicReaderDirectoryImageLoader *)image_loader;
	return self->child_filenames_length;
}

static struct ComicReaderImage *impl_get_image(
	struct ComicReaderImageLoader *image_loader,
	size_t index)
{
	struct ComicReaderDirectoryImageLoader *self =
		(struct ComicReaderDirectoryImageLoader *)image_loader;

	if (index >= self->child_filenames_length)
		abort_printf("index out of range\n");

	const char *filename = self->child_filenames[index];

	struct ComicReaderImage *ret = calloc(1, sizeof(struct ComicReaderImage));
	ret->name = strdup(filename);
	ret->texture = NULL;
	ret->error = NULL;

	GFile *file = g_file_get_child(self->directory, filename);
	GError *error = NULL;
	ret->texture = gdk_texture_new_from_file(file, &error);
	if (error) {
		size_t sz = snprintf(NULL, 0, "%s (%i)", error->message, error->code) + 1;
		ret->error = calloc(sz, sizeof(char));
		snprintf(ret->error, sz, "%s (%i)", error->message, error->code);
		g_error_free(error);
	}
	g_clear_object(&file);

	return ret;
}

static void impl_free(struct ComicReaderImageLoader *image_loader)
{
	struct ComicReaderDirectoryImageLoader *self =
		(struct ComicReaderDirectoryImageLoader *)image_loader;
	g_clear_object(&self->directory);
	for (size_t i = 0; i < self->child_filenames_length; ++i) {
		free(self->child_filenames[i]);
	}
	free(self->child_filenames);
	debug_free("ComicReaderDirectoryImageLoader", self);
	free(image_loader);
}

static void strarray_append(char ***strarray, size_t *length, size_t *capacity, char *str)
{
	if (*length == *capacity) {
		size_t new_capacity = MAX(1, *capacity * 2);
		*strarray = reallocarray(*strarray, new_capacity, sizeof(const char *));
		*capacity = new_capacity;
	}

	(*strarray)[*length] = str;
	++*length;
}

static int strcmpp(const void *str1p, const void *str2p)
{
	return strcmp(*(const char **)str1p, *(const char **)str2p);
}
