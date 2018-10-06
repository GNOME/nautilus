#!/bin/sh

LANG=C.UTF-8 NO_AT_BRIDGE=1 dbus-run-session meson test -C _build
