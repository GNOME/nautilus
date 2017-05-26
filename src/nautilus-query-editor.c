/*
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *
 */

#include <config.h>
#include "nautilus-query-editor.h"
#include "nautilus-search-popover.h"
#include "nautilus-mime-actions.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgd/gd.h>

#include <eel/eel-glib-extensions.h>
#include "nautilus-file-utilities.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-global-preferences.h"

typedef struct
{
    GtkWidget *entry;
    GtkWidget *popover;
    GtkWidget *label;
    GtkWidget *dropdown_button;

    GdTaggedEntryTag *mime_types_tag;
    GdTaggedEntryTag *date_range_tag;

    gboolean change_frozen;

    GFile *location;

    NautilusQuery *query;
} NautilusQueryEditorPrivate;

enum
{
    ACTIVATED,
    CHANGED,
    CANCEL,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_LOCATION,
    PROP_QUERY,
    LAST_PROP
};

static guint signals[LAST_SIGNAL];

static void entry_activate_cb (GtkWidget           *entry,
                               NautilusQueryEditor *editor);
static void entry_changed_cb (GtkWidget           *entry,
                              NautilusQueryEditor *editor);
static void nautilus_query_editor_changed (NautilusQueryEditor *editor);

G_DEFINE_TYPE_WITH_PRIVATE (NautilusQueryEditor, nautilus_query_editor, GTK_TYPE_SEARCH_BAR);

static gboolean
settings_search_is_recursive (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    NautilusFile *file;
    gboolean recursive;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (!priv->location)
    {
        return TRUE;
    }

    file = nautilus_file_get (priv->location);

    if (nautilus_file_is_remote (file))
    {
        recursive = g_settings_get_enum (nautilus_preferences, "recursive-search") == NAUTILUS_SPEED_TRADEOFF_ALWAYS;
    }
    else
    {
        recursive = g_settings_get_enum (nautilus_preferences, "recursive-search") == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY ||
                    g_settings_get_enum (nautilus_preferences, "recursive-search") == NAUTILUS_SPEED_TRADEOFF_ALWAYS;
    }

    nautilus_file_unref (file);

    return recursive;
}

static void
update_information_label (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    gboolean fts_sensitive = TRUE;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (priv->location)
    {
        NautilusFile *file;
        gchar *label;
        gchar *uri;

        file = nautilus_file_get (priv->location);
        label = NULL;
        uri = g_file_get_uri (priv->location);

        if (nautilus_file_is_other_locations (file))
        {
            label = _("Searching locations only");
            fts_sensitive = FALSE;
        }
        else if (g_str_has_prefix (uri, "computer://"))
        {
            label = _("Searching devices only");
        }
        else if (g_str_has_prefix (uri, "network://"))
        {
            label = _("Searching network locations only");
            fts_sensitive = FALSE;
        }
        else if (nautilus_file_is_remote (file) &&
                 !settings_search_is_recursive (editor))
        {
            label = _("Remote location — only searching the current folder");
            fts_sensitive = FALSE;
        }
        else if (!settings_search_is_recursive (editor))
        {
            label = _("Only searching the current folder");
        }

        nautilus_search_popover_set_fts_sensitive (NAUTILUS_SEARCH_POPOVER (priv->popover),
                                                   fts_sensitive);

        gtk_widget_set_visible (priv->label, label != NULL);
        gtk_label_set_label (GTK_LABEL (priv->label), label);

        g_free (uri);
        nautilus_file_unref (file);
    }
}

static void
recursive_search_preferences_changed (GSettings           *settings,
                                      gchar               *key,
                                      NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    gboolean recursive;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (!priv->location || !priv->query)
    {
        return;
    }

    recursive = settings_search_is_recursive (editor);
    if (recursive != nautilus_query_get_recursive (priv->query))
    {
        nautilus_query_set_recursive (priv->query, recursive);
        nautilus_query_editor_changed (editor);
    }

    update_information_label (editor);
}


static void
nautilus_query_editor_dispose (GObject *object)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (object));

    g_clear_object (&priv->location);
    g_clear_object (&priv->query);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          recursive_search_preferences_changed,
                                          object);

    G_OBJECT_CLASS (nautilus_query_editor_parent_class)->dispose (object);
}

static void
nautilus_query_editor_grab_focus (GtkWidget *widget)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (widget));

    if (gtk_widget_get_visible (widget) && !gtk_widget_is_focus (priv->entry))
    {
        /* avoid selecting the entry text */
        gtk_widget_grab_focus (priv->entry);
        gtk_editable_set_position (GTK_EDITABLE (priv->entry), -1);
    }
}

static void
nautilus_query_editor_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (object));

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            g_value_set_object (value, priv->location);
        }
        break;

        case PROP_QUERY:
        {
            g_value_set_object (value, priv->query);
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_query_editor_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NautilusQueryEditor *self;

    self = NAUTILUS_QUERY_EDITOR (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            nautilus_query_editor_set_location (self, g_value_get_object (value));
        }
        break;

        case PROP_QUERY:
        {
            nautilus_query_editor_set_query (self, g_value_get_object (value));
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_query_editor_finalize (GObject *object)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (object));

    g_clear_object (&priv->date_range_tag);
    g_clear_object (&priv->mime_types_tag);

    G_OBJECT_CLASS (nautilus_query_editor_parent_class)->finalize (object);
}

static void
nautilus_query_editor_class_init (NautilusQueryEditorClass *class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = nautilus_query_editor_finalize;
    gobject_class->dispose = nautilus_query_editor_dispose;
    gobject_class->get_property = nautilus_query_editor_get_property;
    gobject_class->set_property = nautilus_query_editor_set_property;

    widget_class = GTK_WIDGET_CLASS (class);
    widget_class->grab_focus = nautilus_query_editor_grab_focus;

    signals[CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusQueryEditorClass, changed),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 2, NAUTILUS_TYPE_QUERY, G_TYPE_BOOLEAN);

    signals[CANCEL] =
        g_signal_new ("cancel",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (NautilusQueryEditorClass, cancel),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[ACTIVATED] =
        g_signal_new ("activated",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (NautilusQueryEditorClass, activated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /**
     * NautilusQueryEditor::location:
     *
     * The current location of the query editor.
     */
    g_object_class_install_property (gobject_class,
                                     PROP_LOCATION,
                                     g_param_spec_object ("location",
                                                          "Location of the search",
                                                          "The current location of the editor",
                                                          G_TYPE_FILE,
                                                          G_PARAM_READWRITE));

    /**
     * NautilusQueryEditor::query:
     *
     * The current query of the query editor. It it always synchronized
     * with the filter popover's query.
     */
    g_object_class_install_property (gobject_class,
                                     PROP_QUERY,
                                     g_param_spec_object ("query",
                                                          "Query of the search",
                                                          "The query that the editor is handling",
                                                          NAUTILUS_TYPE_QUERY,
                                                          G_PARAM_READWRITE));
}

GFile *
nautilus_query_editor_get_location (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_QUERY_EDITOR (editor), NULL);

    priv = nautilus_query_editor_get_instance_private (editor);

    return g_object_ref (priv->location);
}

static void
create_query (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    NautilusQuery *query;
    NautilusFile *file;
    gboolean recursive;
    gboolean fts_enabled;

    priv = nautilus_query_editor_get_instance_private (editor);

    g_return_if_fail (!priv->query);

    fts_enabled = nautilus_search_popover_get_fts_enabled (NAUTILUS_SEARCH_POPOVER (priv->popover));

    file = nautilus_file_get (priv->location);
    query = nautilus_query_new ();

    nautilus_query_set_search_content (query, fts_enabled);

    recursive = settings_search_is_recursive (editor);

    nautilus_query_set_text (query, gtk_entry_get_text (GTK_ENTRY (priv->entry)));
    nautilus_query_set_location (query, priv->location);
    nautilus_query_set_recursive (query, recursive);

    nautilus_query_editor_set_query (editor, query);

    nautilus_file_unref (file);
}

static void
entry_activate_cb (GtkWidget           *entry,
                   NautilusQueryEditor *editor)
{
    g_signal_emit (editor, signals[ACTIVATED], 0);
}

static void
entry_changed_cb (GtkWidget           *entry,
                  NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    gchar *text;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (priv->change_frozen || !gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (editor)))
    {
        return;
    }

    if (!priv->query)
    {
        create_query (editor);
    }
    text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry))));

    nautilus_query_set_text (priv->query, text);
    nautilus_query_editor_changed (editor);

    g_free (text);
}

static void
nautilus_query_editor_on_stop_search (GtkWidget           *entry,
                                      NautilusQueryEditor *editor)
{
    g_signal_emit (editor, signals[CANCEL], 0);
}

/* Type */

static void
nautilus_query_editor_init (NautilusQueryEditor *editor)
{
    g_signal_connect (nautilus_preferences,
                      "changed::recursive-search",
                      G_CALLBACK (recursive_search_preferences_changed),
                      editor);
}

static gboolean
entry_key_press_event_cb (GtkWidget           *widget,
                          GdkEventKey         *event,
                          NautilusQueryEditor *editor)
{
    if (event->keyval == GDK_KEY_Down)
    {
        gtk_widget_grab_focus (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
    }
    return FALSE;
}

static void
search_mode_changed_cb (GObject    *editor,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (editor));

    if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (editor)))
    {
        g_signal_emit (editor, signals[CANCEL], 0);
        gtk_widget_hide (priv->popover);
    }
}

static void
search_popover_date_range_changed_cb (NautilusSearchPopover *popover,
                                      GPtrArray             *date_range,
                                      NautilusQueryEditor   *editor)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (editor));
    if (!priv->query)
    {
        create_query (editor);
    }

    gd_tagged_entry_remove_tag (GD_TAGGED_ENTRY (priv->entry),
                                priv->date_range_tag);
    if (date_range)
    {
        g_autofree gchar *text_for_date_range = NULL;

        text_for_date_range = get_text_for_date_range (date_range, TRUE);
        gd_tagged_entry_tag_set_label (priv->date_range_tag,
                                       text_for_date_range);
        gd_tagged_entry_add_tag (GD_TAGGED_ENTRY (priv->entry),
                                 GD_TAGGED_ENTRY_TAG (priv->date_range_tag));
    }

    nautilus_query_set_date_range (priv->query, date_range);

    nautilus_query_editor_changed (editor);
}

static void
search_popover_mime_type_changed_cb (NautilusSearchPopover *popover,
                                     gint                   mimetype_group,
                                     const gchar           *mimetype,
                                     NautilusQueryEditor   *editor)
{
    NautilusQueryEditorPrivate *priv;
    GList *mimetypes = NULL;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (editor));
    if (!priv->query)
    {
        create_query (editor);
    }

    gd_tagged_entry_remove_tag (GD_TAGGED_ENTRY (priv->entry),
                                priv->mime_types_tag);
    /* group 0 is anything */
    if (mimetype_group == 0)
    {
        mimetypes = nautilus_mime_types_group_get_mimetypes (mimetype_group);
    }
    else if (mimetype_group > 0)
    {
        mimetypes = nautilus_mime_types_group_get_mimetypes (mimetype_group);
        gd_tagged_entry_tag_set_label (priv->mime_types_tag,
                                       nautilus_mime_types_group_get_name (mimetype_group));
        gd_tagged_entry_add_tag (GD_TAGGED_ENTRY (priv->entry),
                                 GD_TAGGED_ENTRY_TAG (priv->mime_types_tag));
    }
    else
    {
        g_autofree gchar *display_name = NULL;

        mimetypes = g_list_append (NULL, (gpointer) mimetype);
        display_name = g_content_type_get_description (mimetype);
        gd_tagged_entry_tag_set_label (priv->mime_types_tag, display_name);
        gd_tagged_entry_add_tag (GD_TAGGED_ENTRY (priv->entry),
                                 GD_TAGGED_ENTRY_TAG (priv->mime_types_tag));
    }
    nautilus_query_set_mime_types (priv->query, mimetypes);

    nautilus_query_editor_changed (editor);

    g_list_free (mimetypes);
}

static void
search_popover_time_type_changed_cb (NautilusSearchPopover   *popover,
                                     NautilusQuerySearchType  data,
                                     NautilusQueryEditor     *editor)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (editor));
    if (!priv->query)
    {
        create_query (editor);
    }

    nautilus_query_set_search_type (priv->query, data);

    nautilus_query_editor_changed (editor);
}

static void
search_popover_fts_changed_cb (GObject                    *popover,
                               GParamSpec                 *pspec,
                               gpointer                    user_data)
{
    NautilusQueryEditorPrivate *priv;
    NautilusQueryEditor *editor;

    editor = user_data;

    priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (editor));

    if (!priv->query)
    {
        create_query (editor);
    }

    nautilus_query_set_search_content (priv->query,
                                       nautilus_search_popover_get_fts_enabled (NAUTILUS_SEARCH_POPOVER (popover)));

    nautilus_query_editor_changed (editor);
}

static void
entry_tag_clicked (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    priv = nautilus_query_editor_get_instance_private (editor);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->dropdown_button),
                                  TRUE);
}

static void
entry_tag_close_button_clicked (NautilusQueryEditor *editor,
                                GdTaggedEntryTag    *tag)
{
    NautilusQueryEditorPrivate *priv;
    priv = nautilus_query_editor_get_instance_private (editor);

    if (tag == priv->mime_types_tag)
    {
        nautilus_search_popover_reset_mime_types (NAUTILUS_SEARCH_POPOVER (priv->popover));
    }
    else
    {
        nautilus_search_popover_reset_date_range (NAUTILUS_SEARCH_POPOVER (priv->popover));
    }
}

static void
setup_widgets (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;
    GtkWidget *hbox;
    GtkWidget *vbox;

    priv = nautilus_query_editor_get_instance_private (editor);

    /* vertical box that holds the search entry and the label below */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add (GTK_CONTAINER (editor), vbox);

    /* horizontal box */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class (gtk_widget_get_style_context (hbox), "linked");
    gtk_container_add (GTK_CONTAINER (vbox), hbox);

    /* create the search entry */
    priv->entry = GTK_WIDGET (gd_tagged_entry_new ());
    gtk_widget_set_size_request (GTK_WIDGET (priv->entry), 400, -1);
    gtk_search_bar_connect_entry (GTK_SEARCH_BAR (editor), GTK_ENTRY (priv->entry));

    gtk_container_add (GTK_CONTAINER (hbox), priv->entry);

    priv->mime_types_tag = gd_tagged_entry_tag_new (NULL);
    priv->date_range_tag = gd_tagged_entry_tag_new (NULL);

    g_signal_connect_swapped (priv->entry,
                              "tag-clicked",
                              G_CALLBACK (entry_tag_clicked),
                              editor);
    g_signal_connect_swapped (priv->entry,
                              "tag-button-clicked",
                              G_CALLBACK (entry_tag_close_button_clicked),
                              editor);

    /* additional information label */
    priv->label = gtk_label_new (NULL);
    gtk_widget_set_no_show_all (priv->label, TRUE);
    gtk_style_context_add_class (gtk_widget_get_style_context (priv->label), "dim-label");

    gtk_container_add (GTK_CONTAINER (vbox), priv->label);

    /* setup the search popover */
    priv->popover = nautilus_search_popover_new ();

    g_signal_connect (priv->popover, "show", (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (priv->popover, "closed", (GCallback) gtk_widget_grab_focus, editor);

    g_object_bind_property (editor, "query",
                            priv->popover, "query",
                            G_BINDING_DEFAULT);

    /* setup the filter menu button */
    priv->dropdown_button = gtk_menu_button_new ();
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (priv->dropdown_button), priv->popover);
    gtk_container_add (GTK_CONTAINER (hbox), priv->dropdown_button);

    g_signal_connect (editor, "notify::search-mode-enabled",
                      G_CALLBACK (search_mode_changed_cb), NULL);
    g_signal_connect (priv->entry, "key-press-event",
                      G_CALLBACK (entry_key_press_event_cb), editor);
    g_signal_connect (priv->entry, "activate",
                      G_CALLBACK (entry_activate_cb), editor);
    g_signal_connect (priv->entry, "search-changed",
                      G_CALLBACK (entry_changed_cb), editor);
    g_signal_connect (priv->entry, "stop-search",
                      G_CALLBACK (nautilus_query_editor_on_stop_search), editor);
    g_signal_connect (priv->popover, "date-range",
                      G_CALLBACK (search_popover_date_range_changed_cb), editor);
    g_signal_connect (priv->popover, "mime-type",
                      G_CALLBACK (search_popover_mime_type_changed_cb), editor);
    g_signal_connect (priv->popover, "time-type",
                      G_CALLBACK (search_popover_time_type_changed_cb), editor);
    g_signal_connect (priv->popover, "notify::fts-enabled",
                      G_CALLBACK (search_popover_fts_changed_cb), editor);

    /* show everything */
    gtk_widget_show_all (vbox);
}

static void
nautilus_query_editor_changed (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (priv->change_frozen)
    {
        return;
    }

    g_signal_emit (editor, signals[CHANGED], 0, priv->query, TRUE);
}

NautilusQuery *
nautilus_query_editor_get_query (NautilusQueryEditor *editor)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (editor == NULL || priv->entry == NULL || priv->query == NULL)
    {
        return NULL;
    }

    return g_object_ref (priv->query);
}

GtkWidget *
nautilus_query_editor_new (void)
{
    GtkWidget *editor;

    editor = g_object_new (NAUTILUS_TYPE_QUERY_EDITOR, NULL);
    setup_widgets (NAUTILUS_QUERY_EDITOR (editor));

    return editor;
}

void
nautilus_query_editor_set_location (NautilusQueryEditor *editor,
                                    GFile               *location)
{
    NautilusQueryEditorPrivate *priv;
    NautilusDirectory *directory;
    NautilusDirectory *base_model;
    gboolean should_notify;

    priv = nautilus_query_editor_get_instance_private (editor);

    /* The client could set us a location that is actually a search directory,
     * like what happens with the slot when updating the query editor location.
     * However here we want the real location used as a model for the search,
     * not the search directory invented uri. */
    directory = nautilus_directory_get (location);
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        GFile *real_location;

        base_model = nautilus_search_directory_get_base_model (NAUTILUS_SEARCH_DIRECTORY (directory));
        real_location = nautilus_directory_get_location (base_model);

        should_notify = g_set_object (&priv->location, real_location);

        g_object_unref (real_location);
    }
    else
    {
        should_notify = g_set_object (&priv->location, location);
    }

    if (!priv->query)
    {
        create_query (editor);
    }
    nautilus_query_set_location (priv->query, priv->location);

    update_information_label (editor);

    if (should_notify)
    {
        g_object_notify (G_OBJECT (editor), "location");
    }

    g_clear_object (&directory);
}

void
nautilus_query_editor_set_query (NautilusQueryEditor *editor,
                                 NautilusQuery       *query)
{
    NautilusQueryEditorPrivate *priv;
    char *text = NULL;
    char *current_text = NULL;

    priv = nautilus_query_editor_get_instance_private (editor);

    if (query != NULL)
    {
        text = nautilus_query_get_text (query);
    }

    if (!text)
    {
        text = g_strdup ("");
    }

    priv->change_frozen = TRUE;

    current_text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry))));
    if (!g_str_equal (current_text, text))
    {
        gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
    }
    g_free (current_text);

    if (g_set_object (&priv->query, query))
    {
        g_object_notify (G_OBJECT (editor), "query");
    }

    priv->change_frozen = FALSE;

    g_free (text);
}

void
nautilus_query_editor_set_text (NautilusQueryEditor *editor,
                                const gchar         *text)
{
    NautilusQueryEditorPrivate *priv;

    priv = nautilus_query_editor_get_instance_private (editor);

    /* The handler of the entry will take care of everything */
    gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
}
