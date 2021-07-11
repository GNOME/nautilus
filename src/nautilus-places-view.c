/* nautilus-places-view.c
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

#include "nautilus-mime-actions.h"
#include "nautilus-places-view.h"

#include "gtk/nautilusgtkplacesviewprivate.h"

#include "nautilus-application.h"
#include "nautilus-file.h"
#include "nautilus-toolbar-menu-sections.h"
#include "nautilus-view.h"
#include "nautilus-window-slot.h"

typedef struct
{
    GFile *location;
    NautilusQuery *search_query;
    NautilusToolbarMenuSections *toolbar_menu_sections;

    GtkWidget *places_view;
} NautilusPlacesViewPrivate;

struct _NautilusPlacesView
{
    GtkFrameClass parent;
};

static void          nautilus_places_view_iface_init (NautilusViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesView, nautilus_places_view, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (NautilusPlacesView)
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_VIEW, nautilus_places_view_iface_init));

enum
{
    PROP_0,
    PROP_LOCATION,
    PROP_SEARCH_QUERY,
    PROP_LOADING,
    PROP_SEARCHING,
    PROP_SELECTION,
    PROP_EXTENSIONS_BACKGROUND_MENU,
    PROP_TEMPLATES_MENU,
    LAST_PROP
};

static void
open_location_cb (NautilusPlacesView *view,
                  GFile              *location,
                  GtkPlacesOpenFlags  open_flags)
{
    NautilusWindowOpenFlags flags;
    GtkWidget *slot;

    slot = gtk_widget_get_ancestor (GTK_WIDGET (view), NAUTILUS_TYPE_WINDOW_SLOT);

    switch (open_flags)
    {
        case GTK_PLACES_OPEN_NEW_TAB:
        {
            flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB |
                    NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE;
        }
        break;

        case GTK_PLACES_OPEN_NEW_WINDOW:
        {
            flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
        }
        break;

        case GTK_PLACES_OPEN_NORMAL: /* fall-through */
        default:
        {
            flags = 0;
        }
        break;
    }

    if (slot)
    {
        NautilusFile *file;
        GtkWidget *window;
        char *path;

        path = "other-locations:///";
        file = nautilus_file_get (location);
        window = gtk_widget_get_toplevel (GTK_WIDGET (view));

        nautilus_mime_activate_file (GTK_WINDOW (window),
                                     NAUTILUS_WINDOW_SLOT (slot),
                                     file,
                                     path,
                                     flags);
        nautilus_file_unref (file);
    }
}

static void
loading_cb (NautilusView *view)
{
    g_object_notify (G_OBJECT (view), "loading");
}

static void
show_error_message_cb (NautilusGtkPlacesView *view,
                       const gchar           *primary,
                       const gchar           *secondary)
{
    GtkWidget *dialog;
    GtkWidget *window;

    window = gtk_widget_get_toplevel (GTK_WIDGET (view));

    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "%s", primary);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", secondary);

    gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);
}

static void
nautilus_places_view_finalize (GObject *object)
{
    NautilusPlacesView *self = (NautilusPlacesView *) object;
    NautilusPlacesViewPrivate *priv = nautilus_places_view_get_instance_private (self);

    g_clear_object (&priv->location);
    g_clear_object (&priv->search_query);

    g_free (priv->toolbar_menu_sections);

    G_OBJECT_CLASS (nautilus_places_view_parent_class)->finalize (object);
}

static void
nautilus_places_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
    NautilusView *view = NAUTILUS_VIEW (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            g_value_set_object (value, nautilus_view_get_location (view));
        }
        break;

        case PROP_SEARCH_QUERY:
        {
            g_value_set_object (value, nautilus_view_get_search_query (view));
        }
        break;

        /* Collect all unused properties and do nothing. Ideally, this wouldn’t
         * have to be done in the first place.
         */
        case PROP_SEARCHING:
        case PROP_SELECTION:
        case PROP_EXTENSIONS_BACKGROUND_MENU:
        case PROP_TEMPLATES_MENU:
        {
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_places_view_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    NautilusView *view = NAUTILUS_VIEW (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            nautilus_view_set_location (view, g_value_get_object (value));
        }
        break;

        case PROP_SEARCH_QUERY:
        {
            nautilus_view_set_search_query (view, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static GFile *
nautilus_places_view_get_location (NautilusView *view)
{
    NautilusPlacesViewPrivate *priv;

    priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

    return priv->location;
}

static void
nautilus_places_view_set_location (NautilusView *view,
                                   GFile        *location)
{
    if (location)
    {
        NautilusPlacesViewPrivate *priv;
        gchar *uri;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));
        uri = g_file_get_uri (location);

        /*
         * If it's not trying to open the places view itself, simply
         * delegates the location to application, which takes care of
         * selecting the appropriate view.
         */
        if (g_strcmp0 (uri, "other-locations:///") != 0)
        {
            nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                     location, 0, NULL, NULL, NULL);
        }
        else
        {
            g_set_object (&priv->location, location);
        }

        g_free (uri);
    }
}

static GList *
nautilus_places_view_get_selection (NautilusView *view)
{
    /* STUB */
    return NULL;
}

static void
nautilus_places_view_set_selection (NautilusView *view,
                                    GList        *selection)
{
    /* STUB */
}

static NautilusQuery *
nautilus_places_view_get_search_query (NautilusView *view)
{
    NautilusPlacesViewPrivate *priv;

    priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

    return priv->search_query;
}

static void
nautilus_places_view_set_search_query (NautilusView  *view,
                                       NautilusQuery *query)
{
    NautilusPlacesViewPrivate *priv;
    gchar *text;

    priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

    g_set_object (&priv->search_query, query);

    text = query ? nautilus_query_get_text (query) : NULL;

    nautilus_gtk_places_view_set_search_query (NAUTILUS_GTK_PLACES_VIEW (priv->places_view), text);

    g_free (text);
}

static NautilusToolbarMenuSections *
nautilus_places_view_get_toolbar_menu_sections (NautilusView *view)
{
    NautilusPlacesViewPrivate *priv;

    priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

    return priv->toolbar_menu_sections;
}

static gboolean
nautilus_places_view_is_loading (NautilusView *view)
{
    NautilusPlacesViewPrivate *priv;

    priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

    return nautilus_gtk_places_view_get_loading (NAUTILUS_GTK_PLACES_VIEW (priv->places_view));
}

static gboolean
nautilus_places_view_is_searching (NautilusView *view)
{
    NautilusPlacesViewPrivate *priv;

    priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

    return priv->search_query != NULL;
}

static guint
nautilus_places_view_get_view_id (NautilusView *view)
{
    return NAUTILUS_VIEW_OTHER_LOCATIONS_ID;
}

static void
nautilus_places_view_iface_init (NautilusViewInterface *iface)
{
    iface->get_location = nautilus_places_view_get_location;
    iface->set_location = nautilus_places_view_set_location;
    iface->get_selection = nautilus_places_view_get_selection;
    iface->set_selection = nautilus_places_view_set_selection;
    iface->get_search_query = nautilus_places_view_get_search_query;
    iface->set_search_query = nautilus_places_view_set_search_query;
    iface->get_toolbar_menu_sections = nautilus_places_view_get_toolbar_menu_sections;
    iface->is_loading = nautilus_places_view_is_loading;
    iface->is_searching = nautilus_places_view_is_searching;
    iface->get_view_id = nautilus_places_view_get_view_id;
}

static void
nautilus_places_view_class_init (NautilusPlacesViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_places_view_finalize;
    object_class->get_property = nautilus_places_view_get_property;
    object_class->set_property = nautilus_places_view_set_property;

    g_object_class_override_property (object_class, PROP_LOADING, "loading");
    g_object_class_override_property (object_class, PROP_SEARCHING, "searching");
    g_object_class_override_property (object_class, PROP_LOCATION, "location");
    g_object_class_override_property (object_class, PROP_SELECTION, "selection");
    g_object_class_override_property (object_class, PROP_SEARCH_QUERY, "search-query");
    g_object_class_override_property (object_class,
                                      PROP_EXTENSIONS_BACKGROUND_MENU,
                                      "extensions-background-menu");
    g_object_class_override_property (object_class,
                                      PROP_TEMPLATES_MENU,
                                      "templates-menu");
}

static void
nautilus_places_view_init (NautilusPlacesView *self)
{
    NautilusPlacesViewPrivate *priv;

    priv = nautilus_places_view_get_instance_private (self);

    /* Location */
    priv->location = g_file_new_for_uri ("other-locations:///");

    /* Places view */
    priv->places_view = nautilus_gtk_places_view_new ();
    nautilus_gtk_places_view_set_open_flags (NAUTILUS_GTK_PLACES_VIEW (priv->places_view),
                                             GTK_PLACES_OPEN_NEW_TAB | GTK_PLACES_OPEN_NEW_WINDOW | GTK_PLACES_OPEN_NORMAL);
    gtk_widget_set_hexpand (priv->places_view, TRUE);
    gtk_widget_set_vexpand (priv->places_view, TRUE);
    gtk_widget_show (priv->places_view);
    gtk_container_add (GTK_CONTAINER (self), priv->places_view);

    g_signal_connect_swapped (priv->places_view,
                              "notify::loading",
                              G_CALLBACK (loading_cb),
                              self);

    g_signal_connect_swapped (priv->places_view,
                              "open-location",
                              G_CALLBACK (open_location_cb),
                              self);

    g_signal_connect_swapped (priv->places_view,
                              "show-error-message",
                              G_CALLBACK (show_error_message_cb),
                              self);

    /* Toolbar menu */
    priv->toolbar_menu_sections = g_new0 (NautilusToolbarMenuSections, 1);
    priv->toolbar_menu_sections->supports_undo_redo = FALSE;
}

NautilusPlacesView *
nautilus_places_view_new (void)
{
    NautilusPlacesView *view;

    view = g_object_new (NAUTILUS_TYPE_PLACES_VIEW, NULL);
    if (g_object_is_floating (view))
    {
        g_object_ref_sink (view);
    }

    return view;
}
