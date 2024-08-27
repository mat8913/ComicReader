/* comicreader-imagedisplay.c
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

#include "comicreader-imagedisplay.h"
#include "comicreader-debug.h"

struct _ComicReaderImageDisplay {
	GtkWidget parent_instance;

	double scale_factor;
	struct ComicReaderImage *image;
};

G_DEFINE_FINAL_TYPE(ComicReaderImageDisplay, comicreader_imagedisplay, GTK_TYPE_WIDGET)

static void comicreader_imagedisplay_update_size_request(ComicReaderImageDisplay *self);
static void comicreader_imagedisplay_snapshot(GtkWidget *widget, GtkSnapshot *snapshot);
static void comicreader_imagedisplay_dispose(GObject *object);

static void comicreader_imagedisplay_class_init(ComicReaderImageDisplayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = comicreader_imagedisplay_dispose;

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	widget_class->snapshot = comicreader_imagedisplay_snapshot;
}

static void comicreader_imagedisplay_init(ComicReaderImageDisplay *self)
{
	debug_init("ComicReaderImageDisplay", self);
	self->scale_factor = 1;
}

double comicreader_imagedisplay_get_scale(ComicReaderImageDisplay *self)
{
	return self->scale_factor;
}

void comicreader_imagedisplay_set_scale(ComicReaderImageDisplay *self, double scale_factor)
{
	self->scale_factor = scale_factor;
	comicreader_imagedisplay_update_size_request(self);
}

struct ComicReaderImage *comicreader_imagedisplay_get_image(ComicReaderImageDisplay *self)
{
	return self->image;
}

void comicreader_imagedisplay_set_image(
	ComicReaderImageDisplay *self,
	struct ComicReaderImage *image)
{
	comicreader_image_clear(&self->image);
	self->image = image;
	comicreader_imagedisplay_update_size_request(self);
}

static void comicreader_imagedisplay_update_size_request(ComicReaderImageDisplay *self)
{
	double width = 1;
	double height = 1;
	if (self->image && !self->image->error) {
		width = gdk_texture_get_width(self->image->texture) * self->scale_factor;
		height = gdk_texture_get_height(self->image->texture) * self->scale_factor;
	}
	gtk_widget_set_size_request(GTK_WIDGET(self), width, height);
	gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void comicreader_imagedisplay_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
	ComicReaderImageDisplay *self = COMICREADER_IMAGEDISPLAY(widget);

	if (self->image) {
		if (self->image->error) {
			fprintf(stderr, "%s\n", self->image->error);  /* TODO: display error */
			return;
		}
		double width = gdk_texture_get_width(self->image->texture) * self->scale_factor;
		double height = gdk_texture_get_height(self->image->texture) * self->scale_factor;
		gdk_paintable_snapshot(
			GDK_PAINTABLE(self->image->texture),
			snapshot,
			width,
			height);
	}
}

static void comicreader_imagedisplay_dispose(GObject *object)
{
	ComicReaderImageDisplay *self = COMICREADER_IMAGEDISPLAY(object);

	comicreader_image_clear(&self->image);

	debug_free("ComicReaderImageDisplay", self);
	G_OBJECT_CLASS(comicreader_imagedisplay_parent_class)->dispose(object);
}
