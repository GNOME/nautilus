/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Paolo Borelli <pborelli@katamail.com>
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "nautilus-trash-bar.h"

#include "nautilus-window.h"
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-trash-monitor.h>

#define NAUTILUS_TRASH_BAR_GET_PRIVATE(o)\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_TRASH_BAR, NautilusTrashBarPrivate))

enum {
	PROP_WINDOW = 1,
	NUM_PROPERTIES
};

struct NautilusTrashBarPrivate
{
	GtkWidget *empty_button;
	GtkWidget *restore_button;

	NautilusWindow *window;
	gulong selection_handler_id;
};

G_DEFINE_TYPE (NautilusTrashBar, nautilus_trash_bar, GTK_TYPE_HBOX);

static void
restore_button_clicked_cb (GtkWidget *button,
			   NautilusTrashBar *bar)
{
	GList *locations, *files, *l;

	locations = nautilus_window_info_get_selection (NAUTILUS_WINDOW_INFO  (bar->priv->window));
	files = NULL;

	for (l = locations; l != NULL; l = l->next) {
		files = g_list_prepend (files, nautilus_file_get (l->data));
	}

	nautilus_restore_files_from_trash (files, GTK_WINDOW (gtk_widget_get_toplevel (button)));

	nautilus_file_list_free (files);
	eel_g_object_list_free (locations);
}

static void
selection_changed_cb (NautilusWindow *window,
		      NautilusTrashBar *bar)
{
	int count;

	count = nautilus_window_info_get_selection_count (NAUTILUS_WINDOW_INFO (window));

	gtk_widget_set_sensitive (bar->priv->restore_button, (count > 0));
}

static void
connect_window_and_update_button (NautilusTrashBar *bar)
{
	bar->priv->selection_handler_id =
		g_signal_connect (bar->priv->window, "selection_changed",
				  G_CALLBACK (selection_changed_cb), bar);

	selection_changed_cb (bar->priv->window, bar);
}

static void
nautilus_trash_bar_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (object);

	switch (prop_id) {
	case PROP_WINDOW:
		bar->priv->window = g_value_get_object (value);
		connect_window_and_update_button (bar);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nautilus_trash_bar_finalize (GObject *obj)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (obj);

	if (bar->priv->selection_handler_id) {
		g_signal_handler_disconnect (bar->priv->window, bar->priv->selection_handler_id);
	}

	G_OBJECT_CLASS (nautilus_trash_bar_parent_class)->finalize (obj);
}

static void
nautilus_trash_bar_trash_state_changed (NautilusTrashMonitor *trash_monitor,
					gboolean              state,
					gpointer              data)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (data);

	gtk_widget_set_sensitive (bar->priv->empty_button,
				  !nautilus_trash_monitor_is_empty ());
}

static void
nautilus_trash_bar_class_init (NautilusTrashBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = nautilus_trash_bar_set_property;
	object_class->finalize = nautilus_trash_bar_finalize;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "window",
							      "the NautilusWindow",
							      NAUTILUS_TYPE_WINDOW,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (klass, sizeof (NautilusTrashBarPrivate));
}

static void
empty_trash_callback (GtkWidget *button, gpointer data)
{
	GtkWidget *window;
	
	window = gtk_widget_get_toplevel (button);

	nautilus_file_operations_empty_trash (window);
}

static void
nautilus_trash_bar_init (NautilusTrashBar *bar)
{
	GtkWidget *label;
	GtkWidget *hbox;

	bar->priv = NAUTILUS_TRASH_BAR_GET_PRIVATE (bar);

	hbox = GTK_WIDGET (bar);

	label = gtk_label_new (_("Trash"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (bar), label, FALSE, FALSE, 0);

	bar->priv->empty_button = gtk_button_new_with_mnemonic (_("Empty _Trash"));
	gtk_widget_show (bar->priv->empty_button);
	gtk_box_pack_end (GTK_BOX (hbox), bar->priv->empty_button, FALSE, FALSE, 0);

	gtk_widget_set_sensitive (bar->priv->empty_button,
				  !nautilus_trash_monitor_is_empty ());
	gtk_widget_set_tooltip_text (bar->priv->empty_button,
				     _("Delete all items in the Trash"));

	g_signal_connect (bar->priv->empty_button,
			  "clicked",
			  G_CALLBACK (empty_trash_callback),
			  bar);

	bar->priv->restore_button = gtk_button_new_with_mnemonic (_("Restore Selected Items"));
	gtk_widget_show (bar->priv->restore_button);
	gtk_box_pack_end (GTK_BOX (hbox), bar->priv->restore_button, FALSE, FALSE, 6);

	gtk_widget_set_sensitive (bar->priv->restore_button, FALSE);
	gtk_widget_set_tooltip_text (bar->priv->restore_button,
				     _("Restore selected items to their original position"));

	g_signal_connect (bar->priv->restore_button,
			  "clicked",
			  G_CALLBACK (restore_button_clicked_cb),
			  bar);

	g_signal_connect_object (nautilus_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (nautilus_trash_bar_trash_state_changed),
				 bar,
				 0);
}

GtkWidget *
nautilus_trash_bar_new (NautilusWindow *window)
{
	GObject *bar;

	bar = g_object_new (NAUTILUS_TYPE_TRASH_BAR, "window", window, NULL);

	return GTK_WIDGET (bar);
}
