/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.h - interface for window that lets user choose 
                                a program from a list

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

   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_PROGRAM_CHOOSER_H
#define NAUTILUS_PROGRAM_CHOOSER_H

#include <gtk/gtkwindow.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "nautilus-file.h"
#include "nautilus-view-identifier.h"

GnomeDialog 		*nautilus_program_chooser_new 		  	  (GnomeVFSMimeActionType  type, 
					   			   	   NautilusFile 	  *file);

GnomeVFSMimeApplication *nautilus_program_chooser_get_application 	  (GnomeDialog 		  *program_chooser);
NautilusViewIdentifier  *nautilus_program_chooser_get_component   	  (GnomeDialog 		  *program_chooser);

void			 nautilus_program_chooser_show_no_choices_message (GnomeVFSMimeActionType action_type,
									   NautilusFile	  	  *file,
									   GtkWindow		  *parent_window);

#endif /* NAUTILUS_PROGRAM_CHOOSER_H */
