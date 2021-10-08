FROM fedora:latest

RUN dnf install --nogpg -y dnf-plugins-core findutils git \
 && dnf builddep --nogpg -y uncrustify \
 && dnf clean all \
 && git clone --depth 1 https://github.com/uncrustify/uncrustify.git \
 && cd uncrustify \
 && mkdir build \
 && cd build \
 && cmake -DCMAKE_INSTALL_PREFIX=/usr .. \
 && make \
 && make install \
 && cd ../.. \
 && rm -rf uncrustify
