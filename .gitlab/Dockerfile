FROM fedora:rawhide

RUN dnf install --nogpg -y dnf-plugins-core uncrustify findutils git \
 && dnf builddep --nogpg -y nautilus \
 && dnf clean all

RUN dnf copr enable -y ssssam/tracker3 \
 && dnf install --nogpg -y tracker3 tracker3-miners tracker3-devel python3-tracker3-testutils python3-gobject dbus-daemon \
 && dnf clean all
