/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.h - interface for window that lets user choose 
                                a program from a list

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>

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

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
*/

#ifndef NAUTILUS_PROGRAM_CHOOSER_H
#define NAUTILUS_PROGRAM_CHOOSER_H

#include <gtk/gtkdialog.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-view-identifier.h>

#define NAUTILUS_TYPE_PROGRAM_CHOOSER            (nautilus_program_chooser_get_type ())
#define NAUTILUS_PROGRAM_CHOOSER(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PROGRAM_CHOOSER, NautilusProgramChooser))
#define NAUTILUS_PROGRAM_CHOOSER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PROGRAM_CHOOSER, NautilusProgramChooserClass))
#define NAUTILUS_IS_PROGRAM_CHOOSER(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PROGRAM_CHOOSER))

typedef struct NautilusProgramChooser NautilusProgramChooser;
typedef struct NautilusProgramChooserClass NautilusProgramChooserClass;
typedef struct NautilusProgramChooserDetails NautilusProgramChooserDetails;

struct NautilusProgramChooser
{
	GtkDialog parent_instance;

	NautilusProgramChooserDetails *details;
};

struct NautilusProgramChooserClass
{
	GtkDialogClass parent_class;
};

GType                    nautilus_program_chooser_get_type                (void);
GtkWidget 		*nautilus_program_chooser_new 		  	  (GnomeVFSMimeActionType  type, 
					   			   	   NautilusFile 	  *file);

GnomeVFSMimeApplication *nautilus_program_chooser_get_application 	  (NautilusProgramChooser *program_chooser);
NautilusViewIdentifier  *nautilus_program_chooser_get_component   	  (NautilusProgramChooser *program_chooser);

void			 nautilus_program_chooser_show_no_choices_message (GnomeVFSMimeActionType action_type,
									   NautilusFile	  	  *file,
									   GtkWindow		  *parent_window);

void			 nautilus_program_chooser_show_invalid_message    (GnomeVFSMimeActionType  action_type,
									   NautilusFile           *file, 
									   GtkWindow              *parent_window);

#endif /* NAUTILUS_PROGRAM_CHOOSER_H */
