/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-signaller.h: Class to manage file-manager-wide signals.
 
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
#include "fm-signaller.h"

#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-gtk-macros.h>

enum 
{
	ICON_TEXT_CHANGED,
	LAST_SIGNAL
};

static guint fm_signaller_signals[LAST_SIGNAL];

static void fm_signaller_initialize_class (gpointer klass);
static void fm_signaller_initialize (gpointer object, gpointer klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMSignaller, fm_signaller, GTK_TYPE_OBJECT)

static void
fm_signaller_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	fm_signaller_signals[ICON_TEXT_CHANGED] =
		gtk_signal_new ("icon_text_changed",
				GTK_RUN_LAST,
				object_class->type,
				0,
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, fm_signaller_signals, LAST_SIGNAL);
}

static void
fm_signaller_initialize (gpointer object, gpointer klass)
{
	/* placeholder to allow use of boilerplate macro */
}

FMSignaller *
fm_signaller_get_current (void)
{
	static FMSignaller *global_signaller = NULL;

	if (global_signaller == NULL)
	{
		global_signaller = gtk_type_new (FM_TYPE_SIGNALLER);
	}

	return global_signaller;
}