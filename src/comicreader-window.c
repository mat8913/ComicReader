/* comicreader-window.c
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

#include "config.h"

#include "comicreader-backgroundimageloader.h"
#include "comicreader-debug.h"
#include "comicreader-directoryimageloader.h"
#include "comicreader-imagedisplay.h"
#include "comicreader-window.h"

struct _ComicReaderWindow {
	AdwApplicationWindow parent_instance;

	/* Template widgets */
	AdwHeaderBar *header_bar;
	GtkLabel *title_label;
	GtkLabel *subtitle_label;
	GtkStack *stack;
	GtkScrolledWindow *scrolled_image;
	ComicReaderImageDisplay *displayed_image;

	/* Private fields */
	GSimpleAction *open_directory_action;
	GSimpleAction *close_comic_action;
	GSimpleAction *prev_page_action;
	GSimpleAction *next_page_action;
	struct ComicReaderImageLoader *image_loader;
	size_t image_idx;

	/* GestureZoom state */
	double start_scale;
	double scale_fixed_x;
	double scale_fixed_y;
	double start_scroll_hadj;
	double start_scroll_vadj;
};

G_DEFINE_FINAL_TYPE(ComicReaderWindow, comicreader_window, ADW_TYPE_APPLICATION_WINDOW)

static void comicreader_window_update_title(ComicReaderWindow *self);
static void key_released(ComicReaderWindow *self, guint kval, guint kcode, GdkModifierType state);
static void open_directory(ComicReaderWindow *self);
static void open_directory_callback(GObject *gobject, GAsyncResult *result, gpointer data);
static void close_comic(ComicReaderWindow *self);
static void set_image_loader(ComicReaderWindow *self, struct ComicReaderImageLoader *loader);
static void set_image_idx(ComicReaderWindow *self, size_t img_idx);
static void next_page(ComicReaderWindow *self);
static void prev_page(ComicReaderWindow *self);
static void scale_begin(GtkGesture *gesture, GdkEventSequence *sequence, ComicReaderWindow *self);
static void scale_changed(ComicReaderWindow *self, gdouble scale);
static void reset_scale_state(ComicReaderWindow *self);
static void scale_end(ComicReaderWindow *self, GdkEventSequence *sequence);
static void scale_cancel(ComicReaderWindow *self, GdkEventSequence *sequence);
static void comicreader_window_dispose(GObject *object);

static void comicreader_window_class_init(ComicReaderWindowClass *klass)
{
	g_type_ensure(COMICREADER_TYPE_IMAGEDISPLAY);

	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = comicreader_window_dispose;

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/name/mbekkema/ComicReader/comicreader-window.ui");
	gtk_widget_class_bind_template_child(widget_class, ComicReaderWindow, header_bar);
	gtk_widget_class_bind_template_child(widget_class, ComicReaderWindow, title_label);
	gtk_widget_class_bind_template_child(widget_class, ComicReaderWindow, subtitle_label);
	gtk_widget_class_bind_template_child(widget_class, ComicReaderWindow, stack);
	gtk_widget_class_bind_template_child(widget_class, ComicReaderWindow, scrolled_image);
	gtk_widget_class_bind_template_child(widget_class, ComicReaderWindow, displayed_image);
}

static void comicreader_window_init(ComicReaderWindow *self)
{
	debug_init("ComicReaderWindow", self);
	gtk_widget_init_template(GTK_WIDGET(self));

	self->open_directory_action = g_simple_action_new("open-directory", NULL);
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(self->open_directory_action));
	g_signal_connect_swapped(
		self->open_directory_action,
		"activate",
		G_CALLBACK(open_directory),
		self);

	self->close_comic_action = g_simple_action_new("close-comic", NULL);
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(self->close_comic_action));
	g_signal_connect_swapped(
		self->close_comic_action,
		"activate",
		G_CALLBACK(close_comic),
		self);
	g_simple_action_set_enabled(self->close_comic_action, false);

	self->next_page_action = g_simple_action_new("comic-next-page", NULL);
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(self->next_page_action));
	g_signal_connect_swapped(self->next_page_action, "activate", G_CALLBACK(next_page), self);

	self->prev_page_action = g_simple_action_new("comic-prev-page", NULL);
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(self->prev_page_action));
	g_signal_connect_swapped(self->prev_page_action, "activate", G_CALLBACK(prev_page), self);

	GtkEventController *controller = gtk_event_controller_key_new();
	g_signal_connect_swapped(controller, "key-released", G_CALLBACK(key_released), self);
	gtk_widget_add_controller(GTK_WIDGET(self), controller);

	GtkGesture *gesture = gtk_gesture_zoom_new();
	g_signal_connect(gesture, "begin", G_CALLBACK(scale_begin), self);
	g_signal_connect_swapped(gesture, "scale-changed", G_CALLBACK(scale_changed), self);
	g_signal_connect_swapped(gesture, "end", G_CALLBACK(scale_end), self);
	g_signal_connect_swapped(gesture, "cancel", G_CALLBACK(scale_cancel), self);
	gtk_widget_add_controller(GTK_WIDGET(self->scrolled_image), GTK_EVENT_CONTROLLER(gesture));

	gtk_window_set_title(GTK_WINDOW(self), "Comic Reader");
	comicreader_window_update_title(self);
}

static void comicreader_window_update_title(ComicReaderWindow *self)
{
	struct ComicReaderImage *image = comicreader_imagedisplay_get_image(self->displayed_image);
	const char *title = "Comic Reader";
	if (image)
		title = image->name;

	char subtitle[64] = {'\0'};
	if (self->image_loader) {
		snprintf(
			subtitle,
			sizeof(subtitle) / sizeof(subtitle[0]),
			"Page %zu of %zu",
			self->image_idx + 1,
			self->image_loader->get_num_images(self->image_loader));
	}

	gboolean has_subtitle = subtitle[0] != '\0';

	gtk_label_set_text(self->title_label, title);
	gtk_label_set_text(self->subtitle_label, subtitle);
	gtk_widget_set_visible(GTK_WIDGET(self->subtitle_label), has_subtitle);
}

static void key_released(ComicReaderWindow *self, guint kval, guint kcode, GdkModifierType state)
{
	switch (kval) {
	case GDK_KEY_minus: {
		double scale = comicreader_imagedisplay_get_scale(self->displayed_image);
		scale = fmax(0.1, scale - 0.1);
		comicreader_imagedisplay_set_scale(self->displayed_image, scale);
		break;
	}
	case GDK_KEY_plus: {
		double scale = comicreader_imagedisplay_get_scale(self->displayed_image);
		scale = fmin(5.0, scale + 0.1);
		comicreader_imagedisplay_set_scale(self->displayed_image, scale);
		break;
	}
	default:
		break;
	}
}

static void open_directory(ComicReaderWindow *self)
{
	GtkFileDialog *file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_select_folder(
		file_dialog,
		GTK_WINDOW(self),
		NULL,
		open_directory_callback,
		self);
}

static void open_directory_callback(GObject *gobject, GAsyncResult *result, gpointer data)
{
	ComicReaderWindow *self = COMICREADER_WINDOW(data);
	GtkFileDialog *file_dialog = GTK_FILE_DIALOG(gobject);

	GError *error = NULL;
	GFile *directory = gtk_file_dialog_select_folder_finish(file_dialog, result, &error);
	g_clear_object(&file_dialog);

	if (error) {
		/* TODO: don't crash on user dismissed dialog */
		abort_printf("Error: %i %s\n", error->code, error->message);
	}

	struct ComicReaderImageLoader *loader = comicreader_directory_image_loader_new(directory);
	loader = comicreader_background_image_loader_new(loader);
	set_image_loader(self, loader);
}

static void close_comic(ComicReaderWindow *self)
{
	set_image_loader(self, NULL);
}

static void set_image_loader(ComicReaderWindow *self, struct ComicReaderImageLoader *loader)
{
	comicreader_image_loader_clear(&self->image_loader);
	self->image_loader = loader;

	set_image_idx(self, 0);
	comicreader_imagedisplay_set_scale(self->displayed_image, 1);
	if (loader) {
		gtk_stack_set_visible_child_full(
			self->stack,
			"comic_view",
			GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
		g_simple_action_set_enabled(self->close_comic_action, true);
	} else {
		gtk_stack_set_visible_child_full(
			self->stack,
			"empty",
			GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
		g_simple_action_set_enabled(self->close_comic_action, false);
	}
}

static void set_image_idx(ComicReaderWindow *self, size_t img_idx)
{
	struct ComicReaderImage *image = NULL;
	if (self->image_loader) {
		img_idx = img_idx % self->image_loader->get_num_images(self->image_loader);
		image = self->image_loader->get_image(self->image_loader, img_idx);
	}
	comicreader_imagedisplay_set_image(self->displayed_image, image);
	self->image_idx = img_idx;
	comicreader_window_update_title(self);
}

static void next_page(ComicReaderWindow *self)
{
	set_image_idx(self, self->image_idx + 1);
}

static void prev_page(ComicReaderWindow *self)
{
	size_t idx = self->image_idx;
	if (idx == 0) {
		idx = self->image_loader->get_num_images(self->image_loader) - 1;
	} else {
		--idx;
	}

	set_image_idx(self, idx);
}

static void scale_begin(GtkGesture *gesture, GdkEventSequence *sequence, ComicReaderWindow *self)
{
	self->start_scale = comicreader_imagedisplay_get_scale(self->displayed_image);
	gboolean result = gtk_gesture_get_bounding_box_center(
		gesture,
		&self->scale_fixed_x,
		&self->scale_fixed_y);
	if (!result)
		abort_printf("gtk_gesture_get_bounding_box_center failed\n");

	GtkAdjustment *adj;
	adj = gtk_scrolled_window_get_hadjustment(self->scrolled_image);
	self->start_scroll_hadj = gtk_adjustment_get_value(adj);

	adj = gtk_scrolled_window_get_vadjustment(self->scrolled_image);
	self->start_scroll_vadj = gtk_adjustment_get_value(adj);
}

static void scale_changed(ComicReaderWindow *self, gdouble scale)
{
	if (self->start_scale == 0)
		return;
	comicreader_imagedisplay_set_scale(
		self->displayed_image,
		fmax(0.1, self->start_scale * scale));

	double sf = scale;
	GtkAdjustment *adj;
	adj = gtk_scrolled_window_get_hadjustment(self->scrolled_image);
	gtk_adjustment_set_value(
		adj,
		(self->start_scroll_hadj + self->scale_fixed_x) * sf - self->scale_fixed_x);

	adj = gtk_scrolled_window_get_vadjustment(self->scrolled_image);
	gtk_adjustment_set_value(
		adj,
		(self->start_scroll_vadj + self->scale_fixed_y) * sf - self->scale_fixed_y);
}

static void reset_scale_state(ComicReaderWindow *self)
{
	self->start_scale = 0;
	self->scale_fixed_x = 0;
	self->scale_fixed_y = 0;
	self->start_scroll_hadj = 0;
	self->start_scroll_vadj = 0;
}

static void scale_end(ComicReaderWindow *self, GdkEventSequence *sequence)
{
	if (self->start_scale == 0)
		return;
	reset_scale_state(self);
}

static void scale_cancel(ComicReaderWindow *self, GdkEventSequence *sequence)
{
	if (self->start_scale == 0)
		return;
	comicreader_imagedisplay_set_scale(self->displayed_image, self->start_scale);
	reset_scale_state(self);
}

static void comicreader_window_dispose(GObject *object)
{
	ComicReaderWindow *self = COMICREADER_WINDOW(object);

	g_clear_object(&self->open_directory_action);
	g_clear_object(&self->close_comic_action);
	g_clear_object(&self->prev_page_action);
	g_clear_object(&self->next_page_action);
	comicreader_image_loader_clear(&self->image_loader);

	debug_free("ComicReaderWindow", self);
	G_OBJECT_CLASS(comicreader_window_parent_class)->dispose(object);
}
