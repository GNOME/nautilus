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

#ifndef NAUTILUS_STRING_PICKER_H
#define NAUTILUS_STRING_PICKER_H

#include <libnautilus-extensions/nautilus-caption.h>
#include <libnautilus-extensions/nautilus-string-list.h>

/*
 * NautilusStringPicker is made up of 2 widgets. 
 *
 * [title label] [string list]
 *
 * The user can select a string from the list.
 */
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_STRING_PICKER            (nautilus_string_picker_get_type ())
#define NAUTILUS_STRING_PICKER(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_STRING_PICKER, NautilusStringPicker))
#define NAUTILUS_STRING_PICKER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_STRING_PICKER, NautilusStringPickerClass))
#define NAUTILUS_IS_STRING_PICKER(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_STRING_PICKER))

typedef struct _NautilusStringPicker	       NautilusStringPicker;
typedef struct _NautilusStringPickerClass      NautilusStringPickerClass;
typedef struct _NautilusStringPickerDetail     NautilusStringPickerDetail;

struct _NautilusStringPicker
{
	/* Super Class */
	NautilusCaption			caption;
	
	/* Private stuff */
	NautilusStringPickerDetail	*detail;
};

struct _NautilusStringPickerClass
{
	NautilusCaptionClass		parent_class;
};

GtkType             nautilus_string_picker_get_type        (void);
GtkWidget*          nautilus_string_picker_new             (void);

/* Set the list of strings. */
void                nautilus_string_picker_set_string_list (NautilusStringPicker       *string_picker,
							    const NautilusStringList   *string_list);

/* Access a copy of the list of strings. */
NautilusStringList *nautilus_string_picker_get_string_list (const NautilusStringPicker *string_picker);

/* Entry text accesor. */
char *              nautilus_string_picker_get_text        (NautilusStringPicker       *string_picker);

/* Entry text mutator. */
void                nautilus_string_picker_set_text        (NautilusStringPicker       *string_picker,
							    const char                 *text);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_STRING_PICKER_H */


