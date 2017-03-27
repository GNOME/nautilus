#!/bin/sh

if [ ! -f "${MESON_SOURCE_ROOT}/subprojects/libgd/meson.build" ]
then
    git \
        --git-dir="${MESON_SOURCE_ROOT}/.git" \
        submodule update --init subprojects/libgd
fi
