/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-font-manager.h - Functions for managing fonts.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_FONT_MANAGER_H
#define NAUTILUS_FONT_MANAGER_H

#include <libgnome/gnome-defs.h>
#include <glib.h>

BEGIN_GNOME_DECLS

typedef enum {
	NAUTILUS_FONT_POSTSCRIPT = 1,
	NAUTILUS_FONT_TRUE_TYPE
} NautilusFontType;

/*
 * A callback which can be invoked for each font available in the system.
 */
typedef gboolean (*NautilusFontManagerCallback) (const char *font_file_name,
						 NautilusFontType font_type,
						 const char *foundry,
						 const char *family,
						 const char *weight,
						 const char *slant,
						 const char *set_width,
						 const char *char_set,
						 gpointer callback_data);

void     nautilus_font_manager_for_each_font         (NautilusFontManagerCallback  callback,
						      gpointer                     callback_data);
char *   nautilus_font_manager_get_default_font      (void);
char *   nautilus_font_manager_get_default_bold_font (void);
gboolean nautilus_font_manager_file_is_scalable_font (const char                  *file_name);
char *   nautilus_font_manager_get_bold              (const char                  *plain_font);
char *   nautilus_font_manager_get_italic            (const char                  *plain_font);
gboolean nautilus_font_manager_weight_is_bold        (const char                  *weight);

END_GNOME_DECLS

#endif /* NAUTILUS_FONT_MANAGER_H */


