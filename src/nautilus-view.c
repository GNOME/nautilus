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
    /**
     * NautilusView::loading:
     *
     * %TRUE if the view is loading the location, %FALSE otherwise.
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_boolean ("loading",
                                                               "Current view is loading",
                                                               "Whether the current view is loading the location or not",
                                                               FALSE,
                                                               G_PARAM_READABLE));

    /**
     * NautilusView::searching:
     *
     * %TRUE if the view is searching, %FALSE otherwise.
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_boolean ("searching",
                                                               "Current view is searching",
                                                               "Whether the current view is searching or not",
                                                               FALSE,
                                                               G_PARAM_READABLE));

    /**
     * NautilusView::location:
     *
     * The current location of the view.
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("location",
                                                              "Location displayed by the view",
                                                              "The current location displayed by the view",
                                                              G_TYPE_FILE,
                                                              G_PARAM_READWRITE));

    /**
     * NautilusView::selection:
     *
     * The current selection of the view.
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_pointer ("selection",
                                                               "Selection of the view",
                                                               "The current selection of the view",
                                                               G_PARAM_READWRITE));

    /**
     * NautilusView::search-query:
     *
     * The search query being performed, or NULL.
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("search-query",
                                                              "Search query being performed",
                                                              "The search query being performed on the view",
                                                              NAUTILUS_TYPE_QUERY,
                                                              G_PARAM_READWRITE));

    /**
     * NautilusView::extensions-background-menu:
     *
     * Menu for the background click of extensions
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("extensions-background-menu",
                                                              "Menu for the background click of extensions",
                                                              "Menu for the background click of extensions",
                                                              G_TYPE_MENU_MODEL,
                                                              G_PARAM_READWRITE));
    /**
     * NautilusView::templates-menu:
     *
     * Menu of templates
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("templates-menu",
                                                              "Menu of templates",
                                                              "Menu of templates",
                                                              G_TYPE_MENU_MODEL,
                                                              G_PARAM_READWRITE));
}

/**
 * nautilus_view_get_icon_name:
 * @view: a #NautilusView
 *
 * Retrieves the icon name that represents @view.
 *
 * Returns: (transfer none): an icon name
 */
const gchar *
nautilus_view_get_icon_name (guint view_id)
{
    if (view_id == NAUTILUS_VIEW_GRID_ID)
    {
        return "view-grid-symbolic";
    }
    else if (view_id == NAUTILUS_VIEW_LIST_ID)
    {
        return "view-list-symbolic";
    }
    else
    {
        return NULL;
    }
}

/**
 * nautilus_view_get_tooltip:
 * @view: a #NautilusView
 *
 * Retrieves the static string that represents @view.
 *
 * Returns: (transfer none): a static string
 */
const gchar *
nautilus_view_get_tooltip (guint view_id)
{
    if (view_id == NAUTILUS_VIEW_GRID_ID)
    {
        return _("Grid View");
    }
    else if (view_id == NAUTILUS_VIEW_LIST_ID)
    {
        return _("List View");
    }
    else
    {
        return NULL;
    }
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

GMenuModel *
nautilus_view_get_extensions_background_menu (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_extensions_background_menu, NULL);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_extensions_background_menu (view);
}

/* Protected */
void
nautilus_view_set_extensions_background_menu (NautilusView *view,
                                              GMenuModel   *menu)
{
    g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->set_extensions_background_menu);

    NAUTILUS_VIEW_GET_IFACE (view)->set_extensions_background_menu (view, menu);
}

GMenuModel *
nautilus_view_get_templates_menu (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_templates_menu, NULL);

    return NAUTILUS_VIEW_GET_IFACE (view)->get_templates_menu (view);
}

/* Protected */
void
nautilus_view_set_templates_menu (NautilusView *view,
                                  GMenuModel   *menu)
{
    g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->set_templates_menu);

    NAUTILUS_VIEW_GET_IFACE (view)->set_templates_menu (view, menu);
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
