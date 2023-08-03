FROM fedora:latest

RUN dnf install --nogpg -y dnf-plugins-core findutils git uncrustify \
 && dnf builddep --nogpg -y nautilus \
 && dnf clean all

RUN dnf builddep -y gtk4 \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/gtk.git \
 && cd gtk \
 && meson setup _build --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf gtk

 RUN dnf builddep -y libadwaita \
 && dnf install -y appstream-devel \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/libadwaita.git \
 && cd libadwaita \
 && meson setup _build --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf libadwaita

RUN dnf builddep -y glib \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/glib.git \
 && cd glib \
 && meson setup _build --prefix /usr \
 && ninja -C _build \
 && ninja install -C _build \
 && cd .. \
 && rm -rf glib
