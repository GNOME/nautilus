/* nautilus-rename-file-popover-controller.c
 *
 * Copyright (C) 2016 the Nautilus developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib/gi18n.h>

#include "nautilus-rename-file-popover-controller.h"

#include "nautilus-directory.h"
#include "nautilus-file-name-widget-controller.h"
#include "nautilus-file-private.h"
#include "nautilus-filename-utilities.h"


#define RENAME_ENTRY_MIN_CHARS 30
#define RENAME_ENTRY_MAX_CHARS 50

struct _NautilusRenameFilePopoverController
{
    GObject parent_instance;

    NautilusFileNameWidgetController *validator;

    NautilusFile *target_file;
    gboolean target_is_folder;

    NautilusRenameCallback callback;
    gpointer callback_data;

    GtkWidget *rename_file_popover;
    GtkWidget *name_entry;
    GtkWidget *title_label;

    gulong closed_handler_id;
    gulong file_changed_handler_id;
    gulong key_press_event_handler_id;
};

G_DEFINE_TYPE (NautilusRenameFilePopoverController, nautilus_rename_file_popover_controller, G_TYPE_OBJECT)

static void
disconnect_signal_handlers (NautilusRenameFilePopoverController *self)
{
    g_assert (NAUTILUS_IS_RENAME_FILE_POPOVER_CONTROLLER (self));

    g_clear_signal_handler (&self->closed_handler_id, self->rename_file_popover);
    g_clear_signal_handler (&self->file_changed_handler_id, self->target_file);
    g_clear_signal_handler (&self->key_press_event_handler_id, self->name_entry);
}

static void
reset_state (NautilusRenameFilePopoverController *self)
{
    g_assert (NAUTILUS_IS_RENAME_FILE_POPOVER_CONTROLLER (self));

    disconnect_signal_handlers (self);

    g_clear_object (&self->target_file);

    gtk_popover_popdown (GTK_POPOVER (self->rename_file_popover));
}

static void
rename_file_popover_controller_on_closed (GtkPopover *popover,
                                          gpointer    user_data)
{
    NautilusRenameFilePopoverController *self = NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER (user_data);

    reset_state (self);
}

static gboolean
name_entry_on_f2_pressed (GtkWidget                           *widget,
                          NautilusRenameFilePopoverController *self)
{
    guint text_length;
    gint start_pos;
    gint end_pos;
    gboolean all_selected;

    text_length = (guint) gtk_entry_get_text_length (GTK_ENTRY (widget));
    if (text_length == 0)
    {
        return GDK_EVENT_STOP;
    }

    gtk_editable_get_selection_bounds (GTK_EDITABLE (widget),
                                       &start_pos, &end_pos);

    all_selected = (start_pos == 0) && ((guint) end_pos == text_length);
    if (!all_selected || !nautilus_file_is_regular_file (self->target_file))
    {
        gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
    }
    else
    {
        /* Select the name part without the file extension */
        const char *text = gtk_editable_get_text (GTK_EDITABLE (widget));
        gtk_editable_select_region (GTK_EDITABLE (widget), 0,
                                    nautilus_filename_get_extension_char_offset (text));
    }

    return GDK_EVENT_STOP;
}

static gboolean
name_entry_on_undo (GtkWidget                           *widget,
                    NautilusRenameFilePopoverController *self)
{
    const char *edit_name = nautilus_file_get_edit_name (self->target_file);

    gtk_editable_set_text (GTK_EDITABLE (widget), edit_name);

    gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);

    return GDK_EVENT_STOP;
}

static gboolean
on_event_controller_key_key_pressed (GtkEventControllerKey *controller,
                                     guint                  keyval,
                                     guint                  keycode,
                                     GdkModifierType        state,
                                     gpointer               user_data)
{
    GtkWidget *widget;
    NautilusRenameFilePopoverController *self;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    self = NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER (user_data);

    if (keyval == GDK_KEY_F2)
    {
        return name_entry_on_f2_pressed (widget, self);
    }
    else if (keyval == GDK_KEY_z && (state & GDK_CONTROL_MASK) != 0)
    {
        return name_entry_on_undo (widget, self);
    }

    return GDK_EVENT_PROPAGATE;
}

static void
target_file_on_changed (NautilusFile *file,
                        gpointer      user_data)
{
    NautilusRenameFilePopoverController *self = NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER (user_data);

    if (nautilus_file_is_gone (file))
    {
        reset_state (self);
    }
}

static void
on_name_accepted (NautilusRenameFilePopoverController *self)
{
    g_autofree char *name = nautilus_file_name_widget_controller_get_new_name (self->validator);

    self->callback (self->target_file, name, self->callback_data);

    reset_state (self);
}

NautilusRenameFilePopoverController *
nautilus_rename_file_popover_controller_new (GtkWidget *relative_to)
{
    NautilusRenameFilePopoverController *self;
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *rename_file_popover;
    GtkWidget *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *activate_button;
    GtkWidget *title_label;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-rename-file-popover.ui");
    rename_file_popover = GTK_WIDGET (gtk_builder_get_object (builder, "rename_file_popover"));
    error_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "error_revealer"));
    error_label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
    name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
    activate_button = GTK_WIDGET (gtk_builder_get_object (builder, "rename_button"));
    title_label = GTK_WIDGET (gtk_builder_get_object (builder, "title_label"));

    self = g_object_new (NAUTILUS_TYPE_RENAME_FILE_POPOVER_CONTROLLER,
                         NULL);

    self->validator = g_object_new (NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER,
                                    "error-revealer", error_revealer,
                                    "error-label", error_label,
                                    "name-entry", name_entry,
                                    "activate-button", activate_button,
                                    NULL);

    g_signal_connect_object (self->validator, "name-accepted",
                             G_CALLBACK (on_name_accepted), self,
                             G_CONNECT_SWAPPED);

    self->rename_file_popover = g_object_ref_sink (rename_file_popover);
    self->name_entry = name_entry;
    self->title_label = title_label;

    gtk_popover_set_default_widget (GTK_POPOVER (rename_file_popover), name_entry);

    gtk_widget_set_parent (rename_file_popover, relative_to);

    return self;
}

void
nautilus_rename_file_popover_controller_show_for_file   (NautilusRenameFilePopoverController *self,
                                                         NautilusFile                        *target_file,
                                                         GdkRectangle                        *pointing_to,
                                                         NautilusRenameCallback               callback,
                                                         gpointer                             callback_data)
{
    g_autoptr (NautilusDirectory) containing_directory = NULL;
    GtkEventController *controller;
    const char *edit_name;
    gint n_chars;

    g_assert (NAUTILUS_IS_RENAME_FILE_POPOVER_CONTROLLER (self));
    g_assert (NAUTILUS_IS_FILE (target_file));

    reset_state (self);

    self->target_file = g_object_ref (target_file);
    self->callback = callback;
    self->callback_data = callback_data;

    if (!nautilus_file_is_self_owned (self->target_file))
    {
        g_autoptr (NautilusFile) parent = NULL;

        parent = nautilus_file_get_parent (self->target_file);
        containing_directory = nautilus_directory_get_for_file (parent);
    }
    else
    {
        containing_directory = nautilus_directory_get_for_file (self->target_file);
    }

    nautilus_file_name_widget_controller_set_containing_directory (self->validator,
                                                                   containing_directory);

    self->target_is_folder = nautilus_file_is_directory (self->target_file);

    nautilus_file_name_widget_controller_set_target_is_folder (self->validator,
                                                               self->target_is_folder);
    nautilus_file_name_widget_controller_set_original_name (self->validator,
                                                            nautilus_file_get_display_name (self->target_file));

    self->closed_handler_id = g_signal_connect (self->rename_file_popover,
                                                "closed",
                                                G_CALLBACK (rename_file_popover_controller_on_closed),
                                                self);

    self->file_changed_handler_id = g_signal_connect (self->target_file,
                                                      "changed",
                                                      G_CALLBACK (target_file_on_changed),
                                                      self);

    controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (self->name_entry, controller);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (on_event_controller_key_key_pressed), self);

    gtk_label_set_text (GTK_LABEL (self->title_label),
                        self->target_is_folder ? _("Rename Folder") :
                                                 _("Rename File"));

    edit_name = nautilus_file_get_edit_name (self->target_file);

    gtk_editable_set_text (GTK_EDITABLE (self->name_entry), edit_name);

    gtk_popover_set_pointing_to (GTK_POPOVER (self->rename_file_popover), pointing_to);

    gtk_popover_popup (GTK_POPOVER (self->rename_file_popover));

    if (nautilus_file_is_regular_file (self->target_file))
    {
        /* Select the name part without the file extension */
        gtk_editable_select_region (GTK_EDITABLE (self->name_entry), 0,
                                    nautilus_filename_get_extension_char_offset (edit_name));
    }

    n_chars = g_utf8_strlen (edit_name, -1);
    gtk_editable_set_width_chars (GTK_EDITABLE (self->name_entry),
                                  MIN (MAX (n_chars, RENAME_ENTRY_MIN_CHARS),
                                       RENAME_ENTRY_MAX_CHARS));
}

static void
nautilus_rename_file_popover_controller_init (NautilusRenameFilePopoverController *self)
{
}

static void
nautilus_rename_file_popover_controller_finalize (GObject *object)
{
    NautilusRenameFilePopoverController *self;

    self = NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER (object);

    reset_state (self);

    g_clear_object (&self->validator);

    g_clear_pointer (&self->rename_file_popover, gtk_widget_unparent);

    G_OBJECT_CLASS (nautilus_rename_file_popover_controller_parent_class)->finalize (object);
}

static void
nautilus_rename_file_popover_controller_class_init (NautilusRenameFilePopoverControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_rename_file_popover_controller_finalize;
}
