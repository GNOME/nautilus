/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* xfer.c - Bonobo::Desktop::FileOperationService transfer service.

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
   
   Author: Ettore Perazzoli <ettore@gnu.org> */

#include <config.h>
#include "dfos.h"

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>


struct _XferInfo {
	GnomeVFSAsyncHandle *handle;
	GtkWidget *progress_dialog;
	GnomeVFSXferOptions options;
	GnomeVFSXferErrorMode error_mode;
	GnomeVFSXferOverwriteMode overwrite_mode;
};
typedef struct _XferInfo XferInfo;


static XferInfo *
xfer_info_new (GnomeVFSAsyncHandle *handle,
	       GnomeVFSXferOptions options,
	       GnomeVFSXferErrorMode error_mode,
	       GnomeVFSXferOverwriteMode overwrite_mode)
{
	XferInfo *new;

	new = g_new (XferInfo, 1);

	new->handle = handle;
	new->options = options;
	new->error_mode = error_mode;
	new->overwrite_mode = overwrite_mode;

	new->progress_dialog = NULL;

	return new;
}

static void
xfer_dialog_clicked_callback (DFOSXferProgressDialog *dialog,
			      gint button_number,
			      gpointer data)
{
	XferInfo *info;

	info = (XferInfo *) data;
	gnome_vfs_async_cancel (info->handle);

	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_warning (_("Operation cancelled"));
}

static void
create_xfer_dialog (const GnomeVFSXferProgressInfo *progress_info,
		    XferInfo *xfer_info)
{
	gchar *op_string;

	g_return_if_fail (xfer_info->progress_dialog == NULL);

	if (xfer_info->options & GNOME_VFS_XFER_REMOVESOURCE)
		op_string = _("Moving");
	else
		op_string = _("Copying");

	xfer_info->progress_dialog
		= dfos_xfer_progress_dialog_new ("Transfer in progress",
						 op_string,
						 progress_info->files_total,
						 progress_info->bytes_total);

	gtk_signal_connect (GTK_OBJECT (xfer_info->progress_dialog),
			    "clicked",
			    GTK_SIGNAL_FUNC (xfer_dialog_clicked_callback),
			    xfer_info);

	gtk_widget_show (xfer_info->progress_dialog);
}

static gint
handle_xfer_ok (const GnomeVFSXferProgressInfo *progress_info,
		XferInfo *xfer_info)
{
	switch (progress_info->phase) {
	case GNOME_VFS_XFER_PHASE_READYTOGO:
		create_xfer_dialog (progress_info, xfer_info);
		return TRUE;
	case GNOME_VFS_XFER_PHASE_XFERRING:
		if (progress_info->bytes_copied == 0) {
			dfos_xfer_progress_dialog_new_file
				(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 progress_info->source_name,
				 progress_info->target_name,
				 progress_info->file_size);
		} else {
			dfos_xfer_progress_dialog_update
				(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 progress_info->bytes_copied,
				 progress_info->total_bytes_copied);
		}
		return TRUE;
	case GNOME_VFS_XFER_PHASE_FILECOMPLETED:
		/* FIXME? */
		return TRUE;
	case GNOME_VFS_XFER_PHASE_COMPLETED:
		gtk_widget_destroy (xfer_info->progress_dialog);
		g_warning ("***** RELEASING HANDLE");
		g_free (xfer_info);
		return TRUE;
	default:
		return TRUE;
	}
}

static gint
handle_xfer_vfs_error (const GnomeVFSXferProgressInfo *progress_info,
		       XferInfo *xfer_info)
{
	/* Notice that the error mode in `xfer_info' is the one we have been
           requested, but the transfer is always performed in mode
           `GNOME_VFS_XFER_ERROR_MODE_QUERY'.  */

	switch (xfer_info->error_mode) {
	case GNOME_VFS_XFER_ERROR_MODE_QUERY: /* FIXME */
	case GNOME_VFS_XFER_ERROR_MODE_ABORT:
	default:
		dfos_xfer_progress_dialog_freeze (DFOS_XFER_PROGRESS_DIALOG
						  (xfer_info->progress_dialog));
		dfos_xfer_progress_dialog_thaw (DFOS_XFER_PROGRESS_DIALOG
						(xfer_info->progress_dialog));
		gtk_widget_destroy (xfer_info->progress_dialog);
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	}
}

static gint
handle_xfer_overwrite (const GnomeVFSXferProgressInfo *progress_info,
		       XferInfo *xfer_info)
{
	return FALSE;
}

static gint
xfer_callback (GnomeVFSAsyncHandle *handle,
	       const GnomeVFSXferProgressInfo *progress_info,
	       gpointer data)
{
	XferInfo *xfer_info;

	xfer_info = (XferInfo *) data;

	switch (progress_info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		return handle_xfer_ok (progress_info, xfer_info);
	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
		return handle_xfer_vfs_error (progress_info, xfer_info);
	case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
		return handle_xfer_overwrite (progress_info, xfer_info);
	default:
		g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
			   progress_info->status);
		return FALSE;
	}
}


void
dfos_xfer (DFOS *dfos,
	   const gchar *source_directory_uri,
	   GList *source_file_name_list,
	   const gchar *target_directory_uri,
	   GList *target_file_name_list,
	   GnomeVFSXferOptions options,
	   GnomeVFSXferErrorMode error_mode,
	   GnomeVFSXferOverwriteMode overwrite_mode)
{
	GnomeVFSResult result;
	XferInfo *xfer_info;
	GnomeVFSAsyncHandle *handle;

	xfer_info = xfer_info_new (handle, options, overwrite_mode, error_mode);

	result = gnome_vfs_async_xfer (&handle,
				       source_directory_uri,
				       source_file_name_list,
				       target_directory_uri,
				       target_file_name_list,
				       options,
				       GNOME_VFS_XFER_ERROR_MODE_QUERY,
				       overwrite_mode,
				       xfer_callback,
				       xfer_info);

	if (result != GNOME_VFS_OK) {
		gchar *message;
		GtkWidget *dialog;

		message = g_strdup_printf (_("The transfer between\n%s\nand\n%s\ncould not be started:\n%s"),
					   source_directory_uri,
					   target_directory_uri,
					   gnome_vfs_result_to_string (result));


		/* FIXME: signals and all that.  */
		dialog = gnome_error_dialog (message);

		gtk_widget_show (dialog);

		g_free (message);
		g_free (xfer_info);
	}
}
