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

#include "nautilus-application.h"
#include "nautilus-dbus-launcher.h"
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
    AdwWindow parent_instance;

    GList *original_files;
    GList *target_files;

    GtkNotebook *notebook;

    /* Basic tab widgets */

    GtkStack *icon_stack;
    GtkWidget *icon_image;
    GtkWidget *icon_button;
    GtkWidget *icon_button_image;
    GtkWidget *icon_chooser;

    GtkLabel *name_value_label;
    GtkWidget *type_value_label;

    GtkWidget *link_target_row;
    GtkWidget *link_target_value_label;
    GtkWidget *size_row;
    GtkWidget *size_value_label;
    GtkWidget *contents_row;
    GtkWidget *contents_value_label;
    GtkWidget *contents_spinner;
    guint update_directory_contents_timeout_id;
    guint update_files_timeout_id;
    GtkWidget *parent_folder_row;
    GtkWidget *parent_folder_value_label;

    GtkWidget *trashed_list_box;
    GtkWidget *trashed_on_value_label;
    GtkWidget *original_folder_value_label;

    GtkWidget *times_list_box;
    GtkWidget *modified_row;
    GtkWidget *modified_value_label;
    GtkWidget *created_row;
    GtkWidget *created_value_label;
    GtkWidget *accessed_row;
    GtkWidget *accessed_value_label;

    GtkWidget *free_space_list_box;
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

    GtkWidget *permissions_stack;

    GtkWidget *unknown_permissions_page;

    GtkWidget *bottom_prompt_seperator;
    GtkWidget *not_the_owner_label;

    AdwComboRow *owner_row;
    AdwComboRow *owner_access_row;
    AdwComboRow *owner_folder_access_row;
    AdwComboRow *owner_file_access_row;

    AdwComboRow *group_row;
    AdwComboRow *group_access_row;
    AdwComboRow *group_folder_access_row;
    AdwComboRow *group_file_access_row;

    AdwComboRow *others_access_row;
    AdwComboRow *others_folder_access_row;
    AdwComboRow *others_file_access_row;

    GtkWidget *execution_list_box;
    AdwComboRow *execution_row;
    GtkRevealer *execution_inconsistent_revealer;
    GtkSwitch *execution_switch;

    GtkWidget *security_context_list_box;
    GtkWidget *security_context_value_label;

    GtkWidget *change_permissions_button_box;
    GtkWidget *change_permissions_button;

    /* Open With tab Widgets */

    GtkWidget *open_with_box;
    GtkWidget *open_with_label;
    GtkWidget *app_chooser_widget_box;
    GtkWidget *app_chooser_widget;
    GtkWidget *reset_button;
    GtkWidget *forget_button;
    GtkWidget *add_button;
    GtkWidget *set_as_default_button;
    char *content_type;
    GList *open_with_files;

    GroupChange *group_change;
    OwnerChange *owner_change;

    GList *permission_rows;
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

typedef enum
{
    NO_FILES_OR_FOLDERS = (0),
    FILES_ONLY          = (1 << 0),
    FOLDERS_ONLY        = (1 << 1),
    FILES_AND_FOLDERS   = FILES_ONLY | FOLDERS_ONLY,
} FilterType;

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
    PERMISSION_NONE = (0),
    PERMISSION_READ = (1 << 0),
    PERMISSION_WRITE = (1 << 1),
    PERMISSION_EXEC = (1 << 2),
    PERMISSION_INCONSISTENT = (1 << 3)
} PermissionValue;

typedef enum
{
    PERMISSION_USER,
    PERMISSION_GROUP,
    PERMISSION_OTHER,
    NUM_PERMISSION_TYPE
} PermissionType;

/** Contains permissions for files and folders for each PermissionType */
typedef struct
{
    NautilusPropertiesWindow *window;

    PermissionValue folder_permissions[NUM_PERMISSION_TYPE];
    PermissionValue file_permissions[NUM_PERMISSION_TYPE];
    PermissionValue file_exec_permissions;
    gboolean has_files;
    gboolean has_folders;
    gboolean can_set_all_folder_permission;
    gboolean can_set_all_file_permission;
    gboolean can_set_any_file_permission;
    gboolean is_multi_file_window;
} TargetPermissions;

/* NautilusPermissionEntry - helper struct for permission AdwComboRow */

#define NAUTILUS_TYPE_PERMISSION_ENTRY (nautilus_permission_entry_get_type ())
G_DECLARE_FINAL_TYPE (NautilusPermissionEntry, nautilus_permission_entry,
                      NAUTILUS, PERMISSION_ENTRY, GObject)

enum
{
    PROP_NAME = 1,
    NUM_PROPERTIES
};

struct _NautilusPermissionEntry
{
    GObject parent;

    char *name;
    PermissionValue permission_value;
};

G_DEFINE_TYPE (NautilusPermissionEntry,
               nautilus_permission_entry,
               G_TYPE_OBJECT)

static void
nautilus_permission_entry_init (NautilusPermissionEntry *self)
{
    self->name = NULL;
    self->permission_value = PERMISSION_NONE;
}

static void
nautilus_permission_entry_finalize (GObject *object)
{
    NautilusPermissionEntry *self = NAUTILUS_PERMISSION_ENTRY (object);

    g_free (self->name);

    G_OBJECT_CLASS (nautilus_permission_entry_parent_class)->finalize (object);
}

static void
nautilus_permission_entry_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    NautilusPermissionEntry *self = NAUTILUS_PERMISSION_ENTRY (object);

    switch (prop_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }
}

static void
nautilus_permission_entry_class_init (NautilusPermissionEntryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = nautilus_permission_entry_finalize;
    gobject_class->get_property = nautilus_permission_entry_get_property;

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name", "", "",
                                                          NULL,
                                                          G_PARAM_READABLE));
}

static gchar *
permission_value_to_string (PermissionValue permission_value,
                            gboolean        describes_folder)
{
    if (permission_value & PERMISSION_INCONSISTENT)
    {
        return "---";
    }
    else if (permission_value & PERMISSION_READ)
    {
        if (permission_value & PERMISSION_WRITE)
        {
            if (!describes_folder)
            {
                return _("Read and write");
            }
            else if (permission_value & PERMISSION_EXEC)
            {
                return _("Create and delete files");
            }
            else
            {
                return _("Read/write, no access");
            }
        }
        else
        {
            if (!describes_folder)
            {
                return _("Read-only");
            }
            else if (permission_value & PERMISSION_EXEC)
            {
                return _("Access files");
            }
            else
            {
                return _("List files only");
            }
        }
    }
    else
    {
        if (permission_value & PERMISSION_WRITE)
        {
            if (!describes_folder || permission_value & PERMISSION_EXEC)
            {
                return _("Write-only");
            }
            else
            {
                return _("Write-only, no access");
            }
        }
        else
        {
            if (describes_folder && permission_value & PERMISSION_EXEC)
            {
                return _("Access-only");
            }
            else
            {
                /* Translators: this is referred to the permissions the user has in a directory. */
                return _("None");
            }
        }
    }
}

/* end NautilusPermissionEntry */

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
static void update_execution_row (GtkWidget         *row,
                                  TargetPermissions *target_perm);
static void update_permission_row (AdwComboRow       *row,
                                   TargetPermissions *target_perm);
static void value_field_update (GtkLabel                 *field,
                                NautilusPropertiesWindow *self);
static void properties_window_update (NautilusPropertiesWindow *self,
                                      GList                    *files);
static void is_directory_ready_callback (NautilusFile *file,
                                         gpointer      data);
static void cancel_group_change_callback (GroupChange *change);
static void cancel_owner_change_callback (OwnerChange *change);
static void update_owner_row (AdwComboRow       *row,
                              TargetPermissions *target_perm);
static void update_group_row (AdwComboRow       *row,
                              TargetPermissions *target_perm);
static void select_image_button_callback (GtkWidget                *widget,
                                          NautilusPropertiesWindow *self);
static void set_icon (const char               *icon_path,
                      NautilusPropertiesWindow *self);
static void remove_pending (StartupData *data,
                            gboolean     cancel_call_when_ready,
                            gboolean     cancel_timed_wait);
static void append_extension_pages (NautilusPropertiesWindow *self);

static void setup_pie_widget (NautilusPropertiesWindow *self);

G_DEFINE_TYPE (NautilusPropertiesWindow, nautilus_properties_window, ADW_TYPE_WINDOW);

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
                                 GdkPaintable             **icon_paintable)
{
    g_autoptr (NautilusIconInfo) icon = NULL;
    GList *l;
    gint icon_scale;

    icon_scale = gtk_widget_get_scale_factor (GTK_WIDGET (self->notebook));
    /* FIXME: Temporary regression: HiDPI icons not supported, ignore scale. */
    icon_scale = 1;

    for (l = self->original_files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        g_autoptr (NautilusIconInfo) new_icon = NULL;

        file = NAUTILUS_FILE (l->data);

        if (!icon)
        {
            icon = nautilus_file_get_icon (file, NAUTILUS_GRID_ICON_SIZE_MEDIUM, icon_scale,
                                           NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
                                           NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING);
        }
        else
        {
            new_icon = nautilus_file_get_icon (file, NAUTILUS_GRID_ICON_SIZE_MEDIUM, icon_scale,
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
        g_autoptr (GIcon) gicon = g_themed_icon_new ("text-x-generic");

        icon = nautilus_icon_info_lookup (gicon, NAUTILUS_GRID_ICON_SIZE_MEDIUM, icon_scale);
    }

    if (icon_name != NULL)
    {
        *icon_name = g_strdup (nautilus_icon_info_get_used_name (icon));
    }

    if (icon_paintable != NULL)
    {
        *icon_paintable = nautilus_icon_info_get_paintable (icon);
    }
}


static void
update_properties_window_icon (NautilusPropertiesWindow *self)
{
    g_autoptr (GdkPaintable) paintable = NULL;
    g_autofree char *name = NULL;
    gint pixel_size;

    get_image_for_properties_window (self, &name, &paintable);

    if (name != NULL)
    {
        gtk_window_set_icon_name (GTK_WINDOW (self), name);
    }

    pixel_size = MAX (gdk_paintable_get_intrinsic_width (paintable),
                      gdk_paintable_get_intrinsic_width (paintable));

    gtk_image_set_from_paintable (GTK_IMAGE (self->icon_image), paintable);
    gtk_image_set_from_paintable (GTK_IMAGE (self->icon_button_image), paintable);
    gtk_image_set_pixel_size (GTK_IMAGE (self->icon_image), pixel_size);
    gtk_image_set_pixel_size (GTK_IMAGE (self->icon_button_image), pixel_size);
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
nautilus_properties_window_drag_drop_cb (GtkDropTarget *target,
                                         const GValue  *value,
                                         gdouble        x,
                                         gdouble        y,
                                         gpointer       user_data)
{
    GSList *file_list;
    gboolean exactly_one;
    GtkImage *image;
    GtkWindow *window;

    image = GTK_IMAGE (user_data);
    window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (image)));

    if (!G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        return;
    }

    file_list = g_value_get_boxed (value);
    exactly_one = file_list != NULL && g_slist_next (file_list) == NULL;

    if (!exactly_one)
    {
        show_dialog (_("You cannot assign more than one custom icon at a time!"),
                     _("Please drop just one image to set a custom icon."),
                     window,
                     GTK_MESSAGE_ERROR);
    }
    else
    {
        g_autofree gchar *uri = g_file_get_uri (file_list->data);

        if (uri_is_local_image (uri))
        {
            set_icon (uri, NAUTILUS_PROPERTIES_WINDOW (window));
        }
        else
        {
            if (!g_file_is_native (file_list->data))
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
        GtkDropTarget *target;

        /* prepare the image to receive dropped objects to assign custom images */
        target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
        gtk_widget_add_controller (self->icon_button, GTK_EVENT_CONTROLLER (target));
        g_signal_connect (target, "drop",
                          G_CALLBACK (nautilus_properties_window_drag_drop_cb), self->icon_button_image);

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
update_name_field (NautilusPropertiesWindow *self)
{
    g_autoptr (GString) name_str = g_string_new ("");
    guint file_counter = 0;

    for (GList *l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_is_gone (file))
        {
            g_autofree gchar *file_name;

            file_counter += 1;
            if (file_counter > 1)
            {
                g_string_append (name_str, ", ");
            }

            file_name = nautilus_file_get_display_name (file);
            g_string_append (name_str, file_name);
        }
    }

    gtk_label_set_text (self->name_value_label, name_str->str);
}

/**
 * Returns the attribute value if all files in file_list have identical
 * attributes, "unknown" if no files exist and NULL otherwise.
 */
static char *
file_list_get_string_attribute (GList      *file_list,
                                const char *attribute_name)
{
    g_autofree char *first_attr = NULL;

    for (GList *l = file_list; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);

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
            g_autofree char *attr;
            attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
            if (!g_str_equal (attr, first_attr))
            {
                /* Not all files have the same value for attribute_name. */
                return NULL;
            }
        }
    }

    if (first_attr != NULL)
    {
        return g_steal_pointer (&first_attr);
    }
    else
    {
        return g_strdup (_("unknown"));
    }
}

static gboolean
file_list_attributes_identical (GList      *file_list,
                                const char *attribute_name)
{
    g_autofree char *attribute = file_list_get_string_attribute (file_list, attribute_name);
    return attribute != NULL;
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
    PermissionValue perm = PERMISSION_NONE;

    g_assert (type >= 0 && type < 3);

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

static PermissionValue
exec_permission_from_vfs (guint32 vfs_perm)
{
    guint32 perm_user = vfs_perm & UNIX_PERM_USER_EXEC;
    guint32 perm_group = vfs_perm & UNIX_PERM_GROUP_EXEC;
    guint32 perm_other = vfs_perm & UNIX_PERM_OTHER_EXEC;

    if (perm_user && perm_group && perm_other)
    {
        return PERMISSION_EXEC;
    }
    else if (perm_user || perm_group || perm_other)
    {
        return PERMISSION_INCONSISTENT;
    }
    else
    {
        return PERMISSION_NONE;
    }
}

static TargetPermissions *
get_target_permissions (NautilusPropertiesWindow *self)
{
    TargetPermissions *p = g_new0 (TargetPermissions, 1);
    p->window = self;
    p->can_set_all_folder_permission = TRUE;
    p->can_set_all_file_permission = TRUE;

    for (GList *entry = self->target_files; entry != NULL; entry = entry->next)
    {
        guint32 vfs_permissions;
        gboolean can_set_permissions;
        NautilusFile *file = NAUTILUS_FILE (entry->data);

        if (nautilus_file_is_gone (file) || !nautilus_file_can_get_permissions (file))
        {
            continue;
        }

        vfs_permissions = nautilus_file_get_permissions (file);
        can_set_permissions = nautilus_file_can_set_permissions (file);

        if (nautilus_file_is_directory (file))
        {
            /* Gather permissions for each type (owner, group, other) */
            for (PermissionType type = PERMISSION_USER; type < NUM_PERMISSION_TYPE; type += 1)
            {
                PermissionValue permissions = permission_from_vfs (type, vfs_permissions)
                                              & (PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXEC);
                if (!p->has_folders)
                {
                    /* first found folder, initialize with its permissions */
                    p->folder_permissions[type] = permissions;
                }
                else if (permissions != p->folder_permissions[type])
                {
                    p->folder_permissions[type] = PERMISSION_INCONSISTENT;
                }
            }

            p->can_set_all_folder_permission &= can_set_permissions;
            p->has_folders = TRUE;
        }
        else
        {
            PermissionValue exec_permissions = exec_permission_from_vfs (vfs_permissions);

            if (!p->has_files)
            {
                p->file_exec_permissions = exec_permissions;
            }
            else if (exec_permissions != p->file_exec_permissions)
            {
                p->file_exec_permissions = PERMISSION_INCONSISTENT;
            }
            for (PermissionType type = PERMISSION_USER ; type < NUM_PERMISSION_TYPE; type += 1)
            {
                PermissionValue permissions = permission_from_vfs (type, vfs_permissions)
                                              & (PERMISSION_READ | PERMISSION_WRITE);

                if (!p->has_files)
                {
                    /* first found file, initialize with its permissions */
                    p->file_permissions[type] = permissions;
                }
                else if (permissions != p->file_permissions[type])
                {
                    p->file_permissions[type] = PERMISSION_INCONSISTENT;
                }
            }

            p->can_set_all_file_permission &= can_set_permissions;
            p->can_set_any_file_permission |= can_set_permissions;
            p->has_files = TRUE;
        }
    }

    p->is_multi_file_window = is_multi_file_window (self);

    return p;
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
        g_autofree TargetPermissions *target_perm = get_target_permissions (self);

        update_owner_row (self->owner_row, target_perm);
        update_group_row (self->group_row, target_perm);
        update_execution_row (GTK_WIDGET (self->execution_row), target_perm);
        g_list_foreach (self->permission_rows,
                        (GFunc) update_permission_row,
                        target_perm);
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
        gtk_window_destroy (GTK_WINDOW (self));
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

    attribute_value = file_list_get_string_attribute (file_list,
                                                      attribute_name);
    if (attribute_value != NULL &&
        g_str_equal (attribute_name, "detailed_type"))
    {
        g_autofree char *mime_type = file_list_get_string_attribute (file_list,
                                                                     "mime_type");
        if (mime_type != NULL &&
            !g_str_equal (mime_type, "inode/directory"))
        {
            g_autofree char *tmp = g_steal_pointer (&attribute_value);
            attribute_value = g_strdup_printf (C_("MIME type description (MIME type)", "%s (%s)"), tmp, mime_type);
        }
    }

    gtk_label_set_text (label, attribute_value);
}

static guint
hash_string_list (GList *list)
{
    guint hash_value = 0;

    for (GList *node = list; node != NULL; node = node->next)
    {
        hash_value ^= g_str_hash ((gconstpointer) node->data);
    }

    return hash_value;
}

static gsize
get_first_word_length (const gchar *str)
{
    const gchar *space_pos = g_strstr_len (str, -1, " ");
    return space_pos ? space_pos - str : strlen (str);
}

static void
update_combo_row_dropdown (AdwComboRow *row,
                           GList       *entries)
{
    /* check if dropdown already exist and is up to date by comparing with stored hash. */
    guint current_hash = hash_string_list (entries);
    guint stored_hash = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "dropdown-hash"));

    if (stored_hash != current_hash)
    {
        /* Recreate the drop down. */
        g_autoptr (GtkStringList) new_model = gtk_string_list_new (NULL);

        for (GList *node = entries; node != NULL; node = node->next)
        {
            const char *entry = (const char *) node->data;
            gtk_string_list_append (new_model, entry);
        }

        adw_combo_row_set_model (row, G_LIST_MODEL (new_model));

        g_object_set_data (G_OBJECT (row), "dropdown-hash", GUINT_TO_POINTER (current_hash));
    }
}

typedef gboolean CompareOwnershipRowFunc (GListModel *list,
                                          guint       position,
                                          const char *str);

static void
select_ownership_row_entry (AdwComboRow             *row,
                            const char              *entry,
                            CompareOwnershipRowFunc  compare_func)
{
    GListModel *list = adw_combo_row_get_model (row);
    guint index_to_select = GTK_INVALID_LIST_POSITION;
    guint n_entries;

    /* check if entry is already selected */
    gint selected_pos = adw_combo_row_get_selected (row);

    if (selected_pos >= 0
        && compare_func (list, selected_pos, entry))
    {
        /* entry already selected */
        return;
    }

    /* check if entry exists in model list */
    n_entries = g_list_model_get_n_items (list);

    for (guint position = 0; position < n_entries; position += 1)
    {
        if (compare_func (list, position, entry))
        {
            /* found entry in list, select it */
            index_to_select = position;
            break;
        }
    }

    if (index_to_select == GTK_INVALID_LIST_POSITION)
    {
        /* entry not in list, add */
        gtk_string_list_append (GTK_STRING_LIST (list), entry);
        index_to_select = n_entries;
    }

    adw_combo_row_set_selected (row, index_to_select);
}

static void
ownership_row_set_single_entry (AdwComboRow             *row,
                                const char              *entry,
                                CompareOwnershipRowFunc  compare_func)
{
    GListModel *list = adw_combo_row_get_model (row);
    gint selected_pos = adw_combo_row_get_selected (row);

    /* check entry not already displayed */
    if (selected_pos < 0
        || g_list_model_get_n_items (list) > 1
        || !compare_func (list, selected_pos, entry))
    {
        g_autoptr (GtkStringList) new_model = gtk_string_list_new (NULL);

        /* set current entry as only entry */
        gtk_string_list_append (new_model, entry);

        adw_combo_row_set_model (row, G_LIST_MODEL (new_model));
        adw_combo_row_set_selected (row, 0);
    }
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

/** Apply group owner change on user selection. */
static void
changed_group_callback (AdwComboRow              *row,
                        GParamSpec               *pspec,
                        NautilusPropertiesWindow *self)
{
    guint selected_pos = adw_combo_row_get_selected (row);

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    if (selected_pos >= 0)
    {
        NautilusFile *file = get_target_file (self);
        GListModel *list = adw_combo_row_get_model (row);
        const gchar *new_group_name = gtk_string_list_get_string (GTK_STRING_LIST (list), selected_pos);
        g_autofree char *current_group_name = nautilus_file_get_group_name (file);

        g_assert (new_group_name);
        g_assert (current_group_name);

        if (strcmp (new_group_name, current_group_name) != 0)
        {
            /* Try to change file group. If this fails, complain to user. */
            unschedule_or_cancel_group_change (self);
            schedule_group_change (self, file, new_group_name);
        }
    }
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
changed_owner_callback (AdwComboRow              *row,
                        GParamSpec               *pspec,
                        NautilusPropertiesWindow *self)
{
    guint selected_pos = adw_combo_row_get_selected (row);
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    if (selected_pos >= 0)
    {
        NautilusFile *file = get_target_file (self);

        GListModel *list = adw_combo_row_get_model (row);
        const gchar *selected_owner_str = gtk_string_list_get_string (GTK_STRING_LIST (list), selected_pos);
        gsize owner_name_length = get_first_word_length (selected_owner_str);
        g_autofree gchar *new_owner_name = g_strndup (selected_owner_str, owner_name_length);
        g_autofree char *current_owner_name = nautilus_file_get_owner_name (file);

        g_assert (NAUTILUS_IS_FILE (file));

        if (strcmp (new_owner_name, current_owner_name) != 0)
        {
            /* Try to change file owner. If this fails, complain to user. */
            unschedule_or_cancel_owner_change (self);
            schedule_owner_change (self, file, new_owner_name);
        }
    }
}

static gboolean
string_list_item_starts_with_word (GListModel *list,
                                   guint       position,
                                   const char *word)
{
    const gchar *entry_str = gtk_string_list_get_string (GTK_STRING_LIST (list), position);

    return g_str_has_prefix (entry_str, word)
           && strlen (word) == get_first_word_length (entry_str);
}

static gboolean
string_list_item_equals_string (GListModel *list,
                                guint       position,
                                const char *string)
{
    const gchar *entry_str = gtk_string_list_get_string (GTK_STRING_LIST (list), position);

    return strcmp (entry_str, string) == 0;
}

/* Select correct owner if file permissions have changed. */
static void
update_owner_row (AdwComboRow       *row,
                  TargetPermissions *target_perm)
{
    NautilusPropertiesWindow *self = target_perm->window;
    gboolean provide_dropdown = (!target_perm->is_multi_file_window
                                 && nautilus_file_can_set_owner (get_target_file (self)));
    gboolean had_dropdown = gtk_widget_is_sensitive (GTK_WIDGET (row));

    gtk_widget_set_sensitive (GTK_WIDGET (row), provide_dropdown);

    /* check if should provide dropdown */
    if (provide_dropdown)
    {
        NautilusFile *file = get_target_file (self);
        g_autofree char *owner_name = nautilus_file_get_owner_name (file);
        GList *users = nautilus_get_user_names ();

        update_combo_row_dropdown (row, users);

        /* display current owner */
        select_ownership_row_entry (row, owner_name, string_list_item_starts_with_word);

        if (!had_dropdown)
        {
            /* Update file when selection changes. */
            g_signal_connect (row, "notify::selected",
                              G_CALLBACK (changed_owner_callback),
                              self);
        }
    }
    else
    {
        g_autofree char *owner_name = file_list_get_string_attribute (self->target_files,
                                                                      "owner");
        if (owner_name == NULL)
        {
            owner_name = g_strdup (_("Multiple"));
        }

        g_signal_handlers_disconnect_by_func (row, G_CALLBACK (changed_owner_callback), self);

        ownership_row_set_single_entry (row, owner_name, string_list_item_starts_with_word);
    }
}

/* Select correct group if file permissions have changed. */
static void
update_group_row (AdwComboRow       *row,
                  TargetPermissions *target_perm)
{
    NautilusPropertiesWindow *self = target_perm->window;
    gboolean provide_dropdown = (!target_perm->is_multi_file_window
                                 && nautilus_file_can_set_group (get_target_file (self)));
    gboolean had_dropdown = gtk_widget_is_sensitive (GTK_WIDGET (row));

    gtk_widget_set_sensitive (GTK_WIDGET (row), provide_dropdown);

    if (provide_dropdown)
    {
        NautilusFile *file = get_target_file (self);
        g_autofree char *group_name = nautilus_file_get_group_name (file);
        GList *groups = nautilus_file_get_settable_group_names (file);
        update_combo_row_dropdown (row, groups);

        /* display current group */
        select_ownership_row_entry (row, group_name, string_list_item_equals_string);

        if (!had_dropdown)
        {
            /* Update file when selection changes. */
            g_signal_connect (row, "notify::selected",
                              G_CALLBACK (changed_group_callback),
                              self);
        }
    }
    else
    {
        g_autofree char *group_name = file_list_get_string_attribute (self->target_files,
                                                                      "group");
        if (group_name == NULL)
        {
            group_name = g_strdup (_("Multiple"));
        }

        g_signal_handlers_disconnect_by_func (row, G_CALLBACK (changed_group_callback), self);

        ownership_row_set_single_entry (row, group_name, string_list_item_equals_string);
    }
}

static void
setup_ownership_row (NautilusPropertiesWindow *self,
                     AdwComboRow              *row)
{
    adw_combo_row_set_model (row, G_LIST_MODEL (gtk_string_list_new (NULL)));

    /* Intial setup of list model is handled via update function, called via properties_window_update. */
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
setup_contents_field (NautilusPropertiesWindow *self)
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
should_show_trashed_info (NautilusPropertiesWindow *self)
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

    if (is_root_directory (file) && nautilus_application_is_sandboxed ())
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
paint_legend (GtkDrawingArea *drawing_area,
              cairo_t        *cr,
              int             width,
              int             height,
              gpointer        data)
{
    GtkWidget *widget = GTK_WIDGET (drawing_area);
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
    GtkBorder border;
    GtkStyleContext *context;
    double x, y, radius;
    gint width, height;

    if (percent_width < .01)
    {
        return;
    }

    context = gtk_widget_get_style_context (widget);
    gtk_style_context_get_border (context, &border);

    gtk_style_context_save (context);
    gtk_style_context_add_class (context, style_class);
    gtk_style_context_get_color (context, &fill);
    gtk_style_context_add_class (context, "border");
    gtk_style_context_get_color (context, &stroke);
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
paint_pie_chart (GtkDrawingArea *drawing_area,
                 cairo_t        *cr,
                 int             width,
                 int             height,
                 gpointer        data)
{
    NautilusPropertiesWindow *self;
    GtkWidget *widget = GTK_WIDGET (drawing_area);
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

    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->pie_chart),
                                    paint_pie_chart, self, NULL);
    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->used_color),
                                    paint_legend, self, NULL);
    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->free_color),
                                    paint_legend, self, NULL);
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
    NautilusDBusLauncher *launcher = nautilus_dbus_launcher_get ();
    g_autoptr (GMount) mount = NULL;
    g_autoptr (GVolume) volume = NULL;
    g_autofree gchar *device_identifier = NULL;
    GVariant *parameters;

    mount = nautilus_file_get_mount (get_original_file (self));
    volume = (mount != NULL) ? g_mount_get_volume (mount) : NULL;
    device_identifier = g_volume_get_identifier (volume,
                                                 G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

    parameters = g_variant_new_parsed ("(objectpath '/org/gnome/DiskUtility', "
                                       "@aay [], {'options': <{'block-device': <%s>}> })",
                                       device_identifier);

    nautilus_dbus_launcher_call (launcher,
                                 NAUTILUS_DBUS_LAUNCHER_DISKS,
                                 "CommandLine", parameters,
                                 GTK_WINDOW (self));
}

static void
add_updatable_label (NautilusPropertiesWindow *self,
                     GtkWidget                *label,
                     const char               *file_attribute)
{
    g_object_set_data_full (G_OBJECT (label), "file_attribute",
                            g_strdup (file_attribute), g_free);

    self->value_fields = g_list_prepend (self->value_fields, label);
}

static void
setup_basic_page (NautilusPropertiesWindow *self)
{
    /* Icon pixmap */

    setup_image_widget (self, should_show_custom_icon_buttons (self));

    self->icon_chooser = NULL;

    update_name_field (self);

    if (should_show_file_type (self))
    {
        gtk_widget_show (self->type_value_label);
        add_updatable_label (self, self->type_value_label, "detailed_type");
    }

    if (should_show_link_target (self))
    {
        gtk_widget_show (self->link_target_row);
        add_updatable_label (self, self->link_target_value_label, "link_target");
    }

    if (is_multi_file_window (self) ||
        nautilus_file_is_directory (get_target_file (self)))
    {
        gtk_widget_show (self->contents_row);
        setup_contents_field (self);
    }
    else
    {
        gtk_widget_show (self->size_row);
        add_updatable_label (self, self->size_value_label, "size_detail");
    }

    if (should_show_location_info (self))
    {
        gtk_widget_show (self->parent_folder_row);
        add_updatable_label (self, self->parent_folder_value_label, "where");
    }

    if (should_show_trashed_info (self))
    {
        gtk_widget_show (self->trashed_list_box);
        add_updatable_label (self, self->original_folder_value_label, "trash_orig_path");
        add_updatable_label (self, self->trashed_on_value_label, "trashed_on_full");
    }

    if (should_show_modified_date (self))
    {
        gtk_widget_show (self->times_list_box);
        gtk_widget_show (self->modified_row);
        add_updatable_label (self, self->modified_value_label, "date_modified_full");
    }

    if (should_show_created_date (self))
    {
        gtk_widget_show (self->created_row);
        gtk_widget_show (self->times_list_box);
        add_updatable_label (self, self->created_value_label, "date_created_full");
    }

    if (should_show_accessed_date (self))
    {
        gtk_widget_show (self->times_list_box);
        gtk_widget_show (self->accessed_row);
        add_updatable_label (self, self->accessed_value_label, "date_accessed_full");
    }

    if (should_show_free_space (self)
        && !should_show_volume_usage (self))
    {
        gtk_widget_show (self->free_space_list_box);
        add_updatable_label (self, self->free_space_value_label, "free_space");
    }

    if (should_show_volume_usage (self))
    {
        gtk_widget_show (self->volume_widget_box);
        g_signal_connect (self->open_in_disks_button, "clicked", G_CALLBACK (open_in_disks), self);
        setup_volume_usage_widget (self);
    }
}

static FilterType
files_get_filter_type (NautilusPropertiesWindow *self)
{
    FilterType filter_type = NO_FILES_OR_FOLDERS;

    for (GList *l = self->target_files; l != NULL && filter_type != FILES_AND_FOLDERS; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        if (nautilus_file_is_directory (file))
        {
            filter_type |= FOLDERS_ONLY;
        }
        else
        {
            filter_type |= FILES_ONLY;
        }
    }

    return filter_type;
}

static gboolean
file_matches_filter_type (NautilusFile *file,
                          FilterType    filter_type)
{
    gboolean is_directory = nautilus_file_is_directory (file);

    switch (filter_type)
    {
        case FILES_AND_FOLDERS:
        {
            return TRUE;
        }

        case FILES_ONLY:
        {
            return !is_directory;
        }

        case FOLDERS_ONLY:
        {
            return is_directory;
        }

        default:
        {
            return FALSE;
        }
    }
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

static void
start_long_operation (NautilusPropertiesWindow *self)
{
    if (self->long_operation_underway == 0)
    {
        /* start long operation */
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "wait");
    }
    self->long_operation_underway++;
}

static void
end_long_operation (NautilusPropertiesWindow *self)
{
    if (gtk_native_get_surface (GTK_NATIVE (self)) != NULL &&
        self->long_operation_underway == 1)
    {
        /* finished !! */
        gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
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
                    FilterType                filter_type,
                    gboolean                  use_original)
{
    for (GList *l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        guint32 permissions;

        if (!nautilus_file_can_get_permissions (file))
        {
            continue;
        }

        if (!nautilus_file_can_get_permissions (file) || !file_matches_filter_type (file, filter_type))
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
                                     FilterType                filter_type)
{
    gboolean first = TRUE;
    guint32 first_permissions = 0;

    for (GList *l = self->target_files; l != NULL; l = l->next)
    {
        NautilusFile *file = l->data;
        guint32 permissions;

        if (!file_matches_filter_type (file, filter_type))
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
execution_bit_changed (NautilusPropertiesWindow *self,
                       GParamSpec               *params,
                       GtkWidget                *widget)
{
    const guint32 permission_mask = UNIX_PERM_USER_EXEC | UNIX_PERM_GROUP_EXEC | UNIX_PERM_OTHER_EXEC;
    const FilterType filter_type = FILES_ONLY;

    /* if activated from switch, switch state is already toggled, thus invert value via XOR. */
    gboolean active = gtk_switch_get_state (self->execution_switch) ^ GTK_IS_SWITCH (widget);

    gboolean inconsistent = gtk_revealer_get_child_revealed (self->execution_inconsistent_revealer);
    gboolean initial_inconsistent = !initial_permission_state_consistent (self, permission_mask, filter_type);

    /* If original execution permissions were inconsistent, circle through
     * states: inconsistent -> on -> off -> inconsistent -> … */
    gboolean set_executable = !active && (inconsistent || !initial_inconsistent);
    gboolean set_inconsistent = !active && !inconsistent && initial_inconsistent;

    update_permissions (self,
                        set_executable ? permission_mask : 0,
                        permission_mask,
                        filter_type,
                        set_inconsistent);
}

static void
update_execution_row (GtkWidget         *row,
                      TargetPermissions *target_perm)
{
    NautilusPropertiesWindow *self = target_perm->window;

    if (target_perm->has_folders)
    {
        gtk_widget_hide (self->execution_list_box);
    }
    else
    {
        gboolean inconsistent = target_perm->file_exec_permissions == PERMISSION_INCONSISTENT;

        gtk_revealer_set_reveal_child (self->execution_inconsistent_revealer, inconsistent);
        gtk_widget_set_has_tooltip (row, inconsistent);

        /* Only toggle switch if permissions are consistent. This still allows switching in case
         * not all permissions can be set. */
        if (!inconsistent)
        {
            g_signal_handlers_block_by_func (self->execution_switch,
                                             G_CALLBACK (execution_bit_changed),
                                             self);

            gtk_switch_set_state (self->execution_switch,
                                  target_perm->file_exec_permissions == PERMISSION_EXEC);

            g_signal_handlers_unblock_by_func (self->execution_switch,
                                               G_CALLBACK (execution_bit_changed),
                                               self);
        }

        gtk_widget_set_sensitive (row,
                                  target_perm->can_set_any_file_permission);

        gtk_widget_show (self->execution_list_box);
    }
}

static void
on_permission_row_change (AdwComboRow              *row,
                          GParamSpec               *pspec,
                          NautilusPropertiesWindow *self)
{
    GListModel *list = adw_combo_row_get_model (row);
    guint position = adw_combo_row_get_selected (row);
    g_autoptr (NautilusPermissionEntry) entry = NULL;
    FilterType filter_type;
    gboolean use_original;
    PermissionType type;
    PermissionValue mask;
    guint32 vfs_new_perm, vfs_mask;

    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    if (position == GTK_INVALID_LIST_POSITION)
    {
        return;
    }

    filter_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "filter-type"));
    type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "permission-type"));

    mask = PERMISSION_READ | PERMISSION_WRITE | ((filter_type == FOLDERS_ONLY) * PERMISSION_EXEC);
    vfs_mask = permission_to_vfs (type, mask);

    entry = g_list_model_get_item (list, position);
    vfs_new_perm = permission_to_vfs (type, entry->permission_value);
    use_original = entry->permission_value & PERMISSION_INCONSISTENT;

    update_permissions (self, vfs_new_perm, vfs_mask,
                        filter_type, use_original);
}

static void
list_store_append_nautilus_permission_entry (GListStore      *list,
                                             PermissionValue  permission_value,
                                             gboolean         describes_folder)
{
    g_autoptr (NautilusPermissionEntry) entry = g_object_new (NAUTILUS_TYPE_PERMISSION_ENTRY, NULL);

    entry->name = g_strdup (permission_value_to_string (permission_value, describes_folder));
    entry->permission_value = permission_value;

    g_list_store_append (list, entry);
}

static gint
get_permission_value_list_position (GListModel      *list,
                                    PermissionValue  wanted_permissions)
{
    const guint n_entries = g_list_model_get_n_items (list);

    for (guint position = 0; position < n_entries; position += 1)
    {
        g_autoptr (NautilusPermissionEntry) entry = g_list_model_get_item (list, position);
        if (entry->permission_value == wanted_permissions)
        {
            return position;
        }
    }

    return -1;
}

static void
update_permission_row (AdwComboRow       *row,
                       TargetPermissions *target_perm)
{
    NautilusPropertiesWindow *self = target_perm->window;
    PermissionType type;
    PermissionValue permissions_to_show;
    FilterType filter_type;
    gboolean is_folder;
    GListModel *model;
    gint position;

    model = adw_combo_row_get_model (row);

    filter_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "filter-type"));
    is_folder = (FOLDERS_ONLY == filter_type);
    type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "permission-type"));

    permissions_to_show = is_folder ? target_perm->folder_permissions[type] :
                                      target_perm->file_permissions[type] & ~PERMISSION_EXEC;

    g_signal_handlers_block_by_func (G_OBJECT (row),
                                     G_CALLBACK (on_permission_row_change),
                                     self);

    position = get_permission_value_list_position (model, permissions_to_show);

    if (position == GTK_INVALID_LIST_POSITION)
    {
        /* configured permissions not listed, create new entry */
        position = g_list_model_get_n_items (model);
        list_store_append_nautilus_permission_entry (G_LIST_STORE (model), permissions_to_show, is_folder);
    }

    adw_combo_row_set_selected (row, position);

    /* Also enable if no files found (for recursive
     *  file changes when only selecting folders) */
    gtk_widget_set_sensitive (GTK_WIDGET (row), is_folder ?
                              target_perm->can_set_all_folder_permission :
                              target_perm->can_set_all_file_permission);

    g_signal_handlers_unblock_by_func (G_OBJECT (row),
                                       G_CALLBACK (on_permission_row_change),
                                       self);
}

static void
setup_permissions_combo_box (GtkComboBox    *combo,
                             PermissionType  type,
                             FilterType      filter_type)
{
    g_autoptr (GtkListStore) store = NULL;
    GtkCellRenderer *cell;
    GtkTreeIter iter;

    store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_STRING);
    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
    gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), COLUMN_ID);

    g_object_set_data (G_OBJECT (combo), "filter-type", GINT_TO_POINTER (filter_type));
    g_object_set_data (G_OBJECT (combo), "permission-type", GINT_TO_POINTER (type));

    if (filter_type == FOLDERS_ONLY)
    {
        if (type != PERMISSION_USER)
        {
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                /* Translators: this is referred to the permissions
                                 * the user has in a directory.
                                 */
                                COLUMN_NAME, _("None"),
                                COLUMN_VALUE, PERMISSION_NONE,
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
                                COLUMN_VALUE, PERMISSION_NONE,
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

static GListModel *
create_permission_list_model (PermissionType type,
                              FilterType     filter_type)
{
    GListStore *store = g_list_store_new (NAUTILUS_TYPE_PERMISSION_ENTRY);

    if (type != PERMISSION_USER)
    {
        list_store_append_nautilus_permission_entry (store, PERMISSION_NONE, /* unused */ FALSE);
    }

    if (filter_type == FOLDERS_ONLY)
    {
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ, TRUE);
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ | PERMISSION_EXEC, TRUE);
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ | PERMISSION_EXEC | PERMISSION_WRITE, TRUE);
    }
    else
    {
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ, FALSE);
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ | PERMISSION_WRITE, FALSE);
    }

    return G_LIST_MODEL (store);
}

static void
create_permissions_row (NautilusPropertiesWindow *self,
                        AdwComboRow              *row,
                        PermissionType            permission_type,
                        FilterType                filter_type)
{
    g_autoptr (GtkExpression) expression = NULL;
    g_autoptr (GListModel) model;

    expression = gtk_property_expression_new (NAUTILUS_TYPE_PERMISSION_ENTRY, NULL, "name");
    adw_combo_row_set_expression (row, expression);

    gtk_widget_show (GTK_WIDGET (row));

    g_object_set_data (G_OBJECT (row), "permission-type", GINT_TO_POINTER (permission_type));
    g_object_set_data (G_OBJECT (row), "filter-type", GINT_TO_POINTER (filter_type));
    model = create_permission_list_model (permission_type, filter_type);
    adw_combo_row_set_model (row, model);

    self->permission_rows = g_list_prepend (self->permission_rows, row);
    g_signal_connect (row, "notify::selected", G_CALLBACK (on_permission_row_change), self);
}

static void
create_simple_permissions (NautilusPropertiesWindow *self)
{
    FilterType filter_type = files_get_filter_type (self);

    g_assert (filter_type != NO_FILES_OR_FOLDERS);

    setup_ownership_row (self, self->owner_row);
    setup_ownership_row (self, self->group_row);

    if (filter_type == FILES_AND_FOLDERS)
    {
        /* owner */
        create_permissions_row (self, self->owner_folder_access_row,
                                PERMISSION_USER, FOLDERS_ONLY);
        create_permissions_row (self, self->owner_file_access_row,
                                PERMISSION_USER, FILES_ONLY);
        /* group */
        create_permissions_row (self, self->group_folder_access_row,
                                PERMISSION_GROUP, FOLDERS_ONLY);
        create_permissions_row (self, self->group_file_access_row,
                                PERMISSION_GROUP, FILES_ONLY);
        /* others */
        create_permissions_row (self, self->others_folder_access_row,
                                PERMISSION_OTHER, FOLDERS_ONLY);
        create_permissions_row (self, self->others_file_access_row,
                                PERMISSION_OTHER, FILES_ONLY);
    }
    else
    {
        create_permissions_row (self, self->owner_access_row,
                                PERMISSION_USER, filter_type);
        create_permissions_row (self, self->group_access_row,
                                PERMISSION_GROUP, filter_type);
        create_permissions_row (self, self->others_access_row,
                                PERMISSION_OTHER, filter_type);
    }

    /* Connect execution bit switch, independent of whether it will be visible or not. */
    g_signal_connect_swapped (self->execution_row, "activated",
                              G_CALLBACK (execution_bit_changed),
                              self);
    g_signal_connect_swapped (self->execution_switch, "notify::active",
                              G_CALLBACK (execution_bit_changed),
                              self);
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
    gboolean use_original;
    FilterType filter_type;
    GList *l;
    GtkTreeModel *model;
    GtkTreeIter iter;
    PermissionType type;
    int new_perm, mask;

    if (response != GTK_RESPONSE_OK)
    {
        g_clear_pointer (&self->change_permission_combos, g_list_free);
        gtk_window_destroy (GTK_WINDOW (dialog));
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
        filter_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "filter-type"));

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
        gtk_tree_model_get (model, &iter,
                            COLUMN_VALUE, &new_perm,
                            COLUMN_USE_ORIGINAL, &use_original, -1);
        if (use_original)
        {
            continue;
        }
        vfs_new_perm = permission_to_vfs (type, new_perm);

        mask = PERMISSION_READ | PERMISSION_WRITE;
        if (filter_type == FOLDERS_ONLY)
        {
            mask |= PERMISSION_EXEC;
        }
        vfs_mask = permission_to_vfs (type, mask);

        if (filter_type == FOLDERS_ONLY)
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
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
set_active_from_umask (GtkComboBox    *combo,
                       PermissionType  type,
                       FilterType      filter_type)
{
    mode_t initial;
    mode_t mask;
    mode_t p;
    const char *id;

    if (filter_type == FOLDERS_ONLY)
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
    setup_permissions_combo_box (combo, PERMISSION_USER, FILES_ONLY);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_USER, FILES_ONLY);

    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "folder_owner_combo"));
    setup_permissions_combo_box (combo, PERMISSION_USER, FOLDERS_ONLY);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_USER, FOLDERS_ONLY);

    /* Group Permissions */
    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "file_group_combo"));
    setup_permissions_combo_box (combo, PERMISSION_GROUP, FILES_ONLY);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_GROUP, FILES_ONLY);

    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "folder_group_combo"));
    setup_permissions_combo_box (combo, PERMISSION_GROUP, FOLDERS_ONLY);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_GROUP, FOLDERS_ONLY);

    /* Others Permissions */
    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "file_other_combo"));
    setup_permissions_combo_box (combo, PERMISSION_OTHER, FILES_ONLY);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_OTHER, FILES_ONLY);

    combo = GTK_COMBO_BOX (gtk_builder_get_object (change_permissions_builder, "folder_other_combo"));
    setup_permissions_combo_box (combo, PERMISSION_OTHER, FOLDERS_ONLY);
    self->change_permission_combos = g_list_prepend (self->change_permission_combos,
                                                     combo);
    set_active_from_umask (combo, PERMISSION_OTHER, FOLDERS_ONLY);

    g_signal_connect (dialog, "response", G_CALLBACK (on_change_permissions_response), self);
    gtk_widget_show (dialog);
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

        gtk_stack_set_visible_child_name (GTK_STACK (self->permissions_stack), "permissions-box");
        create_simple_permissions (self);

#ifdef HAVE_SELINUX
        gtk_widget_show (self->security_context_list_box);

        /* Stash a copy of the file attribute name in this field for the callback's sake. */
        g_object_set_data_full (G_OBJECT (self->security_context_value_label), "file_attribute",
                                g_strdup ("selinux_context"), g_free);

        self->value_fields = g_list_prepend (self->value_fields,
                                             self->security_context_value_label);
#endif

        if (self->has_recursive_apply)
        {
            gtk_widget_show (self->change_permissions_button_box);
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
            adw_status_page_set_description (ADW_STATUS_PAGE (self->unknown_permissions_page), prompt_text);
        }

        gtk_stack_set_visible_child_name (GTK_STACK (self->permissions_stack), "permission-indeterminable");
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
            g_object_set (gtk_notebook_get_page (GTK_NOTEBOOK (self->notebook),
                                                 GTK_WIDGET (page_widget)),
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
                     GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                     GTK_MESSAGE_ERROR);
    }
    else
    {
        gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
        g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
    }
}

static void
forget_clicked_cb (GtkButton *button,
                   gpointer   user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);
    g_autoptr (GAppInfo) info = NULL;

    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));
    if (info != NULL)
    {
        g_autoptr (GError) error = NULL;

        if (!g_app_info_remove_supports_type (info,
                                              self->content_type,
                                              &error))
        {
            show_dialog (_("Could not forget association"),
                         error->message,
                         GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                         GTK_MESSAGE_ERROR);
        }

        gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
    }

    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
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
                     GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
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
    gboolean is_default;
    gboolean can_add;
    gboolean can_remove;

    default_app = g_app_info_get_default_for_type (self->content_type, FALSE);

    is_default = default_app != NULL && g_app_info_equal (info, default_app);
    can_add = app_info_can_add (info, self->content_type);
    can_remove = !is_default && !can_add && g_app_info_can_remove_supports_type (info);

    gtk_widget_set_sensitive (self->forget_button, can_remove);
    gtk_widget_set_sensitive (self->add_button, can_add);
    gtk_widget_set_sensitive (self->set_as_default_button, !is_default);
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
    gtk_box_append (GTK_BOX (self->app_chooser_widget_box), self->app_chooser_widget);

    gtk_app_chooser_widget_set_show_default (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_fallback (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_other (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_widget_show (self->app_chooser_widget);
    g_signal_connect (self->reset_button, "clicked",
                      G_CALLBACK (reset_clicked_cb),
                      self);
    g_signal_connect (self->forget_button, "clicked",
                      G_CALLBACK (forget_clicked_cb),
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
        gtk_window_set_display (GTK_WINDOW (window),
                                gtk_widget_get_display (startup_data->parent_widget));
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
        gtk_widget_show (window->permissions_stack);
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
            gtk_window_set_display (GTK_WINDOW (existing_window),
                                    gtk_widget_get_display (parent_widget));
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

    g_clear_list (&self->permission_rows, NULL);

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

    g_free (self->content_type);
    g_list_free (self->open_with_files);

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
        g_autoptr (GFile) pictures_location = NULL;
        dialog = gtk_file_chooser_dialog_new (_("Select Custom Icon"), GTK_WINDOW (self),
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              _("_Revert"), GTK_RESPONSE_NO,
                                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                                              _("_Open"), GTK_RESPONSE_OK,
                                              NULL);
        pictures_location = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
        gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog),
                                              pictures_location,
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
                gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
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
    GtkWidgetClass *widget_class;
    GObjectClass *oclass;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = real_dispose;
    oclass->finalize = real_finalize;

    gtk_widget_class_add_binding (widget_class,
                                  GDK_KEY_Escape, 0,
                                  (GtkShortcutFunc) gtk_window_close, NULL);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-properties-window.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, notebook);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_image);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_button_image);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, name_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, type_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, link_target_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, link_target_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, size_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, size_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_spinner);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, parent_folder_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, parent_folder_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, trashed_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, trashed_on_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, original_folder_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, times_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, modified_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, modified_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, created_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, created_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, accessed_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, accessed_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, free_space_list_box);
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
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permissions_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, bottom_prompt_seperator);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, not_the_owner_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, unknown_permissions_page);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_folder_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_file_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_folder_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, group_file_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_folder_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, others_file_access_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execution_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execution_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execution_inconsistent_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execution_switch);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, security_context_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, security_context_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, change_permissions_button_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, change_permissions_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, open_with_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, open_with_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, app_chooser_widget_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, reset_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, forget_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, add_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, set_as_default_button);
}

static void
nautilus_properties_window_init (NautilusPropertiesWindow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
