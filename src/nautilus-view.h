/* nautilus-view.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-query.h"
#include "nautilus-toolbar-menu-sections.h"
#include "nautilus-view-info.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW (nautilus_view_get_type ())

G_DECLARE_INTERFACE (NautilusView, nautilus_view, NAUTILUS, VIEW, GtkWidget)

struct _NautilusViewInterface
{
        GTypeInterface parent;

        guint                           (*get_view_id)               (NautilusView         *view);
        /*
         * Returns the menu sections that should be shown in the toolbar menu
         * when this view is active. Implementations must not return %NULL
         */
        NautilusToolbarMenuSections *   (*get_toolbar_menu_sections) (NautilusView         *view);

        /* Current location of the view */
        GFile*                          (*get_location)              (NautilusView         *view);
        void                            (*set_location)              (NautilusView         *view,
                                                                      GFile                *location);

        /* Selection */
        GList*                          (*get_selection)             (NautilusView         *view);
        void                            (*set_selection)             (NautilusView         *view,
                                                                      GList                *selection);

        /* Search */
        NautilusQuery*                  (*get_search_query)          (NautilusView         *view);
        void                            (*set_search_query)          (NautilusView         *view,
                                                                      NautilusQuery        *query);

        /* Whether the current view is loading the location */
        gboolean                        (*is_loading)                (NautilusView         *view);

        /* Whether the current view is searching or not */
        gboolean                        (*is_searching)              (NautilusView         *view);
};

guint                          nautilus_view_get_view_id               (NautilusView         *view);

NautilusToolbarMenuSections *  nautilus_view_get_toolbar_menu_sections (NautilusView         *view);

GFile *                        nautilus_view_get_location              (NautilusView         *view);

void                           nautilus_view_set_location              (NautilusView         *view,
                                                                        GFile                *location);

GList *                        nautilus_view_get_selection             (NautilusView         *view);

void                           nautilus_view_set_selection             (NautilusView         *view,
                                                                        GList                *selection);

NautilusQuery *                nautilus_view_get_search_query          (NautilusView         *view);

void                           nautilus_view_set_search_query          (NautilusView         *view,
                                                                        NautilusQuery        *query);

gboolean                       nautilus_view_is_loading                (NautilusView         *view);

gboolean                       nautilus_view_is_searching              (NautilusView         *view);

G_END_DECLS
