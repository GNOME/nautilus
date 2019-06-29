/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 *         Michael Meeks <michael@nuclecu.unam.mx>
 *     Andy Hertzfeld <andy@eazel.com>
 *
 */

/* nautilus-location-bar.c - Location bar for Nautilus
 */

#include <config.h>
#include "nautilus-location-entry.h"

#include "nautilus-application.h"
#include "nautilus-window.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include "nautilus-file-utilities.h"
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    NAUTILUS_LOCATION_ENTRY_ACTION_GOTO,
    NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR
} NautilusLocationEntryAction;

struct _NautilusLocationEntry
{
    GtkEntry parent_instance;

    char *current_directory;
    GFilenameCompleter *completer;

    guint idle_id;

    GFile *last_location;

    NautilusLocationEntryAction secondary_action;
};

enum
{
    CANCEL,
    LOCATION_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NautilusLocationEntry, nautilus_location_entry, GTK_TYPE_ENTRY);

static GFile *
nautilus_location_entry_get_location (NautilusLocationEntry *self)
{
    g_autofree char *user_location = NULL;

    user_location = gtk_editable_get_chars (GTK_EDITABLE (self), 0, -1);

    return g_file_parse_name (user_location);
}

static void
emit_location_changed (NautilusLocationEntry *entry)
{
    GFile *location;

    location = nautilus_location_entry_get_location (entry);
    g_signal_emit (entry, signals[LOCATION_CHANGED], 0, location);
    g_object_unref (location);
}

static void
nautilus_location_entry_set_secondary_action (NautilusLocationEntry       *self,
                                              NautilusLocationEntryAction  action)
{
    if (self->secondary_action == action)
    {
        return;
    }

    switch (action)
    {
        case NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR:
        {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               "edit-clear-symbolic");
        }
        break;

        case NAUTILUS_LOCATION_ENTRY_ACTION_GOTO:
        {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               "go-next-symbolic");
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    self->secondary_action = action;
}

static void
nautilus_location_entry_update_action (NautilusLocationEntry *self)
{
    GtkEditable *editable;
    const char *current_text;
    g_autoptr (GFile) location = NULL;

    if (self->last_location == NULL)
    {
        nautilus_location_entry_set_secondary_action (self,
                                                      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
        return;
    }

    editable = GTK_EDITABLE (self);
    current_text = gtk_editable_get_text (editable);
    location = g_file_parse_name (current_text);

    if (g_file_equal (self->last_location, location))
    {
        nautilus_location_entry_set_secondary_action (self,
                                                      NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);
    }
    else
    {
        nautilus_location_entry_set_secondary_action (self,
                                                      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
    }
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
    g_autofree char *text = NULL;
    glong length;

    text = gtk_editable_get_chars (editable, 0, -1);
    length = g_utf8_strlen (text, -1);

    return length;
}

static void
set_position_and_selection_to_end (GtkEditable *editable)
{
    int end;

    end = get_editable_number_of_chars (editable);
    gtk_editable_select_region (editable, end, end);
    gtk_editable_set_position (editable, end);
}

static void
nautilus_location_entry_update_current_uri (NautilusLocationEntry *self,
                                            const char            *uri)
{
    g_free (self->current_directory);
    self->current_directory = g_strdup (uri);

    gtk_entry_set_text (GTK_ENTRY (self), uri);
    set_position_and_selection_to_end (GTK_EDITABLE (self));
}

void
nautilus_location_entry_set_location (NautilusLocationEntry *self,
                                      GFile                 *location)
{
    g_autofree char *uri = NULL;
    g_autofree char *formatted_uri = NULL;

    g_return_if_fail (NAUTILUS_IS_LOCATION_ENTRY (self));
    g_return_if_fail (G_IS_FILE (location));

    /* Note: This is called in reaction to external changes, and
     * thus should not emit the LOCATION_CHANGED signal. */
    uri = g_file_get_uri (location);
    formatted_uri = g_file_get_parse_name (location);

    if (eel_uri_is_search (uri))
    {
        nautilus_location_entry_set_special_text (self, "");
    }
    else
    {
        nautilus_location_entry_update_current_uri (self, formatted_uri);
    }

    /* remember the original location for later comparison */
    if (self->last_location == NULL ||
        !g_file_equal (self->last_location, location))
    {
        g_set_object (&self->last_location, location);
    }

    nautilus_location_entry_update_action (self);
}

static void
drag_data_received_callback (GtkWidget        *widget,
                             GdkDrop          *drop,
                             GtkSelectionData *data,
                             gpointer          user_data)
{
    NautilusLocationEntry *self;
    g_auto (GStrv) uris = NULL;
    int name_count;
    GtkWidget *window;
    gboolean new_windows_for_extras;
    GFile *location;

    self = NAUTILUS_LOCATION_ENTRY (user_data);
    uris = gtk_selection_data_get_uris (data);
    if (uris == NULL || *uris == NULL)
    {
        g_warning ("No D&D URI's");
        gdk_drop_finish (drop, 0);
        return;
    }
    window = gtk_widget_get_toplevel (widget);
    new_windows_for_extras = FALSE;
    /* Ask user if they really want to open multiple windows
     * for multiple dropped URIs. This is likely to have been
     * a mistake.
     */
    name_count = g_strv_length (uris);
    if (name_count > 1)
    {
        g_autofree char *prompt = NULL;
        g_autofree char *detail = NULL;

        prompt = g_strdup_printf (ngettext ("Do you want to view %d location?",
                                            "Do you want to view %d locations?",
                                            name_count),
                                  name_count);
        detail = g_strdup_printf (ngettext ("This will open %d separate window.",
                                            "This will open %d separate windows.",
                                            name_count),
                                  name_count);
        /* eel_run_simple_dialog should really take in pairs
         * like gtk_dialog_new_with_buttons() does. */
        new_windows_for_extras = eel_run_simple_dialog (GTK_WIDGET (window),
                                                        TRUE,
                                                        GTK_MESSAGE_QUESTION,
                                                        prompt,
                                                        detail,
                                                        _("_Cancel"), _("_OK"),
                                                        NULL) != 0 /* GNOME_OK */;

        if (!new_windows_for_extras)
        {
            gdk_drop_finish (drop, 0);
            return;
        }
    }

    location = g_file_new_for_uri (uris[0]);
    nautilus_location_entry_set_location (self, location);
    emit_location_changed (self);
    g_object_unref (location);

    if (new_windows_for_extras)
    {
        int i;

        for (i = 1; uris[i] != NULL; ++i)
        {
            location = g_file_new_for_uri (uris[i]);
            nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                     location, NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW, NULL, NULL, NULL);
            g_object_unref (location);
        }
    }

    gdk_drop_finish (drop, gdk_drop_get_actions (drop));
}

static void
drag_data_get_callback (GtkWidget        *widget,
                        GdkDrag          *context,
                        GtkSelectionData *data,
                        gpointer          user_data)
{
    NautilusLocationEntry *self;
    g_autoptr (GFile) location = NULL;
    g_autofree char *uri = NULL;

    if (!gtk_selection_data_targets_include_text (data) &&
        !gtk_selection_data_targets_include_uri (data))
    {
        return;
    }

    self = NAUTILUS_LOCATION_ENTRY (user_data);
    location = nautilus_location_entry_get_location (self);
    uri = g_file_get_uri (location);

    gtk_selection_data_set_uris (data, (char *[]) { uri, NULL });
}

/* routine that performs the tab expansion.  Extract the directory name and
 *  incomplete basename, then iterate through the directory trying to complete it.  If we
 *  find something, add it to the entry */

static gboolean
try_to_expand_path (gpointer callback_data)
{
    NautilusLocationEntry *self;
    GtkEditable *editable;
    g_autofree char *suffix = NULL;
    g_autofree char *user_location = NULL;
    int user_location_length;
    g_autofree char *uri_scheme = NULL;

    self = NAUTILUS_LOCATION_ENTRY (callback_data);
    editable = GTK_EDITABLE (self);
    user_location = gtk_editable_get_chars (editable, 0, -1);
    user_location_length = g_utf8_strlen (user_location, -1);
    user_location = g_strchug (user_location);
    user_location = g_strchomp (user_location);
    uri_scheme = g_uri_parse_scheme (user_location);

    self->idle_id = 0;

    if (!g_path_is_absolute (user_location) && uri_scheme == NULL && user_location[0] != '~')
    {
        g_autofree char *absolute_location = NULL;

        absolute_location = g_build_filename (self->current_directory, user_location, NULL);
        suffix = g_filename_completer_get_completion_suffix (self->completer,
                                                             absolute_location);
    }
    else
    {
        suffix = g_filename_completer_get_completion_suffix (self->completer,
                                                             user_location);
    }

    /* if we've got something, add it to the entry */
    if (suffix != NULL)
    {
        int pos;

        pos = user_location_length;
        gtk_editable_insert_text (editable,
                                  suffix, -1, &pos);
        pos = user_location_length;
        gtk_editable_select_region (editable, pos, -1);
    }

    return G_SOURCE_REMOVE;
}

static gboolean
position_and_selection_are_at_end (GtkEditable *editable)
{
    int end;
    int start_sel, end_sel;

    end = get_editable_number_of_chars (editable);
    if (gtk_editable_get_selection_bounds (editable, &start_sel, &end_sel))
    {
        if (start_sel != end || end_sel != end)
        {
            return FALSE;
        }
    }
    return gtk_editable_get_position (editable) == end;
}

static void
got_completion_data_callback (GFilenameCompleter    *completer,
                              NautilusLocationEntry *entry)
{
    if (entry->idle_id)
    {
        g_source_remove (entry->idle_id);
        entry->idle_id = 0;
    }
    try_to_expand_path (entry);
}

static void
nautilus_location_entry_finalize (GObject *object)
{
    NautilusLocationEntry *self;

    self = NAUTILUS_LOCATION_ENTRY (object);

    g_clear_object (&self->completer);
    g_clear_object (&self->last_location);

    G_OBJECT_CLASS (nautilus_location_entry_parent_class)->finalize (object);
}

static void
nautilus_location_entry_destroy (GtkWidget *object)
{
    NautilusLocationEntry *self;

    self = NAUTILUS_LOCATION_ENTRY (object);

    /* cancel the pending idle call, if any */
    if (self->idle_id != 0)
    {
        g_source_remove (self->idle_id);
        self->idle_id = 0;
    }

    g_clear_pointer (&self->current_directory, g_free);

    GTK_WIDGET_CLASS (nautilus_location_entry_parent_class)->destroy (object);
}

static void
nautilus_location_entry_icon_release (GtkEntry             *entry,
                                      GtkEntryIconPosition  position,
                                      GdkEvent             *event,
                                      gpointer              user_data)
{
    NautilusLocationEntry *self;

    self = NAUTILUS_LOCATION_ENTRY (user_data);

    switch (self->secondary_action)
    {
        case NAUTILUS_LOCATION_ENTRY_ACTION_GOTO:
        {
            g_signal_emit_by_name (entry, "activate", entry);
        }
        break;

        case NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR:
        {
            gtk_entry_set_text (entry, "");
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static gboolean
nautilus_location_entry_key_pressed (GtkEventControllerKey *controller,
                                     unsigned int           keyval,
                                     unsigned int           keycode,
                                     GdkModifierType        state,
                                     gpointer               user_data)
{
    g_autoptr (GdkEvent) event = NULL;
    GtkWidget *widget;
    NautilusLocationEntry *self;
    GtkEditable *editable;
    int start_pos;
    int end_pos;
    gboolean selected;

    event = gtk_get_current_event ();
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    self = NAUTILUS_LOCATION_ENTRY (widget);
    editable = GTK_EDITABLE (widget);
    selected = gtk_editable_get_selection_bounds (editable, &start_pos, &end_pos);

    if (!gtk_editable_get_editable (editable))
    {
        return GDK_EVENT_PROPAGATE;
    }

    /* The location bar entry wants TAB to work kind of
     * like it does in the shell for command completion,
     * so if we get a tab and there's a selection, we
     * should position the insertion point at the end of
     * the selection.
     */
    if (keyval == GDK_KEY_Tab)
    {
        if (selected)
        {
            if (start_pos != end_pos)
            {
                gtk_editable_set_position (editable, MAX (start_pos, end_pos));
            }
        }

        return GDK_EVENT_STOP;
    }

    if ((keyval == GDK_KEY_Right || keyval == GDK_KEY_End) &&
        !(state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) && selected)
    {
        set_position_and_selection_to_end (editable);
    }

    gtk_widget_event (widget, event);

    /* Only do expanding when we are typing at the end of the
     * text. Do the expand at idle time to avoid slowing down
     * typing when the directory is large. Only trigger the expand
     * when we type a key that would have inserted characters.
     */
    if (position_and_selection_are_at_end (editable))
    {
        uint32_t unichar;
        GdkModifierType no_text_input_mask;

        unichar = gdk_keyval_to_unicode (keyval);
        no_text_input_mask = gtk_widget_get_modifier_mask (widget,
                                                           GDK_MODIFIER_INTENT_NO_TEXT_INPUT);

        if (g_unichar_isgraph (unichar) && !(state & no_text_input_mask))
        {
            if (self->idle_id == 0)
            {
                self->idle_id = g_idle_add (try_to_expand_path, self);
            }
        }
    }
    else
    {
        /* FIXME: Also might be good to do this when you click
         * to change the position or selection.
         */
        if (self->idle_id != 0)
        {
            g_source_remove (self->idle_id);
            self->idle_id = 0;
        }
    }

    return GDK_EVENT_STOP;
}

static void
on_entry_activate (GtkEntry *entry,
                   gpointer  user_data)
{
    GtkEditable *editable;
    g_autofree char *text = NULL;
    g_autofree char *uri_scheme = NULL;
    NautilusLocationEntry *self;

    editable = GTK_EDITABLE (entry);
    text = gtk_editable_get_text (editable);
    text = g_strdup (text);
    text = g_strchug (text);
    text = g_strchomp (text);
    if ('\0' == *text)
    {
        return;
    }
    uri_scheme = g_uri_parse_scheme (text);
    self = NAUTILUS_LOCATION_ENTRY (user_data);

    if (!g_path_is_absolute (text) && uri_scheme == NULL && text[0] != '~')
    {
        g_autofree char *full_path = NULL;

        /* Fix non absolute paths */
        full_path = g_build_filename (self->current_directory, text, NULL);
        gtk_entry_set_text (entry, full_path);
    }

    emit_location_changed (self);
}

static void
on_entry_changed (GtkEditable *editable,
                  gpointer     user_data)
{
    NautilusLocationEntry *self;

    self = NAUTILUS_LOCATION_ENTRY (user_data);

    nautilus_location_entry_update_action (self);
}

static void
nautilus_location_entry_cancel (NautilusLocationEntry *self)
{
    nautilus_location_entry_set_location (self, self->last_location);
}

static void
nautilus_location_entry_class_init (NautilusLocationEntryClass *class)
{
    GtkWidgetClass *widget_class;
    GObjectClass *gobject_class;
    GtkBindingSet *binding_set;

    widget_class = GTK_WIDGET_CLASS (class);
    widget_class->destroy = nautilus_location_entry_destroy;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = nautilus_location_entry_finalize;

    signals[CANCEL] = g_signal_new ("cancel",
                                    G_TYPE_FROM_CLASS (class),
                                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                    0,
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0);
    signals[LOCATION_CHANGED] = g_signal_new
                                    ("location-changed",
                                    G_TYPE_FROM_CLASS (class),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__OBJECT,
                                    G_TYPE_NONE, 1, G_TYPE_OBJECT);

    g_signal_override_class_handler ("cancel", G_TYPE_FROM_CLASS (class),
                                     G_CALLBACK (nautilus_location_entry_cancel));

    binding_set = gtk_binding_set_by_class (class);
    gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);
}

static void
nautilus_location_entry_init (NautilusLocationEntry *self)
{
    g_autoptr (GdkContentFormats) targets = NULL;
    GtkEventController *controller;

    targets = gdk_content_formats_new (NULL, 0);
    targets = gtk_content_formats_add_text_targets (targets);
    targets = gtk_content_formats_add_uri_targets (targets);

    gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

    self->completer = g_filename_completer_new ();
    g_filename_completer_set_dirs_only (self->completer, TRUE);

    gtk_entry_set_icon_activatable (GTK_ENTRY (self), GTK_ENTRY_ICON_PRIMARY, FALSE);

    gtk_entry_set_icon_drag_source (GTK_ENTRY (self), GTK_ENTRY_ICON_PRIMARY, targets, GDK_ACTION_ALL);
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self), GTK_ENTRY_ICON_PRIMARY, "folder-symbolic");

    nautilus_location_entry_set_secondary_action (self,
                                                  NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);

    g_signal_connect (self, "icon-release",
                      G_CALLBACK (nautilus_location_entry_icon_release), self);
    g_signal_connect (self, "drag-data-received",
                      G_CALLBACK (drag_data_received_callback), self);
    g_signal_connect (self, "drag-data-get",
                      G_CALLBACK (drag_data_get_callback), self);
    g_signal_connect (self, "activate",
                      G_CALLBACK (on_entry_activate), self);
    g_signal_connect (self, "changed",
                      G_CALLBACK (on_entry_changed), self);

    g_signal_connect (self->completer, "got-completion-data",
                      G_CALLBACK (got_completion_data_callback), self);

    /* Drag dest. */
    gtk_drag_dest_set (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, targets, GDK_ACTION_ALL);

    controller = gtk_event_controller_key_new ();

    gtk_widget_add_controller (GTK_WIDGET (self), controller);

    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);

    g_signal_connect (controller,
                      "key-pressed", G_CALLBACK (nautilus_location_entry_key_pressed),
                      NULL);
}

GtkWidget *
nautilus_location_entry_new (void)
{
    return gtk_widget_new (NAUTILUS_TYPE_LOCATION_ENTRY, "max-width-chars", 350, NULL);
}

void
nautilus_location_entry_set_special_text (NautilusLocationEntry *self,
                                          const char            *special_text)
{
    g_return_if_fail (NAUTILUS_IS_LOCATION_ENTRY (self));

    gtk_entry_set_text (GTK_ENTRY (self), special_text);
}
