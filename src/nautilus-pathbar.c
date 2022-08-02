/* nautilus-pathbar.c
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "nautilus-pathbar.h"
#include "nautilus-properties-window.h"

#include "nautilus-enums.h"
#include "nautilus-enum-types.h"
#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-names.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"

#include "nautilus-window-slot-dnd.h"

enum
{
    OPEN_LOCATION,
    LAST_SIGNAL
};

typedef enum
{
    NORMAL_BUTTON,
    OTHER_LOCATIONS_BUTTON,
    ROOT_BUTTON,
    ADMIN_ROOT_BUTTON,
    HOME_BUTTON,
    STARRED_BUTTON,
    RECENT_BUTTON,
    MOUNT_BUTTON,
    TRASH_BUTTON,
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *) (x))

static guint path_bar_signals[LAST_SIGNAL] = { 0 };

#define NAUTILUS_PATH_BAR_BUTTON_ELLISPIZE_MINIMUM_CHARS 7

typedef struct
{
    GtkWidget *button;
    ButtonType type;
    char *dir_name;
    GFile *path;
    NautilusFile *file;
    unsigned int file_changed_signal_id;

    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *separator;
    GtkWidget *container;

    NautilusPathBar *path_bar;

    guint ignore_changes : 1;
    guint is_root : 1;
} ButtonData;

struct _NautilusPathBar
{
    GtkBox parent_instance;

    GtkWidget *scrolled;
    GtkWidget *buttons_box;

    GFile *current_path;
    gpointer current_button_data;

    GList *button_list;

    GActionGroup *action_group;

    NautilusFile *context_menu_file;
    GtkPopoverMenu *current_view_menu_popover;
    GtkWidget *current_view_menu_button;
    GtkWidget *button_menu_popover;
    GMenu *current_view_menu;
    GMenu *extensions_section;
    GMenu *templates_submenu;
    GMenu *button_menu;

    gchar *os_name;
};

G_DEFINE_TYPE (NautilusPathBar, nautilus_path_bar, GTK_TYPE_BOX);

static void nautilus_path_bar_update_button_state (ButtonData *button_data,
                                                   gboolean    current_dir);
static void nautilus_path_bar_update_path (NautilusPathBar *self,
                                           GFile           *file_path);

static void     unschedule_pop_up_context_menu (NautilusPathBar *self);
static void     action_pathbar_open_item_new_window (GSimpleAction *action,
                                                     GVariant      *state,
                                                     gpointer       user_data);
static void     action_pathbar_open_item_new_tab (GSimpleAction *action,
                                                  GVariant      *state,
                                                  gpointer       user_data);
static void     action_pathbar_properties (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
static void     pop_up_pathbar_context_menu (NautilusPathBar *self,
                                             NautilusFile    *file);
static void nautilus_path_bar_clear_buttons (NautilusPathBar *self);

const GActionEntry path_bar_actions[] =
{
    { "open-item-new-tab", action_pathbar_open_item_new_tab },
    { "open-item-new-window", action_pathbar_open_item_new_window },
    { "properties", action_pathbar_properties}
};


static void
action_pathbar_open_item_new_tab (GSimpleAction *action,
                                  GVariant      *state,
                                  gpointer       user_data)
{
    NautilusPathBar *self;
    GFile *location;

    self = NAUTILUS_PATH_BAR (user_data);

    if (self->context_menu_file == NULL)
    {
        return;
    }

    location = nautilus_file_get_location (self->context_menu_file);

    if (location)
    {
        g_signal_emit (user_data, path_bar_signals[OPEN_LOCATION], 0, location,
                       NAUTILUS_OPEN_FLAG_NEW_TAB | NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE);
        g_object_unref (location);
    }
}

static void
action_pathbar_open_item_new_window (GSimpleAction *action,
                                     GVariant      *state,
                                     gpointer       user_data)
{
    NautilusPathBar *self;
    GFile *location;

    self = NAUTILUS_PATH_BAR (user_data);

    if (self->context_menu_file == NULL)
    {
        return;
    }

    location = nautilus_file_get_location (self->context_menu_file);

    if (location)
    {
        g_signal_emit (user_data, path_bar_signals[OPEN_LOCATION], 0, location, NAUTILUS_OPEN_FLAG_NEW_WINDOW);
        g_object_unref (location);
    }
}

static void
action_pathbar_properties (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    NautilusPathBar *self;
    GList *files;

    self = NAUTILUS_PATH_BAR (user_data);

    g_return_if_fail (NAUTILUS_IS_FILE (self->context_menu_file));

    files = g_list_append (NULL, nautilus_file_ref (self->context_menu_file));

    nautilus_properties_window_present (files, GTK_WIDGET (self), NULL, NULL,
                                        NULL);

    nautilus_file_list_free (files);
}

static void
on_adjustment_changed (GtkAdjustment   *adjustment,
                       NautilusPathBar *self)
{
    /* Automatically scroll to the end, to reveal the current folder. */
    g_autoptr (AdwAnimation) anim = NULL;
    anim = adw_timed_animation_new (GTK_WIDGET (self),
                                    gtk_adjustment_get_value (adjustment),
                                    gtk_adjustment_get_upper (adjustment),
                                    800,
                                    adw_property_animation_target_new (G_OBJECT (adjustment), "value"));
    adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (anim), ADW_EASE_OUT_CUBIC);
    adw_animation_play (anim);
}

static void
on_page_size_changed (GtkAdjustment *adjustment)
{
    /* When window is resized, immediately set new value, otherwise we would get
     * an underflow gradient for an moment. */
    gtk_adjustment_set_value (adjustment, gtk_adjustment_get_upper (adjustment));
}

static gboolean
bind_current_view_menu_model_to_popover (NautilusPathBar *self)
{
    gtk_popover_menu_set_menu_model (self->current_view_menu_popover,
                                     G_MENU_MODEL (self->current_view_menu));
    return G_SOURCE_REMOVE;
}

static void
nautilus_path_bar_init (NautilusPathBar *self)
{
    GtkAdjustment *adjustment;
    GtkBuilder *builder;
    g_autoptr (GError) error = NULL;

    self->os_name = g_get_os_info (G_OS_INFO_KEY_NAME);

    self->scrolled = gtk_scrolled_window_new ();
    /* Scroll horizontally only and don't use internal scrollbar. */
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled),
                                    /* hscrollbar-policy */ GTK_POLICY_EXTERNAL,
                                    /* vscrollbar-policy */ GTK_POLICY_NEVER);
    gtk_widget_set_hexpand (self->scrolled, TRUE);
    gtk_box_append (GTK_BOX (self), self->scrolled);

    adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (self->scrolled));
    g_signal_connect (adjustment, "changed", G_CALLBACK (on_adjustment_changed), self);
    g_signal_connect (adjustment, "notify::page-size", G_CALLBACK (on_page_size_changed), self);

    self->buttons_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled), self->buttons_box);

    self->current_view_menu_button = gtk_menu_button_new ();
    gtk_widget_add_css_class (self->current_view_menu_button, "flat");
    gtk_menu_button_set_child (GTK_MENU_BUTTON (self->current_view_menu_button),
                               gtk_image_new_from_icon_name ("view-more-symbolic"));
    gtk_box_append (GTK_BOX (self), self->current_view_menu_button);

    builder = gtk_builder_new ();

    /* Add context menu for pathbar buttons */
    gtk_builder_add_from_resource (builder,
                                   "/org/gnome/nautilus/ui/nautilus-pathbar-context-menu.ui",
                                   &error);
    if (error != NULL)
    {
        g_error ("Failed to add pathbar-context-menu.ui: %s", error->message);
    }
    self->button_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "button-menu")));
    self->button_menu_popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (self->button_menu));
    gtk_widget_set_parent (self->button_menu_popover, GTK_WIDGET (self));
    gtk_popover_set_has_arrow (GTK_POPOVER (self->button_menu_popover), FALSE);
    gtk_widget_set_halign (self->button_menu_popover, GTK_ALIGN_START);

    /* Add current location menu, which shares features with the view's background context menu */
    self->current_view_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "current-view-menu")));
    self->extensions_section = g_object_ref (G_MENU (gtk_builder_get_object (builder, "background-extensions-section")));
    self->templates_submenu = g_object_ref (G_MENU (gtk_builder_get_object (builder, "templates-submenu")));
    self->current_view_menu_popover = g_object_ref_sink (GTK_POPOVER_MENU (gtk_popover_menu_new_from_model (NULL)));

    g_object_unref (builder);

    gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->current_view_menu_button),
                                 GTK_WIDGET (self->current_view_menu_popover));
    bind_current_view_menu_model_to_popover (self);

    gtk_widget_set_name (GTK_WIDGET (self), "NautilusPathBar");
    gtk_widget_add_css_class (GTK_WIDGET (self), "linked");

    /* Action group */
    self->action_group = G_ACTION_GROUP (g_simple_action_group_new ());
    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     path_bar_actions,
                                     G_N_ELEMENTS (path_bar_actions),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "pathbar",
                                    G_ACTION_GROUP (self->action_group));
}

static void
nautilus_path_bar_finalize (GObject *object)
{
    NautilusPathBar *self;

    self = NAUTILUS_PATH_BAR (object);

    g_clear_object (&self->current_view_menu);
    g_clear_object (&self->extensions_section);
    g_clear_object (&self->templates_submenu);
    g_clear_object (&self->button_menu);
    g_clear_pointer (&self->button_menu_popover, gtk_widget_unparent);
    g_clear_object (&self->current_view_menu_popover);
    g_free (self->os_name);

    unschedule_pop_up_context_menu (NAUTILUS_PATH_BAR (object));

    G_OBJECT_CLASS (nautilus_path_bar_parent_class)->finalize (object);
}

static void
nautilus_path_bar_dispose (GObject *object)
{
    NautilusPathBar *self = NAUTILUS_PATH_BAR (object);

    nautilus_path_bar_clear_buttons (self);

    G_OBJECT_CLASS (nautilus_path_bar_parent_class)->dispose (object);
}

static const char *
get_dir_name (ButtonData *button_data)
{
    switch (button_data->type)
    {
        case ROOT_BUTTON:
        {
            if (button_data->path_bar != NULL &&
                button_data->path_bar->os_name != NULL)
            {
                return button_data->path_bar->os_name;
            }
            /* Translators: This is the label used in the pathbar when seeing
             * the root directory (also known as /) */
            return _("Operating System");
        }

        case ADMIN_ROOT_BUTTON:
        {
            /* Translators: This is the filesystem root directory (also known
             * as /) when seen as administrator */
            return _("Administrator Root");
        }

        case HOME_BUTTON:
        {
            return _("Home");
        }

        case OTHER_LOCATIONS_BUTTON:
        {
            return _("Other Locations");
        }

        case STARRED_BUTTON:
        {
            return _("Starred");
        }

        default:
        {
            return button_data->dir_name;
        }
    }
}

static void
button_data_free (ButtonData *button_data)
{
    g_object_unref (button_data->path);
    g_free (button_data->dir_name);
    if (button_data->file != NULL)
    {
        g_signal_handler_disconnect (button_data->file,
                                     button_data->file_changed_signal_id);
        nautilus_file_monitor_remove (button_data->file, button_data);
        nautilus_file_unref (button_data->file);
    }

    g_free (button_data);
}

static void
nautilus_path_bar_class_init (NautilusPathBarClass *path_bar_class)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass *) path_bar_class;

    gobject_class->finalize = nautilus_path_bar_finalize;
    gobject_class->dispose = nautilus_path_bar_dispose;

    path_bar_signals [OPEN_LOCATION] =
        g_signal_new ("open-location",
                      G_OBJECT_CLASS_TYPE (path_bar_class),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 2,
                      G_TYPE_FILE,
                      NAUTILUS_TYPE_OPEN_FLAGS);
}

void
nautilus_path_bar_set_extensions_background_menu (NautilusPathBar *self,
                                                  GMenuModel      *menu)
{
    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));

    nautilus_gmenu_set_from_model (self->extensions_section, menu);
}

void
nautilus_path_bar_set_templates_menu (NautilusPathBar *self,
                                      GMenuModel      *menu)
{
    gint i;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));

    if (!gtk_widget_is_visible (GTK_WIDGET (self->current_view_menu_popover)))
    {
        /* Workaround to avoid leaking duplicated GtkStack pages each time the
         * templates menu is set. Unbinding the model is the only way to clear
         * all children. After that's done, on idle, we rebind it.
         * See https://gitlab.gnome.org/GNOME/nautilus/-/issues/1705 */
        gtk_popover_menu_set_menu_model (self->current_view_menu_popover, NULL);
    }

    nautilus_gmenu_set_from_model (self->templates_submenu, menu);
    g_idle_add ((GSourceFunc) bind_current_view_menu_model_to_popover, self);

    i = nautilus_g_menu_model_find_by_string (G_MENU_MODEL (self->current_view_menu),
                                              "nautilus-menu-item",
                                              "templates-submenu");
    nautilus_g_menu_replace_string_in_item (self->current_view_menu, i,
                                            "hidden-when",
                                            (menu == NULL) ? "action-missing" : NULL);
}

/* Public functions and their helpers */
static void
nautilus_path_bar_clear_buttons (NautilusPathBar *self)
{
    while (self->button_list != NULL)
    {
        ButtonData *button_data;

        button_data = BUTTON_DATA (self->button_list->data);

        gtk_box_remove (GTK_BOX (self->buttons_box), button_data->container);

        self->button_list = g_list_remove (self->button_list, button_data);
        button_data_free (button_data);
    }
}

void
nautilus_path_bar_show_current_location_menu (NautilusPathBar *self)
{
    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));

    gtk_menu_button_popup (GTK_MENU_BUTTON (self->current_view_menu_button));
}

static void
button_clicked_cb (GtkButton *button,
                   gpointer   data)
{
    ButtonData *button_data;
    NautilusPathBar *self;

    button_data = BUTTON_DATA (data);
    if (button_data->ignore_changes)
    {
        return;
    }

    self = button_data->path_bar;

    if (g_file_equal (button_data->path, self->current_path))
    {
        nautilus_path_bar_show_current_location_menu (self);
    }
    else
    {
        g_signal_emit (self, path_bar_signals[OPEN_LOCATION], 0,
                       button_data->path,
                       0);
    }
}

static void
real_pop_up_pathbar_context_menu (NautilusPathBar *self)
{
    gtk_popover_popup (GTK_POPOVER (self->button_menu_popover));
}

static void
pathbar_popup_file_attributes_ready (NautilusFile *file,
                                     gpointer      data)
{
    NautilusPathBar *self;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (data));

    self = NAUTILUS_PATH_BAR (data);

    g_return_if_fail (file == self->context_menu_file);

    real_pop_up_pathbar_context_menu (self);
}

static void
unschedule_pop_up_context_menu (NautilusPathBar *self)
{
    if (self->context_menu_file != NULL)
    {
        g_return_if_fail (NAUTILUS_IS_FILE (self->context_menu_file));
        nautilus_file_cancel_call_when_ready (self->context_menu_file,
                                              pathbar_popup_file_attributes_ready,
                                              self);
        g_clear_pointer (&self->context_menu_file, nautilus_file_unref);
    }
}

static void
schedule_pop_up_context_menu (NautilusPathBar *self,
                              NautilusFile    *file)
{
    g_return_if_fail (NAUTILUS_IS_FILE (file));

    if (file == self->context_menu_file)
    {
        if (nautilus_file_check_if_ready (file,
                                          NAUTILUS_FILE_ATTRIBUTE_INFO |
                                          NAUTILUS_FILE_ATTRIBUTE_MOUNT |
                                          NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO))
        {
            real_pop_up_pathbar_context_menu (self);
        }
    }
    else
    {
        unschedule_pop_up_context_menu (self);

        self->context_menu_file = nautilus_file_ref (file);
        nautilus_file_call_when_ready (self->context_menu_file,
                                       NAUTILUS_FILE_ATTRIBUTE_INFO |
                                       NAUTILUS_FILE_ATTRIBUTE_MOUNT |
                                       NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO,
                                       pathbar_popup_file_attributes_ready,
                                       self);
    }
}

static void
pop_up_pathbar_context_menu (NautilusPathBar *self,
                             NautilusFile    *file)
{
    if (file != NULL)
    {
        schedule_pop_up_context_menu (self, file);
    }
}


static void
on_click_gesture_pressed (GtkGestureClick *gesture,
                          gint             n_press,
                          gdouble          x,
                          gdouble          y,
                          gpointer         user_data)
{
    ButtonData *button_data;
    NautilusPathBar *self;
    guint current_button;
    GdkModifierType state;
    double x_in_pathbar, y_in_pathbar;

    if (n_press != 1)
    {
        return;
    }

    button_data = BUTTON_DATA (user_data);
    self = button_data->path_bar;
    current_button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

    gtk_widget_translate_coordinates (GTK_WIDGET (button_data->button),
                                      GTK_WIDGET (self),
                                      x, y,
                                      &x_in_pathbar, &y_in_pathbar);

    switch (current_button)
    {
        case GDK_BUTTON_MIDDLE:
        {
            if ((state & gtk_accelerator_get_default_mod_mask ()) == 0)
            {
                g_signal_emit (self, path_bar_signals[OPEN_LOCATION], 0,
                               button_data->path,
                               NAUTILUS_OPEN_FLAG_NEW_TAB | NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE);
            }
        }
        break;

        case GDK_BUTTON_SECONDARY:
        {
            if (g_file_equal (button_data->path, self->current_path))
            {
                nautilus_path_bar_show_current_location_menu (self);
            }
            else
            {
                gtk_popover_set_pointing_to (GTK_POPOVER (self->button_menu_popover),
                                             &(GdkRectangle){x_in_pathbar, y_in_pathbar, 0, 0});
                pop_up_pathbar_context_menu (self, button_data->file);
            }
        }
        break;

        case GDK_BUTTON_PRIMARY:
        {
            if ((state & GDK_CONTROL_MASK) != 0)
            {
                g_signal_emit (button_data->path_bar, path_bar_signals[OPEN_LOCATION], 0,
                               button_data->path,
                               NAUTILUS_OPEN_FLAG_NEW_WINDOW);
            }
            else
            {
                /* GtkButton will claim the primary button presses and emit the
                 * "clicked" signal. Handle it in the singal callback, not here.
                 */
                return;
            }
        }
        break;

        default:
        {
            /* Ignore other buttons in this gesture. */
            return;
        }
        break;
    }

    /* Both middle- and secondary-clicking the title bar can have interesting
     * effects (minimizing the window, popping up a window manager menu, etc.),
     * and this avoids all that.
     */
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static GIcon *
get_gicon_for_mount (ButtonData *button_data)
{
    GIcon *icon;
    GMount *mount;

    icon = NULL;
    mount = nautilus_get_mounted_mount_for_root (button_data->path);

    if (mount != NULL)
    {
        icon = g_mount_get_symbolic_icon (mount);
        g_object_unref (mount);
    }

    return icon;
}

static GIcon *
get_gicon (ButtonData *button_data)
{
    switch (button_data->type)
    {
        case ROOT_BUTTON:
        case ADMIN_ROOT_BUTTON:
        {
            return g_themed_icon_new (NAUTILUS_ICON_FILESYSTEM);
        }

        case HOME_BUTTON:
        {
            return g_themed_icon_new (NAUTILUS_ICON_HOME);
        }

        case MOUNT_BUTTON:
        {
            return get_gicon_for_mount (button_data);
        }

        case STARRED_BUTTON:
        {
            return g_themed_icon_new ("starred-symbolic");
        }

        case RECENT_BUTTON:
        {
            return g_themed_icon_new ("document-open-recent-symbolic");
        }

        case OTHER_LOCATIONS_BUTTON:
        {
            return g_themed_icon_new ("list-add-symbolic");
        }

        case TRASH_BUTTON:
        {
            return nautilus_trash_monitor_get_symbolic_icon ();
        }

        default:
        {
            return NULL;
        }
    }

    return NULL;
}

static void
nautilus_path_bar_update_button_appearance (ButtonData *button_data,
                                            gboolean    current_dir)
{
    const gchar *dir_name = get_dir_name (button_data);
    gint min_chars = NAUTILUS_PATH_BAR_BUTTON_ELLISPIZE_MINIMUM_CHARS;
    GIcon *icon;

    if (button_data->label != NULL)
    {
        gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);

        if (current_dir)
        {
            /* We want to avoid ellipsizing the current directory name, but
             * still need to set a limit. */
            min_chars = 4 * min_chars;
        }

        /* Labels can ellipsize until they become a single ellipsis character.
         * We don't want that, so we must set a minimum.
         *
         * However, for labels shorter than the minimum, setting this minimum
         * width would make them unnecessarily wide. In that case, just make it
         * not ellipsize instead.
         *
         * Due to variable width fonts, labels can be shorter than the space
         * that would be reserved by setting a minimum amount of characters.
         * Compensate for this with a tolerance of +50% characters.
         */
        if (g_utf8_strlen (dir_name, -1) > min_chars * 1.5)
        {
            gtk_label_set_width_chars (GTK_LABEL (button_data->label), min_chars);
            gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_MIDDLE);
        }
        else
        {
            gtk_label_set_width_chars (GTK_LABEL (button_data->label), -1);
            gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_NONE);
        }
    }

    icon = get_gicon (button_data);
    if (icon != NULL)
    {
        gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), icon);
        gtk_widget_show (GTK_WIDGET (button_data->image));
        g_object_unref (icon);
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (button_data->image));
    }
}

static void
nautilus_path_bar_update_button_state (ButtonData *button_data,
                                       gboolean    current_dir)
{
    if (button_data->label != NULL)
    {
        gtk_label_set_label (GTK_LABEL (button_data->label), NULL);
    }

    nautilus_path_bar_update_button_appearance (button_data, current_dir);
}

static void
setup_button_type (ButtonData      *button_data,
                   NautilusPathBar *self,
                   GFile           *location)
{
    g_autoptr (GMount) mount = NULL;
    g_autofree gchar *uri = NULL;

    if (nautilus_is_root_directory (location))
    {
        button_data->type = ROOT_BUTTON;
    }
    else if (nautilus_is_home_directory (location))
    {
        button_data->type = HOME_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (nautilus_is_recent_directory (location))
    {
        button_data->type = RECENT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (nautilus_is_starred_directory (location))
    {
        button_data->type = STARRED_BUTTON;
        button_data->is_root = TRUE;
    }
    else if ((mount = nautilus_get_mounted_mount_for_root (location)) != NULL)
    {
        button_data->dir_name = g_mount_get_name (mount);
        button_data->type = MOUNT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (nautilus_is_other_locations_directory (location))
    {
        button_data->type = OTHER_LOCATIONS_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (strcmp ((uri = g_file_get_uri (location)), "admin:///") == 0)
    {
        button_data->type = ADMIN_ROOT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (strcmp (uri, "trash:///") == 0)
    {
        button_data->type = TRASH_BUTTON;
        button_data->is_root = TRUE;
    }
    else
    {
        button_data->type = NORMAL_BUTTON;
    }
}

static void
button_data_file_changed (NautilusFile *file,
                          ButtonData   *button_data)
{
    GtkWidget *ancestor;
    GFile *location;
    GFile *current_location;
    GFile *parent;
    GFile *button_parent;
    ButtonData *current_button_data;
    char *display_name;
    NautilusPathBar *self;
    gboolean renamed;
    gboolean child;
    gboolean current_dir;

    ancestor = gtk_widget_get_ancestor (button_data->button, NAUTILUS_TYPE_PATH_BAR);
    if (ancestor == NULL)
    {
        return;
    }
    self = NAUTILUS_PATH_BAR (ancestor);

    g_return_if_fail (self->current_path != NULL);
    g_return_if_fail (self->current_button_data != NULL);

    current_button_data = self->current_button_data;

    location = nautilus_file_get_location (file);
    if (!g_file_equal (button_data->path, location))
    {
        parent = g_file_get_parent (location);
        button_parent = g_file_get_parent (button_data->path);

        renamed = (parent != NULL && button_parent != NULL) &&
                  g_file_equal (parent, button_parent);

        if (parent != NULL)
        {
            g_object_unref (parent);
        }
        if (button_parent != NULL)
        {
            g_object_unref (button_parent);
        }

        if (renamed)
        {
            button_data->path = g_object_ref (location);
        }
        else
        {
            /* the file has been moved.
             * If it was below the currently displayed location, remove it.
             * If it was not below the currently displayed location, update the path bar
             */
            child = g_file_has_prefix (button_data->path,
                                       self->current_path);

            if (child)
            {
                /* moved file inside current path hierarchy */
                g_object_unref (location);
                location = g_file_get_parent (button_data->path);
                current_location = g_object_ref (self->current_path);
            }
            else
            {
                /* moved current path, or file outside current path hierarchy.
                 * Update path bar to new locations.
                 */
                current_location = nautilus_file_get_location (current_button_data->file);
            }

            nautilus_path_bar_update_path (self, location);
            nautilus_path_bar_set_path (self, current_location);
            g_object_unref (location);
            g_object_unref (current_location);
            return;
        }
    }
    else if (nautilus_file_is_gone (file))
    {
        gint idx, position;

        /* if the current or a parent location are gone, clear all the buttons,
         * the view will set the new path.
         */
        current_location = nautilus_file_get_location (current_button_data->file);

        if (g_file_has_prefix (current_location, location) ||
            g_file_equal (current_location, location))
        {
            nautilus_path_bar_clear_buttons (self);
        }
        else if (g_file_has_prefix (location, current_location))
        {
            /* remove this and the following buttons */
            position = g_list_position (self->button_list,
                                        g_list_find (self->button_list, button_data));

            if (position != -1)
            {
                for (idx = 0; idx <= position; idx++)
                {
                    ButtonData *data;

                    data = BUTTON_DATA (self->button_list->data);

                    gtk_box_remove (GTK_BOX (self->buttons_box), data->container);
                    self->button_list = g_list_remove (self->button_list, data);
                    button_data_free (data);
                }
            }
        }

        g_object_unref (current_location);
        g_object_unref (location);
        return;
    }
    g_object_unref (location);

    /* MOUNTs use the GMount as the name, so don't update for those */
    if (button_data->type != MOUNT_BUTTON)
    {
        display_name = nautilus_file_get_display_name (file);
        if (g_strcmp0 (display_name, button_data->dir_name) != 0)
        {
            g_free (button_data->dir_name);
            button_data->dir_name = g_strdup (display_name);
        }

        g_free (display_name);
    }
    current_dir = g_file_equal (self->current_path, button_data->path);
    nautilus_path_bar_update_button_appearance (button_data, current_dir);
}

static ButtonData *
make_button_data (NautilusPathBar *self,
                  NautilusFile    *file,
                  gboolean         current_dir)
{
    GFile *path;
    GtkWidget *child = NULL;
    GtkEventController *controller;
    ButtonData *button_data;

    path = nautilus_file_get_location (file);

    /* Is it a special button? */
    button_data = g_new0 (ButtonData, 1);

    setup_button_type (button_data, self, path);
    button_data->button = gtk_button_new ();
    gtk_widget_set_focus_on_click (button_data->button, FALSE);
    gtk_widget_set_name (button_data->button, "NautilusPathButton");

    /* TODO update button type when xdg directories change */

    button_data->image = gtk_image_new ();

    switch (button_data->type)
    {
        case ROOT_BUTTON:
        case ADMIN_ROOT_BUTTON:
        case HOME_BUTTON:
        case MOUNT_BUTTON:
        case TRASH_BUTTON:
        case RECENT_BUTTON:
        case STARRED_BUTTON:
        case OTHER_LOCATIONS_BUTTON:
        {
            button_data->label = gtk_label_new (NULL);
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_box_append (GTK_BOX (button_data->container), button_data->button);

            gtk_box_append (GTK_BOX (child), button_data->image);
            gtk_box_append (GTK_BOX (child), button_data->label);
        }
        break;

        case NORMAL_BUTTON:
        /* Fall through */
        default:
        {
            GtkWidget *separator_label;

            separator_label = gtk_label_new (G_DIR_SEPARATOR_S);
            gtk_style_context_add_class (gtk_widget_get_style_context (separator_label), "dim-label");
            button_data->label = gtk_label_new (NULL);
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_append (GTK_BOX (button_data->container), separator_label);
            gtk_box_append (GTK_BOX (button_data->container), button_data->button);

            gtk_box_append (GTK_BOX (child), button_data->label);
        }
        break;
    }

    if (current_dir)
    {
        gtk_style_context_add_class (gtk_widget_get_style_context (button_data->button),
                                     "current-dir");
        gtk_widget_set_hexpand (button_data->button, TRUE);
        gtk_widget_set_halign (button_data->label, GTK_ALIGN_START);
    }

    if (button_data->label != NULL)
    {
        PangoAttrList *attrs;

        gtk_label_set_single_line_mode (GTK_LABEL (button_data->label), TRUE);

        attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes (GTK_LABEL (button_data->label), attrs);
        pango_attr_list_unref (attrs);

        if (!current_dir)
        {
            gtk_style_context_add_class (gtk_widget_get_style_context (button_data->label), "dim-label");
            gtk_style_context_add_class (gtk_widget_get_style_context (button_data->image), "dim-label");
        }
    }

    if (button_data->path == NULL)
    {
        button_data->path = g_object_ref (path);
    }
    if (button_data->dir_name == NULL)
    {
        button_data->dir_name = nautilus_file_get_display_name (file);
    }
    if (button_data->file == NULL)
    {
        button_data->file = nautilus_file_ref (file);
        nautilus_file_monitor_add (button_data->file, button_data,
                                   NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
        button_data->file_changed_signal_id =
            g_signal_connect (button_data->file, "changed",
                              G_CALLBACK (button_data_file_changed),
                              button_data);
    }

    gtk_button_set_child (GTK_BUTTON (button_data->button), child);
    gtk_widget_show (button_data->container);

    button_data->path_bar = self;

    nautilus_path_bar_update_button_state (button_data, current_dir);

    g_signal_connect (button_data->button, "clicked", G_CALLBACK (button_clicked_cb), button_data);

    /* A gesture is needed here, because GtkButton doesn’t react to middle- or
     * secondary-clicking.
     */
    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (button_data->button, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_click_gesture_pressed), button_data);

    nautilus_drag_slot_proxy_init (button_data->button, button_data->file, NULL);

    g_object_unref (path);

    return button_data;
}

static void
nautilus_path_bar_update_path (NautilusPathBar *self,
                               GFile           *file_path)
{
    NautilusFile *file;
    gboolean first_directory;
    GList *new_buttons, *l;
    ButtonData *button_data;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));
    g_return_if_fail (file_path != NULL);

    first_directory = TRUE;
    new_buttons = NULL;

    file = nautilus_file_get (file_path);

    while (file != NULL)
    {
        NautilusFile *parent_file;

        parent_file = nautilus_file_get_parent (file);
        button_data = make_button_data (self, file, first_directory);
        nautilus_file_unref (file);

        if (first_directory)
        {
            first_directory = FALSE;
        }

        new_buttons = g_list_prepend (new_buttons, button_data);

        if (parent_file != NULL &&
            button_data->is_root)
        {
            nautilus_file_unref (parent_file);
            break;
        }

        file = parent_file;
    }

    nautilus_path_bar_clear_buttons (self);

    /* Buttons are listed in reverse order such that the current location is
     * always the first link. */
    self->button_list = g_list_reverse (new_buttons);

    for (l = self->button_list; l; l = l->next)
    {
        GtkWidget *container;
        container = BUTTON_DATA (l->data)->container;
        gtk_box_prepend (GTK_BOX (self->buttons_box), container);
    }
}

void
nautilus_path_bar_set_path (NautilusPathBar *self,
                            GFile           *file_path)
{
    ButtonData *button_data;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));
    g_return_if_fail (file_path != NULL);

    nautilus_path_bar_update_path (self, file_path);
    button_data = g_list_nth_data (self->button_list, 0);

    if (self->current_path != NULL)
    {
        g_object_unref (self->current_path);
    }

    self->current_path = g_object_ref (file_path);
    self->current_button_data = button_data;
}
