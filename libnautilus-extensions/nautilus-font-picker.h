/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-font-picker.h - A simple widget to select scalable fonts.

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

#ifndef NAUTILUS_FONT_PICKER_H
#define NAUTILUS_FONT_PICKER_H

#include <gtk/gtkhbox.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_FONT_PICKER            (nautilus_font_picker_get_type ())
#define NAUTILUS_FONT_PICKER(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_FONT_PICKER, NautilusFontPicker))
#define NAUTILUS_FONT_PICKER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_FONT_PICKER, NautilusFontPickerClass))
#define NAUTILUS_IS_FONT_PICKER(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_FONT_PICKER))

typedef struct NautilusFontPicker	     NautilusFontPicker;
typedef struct NautilusFontPickerClass	     NautilusFontPickerClass;
typedef struct NautilusFontPickerDetails     NautilusFontPickerDetails;

struct NautilusFontPicker
{
	/* Super Class */
	GtkHBox hbox;
	
	/* Private stuff */
	NautilusFontPickerDetails *details;
};

struct NautilusFontPickerClass
{
	GtkHBoxClass parent_class;
};

GtkType    nautilus_font_picker_get_type                    (void);
GtkWidget* nautilus_font_picker_new                         (void);
char *     nautilus_font_picker_get_selected_font           (const NautilusFontPicker *font_picker);
void       nautilus_font_picker_set_selected_font           (NautilusFontPicker       *font_picker,
							     const char               *font);
void       nautilus_font_picker_set_title_label             (NautilusFontPicker       *font_picker,
							     const char               *title_label);
END_GNOME_DECLS

#endif /* NAUTILUS_FONT_PICKER_H */


