/* comicreader-imageloader.c
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

#include "comicreader-imageloader.h"

#include <stdlib.h>

void comicreader_image_clear(struct ComicReaderImage **image)
{
	if (*image) {
		if ((*image)->name)
			free((*image)->name);
		if ((*image)->error)
			free((*image)->error);
		g_clear_object(&(*image)->texture);
		free(*image);
		*image = NULL;
	}
}

struct ComicReaderImage *comicreader_image_dup(struct ComicReaderImage *image)
{
	if (!image)
		return NULL;

	struct ComicReaderImage *image2 = calloc(1, sizeof(struct ComicReaderImage));
	if (image->name)
		image2->name = strdup(image->name);
	if (image->error)
		image2->error = strdup(image->error);
	if (image->texture) {
		g_object_ref(image->texture);
		image2->texture = image->texture;
	}

	return image2;
}

void comicreader_image_loader_clear(struct ComicReaderImageLoader **image_loader)
{
	if (*image_loader) {
		(*image_loader)->free(*image_loader);
		*image_loader = NULL;
	}
}
