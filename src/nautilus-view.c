/* nautilus-view.c
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

#include "config.h"

#include "nautilus-view.h"
#include <glib/gi18n.h>

G_DEFINE_INTERFACE (NautilusView, nautilus_view, GTK_TYPE_WIDGET)

static void
nautilus_view_default_init (NautilusViewInterface *iface)
{
}

/**
 * nautilus_view_get_view_id:
 * @view: a #NautilusView
 *
 * Retrieves the view id that represents the @view type.
 *
 * Returns: a guint representing the view type
 */
guint
nautilus_view_get_view_id (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_view_id, NAUTILUS_VIEW_INVALID_ID);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_view_id (view);
}

/**
 * nautilus_view_get_toolbar_menu_sections:
 * @view: a #NautilusView
 *
 * Retrieves the menu sections to show in the main toolbar menu when this view
 * is active
 *
 * Returns: (transfer none): a #NautilusToolbarMenuSections with the sections to
 * be displayed
 */
NautilusToolbarMenuSections *
nautilus_view_get_toolbar_menu_sections (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_toolbar_menu_sections, NULL);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_toolbar_menu_sections (view);
}

/**
 * nautilus_view_get_search_query:
 * @view: a #NautilusView
 *
 * Retrieves the current current location of @view.
 *
 * Returns: (transfer none): a #GFile
 */
GFile *
nautilus_view_get_location (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_location, NULL);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_location (view);
}

/**
 * nautilus_view_set_location:
 * @view: a #NautilusView
 * @location: the location displayed by @view
 *
 * Sets the location of @view.
 *
 * Returns:
 */
void
nautilus_view_set_location (NautilusView *view,
                            GFile        *location)
{
    g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->set_location);

    NAUTILUS_VIEW_GET_IFACE (view)->set_location (view, location);
}

/**
 * nautilus_view_get_selection:
 * @view: a #NautilusView
 *
 * Get the current selection of the view.
 *
 * Returns: (transfer full) (type GFile): a newly allocated list
 * of the currently selected files.
 */
GList *
nautilus_view_get_selection (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_selection, NULL);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_selection (view);
}

/**
 * nautilus_view_set_selection:
 * @view: a #NautilusView
 * @selection: (nullable): a list of files
 *
 * Sets the current selection of the view.
 *
 * Returns:
 */
void
nautilus_view_set_selection (NautilusView *view,
                             GList        *selection)
{
    g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->set_selection);

    NAUTILUS_VIEW_GET_IFACE (view)->set_selection (view, selection);
}

/**
 * nautilus_view_get_search_query:
 * @view: a #NautilusView
 *
 * Retrieves the current search query displayed by @view.
 *
 * Returns: (transfer none): a #
 */
NautilusQuery *
nautilus_view_get_search_query (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_search_query, NULL);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_search_query (view);
}

/**
 * nautilus_view_set_search_query:
 * @view: a #NautilusView
 * @query: the search query to be performed, or %NULL
 *
 * Sets the current search query performed by @view.
 *
 * Returns:
 */
void
nautilus_view_set_search_query (NautilusView  *view,
                                NautilusQuery *query)
{
    g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->set_search_query);

    NAUTILUS_VIEW_GET_IFACE (view)->set_search_query (view, query);
}

/**
 * nautilus_view_is_loading:
 * @view: a #NautilusView
 *
 * Whether @view is loading the current location.
 *
 * Returns: %TRUE if @view is loading, %FALSE otherwise.
 */
gboolean
nautilus_view_is_loading (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->is_loading, FALSE);

    return NAUTILUS_VIEW_GET_IFACE (view)->is_loading (view);
}

/**
 * nautilus_view_is_searching:
 * @view: a #NautilusView
 *
 * Whether @view is searching.
 *
 * Returns: %TRUE if @view is searching, %FALSE otherwise.
 */
gboolean
nautilus_view_is_searching (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->is_searching, FALSE);

    return NAUTILUS_VIEW_GET_IFACE (view)->is_searching (view);
}
