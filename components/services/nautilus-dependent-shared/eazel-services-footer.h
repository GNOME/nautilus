/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-footer.h - A footer widget for services views.

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

#ifndef EAZEL_SERVICES_FOOTER_H
#define EAZEL_SERVICES_FOOTER_H

#include <gtk/gtkhbox.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define EAZEL_SERVICES_TYPE_FOOTER		(eazel_services_footer_get_type ())
#define EAZEL_SERVICES_FOOTER(obj)		(GTK_CHECK_CAST ((obj), EAZEL_SERVICES_TYPE_FOOTER, EazelServicesFooter))
#define EAZEL_SERVICES_FOOTER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EAZEL_SERVICES_TYPE_FOOTER, EazelServicesFooterClass))
#define EAZEL_SERVICES_IS_FOOTER(obj)		(GTK_CHECK_TYPE ((obj), EAZEL_SERVICES_TYPE_FOOTER))
#define EAZEL_SERVICES_IS_FOOTER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), EAZEL_SERVICES_TYPE_FOOTER))

typedef struct _EazelServicesFooter	      EazelServicesFooter;
typedef struct _EazelServicesFooterClass      EazelServicesFooterClass;
typedef struct _EazelServicesFooterDetails    EazelServicesFooterDetails;

struct _EazelServicesFooter
{
	/* Super Class */
	GtkHBox hbox;

	/* Private stuff */
	EazelServicesFooterDetails *details;
};

struct _EazelServicesFooterClass
{
	/* Super Class */
	GtkHBoxClass hbox_class;
};

GtkType    eazel_services_footer_get_type (void);
GtkWidget* eazel_services_footer_new      (void);
void       eazel_services_footer_update   (EazelServicesFooter *footer,
					   const char          *items[],
					   const char          *uris[],
					   guint                num_items);

END_GNOME_DECLS

#endif /* EAZEL_SERVICES_FOOTER_H */


