FROM fedora:latest

ENV CFLAGS "-fno-omit-frame-pointer"

RUN dnf install --nogpg -y dnf-plugins-core findutils git uncrustify \
 && dnf builddep --nogpg -y nautilus \
 && dnf clean all

RUN dnf builddep -y glib \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/glib.git \
 && cd glib \
 && meson setup _build -Dintrospection=disabled -Dtests=false --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf glib

RUN dnf builddep -y gobject-introspection \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/gobject-introspection.git \
 && cd gobject-introspection \
 && meson setup _build --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf gobject-introspection

RUN dnf builddep -y glib \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/glib.git \
 && cd glib \
 && meson setup _build -Dtests=false --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf glib

RUN dnf builddep -y gtk4 \
 && dnf install -y glslc \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/gtk.git \
 && cd gtk \
 && meson setup _build -Dbuild-tests=false -Dbuild-testsuite=false -Dbuild-demos=false -Dbuild-examples=false --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf gtk

 RUN dnf builddep -y libadwaita \
 && dnf install -y appstream-devel \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/libadwaita.git \
 && cd libadwaita \
 && meson setup _build -Dtests=false -Dexamples=false --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf libadwaita

RUN dnf install -y libasan \
&& dnf clean all \
