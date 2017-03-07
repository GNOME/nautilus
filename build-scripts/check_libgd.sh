#!/bin/sh

if [ ! -f "${MESON_SOURCE_ROOT}/libgd/meson.build" ]
then
    git \
        --git-dir="${MESON_SOURCE_ROOT}/.git" \
        submodule update --init libgd
fi
