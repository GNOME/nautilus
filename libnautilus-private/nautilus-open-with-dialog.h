/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nautilus-open-with-dialog.c: an open-with dialog
 
   Copyright (C) 2004 Novell, Inc.
 
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

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef NAUTILUS_OPEN_WITH_DIALOG_H
#define NAUTILUS_OPEN_WITH_DIALOG_H

#include <gtk/gtkdialog.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_OPEN_WITH_DIALOG         (nautilus_open_with_dialog_get_type ())
#define NAUTILUS_OPEN_WITH_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_OPEN_WITH_DIALOG, NautilusOpenWithDialog))
#define NAUTILUS_OPEN_WITH_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_OPEN_WITH_DIALOG, NautilusOpenWithDialogClass))
#define NAUTILUS_IS_OPEN_WITH_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), NAUTILUS_TYPE_OPEN_WITH_DIALOG)

typedef struct _NautilusOpenWithDialog        NautilusOpenWithDialog;
typedef struct _NautilusOpenWithDialogClass   NautilusOpenWithDialogClass;
typedef struct _NautilusOpenWithDialogDetails NautilusOpenWithDialogDetails;

struct _NautilusOpenWithDialog {
	GtkDialog parent;
	NautilusOpenWithDialogDetails *details;
};

struct _NautilusOpenWithDialogClass {
	GtkDialogClass parent_class;

	void (*application_selected) (NautilusOpenWithDialog *dialog,
				      GAppInfo *application);
};

GType      nautilus_open_with_dialog_get_type (void);
GtkWidget* nautilus_open_with_dialog_new      (const char *uri,
					       const char *mime_type,
					       const char *extension);
GtkWidget* nautilus_add_application_dialog_new (const char *uri,
						const char *mime_type);
GtkWidget* nautilus_add_application_dialog_new_for_multiple_files (const char *extension,
								   const char *mime_type);



#endif /* NAUTILUS_OPEN_WITH_DIALOG_H */
