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
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the nautilus about dialog
 *
 */

#ifndef NAUTILUS_ABOUT_H
#define NAUTILUS_ABOUT_H

#include <gdk/gdk.h>
#include <libgnomeui/gnome-dialog.h>

#define NAUTILUS_TYPE_ABOUT		(nautilus_about_get_type ())
#define NAUTILUS_ABOUT(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ABOUT, NautilusAbout))
#define NAUTILUS_ABOUT_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ABOUT, NautilusAboutClass))
#define NAUTILUS_IS_ABOUT(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ABOUT))
#define NAUTILUS_IS_ABOUT_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ABOUT))

typedef struct NautilusAbout NautilusAbout;
typedef struct NautilusAboutClass NautilusAboutClass;
typedef struct NautilusAboutDetails NautilusAboutDetails;

struct NautilusAbout {
	GnomeDialog parent;
	NautilusAboutDetails *details;
};

struct NautilusAboutClass {
	GnomeDialogClass parent_class;
};

GtkType    nautilus_about_get_type       (void);
GtkWidget *nautilus_about_new            (const char     *title,
					  const char     *version,
					  const char     *copyright,
					  const char    **authors,
					  const char     *comments,
					  const char	 *translators,
					  const char     *time_stamp);
void       nautilus_about_update_authors (NautilusAbout  *about);

#endif /* NAUTILUS_ABOUT_H */
