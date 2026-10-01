#!/usr/bin/env bash

format_options=(--editor-config --recursive)
meson format "${format_options[@]}" --check-diff || (
    echo "meson.build files do not match the expected format"
    echo "run 'meson format ${format_options[*]} --inplace' to apply the standard formatting"
    exit 1
)
meson format meson.options "${format_options[@]}" --check-diff || (
    echo "meson.build files do not match the expected format"
    echo "run 'meson format meson.options ${format_options[*]} --inplace' to apply the standard formatting"
    exit 1
)
