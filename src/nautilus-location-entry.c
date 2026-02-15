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
#include "nautilus-scheme.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include "nautilus-file-utilities.h"
#include "nautilus-clipboard.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
    GFile *location;
    char *prefix;
    char *typed_path;
    NautilusLocationEntry *entry;
} CompleterData;

typedef struct _NautilusLocationEntryPrivate
{
    GFile *current_location;

    gboolean idle_insert_completion;

    GFile *last_location;

    gboolean has_special_text;
    NautilusLocationEntryAction secondary_action;

    GtkEventController *controller;

    guint completion_id;
    GtkEntryCompletion *completion;
    GtkListStore *completions_store;
    GtkCellRenderer *completion_cell;
    GCancellable *completions_cancellable;
} NautilusLocationEntryPrivate;

enum
{
    CANCEL,
    LOCATION_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (NautilusLocationEntry, nautilus_location_entry, GTK_TYPE_ENTRY);

static void on_after_insert_text (GtkEditable *editable,
                                  const gchar *text,
                                  gint         length,
                                  gint        *position,
                                  gpointer     data);

static void on_after_delete_text (GtkEditable *editable,
                                  gint         start_pos,
                                  gint         end_pos,
                                  gpointer     data);

static GFile *
nautilus_location_entry_get_location (NautilusLocationEntry *entry)
{
    char *user_location;
    GFile *location;

    user_location = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
    location = g_file_parse_name (user_location);
    g_free (user_location);

    return location;
}

static void
nautilus_location_entry_set_text (NautilusLocationEntry *entry,
                                  const char            *new_text)
{
    GtkEditable *delegate;

    delegate = gtk_editable_get_delegate (GTK_EDITABLE (entry));
    g_signal_handlers_block_by_func (delegate, G_CALLBACK (on_after_insert_text), entry);
    g_signal_handlers_block_by_func (delegate, G_CALLBACK (on_after_delete_text), entry);

    gtk_editable_set_text (GTK_EDITABLE (entry), new_text);

    g_signal_handlers_unblock_by_func (delegate, G_CALLBACK (on_after_insert_text), entry);
    g_signal_handlers_unblock_by_func (delegate, G_CALLBACK (on_after_delete_text), entry);
}

static void
nautilus_location_entry_insert_prefix (NautilusLocationEntry *entry,
                                       GtkEntryCompletion    *completion)
{
    GtkEditable *delegate;

    delegate = gtk_editable_get_delegate (GTK_EDITABLE (entry));
    g_signal_handlers_block_by_func (delegate, G_CALLBACK (on_after_insert_text), entry);

    gtk_entry_completion_insert_prefix (completion);

    g_signal_handlers_unblock_by_func (delegate, G_CALLBACK (on_after_insert_text), entry);
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
nautilus_location_entry_update_action (NautilusLocationEntry *entry)
{
    NautilusLocationEntryPrivate *priv;
    const char *current_text;
    GFile *location;

    priv = nautilus_location_entry_get_instance_private (entry);

    if (priv->last_location == NULL)
    {
        nautilus_location_entry_set_secondary_action (entry,
                                                      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
        return;
    }

    current_text = gtk_editable_get_text (GTK_EDITABLE (entry));
    location = g_file_parse_name (current_text);

    if (g_file_equal (priv->last_location, location))
    {
        nautilus_location_entry_set_secondary_action (entry,
                                                      NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);
    }
    else
    {
        nautilus_location_entry_set_secondary_action (entry,
                                                      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
    }

    g_object_unref (location);
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
    char *text;
    int length;

    text = gtk_editable_get_chars (editable, 0, -1);
    length = g_utf8_strlen (text, -1);
    g_free (text);
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

void
nautilus_location_entry_set_location (NautilusLocationEntry *entry,
                                      GFile                 *location)
{
    g_autofree char *scheme = g_file_get_uri_scheme (location);
    NautilusLocationEntryPrivate *priv;

    g_assert (location != NULL);

    priv = nautilus_location_entry_get_instance_private (entry);

    /* Note: This is called in reaction to external changes, and
     * thus should not emit the LOCATION_CHANGED signal. */

    if (nautilus_scheme_is_internal (scheme))
    {
        nautilus_location_entry_set_special_text (entry, "");
    }
    else
    {
        g_set_object (&priv->current_location, location);

        g_autofree gchar *formatted_uri = g_file_get_parse_name (location);

        nautilus_location_entry_set_text (entry, formatted_uri);
        set_position_and_selection_to_end (GTK_EDITABLE (entry));
    }

    /* remember the original location for later comparison */
    if (!priv->last_location ||
        !g_file_equal (priv->last_location, location))
    {
        g_clear_object (&priv->last_location);
        priv->last_location = g_object_ref (location);
    }

    nautilus_location_entry_update_action (entry);

    /* invalidate the completions list */
    gtk_list_store_clear (priv->completions_store);
}

static void
set_prefix_dimming (GtkCellRenderer *completion_cell,
                    char            *typed_path)
{
    if (typed_path == NULL)
    {
        /* Nothing to do*/
        return;
    }

    PangoAttrList *attrs;
    PangoAttribute *attr;

    /* Dim the prefixes of the completion rows, leaving the basenames
     * highlighted. This makes it easier to find what you're looking for.
     *
     * Perhaps a better solution would be to *only* show the basenames, but
     * it would take a reimplementation of GtkEntryCompletion to align the
     * popover. */

    attrs = pango_attr_list_new ();

    /* 55% opacity. This is the same as the dim-label style class in Adwaita. */
    attr = pango_attr_foreground_alpha_new (36045);
    attr->end_index = strlen (typed_path);
    pango_attr_list_insert (attrs, attr);

    g_object_set (completion_cell, "attributes", attrs, NULL);
    pango_attr_list_unref (attrs);
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

static CompleterData *
completer_data_new (const char *typed,
                    GFile      *location)
{
    CompleterData *data = g_new0 (CompleterData, 1);
    const char *last_separator = strrchr (typed, G_DIR_SEPARATOR);
    const char *post_separator = (last_separator != NULL) ? last_separator + 1 : typed;
    g_autofree gchar *uri_scheme = g_uri_parse_scheme (typed);

    if (last_separator != NULL)
    {
        data->typed_path = g_strndup (typed, post_separator - typed);
    }
    data->prefix = g_utf8_casefold (post_separator, -1);

    if (uri_scheme != NULL && last_separator != NULL)
    {
        /* Parse scheme with GFile */
        data->location = g_file_parse_name (data->typed_path);
    }
    else if (data->typed_path == NULL)
    {
        data->location = g_object_ref (location);
    }
    else if (typed[0] == '~' && typed[1] == '/')
    {
        /* "~/" is not handled by g_file_resolve_relative_path */

        if (typed + 1 == last_separator)
        {
            data->location = g_file_new_for_path (g_get_home_dir ());
        }
        else
        {
            const char *subdir_path = typed + 2;
            g_autofree char *concat_path = g_strndup (subdir_path, post_separator - subdir_path);

            data->location = g_file_new_build_filename (g_get_home_dir (), concat_path, NULL);
        }
    }
    else
    {
        data->location = g_file_resolve_relative_path (location, data->typed_path);
    }

    return data;
}

static void
completer_data_free (CompleterData *completer_data)
{
    g_clear_object (&completer_data->location);
    g_free (completer_data->prefix);
    g_free (completer_data->typed_path);
    g_free (completer_data);
}

static void
completer_get_completions_thread (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
    CompleterData *data = task_data;
    gboolean searched_prefix_has_dot = g_str_has_prefix (data->prefix, ".");
    g_autoptr (GPtrArray) completions = g_ptr_array_new_with_free_func (g_free);
    g_autoptr (GFileEnumerator) enumerator = g_file_enumerate_children (data->location,
                                                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                                        G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                                        G_FILE_QUERY_INFO_NONE,
                                                                        cancellable,
                                                                        NULL);
    GFileInfo *info = NULL;

    if (enumerator == NULL)
    {
        if (!g_task_return_error_if_cancelled (task))
        {
            g_task_return_error (task,
                                 g_error_new_literal (G_IO_ERROR,
                                                      G_IO_ERROR_FAILED,
                                                      "Could not enumerate directory"));
        }
        return;
    }

    while (g_file_enumerator_iterate (enumerator, &info, NULL, cancellable, NULL) &&
           info != NULL)
    {
        if (g_task_return_error_if_cancelled (task))
        {
            return;
        }
        const char *name = g_file_info_get_name (info);

        if (g_str_has_prefix (name, ".") && !searched_prefix_has_dot)
        {
            /* skip hidden files until the user type "." */
            continue;
        }

        g_autofree gchar *case_insenstive_name = g_utf8_casefold (name, -1);

        if (g_str_has_prefix (case_insenstive_name, data->prefix))
        {
            char *completion = (data->typed_path != NULL)
                               ? g_strconcat (data->typed_path, name, NULL)
                               : g_strdup (name);

            g_ptr_array_add (completions, completion);
        }
    }

    g_task_return_pointer (task,
                           g_steal_pointer (&completions),
                           (GDestroyNotify) g_ptr_array_unref);
}

static void
completer_get_completions_async (CompleterData       *completer_data,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback)
{
    g_autoptr (GTask) task = g_task_new (NULL, cancellable, callback, completer_data);

    g_task_set_task_data (task, completer_data, (GDestroyNotify) completer_data_free);
    g_task_run_in_thread (task, (GTaskThreadFunc) completer_get_completions_thread);
}

static void
populate_completions_model (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    GtkTreeIter iter;
    GTask *task = G_TASK (res);

    if (g_task_had_error (task))
    {
        return;
    }
    CompleterData *completer_data = user_data;
    NautilusLocationEntry *entry = completer_data->entry;
    NautilusLocationEntryPrivate *priv = nautilus_location_entry_get_instance_private (entry);

    /* populate the completions model */
    gtk_list_store_clear (priv->completions_store);
    g_autoptr (GError) error = NULL;

    g_autoptr (GPtrArray) completions = g_task_propagate_pointer (task, &error);

    for (guint i = 0; i < completions->len; i++)
    {
        char *completion = g_ptr_array_index (completions, i);

        gtk_list_store_append (priv->completions_store, &iter);
        gtk_list_store_set (priv->completions_store, &iter, 0, completion, -1);
    }

    /* refilter the completions dropdown */
    gtk_entry_completion_complete (priv->completion);

    if (priv->idle_insert_completion)
    {
        /* insert the completion */
        nautilus_location_entry_insert_prefix (entry, priv->completion);
    }
}

/* Update the path completions list based on the current text of the entry. */
static gboolean
update_completions_store (gpointer callback_data)
{
    NautilusLocationEntry *entry = NAUTILUS_LOCATION_ENTRY (callback_data);
    NautilusLocationEntryPrivate *priv = nautilus_location_entry_get_instance_private (entry);
    GtkEditable *editable = GTK_EDITABLE (entry);

    priv->completion_id = 0;

    /* Only do completions when we are typing at the end of the
     * text. */
    if (!position_and_selection_are_at_end (editable))
    {
        return FALSE;
    }

    int start_sel;
    g_autofree char *typed = gtk_editable_get_selection_bounds (editable, &start_sel, NULL)
                             ? gtk_editable_get_chars (editable, 0, start_sel)
                             : gtk_editable_get_chars (editable, 0, -1);

    if (typed == NULL || typed[0] == '\0')
    {
        return FALSE;
    }

    g_strstrip (typed);

    CompleterData *completer_data = completer_data_new (typed, priv->current_location);

    completer_data->entry = entry;
    set_prefix_dimming (priv->completion_cell, completer_data->typed_path);

    if (priv->completions_cancellable != NULL)
    {
        g_cancellable_cancel (priv->completions_cancellable);
        g_clear_object (&priv->completions_cancellable);
    }

    priv->completions_cancellable = g_cancellable_new ();
    completer_get_completions_async (completer_data,
                                     priv->completions_cancellable,
                                     populate_completions_model);

    return FALSE;
}

static void
finalize (GObject *object)
{
    NautilusLocationEntry *entry;
    NautilusLocationEntryPrivate *priv;

    entry = NAUTILUS_LOCATION_ENTRY (object);
    priv = nautilus_location_entry_get_instance_private (entry);

    if (priv->completions_cancellable != NULL)
    {
        g_cancellable_cancel (priv->completions_cancellable);
        g_clear_object (&priv->completions_cancellable);
    }

    g_clear_object (&priv->last_location);
    g_clear_object (&priv->completion);
    g_clear_object (&priv->completions_store);
    g_clear_object (&priv->current_location);

    G_OBJECT_CLASS (nautilus_location_entry_parent_class)->finalize (object);
}

static void
nautilus_location_entry_dispose (GObject *object)
{
    NautilusLocationEntry *entry;
    NautilusLocationEntryPrivate *priv;

    entry = NAUTILUS_LOCATION_ENTRY (object);
    priv = nautilus_location_entry_get_instance_private (entry);

    /* cancel the pending idle call, if any */
    g_clear_handle_id (&priv->completion_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_location_entry_parent_class)->dispose (object);
}

static void
on_has_focus_changed (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
    NautilusLocationEntry *entry;
    NautilusLocationEntryPrivate *priv;

    if (!gtk_widget_has_focus (GTK_WIDGET (object)))
    {
        return;
    }

    entry = NAUTILUS_LOCATION_ENTRY (object);
    priv = nautilus_location_entry_get_instance_private (entry);

    /* The entry has text which is not worth preserving on focus-in. */
    if (priv->has_special_text)
    {
        nautilus_location_entry_set_text (entry, "");
    }
}

static void
nautilus_location_entry_text_changed (NautilusLocationEntry *entry,
                                      GParamSpec            *pspec)
{
    NautilusLocationEntryPrivate *priv;

    priv = nautilus_location_entry_get_instance_private (entry);

    priv->has_special_text = FALSE;
}

static void
nautilus_location_entry_icon_release (GtkEntry             *gentry,
                                      GtkEntryIconPosition  position,
                                      gpointer              unused)
{
    NautilusLocationEntry *entry;
    NautilusLocationEntryPrivate *priv;

    entry = NAUTILUS_LOCATION_ENTRY (gentry);
    priv = nautilus_location_entry_get_instance_private (entry);

    switch (priv->secondary_action)
    {
        case NAUTILUS_LOCATION_ENTRY_ACTION_GOTO:
        {
            g_signal_emit_by_name (gentry, "activate", gentry);
        }
        break;

        case NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR:
        {
            nautilus_location_entry_set_text (entry, "");
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
    GtkWidget *widget;
    GtkEditable *editable;
    gboolean selected;


    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    editable = GTK_EDITABLE (widget);
    selected = gtk_editable_get_selection_bounds (editable, NULL, NULL);

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
    if (keyval == GDK_KEY_Tab && !(state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
    {
        if (selected)
        {
            int position;

            position = strlen (gtk_editable_get_text (GTK_EDITABLE (editable)));
            gtk_editable_select_region (editable, position, position);
        }
        else
        {
            gtk_widget_error_bell (widget);
        }

        return GDK_EVENT_STOP;
    }

    if ((keyval == GDK_KEY_Right || keyval == GDK_KEY_End) &&
        !(state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) && selected)
    {
        set_position_and_selection_to_end (editable);
    }

    return GDK_EVENT_PROPAGATE;
}

static void
after_text_change (NautilusLocationEntry *self,
                   gboolean               insert)
{
    NautilusLocationEntryPrivate *priv = nautilus_location_entry_get_instance_private (self);

    /* Only insert a completion if a character was typed. Otherwise,
     * update the completions store (i.e. in case backspace was pressed)
     * but don't insert the completion into the entry. */
    priv->idle_insert_completion = insert;

    /* Do the expand at idle time to avoid slowing down typing when the
     * directory is large. */
    if (priv->completion_id == 0)
    {
        priv->completion_id = g_idle_add (update_completions_store, self);
    }
}

static void
on_after_insert_text (GtkEditable *editable,
                      const gchar *text,
                      gint         length,
                      gint        *position,
                      gpointer     data)
{
    NautilusLocationEntry *self = NAUTILUS_LOCATION_ENTRY (data);

    after_text_change (self, TRUE);
}

static void
on_after_delete_text (GtkEditable *editable,
                      gint         start_pos,
                      gint         end_pos,
                      gpointer     data)
{
    NautilusLocationEntry *self = NAUTILUS_LOCATION_ENTRY (data);

    after_text_change (self, FALSE);
}

static void
nautilus_location_entry_activate (GtkEntry *entry)
{
    NautilusLocationEntry *loc_entry;
    NautilusLocationEntryPrivate *priv;
    const gchar *entry_text;
    g_autofree char *path = NULL;

    loc_entry = NAUTILUS_LOCATION_ENTRY (entry);
    priv = nautilus_location_entry_get_instance_private (loc_entry);
    entry_text = gtk_editable_get_text (GTK_EDITABLE (entry));
    path = g_strdup (entry_text);
    path = g_strchug (path);
    path = g_strchomp (path);

    if (path != NULL && *path != '\0')
    {
        g_autofree gchar *uri_scheme = g_uri_parse_scheme (path);

        if (!g_path_is_absolute (path) && uri_scheme == NULL && path[0] != '~')
        {
            /* Fix non absolute paths */
            g_autoptr (GFile) file = g_file_resolve_relative_path (priv->current_location, path);
            g_autofree char *full_path = g_file_get_parse_name (file);

            nautilus_location_entry_set_text (loc_entry, full_path);
        }
    }
}

static void
nautilus_location_entry_cancel (NautilusLocationEntry *entry)
{
    NautilusLocationEntryPrivate *priv;

    priv = nautilus_location_entry_get_instance_private (entry);

    nautilus_location_entry_set_location (entry, priv->last_location);
}

static void
nautilus_location_entry_class_init (NautilusLocationEntryClass *class)
{
    GObjectClass *gobject_class;
    GtkEntryClass *entry_class;
    g_autoptr (GtkShortcut) shortcut = NULL;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->dispose = nautilus_location_entry_dispose;
    gobject_class->finalize = finalize;

    entry_class = GTK_ENTRY_CLASS (class);
    entry_class->activate = nautilus_location_entry_activate;

    class->cancel = nautilus_location_entry_cancel;

    signals[CANCEL] = g_signal_new
                          ("cancel",
                          G_TYPE_FROM_CLASS (class),
                          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                          G_STRUCT_OFFSET (NautilusLocationEntryClass,
                                           cancel),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

    signals[LOCATION_CHANGED] = g_signal_new
                                    ("location-changed",
                                    G_TYPE_FROM_CLASS (class),
                                    G_SIGNAL_RUN_LAST, 0,
                                    NULL, NULL,
                                    g_cclosure_marshal_generic,
                                    G_TYPE_NONE, 1, G_TYPE_FILE);

    shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_Escape, 0),
                                 gtk_signal_action_new ("cancel"));
    gtk_widget_class_add_shortcut (GTK_WIDGET_CLASS (class), shortcut);
}

void
nautilus_location_entry_set_secondary_action (NautilusLocationEntry       *entry,
                                              NautilusLocationEntryAction  secondary_action)
{
    NautilusLocationEntryPrivate *priv;

    priv = nautilus_location_entry_get_instance_private (entry);

    if (priv->secondary_action == secondary_action)
    {
        return;
    }

    switch (secondary_action)
    {
        case NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR:
        {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               "edit-clear-symbolic");
            gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, _("Clear Entry"));
        }
        break;

        case NAUTILUS_LOCATION_ENTRY_ACTION_GOTO:
        {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               "go-next-symbolic");
            gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, _("Go to Location"));
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
    priv->secondary_action = secondary_action;
}

static void
editable_activate_callback (GtkEntry *entry,
                            gpointer  user_data)
{
    NautilusLocationEntry *self = user_data;
    const char *entry_text;
    g_autofree gchar *path = NULL;

    entry_text = gtk_editable_get_text (GTK_EDITABLE (entry));
    path = g_strdup (entry_text);
    path = g_strchug (path);
    path = g_strchomp (path);

    if (path != NULL && *path != '\0')
    {
        nautilus_location_entry_set_text (self, path);
        emit_location_changed (self);
    }
}

static void
editable_changed_callback (GtkEntry *entry,
                           gpointer  user_data)
{
    nautilus_location_entry_update_action (NAUTILUS_LOCATION_ENTRY (entry));
}

static void
nautilus_location_entry_init (NautilusLocationEntry *entry)
{
    NautilusLocationEntryPrivate *priv;
    GtkEventController *controller;

    priv = nautilus_location_entry_get_instance_private (entry);

    gtk_entry_set_input_purpose (GTK_ENTRY (entry), GTK_INPUT_PURPOSE_URL);
    gtk_entry_set_input_hints (GTK_ENTRY (entry), GTK_INPUT_HINT_NO_SPELLCHECK | GTK_INPUT_HINT_NO_EMOJI);

    nautilus_location_entry_set_secondary_action (entry,
                                                  NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);

    g_signal_connect (entry, "notify::has-focus",
                      G_CALLBACK (on_has_focus_changed), NULL);

    g_signal_connect (entry, "notify::text",
                      G_CALLBACK (nautilus_location_entry_text_changed), NULL);

    g_signal_connect (entry, "icon-release",
                      G_CALLBACK (nautilus_location_entry_icon_release), NULL);

    g_signal_connect_object (entry, "activate",
                             G_CALLBACK (editable_activate_callback), entry, G_CONNECT_AFTER);
    g_signal_connect_object (entry, "changed",
                             G_CALLBACK (editable_changed_callback), entry, 0);

    controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (entry), controller);
    /* In GTK3, the Tab key binding (for focus change) happens in the bubble
     * phase, and we want to stop that from happening. After porting to GTK4
     * we need to check whether this is still correct. */
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (nautilus_location_entry_key_pressed), NULL);

    g_signal_connect_after (gtk_editable_get_delegate (GTK_EDITABLE (entry)),
                            "insert-text",
                            G_CALLBACK (on_after_insert_text),
                            entry);
    g_signal_connect_after (gtk_editable_get_delegate (GTK_EDITABLE (entry)),
                            "delete-text",
                            G_CALLBACK (on_after_delete_text),
                            entry);

    priv->completion = gtk_entry_completion_new ();
    priv->completions_store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_entry_completion_set_model (priv->completion, GTK_TREE_MODEL (priv->completions_store));

    g_object_set (priv->completion,
                  "text-column", 0,
                  "inline-completion", FALSE,
                  "inline-selection", TRUE,
                  "popup-single-match", TRUE,
                  NULL);

    priv->completion_cell = gtk_cell_renderer_text_new ();
    g_object_set (priv->completion_cell, "xpad", 6, NULL);

    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->completion), priv->completion_cell, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->completion), priv->completion_cell, "text", 0);

    gtk_entry_set_completion (GTK_ENTRY (entry), priv->completion);
}

GtkWidget *
nautilus_location_entry_new (void)
{
    GtkWidget *entry;

    entry = GTK_WIDGET (g_object_new (NAUTILUS_TYPE_LOCATION_ENTRY, NULL));

    return entry;
}

void
nautilus_location_entry_set_special_text (NautilusLocationEntry *entry,
                                          const char            *special_text)
{
    NautilusLocationEntryPrivate *priv;

    priv = nautilus_location_entry_get_instance_private (entry);

    nautilus_location_entry_set_text (entry, special_text);
    priv->has_special_text = TRUE;
}
