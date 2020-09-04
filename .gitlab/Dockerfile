FROM fedora:rawhide

RUN dnf install --nogpg -y dnf-plugins-core uncrustify findutils \
 && dnf builddep --nogpg -y nautilus \
 && dnf clean all
