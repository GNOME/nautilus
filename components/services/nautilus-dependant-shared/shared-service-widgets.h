

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *
 */

#ifndef SHARED_SERVICE_WIDGETS_H
#define SHARED_SERVICE_WIDGETS_H

#include <gnome.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus-extensions/nautilus-image.h>

#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR   "rgb:FFFF/FFFF/FFFF"

GtkWidget*	 create_image_widget			(const char			*icon_name,
							 const char			*background_color_spec,
							 NautilusImagePlacementType	placement);
GtkWidget*	 create_services_title_widget		(const char			*title_text);
GtkWidget*	 create_services_header_widget		(const char			*left_text,
							 const char			*right_text);
void		 set_widget_foreground_color		(GtkWidget			*widget,
							 const char			*color_spec);
void		 show_feedback				(GtkWidget			*widget,
							 char				*error_text);

#endif /* SHARED_SERVICE_WIDGETS_H */

