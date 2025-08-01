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

#define NAUTILUS_TYPE_GTK_PLACES_SIDEBAR (nautilus_gtk_places_sidebar_get_type ())
G_DECLARE_FINAL_TYPE (NautilusGtkPlacesSidebar,
                      nautilus_gtk_places_sidebar,
                      NAUTILUS, GTK_PLACES_SIDEBAR,
                      GtkWidget)

GtkWidget *        nautilus_gtk_places_sidebar_new                        (void);

NautilusOpenFlags  nautilus_gtk_places_sidebar_get_open_flags             (NautilusGtkPlacesSidebar   *sidebar);
void               nautilus_gtk_places_sidebar_set_open_flags             (NautilusGtkPlacesSidebar   *sidebar,
                                                                           NautilusOpenFlags           flags);

GFile *            nautilus_gtk_places_sidebar_get_location               (NautilusGtkPlacesSidebar   *sidebar);
void               nautilus_gtk_places_sidebar_set_location               (NautilusGtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);

GFile *            nautilus_gtk_places_sidebar_get_nth_bookmark           (NautilusGtkPlacesSidebar   *sidebar,
                                                                  int                 n);
void               nautilus_gtk_places_sidebar_set_drop_targets_visible   (NautilusGtkPlacesSidebar   *sidebar,
                                                                  gboolean            visible);

void               nautilus_gtk_places_sidebar_set_show_trash             (NautilusGtkPlacesSidebar   *sidebar,
                                                                           gboolean                    show_trash);

/* Keep order, since it's used for the sort functions */
typedef enum {
  NAUTILUS_GTK_PLACES_SECTION_INVALID,
  NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
  NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS,
  NAUTILUS_GTK_PLACES_SECTION_CLOUD,
  NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
  NAUTILUS_GTK_PLACES_N_SECTIONS
} NautilusGtkPlacesSectionType;

typedef enum {
  NAUTILUS_GTK_PLACES_INVALID,
  NAUTILUS_GTK_PLACES_BUILT_IN,
  NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT,
  NAUTILUS_GTK_PLACES_INTERNAL_MOUNT,
  NAUTILUS_GTK_PLACES_NEW_BOOKMARK,
  NAUTILUS_GTK_PLACES_BOOKMARK,
  NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER,
  NAUTILUS_GTK_PLACES_N_PLACES
} NautilusGtkPlacesPlaceType;

char *nautilus_gtk_places_sidebar_get_location_title (NautilusGtkPlacesSidebar *sidebar);

G_END_DECLS
