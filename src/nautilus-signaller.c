/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-signaller.h: Class to manage nautilus-wide signals that don't
   correspond to any particular object.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-signaller.h"

#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

enum 
{
	HISTORY_LIST_CHANGED,
	LAST_SIGNAL
};

static guint nautilus_signaller_signals[LAST_SIGNAL];

static void nautilus_signaller_initialize_class (gpointer klass);
static void nautilus_signaller_initialize (gpointer object, gpointer klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSignaller, nautilus_signaller, GTK_TYPE_OBJECT)

static void
nautilus_signaller_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	nautilus_signaller_signals[HISTORY_LIST_CHANGED] =
		gtk_signal_new ("history_list_changed",
				GTK_RUN_LAST,
				object_class->type,
				0,
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, nautilus_signaller_signals, LAST_SIGNAL);
}

static void
nautilus_signaller_initialize (gpointer object, gpointer klass)
{
	/* placeholder to allow use of boilerplate macro */
}

NautilusSignaller *
nautilus_signaller_get_current (void)
{
	static NautilusSignaller *global_signaller = NULL;

	if (global_signaller == NULL)
	{
		global_signaller = gtk_type_new (NAUTILUS_TYPE_SIGNALLER);
	}

	return global_signaller;
}
