/* nautilus-bookmark.c - implementation of individual bookmarks.
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */
#define G_LOG_DOMAIN "nautilus-bookmarks"

#include <config.h>

#include "nautilus-bookmark.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-icon-names.h"

enum
{
    CONTENTS_CHANGED,
    LAST_SIGNAL
};

enum
{
    PROP_NAME = 1,
    PROP_CUSTOM_NAME,
    PROP_LOCATION,
    PROP_ICON,
    PROP_SYMBOLIC_ICON,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };
static guint signals[LAST_SIGNAL];

struct _NautilusBookmark
{
    GObject parent_instance;

    char *name;
    gboolean has_custom_name;
    GFile *location;
    GIcon *icon;
    GIcon *symbolic_icon;
    NautilusFile *file;

    GStrv selected_uris;

    gboolean exists;
    guint exists_id;
    GCancellable *cancellable;
};

static void nautilus_bookmark_disconnect_file (NautilusBookmark *file);

G_DEFINE_TYPE (NautilusBookmark, nautilus_bookmark, G_TYPE_OBJECT);

void
nautilus_bookmark_set_name (NautilusBookmark *bookmark,
                            const char       *new_name)
{
    if (g_set_str (&bookmark->name, new_name))
    {
        if ((new_name == NULL && bookmark->has_custom_name) ||
            (new_name != NULL && !bookmark->has_custom_name))
        {
            bookmark->has_custom_name = !bookmark->has_custom_name;
            g_object_notify_by_pspec (G_OBJECT (bookmark), properties[PROP_CUSTOM_NAME]);
        }

        g_object_notify_by_pspec (G_OBJECT (bookmark), properties[PROP_NAME]);
    }
}

static void
bookmark_set_name_from_ready_file (NautilusBookmark *self,
                                   NautilusFile     *file)
{
    const char *display_name;

    if (self->has_custom_name)
    {
        return;
    }

    display_name = nautilus_file_get_display_name (self->file);

    if (nautilus_file_is_other_locations (self->file))
    {
        nautilus_bookmark_set_name (self, _("Other Locations"));
    }
    else if (nautilus_file_is_home (self->file))
    {
        nautilus_bookmark_set_name (self, _("Home"));
    }
    else if (g_strcmp0 (self->name, display_name) != 0)
    {
        nautilus_bookmark_set_name (self, display_name);
        g_debug ("%s: name changed to %s", nautilus_bookmark_get_name (self), display_name);
    }
}

static void
bookmark_file_changed_callback (NautilusFile     *file,
                                NautilusBookmark *bookmark)
{
    g_autoptr (GFile) location = NULL;

    g_assert (file == bookmark->file);

    g_debug ("%s: file changed", nautilus_bookmark_get_name (bookmark));

    location = nautilus_file_get_location (file);

    if (!g_file_equal (bookmark->location, location) &&
        !nautilus_file_is_in_trash (file))
    {
        g_debug ("%s: file got moved", nautilus_bookmark_get_name (bookmark));

        g_object_unref (bookmark->location);
        bookmark->location = g_object_ref (location);

        g_object_notify_by_pspec (G_OBJECT (bookmark), properties[PROP_LOCATION]);
        g_signal_emit (bookmark, signals[CONTENTS_CHANGED], 0);
    }

    if (nautilus_file_is_gone (file) ||
        nautilus_file_is_in_trash (file))
    {
        /* The file we were monitoring has been trashed, deleted,
         * or moved in a way that we didn't notice. We should make
         * a spanking new NautilusFile object for this
         * location so if a new file appears in this place
         * we will notice. However, we can't immediately do so
         * because creating a new NautilusFile directly as a result
         * of noticing a file goes away may trigger i/o on that file
         * again, noticeing it is gone, leading to a loop.
         * So, the new NautilusFile is created when the bookmark
         * is used again. However, this is not really a problem, as
         * we don't want to change the icon or anything about the
         * bookmark just because its not there anymore.
         */
        g_debug ("%s: trashed", nautilus_bookmark_get_name (bookmark));
        nautilus_bookmark_disconnect_file (bookmark);
    }
    else
    {
        bookmark_set_name_from_ready_file (bookmark, file);
    }
}

gboolean
nautilus_bookmark_get_is_builtin (NautilusBookmark *bookmark)
{
    GUserDirectory xdg_type;

    /* if this is not an XDG dir, it's never builtin */
    if (!nautilus_bookmark_get_xdg_type (bookmark, &xdg_type))
    {
        return FALSE;
    }

    /* exclude XDG locations which are not in our builtin list */
    return nautilus_special_directory_is_builtin (xdg_type);
}

gboolean
nautilus_bookmark_get_xdg_type (NautilusBookmark *bookmark,
                                GUserDirectory   *directory)
{
    for (GUserDirectory dir = 0; dir < G_USER_N_DIRECTORIES; dir++)
    {
        const gchar *path = g_get_user_special_dir (dir);

        if (path == NULL)
        {
            continue;
        }

        g_autoptr (GFile) location = g_file_new_for_path (path);

        if (g_file_equal (location, bookmark->location))
        {
            if (directory != NULL)
            {
                *directory = dir;
            }
            return TRUE;
        }
    }

    return FALSE;
}

static GIcon *
get_native_icon (NautilusBookmark *bookmark,
                 gboolean          symbolic)
{
    if (!bookmark->exists)
    {
        g_debug ("%s: file does not exist, set warning icon", nautilus_bookmark_get_name (bookmark));
        return g_themed_icon_new (symbolic ? "dialog-warning-symbolic" : "dialog-warning");
    }

    GUserDirectory xdg_type;

    if (nautilus_bookmark_get_xdg_type (bookmark, &xdg_type))
    {
        if (symbolic)
        {
            return nautilus_special_directory_get_symbolic_icon (xdg_type);
        }
        else
        {
            return nautilus_special_directory_get_icon (xdg_type);
        }
    }

    return g_themed_icon_new (symbolic ? NAUTILUS_ICON_FOLDER : NAUTILUS_ICON_FULLCOLOR_FOLDER);
}

static void
nautilus_bookmark_set_icon_to_default (NautilusBookmark *bookmark)
{
    g_autoptr (GIcon) icon = NULL;
    g_autoptr (GIcon) symbolic_icon = NULL;

    if (g_file_is_native (bookmark->location))
    {
        symbolic_icon = get_native_icon (bookmark, TRUE);
        icon = get_native_icon (bookmark, FALSE);
    }
    else
    {
        symbolic_icon = g_themed_icon_new (NAUTILUS_ICON_FOLDER_REMOTE);
        icon = g_themed_icon_new (NAUTILUS_ICON_FULLCOLOR_FOLDER_REMOTE);
    }

    g_debug ("%s: setting icon to default", nautilus_bookmark_get_name (bookmark));

    g_object_set (bookmark,
                  "icon", icon,
                  "symbolic-icon", symbolic_icon,
                  NULL);
}

static void
nautilus_bookmark_disconnect_file (NautilusBookmark *bookmark)
{
    if (bookmark->file != NULL)
    {
        g_debug ("%s: disconnecting file",
                 nautilus_bookmark_get_name (bookmark));

        g_signal_handlers_disconnect_by_func (bookmark->file,
                                              G_CALLBACK (bookmark_file_changed_callback),
                                              bookmark);
        g_clear_object (&bookmark->file);
    }

    if (bookmark->cancellable != NULL)
    {
        g_cancellable_cancel (bookmark->cancellable);
        g_clear_object (&bookmark->cancellable);
    }

    if (bookmark->exists_id != 0)
    {
        g_source_remove (bookmark->exists_id);
        bookmark->exists_id = 0;
    }
}

static void
nautilus_bookmark_connect_file (NautilusBookmark *bookmark)
{
    if (bookmark->file != NULL)
    {
        g_debug ("%s: file already connected, returning",
                 nautilus_bookmark_get_name (bookmark));
        return;
    }

    if (bookmark->exists)
    {
        g_debug ("%s: creating file", nautilus_bookmark_get_name (bookmark));

        bookmark->file = nautilus_file_get (bookmark->location);
        g_assert (!nautilus_file_is_gone (bookmark->file));

        g_signal_connect_object (bookmark->file, "changed",
                                 G_CALLBACK (bookmark_file_changed_callback), bookmark, 0);
    }

    if (bookmark->icon == NULL ||
        bookmark->symbolic_icon == NULL)
    {
        nautilus_bookmark_set_icon_to_default (bookmark);
    }

    if (bookmark->file != NULL &&
        nautilus_file_check_if_ready (bookmark->file, NAUTILUS_FILE_ATTRIBUTE_INFO))
    {
        bookmark_set_name_from_ready_file (bookmark, bookmark->file);
    }

    if (bookmark->name == NULL)
    {
        bookmark->name = nautilus_compute_title_for_location (bookmark->location);
    }
}

static void
nautilus_bookmark_set_exists (NautilusBookmark *bookmark,
                              gboolean          exists)
{
    if (bookmark->exists == exists)
    {
        return;
    }

    bookmark->exists = exists;
    g_debug ("%s: setting bookmark to exist: %d",
             nautilus_bookmark_get_name (bookmark), exists);

    /* refresh icon */
    nautilus_bookmark_set_icon_to_default (bookmark);
}

static gboolean
exists_non_native_idle_cb (gpointer user_data)
{
    NautilusBookmark *bookmark = user_data;
    bookmark->exists_id = 0;
    nautilus_bookmark_set_exists (bookmark, FALSE);

    return FALSE;
}

static void
exists_query_info_ready_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    g_autoptr (GFileInfo) info = NULL;
    NautilusBookmark *bookmark;
    g_autoptr (GError) error = NULL;
    gboolean exists = FALSE;

    info = g_file_query_info_finish (G_FILE (source), res, &error);
    if (!info && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        return;
    }

    bookmark = user_data;

    if (info)
    {
        exists = TRUE;

        g_clear_object (&bookmark->cancellable);
    }

    nautilus_bookmark_set_exists (bookmark, exists);
}

static void
nautilus_bookmark_update_exists (NautilusBookmark *bookmark)
{
    /* Convert to a path, returning FALSE if not local. */
    if (!g_file_is_native (bookmark->location) &&
        bookmark->exists_id == 0)
    {
        bookmark->exists_id =
            g_idle_add (exists_non_native_idle_cb, bookmark);
        return;
    }

    if (bookmark->cancellable != NULL)
    {
        return;
    }

    bookmark->cancellable = g_cancellable_new ();
    g_file_query_info_async (bookmark->location,
                             G_FILE_ATTRIBUTE_STANDARD_TYPE,
                             0, G_PRIORITY_DEFAULT,
                             bookmark->cancellable,
                             exists_query_info_ready_cb, bookmark);
}

/* GObject methods */

static void
nautilus_bookmark_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    NautilusBookmark *self = NAUTILUS_BOOKMARK (object);
    GIcon *new_icon;

    switch (property_id)
    {
        case PROP_ICON:
        {
            new_icon = g_value_get_object (value);

            if (new_icon != NULL && !g_icon_equal (self->icon, new_icon))
            {
                g_clear_object (&self->icon);
                self->icon = g_object_ref (new_icon);
            }
        }
        break;

        case PROP_SYMBOLIC_ICON:
        {
            new_icon = g_value_get_object (value);

            if (new_icon != NULL && !g_icon_equal (self->symbolic_icon, new_icon))
            {
                g_clear_object (&self->symbolic_icon);
                self->symbolic_icon = g_object_ref (new_icon);
            }
        }
        break;

        case PROP_LOCATION:
        {
            self->location = g_value_dup_object (value);
        }
        break;

        case PROP_CUSTOM_NAME:
        {
            self->has_custom_name = g_value_get_boolean (value);
        }
        break;

        case PROP_NAME:
        {
            nautilus_bookmark_set_name (self, g_value_get_string (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_bookmark_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    NautilusBookmark *self = NAUTILUS_BOOKMARK (object);

    switch (property_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        case PROP_ICON:
        {
            g_value_set_object (value, self->icon);
        }
        break;

        case PROP_SYMBOLIC_ICON:
        {
            g_value_set_object (value, self->symbolic_icon);
        }
        break;

        case PROP_LOCATION:
        {
            g_value_set_object (value, self->location);
        }
        break;

        case PROP_CUSTOM_NAME:
        {
            g_value_set_boolean (value, self->has_custom_name);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_bookmark_finalize (GObject *object)
{
    NautilusBookmark *bookmark;

    g_assert (NAUTILUS_IS_BOOKMARK (object));

    bookmark = NAUTILUS_BOOKMARK (object);

    nautilus_bookmark_disconnect_file (bookmark);

    g_object_unref (bookmark->location);
    g_clear_object (&bookmark->icon);
    g_clear_object (&bookmark->symbolic_icon);

    g_free (bookmark->name);
    g_strfreev (bookmark->selected_uris);

    G_OBJECT_CLASS (nautilus_bookmark_parent_class)->finalize (object);
}

static void
nautilus_bookmark_constructed (GObject *obj)
{
    NautilusBookmark *self = NAUTILUS_BOOKMARK (obj);

    nautilus_bookmark_connect_file (self);
    nautilus_bookmark_update_exists (self);
}

static void
nautilus_bookmark_class_init (NautilusBookmarkClass *class)
{
    GObjectClass *oclass = G_OBJECT_CLASS (class);

    oclass->finalize = nautilus_bookmark_finalize;
    oclass->get_property = nautilus_bookmark_get_property;
    oclass->set_property = nautilus_bookmark_set_property;
    oclass->constructed = nautilus_bookmark_constructed;

    signals[CONTENTS_CHANGED] =
        g_signal_new ("contents-changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Bookmark's name",
                             "The name of this bookmark",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

    properties[PROP_CUSTOM_NAME] =
        g_param_spec_boolean ("custom-name",
                              "Whether the bookmark has a custom name",
                              "Whether the bookmark has a custom name",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

    properties[PROP_LOCATION] =
        g_param_spec_object ("location",
                             "Bookmark's location",
                             "The location of this bookmark",
                             G_TYPE_FILE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_ICON] =
        g_param_spec_object ("icon",
                             "Bookmark's icon",
                             "The icon of this bookmark",
                             G_TYPE_ICON,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SYMBOLIC_ICON] =
        g_param_spec_object ("symbolic-icon",
                             "Bookmark's symbolic icon",
                             "The symbolic icon of this bookmark",
                             G_TYPE_ICON,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
nautilus_bookmark_init (NautilusBookmark *bookmark)
{
    bookmark->exists = TRUE;
}

const gchar *
nautilus_bookmark_get_name (NautilusBookmark *bookmark)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

    return bookmark->name;
}

gboolean
nautilus_bookmark_get_has_custom_name (NautilusBookmark *bookmark)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

    return (bookmark->has_custom_name);
}

/**
 * nautilus_bookmark_compare_with:
 *
 * Check whether two bookmarks are considered identical.
 * @a: first NautilusBookmark*.
 * @b: second NautilusBookmark*.
 *
 * Return value: 0 if @a and @b have same name and uri, 1 otherwise
 * (GCompareFunc style)
 **/
int
nautilus_bookmark_compare_with (gconstpointer a,
                                gconstpointer b)
{
    NautilusBookmark *bookmark_a;
    NautilusBookmark *bookmark_b;

    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK ((gpointer) a), 1);
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK ((gpointer) b), 1);

    bookmark_a = NAUTILUS_BOOKMARK ((gpointer) a);
    bookmark_b = NAUTILUS_BOOKMARK ((gpointer) b);

    if (!g_file_equal (bookmark_a->location,
                       bookmark_b->location))
    {
        return 1;
    }

    if (g_strcmp0 (bookmark_a->name,
                   bookmark_b->name) != 0)
    {
        return 1;
    }

    return 0;
}

GIcon *
nautilus_bookmark_get_symbolic_icon (NautilusBookmark *bookmark)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

    /* Try to connect a file in case file exists now but didn't earlier. */
    nautilus_bookmark_connect_file (bookmark);

    if (bookmark->symbolic_icon)
    {
        return g_object_ref (bookmark->symbolic_icon);
    }
    return NULL;
}

GIcon *
nautilus_bookmark_get_icon (NautilusBookmark *bookmark)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

    /* Try to connect a file in case file exists now but didn't earlier. */
    nautilus_bookmark_connect_file (bookmark);

    if (bookmark->icon)
    {
        return g_object_ref (bookmark->icon);
    }
    return NULL;
}

GFile *
nautilus_bookmark_get_location (NautilusBookmark *bookmark)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

    /* Try to connect a file in case file exists now but didn't earlier.
     * This allows a bookmark to update its image properly in the case
     * where a new file appears with the same URI as a previously-deleted
     * file. Calling connect_file here means that attempts to activate the
     * bookmark will update its image if possible.
     */
    nautilus_bookmark_connect_file (bookmark);

    return g_object_ref (bookmark->location);
}

char *
nautilus_bookmark_get_uri (NautilusBookmark *bookmark)
{
    g_autoptr (GFile) file = NULL;

    file = nautilus_bookmark_get_location (bookmark);

    return g_file_get_uri (file);
}

NautilusBookmark *
nautilus_bookmark_new (GFile       *location,
                       const gchar *custom_name)
{
    NautilusBookmark *new_bookmark;

    new_bookmark = NAUTILUS_BOOKMARK (g_object_new (NAUTILUS_TYPE_BOOKMARK,
                                                    "location", location,
                                                    "name", custom_name,
                                                    "custom-name", custom_name != NULL,
                                                    NULL));

    return new_bookmark;
}

void
nautilus_bookmark_take_selected_uris (NautilusBookmark *bookmark,
                                      GStrv             selected_uris)
{
    g_strfreev (bookmark->selected_uris);
    bookmark->selected_uris = g_steal_pointer (&selected_uris);
}

GStrv
nautilus_bookmark_get_selected_uris (NautilusBookmark *bookmark)
{
    return bookmark->selected_uris;
}
