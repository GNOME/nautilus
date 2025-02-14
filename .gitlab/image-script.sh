#!/usr/bin/bash

dnf builddep -y glib \
    && git clone --depth 1 https://gitlab.gnome.org/GNOME/glib.git \
    && cd glib \
    && meson setup _build -Dintrospection=disabled -Dtests=false --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \

dnf builddep -y gobject-introspection \
    && git clone --depth 1 --recurse-submodules -j8 https://gitlab.gnome.org/GNOME/gobject-introspection.git \
    && cd gobject-introspection \
    && meson setup _build --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \
    && rm -rf gobject-introspection

cd glib \
    && git clean \
    && meson setup _build -Dtests=false --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \
    && rm -rf glib

dnf builddep -y --allowerasing gtk4 \
    && dnf install -y glslc \
    && git clone --depth 1 https://gitlab.gnome.org/GNOME/gtk.git \
    && cd gtk \
    && meson setup _build -Dbuild-tests=false -Dbuild-testsuite=false -Dbuild-demos=false -Dbuild-examples=false --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \
    && rm -rf gtk

dnf builddep -y libadwaita \
    && dnf install -y appstream-devel \
    && git clone --depth 1 https://gitlab.gnome.org/GNOME/libadwaita.git \
    && cd libadwaita \
    && meson setup _build -Dtests=false -Dexamples=false --prefix /usr \
    && ninja -C _build \
    && ninja install -C _build \
    && cd .. \
    && rm -rf libadwaita

dnf clean all
