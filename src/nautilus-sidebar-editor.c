/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#include "nautilus-sidebar-editor.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "nautilus-application.h"
#include "nautilus-bookmark.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-file-utilities.h"

struct _NautilusSidebarEditor
{
    AdwPreferencesDialog parent_instance;

    GtkWidget *default_locations_list_box;
};

G_DEFINE_TYPE (NautilusSidebarEditor, nautilus_sidebar_editor, ADW_TYPE_PREFERENCES_DIALOG);


static void
on_default_location_toggled (GObject    *object,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
    NautilusBookmarkList *bookmarks = user_data;
    GUserDirectory dir = GPOINTER_TO_INT (g_object_get_data (object, "nautilus-special-directory"));
    g_autoptr (GFile) location = g_file_new_for_path (g_get_user_special_dir (dir));
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (object));

    if (active)
    {
        g_autoptr (NautilusBookmark) bookmark = nautilus_bookmark_new (location, NULL);
        nautilus_bookmark_list_append (bookmarks, bookmark);
    }
    else
    {
        g_autofree char *uri = g_file_get_uri (location);
        nautilus_bookmark_list_delete_items_with_uri (bookmarks, uri);
    }
}

static void
populate_default_locations (NautilusSidebarEditor *self)
{
    const char * const * builtin_paths = nautilus_get_unique_builtin_special_dirs ();
    NautilusBookmarkList *bookmarks = nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (g_application_get_default ()));

    for (int index = 0; index < G_USER_N_DIRECTORIES; index++)
    {
        const char *path = builtin_paths[index];

        if (path == NULL)
        {
            continue;
        }

        g_autoptr (GFile) location = g_file_new_for_path (path);

        NautilusBookmark *bookmark = nautilus_bookmark_list_item_with_location (bookmarks, location, NULL);

        g_autofree char *name = (bookmark != NULL ?
                                 g_strdup (nautilus_bookmark_get_name (bookmark)) :
                                 g_file_get_basename (location));

        g_autoptr (GIcon) start_icon = nautilus_special_directory_get_symbolic_icon (index);

        GtkWidget *row = adw_switch_row_new ();
        adw_action_row_add_prefix (ADW_ACTION_ROW (row),
                                   gtk_image_new_from_gicon (start_icon));
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), name);
        adw_switch_row_set_active (ADW_SWITCH_ROW (row), (bookmark != NULL));
        g_signal_connect (row, "notify::active",
                          G_CALLBACK (on_default_location_toggled), bookmarks);
        g_object_set_data (G_OBJECT (row), "nautilus-special-directory", GINT_TO_POINTER (index));

        gtk_list_box_append (GTK_LIST_BOX (self->default_locations_list_box), row);
    }
}

static void
nautilus_sidebar_editor_dispose (GObject *object)
{
    NautilusSidebarEditor *self = NAUTILUS_SIDEBAR_EDITOR (object);

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_SIDEBAR_EDITOR);

    G_OBJECT_CLASS (nautilus_sidebar_editor_parent_class)->dispose (object);
}

static void
nautilus_sidebar_editor_init (NautilusSidebarEditor *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    populate_default_locations (self);
}

static void
nautilus_sidebar_editor_class_init (NautilusSidebarEditorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_sidebar_editor_dispose;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-sidebar-editor.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusSidebarEditor, default_locations_list_box);
}
