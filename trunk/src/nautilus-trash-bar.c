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
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-trash-monitor.h>

#define NAUTILUS_TRASH_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_TRASH_BAR, NautilusTrashBarPrivate))

struct NautilusTrashBarPrivate
{
	GtkWidget   *button;
};

G_DEFINE_TYPE (NautilusTrashBar, nautilus_trash_bar, GTK_TYPE_HBOX)

static void
nautilus_trash_bar_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (object);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nautilus_trash_bar_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (object);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nautilus_trash_bar_trash_state_changed (NautilusTrashMonitor *trash_monitor,
					gboolean              state,
					gpointer              data)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (data);

	gtk_widget_set_sensitive (bar->priv->button,
				  !nautilus_trash_monitor_is_empty ());
}

static void
nautilus_trash_bar_class_init (NautilusTrashBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = nautilus_trash_bar_get_property;
	object_class->set_property = nautilus_trash_bar_set_property;

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

	bar->priv->button = gtk_button_new_with_mnemonic (_("Empty _Trash"));
	gtk_widget_show (bar->priv->button);
	gtk_box_pack_end (GTK_BOX (hbox), bar->priv->button, FALSE, FALSE, 0);

	gtk_widget_set_sensitive (bar->priv->button,
				  !nautilus_trash_monitor_is_empty ());
	gtk_widget_set_tooltip_text (bar->priv->button,
				     _("Delete all items in the Trash"));

	g_signal_connect (bar->priv->button,
			  "clicked",
			  G_CALLBACK (empty_trash_callback),
			  bar);

	g_signal_connect_object (nautilus_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (nautilus_trash_bar_trash_state_changed),
				 bar,
				 0);
}

GtkWidget *
nautilus_trash_bar_new (void)
{
	GObject *bar;

	bar = g_object_new (NAUTILUS_TYPE_TRASH_BAR, NULL);

	return GTK_WIDGET (bar);
}
