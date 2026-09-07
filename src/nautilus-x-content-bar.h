/*
 * SPDX-FileCopyrightText: 2008 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Paolo Borelli <pborelli@katamail.com>
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_X_CONTENT_BAR (nautilus_x_content_bar_get_type ())

G_DECLARE_FINAL_TYPE (NautilusXContentBar, nautilus_x_content_bar, NAUTILUS, X_CONTENT_BAR, AdwBin)

GtkWidget *nautilus_x_content_bar_new (GMount             *mount,
                                       const char * const *x_content_types);

G_END_DECLS
