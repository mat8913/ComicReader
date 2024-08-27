#!/bin/sh

exec flatpak --user run --filesystem="$(pwd)" --command=bash --runtime=org.gnome.Sdk//46 name.mbekkema.ComicReader
