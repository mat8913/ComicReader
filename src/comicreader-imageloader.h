/* comicreader-imageloader.h
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

#pragma once

#include <gdk/gdk.h>

struct ComicReaderImage {
	char *name;
	char *error;
	GdkTexture *texture;
};

struct ComicReaderImageLoader {
	size_t (*get_num_images)(struct ComicReaderImageLoader *self);
	struct ComicReaderImage *(*get_image)(struct ComicReaderImageLoader *self, size_t index);
	void (*free)(struct ComicReaderImageLoader *self);
};

void comicreader_image_clear(struct ComicReaderImage **image);
struct ComicReaderImage *comicreader_image_dup(struct ComicReaderImage *image);

void comicreader_image_loader_clear(struct ComicReaderImageLoader **image_loader);
