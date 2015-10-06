/* -*- Mode: C; indent-tabs-mode: s; c-basic-offset: 8; tab-width: 8 -*- */
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

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>

typedef struct {
	GtkWidget *entry;
        GtkWidget *popover;
        GtkWidget *label;
        GtkWidget *spinner;
	gboolean change_frozen;

        GBinding *spinner_active_binding;

	GFile *location;

	NautilusQuery *query;
} NautilusQueryEditorPrivate;

enum {
	ACTIVATED,
	CHANGED,
	CANCEL,
	LAST_SIGNAL
};

enum {
        PROP_0,
        PROP_LOCATION,
        PROP_QUERY,
        LAST_PROP
};

static guint signals[LAST_SIGNAL];

static void entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor);
static void entry_changed_cb  (GtkWidget *entry, NautilusQueryEditor *editor);
static void nautilus_query_editor_changed (NautilusQueryEditor *editor);

G_DEFINE_TYPE_WITH_PRIVATE (NautilusQueryEditor, nautilus_query_editor, GTK_TYPE_SEARCH_BAR);


static void
query_recursive_changed (GObject             *object,
                         GParamSpec          *pspec,
                         NautilusQueryEditor *editor)
{
        NautilusQueryEditorPrivate *priv;
        gchar *key;

        priv = nautilus_query_editor_get_instance_private (editor);
        key = "enable-recursive-search";

        if (priv->location) {
                NautilusFile *file;

                file = nautilus_file_get (priv->location);

                if (!nautilus_file_is_local (file)) {
                        key = "enable-remote-recursive-search";
		}

                nautilus_file_unref (file);
        }

        g_settings_set_boolean (nautilus_preferences,
                                key,
                                nautilus_query_get_recursive (NAUTILUS_QUERY (object)));
}


static void
nautilus_query_editor_dispose (GObject *object)
{
	NautilusQueryEditorPrivate *priv;

        priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (object));

        g_clear_object (&priv->location);
        g_clear_object (&priv->query);

	G_OBJECT_CLASS (nautilus_query_editor_parent_class)->dispose (object);
}

static void
nautilus_query_editor_grab_focus (GtkWidget *widget)
{
	NautilusQueryEditorPrivate *priv;

        priv = nautilus_query_editor_get_instance_private (NAUTILUS_QUERY_EDITOR (widget));

	if (gtk_widget_get_visible (widget) && !gtk_widget_is_focus (priv->entry)) {
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
                g_value_set_object (value, priv->location);
                break;

        case PROP_QUERY:
                g_value_set_object (value, priv->query);
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
                nautilus_query_editor_set_location (self, g_value_get_object (value));
                break;

        case PROP_QUERY:
                nautilus_query_editor_set_query (self, g_value_get_object (value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
nautilus_query_editor_class_init (NautilusQueryEditorClass *class)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS (class);
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

static NautilusQuery*
create_and_get_query (NautilusQueryEditor *editor)
{
        NautilusQueryEditorPrivate *priv;

        priv = nautilus_query_editor_get_instance_private (editor);

        if (!priv->query) {
                NautilusQuery *query;
                NautilusFile *file;
                gboolean recursive;

                file = nautilus_file_get (priv->location);
                query = nautilus_query_new ();

                if (nautilus_file_is_remote (file)) {
                        recursive = g_settings_get_boolean (nautilus_preferences,
                                                            "enable-remote-recursive-search");
                } else {
                        recursive = g_settings_get_boolean (nautilus_preferences,
                                                            "enable-recursive-search");
                }

                nautilus_query_set_text (query, gtk_entry_get_text (GTK_ENTRY (priv->entry)));
                nautilus_query_set_location (query, priv->location);
                nautilus_query_set_recursive (query, recursive);

                nautilus_query_editor_set_query (editor, query);

                g_signal_connect (query,
                                  "notify::recursive",
                                  G_CALLBACK (query_recursive_changed),
                                  editor);


                nautilus_file_unref (file);
        }

        return priv->query;
}

static void
entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	g_signal_emit (editor, signals[ACTIVATED], 0);
}

static void
entry_changed_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
        NautilusQueryEditorPrivate *priv;
        NautilusQuery *query;
        gchar *text;

        priv = nautilus_query_editor_get_instance_private (editor);

	if (priv->change_frozen || !gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (editor))) {
		return;
	}

        text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry))));
        query = create_and_get_query (editor);

        nautilus_query_set_text (query, text);
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
}

static gboolean
entry_key_press_event_cb (GtkWidget           *widget,
			  GdkEventKey         *event,
			  NautilusQueryEditor *editor)
{
	if (event->keyval == GDK_KEY_Down) {
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

        if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (editor))) {
                g_signal_emit (editor, signals[CANCEL], 0);
                gtk_widget_hide (priv->popover);
        }
}

static void
search_popover_changed_cb (NautilusSearchPopover *popover,
                           NautilusSearchFilter   filter,
                           gpointer               data,
                           NautilusQueryEditor   *editor)
{
        NautilusQuery *query;

        query = create_and_get_query (editor);

        switch (filter) {
        case NAUTILUS_SEARCH_FILTER_DATE:
                nautilus_query_set_date (query, data);
                break;

        case NAUTILUS_SEARCH_FILTER_TYPE:
                nautilus_query_set_mime_types (query, data);
                break;

        default:
                break;
        }

        nautilus_query_editor_changed (editor);
}

static void
setup_widgets (NautilusQueryEditor *editor)
{
        NautilusQueryEditorPrivate *priv;
	GtkWidget *button, *hbox, *vbox;

        priv = nautilus_query_editor_get_instance_private (editor);

        /* Spinner for when the search is happening */
        priv->spinner = gtk_spinner_new ();
        gtk_widget_set_margin_end (priv->spinner, 18);
        gtk_widget_show (priv->spinner);

        /* HACK: we're invasively adding the spinner to GtkSearchBar > GtkRevealer > GtkBox */
        gtk_box_pack_end (GTK_BOX (gtk_bin_get_child (GTK_BIN (gtk_bin_get_child (GTK_BIN (editor))))), priv->spinner, FALSE, TRUE, 0);

        /* vertical box that holds the search entry and the label below */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (editor), vbox);

        /* horizontal box */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (hbox), "linked");
	gtk_container_add (GTK_CONTAINER (vbox), hbox);

	/* create the search entry */
	priv->entry = gtk_search_entry_new ();
        gtk_entry_set_width_chars (GTK_ENTRY (priv->entry), 40);
        gtk_search_bar_connect_entry (GTK_SEARCH_BAR (editor), GTK_ENTRY (priv->entry));

        gtk_container_add (GTK_CONTAINER (hbox), priv->entry);

        /* additional information label */
        priv->label = gtk_label_new (NULL);
        gtk_widget_set_no_show_all (priv->label, TRUE);
        gtk_style_context_add_class (gtk_widget_get_style_context (priv->label), "dim-label");

	gtk_container_add (GTK_CONTAINER (vbox), priv->label);

        /* setup the search popover */
        priv->popover = nautilus_search_popover_new ();

        g_object_bind_property (editor, "location",
                                priv->popover, "location",
                                G_BINDING_DEFAULT);

        g_object_bind_property (editor, "query",
                                priv->popover, "query",
                                G_BINDING_DEFAULT);

        /* setup the filter menu button */
        button = gtk_menu_button_new ();
        gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), priv->popover);
        gtk_container_add (GTK_CONTAINER (hbox), button);

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
        g_signal_connect (priv->popover, "changed",
                          G_CALLBACK (search_popover_changed_cb), editor);

	/* show everything */
	gtk_widget_show_all (vbox);
}

static void
nautilus_query_editor_changed (NautilusQueryEditor *editor)
{
        NautilusQueryEditorPrivate *priv;

        priv = nautilus_query_editor_get_instance_private (editor);

        if (priv->change_frozen) {
		return;
	}

	g_signal_emit (editor, signals[CHANGED], 0, priv->query, TRUE);
}

NautilusQuery *
nautilus_query_editor_get_query (NautilusQueryEditor *editor)
{
        NautilusQueryEditorPrivate *priv;

        priv = nautilus_query_editor_get_instance_private (editor);

        if (editor == NULL || priv->entry == NULL || priv->query == NULL) {
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
        should_notify = FALSE;

        /* The client could set us a location that is actually a search directory,
         * like what happens with the slot when updating the query editor location.
         * However here we want the real location used as a model for the search,
         * not the search directory invented uri. */
        directory = nautilus_directory_get (location);
        if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
                GFile *real_location;

                base_model = nautilus_search_directory_get_base_model (NAUTILUS_SEARCH_DIRECTORY (directory));
                real_location = nautilus_directory_get_location (base_model);

                should_notify = g_set_object (&priv->location, real_location);

                g_object_unref (real_location);
        } else {
                should_notify = g_set_object (&priv->location, location);
        }

        /* Update label if needed */
        if (priv->location) {
                NautilusFile *file;
                gchar *label;
                gchar *uri;

                file = nautilus_file_get (priv->location);
                label = NULL;
                uri = g_file_get_uri (priv->location);

                if (nautilus_file_is_other_locations (file)) {
                        label = _("Searching locations only");
                } else if (g_str_has_prefix (uri, "computer://")) {
                        label = _("Searching devices only");
                } else if (g_str_has_prefix (uri, "network://")) {
                        label = _("Searching network locations only");
                } else if (!nautilus_file_is_local (file)) {
                        label = _("Remote location - only searching the current folder");
                }

		gtk_widget_set_visible (priv->label, label != NULL);
		gtk_label_set_label (GTK_LABEL (priv->label), label);

      		g_free (uri);
        }

        if (should_notify) {
                g_object_notify (G_OBJECT (editor), "location");
        }

        g_clear_object (&directory);

}

void
nautilus_query_editor_set_query (NautilusQueryEditor	*editor,
				 NautilusQuery		*query)
{
        NautilusQueryEditorPrivate *priv;
	char *text = NULL;
	char *current_text = NULL;

        priv = nautilus_query_editor_get_instance_private (editor);

	if (query != NULL) {
		text = nautilus_query_get_text (query);
	}

	if (!text) {
		text = g_strdup ("");
	}

	priv->change_frozen = TRUE;

	current_text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry))));
	if (!g_str_equal (current_text, text)) {
		gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
	}
	g_free (current_text);

        /* Clear bindings */
        g_clear_pointer (&priv->spinner_active_binding, g_binding_unbind);

        if (g_set_object (&priv->query, query)) {
                if (query) {
		        priv->spinner_active_binding = g_object_bind_property (query, "searching",
                                                                               priv->spinner, "active",
                                                                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
		}

                g_object_notify (G_OBJECT (editor), "query");
        }

	priv->change_frozen = FALSE;

	g_free (text);
}
