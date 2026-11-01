#!/usr/bin/env bash

format_options=(--editor-config --recursive)
meson format "${format_options[@]}" --inplace
meson format meson.options "${format_options[@]}" --inplace
