/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-background-box.c - an event box that renders an eel background

   Copyright (C) 2002 Sun Microsystems, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Dave Camp <dave@ximian.com>
*/

#include <config.h>
#include "eel-background-box.h"

#include "eel-gtk-macros.h"
#include "eel-background.h"

static void eel_background_box_class_init   (EelBackgroundBoxClass *background_box_class);
static void eel_background_box_init         (EelBackgroundBox      *background);

EEL_CLASS_BOILERPLATE (EelBackgroundBox, eel_background_box, GTK_TYPE_EVENT_BOX)

static gboolean
eel_background_box_expose_event (GtkWidget *widget,
				 GdkEventExpose *event)
{
	eel_background_expose (widget, event);
	
	gtk_container_propagate_expose (GTK_CONTAINER (widget), 
					GTK_BIN (widget)->child,
					event);
	
	return TRUE;
}

static void
eel_background_box_class_init (EelBackgroundBoxClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	
 	widget_class->expose_event = eel_background_box_expose_event;
}

static void
eel_background_box_init (EelBackgroundBox *box)
{
}

GtkWidget*
eel_background_box_new (void)
{
	EelBackgroundBox *background_box;

	background_box = EEL_BACKGROUND_BOX (gtk_widget_new (eel_background_box_get_type (), NULL));
	
	return GTK_WIDGET (background_box);
}
