#!/bin/sh

exec meson setup --prefix="$(pwd)/builddir/inst" builddir .
