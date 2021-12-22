/* fm-properties-window.c - window that lets user modify file properties
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Darin Adler <darin@bentspoon.com>
 */

#include "nautilus-properties-window.h"

#include <cairo.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <nautilus-extension.h>
#include <string.h>
#include <sys/stat.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "nautilus-enums.h"
#include "nautilus-error-reporting.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-metadata.h"
#include "nautilus-mime-actions.h"
#include "nautilus-module.h"
#include "nautilus-signaller.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-signaller.h"

static GHashTable *windows;
static GHashTable *pending_lists;

typedef struct
{
    NautilusFile *file;
    char *owner;
    GtkWindow *window;
    unsigned int timeout;
    gboolean cancelled;
} OwnerChange;

typedef struct
{
    NautilusFile *file;
    char *group;
    GtkWindow *window;
    unsigned int timeout;
    gboolean cancelled;
} GroupChange;

struct _NautilusPropertiesWindow
{
    HdyWindow parent_instance;

    GList *original_files;
    GList *target_files;

    GtkNotebook *notebook;

    /* Basic tab widgets */

    GtkStack *icon_stack;
    GtkWidget *icon_image;
    GtkWidget *icon_button;
    GtkWidget *icon_button_image;
    GtkWidget *icon_chooser;

    GtkGrid *basic_grid;

    GtkLabel *name_title_label;
    GtkStack *name_stack;
    GtkWidget *name_field;
    char *pending_name;

    guint select_idle_id;

    GtkWidget *type_title_label;
    GtkWidget *type_value_label;

    GtkWidget *link_target_title_label;
    GtkWidget *link_target_value_label;

    GtkWidget *contents_title_label;
    GtkWidget *contents_value_label;
    GtkWidget *contents_spinner;
    guint update_directory_contents_timeout_id;
    guint update_files_timeout_id;

    GtkWidget *size_title_label;
    GtkWidget *size_value_label;

    GtkWidget *parent_folder_title_label;
    GtkWidget *parent_folder_value_label;

    GtkWidget *original_folder_title_label;
    GtkWidget *original_folder_value_label;

    GtkWidget *volume_title_label;
    GtkWidget *volume_value_label;

    GtkWidget *trashed_on_title_label;
    GtkWidget *trashed_on_value_label;

    GtkWidget *spacer_2;

    GtkWidget *accessed_title_label;
    GtkWidget *accessed_value_label;

    GtkWidget *modified_title_label;
    GtkWidget *modified_value_label;

    GtkWidget *created_title_label;
    GtkWidget *created_value_label;

    GtkWidget *spacer_3;

    GtkWidget *free_space_title_label;
    GtkWidget *free_space_value_label;

    GtkWidget *volume_widget_box;
    GtkWidget *open_in_disks_button;

    GtkWidget *pie_chart;
    GtkWidget *used_color;
    GtkWidget *used_value;
    GtkWidget *free_color;
    GtkWidget *free_value;
    GtkWidget *total_capacity_value;
    GtkWidget *file_system_value;

    /* Permissions tab Widgets */

    GtkWidget *permissions_box;
    GtkWidget *permissions_grid;

    GtkWidget *bottom_prompt_seperator;
    GtkWidget *not_the_owner_label;

    GtkWidget *permission_indeterminable_label;

    GtkWidget *owner_value_stack;
    GtkWidget *owner_access_label;
    GtkWidget *owner_access_combo;
    GtkWidget *owner_folder_access_label;
    GtkWidget *owner_folder_access_combo;
    GtkWidget *owner_file_access_label;
    GtkWidget *owner_file_access_combo;

    GtkWidget *group_value_stack;
    GtkWidget *group_access_label;
    GtkWidget *group_access_combo;
    GtkWidget *group_folder_access_label;
    GtkWidget *group_folder_access_combo;
    GtkWidget *group_file_access_label;
    GtkWidget *group_file_access_combo;

    GtkWidget *others_access_label;
    GtkWidget *others_access_combo;
    GtkWidget *others_folder_access_label;
    GtkWidget *others_folder_access_combo;
    GtkWidget *others_file_access_label;
    GtkWidget *others_file_access_combo;

    GtkWidget *execute_label;
    GtkWidget *execute_checkbox;

    GtkWidget *security_context_title_label;
    GtkWidget *security_context_value_label;

    GtkWidget *change_permissions_button_box;
    GtkWidget *change_permissions_button;

    /* Open With tab Widgets */

    GtkWidget *open_with_box;
    GtkWidget *open_with_label;
    GtkWidget *app_chooser_widget_box;
    GtkWidget *app_chooser_widget;
    GtkWidget *reset_button;
    GtkWidget *add_button;
    GtkWidget *set_as_default_button;
    char *content_type;
    GList *open_with_files;

    GroupChange *group_change;
    OwnerChange *owner_change;

    GList *permission_buttons;
    GList *permission_combos;
    GList *change_permission_combos;
    GHashTable *initial_permissions;
    gboolean has_recursive_apply;

    GList *value_fields;

    GList *mime_list;

    gboolean deep_count_finished;
    GList *deep_count_files;
    guint deep_count_spinner_timeout_id;

    guint long_operation_underway;

    GList *changed_files;

    guint64 volume_capacity;
    guint64 volume_free;
    guint64 volume_used;
};

enum
{
    COLUMN_NAME,
    COLUMN_VALUE,
    COLUMN_USE_ORIGINAL,
    COLUMN_ID,
    NUM_COLUMNS
};

typedef struct
{
    GList *original_files;
    GList *target_files;
    GtkWidget *parent_widget;
    GtkWindow *parent_window;
    char *startup_id;
    char *pending_key;
    GHashTable *pending_files;
    NautilusPropertiesWindowCallback callback;
    gpointer callback_data;
    NautilusPropertiesWindow *window;
    gboolean cancelled;
} StartupData;

/* drag and drop definitions */

enum
{
    TARGET_URI_LIST,
    TARGET_GNOME_URI_LIST,
};

static const GtkTargetEntry target_table[] =
{
    { "text/uri-list", 0, TARGET_URI_LIST },
    { "x-special/gnome-icon-list", 0, TARGET_GNOME_URI_LIST },
};

#define DIRECTORY_CONTENTS_UPDATE_INTERVAL      200 /* milliseconds */
#define FILES_UPDATE_INTERVAL                   200 /* milliseconds */

/*
 * A timeout before changes through the user/group combo box will be applied.
 * When quickly changing owner/groups (i.e. by keyboard or scroll wheel),
 * this ensures that the GUI doesn't end up unresponsive.
 *
 * Both combos react on changes by scheduling a new change and unscheduling
 * or cancelling old pending changes.
 */
#define CHOWN_CHGRP_TIMEOUT                     300 /* milliseconds */

static void schedule_directory_contents_update (NautilusPropertiesWindow *self);
static void directory_contents_value_field_update (NautilusPropertiesWindow *self);
static void file_changed_callback (NautilusFile *file,
                                   gpointer      user_data);
static void permission_button_update (GtkToggleButton          *button,
                                      NautilusPropertiesWindow *self);
static void permission_combo_update (GtkComboBox              *combo,
                                     NautilusPropertiesWindow *self);
static void value_field_update (GtkLabel                 *field,
                                NautilusPropertiesWindow *self);
static void properties_window_update (NautilusPropertiesWindow *self,
                                      GList                    *files);
static void is_directory_ready_callback (NautilusFile *file,
                                         gpointer      data);
static void cancel_group_change_callback (GroupChange *change);
static void cancel_owner_change_callback (OwnerChange *change);
static void select_image_button_callback (GtkWidget                *widget,
                                          NautilusPropertiesWindow *self);
static void set_icon (const char               *icon_path,
                      NautilusPropertiesWindow *self);
static void remove_pending (StartupData *data,
                            gboolean     cancel_call_when_ready,
                            gboolean     cancel_timed_wait);
static void append_extension_pages (NautilusPropertiesWindow *self);

static void name_field_focus_changed (GObject    *object,
                                      GParamSpec *pspec,
                                      gpointer    user_data);
static void name_field_activate (GtkWidget *name_field,
                                 gpointer   user_data);
static void setup_pie_widget (NautilusPropertiesWindow *self);

G_DEFINE_TYPE (NautilusPropertiesWindow, nautilus_properties_window, HDY_TYPE_WINDOW);

static gboolean
is_multi_file_window (NautilusPropertiesWindow *self)
{
    GList *l;
    int count;

    count = 0;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_gone (NAUTILUS_FILE (l->data)))
        {
            count++;
            if (count > 1)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static int
get_not_gone_original_file_count (NautilusPropertiesWindow *self)
{
    GList *l;
    int count;

    count = 0;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_gone (NAUTILUS_FILE (l->data)))
        {
            count++;
        }
    }

    return count;
}

static NautilusFile *
get_original_file (NautilusPropertiesWindow *self)
{
    g_return_val_if_fail (!is_multi_file_window (self), NULL);

    if (self->original_files == NULL)
    {
        return NULL;
    }

    return NAUTILUS_FILE (self->original_files->data);
}

static NautilusFile *
get_target_file_for_original_file (NautilusFile *file)
{
    NautilusFile *target_file;
    g_autoptr (GFile) location = NULL;
    g_autofree char *uri_to_display = NULL;

    uri_to_display = nautilus_file_get_uri (file);
    location = g_file_new_for_uri (uri_to_display);
    target_file = nautilus_file_get (location);

    return target_file;
}

static NautilusFile *
get_target_file (NautilusPropertiesWindow *self)
{
    return NAUTILUS_FILE (self->target_files->data);
}

static void
get_image_for_properties_window (NautilusPropertiesWindow  *self,
                                 char                     **icon_name,
                                 GdkPixbuf                **icon_pixbuf)
{
    g_autoptr (NautilusIconInfo) icon = NULL;
    GList *l;
    gint icon_scale;

    icon_scale = gtk_widget_get_scale_factor (GTK_WIDGET (self->notebook));

    for (l = self->original_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        g_autoptr (NautilusIconInfo) new_icon = NULL;

        file = NAUTILUS_FILE (l->data);

        if (!icon)
        {
            icon = nautilus_file_get_icon (file, NAUTILUS_CANVAS_ICON_SIZE_STANDARD, icon_scale,
                                           NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
                                           NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING);
        }
        else
        {
            new_icon = nautilus_file_get_icon (file, NAUTILUS_CANVAS_ICON_SIZE_STANDARD, icon_scale,
                                               NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
                                               NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING);
            if (!new_icon || new_icon != icon)
            {
                g_object_unref (icon);
                icon = NULL;
                break;
            }
        }
    }

    if (!icon)
    {
        icon = nautilus_icon_info_lookup_from_name ("text-x-generic",
                                                    NAUTILUS_CANVAS_ICON_SIZE_STANDARD,
                                                    icon_scale);
    }

    if (icon_name != NULL)
    {
        *icon_name = g_strdup (nautilus_icon_info_get_used_name (icon));
    }

    if (icon_pixbuf != NULL)
    {
        *icon_pixbuf = nautilus_icon_info_get_pixbuf_at_size (icon, NAUTILUS_CANVAS_ICON_SIZE_STANDARD);
    }
}


static void
update_properties_window_icon (NautilusPropertiesWindow *self)
{
    g_autoptr (GdkPixbuf) pixbuf = NULL;
    cairo_surface_t *surface;
    g_autofree char *name = NULL;

    get_image_for_properties_window (self, &name, &pixbuf);

    if (name != NULL)
    {
        gtk_window_set_icon_name (GTK_WINDOW (self), name);
    }

    surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, gtk_widget_get_scale_factor (GTK_WIDGET (self)),
                                                    gtk_widget_get_window (GTK_WIDGET (self)));
    gtk_image_set_from_surface (GTK_IMAGE (self->icon_image), surface);
    gtk_image_set_from_surface (GTK_IMAGE (self->icon_button_image), surface);

    cairo_surface_destroy (surface);
}

/* utility to test if a uri refers to a local image */
static gboolean
uri_is_local_image (const char *uri)
{
    g_autoptr (GdkPixbuf) pixbuf = NULL;
    g_autofree char *image_path = NULL;

    image_path = g_filename_from_uri (uri, NULL, NULL);
    if (image_path == NULL)
    {
        return FALSE;
    }

    pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);

    if (pixbuf == NULL)
    {
        return FALSE;
    }

    return TRUE;
}


static void
reset_icon (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        nautilus_file_set_metadata (file,
                                    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
                                    NULL, NULL);
    }
}


static void
nautilus_properties_window_drag_data_received (GtkWidget        *widget,
                                               GdkDragContext   *context,
                                               int               x,
                                               int               y,
                                               GtkSelectionData *selection_data,
                                               guint             info,
                                               guint             time)
{
    g_auto (GStrv) uris = NULL;
    gboolean exactly_one;
    GtkImage *image;
    GtkWindow *window;

    image = GTK_IMAGE (widget);
    window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (image)));

    uris = g_strsplit ((const gchar *) gtk_selection_data_get_data (selection_data), "\r\n", 0);
    exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');


    if (!exactly_one)
    {
        show_dialog (_("You cannot assign more than one custom icon at a time!"),
                     _("Please drop just one image to set a custom icon."),
                     window,
                     GTK_MESSAGE_ERROR);
    }
    else
    {
        if (uri_is_local_image (uris[0]))
        {
            set_icon (uris[0], NAUTILUS_PROPERTIES_WINDOW (window));
        }
        else
        {
            g_autoptr (GFile) f = NULL;

            f = g_file_new_for_uri (uris[0]);
            if (!g_file_is_native (f))
            {
                show_dialog (_("The file that you dropped is not local."),
                             _("You can only use local images as custom icons."),
                             window,
                             GTK_MESSAGE_ERROR);
            }
            else
            {
                show_dialog (_("The file that you dropped is not an image."),
                             _("You can only use local images as custom icons."),
                             window,
                             GTK_MESSAGE_ERROR);
            }
        }
    }
}

static void
setup_image_widget (NautilusPropertiesWindow *self,
                    gboolean                  is_customizable)
{
    update_properties_window_icon (self);

    if (is_customizable)
    {
        /* prepare the image to receive dropped objects to assign custom images */
        gtk_drag_dest_set (self->icon_button_image,
                           GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
                           target_table, G_N_ELEMENTS (target_table),
                           GDK_ACTION_COPY | GDK_ACTION_MOVE);

        g_signal_connect (self->icon_button_image, "drag-data-received",
                          G_CALLBACK (nautilus_properties_window_drag_data_received), NULL);
        g_signal_connect (self->icon_button, "clicked",
                          G_CALLBACK (select_image_button_callback), self);
        gtk_stack_set_visible_child (self->icon_stack, self->icon_button);
    }
    else
    {
        gtk_stack_set_visible_child (self->icon_stack, self->icon_image);
    }
}

static void
set_name_field (NautilusPropertiesWindow *self,
                const gchar              *original_name,
                const gchar              *name)
{
    GtkWidget *stack_child_label;
    GtkWidget *stack_child_entry;
    gboolean use_label;

    stack_child_label = gtk_stack_get_child_by_name (self->name_stack, "name_value_label");
    stack_child_entry = gtk_stack_get_child_by_name (self->name_stack, "name_value_entry");

    use_label = is_multi_file_window (self) || !nautilus_file_can_rename (get_original_file (self));

    if (use_label)
    {
        gtk_label_set_text (GTK_LABEL (stack_child_label), name);
        gtk_stack_set_visible_child (self->name_stack, stack_child_label);
    }
    else
    {
        gtk_stack_set_visible_child (self->name_stack, stack_child_entry);
    }

    /* Only replace text if the file's name has changed. */
    if (original_name == NULL || strcmp (original_name, name) != 0)
    {
        if (!use_label)
        {
            /* Only reset the text if it's different from what is
             * currently showing. This causes minimal ripples (e.g.
             * selection change).
             */
            g_autofree gchar *displayed_name = gtk_editable_get_chars (GTK_EDITABLE (self->name_field), 0, -1);
            if (strcmp (displayed_name, name) != 0)
            {
                gtk_entry_set_text (GTK_ENTRY (self->name_field), name);
            }
        }
    }
}

static void
update_name_field (NautilusPropertiesWindow *self)
{
    NautilusFile *file;

    gtk_label_set_text_with_mnemonic (self->name_title_label,
                                      ngettext ("_Name", "_Names",
                                                get_not_gone_original_file_count (self)));

    if (is_multi_file_window (self))
    {
        /* Multifile property dialog, show all names */
        g_autoptr (GString) str = NULL;
        gboolean first;
        GList *l;

        str = g_string_new ("");

        first = TRUE;

        for (l = self->target_files; l != NULL; l = l->next)
        {
            g_autofree gchar *name = NULL;

            file = NAUTILUS_FILE (l->data);

            if (!nautilus_file_is_gone (file))
            {
                if (!first)
                {
                    g_string_append (str, ", ");
                }
                first = FALSE;

                name = nautilus_file_get_display_name (file);
                g_string_append (str, name);
            }
        }
        set_name_field (self, NULL, str->str);
    }
    else
    {
        const char *original_name = NULL;
        g_autofree char *current_name = NULL;
        gboolean use_label;

        file = get_original_file (self);
        use_label = !nautilus_file_can_rename (file);

        if (file == NULL || nautilus_file_is_gone (file))
        {
            current_name = g_strdup ("");
        }
        else
        {
            if (use_label)
            {
                current_name = nautilus_file_get_display_name (file);
            }
            else
            {
                current_name = nautilus_file_get_edit_name (file);
            }
        }

        /* If the file name has changed since the original name was stored,
         * update the text in the text field, possibly (deliberately) clobbering
         * an edit in progress. If the name hasn't changed (but some other
         * aspect of the file might have), then don't clobber changes.
         */
        original_name = (const char *) g_object_get_data (G_OBJECT (self->name_field), "original_name");

        set_name_field (self, original_name, current_name);

        if (original_name == NULL ||
            g_strcmp0 (original_name, current_name) != 0)
        {
            g_object_set_data_full (G_OBJECT (self->name_field),
                                    "original_name",
                                    g_steal_pointer (&current_name),
                                    g_free);
        }
    }
}

static void
name_field_restore_original_name (GtkWidget *name_field)
{
    const char *original_name;
    g_autofree char *displayed_name = NULL;

    original_name = (const char *) g_object_get_data (G_OBJECT (name_field),
                                                      "original_name");

    if (!original_name)
    {
        return;
    }

    displayed_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);

    if (strcmp (original_name, displayed_name) != 0)
    {
        gtk_entry_set_text (GTK_ENTRY (name_field), original_name);
    }
    gtk_editable_select_region (GTK_EDITABLE (name_field), 0, -1);
}

static void
rename_callback (NautilusFile *file,
                 GFile        *res_loc,
                 GError       *error,
                 gpointer      callback_data)
{
    g_autoptr (NautilusPropertiesWindow) self = NAUTILUS_PROPERTIES_WINDOW (callback_data);

    /* Complain to user if rename failed. */
    if (error != NULL)
    {
        nautilus_report_error_renaming_file (file,
                                             self->pending_name,
                                             error,
                                             GTK_WINDOW (self));
        name_field_restore_original_name (self->name_field);
    }
}

static void
set_pending_name (NautilusPropertiesWindow *self,
                  const char               *name)
{
    g_free (self->pending_name);
    self->pending_name = g_strdup (name);
}

static void
name_field_done_editing (GtkWidget                *name_field,
                         NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    g_autofree char *new_name = NULL;
    const char *original_name;

    g_return_if_fail (GTK_IS_ENTRY (name_field));

    /* Don't apply if the dialog has more than one file */
    if (is_multi_file_window (self))
    {
        return;
    }

    file = get_original_file (self);

    /* This gets called when the window is closed, which might be
     * caused by the file having been deleted.
     */
    if (file == NULL || nautilus_file_is_gone (file))
    {
        return;
    }

    new_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);

    /* Special case: silently revert text if new text is empty. */
    if (strlen (new_name) == 0)
    {
        name_field_restore_original_name (name_field);
    }
    else
    {
        original_name = (const char *) g_object_get_data (G_OBJECT (self->name_field),
                                                          "original_name");
        /* Don't rename if not changed since we read the display name.
         *  This is needed so that we don't save the display name to the
         *  file when nothing is changed */
        if (strcmp (new_name, original_name) != 0)
        {
            set_pending_name (self, new_name);
            g_object_ref (self);
            nautilus_file_rename (file, new_name,
                                  rename_callback, self);
        }
    }
}

static void
name_field_focus_changed (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
    GtkWidget *widget;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (user_data));

    widget = GTK_WIDGET (object);

    if (!gtk_widget_has_focus (widget) && gtk_widget_get_sensitive (widget))
    {
        name_field_done_editing (widget, NAUTILUS_PROPERTIES_WINDOW (user_data));
    }
}

static gboolean
select_all_at_idle (gpointer user_data)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (user_data);

    gtk_editable_select_region (GTK_EDITABLE (self->name_field),
                                0, -1);

    self->select_idle_id = 0;

    return FALSE;
}

static void
name_field_activate (GtkWidget *name_field,
                     gpointer   user_data)
{
    NautilusPropertiesWindow *self;

    g_assert (GTK_IS_ENTRY (name_field));
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (user_data));

    self = NAUTILUS_PROPERTIES_WINDOW (user_data);

    /* Accept changes. */
    name_field_done_editing (name_field, self);

    if (self->select_idle_id == 0)
    {
        self->select_idle_id = g_idle_add (select_all_at_idle,
                                           self);
    }
}

static void
update_properties_window_title (NautilusPropertiesWindow *self)
{
    g_autofree gchar *title = NULL;
    NautilusFile *file;

    g_return_if_fail (GTK_IS_WINDOW (self));

    if (!is_multi_file_window (self))
    {
        file = get_original_file (self);

        if (file != NULL)
        {
            g_autofree gchar *name = NULL;
            g_autofree gchar *truncated_name = NULL;

            name = nautilus_file_get_display_name (file);
            truncated_name = eel_str_middle_truncate (name, 30);

            if (nautilus_file_is_directory (file))
            {
                /* To translators: %s is the name of the folder. */
                title = g_strdup_printf (C_("folder", "%s Properties"),
                                         truncated_name);
            }
            else
            {
                /* To translators: %s is the name of the file. */
                title = g_strdup_printf (C_("file", "%s Properties"),
                                         truncated_name);
            }
        }
    }

    if (title == NULL)
    {
        title = g_strdup_printf (_("Properties"));
    }

    gtk_window_set_title (GTK_WINDOW (self), title);
}

static void
clear_extension_pages (NautilusPropertiesWindow *self)
{
    int i;
    int num_pages;
    GtkWidget *page;

    num_pages = gtk_notebook_get_n_pages
                    (GTK_NOTEBOOK (self->notebook));

    for (i = 0; i < num_pages; i++)
    {
        page = gtk_notebook_get_nth_page
                   (GTK_NOTEBOOK (self->notebook), i);

        if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (page), "is-extension-page")))
        {
            gtk_notebook_remove_page
                (GTK_NOTEBOOK (self->notebook), i);
            num_pages--;
            i--;
        }
    }
}

static void
refresh_extension_pages (NautilusPropertiesWindow *self)
{
    clear_extension_pages (self);
    append_extension_pages (self);
}

static void
remove_from_dialog (NautilusPropertiesWindow *self,
                    NautilusFile             *file)
{
    int index;
    GList *original_link;
    GList *target_link;
    g_autoptr (NautilusFile) original_file = NULL;
    g_autoptr (NautilusFile) target_file = NULL;

    index = g_list_index (self->target_files, file);
    if (index == -1)
    {
        index = g_list_index (self->original_files, file);
        g_return_if_fail (index != -1);
    }

    original_link = g_list_nth (self->original_files, index);
    target_link = g_list_nth (self->target_files, index);

    g_return_if_fail (original_link && target_link);

    original_file = NAUTILUS_FILE (original_link->data);
    target_file = NAUTILUS_FILE (target_link->data);

    self->original_files = g_list_delete_link (self->original_files, original_link);
    self->target_files = g_list_delete_link (self->target_files, target_link);

    g_hash_table_remove (self->initial_permissions, target_file);

    g_signal_handlers_disconnect_by_func (original_file,
                                          G_CALLBACK (file_changed_callback),
                                          self);
    g_signal_handlers_disconnect_by_func (target_file,
                                          G_CALLBACK (file_changed_callback),
                                          self);

    nautilus_file_monitor_remove (original_file, &self->original_files);
    nautilus_file_monitor_remove (target_file, &self->target_files);
}

static gboolean
mime_list_equal (GList *a,
                 GList *b)
{
    while (a && b)
    {
        if (strcmp (a->data, b->data))
        {
            return FALSE;
        }
        a = a->next;
        b = b->next;
    }

    return (a == b);
}

static GList *
get_mime_list (NautilusPropertiesWindow *self)
{
    return g_list_copy_deep (self->target_files,
                             (GCopyFunc) nautilus_file_get_mime_type,
                             NULL);
}

static gboolean
start_spinner_callback (NautilusPropertiesWindow *self)
{
    gtk_widget_show (self->contents_spinner);
    gtk_spinner_start (GTK_SPINNER (self->contents_spinner));
    self->deep_count_spinner_timeout_id = 0;

    return FALSE;
}

static void
schedule_start_spinner (NautilusPropertiesWindow *self)
{
    if (self->deep_count_spinner_timeout_id == 0)
    {
        self->deep_count_spinner_timeout_id
            = g_timeout_add_seconds (1,
                                     (GSourceFunc) start_spinner_callback,
                                     self);
    }
}

static void
stop_spinner (NautilusPropertiesWindow *self)
{
    gtk_spinner_stop (GTK_SPINNER (self->contents_spinner));
    gtk_widget_hide (self->contents_spinner);
    g_clear_handle_id (&self->deep_count_spinner_timeout_id, g_source_remove);
}

static void
stop_deep_count_for_file (NautilusPropertiesWindow *self,
                          NautilusFile             *file)
{
    if (g_list_find (self->deep_count_files, file))
    {
        g_signal_handlers_disconnect_by_func (file,
                                              G_CALLBACK (schedule_directory_contents_update),
                                              self);
        nautilus_file_unref (file);
        self->deep_count_files = g_list_remove (self->deep_count_files, file);
    }
}

static void
start_deep_count_for_file (NautilusFile             *file,
                           NautilusPropertiesWindow *self)
{
    if (!nautilus_file_is_directory (file))
    {
        return;
    }

    if (!g_list_find (self->deep_count_files, file))
    {
        nautilus_file_ref (file);
        self->deep_count_files = g_list_prepend (self->deep_count_files, file);

        nautilus_file_recompute_deep_counts (file);
        if (!self->deep_count_finished)
        {
            g_signal_connect_object (file,
                                     "updated-deep-count-in-progress",
                                     G_CALLBACK (schedule_directory_contents_update),
                                     self, G_CONNECT_SWAPPED);
            schedule_start_spinner (self);
        }
    }
}

static void
properties_window_update (NautilusPropertiesWindow *self,
                          GList                    *files)
{
    GList *mime_list;
    NautilusFile *changed_file;
    gboolean dirty_original = FALSE;
    gboolean dirty_target = FALSE;

    if (files == NULL)
    {
        dirty_original = TRUE;
        dirty_target = TRUE;
    }

    for (GList *tmp = files; tmp != NULL; tmp = tmp->next)
    {
        changed_file = NAUTILUS_FILE (tmp->data);

        if (changed_file && nautilus_file_is_gone (changed_file))
        {
            /* Remove the file from the property dialog */
            remove_from_dialog (self, changed_file);
            changed_file = NULL;

            if (self->original_files == NULL)
            {
                return;
            }
        }
        if (changed_file == NULL ||
            g_list_find (self->original_files, changed_file))
        {
            dirty_original = TRUE;
        }
        if (changed_file == NULL ||
            g_list_find (self->target_files, changed_file))
        {
            dirty_target = TRUE;
        }
    }

    if (dirty_original)
    {
        update_properties_window_title (self);
        update_properties_window_icon (self);
        update_name_field (self);

        /* If any of the value fields start to depend on the original
         * value, value_field_updates should be added here */
    }

    if (dirty_target)
    {
        g_list_foreach (self->permission_buttons,
                        (GFunc) permission_button_update,
                        self);
        g_list_foreach (self->permission_combos,
                        (GFunc) permission_combo_update,
                        self);
        g_list_foreach (self->value_fields,
                        (GFunc) value_field_update,
                        self);
    }

    mime_list = get_mime_list (self);

    if (self->mime_list == NULL)
    {
        self->mime_list = mime_list;
    }
    else
    {
        if (!mime_list_equal (self->mime_list, mime_list))
        {
            refresh_extension_pages (self);
        }

        g_list_free_full (self->mime_list, g_free);
        self->mime_list = mime_list;
    }
}

static gboolean
update_files_callback (gpointer data)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (data);

    self->update_files_timeout_id = 0;

    properties_window_update (self, self->changed_files);

    if (self->original_files == NULL)
    {
        /* Close the window if no files are left */
        gtk_widget_destroy (GTK_WIDGET (self));
    }
    else
    {
        nautilus_file_list_free (self->changed_files);
        self->changed_files = NULL;
    }

    return FALSE;
}

static void
schedule_files_update (NautilusPropertiesWindow *self)
{
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    if (self->update_files_timeout_id == 0)
    {
        self->update_files_timeout_id
            = g_timeout_add (FILES_UPDATE_INTERVAL,
                             update_files_callback,
                             self);
    }
}

static gboolean
file_list_attributes_identical (GList      *file_list,
                                const char *attribute_name)
{
    gboolean identical;
    g_autofree char *first_attr = NULL;
    GList *l;

    identical = TRUE;

    for (l = file_list; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_is_gone (file))
        {
            continue;
        }

        if (first_attr == NULL)
        {
            first_attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
        }
        else
        {
            g_autofree char *attr = NULL;
            attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
            if (strcmp (attr, first_attr))
            {
                identical = FALSE;
                break;
            }
        }
    }

    return identical;
}

static char *
file_list_get_string_attribute (GList      *file_list,
                                const char *attribute_name,
                                const char *inconsistent_value)
{
    if (file_list_attributes_identical (file_list, attribute_name))
    {
        GList *l;

        for (l = file_list; l != NULL; l = l->next)
        {
            NautilusFile *file;

            file = NAUTILUS_FILE (l->data);
            if (!nautilus_file_is_gone (file))
            {
                return nautilus_file_get_string_attribute_with_default
                           (file,
                           attribute_name);
            }
        }
        return g_strdup (_("unknown"));
    }
    else
    {
        return g_strdup (inconsistent_value);
    }
}

#define INCONSISTENT_STATE_STRING \
    "\xE2\x80\x92"

static gboolean
location_show_original (NautilusPropertiesWindow *self)
{
    NautilusFile *file;

    /* there is no way a recent item will be mixed with
     *   other items so just pick the first file to check */
    file = NAUTILUS_FILE (g_list_nth_data (self->original_files, 0));
    return (file != NULL && !nautilus_file_is_in_recent (file));
}

static void
value_field_update (GtkLabel                 *label,
                    NautilusPropertiesWindow *self)
{
    GList *file_list;
    const char *attribute_name;
    g_autofree char *attribute_value = NULL;
    char *inconsistent_string;
    gboolean is_where;

    g_assert (GTK_IS_LABEL (label));

    attribute_name = g_object_get_data (G_OBJECT (label), "file_attribute");

    is_where = (g_strcmp0 (attribute_name, "where") == 0);
    if (is_where && location_show_original (self))
    {
        file_list = self->original_files;
    }
    else
    {
        file_list = self->target_files;
    }

    inconsistent_string = INCONSISTENT_STATE_STRING;
    attribute_value = file_list_get_string_attribute (file_list,
                                                      attribute_name,
                                                      inconsistent_string);
    if (!strcmp (attribute_name, "detailed_type") && strcmp (attribute_value, inconsistent_string))
    {
        g_autofree char *mime_type = file_list_get_string_attribute (file_list,
                                                                     "mime_type",
                                                                     inconsistent_string);
        if (strcmp (mime_type, inconsistent_string) && strcmp (mime_type, "inode/directory"))
        {
            g_autofree char *tmp = g_steal_pointer (&attribute_value);
            attribute_value = g_strdup_printf (C_("MIME type description (MIME type)", "%s (%s)"), tmp, mime_type);
        }
    }

    gtk_label_set_text (label, attribute_value);
}

static void
group_change_free (GroupChange *change)
{
    nautilus_file_unref (change->file);
    g_free (change->group);
    g_object_unref (change->window);

    g_free (change);
}

static void
group_change_callback (NautilusFile *file,
                       GFile        *res_loc,
                       GError       *error,
                       GroupChange  *change)
{
    NautilusPropertiesWindow *self;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (change->window));
    g_assert (NAUTILUS_IS_FILE (change->file));
    g_assert (change->group != NULL);

    if (!change->cancelled)
    {
        /* Report the error if it's an error. */
        eel_timed_wait_stop ((EelCancelCallback) cancel_group_change_callback, change);
        nautilus_report_error_setting_group (change->file, error, change->window);
    }

    self = NAUTILUS_PROPERTIES_WINDOW (change->window);
    if (self->group_change == change)
    {
        self->group_change = NULL;
    }

    group_change_free (change);
}

static void
cancel_group_change_callback (GroupChange *change)
{
    g_assert (NAUTILUS_IS_FILE (change->file));
    g_assert (change->group != NULL);

    change->cancelled = TRUE;
    nautilus_file_cancel (change->file, (NautilusFileOperationCallback) group_change_callback, change);
}

static gboolean
schedule_group_change_timeout (GroupChange *change)
{
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (change->window));
    g_assert (NAUTILUS_IS_FILE (change->file));
    g_assert (change->group != NULL);

    change->timeout = 0;

    eel_timed_wait_start
        ((EelCancelCallback) cancel_group_change_callback,
        change,
        _("Cancel Group Change?"),
        change->window);

    nautilus_file_set_group
        (change->file, change->group,
        (NautilusFileOperationCallback) group_change_callback, change);

    return FALSE;
}

static void
schedule_group_change (NautilusPropertiesWindow *self,
                       NautilusFile             *file,
                       const char               *group)
{
    GroupChange *change;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));
    g_assert (self->group_change == NULL);
    g_assert (NAUTILUS_IS_FILE (file));

    change = g_new0 (GroupChange, 1);

    change->file = nautilus_file_ref (file);
    change->group = g_strdup (group);
    change->window = GTK_WINDOW (g_object_ref (self));
    change->timeout =
        g_timeout_add (CHOWN_CHGRP_TIMEOUT,
                       (GSourceFunc) schedule_group_change_timeout,
                       change);

    self->group_change = change;
}

static void
unschedule_or_cancel_group_change (NautilusPropertiesWindow *self)
{
    GroupChange *change;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    change = self->group_change;

    if (change != NULL)
    {
        if (change->timeout == 0)
        {
            /* The operation was started, cancel it and let the operation callback free the change */
            cancel_group_change_callback (change);
            eel_timed_wait_stop ((EelCancelCallback) cancel_group_change_callback, change);
        }
        else
        {
            g_source_remove (change->timeout);
            group_change_free (change);
        }

        self->group_change = NULL;
    }
}

static void
changed_group_callback (GtkComboBox  *combo_box,
                        NautilusFile *file)
{
    NautilusPropertiesWindow *self;
    g_autofree char *group = NULL;
    g_autofree char *cur_group = NULL;

    g_assert (GTK_IS_COMBO_BOX (combo_box));
    g_assert (NAUTILUS_IS_FILE (file));

    group = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box));
    cur_group = nautilus_file_get_group_name (file);

    if (group != NULL && strcmp (group, cur_group) != 0)
    {
        /* Try to change file group. If this fails, complain to user. */
        self = NAUTILUS_PROPERTIES_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (combo_box), GTK_TYPE_WINDOW));

        unschedule_or_cancel_group_change (self);
        schedule_group_change (self, file, group);
    }
}

/* checks whether the given column at the first level
 * of model has the specified entries in the given order. */
static gboolean
tree_model_entries_equal (GtkTreeModel *model,
                          unsigned int  column,
                          GList        *entries)
{
    GtkTreeIter iter;
    gboolean empty_model;

    g_assert (GTK_IS_TREE_MODEL (model));
    g_assert (gtk_tree_model_get_column_type (model, column) == G_TYPE_STRING);

    empty_model = !gtk_tree_model_get_iter_first (model, &iter);

    if (!empty_model && entries != NULL)
    {
        GList *l;

        l = entries;

        do
        {
            g_autofree char *val = NULL;

            gtk_tree_model_get (model, &iter,
                                column, &val,
                                -1);
            if ((val == NULL && l->data != NULL) ||
                (val != NULL && l->data == NULL) ||
                (val != NULL && strcmp (val, l->data)))
            {
                return FALSE;
            }

            l = l->next;
        }
        while (gtk_tree_model_iter_next (model, &iter));

        return l == NULL;
    }
    else
    {
        return (empty_model && entries == NULL) ||
               (!empty_model && entries != NULL);
    }
}

static char *
combo_box_get_active_entry (GtkComboBox *combo_box,
                            unsigned int column)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *val;

    g_assert (GTK_IS_COMBO_BOX (combo_box));

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter))
    {
        model = gtk_combo_box_get_model (combo_box);
        g_assert (GTK_IS_TREE_MODEL (model));

        gtk_tree_model_get (model, &iter,
                            column, &val,
                            -1);
        return val;
    }

    return NULL;
}

/* returns the index of the given entry in the the given column
 * at the first level of model. Returns -1 if entry can't be found
 * or entry is NULL.
 * */
static int
tree_model_get_entry_index (GtkTreeModel *model,
                            unsigned int  column,
                            const char   *entry)
{
    GtkTreeIter iter;
    int index;
    gboolean empty_model;

    g_assert (GTK_IS_TREE_MODEL (model));
    g_assert (gtk_tree_model_get_column_type (model, column) == G_TYPE_STRING);

    empty_model = !gtk_tree_model_get_iter_first (model, &iter);
    if (!empty_model && entry != NULL)
    {
        index = 0;

        do
        {
            g_autofree char *val = NULL;

            gtk_tree_model_get (model, &iter,
                                column, &val,
                                -1);
            if (val != NULL && !strcmp (val, entry))
            {
                return index;
            }

            index++;
        }
        while (gtk_tree_model_iter_next (model, &iter));
    }

    return -1;
}


static void
synch_groups_combo_box (GtkComboBox  *combo_box,
                        NautilusFile *file)
{
    GList *groups = NULL;
    GList *node;
    GtkTreeModel *model;
    GtkListStore *store;
    const char *group_name;
    g_autofree char *current_group_name = NULL;
    int group_index;
    int current_group_index;

    g_assert (GTK_IS_COMBO_BOX (combo_box));
    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_gone (file))
    {
        return;
    }

    groups = nautilus_file_get_settable_group_names (file);

    model = gtk_combo_box_get_model (combo_box);
    store = GTK_LIST_STORE (model);
    g_assert (GTK_IS_LIST_STORE (model));

    if (!tree_model_entries_equal (model, 0, groups))
    {
        /* Clear the contents of ComboBox in a wacky way because there
         * is no function to clear all items and also no function to obtain
         * the number of items in a combobox.
         */
        gtk_list_store_clear (store);

        for (node = groups, group_index = 0; node != NULL; node = node->next, ++group_index)
        {
            group_name = (const char *) node->data;
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), group_name);
        }
    }

    current_group_name = nautilus_file_get_group_name (file);
    current_group_index = tree_model_get_entry_index (model, 0, current_group_name);

    /* If current group wasn't in list, we prepend it (with a separator).
     * This can happen if the current group is an id with no matching
     * group in the groups file.
     */
    if (current_group_index < 0 && current_group_name != NULL)
    {
        if (groups != NULL)
        {
            /* add separator */
            gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (combo_box), "-");
        }

        gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (combo_box), current_group_name);
        current_group_index = 0;
    }
    gtk_combo_box_set_active (combo_box, current_group_index);

    g_list_free_full (groups, g_free);
}

static gboolean
combo_box_row_separator_func (GtkTreeModel *model,
                              GtkTreeIter  *iter,
                              gpointer      data)
{
    g_autofree gchar *text = NULL;

    gtk_tree_model_get (model, iter, 0, &text, -1);

    if (text == NULL)
    {
        return FALSE;
    }

    if (strcmp (text, "-") == 0)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void
setup_group_combo_box (GtkWidget    *combo_box,
                       NautilusFile *file)
{
    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo_box),
                                          combo_box_row_separator_func,
                                          NULL,
                                          NULL);

    synch_groups_combo_box (GTK_COMBO_BOX (combo_box), file);

    /* Connect to signal to update menu when file changes. */
    g_signal_connect_object (file, "changed",
                             G_CALLBACK (synch_groups_combo_box),
                             combo_box, G_CONNECT_SWAPPED);
    g_signal_connect_data (combo_box, "changed",
                           G_CALLBACK (changed_group_callback),
                           nautilus_file_ref (file),
                           (GClosureNotify) nautilus_file_unref, 0);
}

static void
owner_change_free (OwnerChange *change)
{
    nautilus_file_unref (change->file);
    g_free (change->owner);
    g_object_unref (change->window);

    g_free (change);
}

static void
owner_change_callback (NautilusFile *file,
                       GFile        *result_location,
                       GError       *error,
                       OwnerChange  *change)
{
    NautilusPropertiesWindow *self;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (change->window));
    g_assert (NAUTILUS_IS_FILE (change->file));
    g_assert (change->owner != NULL);

    if (!change->cancelled)
    {
        /* Report the error if it's an error. */
        eel_timed_wait_stop ((EelCancelCallback) cancel_owner_change_callback, change);
        nautilus_report_error_setting_owner (file, error, change->window);
    }

    self = NAUTILUS_PROPERTIES_WINDOW (change->window);
    if (self->owner_change == change)
    {
        self->owner_change = NULL;
    }

    owner_change_free (change);
}

static void
cancel_owner_change_callback (OwnerChange *change)
{
    g_assert (NAUTILUS_IS_FILE (change->file));
    g_assert (change->owner != NULL);

    change->cancelled = TRUE;
    nautilus_file_cancel (change->file, (NautilusFileOperationCallback) owner_change_callback, change);
}

static gboolean
schedule_owner_change_timeout (OwnerChange *change)
{
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (change->window));
    g_assert (NAUTILUS_IS_FILE (change->file));
    g_assert (change->owner != NULL);

    change->timeout = 0;

    eel_timed_wait_start
        ((EelCancelCallback) cancel_owner_change_callback,
        change,
        _("Cancel Owner Change?"),
        change->window);

    nautilus_file_set_owner
        (change->file, change->owner,
        (NautilusFileOperationCallback) owner_change_callback, change);

    return FALSE;
}

static void
schedule_owner_change (NautilusPropertiesWindow *self,
                       NautilusFile             *file,
                       const char               *owner)
{
    OwnerChange *change;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));
    g_assert (self->owner_change == NULL);
    g_assert (NAUTILUS_IS_FILE (file));

    change = g_new0 (OwnerChange, 1);

    change->file = nautilus_file_ref (file);
    change->owner = g_strdup (owner);
    change->window = GTK_WINDOW (g_object_ref (self));
    change->timeout =
        g_timeout_add (CHOWN_CHGRP_TIMEOUT,
                       (GSourceFunc) schedule_owner_change_timeout,
                       change);

    self->owner_change = change;
}

static void
unschedule_or_cancel_owner_change (NautilusPropertiesWindow *self)
{
    OwnerChange *change;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    change = self->owner_change;

    if (change != NULL)
    {
        g_assert (NAUTILUS_IS_FILE (change->file));

        if (change->timeout == 0)
        {
            /* The operation was started, cancel it and let the operation callback free the change */
            cancel_owner_change_callback (change);
            eel_timed_wait_stop ((EelCancelCallback) cancel_owner_change_callback, change);
        }
        else
        {
            g_source_remove (change->timeout);
            owner_change_free (change);
        }

        self->owner_change = NULL;
    }
}

static void
changed_owner_callback (GtkComboBox  *combo_box,
                        NautilusFile *file)
{
    NautilusPropertiesWindow *self;
    g_autofree char *new_owner = NULL;
    g_autofree char *cur_owner = NULL;

    g_assert (GTK_IS_COMBO_BOX (combo_box));
    g_assert (NAUTILUS_IS_FILE (file));

    new_owner = combo_box_get_active_entry (combo_box, 2);
    if (!new_owner)
    {
        return;
    }
    cur_owner = nautilus_file_get_owner_name (file);

    if (strcmp (new_owner, cur_owner) != 0)
    {
        /* Try to change file owner. If this fails, complain to user. */
        self = NAUTILUS_PROPERTIES_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (combo_box), GTK_TYPE_WINDOW));

        unschedule_or_cancel_owner_change (self);
        schedule_owner_change (self, file, new_owner);
    }
}

static void
synch_user_menu (GtkComboBox  *combo_box,
                 NautilusFile *file)
{
    GList *users = NULL;
    GList *node;
    GtkTreeModel *model;
    GtkListStore *store;
    GtkTreeIter iter;
    char *user_name;
    g_autofree char *owner_name = NULL;
    g_autofree char *nice_owner_name = NULL;
    int user_index;
    int owner_index;

    g_assert (GTK_IS_COMBO_BOX (combo_box));
    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_gone (file))
    {
        return;
    }

    users = nautilus_get_user_names ();

    model = gtk_combo_box_get_model (combo_box);
    store = GTK_LIST_STORE (model);
    g_assert (GTK_IS_LIST_STORE (model));

    if (!tree_model_entries_equal (model, 1, users))
    {
        /* Clear the contents of ComboBox in a wacky way because there
         * is no function to clear all items and also no function to obtain
         * the number of items in a combobox.
         */
        gtk_list_store_clear (store);

        for (node = users, user_index = 0; node != NULL; node = node->next, ++user_index)
        {
            g_auto (GStrv) name_array = NULL;
            g_autofree char *combo_text = NULL;

            user_name = (char *) node->data;

            name_array = g_strsplit (user_name, "\n", 2);
            if (name_array[1] != NULL && *name_array[1] != 0)
            {
                combo_text = g_strdup_printf ("%s - %s", name_array[0], name_array[1]);
            }
            else
            {
                combo_text = g_strdup (name_array[0]);
            }

            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                0, combo_text,
                                1, user_name,
                                2, name_array[0],
                                -1);
        }
    }

    owner_name = nautilus_file_get_owner_name (file);
    owner_index = tree_model_get_entry_index (model, 2, owner_name);
    nice_owner_name = nautilus_file_get_string_attribute (file, "owner");

    /* If owner wasn't in list, we prepend it (with a separator).
     * This can happen if the owner is an id with no matching
     * identifier in the passwords file.
     */
    if (owner_index < 0 && owner_name != NULL)
    {
        if (users != NULL)
        {
            /* add separator */
            gtk_list_store_prepend (store, &iter);
            gtk_list_store_set (store, &iter,
                                0, "-",
                                1, NULL,
                                2, NULL,
                                -1);
        }

        owner_index = 0;

        gtk_list_store_prepend (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, nice_owner_name,
                            1, owner_name,
                            2, owner_name,
                            -1);
    }

    gtk_combo_box_set_active (combo_box, owner_index);

    g_list_free_full (users, g_free);
}

static void
setup_owner_combo_box (GtkWidget    *combo_box,
                       NautilusFile *file)
{
    g_autoptr (GtkTreeModel) model = NULL;
    GtkCellRenderer *renderer;

    model = GTK_TREE_MODEL (gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));
    gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), model);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box), renderer,
                                   "text", 0);

    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo_box),
                                          combo_box_row_separator_func,
                                          NULL,
                                          NULL);

    synch_user_menu (GTK_COMBO_BOX (combo_box), file);

    /* Connect to signal to update menu when file changes. */
    g_signal_connect_object (file, "changed",
                             G_CALLBACK (synch_user_menu),
                             combo_box, G_CONNECT_SWAPPED);
    g_signal_connect_data (combo_box, "changed",
                           G_CALLBACK (changed_owner_callback),
                           nautilus_file_ref (file),
                           (GClosureNotify) nautilus_file_unref, 0);
}

static gboolean
file_has_prefix (NautilusFile *file,
                 GList        *prefix_candidates)
{
    GList *p;
    g_autoptr (GFile) location = NULL;

    location = nautilus_file_get_location (file);

    for (p = prefix_candidates; p != NULL; p = p->next)
    {
        g_autoptr (GFile) candidate_location = NULL;

        if (file == p->data)
        {
            continue;
        }

        candidate_location = nautilus_file_get_location (NAUTILUS_FILE (p->data));
        if (g_file_has_prefix (location, candidate_location))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
directory_contents_value_field_update (NautilusPropertiesWindow *self)
{
    NautilusRequestStatus file_status;
    g_autofree char *text = NULL;
    guint directory_count;
    guint file_count;
    guint total_count;
    guint unreadable_directory_count;
    goffset total_size;
    NautilusFile *file;
    GList *l;
    guint file_unreadable;
    goffset file_size;
    gboolean deep_count_active;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    total_count = 0;
    total_size = 0;
    unreadable_directory_count = FALSE;

    for (l = self->target_files; l; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (file_has_prefix (file, self->target_files))
        {
            /* don't count nested files twice */
            continue;
        }

        if (nautilus_file_is_directory (file))
        {
            file_status = nautilus_file_get_deep_counts (file,
                                                         &directory_count,
                                                         &file_count,
                                                         &file_unreadable,
                                                         &file_size,
                                                         TRUE);
            total_count += (file_count + directory_count);
            total_size += file_size;

            if (file_unreadable)
            {
                unreadable_directory_count = TRUE;
            }

            if (file_status == NAUTILUS_REQUEST_DONE)
            {
                stop_deep_count_for_file (self, file);
            }
        }
        else
        {
            ++total_count;
            total_size += nautilus_file_get_size (file);
        }
    }

    deep_count_active = (self->deep_count_files != NULL);
    /* If we've already displayed the total once, don't do another visible
     * count-up if the deep_count happens to get invalidated.
     * But still display the new total, since it might have changed.
     */
    if (self->deep_count_finished && deep_count_active)
    {
        return;
    }

    text = NULL;

    if (total_count == 0)
    {
        if (!deep_count_active)
        {
            if (unreadable_directory_count == 0)
            {
                text = g_strdup (_("nothing"));
            }
            else
            {
                text = g_strdup (_("unreadable"));
            }
        }
        else
        {
            text = g_strdup ("…");
        }
    }
    else
    {
        g_autofree char *size_str = NULL;
        size_str = g_format_size (total_size);
        text = g_strdup_printf (ngettext ("%'d item, with size %s",
                                          "%'d items, totalling %s",
                                          total_count),
                                total_count, size_str);

        if (unreadable_directory_count != 0)
        {
            g_autofree char *temp = g_steal_pointer (&text);

            text = g_strconcat (temp, "\n",
                                _("(some contents unreadable)"),
                                NULL);
        }
    }

    gtk_label_set_text (GTK_LABEL (self->contents_value_label),
                        text);

    if (!deep_count_active)
    {
        self->deep_count_finished = TRUE;
        stop_spinner (self);
    }
}

static gboolean
update_directory_contents_callback (gpointer data)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (data);

    self->update_directory_contents_timeout_id = 0;
    directory_contents_value_field_update (self);

    return FALSE;
}

static void
schedule_directory_contents_update (NautilusPropertiesWindow *self)
{
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    if (self->update_directory_contents_timeout_id == 0)
    {
        self->update_directory_contents_timeout_id
            = g_timeout_add (DIRECTORY_CONTENTS_UPDATE_INTERVAL,
                             update_directory_contents_callback,
                             self);
    }
}

static void
setup_contents_field (NautilusPropertiesWindow *self,
                      GtkGrid                  *grid)
{
    g_list_foreach (self->target_files,
                    (GFunc) start_deep_count_for_file,
                    self);

    /* Fill in the initial value. */
    directory_contents_value_field_update (self);
}

static gboolean
is_root_directory (NautilusFile *file)
{
    g_autoptr (GFile) location = NULL;
    gboolean result;

    location = nautilus_file_get_location (file);
    result = nautilus_is_root_directory (location);

    return result;
}

static gboolean
is_network_directory (NautilusFile *file)
{
    g_autofree char *file_uri = NULL;

    file_uri = nautilus_file_get_uri (file);

    return strcmp (file_uri, "network:///") == 0;
}

static gboolean
is_burn_directory (NautilusFile *file)
{
    g_autofree char *file_uri = NULL;

    file_uri = nautilus_file_get_uri (file);

    return strcmp (file_uri, "burn:///") == 0;
}

static gboolean
should_show_custom_icon_buttons (NautilusPropertiesWindow *self)
{
    if (is_multi_file_window (self))
    {
        return FALSE;
    }

    return TRUE;
}

static gboolean
should_show_file_type (NautilusPropertiesWindow *self)
{
    if (!is_multi_file_window (self)
        && (nautilus_file_is_in_trash (get_target_file (self)) ||
            is_network_directory (get_target_file (self)) ||
            is_burn_directory (get_target_file (self))))
    {
        return FALSE;
    }


    return TRUE;
}

static gboolean
should_show_location_info (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        if (nautilus_file_is_in_trash (NAUTILUS_FILE (l->data)) ||
            is_root_directory (NAUTILUS_FILE (l->data)) ||
            is_network_directory (NAUTILUS_FILE (l->data)) ||
            is_burn_directory (NAUTILUS_FILE (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
should_show_trash_orig_path (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_in_trash (NAUTILUS_FILE (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
should_show_accessed_date (NautilusPropertiesWindow *self)
{
    /* Accessed date for directory seems useless. If we some
     * day decide that it is useful, we should separately
     * consider whether it's useful for "trash:".
     */
    if (nautilus_file_list_are_all_folders (self->target_files)
        || is_multi_file_window (self))
    {
        return FALSE;
    }

    return TRUE;
}

static gboolean
should_show_modified_date (NautilusPropertiesWindow *self)
{
    return !is_multi_file_window (self);
}

static gboolean
should_show_created_date (NautilusPropertiesWindow *self)
{
    return !is_multi_file_window (self);
}

static gboolean
should_show_trashed_on (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_in_trash (NAUTILUS_FILE (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
should_show_link_target (NautilusPropertiesWindow *self)
{
    if (!is_multi_file_window (self)
        && nautilus_file_is_symbolic_link (get_target_file (self)))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
should_show_free_space (NautilusPropertiesWindow *self)
{
    if (!is_multi_file_window (self)
        && (nautilus_file_is_in_trash (get_target_file (self)) ||
            is_network_directory (get_target_file (self)) ||
            nautilus_file_is_in_recent (get_target_file (self)) ||
            is_burn_directory (get_target_file (self))))
    {
        return FALSE;
    }

    if (nautilus_file_list_are_all_folders (self->target_files))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
should_show_volume_info (NautilusPropertiesWindow *self)
{
    NautilusFile *file;

    if (is_multi_file_window (self))
    {
        return FALSE;
    }

    file = get_original_file (self);

    if (file == NULL)
    {
        return FALSE;
    }

    if (nautilus_file_can_unmount (file))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
should_show_volume_usage (NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    gboolean success = FALSE;

    if (is_multi_file_window (self))
    {
        return FALSE;
    }

    file = get_original_file (self);

    if (file == NULL)
    {
        return FALSE;
    }

    if (nautilus_file_can_unmount (file))
    {
        return TRUE;
    }

    success = is_root_directory (file);

#ifdef TODO_GIO
    /* Look at is_mountpoint for activation uri */
#endif

    return success;
}

static void
paint_legend (GtkWidget *widget,
              cairo_t   *cr,
              gpointer   data)
{
    GtkStyleContext *context;
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);
    context = gtk_widget_get_style_context (widget);

    gtk_render_background (context, cr, 0, 0, allocation.width, allocation.height);
    gtk_render_frame (context, cr, 0, 0, allocation.width, allocation.height);
}

static void
paint_slice (GtkWidget   *widget,
             cairo_t     *cr,
             double       percent_start,
             double       percent_width,
             const gchar *style_class)
{
    double angle1;
    double angle2;
    gboolean full;
    double offset = G_PI / 2.0;
    GdkRGBA fill;
    GdkRGBA stroke;
    GtkStateFlags state;
    GtkBorder border;
    GtkStyleContext *context;
    double x, y, radius;
    gint width, height;

    if (percent_width < .01)
    {
        return;
    }

    context = gtk_widget_get_style_context (widget);
    state = gtk_style_context_get_state (context);
    gtk_style_context_get_border (context, state, &border);

    gtk_style_context_save (context);
    gtk_style_context_add_class (context, style_class);
    gtk_style_context_get_color (context, state, &fill);
    gtk_style_context_add_class (context, "border");
    gtk_style_context_get_color (context, state, &stroke);
    gtk_style_context_restore (context);

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);
    x = width / 2;
    y = height / 2;

    if (width < height)
    {
        radius = (width - border.left) / 2;
    }
    else
    {
        radius = (height - border.top) / 2;
    }

    angle1 = (percent_start * 2 * G_PI) - offset;
    angle2 = angle1 + (percent_width * 2 * G_PI);

    full = (percent_width > .99);

    if (!full)
    {
        cairo_move_to (cr, x, y);
    }
    cairo_arc (cr, x, y, radius, angle1, angle2);

    if (!full)
    {
        cairo_line_to (cr, x, y);
    }

    cairo_set_line_width (cr, border.top);
    gdk_cairo_set_source_rgba (cr, &fill);
    cairo_fill_preserve (cr);

    gdk_cairo_set_source_rgba (cr, &stroke);
    cairo_stroke (cr);
}

static void
paint_pie_chart (GtkWidget *widget,
                 cairo_t   *cr,
                 gpointer   data)
{
    NautilusPropertiesWindow *self;
    double free, used, reserved;

    self = NAUTILUS_PROPERTIES_WINDOW (data);

    free = (double) self->volume_free / (double) self->volume_capacity;
    used = (double) self->volume_used / (double) self->volume_capacity;
    reserved = 1.0 - (used + free);

    paint_slice (widget, cr,
                 0, free, "free");
    paint_slice (widget, cr,
                 free + used, reserved, "unknown");
    /* paint the used last so its slice strokes are on top */
    paint_slice (widget, cr,
                 free, used, "used");
}

static void
setup_pie_widget (NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    g_autofree gchar *capacity = NULL;
    g_autofree gchar *used = NULL;
    g_autofree gchar *free = NULL;
    const char *fs_type;
    g_autofree gchar *uri = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFileInfo) info = NULL;

    capacity = g_format_size (self->volume_capacity);
    free = g_format_size (self->volume_free);
    used = g_format_size (self->volume_used);

    file = get_original_file (self);

    uri = nautilus_file_get_activation_uri (file);

    /* Translators: "used" refers to the capacity of the filesystem */
    gtk_label_set_text (GTK_LABEL (self->used_value), used);

    /* Translators: "free" refers to the capacity of the filesystem */
    gtk_label_set_text (GTK_LABEL (self->free_value), free);

    gtk_label_set_text (GTK_LABEL (self->total_capacity_value), capacity);

    gtk_label_set_text (GTK_LABEL (self->file_system_value), NULL);

    location = g_file_new_for_uri (uri);
    info = g_file_query_filesystem_info (location, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
                                         NULL, NULL);
    if (info)
    {
        fs_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
        if (fs_type != NULL)
        {
            gtk_label_set_text (GTK_LABEL (self->file_system_value), fs_type);
        }
    }

    g_signal_connect (self->pie_chart, "draw",
                      G_CALLBACK (paint_pie_chart), self);
    g_signal_connect (self->used_color, "draw",
                      G_CALLBACK (paint_legend), self);
    g_signal_connect (self->free_color, "draw",
                      G_CALLBACK (paint_legend), self);
}

static void
setup_volume_usage_widget (NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    g_autofree gchar *uri = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFileInfo) info = NULL;

    file = get_original_file (self);

    uri = nautilus_file_get_activation_uri (file);

    location = g_file_new_for_uri (uri);
    info = g_file_query_filesystem_info (location, "filesystem::*", NULL, NULL);

    if (info)
    {
        self->volume_capacity = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        self->volume_free = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED))
        {
            self->volume_used = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED);
        }
        else
        {
            self->volume_used = self->volume_capacity - self->volume_free;
        }
    }
    else
    {
        self->volume_capacity = 0;
        self->volume_free = 0;
        self->volume_used = 0;
    }

    if (self->volume_capacity > 0)
    {
        setup_pie_widget (self);
    }
}

static void
open_in_disks (GtkButton                *button,
               NautilusPropertiesWindow *self)
{
    g_autofree char *message = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GAppInfo) app_info = NULL;

    app_info = g_app_info_create_from_commandline ("gnome-disks",
                                                   NULL,
                                                   G_APP_INFO_CREATE_NONE,
                                                   NULL);

    g_app_info_launch (app_info, NULL, NULL, &error);

    if (error != NULL)
    {
        message = g_strdup_printf (_("Details: %s"), error->message);
        show_dialog (_("There was an error launching the application."),
                     message,
                     GTK_WINDOW (self),
                     GTK_MESSAGE_ERROR);
    }
}

static void
setup_basic_page (NautilusPropertiesWindow *self)
{
    GtkGrid *grid;

    /* Icon pixmap */

    setup_image_widget (self, should_show_custom_icon_buttons (self));

    self->icon_chooser = NULL;

    /* Grid */

    grid = self->basic_grid;

    update_name_field (self);

    g_signal_connect_object (self->name_field, "notify::has-focus",
                             G_CALLBACK (name_field_focus_changed), self, 0);
    g_signal_connect_object (self->name_field, "activate",
                             G_CALLBACK (name_field_activate), self, 0);

    if (should_show_file_type (self))
    {
        gtk_widget_show (self->type_title_label);
        gtk_widget_show (self->type_value_label);
        g_object_set_data_full (G_OBJECT (self->type_value_label), "file_attribute",
                                g_strdup ("detailed_type"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->type_value_label);
    }

    if (should_show_link_target (self))
    {
        gtk_widget_show (self->link_target_title_label);
        gtk_widget_show (self->link_target_value_label);
        g_object_set_data_full (G_OBJECT (self->link_target_value_label), "file_attribute",
                                g_strdup ("link_target"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->link_target_value_label);
    }

    if (is_multi_file_window (self) ||
        nautilus_file_is_directory (get_target_file (self)))
    {
        gtk_widget_show (self->contents_title_label);
        gtk_widget_show (self->contents_value_label);
        setup_contents_field (self, grid);
    }
    else
    {
        gtk_widget_show (self->size_title_label);
        gtk_widget_show (self->size_value_label);

        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->size_value_label), "file_attribute",
                                g_strdup ("size_detail"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->size_value_label);
    }

    if (should_show_location_info (self))
    {
        gtk_widget_show (self->parent_folder_title_label);
        gtk_widget_show (self->parent_folder_value_label);

        g_object_set_data_full (G_OBJECT (self->parent_folder_value_label), "file_attribute",
                                g_strdup ("where"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->parent_folder_value_label);
    }

    if (should_show_trash_orig_path (self))
    {
        gtk_widget_show (self->original_folder_title_label);
        gtk_widget_show (self->original_folder_value_label);
        g_object_set_data_full (G_OBJECT (self->original_folder_value_label), "file_attribute",
                                g_strdup ("trash_orig_path"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->original_folder_value_label);
    }

    if (should_show_volume_info (self))
    {
        gtk_widget_show (self->volume_title_label);
        gtk_widget_show (self->volume_value_label);
        g_object_set_data_full (G_OBJECT (self->volume_value_label), "file_attribute",
                                g_strdup ("volume"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->volume_value_label);
    }

    if (should_show_trashed_on (self))
    {
        gtk_widget_show (self->trashed_on_title_label);
        gtk_widget_show (self->trashed_on_value_label);
        g_object_set_data_full (G_OBJECT (self->trashed_on_value_label), "file_attribute",
                                g_strdup ("trashed_on_full"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->trashed_on_value_label);
    }

    if (should_show_accessed_date (self)
        || should_show_modified_date (self)
        || should_show_created_date (self))
    {
        gtk_widget_show (self->spacer_2);
    }

    if (should_show_accessed_date (self))
    {
        gtk_widget_show (self->accessed_title_label);
        gtk_widget_show (self->accessed_value_label);
        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->accessed_value_label), "file_attribute",
                                g_strdup ("date_accessed_full"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->accessed_value_label);
    }

    if (should_show_modified_date (self))
    {
        gtk_widget_show (self->modified_title_label);
        gtk_widget_show (self->modified_value_label);
        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->modified_value_label), "file_attribute",
                                g_strdup ("date_modified_full"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->modified_value_label);
    }

    if (should_show_created_date (self))
    {
        gtk_widget_show (self->created_title_label);
        gtk_widget_show (self->created_value_label);
        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->created_value_label), "file_attribute",
                                g_strdup ("date_created_full"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->created_value_label);
    }

    if (should_show_free_space (self)
        && !should_show_volume_usage (self))
    {
        gtk_widget_show (self->spacer_3);
        gtk_widget_show (self->free_space_title_label);
        gtk_widget_show (self->free_space_value_label);

        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->free_space_value_label), "file_attribute",
                                g_strdup ("free_space"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->free_space_value_label);
    }

    if (should_show_volume_usage (self))
    {
        gtk_widget_show (self->volume_widget_box);
        gtk_widget_show (self->open_in_disks_button);
        setup_volume_usage_widget (self);
        /*Translators: Here Disks mean the name of application GNOME Disks.*/
        g_signal_connect (self->open_in_disks_button, "clicked", G_CALLBACK (open_in_disks), NULL);
    }
}

static gboolean
files_has_directory (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        file = NAUTILUS_FILE (l->data);
        if (nautilus_file_is_directory (file))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
files_has_changable_permissions_directory (NautilusPropertiesWindow *self)
{
    GList *l;
    gboolean changable = FALSE;

    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        file = NAUTILUS_FILE (l->data);
        if (nautilus_file_is_directory (file) &&
            nautilus_file_can_get_permissions (file) &&
            nautilus_file_can_set_permissions (file))
        {
            changable = TRUE;
        }
        else
        {
            changable = FALSE;
            break;
        }
    }

    return changable;
}

static gboolean
files_has_file (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        file = NAUTILUS_FILE (l->data);
        if (!nautilus_file_is_directory (file))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
start_long_operation (NautilusPropertiesWindow *self)
{
    if (self->long_operation_underway == 0)
    {
        /* start long operation */
        GdkDisplay *display;
        g_autoptr (GdkCursor) cursor = NULL;

        display = gtk_widget_get_display (GTK_WIDGET (self));
        cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (self)), cursor);
    }
    self->long_operation_underway++;
}

static void
end_long_operation (NautilusPropertiesWindow *self)
{
    if (gtk_widget_get_window (GTK_WIDGET (self)) != NULL &&
        self->long_operation_underway == 1)
    {
        /* finished !! */
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (self)), NULL);
    }
    self->long_operation_underway--;
}

static void
permission_change_callback (NautilusFile *file,
                            GFile        *res_loc,
                            GError       *error,
                            gpointer      callback_data)
{
    g_autoptr (NautilusPropertiesWindow) self = NAUTILUS_PROPERTIES_WINDOW (callback_data);
    g_assert (self != NULL);

    end_long_operation (self);

    /* Report the error if it's an error. */
    nautilus_report_error_setting_permissions (file, error, GTK_WINDOW (self));
}

static void
update_permissions (NautilusPropertiesWindow *self,
                    guint32                   vfs_new_perm,
                    guint32                   vfs_mask,
                    gboolean                  is_folder,
                    gboolean                  apply_to_both_folder_and_dir,
                    gboolean                  use_original)
{
    GList *l;

    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        guint32 permissions;

        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_get_permissions (file))
        {
            continue;
        }

        if (!apply_to_both_folder_and_dir &&
            ((nautilus_file_is_directory (file) && !is_folder) ||
             (!nautilus_file_is_directory (file) && is_folder)))
        {
            continue;
        }

        permissions = nautilus_file_get_permissions (file);
        if (use_original)
        {
            gpointer ptr;
            if (g_hash_table_lookup_extended (self->initial_permissions,
                                              file, NULL, &ptr))
            {
                permissions = (permissions & ~vfs_mask) | (GPOINTER_TO_INT (ptr) & vfs_mask);
            }
        }
        else
        {
            permissions = (permissions & ~vfs_mask) | vfs_new_perm;
        }

        start_long_operation (self);
        g_object_ref (self);
        nautilus_file_set_permissions
            (file, permissions,
            permission_change_callback,
            self);
    }
}

static gboolean
initial_permission_state_consistent (NautilusPropertiesWindow *self,
                                     guint32                   mask,
                                     gboolean                  is_folder,
                                     gboolean                  both_folder_and_dir)
{
    GList *l;
    gboolean first;
    guint32 first_permissions;

    first = TRUE;
    first_permissions = 0;
    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        guint32 permissions;

        file = l->data;

        if (!both_folder_and_dir &&
            ((nautilus_file_is_directory (file) && !is_folder) ||
             (!nautilus_file_is_directory (file) && is_folder)))
        {
            continue;
        }

        permissions = GPOINTER_TO_INT (g_hash_table_lookup (self->initial_permissions,
                                                            file));

        if (first)
        {
            if ((permissions & mask) != mask &&
                (permissions & mask) != 0)
            {
                /* Not fully on or off -> inconsistent */
                return FALSE;
            }

            first_permissions = permissions;
            first = FALSE;
        }
        else if ((permissions & mask) != (first_permissions & mask))
        {
            /* Not same permissions as first -> inconsistent */
            return FALSE;
        }
    }
    return TRUE;
}

static void
permission_button_toggled (GtkToggleButton          *button,
                           NautilusPropertiesWindow *self)
{
    gboolean is_folder, is_special;
    guint32 permission_mask;
    gboolean inconsistent;
    gboolean on;

    permission_mask = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                          "permission"));
    is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                    "is-folder"));
    is_special = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                     "is-special"));

    if (gtk_toggle_button_get_active (button)
        && !gtk_toggle_button_get_inconsistent (button))
    {
        /* Go to the initial state unless the initial state was
         *  consistent, or we support recursive apply */
        inconsistent = TRUE;
        on = TRUE;

        if (initial_permission_state_consistent (self, permission_mask, is_folder, is_special))
        {
            inconsistent = FALSE;
            on = TRUE;
        }
    }
    else if (gtk_toggle_button_get_inconsistent (button)
             && !gtk_toggle_button_get_active (button))
    {
        inconsistent = FALSE;
        on = TRUE;
    }
    else
    {
        inconsistent = FALSE;
        on = FALSE;
    }

    g_signal_handlers_block_by_func (G_OBJECT (button),
                                     G_CALLBACK (permission_button_toggled),
                                     self);

    gtk_toggle_button_set_active (button, on);
    gtk_toggle_button_set_inconsistent (button, inconsistent);

    g_signal_handlers_unblock_by_func (G_OBJECT (button),
                                       G_CALLBACK (permission_button_toggled),
                                       self);

    update_permissions (self,
                        on ? permission_mask : 0,
                        permission_mask,
                        is_folder,
                        is_special,
                        inconsistent);
}

static void
permission_button_update (GtkToggleButton          *button,
                          NautilusPropertiesWindow *self)
{
    GList *l;
    gboolean all_set;
    gboolean all_unset;
    gboolean all_cannot_set;
    gboolean is_folder, is_special;
    gboolean no_match;
    gboolean sensitive;
    guint32 button_permission;

    button_permission = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                            "permission"));
    is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                    "is-folder"));
    is_special = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                                     "is-special"));

    all_set = TRUE;
    all_unset = TRUE;
    all_cannot_set = TRUE;
    no_match = TRUE;
    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        guint32 file_permissions;

        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_get_permissions (file))
        {
            continue;
        }

        if (!is_special &&
            ((nautilus_file_is_directory (file) && !is_folder) ||
             (!nautilus_file_is_directory (file) && is_folder)))
        {
            continue;
        }

        no_match = FALSE;

        file_permissions = nautilus_file_get_permissions (file);

        if ((file_permissions & button_permission) == button_permission)
        {
            all_unset = FALSE;
        }
        else if ((file_permissions & button_permission) == 0)
        {
            all_set = FALSE;
        }
        else
        {
            all_unset = FALSE;
            all_set = FALSE;
        }

        if (nautilus_file_can_set_permissions (file))
        {
            all_cannot_set = FALSE;
        }
    }

    sensitive = !all_cannot_set;

    g_signal_handlers_block_by_func (G_OBJECT (button),
                                     G_CALLBACK (permission_button_toggled),
                                     self);

    gtk_toggle_button_set_active (button, !all_unset);
    /* if actually inconsistent, or default value for file buttons
     *  if no files are selected. (useful for recursive apply) */
    gtk_toggle_button_set_inconsistent (button,
                                        (!all_unset && !all_set) ||
                                        (!is_folder && no_match));
    gtk_widget_set_sensitive (GTK_WIDGET (button), sensitive);

    g_signal_handlers_unblock_by_func (G_OBJECT (button),
                                       G_CALLBACK (permission_button_toggled),
                                       self);
}

static void
setup_execute_checkbox_with_label (NautilusPropertiesWindow *self,
                                   guint32                   permission_to_check)
{
    gboolean a11y_enabled;
    GtkLabel *label_for;

    label_for = GTK_LABEL (self->execute_label);
    gtk_widget_show (self->execute_label);
    gtk_widget_show (self->execute_checkbox);

    /* Load up the check_button with data we'll need when updating its state. */
    g_object_set_data (G_OBJECT (self->execute_checkbox), "permission",
                       GINT_TO_POINTER (permission_to_check));
    g_object_set_data (G_OBJECT (self->execute_checkbox), "properties_window",
                       self);
    g_object_set_data (G_OBJECT (self->execute_checkbox), "is-folder",
                       GINT_TO_POINTER (FALSE));

    self->permission_buttons =
        g_list_prepend (self->permission_buttons,
                        self->execute_checkbox);

    g_signal_connect_object (self->execute_checkbox, "toggled",
                             G_CALLBACK (permission_button_toggled),
                             self,
                             0);

    a11y_enabled = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (self->execute_checkbox));
    if (a11y_enabled && label_for != NULL)
    {
        AtkObject *atk_widget;
        AtkObject *atk_label;

        atk_label = gtk_widget_get_accessible (GTK_WIDGET (label_for));
        atk_widget = gtk_widget_get_accessible (self->execute_checkbox);

        /* Create the label -> widget relation */
        atk_object_add_relationship (atk_label, ATK_RELATION_LABEL_FOR, atk_widget);

        /* Create the widget -> label relation */
        atk_object_add_relationship (atk_widget, ATK_RELATION_LABELLED_BY, atk_label);
    }
}

enum
{
    UNIX_PERM_SUID = S_ISUID,
    UNIX_PERM_SGID = S_ISGID,
    UNIX_PERM_STICKY = 01000,           /* S_ISVTX not defined on all systems */
    UNIX_PERM_USER_READ = S_IRUSR,
    UNIX_PERM_USER_WRITE = S_IWUSR,
    UNIX_PERM_USER_EXEC = S_IXUSR,
    UNIX_PERM_USER_ALL = S_IRUSR | S_IWUSR | S_IXUSR,
    UNIX_PERM_GROUP_READ = S_IRGRP,
    UNIX_PERM_GROUP_WRITE = S_IWGRP,
    UNIX_PERM_GROUP_EXEC = S_IXGRP,
    UNIX_PERM_GROUP_ALL = S_IRGRP | S_IWGRP | S_IXGRP,
    UNIX_PERM_OTHER_READ = S_IROTH,
    UNIX_PERM_OTHER_WRITE = S_IWOTH,
    UNIX_PERM_OTHER_EXEC = S_IXOTH,
    UNIX_PERM_OTHER_ALL = S_IROTH | S_IWOTH | S_IXOTH
};

typedef enum
{
    PERMISSION_READ = (1 << 0),
    PERMISSION_WRITE = (1 << 1),
    PERMISSION_EXEC = (1 << 2)
} PermissionValue;

typedef enum
{
    PERMISSION_USER,
    PERMISSION_GROUP,
    PERMISSION_OTHER
} PermissionType;

static guint32 vfs_perms[3][3] =
{
    {UNIX_PERM_USER_READ, UNIX_PERM_USER_WRITE, UNIX_PERM_USER_EXEC},
    {UNIX_PERM_GROUP_READ, UNIX_PERM_GROUP_WRITE, UNIX_PERM_GROUP_EXEC},
    {UNIX_PERM_OTHER_READ, UNIX_PERM_OTHER_WRITE, UNIX_PERM_OTHER_EXEC},
};

static guint32
permission_to_vfs (PermissionType  type,
                   PermissionValue perm)
{
    guint32 vfs_perm;
    g_assert (type >= 0 && type < 3);

    vfs_perm = 0;
    if (perm & PERMISSION_READ)
    {
        vfs_perm |= vfs_perms[type][0];
    }
    if (perm & PERMISSION_WRITE)
    {
        vfs_perm |= vfs_perms[type][1];
    }
    if (perm & PERMISSION_EXEC)
    {
        vfs_perm |= vfs_perms[type][2];
    }

    return vfs_perm;
}


static PermissionValue
permission_from_vfs (PermissionType type,
                     guint32        vfs_perm)
{
    PermissionValue perm;
    g_assert (type >= 0 && type < 3);

    perm = 0;
    if (vfs_perm & vfs_perms[type][0])
    {
        perm |= PERMISSION_READ;
    }
    if (vfs_perm & vfs_perms[type][1])
    {
        perm |= PERMISSION_WRITE;
    }
    if (vfs_perm & vfs_perms[type][2])
    {
        perm |= PERMISSION_EXEC;
    }

    return perm;
}

static void
permission_combo_changed (GtkWidget                *combo,
                          NautilusPropertiesWindow *self)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    gboolean is_folder, use_original;
    PermissionType type;
    int new_perm, mask;
    guint32 vfs_new_perm, vfs_mask;

    is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "is-folder"));
    type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "permission-type"));

    if (is_folder)
    {
        mask = PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXEC;
    }
    else
    {
        mask = PERMISSION_READ | PERMISSION_WRITE;
    }

    vfs_mask = permission_to_vfs (type, mask);

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    {
        return;
    }
    gtk_tree_model_get (model, &iter, COLUMN_VALUE, &new_perm,
                        COLUMN_USE_ORIGINAL, &use_original, -1);
    vfs_new_perm = permission_to_vfs (type, new_perm);

    update_permissions (self, vfs_new_perm, vfs_mask,
                        is_folder, FALSE, use_original);
}

static void
permission_combo_add_multiple_choice (GtkComboBox *combo,
                                      GtkTreeIter *iter)
{
    GtkTreeModel *model;
    GtkListStore *store;
    gboolean found;

    model = gtk_combo_box_get_model (combo);
    store = GTK_LIST_STORE (model);

    found = FALSE;
    gtk_tree_model_get_iter_first (model, iter);
    do
    {
        gboolean multi;
        gtk_tree_model_get (model, iter, COLUMN_USE_ORIGINAL, &multi, -1);

        if (multi)
        {
            found = TRUE;
            break;
        }
    }
    while (gtk_tree_model_iter_next (model, iter));

    if (!found)
    {
        gtk_list_store_append (store, iter);
        gtk_list_store_set (store, iter,
                            COLUMN_NAME, "---",
                            COLUMN_VALUE, 0,
                            COLUMN_USE_ORIGINAL, TRUE, -1);
    }
}

static void
permission_combo_update (GtkComboBox              *combo,
                         NautilusPropertiesWindow *self)
{
    PermissionType type;
    PermissionValue perm, all_dir_perm, all_file_perm, all_perm;
    gboolean is_folder, no_files, no_dirs, all_file_same, all_dir_same, all_same;
    gboolean all_dir_cannot_set, all_file_cannot_set, sensitive;
    GtkTreeIter iter;
    int mask;
    GtkTreeModel *model;
    GtkListStore *store;
    GList *l;
    gboolean is_multi;

    model = gtk_combo_box_get_model (combo);

    is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "is-folder"));
    type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "permission-type"));

    is_multi = FALSE;
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    {
        gtk_tree_model_get (model, &iter, COLUMN_USE_ORIGINAL, &is_multi, -1);
    }

    no_files = TRUE;
    no_dirs = TRUE;
    all_dir_same = TRUE;
    all_file_same = TRUE;
    all_dir_perm = 0;
    all_file_perm = 0;
    all_dir_cannot_set = TRUE;
    all_file_cannot_set = TRUE;

    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        guint32 file_permissions;

        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_get_permissions (file))
        {
            continue;
        }

        if (nautilus_file_is_directory (file))
        {
            mask = PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXEC;
        }
        else
        {
            mask = PERMISSION_READ | PERMISSION_WRITE;
        }

        file_permissions = nautilus_file_get_permissions (file);

        perm = permission_from_vfs (type, file_permissions) & mask;

        if (nautilus_file_is_directory (file))
        {
            if (no_dirs)
            {
                all_dir_perm = perm;
                no_dirs = FALSE;
            }
            else if (perm != all_dir_perm)
            {
                all_dir_same = FALSE;
            }

            if (nautilus_file_can_set_permissions (file))
            {
                all_dir_cannot_set = FALSE;
            }
        }
        else
        {
            if (no_files)
            {
                all_file_perm = perm;
                no_files = FALSE;
            }
            else if (perm != all_file_perm)
            {
                all_file_same = FALSE;
            }

            if (nautilus_file_can_set_permissions (file))
            {
                all_file_cannot_set = FALSE;
            }
        }
    }

    if (is_folder)
    {
        all_same = all_dir_same;
        all_perm = all_dir_perm;
    }
    else
    {
        all_same = all_file_same && !no_files;
        all_perm = all_file_perm;
    }

    store = GTK_LIST_STORE (model);
    if (all_same)
    {
        gboolean found;

        found = FALSE;
        gtk_tree_model_get_iter_first (model, &iter);
        do
        {
            int current_perm;
            gtk_tree_model_get (model, &iter, 1, &current_perm, -1);

            if (current_perm == all_perm)
            {
                found = TRUE;
                break;
            }
        }
        while (gtk_tree_model_iter_next (model, &iter));

        if (!found)
        {
            g_autoptr (GString) str = NULL;

            str = g_string_new ("");

            if (!(all_perm & PERMISSION_READ))
            {
                /* translators: this gets concatenated to "no read",
                 * "no access", etc. (see following strings)
                 */
                g_string_append (str, _("no "));
            }
            if (is_folder)
            {
                g_string_append (str, _("list"));
            }
            else
            {
                g_string_append (str, _("read"));
            }

            g_string_append (str, ", ");

            if (!(all_perm & PERMISSION_WRITE))
            {
                g_string_append (str, _("no "));
            }
            if (is_folder)
            {
                g_string_append (str, _("create/delete"));
            }
            else
            {
                g_string_append (str, _("write"));
            }

            if (is_folder)
            {
                g_string_append (str, ", ");

                if (!(all_perm & PERMISSION_EXEC))
                {
                    g_string_append (str, _("no "));
                }
                g_string_append (str, _("access"));
            }

            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                0, str->str,
                                1, all_perm, -1);
        }
    }
    else
    {
        permission_combo_add_multiple_choice (combo, &iter);
    }

    g_signal_handlers_block_by_func (G_OBJECT (combo),
                                     G_CALLBACK (permission_combo_changed),
                                     self);

    gtk_combo_box_set_active_iter (combo, &iter);

    /* Also enable if no files found (for recursive
     *  file changes when only selecting folders) */
    if (is_folder)
    {
        sensitive = !all_dir_cannot_set;
    }
    else
    {
        sensitive = !all_file_cannot_set;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combo), sensitive);

    g_signal_handlers_unblock_by_func (G_OBJECT (combo),
                                       G_CALLBACK (permission_combo_changed),
                                       self);
}

static void
setup_permissions_combo_box (GtkComboBox    *combo,
                             PermissionType  type,
                             gboolean        is_folder)
{
    g_autoptr (GtkListStore) store = NULL;
    GtkCellRenderer *cell;
    GtkTreeIter iter;

    store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_STRING);
    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
    gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), COLUMN_ID);

    g_object_set_data (G_OBJECT (combo), "is-folder", GINT_TO_POINTER (is_folder));
    g_object_set_data (G_OBJECT (combo), "permission-type", GINT_TO_POINTER (type));

    if (is_folder)
    {
        if (type != PERMISSION_USER)
        {
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                /* Translators: this is referred to the permissions
                                 * the user has in a directory.
                                 */
                                COLUMN_NAME, _("None"),
                                COLUMN_VALUE, 0,
                                COLUMN_ID, "none",
                                -1);
        }
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_NAME, _("List files only"),
                            COLUMN_VALUE, PERMISSION_READ,
                            COLUMN_ID, "r",
                            -1);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_NAME, _("Access files"),
                            COLUMN_VALUE, PERMISSION_READ | PERMISSION_EXEC,
                            COLUMN_ID, "rx",
                            -1);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_NAME, _("Create and delete files"),
                            COLUMN_VALUE, PERMISSION_READ | PERMISSION_EXEC | PERMISSION_WRITE,
                            COLUMN_ID, "rwx",
                            -1);
    }
    else
    {
        if (type != PERMISSION_USER)
        {
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                COLUMN_NAME, _("None"),
                                COLUMN_VALUE, 0,
                                COLUMN_ID, "none",
                                -1);
        }
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_NAME, _("Read-only"),
                            COLUMN_VALUE, PERMISSION_READ,
                            COLUMN_ID, "r",
                            -1);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_NAME, _("Read and write"),
                            COLUMN_VALUE, PERMISSION_READ | PERMISSION_WRITE,
                            COLUMN_ID, "rw",
                            -1);
    }

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
                                    "text", COLUMN_NAME,
                                    NULL);
}

static gboolean
all_can_get_permissions (GList *file_list)
{
    GList *l;
    for (l = file_list; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_get_permissions (file))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
all_can_set_permissions (GList *file_list)
{
    GList *l;
    for (l = file_list; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_set_permissions (file))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static GHashTable *
get_initial_permissions (GList *file_list)
{
    GHashTable *ret;
    GList *l;

    ret = g_hash_table_new (g_direct_hash,
                            g_direct_equal);

    for (l = file_list; l != NULL; l = l->next)
    {
        guint32 permissions;
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        permissions = nautilus_file_get_permissions (file);
        g_hash_table_insert (ret, file,
                             GINT_TO_POINTER (permissions));
    }

    return ret;
}

static void
create_simple_permissions (NautilusPropertiesWindow *self,
                           GtkGrid                  *page_grid)
{
    gboolean has_directory;
    gboolean has_file;
    GtkWidget *owner_combo_box;
    GtkWidget *owner_value_label;
    GtkWidget *group_combo_box;
    GtkWidget *group_value_label;

    has_directory = files_has_directory (self);
    has_file = files_has_file (self);

    if (!is_multi_file_window (self) && nautilus_file_can_set_owner (get_target_file (self)))
    {
        /* Combo box in this case. */
        owner_combo_box = gtk_stack_get_child_by_name (GTK_STACK (self->owner_value_stack), "combo_box");
        gtk_stack_set_visible_child (GTK_STACK (self->owner_value_stack), owner_combo_box);
        setup_owner_combo_box (owner_combo_box, get_target_file (self));
    }
    else
    {
        /* Static text in this case. */
        owner_value_label = gtk_stack_get_child_by_name (GTK_STACK (self->owner_value_stack), "label");
        gtk_stack_set_visible_child (GTK_STACK (self->owner_value_stack), owner_value_label);

        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (owner_value_label), "file_attribute",
                                g_strdup ("owner"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             owner_value_label);
    }
    if (has_directory && has_file)
    {
        gtk_widget_show (self->owner_folder_access_label);
        gtk_widget_show (self->owner_folder_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->owner_folder_access_combo),
                                     PERMISSION_USER, TRUE);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->owner_folder_access_combo);
        g_signal_connect (self->owner_folder_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);

        gtk_widget_show (self->owner_file_access_label);
        gtk_widget_show (self->owner_file_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->owner_file_access_combo),
                                     PERMISSION_USER, FALSE);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->owner_file_access_combo);
        g_signal_connect (self->owner_file_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);
    }
    else
    {
        gtk_widget_show (self->owner_access_label);
        gtk_widget_show (self->owner_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->owner_access_combo),
                                     PERMISSION_USER, has_directory);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->owner_access_combo);
        g_signal_connect (self->owner_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);
    }

    if (!is_multi_file_window (self) && nautilus_file_can_set_group (get_target_file (self)))
    {
        /* Combo box in this case. */
        group_combo_box = gtk_stack_get_child_by_name (GTK_STACK (self->group_value_stack), "combo_box");
        gtk_stack_set_visible_child (GTK_STACK (self->group_value_stack), group_combo_box);
        setup_group_combo_box (group_combo_box, get_target_file (self));
    }
    else
    {
        group_value_label = gtk_stack_get_child_by_name (GTK_STACK (self->group_value_stack), "label");
        gtk_stack_set_visible_child (GTK_STACK (self->group_value_stack), group_value_label);

        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (group_value_label), "file_attribute",
                                g_strdup ("group"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             group_value_label);
    }

    if (has_directory && has_file)
    {
        gtk_widget_show (self->group_folder_access_label);
        gtk_widget_show (self->group_folder_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->group_folder_access_combo),
                                     PERMISSION_GROUP, TRUE);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->group_folder_access_combo);
        g_signal_connect (self->group_folder_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);

        gtk_widget_show (self->group_file_access_label);
        gtk_widget_show (self->group_file_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->group_file_access_combo),
                                     PERMISSION_GROUP, FALSE);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->group_file_access_combo);
        g_signal_connect (self->group_file_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);
    }
    else
    {
        gtk_widget_show (self->group_access_label);
        gtk_widget_show (self->group_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->group_access_combo),
                                     PERMISSION_GROUP, has_directory);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->group_access_combo);
        g_signal_connect (self->group_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);
    }

    /* Others Row */
    if (has_directory && has_file)
    {
        gtk_widget_show (self->others_folder_access_label);
        gtk_widget_show (self->others_folder_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->others_folder_access_combo),
                                     PERMISSION_OTHER, TRUE);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->others_folder_access_combo);
        g_signal_connect (self->others_folder_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);

        gtk_widget_show (self->others_file_access_label);
        gtk_widget_show (self->others_file_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->others_file_access_combo),
                                     PERMISSION_OTHER, FALSE);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->others_file_access_combo);
        g_signal_connect (self->others_file_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);
    }
    else
    {
        gtk_widget_show (self->others_access_label);
        gtk_widget_show (self->others_access_combo);
        setup_permissions_combo_box (GTK_COMBO_BOX (self->others_access_combo),
                                     PERMISSION_OTHER, has_directory);
        self->permission_combos = g_list_prepend (self->permission_combos,
                                                  self->others_access_combo);
        g_signal_connect (self->others_access_combo, "changed", G_CALLBACK (permission_combo_changed), self);
    }

    if (!has_directory)
    {
        setup_execute_checkbox_with_label (self,
                                           UNIX_PERM_USER_EXEC | UNIX_PERM_GROUP_EXEC | UNIX_PERM_OTHER_EXEC);
    }
}

static void
set_recursive_permissions_done (gboolean success,
                                gpointer callback_data)
{
    g_autoptr (NautilusPropertiesWindow) self = NAUTILUS_PROPERTIES_WINDOW (callback_data);
    end_long_operation (self);
}

static void
on_change_permissions_response (GtkDialog                *dialog,
                                int                       response,
                                NautilusPropertiesWindow *self)
{
    guint32 file_permission, file_permission_mask;
    guint32 dir_permission, dir_permission_mask;
    guint32 vfs_mask, vfs_new_perm;
    GtkWidget *combo;
    gboolean is_folder, use_original;
    GList *l;
    GtkTreeModel *model;
    GtkTreeIter iter;
    PermissionType type;
    int new_perm, mask;

    if (response != GTK_RESPONSE_OK)
    {
        g_clear_pointer (&self->change_permission_combos, g_list_free);
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }

    file_permission = 0;
    file_permission_mask = 0;
    dir_permission = 0;
    dir_permission_mask = 0;

    /* Simple mode, minus exec checkbox */
    for (l = self->change_permission_combos; l != NULL; l = l->next)
    {
        combo = l->data;

        if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
        {
            continue;
        }

        type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "permission-type"));
        is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "is-folder"));

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
        gtk_tree_model_get (model, &iter,
                            COLUMN_VALUE, &new_perm,
                            COLUMN_USE_ORIGINAL, &use_original, -1);
        if (use_original)
        {
            continue;
        }
        vfs_new_perm = permission_to_vfs (type, new_perm);

        if (is_folder)
        {
            mask = PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXEC;
        }
        else
        {
            mask = PERMISSION_READ | PERMISSION_WRITE;
        }
        vfs_mask = permission_to_vfs (type, mask);

        if (is_folder)
        {
            dir_permission_mask |= vfs_mask;
            dir_permission |= vfs_new_perm;
        }
        else
        {
            file_permission_mask |= vfs_mask;
            file_permission |= vfs_new_perm;
        }
    }

    for (l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_is_directory (file) &&
            nautilus_file_can_set_permissions (file))
        {
            g_autofree gchar *uri = NULL;

            uri = nautilus_file_get_uri (file);
            start_long_operation (self);
            g_object_ref (self);
            nautilus_file_set_permissions_recursive (uri,
                                                     file_permission,
                                                     file_permission_mask,
                                                     dir_permission,
                                                     dir_permission_mask,
                                                     set_recursive_permissions_done,
                                                     self);
        }
    }
    g_clear_pointer (&self->change_permission_combos, g_list_free);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
set_active_from_umask (GtkComboBox    *combo,
                       PermissionType  type,
                       gboolean        is_folder)
{
    mode_t initial;
    mode_t mask;
    mode_t p;
    const char *id;

    if (is_folder)
    {
        initial = (S_IRWXU | S_IRWXG | S_IRWXO);
    }
    else
    {
        initial = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    }

    umask (mask = umask (0));

    p = ~mask & initial;

    if (type == PERMISSION_USER)
    {
        p &= ~(S_IRWXG | S_IRWXO);
        if ((p & S_IRWXU) == S_IRWXU)
        {
            id = "rwx";
        }
        else if ((p & (S_IRUSR | S_IWUSR)) == (S_IRUSR | S_IWUSR))
        {
            id = "rw";
        }
        else if ((p & (S_IRUSR | S_IXUSR)) == (S_IRUSR | S_IXUSR))
        {
            id = "rx";
        }
        else if ((p & S_IRUSR) == S_IRUSR)
        {
            id = "r";
        }
        else
        {
            id = "none";
        }
    }
    else if (type == PERMISSION_GROUP)
    {
        p &= ~(S_IRWXU | S_IRWXO);
        if ((p & S_IRWXG) == S_IRWXG)
        {
            id = "rwx";
        }
        else if ((p & (S_IRGRP | S_IWGRP)) == (S_IRGRP | S_IWGRP))
        {
            id = "rw";
        }
        else if ((p & (S_IRGRP | S_IXGRP)) == (S_IRGRP | S_IXGRP))
        {
            id = "rx";
        }
        else if ((p & S_IRGRP) == S_IRGRP)
        {
            id = "r";
        }
        else
        {
            id = "none";
        }
    }
    else
    {
        p &= ~(S_IRWXU | S_IRWXG);
        if ((p & S_IRWXO) == S_IRWXO)
        {
            id = "rwx";
        }
        else if ((p & (S_IROTH | S_IWOTH)) == (S_IROTH | S_IWOTH))
        {
            id = "rw";
        }
        else if ((p & (S_IROTH | S_IXOTH)) == (S_IROTH | S_IXOTH))
        {
            id = "rx";
        }
        else if ((p & S_IROTH) == S_IROTH)
        {
            id = "r";
        }
        else
        {
            id = "none";
        }
    }

    gtk_combo_box_set_active_id (combo, id);
}

static void
on_change_permissions_clicked (GtkWidget                *button,
                               NautilusPropertiesWindow *self)
{
    GtkWidget *dialog;
    GtkComboBox *combo;
    g_autoptr (GtkBuilder) change_permissions_builder = NULL;

    change_permissions_builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-file-properties-change-permissions.ui");

    dialog = GTK_WIDGET (gtk_builder_get_object (change_permissions_builder, "change_permissions_dialog"));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (self));

    /* Owner Permissions */
    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "file_owner_combo"));
    setup_permissions_combo_box (combo, PERMISSION_USER, FALSE);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_USER, FALSE);

    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "folder_owner_combo"));
    setup_permissions_combo_box (combo, PERMISSION_USER, TRUE);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_USER, TRUE);

    /* Group Permissions */
    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "file_group_combo"));
    setup_permissions_combo_box (combo, PERMISSION_GROUP, FALSE);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_GROUP, FALSE);

    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "folder_group_combo"));
    setup_permissions_combo_box (combo, PERMISSION_GROUP, TRUE);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_GROUP, TRUE);

    /* Others Permissions */
    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "file_other_combo"));
    setup_permissions_combo_box (combo, PERMISSION_OTHER, FALSE);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_OTHER, FALSE);

    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "folder_other_combo"));
    setup_permissions_combo_box (combo, PERMISSION_OTHER, TRUE);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_OTHER, TRUE);

    g_signal_connect (dialog, "response", G_CALLBACK (on_change_permissions_response), self);
    gtk_widget_show_all (dialog);
}

static void
setup_permissions_page (NautilusPropertiesWindow *self)
{
    GList *file_list;

    file_list = self->original_files;

    self->initial_permissions = NULL;

    if (all_can_get_permissions (file_list) && all_can_get_permissions (self->target_files))
    {
        self->initial_permissions = get_initial_permissions (self->target_files);
        self->has_recursive_apply = files_has_changable_permissions_directory (self);

        if (!all_can_set_permissions (file_list))
        {
            gtk_widget_show (self->not_the_owner_label);
            gtk_widget_show (self->bottom_prompt_seperator);
        }

        gtk_widget_show (self->permissions_grid);
        create_simple_permissions (self, GTK_GRID (self->permissions_grid));

#ifdef HAVE_SELINUX
        gtk_widget_show (self->security_context_title_label);
        gtk_widget_show (self->security_context_value_label);

        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->security_context_value_label), "file_attribute",
                                g_strdup ("selinux_context"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->security_context_value_label);
#endif

        if (self->has_recursive_apply)
        {
            gtk_widget_show_all (self->change_permissions_button_box);
            g_signal_connect (self->change_permissions_button, "clicked",
                              G_CALLBACK (on_change_permissions_clicked),
                              self);
        }
    }
    else
    {
        /*
         *  This if block only gets executed if its a single file window,
         *  in which case the label text needs to be different from the
         *  default label text. The default label text for a multifile
         *  window is set in nautilus-properties-window.ui so no else block.
         */
        if (!is_multi_file_window (self))
        {
            g_autofree gchar *file_name = NULL;
            g_autofree gchar *prompt_text = NULL;

            file_name = nautilus_file_get_display_name (get_target_file (self));
            prompt_text = g_strdup_printf (_("The permissions of “%s” could not be determined."), file_name);
            gtk_label_set_text (GTK_LABEL (self->permission_indeterminable_label), prompt_text);
        }

        gtk_widget_show (self->permission_indeterminable_label);
    }
}

static void
append_extension_pages (NautilusPropertiesWindow *self)
{
    g_autolist (GObject) providers = NULL;
    GList *p;

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER);

    for (p = providers; p != NULL; p = p->next)
    {
        NautilusPropertyPageProvider *provider;
        g_autolist (NautilusPropertyPage) pages = NULL;
        GList *l;

        provider = NAUTILUS_PROPERTY_PAGE_PROVIDER (p->data);

        pages = nautilus_property_page_provider_get_pages
                    (provider, self->original_files);

        for (l = pages; l != NULL; l = l->next)
        {
            NautilusPropertyPage *page;
            g_autoptr (GtkWidget) page_widget = NULL;
            g_autoptr (GtkWidget) label = NULL;

            page = NAUTILUS_PROPERTY_PAGE (l->data);

            g_object_get (G_OBJECT (page),
                          "page", &page_widget, "label", &label,
                          NULL);

            gtk_notebook_append_page (self->notebook,
                                      page_widget, label);
            gtk_container_child_set (GTK_CONTAINER (self->notebook),
                                     page_widget,
                                     "tab-expand", TRUE,
                                     NULL);

            g_object_set_data (G_OBJECT (page_widget),
                               "is-extension-page",
                               GINT_TO_POINTER (TRUE));
        }
    }
}

static gboolean
should_show_permissions (NautilusPropertiesWindow *self)
{
    GList *l;

    /* Don't show permissions for Trash and Computer since they're not
     * really file system objects.
     */
    for (l = self->original_files; l != NULL; l = l->next)
    {
        if (nautilus_file_is_in_trash (NAUTILUS_FILE (l->data)) ||
            nautilus_file_is_in_recent (NAUTILUS_FILE (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static char *
get_pending_key (GList *file_list)
{
    GList *uris = NULL;
    GList *l;
    GString *key;
    char *ret;

    uris = NULL;
    for (l = file_list; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, nautilus_file_get_uri (NAUTILUS_FILE (l->data)));
    }
    uris = g_list_sort (uris, (GCompareFunc) strcmp);

    key = g_string_new ("");
    for (l = uris; l != NULL; l = l->next)
    {
        g_string_append (key, l->data);
        g_string_append (key, ";");
    }

    g_list_free_full (uris, g_free);

    ret = key->str;
    g_string_free (key, FALSE);

    return ret;
}

static StartupData *
startup_data_new (GList                            *original_files,
                  GList                            *target_files,
                  const char                       *pending_key,
                  GtkWidget                        *parent_widget,
                  GtkWindow                        *parent_window,
                  const char                       *startup_id,
                  NautilusPropertiesWindowCallback  callback,
                  gpointer                          callback_data,
                  NautilusPropertiesWindow         *window)
{
    StartupData *data;
    GList *l;

    data = g_new0 (StartupData, 1);
    data->original_files = nautilus_file_list_copy (original_files);
    data->target_files = nautilus_file_list_copy (target_files);
    data->parent_widget = parent_widget;
    data->parent_window = parent_window;
    data->startup_id = g_strdup (startup_id);
    data->pending_key = g_strdup (pending_key);
    data->pending_files = g_hash_table_new (g_direct_hash,
                                            g_direct_equal);
    data->callback = callback;
    data->callback_data = callback_data;
    data->window = window;

    for (l = data->target_files; l != NULL; l = l->next)
    {
        g_hash_table_insert (data->pending_files, l->data, l->data);
    }

    return data;
}

static void
startup_data_free (StartupData *data)
{
    nautilus_file_list_free (data->original_files);
    nautilus_file_list_free (data->target_files);
    g_hash_table_destroy (data->pending_files);
    g_free (data->pending_key);
    g_free (data->startup_id);
    g_free (data);
}

static void
file_changed_callback (NautilusFile *file,
                       gpointer      user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);

    if (!g_list_find (self->changed_files, file))
    {
        nautilus_file_ref (file);
        self->changed_files = g_list_prepend (self->changed_files, file);
        schedule_files_update (self);
    }
}

static gboolean
should_show_open_with (NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    g_autofree gchar *mime_type = NULL;
    g_autofree gchar *extension = NULL;
    gboolean hide;

    /* Don't show open with tab for desktop special icons (trash, etc)
     * or desktop files. We don't get the open-with menu for these anyway.
     *
     * Also don't show it for folders. Changing the default app for folders
     * leads to all sort of hard to understand errors.
     */

    if (is_multi_file_window (self))
    {
        GList *l;

        if (!file_list_attributes_identical (self->target_files,
                                             "mime_type"))
        {
            return FALSE;
        }

        for (l = self->target_files; l; l = l->next)
        {
            g_autoptr (GAppInfo) app_info = NULL;

            file = NAUTILUS_FILE (l->data);
            app_info = nautilus_mime_get_default_application_for_file (file);
            if (nautilus_file_is_directory (file) || !app_info || file == NULL)
            {
                return FALSE;
            }
        }

        /* since we just confirmed all the mime types are the
         *  same we only need to test one file */
        file = self->target_files->data;
    }
    else
    {
        g_autoptr (GAppInfo) app_info = NULL;

        file = get_target_file (self);
        app_info = nautilus_mime_get_default_application_for_file (file);
        if (nautilus_file_is_directory (file) || !app_info || file == NULL)
        {
            return FALSE;
        }
    }

    mime_type = nautilus_file_get_mime_type (file);
    extension = nautilus_file_get_extension (file);
    hide = (g_content_type_is_unknown (mime_type) && extension == NULL);

    return !hide;
}

static void
add_clicked_cb (GtkButton *button,
                gpointer   user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);
    g_autoptr (GAppInfo) info = NULL;
    g_autoptr (GError) error = NULL;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));

    if (info == NULL)
    {
        return;
    }

    g_app_info_set_as_last_used_for_type (info, self->content_type, &error);

    if (error != NULL)
    {
        g_autofree gchar *message = NULL;

        message = g_strdup_printf (_("Error while adding “%s”: %s"),
                                   g_app_info_get_display_name (info), error->message);
        show_dialog (_("Could not add application"),
                     message,
                     GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                     GTK_MESSAGE_ERROR);
    }
    else
    {
        gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
        g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
    }
}

static void
remove_clicked_cb (GtkMenuItem *item,
                   gpointer     user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);
    g_autoptr (GAppInfo) info = NULL;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));

    if (info)
    {
        g_autoptr (GError) error = NULL;

        if (!g_app_info_remove_supports_type (info,
                                              self->content_type,
                                              &error))
        {
            show_dialog (_("Could not forget association"),
                         error->message,
                         GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                         GTK_MESSAGE_ERROR);
        }

        gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
    }

    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static void
populate_popup_cb (GtkAppChooserWidget *widget,
                   GtkMenu             *menu,
                   GAppInfo            *app,
                   gpointer             user_data)
{
    GtkWidget *item;
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);

    if (g_app_info_can_remove_supports_type (app))
    {
        item = gtk_menu_item_new_with_label (_("Forget association"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);

        g_signal_connect (item, "activate",
                          G_CALLBACK (remove_clicked_cb), self);
    }
}

static void
reset_clicked_cb (GtkButton *button,
                  gpointer   user_data)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (user_data);

    g_app_info_reset_type_associations (self->content_type);
    gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));

    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static void
set_as_default_clicked_cb (GtkButton *button,
                           gpointer   user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);
    g_autoptr (GAppInfo) info = NULL;
    g_autoptr (GError) error = NULL;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));

    g_app_info_set_as_default_for_type (info, self->content_type,
                                        &error);

    if (error != NULL)
    {
        g_autofree gchar *message = NULL;

        message = g_strdup_printf (_("Error while setting “%s” as default application: %s"),
                                   g_app_info_get_display_name (info), error->message);
        show_dialog (_("Could not set as default"),
                     message,
                     GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                     GTK_MESSAGE_ERROR);
    }

    gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static gint
app_compare (gconstpointer a,
             gconstpointer b)
{
    return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static gboolean
app_info_can_add (GAppInfo    *info,
                  const gchar *content_type)
{
    g_autolist (GAppInfo) recommended = NULL;
    g_autolist (GAppInfo) fallback = NULL;

    recommended = g_app_info_get_recommended_for_type (content_type);
    fallback = g_app_info_get_fallback_for_type (content_type);

    if (g_list_find_custom (recommended, info, app_compare))
    {
        return FALSE;
    }

    if (g_list_find_custom (fallback, info, app_compare))
    {
        return FALSE;
    }

    return TRUE;
}

static void
application_selected_cb (GtkAppChooserWidget *widget,
                         GAppInfo            *info,
                         gpointer             user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);
    g_autoptr (GAppInfo) default_app = NULL;

    default_app = g_app_info_get_default_for_type (self->content_type, FALSE);
    if (default_app != NULL)
    {
        gtk_widget_set_sensitive (self->set_as_default_button,
                                  !g_app_info_equal (info, default_app));
    }
    gtk_widget_set_sensitive (self->add_button,
                              app_info_can_add (info, self->content_type));
}

static void
application_chooser_apply_labels (NautilusPropertiesWindow *self)
{
    g_autofree gchar *label = NULL;
    g_autofree gchar *extension = NULL;
    g_autofree gchar *description = NULL;
    gint num_files;
    NautilusFile *file;

    num_files = g_list_length (self->open_with_files);
    file = self->open_with_files->data;

    /* here we assume all files are of the same content type */
    if (g_content_type_is_unknown (self->content_type))
    {
        extension = nautilus_file_get_extension (file);

        /* Translators: the %s here is a file extension */
        description = g_strdup_printf (_("%s document"), extension);
    }
    else
    {
        description = g_content_type_get_description (self->content_type);
    }

    if (num_files > 1)
    {
        /* Translators; %s here is a mime-type description */
        label = g_strdup_printf (_("Open all files of type “%s” with"),
                                 description);
    }
    else
    {
        g_autofree gchar *display_name = NULL;
        display_name = nautilus_file_get_display_name (file);

        /* Translators: first %s is filename, second %s is mime-type description */
        label = g_strdup_printf (_("Select an application to open “%s” and other files of type “%s”"),
                                 display_name, description);
    }

    gtk_label_set_markup (GTK_LABEL (self->open_with_label), label);
}

static void
setup_app_chooser_area (NautilusPropertiesWindow *self)
{
    g_autoptr (GAppInfo) info = NULL;

    self->app_chooser_widget = gtk_app_chooser_widget_new (self->content_type);
    gtk_widget_set_vexpand (self->app_chooser_widget, TRUE);
    gtk_box_pack_start (GTK_BOX (self->app_chooser_widget_box), self->app_chooser_widget, FALSE, TRUE, 0);

    gtk_app_chooser_widget_set_show_default (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_fallback (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_other (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_widget_show (self->app_chooser_widget);
    g_signal_connect (self->reset_button, "clicked",
                      G_CALLBACK (reset_clicked_cb),
                      self);
    g_signal_connect (self->add_button, "clicked",
                      G_CALLBACK (add_clicked_cb),
                      self);
    g_signal_connect (self->set_as_default_button, "clicked",
                      G_CALLBACK (set_as_default_clicked_cb),
                      self);

    /* initialize sensitivity */
    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));
    if (info != NULL)
    {
        application_selected_cb (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget),
                                 info, self);
    }

    g_signal_connect (self->app_chooser_widget,
                      "application-selected",
                      G_CALLBACK (application_selected_cb),
                      self);
    g_signal_connect (self->app_chooser_widget,
                      "populate-popup",
                      G_CALLBACK (populate_popup_cb),
                      self);

    application_chooser_apply_labels (self);
}

static void
setup_open_with_page (NautilusPropertiesWindow *self)
{
    GList *files = NULL;
    NautilusFile *target_file;

    target_file = get_target_file (self);
    self->content_type = nautilus_file_get_mime_type (target_file);

    if (!is_multi_file_window (self))
    {
        files = g_list_prepend (NULL, target_file);
    }
    else
    {
        files = g_list_copy (self->original_files);
        if (files == NULL)
        {
            return;
        }
    }

    self->open_with_files = files;
    setup_app_chooser_area (self);
}


static NautilusPropertiesWindow *
create_properties_window (StartupData *startup_data)
{
    NautilusPropertiesWindow *window;
    GList *l;

    window = NAUTILUS_PROPERTIES_WINDOW (g_object_new (NAUTILUS_TYPE_PROPERTIES_WINDOW,
                                                       NULL));

    window->original_files = nautilus_file_list_copy (startup_data->original_files);

    window->target_files = nautilus_file_list_copy (startup_data->target_files);

    if (startup_data->parent_widget)
    {
        gtk_window_set_screen (GTK_WINDOW (window),
                               gtk_widget_get_screen (startup_data->parent_widget));
    }

    if (startup_data->parent_window)
    {
        gtk_window_set_transient_for (GTK_WINDOW (window), startup_data->parent_window);
    }

    if (startup_data->startup_id)
    {
        gtk_window_set_startup_id (GTK_WINDOW (window), startup_data->startup_id);
    }

    /* Set initial window title */
    update_properties_window_title (window);

    /* Start monitoring the file attributes we display. Note that some
     * of the attributes are for the original file, and some for the
     * target files.
     */

    for (l = window->original_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        NautilusFileAttributes attributes;

        file = NAUTILUS_FILE (l->data);

        attributes =
            NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
            NAUTILUS_FILE_ATTRIBUTE_INFO;

        nautilus_file_monitor_add (file,
                                   &window->original_files,
                                   attributes);
    }

    for (l = window->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        NautilusFileAttributes attributes;

        file = NAUTILUS_FILE (l->data);

        attributes = 0;
        if (nautilus_file_is_directory (file))
        {
            attributes |= NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS;
        }

        attributes |= NAUTILUS_FILE_ATTRIBUTE_INFO;
        nautilus_file_monitor_add (file, &window->target_files, attributes);
    }

    for (l = window->target_files; l != NULL; l = l->next)
    {
        g_signal_connect_object (NAUTILUS_FILE (l->data),
                                 "changed",
                                 G_CALLBACK (file_changed_callback),
                                 G_OBJECT (window),
                                 0);
    }

    for (l = window->original_files; l != NULL; l = l->next)
    {
        g_signal_connect_object (NAUTILUS_FILE (l->data),
                                 "changed",
                                 G_CALLBACK (file_changed_callback),
                                 G_OBJECT (window),
                                 0);
    }

    /* Create the pages. */
    setup_basic_page (window);

    if (should_show_permissions (window))
    {
        setup_permissions_page (window);
        gtk_widget_show (window->permissions_box);
    }

    if (should_show_open_with (window))
    {
        setup_open_with_page (window);
        gtk_widget_show (window->open_with_box);
    }

    /* append pages from available views */
    append_extension_pages (window);

    /* Update from initial state */
    properties_window_update (window, NULL);

    return window;
}

static GList *
get_target_file_list (GList *original_files)
{
    return g_list_copy_deep (original_files,
                             (GCopyFunc) get_target_file_for_original_file,
                             NULL);
}

static void
add_window (NautilusPropertiesWindow *self)
{
    if (!is_multi_file_window (self))
    {
        g_hash_table_insert (windows,
                             get_original_file (self),
                             self);
        g_object_set_data (G_OBJECT (self), "window_key",
                           get_original_file (self));
    }
}

static void
remove_window (NautilusPropertiesWindow *self)
{
    gpointer key;

    key = g_object_get_data (G_OBJECT (self), "window_key");
    if (key)
    {
        g_hash_table_remove (windows, key);
    }
}

static NautilusPropertiesWindow *
get_existing_window (GList *file_list)
{
    if (!file_list->next)
    {
        return g_hash_table_lookup (windows, file_list->data);
    }

    return NULL;
}

static void
properties_window_finish (StartupData *data)
{
    gboolean cancel_timed_wait;

    if (data->parent_widget != NULL)
    {
        g_signal_handlers_disconnect_by_data (data->parent_widget,
                                              data);
    }
    if (data->window != NULL)
    {
        g_signal_handlers_disconnect_by_data (data->window,
                                              data);
    }

    cancel_timed_wait = (data->window == NULL && !data->cancelled);
    remove_pending (data, TRUE, cancel_timed_wait);

    startup_data_free (data);
}

static void
cancel_create_properties_window_callback (gpointer callback_data)
{
    StartupData *data;

    data = callback_data;
    data->cancelled = TRUE;

    properties_window_finish (data);
}

static void
parent_widget_destroyed_callback (GtkWidget *widget,
                                  gpointer   callback_data)
{
    g_assert (widget == ((StartupData *) callback_data)->parent_widget);

    properties_window_finish ((StartupData *) callback_data);
}

static void
cancel_call_when_ready_callback (gpointer key,
                                 gpointer value,
                                 gpointer user_data)
{
    nautilus_file_cancel_call_when_ready
        (NAUTILUS_FILE (key),
        is_directory_ready_callback,
        user_data);
}

static void
remove_pending (StartupData *startup_data,
                gboolean     cancel_call_when_ready,
                gboolean     cancel_timed_wait)
{
    if (cancel_call_when_ready)
    {
        g_hash_table_foreach (startup_data->pending_files,
                              cancel_call_when_ready_callback,
                              startup_data);
    }
    if (cancel_timed_wait)
    {
        eel_timed_wait_stop
            (cancel_create_properties_window_callback, startup_data);
    }
    if (startup_data->pending_key != NULL)
    {
        g_hash_table_remove (pending_lists, startup_data->pending_key);
    }
}

static gboolean
widget_on_destroy (GtkWidget *widget,
                   gpointer   user_data)
{
    StartupData *data = (StartupData *) user_data;


    if (data->callback != NULL)
    {
        data->callback (data->callback_data);
    }

    properties_window_finish (data);

    return GDK_EVENT_PROPAGATE;
}

static void
is_directory_ready_callback (NautilusFile *file,
                             gpointer      data)
{
    StartupData *startup_data;

    startup_data = data;

    g_hash_table_remove (startup_data->pending_files, file);

    if (g_hash_table_size (startup_data->pending_files) == 0)
    {
        NautilusPropertiesWindow *new_window;

        new_window = create_properties_window (startup_data);

        add_window (new_window);
        startup_data->window = new_window;

        remove_pending (startup_data, FALSE, TRUE);

        gtk_window_present (GTK_WINDOW (new_window));
        g_signal_connect (GTK_WIDGET (new_window), "destroy",
                          G_CALLBACK (widget_on_destroy), startup_data);
    }
}

void
nautilus_properties_window_present (GList                            *original_files,
                                    GtkWidget                        *parent_widget,
                                    const gchar                      *startup_id,
                                    NautilusPropertiesWindowCallback  callback,
                                    gpointer                          callback_data)
{
    GList *l, *next;
    GtkWindow *parent_window;
    StartupData *startup_data;
    g_autolist (NautilusFile) target_files = NULL;
    NautilusPropertiesWindow *existing_window;
    g_autofree char *pending_key = NULL;

    g_return_if_fail (original_files != NULL);
    g_return_if_fail (parent_widget == NULL || GTK_IS_WIDGET (parent_widget));


    /* Create the hash tables first time through. */
    if (windows == NULL)
    {
        windows = g_hash_table_new (NULL, NULL);
    }

    if (pending_lists == NULL)
    {
        pending_lists = g_hash_table_new (g_str_hash, g_str_equal);
    }

    /* Look to see if there's already a window for this file. */
    existing_window = get_existing_window (original_files);
    if (existing_window != NULL)
    {
        if (parent_widget)
        {
            gtk_window_set_screen (GTK_WINDOW (existing_window),
                                   gtk_widget_get_screen (parent_widget));
        }
        else if (startup_id)
        {
            gtk_window_set_startup_id (GTK_WINDOW (existing_window), startup_id);
        }

        gtk_window_present (GTK_WINDOW (existing_window));
        startup_data = startup_data_new (NULL, NULL, NULL, NULL, NULL, NULL,
                                         callback, callback_data, existing_window);
        g_signal_connect (GTK_WIDGET (existing_window), "destroy",
                          G_CALLBACK (widget_on_destroy), startup_data);
        return;
    }


    pending_key = get_pending_key (original_files);

    /* Look to see if we're already waiting for a window for this file. */
    if (g_hash_table_lookup (pending_lists, pending_key) != NULL)
    {
        /* FIXME: No callback is done if this happen. In practice, it's a quite
         * corner case
         */
        return;
    }

    target_files = get_target_file_list (original_files);

    if (parent_widget)
    {
        parent_window = GTK_WINDOW (gtk_widget_get_ancestor (parent_widget, GTK_TYPE_WINDOW));
    }
    else
    {
        parent_window = NULL;
    }

    startup_data = startup_data_new (original_files,
                                     target_files,
                                     pending_key,
                                     parent_widget,
                                     parent_window,
                                     startup_id,
                                     callback,
                                     callback_data,
                                     NULL);

    /* Wait until we can tell whether it's a directory before showing, since
     * some one-time layout decisions depend on that info.
     */

    g_hash_table_insert (pending_lists, startup_data->pending_key, startup_data->pending_key);
    if (parent_widget)
    {
        g_signal_connect (parent_widget, "destroy",
                          G_CALLBACK (parent_widget_destroyed_callback), startup_data);
    }

    eel_timed_wait_start
        (cancel_create_properties_window_callback,
        startup_data,
        _("Creating Properties window."),
        parent_window == NULL ? NULL : GTK_WINDOW (parent_window));

    for (l = startup_data->target_files; l != NULL; l = next)
    {
        next = l->next;
        nautilus_file_call_when_ready
            (NAUTILUS_FILE (l->data),
            NAUTILUS_FILE_ATTRIBUTE_INFO,
            is_directory_ready_callback,
            startup_data);
    }
}

static void
real_dispose (GObject *object)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (object);

    remove_window (self);

    unschedule_or_cancel_group_change (self);
    unschedule_or_cancel_owner_change (self);

    g_list_foreach (self->original_files,
                    (GFunc) nautilus_file_monitor_remove,
                    &self->original_files);
    g_clear_list (&self->original_files, (GDestroyNotify) nautilus_file_unref);

    g_list_foreach (self->target_files,
                    (GFunc) nautilus_file_monitor_remove,
                    &self->target_files);
    g_clear_list (&self->target_files, (GDestroyNotify) nautilus_file_unref);

    g_clear_list (&self->changed_files, (GDestroyNotify) nautilus_file_unref);

    g_clear_handle_id (&self->deep_count_spinner_timeout_id, g_source_remove);

    while (self->deep_count_files)
    {
        stop_deep_count_for_file (self, self->deep_count_files->data);
    }

    g_clear_list (&self->permission_buttons, NULL);

    g_clear_list (&self->permission_combos, NULL);

    g_clear_list (&self->change_permission_combos, NULL);

    g_clear_pointer (&self->initial_permissions, g_hash_table_destroy);

    g_clear_list (&self->value_fields, NULL);

    g_clear_handle_id (&self->update_directory_contents_timeout_id, g_source_remove);
    g_clear_handle_id (&self->update_files_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_properties_window_parent_class)->dispose (object);
}

static void
real_finalize (GObject *object)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (object);

    g_list_free_full (self->mime_list, g_free);

    g_free (self->pending_name);
    g_free (self->content_type);
    g_list_free (self->open_with_files);

    g_clear_handle_id (&self->select_idle_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_properties_window_parent_class)->finalize (object);
}

/* icon selection callback to set the image of the file object to the selected file */
static void
set_icon (const char               *icon_uri,
          NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    g_autofree gchar *icon_path = NULL;

    g_assert (icon_uri != NULL);
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    icon_path = g_filename_from_uri (icon_uri, NULL, NULL);
    /* we don't allow remote URIs */
    if (icon_path != NULL)
    {
        GList *l;

        for (l = self->original_files; l != NULL; l = l->next)
        {
            g_autofree gchar *file_uri = NULL;
            g_autoptr (GFile) file_location = NULL;
            g_autoptr (GFile) icon_location = NULL;
            g_autofree gchar *real_icon_uri = NULL;

            file = NAUTILUS_FILE (l->data);
            file_uri = nautilus_file_get_uri (file);
            file_location = nautilus_file_get_location (file);
            icon_location = g_file_new_for_uri (icon_uri);

            /* ’Tis a little bit of a misnomer. Actually a path. */
            real_icon_uri = g_file_get_relative_path (icon_location,
                                                      file_location);

            if (real_icon_uri == NULL)
            {
                real_icon_uri = g_strdup (icon_uri);
            }

            nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, real_icon_uri);
        }
    }
}

static void
custom_icon_file_chooser_response_cb (GtkDialog                *dialog,
                                      gint                      response,
                                      NautilusPropertiesWindow *self)
{
    switch (response)
    {
        case GTK_RESPONSE_NO:
        {
            reset_icon (self);
        }
        break;

        case GTK_RESPONSE_OK:
        {
            g_autoptr (GFile) location = NULL;
            g_autofree gchar *uri = NULL;

            location = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
            if (location != NULL)
            {
                uri = g_file_get_uri (location);
                set_icon (uri, self);
            }
            else
            {
                reset_icon (self);
            }
        }
        break;

        default:
        {
        }
        break;
    }

    gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
select_image_button_callback (GtkWidget                *widget,
                              NautilusPropertiesWindow *self)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GList *l;
    NautilusFile *file;
    gboolean revert_is_sensitive;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    dialog = self->icon_chooser;

    if (dialog == NULL)
    {
        dialog = gtk_file_chooser_dialog_new (_("Select Custom Icon"), GTK_WINDOW (self),
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              _("_Revert"), GTK_RESPONSE_NO,
                                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                                              _("_Open"), GTK_RESPONSE_OK,
                                              NULL);
        gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog),
                                              g_get_user_special_dir (G_USER_DIRECTORY_PICTURES),
                                              NULL);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

        self->icon_chooser = dialog;

        g_object_add_weak_pointer (G_OBJECT (dialog),
                                   (gpointer *) &self->icon_chooser);
    }

    /* it's likely that the user wants to pick an icon that is inside a local directory */
    if (g_list_length (self->original_files) == 1)
    {
        file = NAUTILUS_FILE (self->original_files->data);

        if (nautilus_file_is_directory (file))
        {
            g_autoptr (GFile) image_location = NULL;

            image_location = nautilus_file_get_location (file);

            if (image_location != NULL)
            {
                gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (dialog),
                                                          image_location,
                                                          NULL);
            }
        }
    }

    revert_is_sensitive = FALSE;
    for (l = self->original_files; l != NULL; l = l->next)
    {
        g_autofree gchar *image_path = NULL;

        file = NAUTILUS_FILE (l->data);
        image_path = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
        revert_is_sensitive = (image_path != NULL);

        if (revert_is_sensitive)
        {
            break;
        }
    }
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_NO, revert_is_sensitive);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (custom_icon_file_chooser_response_cb), self);
    gtk_widget_show (dialog);
}

static void
nautilus_properties_window_class_init (NautilusPropertiesWindowClass *klass)
{
    GtkBindingSet *binding_set;
    GtkWidgetClass *widget_class;
    GObjectClass *oclass;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = real_dispose;
    oclass->finalize = real_finalize;

    binding_set = gtk_binding_set_by_class (klass);
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
                                  "close", 0);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-properties-window.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, notebook);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_image);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_button_image);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, basic_grid);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, name_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, name_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, name_field);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, type_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, type_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, link_target_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, link_target_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_spinner);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, size_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, size_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, parent_folder_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, parent_folder_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, original_folder_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, original_folder_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, volume_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, volume_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, trashed_on_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, trashed_on_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, accessed_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, accessed_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, spacer_2);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, modified_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, modified_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, created_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, created_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, spacer_3);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, free_space_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, free_space_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, volume_widget_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, open_in_disks_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, pie_chart);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, used_color);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, used_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, free_color);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, free_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, total_capacity_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, file_system_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permissions_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permissions_grid);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, bottom_prompt_seperator);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, not_the_owner_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permission_indeterminable_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_value_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_folder_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_file_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_folder_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_file_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_value_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_folder_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_file_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_folder_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_file_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_folder_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_file_access_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_folder_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_file_access_combo);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execute_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execute_checkbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, security_context_title_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, security_context_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, change_permissions_button_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, change_permissions_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, open_with_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, open_with_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, app_chooser_widget_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, reset_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, add_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, set_as_default_button);
}

static void
nautilus_properties_window_init (NautilusPropertiesWindow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    g_signal_connect (self, "close", G_CALLBACK (gtk_window_close), NULL);
}
