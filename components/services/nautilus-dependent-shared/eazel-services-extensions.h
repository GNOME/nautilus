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

#define EAZEL_SERVICES_LOGO_LEFT_SIDE_REPEAT_ICON	"eazel-logo-left-side-repeat.png"
#define EAZEL_SERVICES_LOGO_RIGHT_SIDE_ICON		"eazel-logo-right-side-logo.png"

#define EAZEL_SERVICES_NORMAL_FILL			"summary-service-normal-fill.png"
#define EAZEL_SERVICES_NORMAL_LEFT_BUMPER		"summary-service-normal-left-bumper.png"
#define EAZEL_SERVICES_NORMAL_RIGHT_BUMPER		"summary-service-normal-right-bumper.png"

#define EAZEL_SERVICES_PRELIGHT_FILL			"summary-service-prelight-fill.png"
#define EAZEL_SERVICES_PRELIGHT_LEFT_BUMPER		"summary-service-prelight-left-bumper.png"
#define EAZEL_SERVICES_PRELIGHT_RIGHT_BUMPER		"summary-service-prelight-right-bumper.png"

#define EAZEL_SERVICES_REMAINDER_LEFT_BUMPER		"summary-service-remainder-left-bumper.png"
#define EAZEL_SERVICES_REMAINDER_FILL			"summary-service-remainder-fill.png"
#define EAZEL_SERVICES_REMAINDER_RIGHT_BUMPER		"summary-service-remainder-right-bumper.png"

#define EAZEL_SERVICES_FONT_FAMILY			"helvetica"

BEGIN_GNOME_DECLS

GdkPixbuf *eazel_services_pixbuf_new              (const char *name);
GtkWidget *eazel_services_image_new               (const char *icon_name,
						   const char *tile_name,
						   guint32     background_color);
GtkWidget *eazel_services_label_new               (const char *text,
						   gboolean    bold,
						   guint       font_size,
						   gint        xpadding,
						   gint        ypadding,
						   guint       vertical_offset,
						   guint       horizontal_offset,
						   guint32     background_color,
						   const char *tile_name);
char *     eazel_services_get_current_date_string (void);

END_GNOME_DECLS

#endif /* EAZEL_SERVICES_EXTENSIONS_H */


