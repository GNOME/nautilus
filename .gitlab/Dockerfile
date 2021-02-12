FROM fedora:rawhide

RUN dnf install --nogpg -y dnf-plugins-core uncrustify findutils git libportal-devel tracker3-miners python3-gobject dbus-daemon libhandy1-devel \
 && dnf builddep --nogpg -y nautilus \
 && dnf clean all

RUN dnf install --nogpg -y gnome-common \
 && dnf builddep --nogpg -y gnome-autoar \
 && dnf clean all \
 && git clone --depth 1 https://gitlab.gnome.org/GNOME/gnome-autoar.git \
 && cd gnome-autoar \
 && ./autogen.sh --prefix /usr \
 && make \
 && make install \
 && cd .. \
 && rm -rf gnome-autoar
