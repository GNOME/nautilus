/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-extensions.h - Extensions to Nautilus and gtk widget.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EAZEL_SERVICES_EXTENSIONS_H
#define EAZEL_SERVICES_EXTENSIONS_H

#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>

#define EAZEL_SERVICES_BACKGROUND_COLOR_STRING		"white"
#define EAZEL_SERVICES_BACKGROUND_COLOR_RGBA		NAUTILUS_RGB_COLOR_WHITE
#define EAZEL_SERVICES_DROP_SHADOW_COLOR_RGBA		NAUTILUS_RGB_COLOR_BLACK
#define EAZEL_SERVICES_TEXT_COLOR_RGBA			NAUTILUS_RGB_COLOR_WHITE

#define EAZEL_SERVICES_HEADER_TITLE_FILL_ICON		"eazel-logo-left-side-repeat.png"
#define EAZEL_SERVICES_HEADER_TITLE_LOGO_ICON		"eazel-logo-right-side-logo.png"

#define EAZEL_SERVICES_NORMAL_FILL			"summary-service-normal-fill.png"
#define EAZEL_SERVICES_NORMAL_LEFT_BUMPER		"summary-service-normal-left-bumper.png"
#define EAZEL_SERVICES_NORMAL_RIGHT_BUMPER		"summary-service-normal-right-bumper.png"

#define EAZEL_SERVICES_PRELIGHT_FILL			"summary-service-prelight-fill.png"
#define EAZEL_SERVICES_PRELIGHT_LEFT_BUMPER		"summary-service-prelight-left-bumper.png"
#define EAZEL_SERVICES_PRELIGHT_RIGHT_BUMPER		"summary-service-prelight-right-bumper.png"

#define EAZEL_SERVICES_REMAINDER_LEFT_BUMPER		"summary-service-remainder-left-bumper.png"
#define EAZEL_SERVICES_REMAINDER_FILL			"summary-service-remainder-fill.png"
#define EAZEL_SERVICES_REMAINDER_RIGHT_BUMPER		"summary-service-remainder-right-bumper.png"

#define EAZEL_SERVICES_HEADER_MIDDLE_FILL_ICON		"eazel-services-header-middle-fill.png"

#define EAZEL_SERVICES_FONT_FAMILY			"helvetica"

#define EAZEL_SERVICES_FOOTER_FONT_SIZE			11
#define EAZEL_SERVICES_FOOTER_FONT_WEIGHT		"bold"

#define EAZEL_SERVICES_HEADER_TITLE_FONT_SIZE		18
#define EAZEL_SERVICES_HEADER_TITLE_FONT_WEIGHT		"bold"

#define EAZEL_SERVICES_HEADER_MIDDLE_FONT_SIZE		12
#define EAZEL_SERVICES_HEADER_MIDDLE_FONT_WEIGHT	"bold"

/*
 * X_PADDING: Amount of extra horizontal pad to give each item.  The x padding
 * is divided evenly on the left/right margins of the item.
 *
 * Y_PADDING: Amount of extra vertical pad to give each item.  The y padding
 * is divided evenly on the top/bottom margins of the item.
 *
 * VERTICAL_OFFSET: Positive or negative amount to offset the item vertically.
 *
 * HORIZONTAL_OFFSET: Positive or negative amount to offset the item horizontally.
 */
#define EAZEL_SERVICES_FOOTER_X_PADDING			2
#define EAZEL_SERVICES_FOOTER_Y_PADDING			0
#define EAZEL_SERVICES_FOOTER_VERTICAL_OFFSET		2
#define EAZEL_SERVICES_FOOTER_HORIZONTAL_OFFSET		0

#define EAZEL_SERVICES_FOOTER_DATE_X_PADDING		5
#define EAZEL_SERVICES_FOOTER_DATE_Y_PADDING		0
#define EAZEL_SERVICES_FOOTER_DATE_VERTICAL_OFFSET	2
#define EAZEL_SERVICES_FOOTER_DATE_HORIZONTAL_OFFSET	-5

#define EAZEL_SERVICES_HEADER_X_PADDING			10
#define EAZEL_SERVICES_HEADER_Y_PADDING			0
#define EAZEL_SERVICES_HEADER_VERTICAL_OFFSET		4
#define EAZEL_SERVICES_HEADER_HORIZONTAL_OFFSET		0

#define EAZEL_SERVICES_HEADER_TITLE_X_PADDING		10
#define EAZEL_SERVICES_HEADER_TITLE_Y_PADDING		0
#define EAZEL_SERVICES_HEADER_TITLE_VERTICAL_OFFSET	4
#define EAZEL_SERVICES_HEADER_TITLE_HORIZONTAL_OFFSET	0

#define EAZEL_SERVICES_HEADER_MIDDLE_LEFT_X_PADDING		12
#define EAZEL_SERVICES_HEADER_MIDDLE_LEFT_Y_PADDING		0
#define EAZEL_SERVICES_HEADER_MIDDLE_LEFT_VERTICAL_OFFSET	8
#define EAZEL_SERVICES_HEADER_MIDDLE_LEFT_HORIZONTAL_OFFSET	0

#define EAZEL_SERVICES_HEADER_MIDDLE_RIGHT_X_PADDING		76
#define EAZEL_SERVICES_HEADER_MIDDLE_RIGHT_Y_PADDING		0
#define EAZEL_SERVICES_HEADER_MIDDLE_RIGHT_VERTICAL_OFFSET	8
#define EAZEL_SERVICES_HEADER_MIDDLE_RIGHT_HORIZONTAL_OFFSET	-46

BEGIN_GNOME_DECLS

GdkPixbuf *eazel_services_pixbuf_new              (const char *name);
GtkWidget *eazel_services_image_new               (const char *icon_name,
						   const char *tile_name,
						   guint32     background_color);
GtkWidget *eazel_services_label_new               (const char *text,
						   guint       font_size,
						   const char *weight,
						   gint        xpadding,
						   gint        ypadding,
						   guint       vertical_offset,
						   guint       horizontal_offset,
						   guint32     background_color,
						   const char *tile_name);
char *     eazel_services_get_current_date_string (void);

END_GNOME_DECLS

#endif /* EAZEL_SERVICES_EXTENSIONS_H */


