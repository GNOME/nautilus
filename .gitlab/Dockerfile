FROM fedora:rawhide

RUN dnf install --nogpg -y dnf-plugins-core uncrustify findutils git libportal-devel tracker3-miners python3-gobject dbus-daemon \
 && dnf builddep --nogpg -y nautilus \
 && dnf clean all

