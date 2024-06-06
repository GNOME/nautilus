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

#include <adwaita.h>
#include <cairo.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gunixmounts.h>
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
#include "nautilus-properties-model.h"
#include "nautilus-properties-item.h"
#include "nautilus-scheme.h"
#include "nautilus-signaller.h"
#include "nautilus-tag-manager.h"
#include "nautilus-ui-utilities.h"

/* Put an upper limit on the number of names added to the properties window.
 * This improves performance when selecting a large number of files. Moreover,
 * it is unlikely for many names to fit in the allocated text area.
 */
#define PROPERTIES_MAX_NAMES 50

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

    GList *files;

    AdwWindowTitle *window_title;

    AdwToastOverlay *toast_overlay;
    AdwNavigationView *nav_view;

    /* Basic page */

    GtkStack *icon_stack;
    GtkWidget *icon_image;
    GtkWidget *icon_overlay;
    GtkWidget *select_icon_button;
    GtkWidget *reset_icon_button;
    char *custom_icon_for_undo;

    GtkWidget *star_button;

    GtkLabel *name_value_label;
    GtkWidget *type_value_label;
    GtkLabel *type_file_system_label;
    GtkWidget *size_value_label;
    GtkWidget *contents_box;
    GtkWidget *contents_value_label;
    GtkWidget *free_space_value_label;

    GtkWidget *disk_group;
    GtkWidget *volume_usage_row;
    GtkLevelBar *disk_space_level_bar;
    GtkWidget *disk_space_used_value;
    GtkWidget *disk_space_free_value;
    GtkWidget *disk_space_capacity_value;
    GtkWidget *open_in_disks_button_row;
    gchar *device_identifier;

    GtkWidget *locations_group;
    GtkWidget *link_target_row;
    GtkWidget *contents_spinner;
    guint update_directory_contents_timeout_id;
    guint update_files_timeout_id;
    GtkWidget *parent_folder_row;

    GtkWidget *trashed_group;
    GtkWidget *trashed_on_row;
    GtkWidget *original_folder_row;

    GtkWidget *times_group;
    GtkWidget *modified_row;
    GtkWidget *created_row;
    GtkWidget *accessed_row;

    GtkWidget *permissions_navigation_row;
    GtkWidget *permissions_value_label;

    GtkWidget *extension_models_group;
    GtkWidget *extension_models_list_box;

    /* Permissions page */

    GtkWidget *permissions_stack;

    GtkWidget *unknown_permissions_page;

    AdwBanner *owner_permission_banner;

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

    AdwSwitchRow *execution_row;

    GtkWidget *security_context_group;
    GtkWidget *security_context_row;

    GtkWidget *change_permissions_enclosed_files_group;

    GroupChange *group_change;
    OwnerChange *owner_change;

    GList *permission_rows;
    GList *change_permission_combo_rows;
    GHashTable *initial_permissions;
    gboolean has_recursive_apply;

    /* Extensions */
    GtkListBox *extension_list_box;

    GList *value_labels;
    GList *value_rows;

    char *mime_type;

    gboolean deep_count_finished;
    GList *deep_count_files;
    guint deep_count_spinner_timeout_id;

    guint long_operation_underway;

    GList *changed_files;
    GListStore *extensions_list;
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
} PermissionsInfo;

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
                return _("Read and Write");
            }
            else if (permission_value & PERMISSION_EXEC)
            {
                return _("Create and Delete Files");
            }
            else
            {
                return _("Read and Write, No Access");
            }
        }
        else
        {
            if (!describes_folder)
            {
                return _("Read-Only");
            }
            else if (permission_value & PERMISSION_EXEC)
            {
                return _("Access Files");
            }
            else
            {
                return _("List Files Only");
            }
        }
    }
    else
    {
        if (permission_value & PERMISSION_WRITE)
        {
            if (!describes_folder || permission_value & PERMISSION_EXEC)
            {
                return _("Write-Only");
            }
            else
            {
                return _("Write-Only, No Access");
            }
        }
        else
        {
            if (describes_folder && permission_value & PERMISSION_EXEC)
            {
                return _("Access-Only");
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

typedef struct
{
    GList *files;
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
static void update_execution_row (GtkWidget       *row,
                                  PermissionsInfo *permissions_info);
static void update_permission_row (AdwComboRow     *row,
                                   PermissionsInfo *permissions_info);
static void value_label_update (GtkLabel                 *field,
                                NautilusPropertiesWindow *self);
static void value_row_update (AdwActionRow             *row,
                              NautilusPropertiesWindow *self);
static void properties_window_update (NautilusPropertiesWindow *self,
                                      GList                    *files);
static void is_directory_ready_callback (NautilusFile *file,
                                         gpointer      data);
static void cancel_group_change_callback (GroupChange *change);
static void cancel_owner_change_callback (OwnerChange *change);
static void update_owner_row (AdwComboRow     *row,
                              PermissionsInfo *permissions_info);
static void update_group_row (AdwComboRow     *row,
                              PermissionsInfo *permissions_info);
static void select_image_button_callback (GtkWidget                *widget,
                                          NautilusPropertiesWindow *self);
static void set_icon (const char               *icon_path,
                      NautilusPropertiesWindow *self);
static void remove_pending (StartupData *data,
                            gboolean     cancel_call_when_ready,
                            gboolean     cancel_timed_wait);
static void refresh_extension_model_pages (NautilusPropertiesWindow *self);
static gboolean is_root_directory (NautilusFile *file);
static gboolean is_volume_properties (NautilusPropertiesWindow *self);

G_DEFINE_TYPE (NautilusPropertiesWindow, nautilus_properties_window, ADW_TYPE_WINDOW);

static gboolean
is_multi_file_window (NautilusPropertiesWindow *self)
{
    GList *l;
    int count;

    count = 0;

    for (l = self->files; l != NULL; l = l->next)
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
get_file (NautilusPropertiesWindow *self)
{
    return NAUTILUS_FILE (self->files->data);
}

static void
navigate_permissions_page (NautilusPropertiesWindow *self,
                           GParamSpec               *params,
                           GtkWidget                *widget)
{
    adw_navigation_view_push_by_tag (self->nav_view, "permissions");
}

static void
get_image_for_properties_window (NautilusPropertiesWindow  *self,
                                 char                     **icon_name,
                                 GdkPaintable             **icon_paintable)
{
    g_autoptr (NautilusIconInfo) icon = NULL;
    GList *l;
    gint icon_scale;

    icon_scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));

    for (l = self->files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        g_autoptr (NautilusIconInfo) new_icon = NULL;

        file = NAUTILUS_FILE (l->data);

        if (!icon)
        {
            icon = nautilus_file_get_icon (file, NAUTILUS_GRID_ICON_SIZE_MEDIUM, icon_scale,
                                           NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
                                           NAUTILUS_FILE_ICON_FLAGS_USE_MOUNT_ICON);
        }
        else
        {
            new_icon = nautilus_file_get_icon (file, NAUTILUS_GRID_ICON_SIZE_MEDIUM, icon_scale,
                                               NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
                                               NAUTILUS_FILE_ICON_FLAGS_USE_MOUNT_ICON);
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
        g_autoptr (GIcon) gicon = g_themed_icon_new_from_names ((char *[]){"application-x-generic",
                                                                           "text-x-generic"}, 2);

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
    gtk_image_set_pixel_size (GTK_IMAGE (self->icon_image), pixel_size);
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
on_undo_icon_reset (NautilusPropertiesWindow *self)
{
    NautilusFile *file = get_file (self);

    if (file != NULL && !nautilus_file_is_gone (file))
    {
        nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON,
                                    NULL, self->custom_icon_for_undo);
    }

    g_clear_pointer (&self->custom_icon_for_undo, g_free);
}

static void
reset_icon (NautilusPropertiesWindow *self)
{
    NautilusFile *file = get_file (self);

    if (file != NULL && !nautilus_file_is_gone (file))
    {
        g_set_str (&self->custom_icon_for_undo,
                   nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL));

        nautilus_file_set_metadata (file,
                                    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
                                    NULL, NULL);
    }

    gtk_widget_set_visible (self->reset_icon_button, FALSE);

    g_autoptr (AdwToast) toast = adw_toast_new (_("Custom icon removed"));
    adw_toast_set_button_label (toast, _("_Undo"));
    g_signal_connect_swapped (toast, "button-clicked", G_CALLBACK (on_undo_icon_reset), self);
    adw_toast_overlay_add_toast (self->toast_overlay, g_steal_pointer (&toast));
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
star_clicked (NautilusPropertiesWindow *self)
{
    NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
    NautilusFile *file = get_file (self);
    g_autofree gchar *uri = nautilus_file_get_uri (file);

    if (nautilus_tag_manager_file_is_starred (tag_manager, uri))
    {
        nautilus_tag_manager_unstar_files (tag_manager, G_OBJECT (self),
                                           &(GList){ .data = file }, NULL, NULL);
        gtk_widget_remove_css_class (self->star_button, "starred");
    }
    else
    {
        nautilus_tag_manager_star_files (tag_manager, G_OBJECT (self),
                                         &(GList){ .data = file }, NULL, NULL);
        gtk_widget_add_css_class (self->star_button, "starred");
    }
    gtk_widget_add_css_class (self->star_button, "interacted");
}

static void
update_star (NautilusPropertiesWindow *self,
             NautilusTagManager       *tag_manager)
{
    gboolean is_starred;
    g_autofree gchar *file_uri = NULL;

    file_uri = nautilus_file_get_uri (get_file (self));
    is_starred = nautilus_tag_manager_file_is_starred (tag_manager, file_uri);

    gtk_button_set_icon_name (GTK_BUTTON (self->star_button),
                              is_starred ? "starred-symbolic" : "non-starred-symbolic");
    /* Translators: This is a verb for tagging or untagging a file with a star. */
    gtk_widget_set_tooltip_text (self->star_button, is_starred ? _("Unstar") : _("Star"));
}

static void
on_starred_changed (NautilusTagManager *tag_manager,
                    GList              *changed_files,
                    gpointer            user_data)
{
    NautilusPropertiesWindow *self = user_data;
    NautilusFile *file = get_file (self);

    if (g_list_find (changed_files, file))
    {
        update_star (self, tag_manager);
    }
}

static void
setup_star_button (NautilusPropertiesWindow *self)
{
    NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
    NautilusFile *file = get_file (self);
    g_autoptr (GFile) parent_location = nautilus_file_get_parent_location (file);

    if (parent_location == NULL)
    {
        return;
    }

    if (nautilus_tag_manager_can_star_contents (tag_manager, parent_location))
    {
        gtk_widget_set_visible (self->star_button, TRUE);
        update_star (self, tag_manager);
        g_signal_connect_object (tag_manager, "starred-changed",
                                 G_CALLBACK (on_starred_changed), self, 0);
    }
}

static void
update_image_widget (NautilusPropertiesWindow *self)
{
    gboolean show_select = FALSE;
    gboolean show_reset = FALSE;

    update_properties_window_icon (self);

    if (!is_multi_file_window (self) && !is_volume_properties (self))
    {
        NautilusFile *file = get_file (self);
        if (file != NULL && !nautilus_file_is_gone (file) && !is_root_directory (file))
        {
            const gchar *image_path = nautilus_file_get_metadata (file,
                                                                  NAUTILUS_METADATA_KEY_CUSTOM_ICON,
                                                                  NULL);

            show_select = nautilus_file_is_directory (file);
            show_reset = (image_path != NULL);
        }
    }

    if (!show_select && !show_reset)
    {
        gtk_stack_set_visible_child (self->icon_stack, self->icon_image);
    }
    else
    {
        gtk_stack_set_visible_child (self->icon_stack, self->icon_overlay);

        gtk_widget_set_visible (self->select_icon_button, show_select);
        gtk_widget_set_visible (self->reset_icon_button, show_reset);
    }
}

static void
setup_image_widget (NautilusPropertiesWindow *self)
{
    /* prepare the image to receive dropped objects to assign custom images */
    GtkDropTarget *target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    gtk_widget_add_controller (self->icon_overlay, GTK_EVENT_CONTROLLER (target));
    g_signal_connect (target, "drop",
                      G_CALLBACK (nautilus_properties_window_drag_drop_cb), self->icon_image);

    update_image_widget (self);
}

static void
update_name_field (NautilusPropertiesWindow *self)
{
    g_autoptr (GString) name_str = g_string_new ("");
    g_autofree gchar *os_name = NULL;
    gchar *name_value;
    guint file_counter = 0;

    for (GList *l = self->files; l != NULL && file_counter <= PROPERTIES_MAX_NAMES; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_is_gone (file))
        {
            file_counter += 1;
            if (file_counter > 1)
            {
                g_string_append (name_str, ", ");
            }

            g_string_append (name_str, nautilus_file_get_display_name (file));
        }
    }

    if (!is_multi_file_window (self) && is_root_directory (get_file (self)))
    {
        os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
        name_value = (os_name != NULL) ? os_name : _("Operating System");
    }
    else
    {
        name_value = name_str->str;
    }

    gtk_label_set_text (self->name_value_label, name_value);
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
            g_autofree char *attr = NULL;
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

static GtkWidget *
create_extension_group_row (NautilusPropertiesItem   *item,
                            NautilusPropertiesWindow *self)
{
    GtkWidget *row = adw_action_row_new ();

    adw_action_row_set_subtitle_selectable (ADW_ACTION_ROW (row), TRUE);
    gtk_widget_add_css_class (row, "property");

    g_object_bind_property (item, "name", row, "title", G_BINDING_SYNC_CREATE);
    g_object_bind_property (item, "value", row, "subtitle", G_BINDING_SYNC_CREATE);

    return row;
}

static void
navigate_extension_model_page (AdwPreferencesRow *row,
                               gpointer           user_data)
{
    NautilusPropertiesWindow *self = NAUTILUS_PROPERTIES_WINDOW (user_data);
    GListModel *list_model = g_object_get_data (G_OBJECT (row), "nautilus-extension-properties-model");
    AdwNavigationPage *page;

    gtk_list_box_bind_model (self->extension_list_box,
                             list_model,
                             (GtkListBoxCreateWidgetFunc) create_extension_group_row,
                             self,
                             NULL);

    page = adw_navigation_view_find_page (ADW_NAVIGATION_VIEW (self->nav_view), "extension");
    adw_navigation_page_set_title (page, adw_preferences_row_get_title (row));
    adw_navigation_view_push (self->nav_view, page);
}

static GtkWidget *
add_extension_model_page (NautilusPropertiesModel  *model,
                          NautilusPropertiesWindow *self)
{
    GListModel *list_model = nautilus_properties_model_get_model (model);
    GtkWidget *row;

    row = adw_action_row_new ();
    g_object_bind_property (model, "title", row, "title", G_BINDING_SYNC_CREATE);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
    gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
    adw_action_row_add_suffix (ADW_ACTION_ROW (row),
                               gtk_image_new_from_icon_name ("go-next-symbolic"));
    g_signal_connect (row, "activated",
                      G_CALLBACK (navigate_extension_model_page), self);

    g_object_set_data (G_OBJECT (row), "nautilus-extension-properties-model", list_model);

    return row;
}

static void
remove_from_dialog (NautilusPropertiesWindow *self,
                    NautilusFile             *file)
{
    GList *link = g_list_find (self->files, file);

    g_return_if_fail (link != NULL);

    g_hash_table_remove (self->initial_permissions, file);

    g_signal_handlers_disconnect_by_func (file,
                                          G_CALLBACK (file_changed_callback),
                                          self);
    nautilus_file_monitor_remove (file, &self->files);

    /* Delete list link and release the reference it held. */
    self->files = g_list_delete_link (self->files, link);
    g_object_unref (file);
}

static gboolean
start_spinner_callback (NautilusPropertiesWindow *self)
{
    gtk_widget_set_visible (self->contents_spinner, TRUE);
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
    gtk_widget_set_visible (self->contents_spinner, FALSE);
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

static PermissionsInfo *
get_permissions_info (NautilusPropertiesWindow *self)
{
    PermissionsInfo *p = g_new0 (PermissionsInfo, 1);
    p->window = self;
    p->can_set_all_folder_permission = TRUE;
    p->can_set_all_file_permission = TRUE;

    for (GList *entry = self->files; entry != NULL; entry = entry->next)
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
update_permissions_navigation_row (NautilusPropertiesWindow *self,
                                   PermissionsInfo          *permissions_info)
{
    if (!permissions_info->is_multi_file_window)
    {
        uid_t user_id = geteuid ();
        gid_t group_id = getegid ();
        PermissionType permission_type = PERMISSION_OTHER;
        const gchar *text;

        if (user_id == nautilus_file_get_uid (get_file (self)))
        {
            permission_type = PERMISSION_USER;
        }
        else if (group_id == nautilus_file_get_gid (get_file (self)))
        {
            permission_type = PERMISSION_GROUP;
        }

        if (nautilus_file_is_directory (get_file (self)))
        {
            text = permission_value_to_string (permissions_info->folder_permissions[permission_type], TRUE);
        }
        else
        {
            text = permission_value_to_string (permissions_info->file_permissions[permission_type], FALSE);
        }

        gtk_label_set_text (GTK_LABEL (self->permissions_value_label), text);
    }
}

static void
update_extension_list (NautilusPropertiesWindow *self)
{
    if (is_multi_file_window (self))
    {
        g_free (self->mime_type);
        return;
    }

    const char *mime_type = nautilus_file_get_mime_type (get_file (self));

    if (g_set_str (&self->mime_type, mime_type))
    {
        refresh_extension_model_pages (self);
    }
}

static void
properties_window_update (NautilusPropertiesWindow *self,
                          GList                    *files)
{
    NautilusFile *changed_file;
    gboolean dirty = FALSE;

    if (files == NULL)
    {
        dirty = TRUE;
    }

    for (GList *tmp = files; tmp != NULL; tmp = tmp->next)
    {
        changed_file = NAUTILUS_FILE (tmp->data);

        if (changed_file && nautilus_file_is_gone (changed_file))
        {
            /* Remove the file from the property dialog */
            remove_from_dialog (self, changed_file);
            changed_file = NULL;

            if (self->files == NULL)
            {
                return;
            }
        }
        if (changed_file == NULL ||
            g_list_find (self->files, changed_file))
        {
            dirty = TRUE;
        }
    }

    if (dirty)
    {
        g_autofree PermissionsInfo *permissions_info = get_permissions_info (self);

        update_image_widget (self);
        update_name_field (self);
        update_permissions_navigation_row (self, permissions_info);
        update_owner_row (self->owner_row, permissions_info);
        update_group_row (self->group_row, permissions_info);
        update_execution_row (GTK_WIDGET (self->execution_row), permissions_info);
        g_list_foreach (self->permission_rows,
                        (GFunc) update_permission_row,
                        permissions_info);
        g_list_foreach (self->value_labels,
                        (GFunc) value_label_update,
                        self);
        g_list_foreach (self->value_rows,
                        (GFunc) value_row_update,
                        self);
    }

    update_extension_list (self);
}

static gboolean
update_files_callback (gpointer data)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (data);

    self->update_files_timeout_id = 0;

    properties_window_update (self, self->changed_files);

    if (self->files == NULL)
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

static void
value_label_update (GtkLabel                 *label,
                    NautilusPropertiesWindow *self)
{
    const char *attr_value_name;
    const char *attr_tooltip_name;
    g_autofree char *attribute_value = NULL;

    g_assert (GTK_IS_LABEL (label));

    attr_value_name = g_object_get_data (G_OBJECT (label),
                                         "file_attribute_value");
    attr_tooltip_name = g_object_get_data (G_OBJECT (label),
                                           "file_attribute_tooltip");

    attribute_value = file_list_get_string_attribute (self->files,
                                                      attr_value_name);

    if (attr_tooltip_name != NULL)
    {
        g_autofree char *attribute_tooltip = file_list_get_string_attribute (self->files,
                                                                             attr_tooltip_name);

        gtk_widget_set_tooltip_text (GTK_WIDGET (label), attribute_tooltip);
    }

    gtk_label_set_text (label, attribute_value);
}

static void
value_row_update (AdwActionRow             *row,
                  NautilusPropertiesWindow *self)
{
    const char *attribute_name;
    g_autofree char *attribute_value = NULL;

    g_assert (ADW_IS_ACTION_ROW (row));

    attribute_name = g_object_get_data (G_OBJECT (row), "file_attribute");

    attribute_value = file_list_get_string_attribute (self->files,
                                                      attribute_name);

    adw_action_row_set_subtitle (row, attribute_value);
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
    if (space_pos != NULL)
    {
        /* Calculate length through pointer arithmetic. */
        return (gsize) (space_pos - str);
    }

    return strlen (str);
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
        guint n_items = 0;

        for (GList *node = entries; node != NULL; node = node->next, n_items++)
        {
            const char *entry = (const char *) node->data;
            gtk_string_list_append (new_model, entry);
        }

        adw_combo_row_set_model (row, G_LIST_MODEL (new_model));
        adw_combo_row_set_enable_search (row, n_items >= 7);

        g_object_set_data (G_OBJECT (row), "dropdown-hash", GUINT_TO_POINTER (current_hash));
    }
}

typedef gboolean CompareOwnershipRowFunc (GListModel *list,
                                          guint position,
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

    if (selected_pos != GTK_INVALID_LIST_POSITION)
    {
        NautilusFile *file = get_file (self);
        GListModel *list = adw_combo_row_get_model (row);
        const gchar *new_group_name = gtk_string_list_get_string (GTK_STRING_LIST (list), selected_pos);
        const char *current_group_name = nautilus_file_get_group_name (file);

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

    if (selected_pos != GTK_INVALID_LIST_POSITION)
    {
        NautilusFile *file = get_file (self);

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
update_owner_row (AdwComboRow     *row,
                  PermissionsInfo *permissions_info)
{
    NautilusPropertiesWindow *self = permissions_info->window;
    gboolean provide_dropdown = (!permissions_info->is_multi_file_window
                                 && nautilus_file_can_set_owner (get_file (self)));
    gboolean had_dropdown = gtk_widget_is_sensitive (GTK_WIDGET (row));

    gtk_widget_set_sensitive (GTK_WIDGET (row), provide_dropdown);

    /* check if should provide dropdown */
    if (provide_dropdown)
    {
        NautilusFile *file = get_file (self);
        g_autofree char *owner_name = nautilus_file_get_owner_name (file);
        GList *users = nautilus_get_user_names ();

        update_combo_row_dropdown (row, users);
        g_list_free_full (users, g_free);

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
        g_autofree char *owner_name = file_list_get_string_attribute (self->files,
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
update_group_row (AdwComboRow     *row,
                  PermissionsInfo *permissions_info)
{
    NautilusPropertiesWindow *self = permissions_info->window;
    gboolean provide_dropdown = (!permissions_info->is_multi_file_window
                                 && nautilus_file_can_set_group (get_file (self)));
    gboolean had_dropdown = gtk_widget_is_sensitive (GTK_WIDGET (row));

    gtk_widget_set_sensitive (GTK_WIDGET (row), provide_dropdown);

    if (provide_dropdown)
    {
        NautilusFile *file = get_file (self);
        const char *group_name = nautilus_file_get_group_name (file);
        GList *groups = nautilus_file_get_settable_group_names (file);
        update_combo_row_dropdown (row, groups);

        g_list_free_full (groups, g_free);

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
        g_autofree char *group_name = file_list_get_string_attribute (self->files,
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
                 GHashTable   *prefix_candidates)
{
    for (g_autoptr (GFile) parent = nautilus_file_get_parent_location (file);
         parent != NULL;)
    {
        g_autofree gchar *parent_uri = g_file_get_uri (parent);

        if (g_hash_table_lookup (prefix_candidates, parent_uri))
        {
            return TRUE;
        }
        else
        {
            g_autoptr (GFile) child = parent;
            parent = g_file_get_parent (parent);
        }
    }

    return FALSE;
}

static void
directory_contents_value_field_update (NautilusPropertiesWindow *self)
{
    NautilusRequestStatus file_status;
    g_autoptr (GHashTable) prefix_hashes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                  NULL, g_free);
    g_autofree char *text = NULL;
    g_autofree char *bytes_str = NULL;
    g_autofree char *tooltip = NULL;
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

    for (l = self->files; l; l = l->next)
    {
        g_autoptr (GFile) location = nautilus_file_get_location (NAUTILUS_FILE (l->data));
        GFileType type = nautilus_file_get_file_type (NAUTILUS_FILE (l->data));

        if (type & ~(G_FILE_TYPE_REGULAR | G_FILE_TYPE_SPECIAL | G_FILE_TYPE_SHORTCUT))
        {
            g_hash_table_add (prefix_hashes, g_file_get_uri (location));
        }
    }

    for (l = self->files; l; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (file_has_prefix (file, prefix_hashes))
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
                text = g_strdup (_("Empty folder"));
            }
            else
            {
                text = g_strdup (_("Contents unreadable"));
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

    bytes_str = g_strdup_printf ("%'" G_GOFFSET_FORMAT, total_size);

    tooltip = g_strdup_printf (ngettext ("%s byte", "%s bytes", total_size),
                               bytes_str);

    gtk_label_set_text (GTK_LABEL (self->contents_value_label),
                        text);

    gtk_widget_set_tooltip_text (GTK_WIDGET (self->contents_value_label),
                                 tooltip);

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
    g_list_foreach (self->files,
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
    g_autoptr (GFile) location = nautilus_file_get_location (file);

    return nautilus_is_root_for_scheme (location, SCHEME_NETWORK);
}

static gboolean
is_burn_directory (NautilusFile *file)
{
    g_autoptr (GFile) location = nautilus_file_get_location (file);

    return nautilus_is_root_for_scheme (location, SCHEME_BURN);
}


static gboolean
is_volume_properties (NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    gboolean success = FALSE;

    if (is_multi_file_window (self))
    {
        return FALSE;
    }

    file = get_file (self);

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

static gboolean
is_single_file_type (NautilusPropertiesWindow *self)
{
    if (is_multi_file_window (self))
    {
        GList *l = self->files;
        const char *mime_type = nautilus_file_get_mime_type (NAUTILUS_FILE (l->data));

        for (l = l->next; l != NULL; l = l->next)
        {
            NautilusFile *file = NAUTILUS_FILE (l->data);
            g_autofree gchar *next_mime_type = NULL;

            if (nautilus_file_is_gone (file))
            {
                continue;
            }

            if (g_strcmp0 (nautilus_file_get_mime_type (file), mime_type) != 0)
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static gboolean
should_show_file_type (NautilusPropertiesWindow *self)
{
    if (!is_single_file_type (self))
    {
        return FALSE;
    }

    if (!is_multi_file_window (self)
        && (nautilus_file_is_in_trash (get_file (self)) ||
            nautilus_file_is_directory (get_file (self)) ||
            is_network_directory (get_file (self)) ||
            is_burn_directory (get_file (self)) ||
            is_volume_properties (self)))
    {
        return FALSE;
    }

    return TRUE;
}

static GFile *
get_parent_location (NautilusFile *file)
{
    if (nautilus_file_is_in_recent (file))
    {
        /* Use activation location since parent location points to recent:// */
        g_autoptr (GFile) location = nautilus_file_get_activation_location (file);

        return g_file_get_parent (location);
    }
    else
    {
        return nautilus_file_get_parent_location (file);
    }
}

static gboolean
should_show_location_info (NautilusPropertiesWindow *self)
{
    g_autoptr (GFile) first_parent = get_parent_location (get_file (self));

    if (first_parent == NULL || nautilus_file_is_in_trash (get_file (self)))
    {
        return FALSE;
    }

    if (is_multi_file_window (self))
    {
        for (GList *l = self->files->next; l != NULL; l = l->next)
        {
            g_autoptr (GFile) parent = get_parent_location (NAUTILUS_FILE (l->data));
            if (!g_file_equal (first_parent, parent))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static gboolean
should_show_trashed_info (NautilusPropertiesWindow *self)
{
    GList *l;

    for (l = self->files; l != NULL; l = l->next)
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
    if (nautilus_file_list_are_all_folders (self->files)
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
        && nautilus_file_is_symbolic_link (get_file (self)))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
should_show_free_space (NautilusPropertiesWindow *self)
{
    if (!is_multi_file_window (self)
        && (nautilus_file_is_in_trash (get_file (self)) ||
            is_network_directory (get_file (self)) ||
            nautilus_file_is_in_recent (get_file (self)) ||
            is_burn_directory (get_file (self)) ||
            is_volume_properties (self)))
    {
        return FALSE;
    }

    if (nautilus_file_list_are_all_folders (self->files))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
should_show_volume_usage (NautilusPropertiesWindow *self)
{
    return is_volume_properties (self);
}

static void
setup_volume_information (NautilusPropertiesWindow *self)
{
    NautilusFile *file;
    const char *fs_type;
    g_autofree gchar *uri = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFileInfo) info = NULL;

    file = get_file (self);

    uri = nautilus_file_get_activation_uri (file);

    location = g_file_new_for_uri (uri);
    info = g_file_query_filesystem_info (location, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
                                         NULL, NULL);
    if (info)
    {
        fs_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);

        /* We shouldn't be using filesystem::type, it's not meant for UI.
         * https://gitlab.gnome.org/GNOME/nautilus/-/issues/98
         *
         * Until we fix that issue, workaround this common outrageous case. */
        if (g_strcmp0 (fs_type, "msdos") == 0)
        {
            fs_type = "FAT";
        }

        if (fs_type != NULL)
        {
            /* Translators: %s will be filled with a filesystem type, such as 'ext4' or 'msdos'. */
            g_autofree gchar *fs_label = g_strdup_printf (_("%s Filesystem"), fs_type);
            gchar *cap_label = nautilus_capitalize_str (fs_label);
            if (cap_label != NULL)
            {
                g_free (fs_label);
                fs_label = cap_label;
            }

            gtk_label_set_text (self->type_file_system_label, fs_label);
            gtk_widget_set_visible (GTK_WIDGET (self->type_file_system_label), TRUE);
        }
    }
}

static void
setup_volume_usage_widget (NautilusPropertiesWindow *self)
{
    NautilusFile *file = get_file (self);
    g_autofree gchar *uri = nautilus_file_get_activation_uri (file);
    g_autoptr (GFile) location = g_file_new_for_uri (uri);
    g_autoptr (GFileInfo) info = NULL;
    guint64 volume_capacity = 0;
    guint64 volume_free = 0;
    guint64 volume_used = 0;

    info = g_file_query_filesystem_info (location, "filesystem::*", NULL, NULL);
    if (info != NULL)
    {
        volume_capacity = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        volume_free = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED))
        {
            volume_used = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED);
        }
        else
        {
            volume_used = volume_capacity - volume_free;
        }
    }

    gtk_widget_set_visible (self->volume_usage_row, (volume_capacity > 0));
    if (volume_capacity > 0)
    {
        g_autofree gchar *capacity = g_format_size (volume_capacity);
        g_autofree gchar *used = g_format_size (volume_used);
        g_autofree gchar *free = g_format_size (volume_free);

        gtk_label_set_text (GTK_LABEL (self->disk_space_used_value), used);
        gtk_label_set_text (GTK_LABEL (self->disk_space_free_value), free);
        gtk_label_set_text (GTK_LABEL (self->disk_space_capacity_value), capacity);
        gtk_level_bar_set_value (self->disk_space_level_bar, (double) volume_used / (double) volume_capacity);
        /* display color changing based on filled level */
        gtk_level_bar_add_offset_value (self->disk_space_level_bar, GTK_LEVEL_BAR_OFFSET_FULL, 0.0);
    }
}

static void
open_parent_folder (NautilusPropertiesWindow *self)
{
    NautilusFile *file = get_file (self);
    g_autoptr (GFile) parent_location = get_parent_location (file);

    g_return_if_fail (parent_location != NULL);

    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             parent_location,
                                             NAUTILUS_OPEN_FLAG_NEW_WINDOW,
                                             &(GList){ .data = file },
                                             NULL, NULL);
}

static void
open_link_target (NautilusPropertiesWindow *self)
{
    g_autofree gchar *link_target_uri = NULL;
    g_autoptr (GFile) link_target_location = NULL;
    g_autoptr (NautilusFile) link_target_file = NULL;
    g_autoptr (GFile) parent_location = NULL;

    link_target_uri = nautilus_file_get_symbolic_link_target_uri (get_file (self));
    g_return_if_fail (link_target_uri != NULL);
    link_target_location = g_file_new_for_uri (link_target_uri);
    link_target_file = nautilus_file_get (link_target_location);
    parent_location = nautilus_file_get_parent_location (link_target_file);
    g_return_if_fail (parent_location != NULL);

    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             parent_location,
                                             NAUTILUS_OPEN_FLAG_NEW_WINDOW,
                                             &(GList){ .data = link_target_file },
                                             NULL, NULL);
}

static void
setup_open_in_disks (NautilusPropertiesWindow *self)
{
    g_autoptr (GMount) mount = NULL;
    g_autoptr (GVolume) volume = NULL;

    mount = nautilus_file_get_mount (get_file (self));
    volume = (mount != NULL) ? g_mount_get_volume (mount) : NULL;

    if (volume != NULL)
    {
        self->device_identifier = g_volume_get_identifier (volume,
                                                           G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    }
    else
    {
        g_autoptr (GFile) location = NULL;
        g_autofree gchar *path = NULL;
        g_autoptr (GUnixMountEntry) mount_entry = NULL;

        location = nautilus_file_get_location (get_file (self));
        path = g_file_get_path (location);
        mount_entry = (path != NULL) ? g_unix_mount_at (path, NULL) : NULL;
        if (mount_entry != NULL)
        {
            self->device_identifier = g_strdup (g_unix_mount_get_device_path (mount_entry));
        }
    }

    gtk_widget_set_visible (self->open_in_disks_button_row, (self->device_identifier != NULL));
}

static void
open_in_disks (NautilusPropertiesWindow *self)
{
    NautilusDBusLauncher *launcher = nautilus_dbus_launcher_get ();
    GVariant *parameters;

    g_return_if_fail (self->device_identifier != NULL);

    parameters = g_variant_new_parsed ("(objectpath '/org/gnome/DiskUtility', "
                                       "@aay [], {'options': <{'block-device': <%s>}> })",
                                       self->device_identifier);

    nautilus_dbus_launcher_call (launcher,
                                 NAUTILUS_DBUS_LAUNCHER_DISKS,
                                 "CommandLine", parameters,
                                 GTK_WINDOW (self));
}

static void
add_updatable_label (NautilusPropertiesWindow *self,
                     GtkWidget                *label,
                     const char               *file_attribute_value,
                     const char               *file_attribute_tooltip)
{
    g_object_set_data_full (G_OBJECT (label),
                            "file_attribute_value",
                            g_strdup (file_attribute_value),
                            g_free);

    if (file_attribute_tooltip != NULL)
    {
        g_object_set_data_full (G_OBJECT (label),
                                "file_attribute_tooltip",
                                g_strdup (file_attribute_tooltip),
                                g_free);
    }

    self->value_labels = g_list_prepend (self->value_labels, label);
}

static void
add_updatable_row (NautilusPropertiesWindow *self,
                   GtkWidget                *row,
                   const char               *file_attribute)
{
    g_object_set_data_full (G_OBJECT (row), "file_attribute",
                            g_strdup (file_attribute), g_free);

    self->value_rows = g_list_prepend (self->value_rows, row);
}

static void
setup_basic_page (NautilusPropertiesWindow *self)
{
    gboolean should_show_locations_group = FALSE;

    /* Icon pixmap */

    setup_image_widget (self);

    if (!is_multi_file_window (self))
    {
        setup_star_button (self);
    }

    update_name_field (self);

    if (should_show_volume_usage (self))
    {
        setup_open_in_disks (self);
        setup_volume_information (self);
        setup_volume_usage_widget (self);

        gtk_widget_set_visible (self->disk_group,
                                (gtk_widget_get_visible (self->volume_usage_row) ||
                                 gtk_widget_get_visible (self->open_in_disks_button_row)));
    }

    if (should_show_file_type (self))
    {
        gtk_widget_set_visible (self->type_value_label, TRUE);
        add_updatable_label (self, self->type_value_label, "detailed_type", "mime_type");
    }

    if (should_show_link_target (self))
    {
        gtk_widget_set_visible (self->link_target_row, TRUE);
        add_updatable_row (self, self->link_target_row, "link_target");

        should_show_locations_group = TRUE;
    }

    if (is_multi_file_window (self) ||
        nautilus_file_is_directory (get_file (self)))
    {
        /* We have a more efficient way to measure used space in volumes. */
        if (!is_volume_properties (self))
        {
            gtk_widget_set_visible (self->contents_box, TRUE);
            setup_contents_field (self);
        }
    }
    else
    {
        gtk_widget_set_visible (self->size_value_label, TRUE);
        add_updatable_label (self, self->size_value_label, "size", "size_detail");
    }

    if (should_show_location_info (self))
    {
        gtk_widget_set_visible (self->parent_folder_row, TRUE);
        add_updatable_row (self, self->parent_folder_row, "where");

        should_show_locations_group = TRUE;
    }

    if (should_show_trashed_info (self))
    {
        gtk_widget_set_visible (self->trashed_group, TRUE);
        add_updatable_row (self, self->original_folder_row, "trash_orig_path");
        add_updatable_row (self, self->trashed_on_row, "trashed_on_full");
    }

    if (should_show_modified_date (self))
    {
        gtk_widget_set_visible (self->times_group, TRUE);
        gtk_widget_set_visible (self->modified_row, TRUE);
        add_updatable_row (self, self->modified_row, "date_modified_full");
    }

    if (should_show_created_date (self))
    {
        gtk_widget_set_visible (self->created_row, TRUE);
        gtk_widget_set_visible (self->times_group, TRUE);
        add_updatable_row (self, self->created_row, "date_created_full");
    }

    if (should_show_accessed_date (self))
    {
        gtk_widget_set_visible (self->times_group, TRUE);
        gtk_widget_set_visible (self->accessed_row, TRUE);
        add_updatable_row (self, self->accessed_row, "date_accessed_full");
    }

    if (should_show_free_space (self))
    {
        /* We have a more efficient way to measure free space in volumes. */
        if (!is_volume_properties (self))
        {
            gtk_widget_set_visible (self->free_space_value_label, TRUE);
            add_updatable_label (self, self->free_space_value_label, "free_space", NULL);
        }
    }

    if (should_show_locations_group)
    {
        gtk_widget_set_visible (self->locations_group, TRUE);
    }
}

static FilterType
files_get_filter_type (NautilusPropertiesWindow *self)
{
    FilterType filter_type = NO_FILES_OR_FOLDERS;

    for (GList *l = self->files; l != NULL && filter_type != FILES_AND_FOLDERS; l = l->next)
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

    for (l = self->files; l != NULL; l = l->next)
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
    for (GList *l = self->files; l != NULL; l = l->next)
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

static void
execution_bit_changed (NautilusPropertiesWindow *self,
                       GParamSpec               *params,
                       GtkWidget                *widget)
{
    const guint32 permission_mask = UNIX_PERM_USER_EXEC | UNIX_PERM_GROUP_EXEC | UNIX_PERM_OTHER_EXEC;
    const FilterType filter_type = FILES_ONLY;

    gboolean set_executable = adw_switch_row_get_active (self->execution_row);

    update_permissions (self,
                        set_executable ? permission_mask : 0,
                        permission_mask,
                        filter_type,
                        FALSE);
}

static gboolean
should_show_exectution_switch (NautilusPropertiesWindow *self)
{
    if (is_multi_file_window (self))
    {
        return FALSE;
    }

    const char *mime_type = nautilus_file_get_mime_type (get_file (self));
    return g_content_type_can_be_executable (mime_type);
}

static void
update_execution_row (GtkWidget       *row,
                      PermissionsInfo *permissions_info)
{
    NautilusPropertiesWindow *self = permissions_info->window;

    if (!should_show_exectution_switch (self))
    {
        gtk_widget_set_visible (GTK_WIDGET (self->execution_row), FALSE);
    }
    else
    {
        g_signal_handlers_block_by_func (self->execution_row,
                                         G_CALLBACK (execution_bit_changed),
                                         self);

        adw_switch_row_set_active (self->execution_row,
                                   permissions_info->file_exec_permissions == PERMISSION_EXEC);

        g_signal_handlers_unblock_by_func (self->execution_row,
                                           G_CALLBACK (execution_bit_changed),
                                           self);

        gtk_widget_set_sensitive (row,
                                  permissions_info->can_set_any_file_permission);

        gtk_widget_set_visible (GTK_WIDGET (self->execution_row), TRUE);
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

static guint
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

    return GTK_INVALID_LIST_POSITION;
}

static void
update_permission_row (AdwComboRow     *row,
                       PermissionsInfo *permissions_info)
{
    NautilusPropertiesWindow *self = permissions_info->window;
    PermissionType type;
    PermissionValue permissions_to_show;
    FilterType filter_type;
    gboolean is_folder;
    GListModel *model;
    guint position;

    model = adw_combo_row_get_model (row);

    filter_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "filter-type"));
    is_folder = (FOLDERS_ONLY == filter_type);
    type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "permission-type"));

    permissions_to_show = is_folder ? permissions_info->folder_permissions[type] :
                                      permissions_info->file_permissions[type] & ~PERMISSION_EXEC;

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
                              permissions_info->can_set_all_folder_permission :
                              permissions_info->can_set_all_file_permission);

    g_signal_handlers_unblock_by_func (G_OBJECT (row),
                                       G_CALLBACK (on_permission_row_change),
                                       self);
}

static void
setup_permissions_combo_row (AdwComboRow    *combo_row,
                             PermissionType  type,
                             FilterType      filter_type)
{
    g_autoptr (GListStore) store = NULL;
    g_autoptr (GtkExpression) expression = NULL;

    store = g_list_store_new (NAUTILUS_TYPE_PERMISSION_ENTRY);
    adw_combo_row_set_model (combo_row, G_LIST_MODEL (store));
    expression = gtk_property_expression_new (NAUTILUS_TYPE_PERMISSION_ENTRY, NULL, "name");
    adw_combo_row_set_expression (combo_row, expression);


    g_object_set_data (G_OBJECT (combo_row), "filter-type", GINT_TO_POINTER (filter_type));
    g_object_set_data (G_OBJECT (combo_row), "permission-type", GINT_TO_POINTER (type));

    if (filter_type == FOLDERS_ONLY)
    {
        if (type != PERMISSION_USER)
        {
            list_store_append_nautilus_permission_entry (store, PERMISSION_NONE, TRUE);
        }
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ, TRUE);
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ | PERMISSION_EXEC, TRUE);
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ | PERMISSION_EXEC | PERMISSION_WRITE, TRUE);
    }
    else
    {
        if (type != PERMISSION_USER)
        {
            list_store_append_nautilus_permission_entry (store, PERMISSION_NONE, FALSE);
        }
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ, FALSE);
        list_store_append_nautilus_permission_entry (store, PERMISSION_READ | PERMISSION_WRITE, FALSE);
    }
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
    g_autoptr (GListModel) model = NULL;

    expression = gtk_property_expression_new (NAUTILUS_TYPE_PERMISSION_ENTRY, NULL, "name");
    adw_combo_row_set_expression (row, expression);

    gtk_widget_set_visible (GTK_WIDGET (row), TRUE);

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
    g_signal_connect_swapped (self->execution_row, "notify::active",
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
on_change_permissions_response_cancel (AdwWindow *dialog,
                                       gpointer   user_data)
{
    NautilusPropertiesWindow *self =
        NAUTILUS_PROPERTIES_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog)));

    g_clear_pointer (&self->change_permission_combo_rows, g_list_free);
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
on_change_permissions_response_change (AdwWindow *dialog,
                                       gpointer   user_data)
{
    NautilusPropertiesWindow *self =
        NAUTILUS_PROPERTIES_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog)));

    guint32 file_permission = 0, file_permission_mask = 0;
    guint32 dir_permission = 0, dir_permission_mask = 0;
    guint32 vfs_mask, vfs_new_perm;
    FilterType filter_type;
    GList *l;
    PermissionType type;
    int mask;

    /* Simple mode, minus exec checkbox */
    for (l = self->change_permission_combo_rows; l != NULL; l = l->next)
    {
        AdwComboRow *combo_row = l->data;
        NautilusPermissionEntry *selected = adw_combo_row_get_selected_item (combo_row);

        if (selected == NULL)
        {
            continue;
        }

        type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo_row), "permission-type"));
        filter_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo_row), "filter-type"));

        vfs_new_perm = permission_to_vfs (type, selected->permission_value);

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

    for (l = self->files; l != NULL; l = l->next)
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
    g_clear_pointer (&self->change_permission_combo_rows, g_list_free);
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
set_active_from_umask (AdwComboRow    *combo_row,
                       PermissionType  type,
                       FilterType      filter_type)
{
    GListModel *model = adw_combo_row_get_model (combo_row);
    mode_t initial;
    mode_t mask;
    mode_t p;
    PermissionValue perm;

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
            perm = PERMISSION_READ | PERMISSION_EXEC | PERMISSION_WRITE;
        }
        else if ((p & (S_IRUSR | S_IWUSR)) == (S_IRUSR | S_IWUSR))
        {
            perm = PERMISSION_READ | PERMISSION_WRITE;
        }
        else if ((p & (S_IRUSR | S_IXUSR)) == (S_IRUSR | S_IXUSR))
        {
            perm = PERMISSION_READ | PERMISSION_EXEC;
        }
        else if ((p & S_IRUSR) == S_IRUSR)
        {
            perm = PERMISSION_READ;
        }
        else
        {
            perm = PERMISSION_NONE;
        }
    }
    else if (type == PERMISSION_GROUP)
    {
        p &= ~(S_IRWXU | S_IRWXO);
        if ((p & S_IRWXG) == S_IRWXG)
        {
            perm = PERMISSION_READ | PERMISSION_EXEC | PERMISSION_WRITE;
        }
        else if ((p & (S_IRGRP | S_IWGRP)) == (S_IRGRP | S_IWGRP))
        {
            perm = PERMISSION_READ | PERMISSION_WRITE;
        }
        else if ((p & (S_IRGRP | S_IXGRP)) == (S_IRGRP | S_IXGRP))
        {
            perm = PERMISSION_READ | PERMISSION_EXEC;
        }
        else if ((p & S_IRGRP) == S_IRGRP)
        {
            perm = PERMISSION_READ;
        }
        else
        {
            perm = PERMISSION_NONE;
        }
    }
    else
    {
        p &= ~(S_IRWXU | S_IRWXG);
        if ((p & S_IRWXO) == S_IRWXO)
        {
            perm = PERMISSION_READ | PERMISSION_EXEC | PERMISSION_WRITE;
        }
        else if ((p & (S_IROTH | S_IWOTH)) == (S_IROTH | S_IWOTH))
        {
            perm = PERMISSION_READ | PERMISSION_WRITE;
        }
        else if ((p & (S_IROTH | S_IXOTH)) == (S_IROTH | S_IXOTH))
        {
            perm = PERMISSION_READ | PERMISSION_EXEC;
        }
        else if ((p & S_IROTH) == S_IROTH)
        {
            perm = PERMISSION_READ;
        }
        else
        {
            perm = PERMISSION_NONE;
        }
    }

    for (guint i = 0; i < g_list_model_get_n_items (model); i++)
    {
        g_autoptr (NautilusPermissionEntry) entry = g_list_model_get_item (model, i);

        if (entry->permission_value == perm)
        {
            adw_combo_row_set_selected (combo_row, i);
            break;
        }
    }
}

static gboolean
on_change_permissions_close (GtkWidget *widget,
                             GVariant  *args,
                             gpointer   user_data)
{
    on_change_permissions_response_cancel (ADW_WINDOW (user_data), widget);

    return TRUE;
}

static void
on_change_permissions_clicked (NautilusPropertiesWindow *self)
{
    GtkWidget *dialog;
    AdwComboRow *combo_row;
    GtkButton *cancel_button, *change_button;
    GtkShortcut *esc_shortcut;
    GtkShortcutAction *cb_action;
    g_autoptr (GtkBuilder) change_permissions_builder = NULL;

    change_permissions_builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-file-properties-change-permissions.ui");

    dialog = GTK_WIDGET (gtk_builder_get_object (change_permissions_builder, "change_permissions_dialog"));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (self));

    /* Owner Permissions */
    combo_row = ADW_COMBO_ROW (gtk_builder_get_object (change_permissions_builder, "file_owner_combo_row"));
    setup_permissions_combo_row (combo_row, PERMISSION_USER, FILES_ONLY);
    self->change_permission_combo_rows = g_list_prepend (self->change_permission_combo_rows,
                                                         combo_row);
    set_active_from_umask (combo_row, PERMISSION_USER, FILES_ONLY);

    combo_row = ADW_COMBO_ROW (gtk_builder_get_object (change_permissions_builder, "folder_owner_combo_row"));
    setup_permissions_combo_row (combo_row, PERMISSION_USER, FOLDERS_ONLY);
    self->change_permission_combo_rows = g_list_prepend (self->change_permission_combo_rows,
                                                         combo_row);
    set_active_from_umask (combo_row, PERMISSION_USER, FOLDERS_ONLY);

    /* Group Permissions */
    combo_row = ADW_COMBO_ROW (gtk_builder_get_object (change_permissions_builder, "file_group_combo_row"));
    setup_permissions_combo_row (combo_row, PERMISSION_GROUP, FILES_ONLY);
    self->change_permission_combo_rows = g_list_prepend (self->change_permission_combo_rows,
                                                         combo_row);
    set_active_from_umask (combo_row, PERMISSION_GROUP, FILES_ONLY);

    combo_row = ADW_COMBO_ROW (gtk_builder_get_object (change_permissions_builder, "folder_group_combo_row"));
    setup_permissions_combo_row (combo_row, PERMISSION_GROUP, FOLDERS_ONLY);
    self->change_permission_combo_rows = g_list_prepend (self->change_permission_combo_rows,
                                                         combo_row);
    set_active_from_umask (combo_row, PERMISSION_GROUP, FOLDERS_ONLY);

    /* Others Permissions */
    combo_row = ADW_COMBO_ROW (gtk_builder_get_object (change_permissions_builder, "file_other_combo_row"));
    setup_permissions_combo_row (combo_row, PERMISSION_OTHER, FILES_ONLY);
    self->change_permission_combo_rows = g_list_prepend (self->change_permission_combo_rows,
                                                         combo_row);
    set_active_from_umask (combo_row, PERMISSION_OTHER, FILES_ONLY);

    combo_row = ADW_COMBO_ROW (gtk_builder_get_object (change_permissions_builder, "folder_other_combo_row"));
    setup_permissions_combo_row (combo_row, PERMISSION_OTHER, FOLDERS_ONLY);
    self->change_permission_combo_rows = g_list_prepend (self->change_permission_combo_rows,
                                                         combo_row);
    set_active_from_umask (combo_row, PERMISSION_OTHER, FOLDERS_ONLY);

    cancel_button = GTK_BUTTON (gtk_builder_get_object (change_permissions_builder, "cancel_button"));
    change_button = GTK_BUTTON (gtk_builder_get_object (change_permissions_builder, "change_button"));

    g_signal_connect_swapped (cancel_button, "clicked",
                              G_CALLBACK (on_change_permissions_response_cancel),
                              dialog);
    g_signal_connect_swapped (change_button, "clicked",
                              G_CALLBACK (on_change_permissions_response_change),
                              dialog);

    esc_shortcut = GTK_SHORTCUT (gtk_builder_get_object (change_permissions_builder, "esc_shortcut"));
    cb_action = gtk_callback_action_new ((GtkShortcutFunc) on_change_permissions_close,
                                         dialog, NULL);
    gtk_shortcut_set_action (esc_shortcut, cb_action);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
setup_permissions_page (NautilusPropertiesWindow *self)
{
    self->initial_permissions = NULL;

    if (all_can_get_permissions (self->files))
    {
        self->initial_permissions = get_initial_permissions (self->files);
        self->has_recursive_apply = files_has_changable_permissions_directory (self);

        if (!all_can_set_permissions (self->files))
        {
            adw_banner_set_revealed (self->owner_permission_banner, TRUE);
        }

        gtk_stack_set_visible_child_name (GTK_STACK (self->permissions_stack), "permissions-box");
        create_simple_permissions (self);

#ifdef HAVE_SELINUX
        gtk_widget_set_visible (self->security_context_group, TRUE);
        add_updatable_row (self, self->security_context_row, "selinux_context");
#endif

        if (self->has_recursive_apply)
        {
            gtk_widget_set_visible (self->change_permissions_enclosed_files_group, TRUE);
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
            g_autofree gchar *prompt_text = NULL;

            prompt_text = g_strdup_printf (_("The permissions of “%s” could not be determined."),
                                           nautilus_file_get_display_name (get_file (self)));
            adw_status_page_set_description (ADW_STATUS_PAGE (self->unknown_permissions_page), prompt_text);
        }

        gtk_stack_set_visible_child_name (GTK_STACK (self->permissions_stack), "permission-indeterminable");
    }
}

static void
refresh_extension_model_pages (NautilusPropertiesWindow *self)
{
    g_autoptr (GListStore) extensions_list = g_list_store_new (NAUTILUS_TYPE_PROPERTIES_MODEL);
    g_autolist (NautilusPropertiesModel) all_models = NULL;
    g_autolist (GObject) providers =
        nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_PROPERTIES_MODEL_PROVIDER);

    for (GList *l = providers; l != NULL; l = l->next)
    {
        GList *models = nautilus_properties_model_provider_get_models (l->data, self->files);

        all_models = g_list_concat (all_models, models);
    }

    for (GList *l = all_models; l != NULL; l = l->next)
    {
        g_list_store_append (extensions_list, NAUTILUS_PROPERTIES_MODEL (l->data));
    }

    gtk_widget_set_visible (self->extension_models_group,
                            g_list_model_get_n_items (G_LIST_MODEL (extensions_list)) > 0);

    gtk_list_box_bind_model (GTK_LIST_BOX (self->extension_models_list_box),
                             G_LIST_MODEL (extensions_list),
                             (GtkListBoxCreateWidgetFunc) add_extension_model_page,
                             self,
                             NULL);
    g_set_object (&self->extensions_list, extensions_list);
}

static gboolean
should_show_permissions (NautilusPropertiesWindow *self)
{
    GList *l;

    /* Don't show permissions for Trash and Recent since they're not
     * really file system objects.
     */
    for (l = self->files; l != NULL; l = l->next)
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

    return g_string_free (key, FALSE);
}

static StartupData *
startup_data_new (GList                            *files,
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
    data->files = nautilus_file_list_copy (files);
    data->parent_widget = parent_widget;
    data->parent_window = parent_window;
    data->startup_id = g_strdup (startup_id);
    data->pending_key = g_strdup (pending_key);
    data->pending_files = g_hash_table_new (g_direct_hash,
                                            g_direct_equal);
    data->callback = callback;
    data->callback_data = callback_data;
    data->window = window;

    for (l = data->files; l != NULL; l = l->next)
    {
        g_hash_table_insert (data->pending_files, l->data, l->data);
    }

    return data;
}

static void
startup_data_free (StartupData *data)
{
    nautilus_file_list_free (data->files);
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

static NautilusPropertiesWindow *
create_properties_window (StartupData *startup_data)
{
    NautilusPropertiesWindow *window;
    GList *l;

    window = NAUTILUS_PROPERTIES_WINDOW (g_object_new (NAUTILUS_TYPE_PROPERTIES_WINDOW,
                                                       NULL));

    window->files = nautilus_file_list_copy (startup_data->files);

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

    for (l = window->files; l != NULL; l = l->next)
    {
        NautilusFile *file;
        NautilusFileAttributes attributes;

        file = NAUTILUS_FILE (l->data);

        attributes = NAUTILUS_FILE_ATTRIBUTES_FOR_ICON;
        if (nautilus_file_is_directory (file))
        {
            attributes |= NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS;
        }

        nautilus_file_monitor_add (file, &window->files, attributes);
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
        gtk_widget_set_visible (window->permissions_navigation_row, TRUE);
    }

    if (should_show_exectution_switch (window))
    {
        gtk_widget_set_visible (GTK_WIDGET (window->execution_row), TRUE);
    }

    /* Update from initial state */
    properties_window_update (window, NULL);

    return window;
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

        startup_data->window = new_window;

        remove_pending (startup_data, FALSE, TRUE);

        gtk_window_present (GTK_WINDOW (new_window));
        g_signal_connect (GTK_WIDGET (new_window), "destroy",
                          G_CALLBACK (widget_on_destroy), startup_data);

        /* We wish the label to be selectable, but not selected by default. */
        gtk_label_select_region (GTK_LABEL (new_window->name_value_label), -1, -1);
    }
}

void
nautilus_properties_window_present (GList                            *files,
                                    GtkWidget                        *parent_widget,
                                    const gchar                      *startup_id,
                                    NautilusPropertiesWindowCallback  callback,
                                    gpointer                          callback_data)
{
    GList *l, *next;
    GtkWindow *parent_window;
    StartupData *startup_data;
    g_autofree char *pending_key = NULL;

    g_return_if_fail (files != NULL);
    g_return_if_fail (parent_widget == NULL || GTK_IS_WIDGET (parent_widget));

    if (pending_lists == NULL)
    {
        pending_lists = g_hash_table_new (g_str_hash, g_str_equal);
    }

    pending_key = get_pending_key (files);

    /* Look to see if we're already waiting for a window for this file. */
    if (g_hash_table_lookup (pending_lists, pending_key) != NULL)
    {
        /* FIXME: No callback is done if this happen. In practice, it's a quite
         * corner case
         */
        return;
    }

    if (parent_widget)
    {
        parent_window = GTK_WINDOW (gtk_widget_get_ancestor (parent_widget, GTK_TYPE_WINDOW));
    }
    else
    {
        parent_window = NULL;
    }

    startup_data = startup_data_new (files,
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

    for (l = startup_data->files; l != NULL; l = next)
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

    unschedule_or_cancel_group_change (self);
    unschedule_or_cancel_owner_change (self);

    g_clear_pointer (&self->custom_icon_for_undo, g_free);

    g_list_foreach (self->files,
                    (GFunc) nautilus_file_monitor_remove,
                    &self->files);
    g_clear_list (&self->files, (GDestroyNotify) nautilus_file_unref);

    g_clear_list (&self->changed_files, (GDestroyNotify) nautilus_file_unref);

    g_clear_handle_id (&self->deep_count_spinner_timeout_id, g_source_remove);

    while (self->deep_count_files)
    {
        stop_deep_count_for_file (self, self->deep_count_files->data);
    }

    g_clear_list (&self->permission_rows, NULL);

    g_clear_list (&self->change_permission_combo_rows, NULL);

    g_clear_pointer (&self->initial_permissions, g_hash_table_destroy);

    g_clear_list (&self->value_labels, NULL);
    g_clear_list (&self->value_rows, NULL);

    g_clear_handle_id (&self->update_directory_contents_timeout_id, g_source_remove);
    g_clear_handle_id (&self->update_files_timeout_id, g_source_remove);

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_PROPERTIES_WINDOW);

    G_OBJECT_CLASS (nautilus_properties_window_parent_class)->dispose (object);
}

static void
real_finalize (GObject *object)
{
    NautilusPropertiesWindow *self;

    self = NAUTILUS_PROPERTIES_WINDOW (object);

    g_free (self->mime_type);
    g_free (self->device_identifier);
    g_clear_object (&self->extensions_list);

    G_OBJECT_CLASS (nautilus_properties_window_parent_class)->finalize (object);
}

/* icon selection callback to set the image of the file object to the selected file */
static void
set_icon (const char               *icon_uri,
          NautilusPropertiesWindow *self)
{
    g_autofree gchar *icon_path = NULL;

    g_assert (icon_uri != NULL);
    g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (self));

    icon_path = g_filename_from_uri (icon_uri, NULL, NULL);
    /* we don't allow remote URIs */
    if (icon_path != NULL)
    {
        NautilusFile *file = get_file (self);

        if (file != NULL && !nautilus_file_is_gone (file))
        {
            g_autoptr (GFile) file_location = nautilus_file_get_location (file);
            g_autoptr (GFile) icon_location = g_file_new_for_uri (icon_uri);
            /* ’Tis a little bit of a misnomer. Actually a path. */
            g_autofree gchar *real_icon_uri = g_file_get_relative_path (file_location,
                                                                        icon_location);

            if (real_icon_uri == NULL)
            {
                real_icon_uri = g_strdup (icon_uri);
            }

            nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, real_icon_uri);
        }
    }
}

static void
custom_icon_file_chooser_response_cb (GtkFileDialog            *dialog,
                                      GAsyncResult             *result,
                                      NautilusPropertiesWindow *self)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) location = gtk_file_dialog_open_finish (dialog, result, &error);

    if (location != NULL)
    {
        g_autofree gchar *uri = g_file_get_uri (location);
        set_icon (uri, self);
    }
    else if (error != NULL &&
             !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
    {
        g_warning ("Error while choosing an icon: %s", error->message);
    }
}

static void
select_image_button_callback (GtkWidget                *widget,
                              NautilusPropertiesWindow *self)
{
    g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();
    g_autoptr (GtkFileFilter) filter = gtk_file_filter_new ();
    NautilusFile *file = get_file (self);

    gtk_file_dialog_set_title (dialog, _("Select Custom Icon"));
    gtk_file_dialog_set_accept_label (dialog, _("_Select"));

    gtk_file_filter_add_pixbuf_formats (filter);
    gtk_file_dialog_set_default_filter (dialog, filter);

    /* it's likely that the user wants to pick an icon that is inside a local directory */
    if (nautilus_file_is_directory (file))
    {
        g_autoptr (GFile) image_location = NULL;

        image_location = nautilus_file_get_location (file);

        if (image_location != NULL)
        {
            gtk_file_dialog_set_initial_folder (dialog, image_location);
        }
    }

    gtk_file_dialog_open (dialog,
                          GTK_WINDOW (self),
                          NULL,
                          (GAsyncReadyCallback) custom_icon_file_chooser_response_cb,
                          self);
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

    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, toast_overlay);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, nav_view);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_image);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, icon_overlay);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, select_icon_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, reset_icon_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, star_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, name_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, type_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, type_file_system_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, size_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, contents_spinner);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, disk_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, volume_usage_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, disk_space_level_bar);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, disk_space_used_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, disk_space_free_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, disk_space_capacity_value);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, open_in_disks_button_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, locations_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, link_target_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, parent_folder_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, trashed_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, trashed_on_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, original_folder_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, times_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, modified_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, created_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, accessed_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permissions_navigation_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permissions_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, extension_models_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, extension_models_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, free_space_value_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, permissions_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, owner_permission_banner);
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
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, execution_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, security_context_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, security_context_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, change_permissions_enclosed_files_group);
    gtk_widget_class_bind_template_child (widget_class, NautilusPropertiesWindow, extension_list_box);

    gtk_widget_class_bind_template_callback (widget_class, star_clicked);
    gtk_widget_class_bind_template_callback (widget_class, open_in_disks);
    gtk_widget_class_bind_template_callback (widget_class, open_parent_folder);
    gtk_widget_class_bind_template_callback (widget_class, open_link_target);
    gtk_widget_class_bind_template_callback (widget_class, navigate_permissions_page);
    gtk_widget_class_bind_template_callback (widget_class, reset_icon);
    gtk_widget_class_bind_template_callback (widget_class, select_image_button_callback);
    gtk_widget_class_bind_template_callback (widget_class, on_change_permissions_clicked);
}

static void
nautilus_properties_window_init (NautilusPropertiesWindow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
