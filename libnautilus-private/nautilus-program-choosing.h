/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-choosing.h - functions for selecting and activating
 				 programs for opening/viewing particular files.

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

#ifndef NAUTILUS_PROGRAM_CHOOSING_H
#define NAUTILUS_PROGRAM_CHOOSING_H

#include "nautilus-file.h"
#include "nautilus-view-identifier.h"
#include <gtk/gtkwindow.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

typedef void (*NautilusApplicationChoiceCallback) (GnomeVFSMimeApplication	 *application,
						   gpointer			  callback_data);
typedef void (*NautilusComponentChoiceCallback)   (NautilusViewIdentifier 	 *identifier,
						   gpointer		 	  callback_data);

void nautilus_choose_application_for_file        (NautilusFile                      *file,
						  GtkWindow                         *parent_window,
						  NautilusApplicationChoiceCallback  callback,
						  gpointer                           callback_data);
void nautilus_cancel_choose_application_for_file (NautilusFile                      *file,
						  NautilusComponentChoiceCallback    callback,
						  gpointer                           callback_data);
void nautilus_choose_component_for_file          (NautilusFile                      *file,
						  GtkWindow                         *parent_window,
						  NautilusComponentChoiceCallback    callback,
						  gpointer                           callback_data);
void nautilus_cancel_choose_component_for_file   (NautilusFile                      *file,
						  NautilusComponentChoiceCallback    callback,
						  gpointer                           callback_data);
void nautilus_launch_application                 (GnomeVFSMimeApplication           *application,
						  NautilusFile                      *file,
						  GtkWindow                         *parent_window);
void nautilus_launch_application_from_command    (const char                        *name,
						  const char                        *command_string,
						  const char                        *parameter,
						  gboolean                           use_terminal);

#endif /* NAUTILUS_PROGRAM_CHOOSING_H */
