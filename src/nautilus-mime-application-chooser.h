
/*
   nautilus-mime-application-chooser.c: Manages applications for mime types
 
   Copyright (C) 2004 Novell, Inc.
 
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along application the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef NAUTILUS_MIME_APPLICATION_CHOOSER_H
#define NAUTILUS_MIME_APPLICATION_CHOOSER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER (nautilus_mime_application_chooser_get_type ())

G_DECLARE_FINAL_TYPE (NautilusMimeApplicationChooser, nautilus_mime_application_chooser, NAUTILUS, MIME_APPLICATION_CHOOSER, GtkBox)

GtkWidget * nautilus_mime_application_chooser_new (GList *files,
						   const char *mime_type);

G_END_DECLS

#endif /* NAUTILUS_MIME_APPLICATION_CHOOSER_H */
