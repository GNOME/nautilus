/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-string-picker.h - A widget to pick a string from a list.

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

#ifndef NAUTILUS_TEXT_CAPTION_H
#define NAUTILUS_TEXT_CAPTION_H

#include <libnautilus-extensions/nautilus-caption.h>

/*
 * NautilusTextCaption is made up of 2 widgets. 
 *
 * [title label] [string combo box]
 *
 * The user can select a string from the list.
 */
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_TEXT_CAPTION            (nautilus_text_caption_get_type ())
#define NAUTILUS_TEXT_CAPTION(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TEXT_CAPTION, NautilusTextCaption))
#define NAUTILUS_TEXT_CAPTION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TEXT_CAPTION, NautilusTextCaptionClass))
#define NAUTILUS_IS_TEXT_CAPTION(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TEXT_CAPTION))

typedef struct _NautilusTextCaption	       NautilusTextCaption;
typedef struct _NautilusTextCaptionClass      NautilusTextCaptionClass;
typedef struct _NautilusTextCaptionDetail     NautilusTextCaptionDetail;

struct _NautilusTextCaption
{
	/* Super Class */
	NautilusCaption			caption;
	
	/* Private stuff */
	NautilusTextCaptionDetail	*detail;
};

struct _NautilusTextCaptionClass
{
	NautilusCaptionClass		parent_class;
};

GtkType    nautilus_text_caption_get_type        (void);
GtkWidget* nautilus_text_caption_new             (void);

/* Entry text accesor. */
char *nautilus_text_caption_get_text        (NautilusTextCaption      *text_caption);

/* Entry text mutator. */
void  nautilus_text_caption_set_text        (NautilusTextCaption      *text_caption,
					     const char               *text);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_TEXT_CAPTION_H */


