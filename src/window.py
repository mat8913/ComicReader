# window.py
#
# Copyright 2020 Matthew Harm Bekkema
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from gi.repository import Gtk, Gdk, GdkPixbuf
from gi.repository.GdkPixbuf import InterpType
import cairo


@Gtk.Template(resource_path='/name/mbekkema/ComicReader/window.ui')
class ComicReaderWindow(Gtk.ApplicationWindow):
    __gtype_name__ = 'ComicReaderWindow'

    displayed_image = Gtk.Template.Child()
    scrolled_image = Gtk.Template.Child()
    next_button = Gtk.Template.Child()
    prev_button = Gtk.Template.Child()
    header_bar = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.displayed_image.connect("draw", self.draw_displayed_image)
        self.displayed_image.connect("configure-event", self.resize_displayed_image)
        self.scrolled_image.connect("realize", self.realize_scrolled_image)

        self.connect("key-release-event", self.key_press)
        self.zoom = 100
        self.start_zoom = None
        self.start_adj = None
        self.fixed_point = None
        self.sf = 1
        self.gesture_zoom = Gtk.GestureZoom(widget=self.scrolled_image)
        self.gesture_zoom.connect("scale-changed", self.change_scale)
        self.gesture_zoom.connect("begin", self.begin_scale)
        self.gesture_zoom.connect("end", self.end_scale)
        self.gesture_zoom.connect("cancel", self.cancel_scale)
        self.next_button.connect("clicked", self.load_next_image)
        self.prev_button.connect("clicked", self.load_prev_image)

        self.iw = 1
        self.ih = 1
        self.csurface = cairo.ImageSurface(cairo.Format.RGB24, 1, 1)

        self.image_ix = None
        self.images = None

    def load_next_image(self, widget):
        self.set_image_ix(self.image_ix + 1)

    def load_prev_image(self, widget):
        self.set_image_ix(self.image_ix - 1)

    def resize_displayed_image(self, widget, event):
        if self.start_zoom is None:
            return
        sf = self.zoom / self.start_zoom
        self.scrolled_image.get_hadjustment().set_value((self.fixed_point[0] + self.start_adj[0]) * sf - self.fixed_point[0])
        self.scrolled_image.get_vadjustment().set_value((self.fixed_point[1] + self.start_adj[1]) * sf - self.fixed_point[1])

    def begin_scale(self, controller, seq):
        self.start_zoom = self.zoom
        c = controller.get_bounding_box_center()
        self.fixed_point = (c.x, c.y)
        self.start_adj = (self.scrolled_image.get_hadjustment().get_value(), self.scrolled_image.get_vadjustment().get_value())

    def end_scale(self, controller, seq):
        self.start_zoom = None
        self.fixed_point = None
        self.start_adj = None

    def cancel_scale(self, controller, seq):
        self.zoom = self.start_zoom
        self.sf = self.zoom / 100
        self.displayed_image.set_size_request(self.iw * self.sf, self.ih * self.sf)
        self.start_zoom = None
        self.fixed_point = None
        self.start_adj = None

    def change_scale(self, controller, scale):
        self.zoom = scale * self.start_zoom
        if self.zoom < 1:
            self.zoom = 1
        self.sf = self.zoom / 100
        self.displayed_image.set_size_request(self.iw * self.sf, self.ih * self.sf)

    def realize_scrolled_image(self, widget):
        self.gesture_zoom.set_window(widget.get_window())

    def draw_displayed_image(self, widget, cr):
        sf = self.sf

        cr.scale(sf, sf)
        cr.set_source_surface(self.csurface, 0, 0)
        cr.paint()
        return False

    def key_press(self, widget, key):
        if key.keyval == Gdk.KEY_minus:
            self.zoom -= 1
        elif key.keyval == Gdk.KEY_equal:
            self.zoom += 1
        else:
            return
        sf = self.zoom / 100
        self.sf = sf
        self.displayed_image.set_size_request(self.iw * sf, self.ih * sf)

    def set_images(self, images):
        assert(len(images) > 0)
        self.images = images
        self.set_image_ix(0)

    def set_image(self, image):
        stream = image.read(None)

        pixbuf = GdkPixbuf.Pixbuf.new_from_stream(stream)
        self.iw = pixbuf.get_width()
        self.ih = pixbuf.get_height()
        self.displayed_image.set_size_request(self.iw * self.sf, self.ih * self.sf)

        self.csurface = cairo.ImageSurface(cairo.Format.RGB24, self.iw, self.ih)
        ctx = cairo.Context(self.csurface)
        Gdk.cairo_set_source_pixbuf(ctx, pixbuf, 0, 0)
        ctx.paint()

        self.displayed_image.queue_draw()

    def set_image_ix(self, ix):
        self.image_ix = ix % len(self.images)
        self.header_bar.set_title(self.images[self.image_ix][0])
        self.header_bar.set_subtitle("Page {} of {}".format(self.image_ix + 1, len(self.images)))
        self.set_image(self.images[self.image_ix][1])
