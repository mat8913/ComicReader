# main.py
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

import sys
import gi

gi.require_version('Gtk', '3.0')

from gi.repository import Gtk, Gio, GLib

from .window import ComicReaderWindow


class Application(Gtk.Application):
    def __init__(self):
        super().__init__(application_id='name.mbekkema.ComicReader',
                         flags=Gio.ApplicationFlags.HANDLES_OPEN)

    def do_activate(self):
        fc = Gtk.FileChooserNative(title="Select comic folder", action=Gtk.FileChooserAction.SELECT_FOLDER)
        res = fc.run()
        if res == Gtk.ResponseType.ACCEPT:
            f = fc.get_file()
            self.do_open([f], 1, None)

    def do_open(self, fs, n, hints):
        assert(n == 1)
        fn_dict = {}
        win = self.props.active_window
        if not win:
            win = ComicReaderWindow(application=self)
            win.present()
        cs = fs[0].enumerate_children("standard::name", Gio.FileQueryInfoFlags.NONE, None)
        for c in cs:
            sn = c.get_attribute_byte_string("standard::name")
            fn_dict[sn] = fs[0].get_child(sn)
        win.set_images(sorted(fn_dict.items(), key=collate_key))


def main(version):
    app = Application()
    return app.run(sys.argv)


def collate_key(x):
    return GLib.utf8_collate_key_for_filename(x[0], -1)
