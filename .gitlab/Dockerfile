FROM fedora:rawhide

RUN dnf install -y dnf-plugins-core uncrustify findutils
 && dnf builddep -y nautilus --nogpgcheck
 && dnf clean all
