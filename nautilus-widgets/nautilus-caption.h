/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-caption.h - A captioned text widget

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_CAPTION_H
#define NAUTILUS_CAPTION_H

#include <gtk/gtkvbox.h>
#include <gnome.h>
#include <libnautilus-extensions/nautilus-string-list.h>

/*
 * NautilusCaption is made up of 2 widgets. 
 *
 * [title label] [something]
 *
 */
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_CAPTION            (nautilus_caption_get_type ())
#define NAUTILUS_CAPTION(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CAPTION, NautilusCaption))
#define NAUTILUS_CAPTION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CAPTION, NautilusCaptionClass))
#define NAUTILUS_IS_CAPTION(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CAPTION))

typedef struct _NautilusCaption		 NautilusCaption;
typedef struct _NautilusCaptionClass	 NautilusCaptionClass;
typedef struct _NautilusCaptionDetail	 NautilusCaptionDetail;

struct _NautilusCaption
{
	/* Super Class */
	GtkHBox			hbox;
	
	/* Private stuff */
	NautilusCaptionDetail	*detail;
};

struct _NautilusCaptionClass
{
	GtkHBoxClass		parent_class;
};

GtkType    nautilus_caption_get_type        (void);
GtkWidget* nautilus_caption_new             (void);

/* Title label mutator. */
void  nautilus_caption_set_title_label (NautilusCaption       *caption,
					const char            *title_label);

/* Title label accessor. */
char *nautilus_caption_get_title_label (const NautilusCaption *caption);

/* Set the child. */
void  nautilus_caption_set_child       (NautilusCaption       *caption,
					GtkWidget             *child);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_CAPTION_H */


