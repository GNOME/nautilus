/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-header.h - A header widget for services views.

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

#ifndef EAZEL_SERVICES_HEADER_H
#define EAZEL_SERVICES_HEADER_H

#include <gtk/gtkhbox.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define EAZEL_SERVICES_TYPE_HEADER		(eazel_services_header_get_type ())
#define EAZEL_SERVICES_HEADER(obj)		(GTK_CHECK_CAST ((obj), EAZEL_SERVICES_TYPE_HEADER, EazelServicesHeader))
#define EAZEL_SERVICES_HEADER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EAZEL_SERVICES_TYPE_HEADER, EazelServicesHeaderClass))
#define EAZEL_SERVICES_IS_HEADER(obj)		(GTK_CHECK_TYPE ((obj), EAZEL_SERVICES_TYPE_HEADER))
#define EAZEL_SERVICES_IS_HEADER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), EAZEL_SERVICES_TYPE_HEADER))

typedef struct _EazelServicesHeader	      EazelServicesHeader;
typedef struct _EazelServicesHeaderClass      EazelServicesHeaderClass;
typedef struct _EazelServicesHeaderDetails    EazelServicesHeaderDetails;

struct _EazelServicesHeader
{
	/* Super Class */
	GtkHBox hbox;

	/* Private stuff */
	EazelServicesHeaderDetails *details;
};

struct _EazelServicesHeaderClass
{
	/* Super Class */
	GtkHBoxClass hbox_class;
};

GtkType    eazel_services_header_get_type       (void);
GtkWidget* eazel_services_header_title_new      (const char          *left_text);
GtkWidget* eazel_services_header_middle_new     (const char          *left_text,
						 const char          *right_text);
void       eazel_services_header_set_left_text  (EazelServicesHeader *header,
						 const char          *text);
void       eazel_services_header_set_right_text (EazelServicesHeader *header,
						 const char          *text);

END_GNOME_DECLS

#endif /* EAZEL_SERVICES_HEADER_H */


