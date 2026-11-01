#!/usr/bin/env bash

LANG=C.utf8

build-aux/format-blueprint.sh
build-aux/format-meson.sh
build-aux/run-uncrustify.sh

git diff --exit-code | tee style-check.diff
