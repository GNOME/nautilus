FROM fedora:rawhide

RUN dnf install -y dnf-plugins-core uncrustify findutils \
 && dnf builddep -y nautilus \
 && dnf clean all
