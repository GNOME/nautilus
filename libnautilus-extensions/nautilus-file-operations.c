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
#include <libgnomevfs/gnome-vfs-find-directory.h>

#include "dfos-xfer.h"
#include <libnautilus-extensions/nautilus-file-changes-queue.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include "fm-directory-view.h"

typedef enum {
	XFER_MOVE,
	XFER_COPY,
	XFER_MOVE_TO_TRASH,
	XFER_EMPTY_TRASH,
	XFER_DELETE
} XferKind;


typedef struct XferInfo {
	GnomeVFSAsyncHandle *handle;
	GtkWidget *progress_dialog;
	const char *operation_name;
	const char *preparation_name;
	GnomeVFSXferErrorMode error_mode;
	GnomeVFSXferOverwriteMode overwrite_mode;
	GtkWidget *parent_view;
	XferKind kind;
	gboolean show_progress_dialog;
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
	if (!xfer_info->show_progress_dialog)
		return;

	g_return_if_fail (xfer_info->progress_dialog == NULL);

	xfer_info->progress_dialog
		= dfos_xfer_progress_dialog_new ("Transfer in progress",
						 "", 
						 "", "",
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
		if (xfer_info->progress_dialog != NULL) {
			dfos_xfer_progress_dialog_set_operation_string
				(DFOS_XFER_PROGRESS_DIALOG
					 (xfer_info->progress_dialog),
					 xfer_info->preparation_name);
		}
		return TRUE;

	case GNOME_VFS_XFER_PHASE_READYTOGO:
		if (xfer_info->progress_dialog != NULL) {
			dfos_xfer_progress_dialog_set_operation_string
				(DFOS_XFER_PROGRESS_DIALOG
					 (xfer_info->progress_dialog),
					 xfer_info->operation_name);
			dfos_xfer_progress_dialog_set_total
				(DFOS_XFER_PROGRESS_DIALOG
					 (xfer_info->progress_dialog),
					 progress_info->files_total,
					 progress_info->bytes_total);
		}
		return TRUE;
				 
	case GNOME_VFS_XFER_PHASE_MOVING:
	case GNOME_VFS_XFER_PHASE_DELETESOURCE:
	case GNOME_VFS_XFER_PHASE_OPENSOURCE:
	case GNOME_VFS_XFER_PHASE_OPENTARGET:
		nautilus_file_changes_consume_changes (FALSE);
		/* fall through */

	case GNOME_VFS_XFER_PHASE_COPYING:
		if (xfer_info->progress_dialog != NULL) {
			if (progress_info->bytes_copied == 0) {
				dfos_xfer_progress_dialog_new_file
					(DFOS_XFER_PROGRESS_DIALOG
					 (xfer_info->progress_dialog),
					 progress_info->source_name,
					 progress_info->target_name,
					 xfer_info->kind != XFER_MOVE_TO_TRASH 
					 && xfer_info->kind != XFER_EMPTY_TRASH 
					 && xfer_info->kind != XFER_DELETE
					 ? _("From:") : "",
					 xfer_info->kind != XFER_MOVE_TO_TRASH 
					 && xfer_info->kind != XFER_EMPTY_TRASH 
					 && xfer_info->kind != XFER_DELETE
					 ? _("To:") : NULL,
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
		}
		return TRUE;

	case GNOME_VFS_XFER_PHASE_COMPLETED:
		nautilus_file_changes_consume_changes (TRUE);
		if (xfer_info->progress_dialog != NULL) {
			gtk_widget_destroy (xfer_info->progress_dialog);
		}
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
		text = g_strdup_printf ( _("Error %s copying file %s.\n"
			"Would you like to continue?"), 
			gnome_vfs_result_to_string (progress_info->vfs_status),
			progress_info->source_name);
		result = nautilus_simple_dialog
			(xfer_info->parent_view, text, 
			 _("File copy error"),
			 _("Skip"), _("Retry"), _("Stop"), NULL);

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
		if (xfer_info->progress_dialog != NULL) {
			dfos_xfer_progress_dialog_freeze (DFOS_XFER_PROGRESS_DIALOG
							  (xfer_info->progress_dialog));
			dfos_xfer_progress_dialog_thaw (DFOS_XFER_PROGRESS_DIALOG
							(xfer_info->progress_dialog));
			gtk_widget_destroy (xfer_info->progress_dialog);
		}
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

	text = g_strdup_printf ( _("File %s already exists.\n"
		"Would you like to replace it?"), 
		progress_info->target_name);

	if (progress_info->duplicate_count == 1) {
		/* we are going to only get one duplicate alert, don't offer
		 * Replace All
		 */
		result = nautilus_simple_dialog
			(xfer_info->parent_view, text, 
			 _("File copy conflict"),
			 _("Replace"), _("Skip"), NULL);
		switch (result) {
		case 0:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
		case 1:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
		}
	} else {
		result = nautilus_simple_dialog
			(xfer_info->parent_view, text, 
			 _("File copy conflict"),
			 _("Replace All"), _("Replace"), _("Skip"), NULL);

		switch (result) {
		case 0:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE_ALL;
		case 1:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
		case 2:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
		}
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


		/* FIXME bugzilla.eazel.com 677: signals and all that.  */
		dialog = gnome_error_dialog (message);

		gtk_widget_show (dialog);

		g_free (message);
		g_free (xfer_info);
	}
}

static void
get_parent_make_name_list (const GList *item_uris, GnomeVFSURI **source_dir_uri,
	GList **item_names)
{
	const GList *p;
	GnomeVFSURI *item_uri;
	const gchar *item_name;
	char *unescaped_item_name;

	/* convert URI list to a source parent URI and a list of names */
	for (p = item_uris; p != NULL; p = p->next) {
		item_uri = gnome_vfs_uri_new (p->data);
		item_name = gnome_vfs_uri_get_basename (item_uri);
		unescaped_item_name = gnome_vfs_unescape_string (item_name, NULL);
		/* FIXME bugzilla.eazel.com 1107: If a file had %00 in
		 * its name, then this assert would fail. Also, people
		 * could pass us bad URIs and it would fail.
		 */
		g_assert (unescaped_item_name != NULL);
		*item_names = g_list_prepend (*item_names, unescaped_item_name);
		if (*source_dir_uri == NULL) {
			*source_dir_uri = gnome_vfs_uri_get_parent (item_uri);
		}

		gnome_vfs_uri_unref (item_uri);
	}
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
	char *source_dir;
	GnomeVFSURI *source_dir_uri;
	GnomeVFSURI *target_dir_uri;
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSURI *uri;
	XferInfo *xfer_info;
	char *target_dir_uri_text;
	GnomeVFSResult result;
	
	g_assert (item_uris != NULL);
	
	item_names = NULL;
	source_dir_uri = NULL;
	trash_dir_uri = NULL;
	result = GNOME_VFS_OK;

	get_parent_make_name_list (item_uris, &source_dir_uri, &item_names);

	source_dir = gnome_vfs_uri_to_string (source_dir_uri, GNOME_VFS_URI_HIDE_NONE);
	if (target_dir != NULL) {
		target_dir_uri = gnome_vfs_uri_new (target_dir);
		target_dir_uri_text = g_strdup (target_dir);
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
		xfer_info->kind = XFER_MOVE;
		/* Do an arbitrary guess that an operation will take very little
		 * time and the progress shouldn't be shown.
		 */
		xfer_info->show_progress_dialog =  g_list_length ((GList *)item_uris) > 20;
	} else {
		xfer_info->operation_name = _("Copying");
		xfer_info->preparation_name =_("Preparing To Copy...");
		xfer_info->kind = XFER_COPY;
		/* always show progress during copy */
		xfer_info->show_progress_dialog = TRUE;
	}

	/* we'll need to check for copy into Trash and for moving/copying the Trash itself */
	gnome_vfs_find_directory (target_dir_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, TRUE, 0777);

	if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) == 0) {

		if (trash_dir_uri != NULL
			&& (gnome_vfs_uri_equal (trash_dir_uri, target_dir_uri)
				|| gnome_vfs_uri_is_parent (trash_dir_uri, target_dir_uri, FALSE))) {
			nautilus_simple_dialog (view, 
				_("You cannot copy items into the Trash."), 
				_("Error copying"),
				_("OK"), NULL, NULL);			
			result = GNOME_VFS_ERROR_NOT_PERMITTED;
		}
	}

	if (result == GNOME_VFS_OK) {
		for (p = item_uris; p != NULL; p = p->next) {
			gboolean bail;

			bail = FALSE;
			uri = gnome_vfs_uri_new (p->data);

			/* Check that the Trash is not being moved/copied */
			if (trash_dir_uri != NULL && gnome_vfs_uri_equal (uri, trash_dir_uri)) {
				nautilus_simple_dialog (view, 
					((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					? _("You cannot move the Trash.")
					: _("You cannot copy the Trash."), 
					_("Error moving to Trash"),
					_("OK"), NULL, NULL);			
				bail = TRUE;
			}

			/* Don't allow recursive move/copy into itself. 
			 * (We would get a file system error if we proceeded but it is nicer to
			 * detect and report it at this level)
			 */
			if (!bail && (gnome_vfs_uri_equal (uri, target_dir_uri)
				|| gnome_vfs_uri_is_parent (uri, target_dir_uri, TRUE))) {
				nautilus_simple_dialog (view, 
					((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					? _("You cannot move an item into itself.")
					: _("You cannot copy an item into itself."), 
					_("Error moving to Trash"),
					_("OK"), NULL, NULL);			
				bail = TRUE;
			}
			gnome_vfs_uri_unref (uri);
			if (bail) {
				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}
		}
	}

	xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
	
	if (result == GNOME_VFS_OK) {
		gnome_vfs_async_xfer (&xfer_info->handle, source_dir, item_names,
		      		      target_dir_uri_text, NULL,
		      		      move_options, GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, NULL);
	}

	g_free (target_dir_uri_text);

	gnome_vfs_uri_unref (trash_dir_uri);
	gnome_vfs_uri_unref (target_dir_uri);
	gnome_vfs_uri_unref (source_dir_uri);
	nautilus_g_list_free_deep (item_names);
	g_free (source_dir);
}

typedef struct {
	GnomeVFSAsyncHandle *handle;
	void (* done_callback)(const char *new_folder_uri, gpointer data);
	gpointer data;
} NewFolderXferState;

static int
new_folder_xfer_callback (GnomeVFSAsyncHandle *handle,
	GnomeVFSXferProgressInfo *progress_info, gpointer data)
{
	NewFolderXferState *state;
	char *old_name;
	
	state = (NewFolderXferState *) data;

	switch (progress_info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		nautilus_file_changes_consume_changes (TRUE);
		(state->done_callback) (progress_info->target_name, state->data);
		g_free (state);
		return 0;

	case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
		old_name = progress_info->duplicate_name;
		progress_info->duplicate_name = g_strdup_printf ("%s %d", 
			progress_info->duplicate_name,
			progress_info->duplicate_count);
	
		g_free (old_name);
		return GNOME_VFS_XFER_ERROR_ACTION_SKIP;

	default:
		g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
			   progress_info->status);
		return 0;
	}
}


void 
fs_new_folder (GtkWidget *parent_view, const char *parent_dir,
	void (*done_callback)(const char *, gpointer), gpointer data)
{
	NewFolderXferState *state;
	GList *dest_names;

	state = g_new (NewFolderXferState, 1);
	state->done_callback = done_callback;
	state->data = data;

	dest_names = g_list_append (NULL, "untitled folder");
	gnome_vfs_async_xfer (&state->handle, NULL, NULL,
	      		      parent_dir, dest_names,
	      		      GNOME_VFS_XFER_NEW_UNIQUE_DIRECTORY,
	      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
	      		      &new_folder_xfer_callback, state,
	      		      &sync_xfer_callback, NULL);
	g_list_free (dest_names);
}

void 
fs_move_to_trash (const GList *item_uris, GtkWidget *parent_view)
{
	const GList *p;
	GList *item_names;
	GnomeVFSURI *source_dir_uri;
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSURI *uri;
	char *source_dir;
	char *trash_dir_uri_text;
	GnomeVFSResult result;
	XferInfo *xfer_info;
	gboolean bail;
	char *text;
	char *item_name;

	g_assert (item_uris != NULL);
	
	item_names = NULL;
	source_dir_uri = NULL;
	trash_dir_uri = NULL;
	
	get_parent_make_name_list (item_uris, &source_dir_uri, &item_names);
	source_dir = gnome_vfs_uri_to_string (source_dir_uri, GNOME_VFS_URI_HIDE_NONE);

	result = gnome_vfs_find_directory (source_dir_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, TRUE, 0777);

	for (p = item_uris; p != NULL; p = p->next) {
		bail = FALSE;
		uri = gnome_vfs_uri_new (p->data);

		if (gnome_vfs_uri_equal (uri, trash_dir_uri)) {
			nautilus_simple_dialog (parent_view, 
				_("You cannot throw away the Trash."), 
				_("Error moving to Trash"),
				_("OK"), NULL, NULL);			
			bail = TRUE;
		} else if (gnome_vfs_uri_is_parent (uri, trash_dir_uri, TRUE)) {
			item_name = gnome_vfs_uri_extract_short_name (uri);
			text = g_strdup_printf ( _("You cannot throw \"%s\" "
				"into the Trash."), item_name);

			nautilus_simple_dialog (parent_view, text, 
				_("Error moving to Trash"),
				_("OK"), NULL, NULL);			
			bail = TRUE;
			g_free (text);
			g_free (item_name);
		}
		gnome_vfs_uri_unref (uri);
		if (bail) {
			result = GNOME_VFS_ERROR_NOT_PERMITTED;
			break;
		}
	}

	if (result == GNOME_VFS_OK) {
		g_assert (trash_dir_uri != NULL);

		trash_dir_uri_text = gnome_vfs_uri_to_string (trash_dir_uri, 
							      GNOME_VFS_URI_HIDE_NONE);
		/* set up the move parameters */
		xfer_info = g_new (XferInfo, 1);
		xfer_info->parent_view = parent_view;
		xfer_info->progress_dialog = NULL;

		/* Do an arbitrary guess that an operation will take very little
		 * time and the progress shouldn't be shown.
		 */
		xfer_info->show_progress_dialog = g_list_length ((GList *)item_uris) > 20;

		xfer_info->operation_name = _("Moving to Trash");
		xfer_info->preparation_name =_("Preparing to Move to Trash...");
		xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
		xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
		xfer_info->kind = XFER_MOVE_TO_TRASH;
		
		gnome_vfs_async_xfer (&xfer_info->handle, source_dir, item_names,
		      		      trash_dir_uri_text, NULL,
		      		      GNOME_VFS_XFER_REMOVESOURCE | GNOME_VFS_XFER_USE_UNIQUE_NAMES,
		      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, NULL);

		g_free (trash_dir_uri_text);
	}

	gnome_vfs_uri_unref (trash_dir_uri);
	gnome_vfs_uri_unref (source_dir_uri);
	nautilus_g_list_free_deep (item_names);
	g_free (source_dir);
}

void 
fs_delete (const GList *item_uris, GtkWidget *parent_view)
{
	GnomeVFSURI *source_dir_uri;
	char *source_dir;
	GList *item_names;
	XferInfo *xfer_info;

	item_names = NULL;
	source_dir_uri = NULL;
	get_parent_make_name_list (item_uris, &source_dir_uri, &item_names);
	source_dir = gnome_vfs_uri_to_string (source_dir_uri, GNOME_VFS_URI_HIDE_NONE);

	xfer_info = g_new (XferInfo, 1);
	xfer_info->parent_view = parent_view;
	xfer_info->progress_dialog = NULL;
	xfer_info->show_progress_dialog = TRUE;
	xfer_info->operation_name = _("Deleting");
	xfer_info->preparation_name =_("Preparing to Delete...");
	xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
	xfer_info->kind = XFER_DELETE;
	
	gnome_vfs_async_xfer (&xfer_info->handle, source_dir, item_names,
	      		      NULL, NULL,
	      		      GNOME_VFS_XFER_DELETE_ITEMS | GNOME_VFS_XFER_RECURSIVE,
	      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
	      		      &update_xfer_callback, xfer_info,
	      		      &sync_xfer_callback, NULL);

	gnome_vfs_uri_unref (source_dir_uri);
	nautilus_g_list_free_deep (item_names);
	g_free (source_dir);
}

void 
fs_empty_trash (GtkWidget *parent_view)
{
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSResult result;
	XferInfo *xfer_info;

	/* FIXME bugzilla.eazel.com 638:
	 * add the different trash directories from the different volumes
	 */

	result = gnome_vfs_find_directory (NULL, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, TRUE, 0777);

	if (result == GNOME_VFS_OK) {
		GList *trash_dir_list;
		char *trash_dir_name;
		trash_dir_list = NULL;

		g_assert (trash_dir_uri != NULL);

		/* set up the move parameters */
		xfer_info = g_new (XferInfo, 1);
		xfer_info->parent_view = parent_view;
		xfer_info->progress_dialog = NULL;
		xfer_info->show_progress_dialog = TRUE;
		xfer_info->operation_name = _("Emptying the Trash");
		xfer_info->preparation_name =_("Preparing to Empty the Trash...");
		xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
		xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
		xfer_info->kind = XFER_EMPTY_TRASH;

		trash_dir_name = gnome_vfs_uri_to_string (trash_dir_uri, 
							  GNOME_VFS_URI_HIDE_NONE);
		trash_dir_list = g_list_append (trash_dir_list, trash_dir_name);

		gnome_vfs_async_xfer (&xfer_info->handle, NULL, trash_dir_list,
		      		      NULL, NULL,
		      		      GNOME_VFS_XFER_EMPTY_DIRECTORIES,
		      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, NULL);

		nautilus_g_list_free_deep (trash_dir_list);
	}

	gnome_vfs_uri_unref (trash_dir_uri);
}
