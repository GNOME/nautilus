#!/bin/sh

# Avoid compiling schemas when packaging.
if [ -z "$DESTDIR" ]
then
    glib-compile-schemas "${MESON_INSTALL_PREFIX}/share/glib-2.0/schemas"
fi
