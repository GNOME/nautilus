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
   
   Authors: 
   Ettore Perazzoli <ettore@gnu.org> 
   Pavel Cisler <pavel@eazel.com> 
   */

#include <config.h>

#include <gnome.h>
#include "dfos-xfer.h"
#include "libnautilus-extensions/nautilus-file-changes-queue.h"
#include "fm-directory-view.h"


typedef struct XferInfo {
	GnomeVFSAsyncHandle *handle;
	GtkWidget *progress_dialog;
	const char *operation_name;
	const char *preparation_name;
	GnomeVFSXferErrorMode error_mode;
	GnomeVFSXferOverwriteMode overwrite_mode;
	GtkWidget *parent_view;
} XferInfo;


static XferInfo *
xfer_info_new (GnomeVFSAsyncHandle *handle,
	       GnomeVFSXferOptions options,
	       GnomeVFSXferErrorMode error_mode,
	       GnomeVFSXferOverwriteMode overwrite_mode)
{
	XferInfo *info;

	info = g_new (XferInfo, 1);

	info->handle = handle;
	info->error_mode = error_mode;
	info->overwrite_mode = overwrite_mode;

	info->progress_dialog = NULL;
	info->parent_view = NULL;

	return info;
}

static void
xfer_dialog_clicked_callback (DFOSXferProgressDialog *dialog,
			      int button_number,
			      gpointer data)
{
	XferInfo *info;

	info = (XferInfo *) data;
	gnome_vfs_async_cancel (info->handle);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
create_xfer_dialog (const GnomeVFSXferProgressInfo *progress_info,
		    XferInfo *xfer_info)
{
	g_return_if_fail (xfer_info->progress_dialog == NULL);

	xfer_info->progress_dialog
		= dfos_xfer_progress_dialog_new ("Transfer in progress",
						 "",
						 1,
						 1);

	gtk_signal_connect (GTK_OBJECT (xfer_info->progress_dialog),
			    "clicked",
			    GTK_SIGNAL_FUNC (xfer_dialog_clicked_callback),
			    xfer_info);

	gtk_widget_show (xfer_info->progress_dialog);
}

static int
handle_xfer_ok (const GnomeVFSXferProgressInfo *progress_info,
		XferInfo *xfer_info)
{
	switch (progress_info->phase) {
	case GNOME_VFS_XFER_PHASE_INITIAL:
		create_xfer_dialog (progress_info, xfer_info);
		return TRUE;

	case GNOME_VFS_XFER_PHASE_COLLECTING:
		dfos_xfer_progress_dialog_set_operation_string
			(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 xfer_info->preparation_name);
		return TRUE;

	case GNOME_VFS_XFER_PHASE_READYTOGO:
		dfos_xfer_progress_dialog_set_operation_string
			(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 xfer_info->operation_name);
		dfos_xfer_progress_dialog_set_total
			(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 progress_info->files_total,
				 progress_info->bytes_total);
		return TRUE;
				 
	case GNOME_VFS_XFER_PHASE_MOVING:
	case GNOME_VFS_XFER_PHASE_DELETESOURCE:
	case GNOME_VFS_XFER_PHASE_OPENSOURCE:
	case GNOME_VFS_XFER_PHASE_OPENTARGET:
		nautilus_file_changes_consume_changes (FALSE);
		/* fall through */

	case GNOME_VFS_XFER_PHASE_COPYING:
		if (progress_info->bytes_copied == 0) {
			dfos_xfer_progress_dialog_new_file
				(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 progress_info->source_name,
				 progress_info->target_name,
				 progress_info->file_index,
				 progress_info->file_size);
		} else {
			dfos_xfer_progress_dialog_update
				(DFOS_XFER_PROGRESS_DIALOG
				 (xfer_info->progress_dialog),
				 MIN (progress_info->bytes_copied, 
				 	progress_info->bytes_total),
				 MIN (progress_info->total_bytes_copied,
				 	progress_info->bytes_total));
		}
		return TRUE;

	case GNOME_VFS_XFER_PHASE_FILECOMPLETED:
		/* FIXME? */
		return TRUE;
		
	case GNOME_VFS_XFER_PHASE_COMPLETED:
		nautilus_file_changes_consume_changes (TRUE);
		gtk_widget_destroy (xfer_info->progress_dialog);
		g_free (xfer_info);
		return TRUE;

	default:
		return TRUE;
	}
}

static int
handle_xfer_vfs_error (const GnomeVFSXferProgressInfo *progress_info,
		       XferInfo *xfer_info)
{
	/* Notice that the error mode in `xfer_info' is the one we have been
           requested, but the transfer is always performed in mode
           `GNOME_VFS_XFER_ERROR_MODE_QUERY'.  */

	int result;
	char *text;

	switch (xfer_info->error_mode) {
	case GNOME_VFS_XFER_ERROR_MODE_QUERY:

		/* transfer error, prompt the user to continue or stop */
		text = g_strdup_printf ( _("Error %s copying file %s. \n"
			"Would you like to continue?"), 
			gnome_vfs_result_to_string (progress_info->vfs_status),
			progress_info->source_name);
		result = file_operation_alert (xfer_info->parent_view, text, 
			_("File copy error"),
			_("Skip"), _("Retry"), _("Stop"));

		switch (result) {
		case 0:
			return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
		case 1:
			return GNOME_VFS_XFER_ERROR_ACTION_RETRY;
		case 2:
			return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
		}						

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

static int
handle_xfer_overwrite (const GnomeVFSXferProgressInfo *progress_info,
		       XferInfo *xfer_info)
{
	/* transfer conflict, prompt the user to replace or skip */
	int result;
	char *text;

	text = g_strdup_printf ( _("File %s already exists. \n"
		"Would you like to replace it?"), 
		progress_info->target_name);
	result = file_operation_alert (xfer_info->parent_view, text, 
		_("File copy conflict"),
		_("Replace All"), _("Replace"), _("Skip"));

	switch (result) {
	case 0:
		return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE_ALL;
	case 1:
		return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
	case 2:
		return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
	}

	return 0;					
}

static int
handle_xfer_duplicate (GnomeVFSXferProgressInfo *progress_info,
		       XferInfo *xfer_info)
{
	char *old_name = progress_info->duplicate_name;

	if (progress_info->duplicate_count < 2) {
		progress_info->duplicate_name = g_strdup_printf ("%s (copy)", 
			progress_info->duplicate_name);
	} else {
		progress_info->duplicate_name = g_strdup_printf ("%s (copy %d)", 
			progress_info->duplicate_name,
			progress_info->duplicate_count);
	}
	
	g_free (old_name);

	return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
}

static int
update_xfer_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSXferProgressInfo *progress_info,
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
	case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
		return handle_xfer_duplicate (progress_info, xfer_info);
	default:
		g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
			   progress_info->status);
		return 0;
	}
}

/* Low-level callback, called for every copy engine operation.
 * Generates notifications about new, deleted and moved files.
 */
static int
sync_xfer_callback (GnomeVFSXferProgressInfo *progress_info, gpointer data)
{
	XferInfo *xfer_info;

	xfer_info = (XferInfo *) data;

	if (progress_info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OK) {
		switch (progress_info->phase) {
		case GNOME_VFS_XFER_PHASE_OPENTARGET:
			nautilus_file_changes_queue_file_added (progress_info->target_name);
			break;

		case GNOME_VFS_XFER_PHASE_MOVING:
			nautilus_file_changes_queue_file_moved (progress_info->source_name,
				progress_info->target_name);
			break;

		case GNOME_VFS_XFER_PHASE_DELETESOURCE:
			nautilus_file_changes_queue_file_removed (progress_info->source_name);
			break;

		default:
			break;
		}
	}
	return 1;
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

	xfer_info = xfer_info_new (NULL, options, overwrite_mode, error_mode);

	result = gnome_vfs_async_xfer (&xfer_info->handle,
				       source_directory_uri,
				       source_file_name_list,
				       target_directory_uri,
				       target_file_name_list,
				       options,
				       GNOME_VFS_XFER_ERROR_MODE_QUERY,
				       overwrite_mode,
				       update_xfer_callback,
				       xfer_info,
				       NULL, NULL);

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

int
file_operation_alert (GtkWidget *parent_view, const char *text, 
			const char *title, const char *button_1_text,
			const char *button_2_text, const char *button_3_text)
{
        GtkWidget *dialog;
        GtkWidget *prompt_widget;
        GtkWidget *top_widget;

        /* Don't use GNOME_STOCK_BUTTON_CANCEL because the
         * red X is confusing in this context.
         */

        dialog = gnome_dialog_new (title,
                                   button_1_text,
                                   button_2_text,
                                   button_3_text,
                                   NULL);

        gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
        gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);

        top_widget = gtk_widget_get_toplevel (parent_view);
        g_assert (GTK_IS_WINDOW (top_widget));
        gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (top_widget));

        prompt_widget = gtk_label_new (text);
        
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
                            prompt_widget,
                            TRUE, TRUE, GNOME_PAD);

        gtk_widget_show_all (dialog);

        return gnome_dialog_run (GNOME_DIALOG (dialog));
}

void
fs_xfer (const GList *item_uris,
	 const GdkPoint *relative_item_points,
	 const char *target_dir,
	 int copy_action,
	 GtkWidget *view)
{
	const GList *p;
	GList *item_names;
	GnomeVFSXferOptions move_options;
	gchar *source_dir;
	GnomeVFSURI *source_dir_uri;
	GnomeVFSURI *target_dir_uri;
	XferInfo *xfer_info;
	const char *target_dir_uri_text;
	
	g_assert (item_uris != NULL);
	
	item_names = NULL;
	source_dir_uri = NULL;


	/* convert URI list to a source parent URI and a list of names */
	for (p = item_uris; p != NULL; p = p->next) {
		GnomeVFSURI *item_uri;
		const gchar *item_name;
		
		item_uri = gnome_vfs_uri_new (p->data);
		item_name = gnome_vfs_uri_get_basename (item_uri);
		item_names = g_list_append (item_names, g_strdup (item_name));
		if (source_dir_uri == NULL)
			source_dir_uri = gnome_vfs_uri_get_parent (item_uri);

		gnome_vfs_uri_unref (item_uri);
	}

	source_dir = gnome_vfs_uri_to_string (source_dir_uri, GNOME_VFS_URI_HIDE_NONE);
	if (target_dir != NULL) {
		target_dir_uri = gnome_vfs_uri_new (target_dir);
		target_dir_uri_text = target_dir;
	} else {
		/* assume duplication */
		target_dir_uri = gnome_vfs_uri_ref (source_dir_uri);
		target_dir_uri_text = gnome_vfs_uri_to_string (target_dir_uri,
							       GNOME_VFS_URI_HIDE_NONE);
	}
	/* figure out the right move/copy mode */
	move_options = GNOME_VFS_XFER_RECURSIVE;

	if (gnome_vfs_uri_equal (target_dir_uri, source_dir_uri)) {
		g_assert (copy_action == GDK_ACTION_COPY);
		move_options |= GNOME_VFS_XFER_USE_UNIQUE_NAMES;
	} else if (copy_action == GDK_ACTION_MOVE) {
		move_options |= GNOME_VFS_XFER_REMOVESOURCE;
	}

	/* set up the copy/move parameters */
	xfer_info = g_new (XferInfo, 1);
	xfer_info->parent_view = view;
	xfer_info->progress_dialog = NULL;

	if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) {
		xfer_info->operation_name = _("Moving");
		xfer_info->preparation_name =_("Preparing To Move...");
	} else {
		xfer_info->operation_name = _("Copying");
		xfer_info->preparation_name =_("Preparing To Copy...");
	}

	xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
	
	gnome_vfs_async_xfer (&xfer_info->handle, source_dir, item_names,
	      		      target_dir_uri_text, NULL,
	      		      move_options, GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
	      		      &update_xfer_callback, xfer_info,
	      		      &sync_xfer_callback, xfer_info);

	if (!target_dir)
		g_free ((char *)target_dir_uri_text);

	gnome_vfs_uri_unref (target_dir_uri);
	gnome_vfs_uri_unref (source_dir_uri);
	g_free (source_dir);
}
