/*
 * Copyright (C) 2015 Red Hat
 *
 * SPDX-License-Identifier: GPL-2.1-or-later
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */
#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-enums.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PLACES_SIDEBAR (nautilus_sidebar_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSidebar,
                      nautilus_sidebar,
                      NAUTILUS, PLACES_SIDEBAR,
                      GtkWidget)

GtkWidget *        nautilus_sidebar_new                        (void);

NautilusOpenFlags  nautilus_sidebar_get_open_flags             (NautilusSidebar   *sidebar);
void               nautilus_sidebar_set_open_flags             (NautilusSidebar   *sidebar,
                                                                           NautilusOpenFlags           flags);

GFile *            nautilus_sidebar_get_location               (NautilusSidebar   *sidebar);
void               nautilus_sidebar_set_location               (NautilusSidebar   *sidebar,
                                                                  GFile              *location);

GFile *            nautilus_sidebar_get_nth_bookmark           (NautilusSidebar   *sidebar,
                                                                  int                 n);
void               nautilus_sidebar_set_drop_targets_visible   (NautilusSidebar   *sidebar,
                                                                  gboolean            visible);

void               nautilus_sidebar_set_show_trash             (NautilusSidebar   *sidebar,
                                                                           gboolean                    show_trash);

char *nautilus_sidebar_get_location_title (NautilusSidebar *sidebar);

G_END_DECLS
