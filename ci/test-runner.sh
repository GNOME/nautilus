#!/bin/sh

NO_AT_BRIDGE=1 dbus-run-session meson test -C _build
