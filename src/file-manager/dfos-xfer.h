/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* xfer.h - Bonobo::Desktop::FileOperationService transfer service.

   Copyright (C) 1999, 2000 Free Software Foundation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef _XFER_H
#define _XFER_H

#include <libgnomevfs/gnome-vfs.h>
#include "dfos.h"

void dfos_xfer (DFOS *dfos,
		const gchar *source_directory_uri,
		GList *source_file_name_list,
		const gchar *target_directory_uri,
		GList *target_file_name_list,
		GnomeVFSXferOptions options,
		GnomeVFSXferErrorMode error_mode,
		GnomeVFSXferOverwriteMode overwrite_mode);

void fs_xfer   (const GList *item_uris,
		const GdkPoint *target_item_points,
		const char *target_dir,
		int copy_action,
		GtkWidget *parent_view);

int file_operation_alert (GtkWidget *parent_view, 
			  const char *text, 
			  const char *title,
			  const char *button_1_text,
			  const char *button_2_text, 
			  const char *button_3_text);

#endif /* _XFER_H */
