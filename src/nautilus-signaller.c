/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-signaller.h: Class to manage nautilus-wide signals that don't
 * correspond to any particular object.
 */

#include <config.h>
#include "nautilus-signaller.h"

#include <gtk/gtksignal.h>
#include <eel/eel-gtk-macros.h>

typedef GtkObject NautilusSignaller;
typedef GtkObjectClass NautilusSignallerClass;

enum {
	HISTORY_LIST_CHANGED,
	EMBLEMS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GtkObject *global_signaller = NULL;

static GtkType nautilus_signaller_get_type         (void);
static void    nautilus_signaller_initialize_class (gpointer klass);
static void    nautilus_signaller_initialize       (gpointer object,
						    gpointer klass);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSignaller,
				   nautilus_signaller,
				   GTK_TYPE_OBJECT)

static void
nautilus_signaller_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	signals[HISTORY_LIST_CHANGED] =
		gtk_signal_new ("history_list_changed",
				GTK_RUN_LAST,
				object_class->type,
				0,
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[EMBLEMS_CHANGED] =
		gtk_signal_new ("emblems_changed",
				GTK_RUN_LAST,
				object_class->type,
				0,
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_signaller_initialize (gpointer object, gpointer klass)
{
	/* placeholder to allow use of boilerplate macro */
}

static void
unref_global_signaller (void)
{
	gtk_object_unref (GTK_OBJECT (global_signaller));
}

GtkObject *
nautilus_signaller_get_current (void)
{
	if (global_signaller == NULL) {
		global_signaller = gtk_object_new (nautilus_signaller_get_type (), NULL);
		gtk_object_ref (GTK_OBJECT (global_signaller));
		gtk_object_sink (GTK_OBJECT (global_signaller));
		g_atexit (unref_global_signaller);
	}

	return global_signaller;
}
