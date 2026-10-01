#!/usr/bin/env bash

srcdir="src"
find "$srcdir" -name '*.blp' -exec blueprint-compiler format "$@" {} +
