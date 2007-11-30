/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

/* nautilus-customization-data.h - functions to collect and load property
   names and imges */



#ifndef NAUTILUS_CUSTOMIZATION_DATA_H
#define NAUTILUS_CUSTOMIZATION_DATA_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtklabel.h>

#define RESET_IMAGE_NAME "reset.png"

typedef struct NautilusCustomizationData NautilusCustomizationData;



NautilusCustomizationData* nautilus_customization_data_new                          (const char *customization_name,
										     gboolean show_public_customizations,
										     int maximum_icon_height,
										     int maximum_icon_width);

/* Returns the following attrbiutes for a customization object (pattern, emblem)
 *
 * object_name   - The name of the object.  Usually what is used to represent it in storage.
 * object_pixbuf - Pixbuf for graphical display of the object. 
 * object_label  - Textual label display of the object. 
 */
gboolean                   nautilus_customization_data_get_next_element_for_display (NautilusCustomizationData *data,
										     char **object_name,
										     GdkPixbuf **object_pixbuf,
										     char **object_label);
gboolean                   nautilus_customization_data_private_data_was_displayed   (NautilusCustomizationData *data);

void                       nautilus_customization_data_destroy                      (NautilusCustomizationData *data);
									     


GdkPixbuf*                 nautilus_customization_make_pattern_chit                 (GdkPixbuf *pattern_tile, 
										     GdkPixbuf *frame, 
										     gboolean dragging,
										     gboolean is_reset);

#endif /* NAUTILUS_CUSTOMIZATION_DATA_H */
