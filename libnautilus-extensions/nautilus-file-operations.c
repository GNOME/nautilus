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
#include <libgnomevfs/gnome-vfs-uri.h>

#include "nautilus-file-operations.h"
#include "nautilus-file-operations-progress.h"
#include "nautilus-lib-self-check-functions.h"
#include <libnautilus-extensions/nautilus-file-changes-queue.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include "fm-directory-view.h"

typedef enum {
	XFER_MOVE,
	XFER_COPY,
	XFER_DUPLICATE,
	XFER_MOVE_TO_TRASH,
	XFER_EMPTY_TRASH,
	XFER_DELETE,
	XFER_LINK
} XferKind;

/* Copy engine callback state */
typedef struct {
	GnomeVFSAsyncHandle *handle;
	GtkWidget *progress_dialog;
	const char *operation_title;	/* "Copying files" */
	const char *action_verb;	/* "copied" */
	const char *progress_verb;	/* "Copying" */
	const char *preparation_name;	/* "Preparing To Copy..." */
	const char *cleanup_name;	/* "Finishing Move..." */
	GnomeVFSXferErrorMode error_mode;
	GnomeVFSXferOverwriteMode overwrite_mode;
	GtkWidget *parent_view;
	XferKind kind;
	gboolean show_progress_dialog;
} XferInfo;

/* Struct used to control applying icon positions to 
 * top level items during a copy, drag, new folder creation and
 * link creation
 */
typedef struct {
	GdkPoint *icon_positions;
	int last_icon_position_index;
	GList *uris;
	const GList *last_uri;
} IconPositionIterator;

static IconPositionIterator *
icon_position_iterator_new (const GdkPoint *icon_positions, const GList *uris)
{
	IconPositionIterator *result;
	int uri_count;

	uri_count = g_list_length ((GList *)uris);
	result = g_new (IconPositionIterator, 1);
	
	result->icon_positions = g_new (GdkPoint, uri_count);
	memcpy (result->icon_positions, icon_positions, uri_count * sizeof (GdkPoint));
	result->last_icon_position_index = 0;

	result->uris = nautilus_g_str_list_copy ((GList *)uris);
	result->last_uri = result->uris;

	return result;
}

static void
icon_position_iterator_free (IconPositionIterator *position_iterator)
{
	if (position_iterator == NULL) {
		return;
	}
	
	g_free (position_iterator->icon_positions);
	nautilus_g_list_free_deep (position_iterator->uris);
	g_free (position_iterator);
}

char *
nautilus_convert_to_unescaped_string_for_display  (char *escaped)
{
	char *result;
	if (!escaped) {
		return NULL;
	}
	result = gnome_vfs_unescape_string_for_display (escaped);
	g_free (escaped);
	return result;
}

static void
xfer_dialog_clicked_callback (NautilusFileOperationsProgress *dialog,
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
	if (!xfer_info->show_progress_dialog) {
		return;
	}

	g_return_if_fail (xfer_info->progress_dialog == NULL);

	xfer_info->progress_dialog = nautilus_file_operations_progress_new 
		(xfer_info->operation_title, "", "", "", 1, 1);

	gtk_signal_connect (GTK_OBJECT (xfer_info->progress_dialog),
			    "clicked",
			    GTK_SIGNAL_FUNC (xfer_dialog_clicked_callback),
			    xfer_info);

	gtk_widget_show (xfer_info->progress_dialog);
}

static void
progress_dialog_set_files_remaining_text ( NautilusFileOperationsProgress *dialog, 
	const char *action_verb)
{
	char *text;

	text = g_strdup_printf ("Files remaining to be %s:", action_verb);
	nautilus_file_operations_progress_set_operation_string (dialog, text);
	g_free (text);
}

static void
progress_dialog_set_to_from_item_text (NautilusFileOperationsProgress *dialog,
	const char *progress_verb, const char *from_uri, const char *to_uri, 
	gulong index, gulong size)
{
	char *item;
	char *from_path;
	char *to_path;
	char *progress_label_text;
	const char *from_prefix;
	const char *to_prefix;
	GnomeVFSURI *uri;
	int length;

	item = NULL;
	from_path = NULL;
	to_path = NULL;
	from_prefix = "";
	to_prefix = "";
	progress_label_text = NULL;

	if (from_uri != NULL) {
		uri = gnome_vfs_uri_new (from_uri);
		item = gnome_vfs_uri_extract_short_name (uri);
		from_path = gnome_vfs_uri_extract_dirname (uri);

		/* remove the last '/' */
		length = strlen (from_path);
		if (from_path [length - 1] == '/') {
			from_path [length - 1] = '\0';
		}
		
		gnome_vfs_uri_unref (uri);
		g_assert (progress_verb);
		progress_label_text = g_strdup_printf ("%s:", progress_verb);
		from_prefix = _("From:");
	}

	if (to_uri != NULL) {
		uri = gnome_vfs_uri_new (from_uri);
		to_path = gnome_vfs_uri_extract_dirname (uri);

		/* remove the last '/' */
		length = strlen (to_path);
		if (to_path [length - 1] == '/') {
			to_path [length - 1] = '\0';
		}

		gnome_vfs_uri_unref (uri);
		to_prefix = _("To:");
	}

	nautilus_file_operations_progress_new_file (dialog,
		progress_label_text ? progress_label_text : "",
		item ? item : "",
		from_path ? from_path : "",
		to_path ? to_path : "",
		from_prefix, to_prefix, index, size);

	g_free (progress_label_text);
	g_free (item);
	g_free (from_path);
	g_free (to_path);
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
			nautilus_file_operations_progress_set_operation_string
				(NAUTILUS_FILE_OPERATIONS_PROGRESS
					 (xfer_info->progress_dialog),
					 xfer_info->preparation_name);
		}
		return TRUE;

	case GNOME_VFS_XFER_PHASE_READYTOGO:
		if (xfer_info->progress_dialog != NULL) {
			progress_dialog_set_files_remaining_text (
				NAUTILUS_FILE_OPERATIONS_PROGRESS (xfer_info->progress_dialog),
					 xfer_info->action_verb);
			nautilus_file_operations_progress_set_total
				(NAUTILUS_FILE_OPERATIONS_PROGRESS
					 (xfer_info->progress_dialog),
					 progress_info->files_total,
					 progress_info->bytes_total);
		}
		return TRUE;
				 
	case GNOME_VFS_XFER_PHASE_DELETESOURCE:
		nautilus_file_changes_consume_changes (FALSE);
		if (xfer_info->progress_dialog != NULL) {
			progress_dialog_set_to_from_item_text (
				NAUTILUS_FILE_OPERATIONS_PROGRESS (xfer_info->progress_dialog),
				xfer_info->progress_verb,
				progress_info->source_name,
				NULL,
				progress_info->file_index,
				progress_info->file_size);

			nautilus_file_operations_progress_update_sizes
				(NAUTILUS_FILE_OPERATIONS_PROGRESS
				 (xfer_info->progress_dialog),
				 MIN (progress_info->bytes_copied, 
				 	progress_info->bytes_total),
				 MIN (progress_info->total_bytes_copied,
				 	progress_info->bytes_total));
		}
		return TRUE;

	case GNOME_VFS_XFER_PHASE_MOVING:
	case GNOME_VFS_XFER_PHASE_OPENSOURCE:
	case GNOME_VFS_XFER_PHASE_OPENTARGET:
		/* fall through */

	case GNOME_VFS_XFER_PHASE_COPYING:
		if (xfer_info->progress_dialog != NULL) {
				
			if (progress_info->bytes_copied == 0) {
				progress_dialog_set_to_from_item_text (
					NAUTILUS_FILE_OPERATIONS_PROGRESS (xfer_info->progress_dialog),
					xfer_info->progress_verb,
					progress_info->source_name,
					progress_info->target_name,
					progress_info->file_index,
					progress_info->file_size);
			} else {
				nautilus_file_operations_progress_update_sizes
					(NAUTILUS_FILE_OPERATIONS_PROGRESS
					 (xfer_info->progress_dialog),
					 MIN (progress_info->bytes_copied, 
					 	progress_info->bytes_total),
					 MIN (progress_info->total_bytes_copied,
					 	progress_info->bytes_total));
			}
		}
		return TRUE;

	case GNOME_VFS_XFER_PHASE_CLEANUP:
		if (xfer_info->progress_dialog != NULL) {
			nautilus_file_operations_progress_clear(
				NAUTILUS_FILE_OPERATIONS_PROGRESS
					 (xfer_info->progress_dialog));
			nautilus_file_operations_progress_set_operation_string
				(NAUTILUS_FILE_OPERATIONS_PROGRESS
					 (xfer_info->progress_dialog),
					 xfer_info->cleanup_name);
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
         * requested, but the transfer is always performed in mode
         * `GNOME_VFS_XFER_ERROR_MODE_QUERY'.
         */

	int result;
	char *text;
	char *unescaped_name;
	char *current_operation;

	switch (xfer_info->error_mode) {
	case GNOME_VFS_XFER_ERROR_MODE_QUERY:

		/* transfer error, prompt the user to continue or stop */

		unescaped_name = gnome_vfs_unescape_string_for_display (progress_info->source_name);
		current_operation = g_strdup (xfer_info->progress_verb);

		g_strdown (current_operation);
		if (progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM
		    || progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY
		    || progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
			
			if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
				text = g_strdup_printf
					(_("Error while %s \"%s\".\n"
					   "The destination is read-only. "
					   "Would you like to continue?"),
					 current_operation,
					 unescaped_name);
			} else {
				text = g_strdup_printf
					(_("Error while %s \"%s\".\n"
					   "The destination is read-only. "
					   "Would you like to continue?"),
					 current_operation,
					 unescaped_name);
			}
			g_free (current_operation);
			g_free (unescaped_name);
			
			result = nautilus_simple_dialog
				(xfer_info->parent_view, text, 
				 _("Nautilus: File copy error"),
				 _("Skip"), _("Stop"), NULL);

			g_free (text);

			switch (result) {
			case 0:
				return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
			default:
				g_assert_not_reached ();
				/* fall through */
			case 1:
				return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
			}						
		} else {

			text = g_strdup_printf
				(_("Error \"%s\" while %s \"%s\".\n"
				   "Would you like to continue?"), 
				 gnome_vfs_result_to_string (progress_info->vfs_status),
				 current_operation,
				 unescaped_name);
			g_free (current_operation);
			g_free (unescaped_name);

			result = nautilus_simple_dialog
				(xfer_info->parent_view, text, 
				 _("Nautilus: File copy error"),
				 _("Skip"), _("Retry"), _("Stop"), NULL);
			g_free (text);

			switch (result) {
			case 0:
				return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
			case 1:
				return GNOME_VFS_XFER_ERROR_ACTION_RETRY;
			default:
				g_assert_not_reached ();
				/* fall through */
			case 2:
				return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
			}						
		}

	case GNOME_VFS_XFER_ERROR_MODE_ABORT:
	default:
		if (xfer_info->progress_dialog != NULL) {
			nautilus_file_operations_progress_freeze
				(NAUTILUS_FILE_OPERATIONS_PROGRESS (xfer_info->progress_dialog));
			nautilus_file_operations_progress_thaw
				(NAUTILUS_FILE_OPERATIONS_PROGRESS (xfer_info->progress_dialog));
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
	char *unescaped_name;

	unescaped_name = gnome_vfs_unescape_string_for_display (progress_info->target_name);
	text = g_strdup_printf (_("File %s already exists.\n"
				  "Would you like to replace it?"), 
				unescaped_name);
	g_free (unescaped_name);

	if (progress_info->duplicate_count == 1) {
		/* we are going to only get one duplicate alert, don't offer
		 * Replace All
		 */
		result = nautilus_simple_dialog
			(xfer_info->parent_view, text, 
			 _("Nautilus: File copy conflict"),
			 _("Replace"), _("Skip"), NULL);
		switch (result) {
		case 0:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
		default:
			g_assert_not_reached ();
			/* fall through */
		case 1:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
		}
	} else {
		result = nautilus_simple_dialog
			(xfer_info->parent_view, text, 
			 _("Nautilus: File copy conflict"),
			 _("Replace All"), _("Replace"), _("Skip"), NULL);

		switch (result) {
		case 0:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE_ALL;
		case 1:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
		default:
			g_assert_not_reached ();
			/* fall through */
		case 2:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
		}
	}
}

/* Note that we have these two separate functions with separate format
 * strings for ease of localization.
 */

static char *
get_link_name (char *name, int count) 
{
	const char *format;
	char *result;
	
	g_assert (name != NULL);

	if (count < 1) {
		g_warning ("bad count in get_link_name");
		count = 1;
	}

	if (count <= 2) {
		/* Handle special cases for low numbers.
		 * Perhaps for some locales we will need to add more.
		 */
		switch (count) {
		default:
			g_assert_not_reached ();
			/* fall through */
		case 1:
			format = _("link to %s");
			break;
		case 2:
			format = _("another link to %s");
			break;
		}
		result = g_strdup_printf (format, name);

	} else {
		/* Handle special cases for the first few numbers of each ten.
		 * For locales where getting this exactly right is difficult,
		 * these can just be made all the same as the general case below.
		 */
		switch (count % 10) {
		case 1:
			/* Localizers: Feel free to leave out the "st" suffix
			 * if there's no way to do that nicely for a
			 * particular language.
			 */
			format = _("%dst link to %s");
			break;
		case 2:
			format = _("%dnd link to %s");
			break;
		case 3:
			format = _("%drd link to %s");
			break;
		default:
			format = _("%dth link to %s");
			break;
		}
		result = g_strdup_printf (format, count, name);
	}

	g_free (name);
	return result;
}

/* Localizers: 
 * Feel free to leave out the st, nd, rd and th suffix or
 * make some or all of them match.
 */

#define COPY_DUPLICATE_TAG " (copy)"
#define FRIST_COPY_DUPLICATE_FORMAT _("%s"COPY_DUPLICATE_TAG"%s")
#define ANOTHER_COPY_DUPLICATE_TAG " (another copy)"
#define SECOND_COPY_DUPLICATE_FORMAT _("%s"ANOTHER_COPY_DUPLICATE_TAG"%s")

#define ST_COPY_DUPLICATE_TAG _("st copy)")
#define ST_COPY_DUPLICATE_FORMAT _("%s (%dst copy)%s")
#define ND_COPY_DUPLICATE_TAG _("nd copy)")
#define ND_COPY_DUPLICATE_FORMAT _("%s (%dnd copy)%s")
#define RD_COPY_DUPLICATE_TAG _("rd copy)")
#define RD_COPY_DUPLICATE_FORMAT _("%s (%drd copy)%s")
#define TH_COPY_DUPLICATE_TAG _("th copy)")
#define TH_COPY_DUPLICATE_FORMAT _("%s (%dth copy)%s")


static char *
extract_string_until (const char *original, const char *until_string)
{
	char *result;
	
	g_assert (strlen (original) >= until_string - original);
	g_assert (until_string - original >= 0);

	result = g_malloc (until_string - original + 1);
	strncpy (result, original, until_string - original);
	result[until_string - original] = '\0';
	
	return result;
}

static void
parse_previous_duplicate_name (const char *name, char **name_base, const char **suffix,
	int *count)
{
	const char *tag;
	
	*suffix = strrchr (name, '.');
	if (*suffix == NULL || (*suffix)[1] == '\0') {
		/* no suffix */
		*suffix = "";
	}

	tag = strstr (name, COPY_DUPLICATE_TAG);
	if (tag != NULL) {
		if (tag > *suffix) {
			/* handle case "foo. (copy)" */
			*suffix = "";
		}
		*name_base = extract_string_until (name, tag);
		*count = 1;
		return;
	}


	tag = strstr (name, ANOTHER_COPY_DUPLICATE_TAG);
	if (tag != NULL) {
		if (tag > *suffix) {
			/* handle case "foo. (another copy)" */
			*suffix = "";
		}
		*name_base = extract_string_until (name, tag);
		*count = 2;
		return;
	}


	tag = strstr (name, ST_COPY_DUPLICATE_TAG);
	if (tag == NULL) {
		tag = strstr (name, ND_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, RD_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, TH_COPY_DUPLICATE_TAG);
	}

	if (tag != NULL) {
		tag = strstr (name, " (");
		if (tag != NULL) {
			if (tag > *suffix) {
				/* handle case "foo. (22nd copy)" */
				*suffix = "";
			}
			*name_base = extract_string_until (name, tag);
			if (sscanf (tag, " (%d", count) == 1) {
				return;
			}
			*count = 0;
			return;
		}
	}

	
	*count = 0;
	*name_base = strdup (name);
	if (**suffix != '\0') {
		(*name_base) [*suffix - name] = '\0';
	}
}

static char *
make_next_duplicate_name (const char *base, const char *suffix, int count)
{
	const char *format;
	char *result;


	if (count < 1) {
		g_warning ("bad count in get_duplicate_name");
		count = 1;
	}

	if (count <= 2) {
		/* Handle special cases for low numbers.
		 * Perhaps for some locales we will need to add more.
		 */
		switch (count) {
		default:
			g_assert_not_reached ();
			/* fall through */
		case 1:
			format = FRIST_COPY_DUPLICATE_FORMAT;
			break;
		case 2:
			format = SECOND_COPY_DUPLICATE_FORMAT;
			break;

		}
		result = g_strdup_printf (format, base, suffix);
	} else {

		/* Handle special cases for the first few numbers of each ten.
		 * For locales where getting this exactly right is difficult,
		 * these can just be made all the same as the general case below.
		 */
		switch (count % 10) {
		case 1:
			format = ST_COPY_DUPLICATE_FORMAT;
			break;
		case 2:
			format = ND_COPY_DUPLICATE_FORMAT;
			break;
		case 3:
			format = RD_COPY_DUPLICATE_FORMAT;
			break;
		default:
			/* The general case. */
			format = TH_COPY_DUPLICATE_FORMAT;
			break;
		}

		result = g_strdup_printf (format, base, count, suffix);
	}

	return result;
}

static char *
get_duplicate_name (const char *name, int count_increment)
{
	char *result;
	char *name_base;
	const char *suffix;
	int count;

	parse_previous_duplicate_name (name, &name_base, &suffix, &count);
	result = make_next_duplicate_name (name_base, suffix, count + count_increment);

	g_free (name_base);

	return result;
}

static char *
get_next_duplicate_name (char *name, int count_increment)
{
	char *result;

	result = get_duplicate_name (name, count_increment);
	g_free (name);
	
	return result;
}

static int
handle_xfer_duplicate (GnomeVFSXferProgressInfo *progress_info,
		       XferInfo *xfer_info)
{
	switch (xfer_info->kind) {
	case XFER_LINK:
		progress_info->duplicate_name = get_link_name
			(progress_info->duplicate_name,
			 progress_info->duplicate_count);
		break;

	case XFER_COPY:
		progress_info->duplicate_name = get_next_duplicate_name
			(progress_info->duplicate_name,
			 progress_info->duplicate_count);
		break;

	default:
		/* For all other cases we use the name as-is. */
	}

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

static void
apply_one_position (IconPositionIterator *position_iterator, 
	const char *source_name, const char *target_name)
{
	const char *item_uri;

	if (position_iterator == NULL || position_iterator->last_uri == NULL) {
		return;
	}
		
	for (;;) {
		/* Scan for the next point that matches the source_name
		 * uri.
		 */
		if (strcmp ((const char *)position_iterator->last_uri->data, 
			source_name) == 0) {
			break;
		}
		/* Didn't match -- a uri must have been skipped by the copy 
		 * engine because of a name conflict. All we need to do is 
		 * skip ahead too.
		 */
		position_iterator->last_uri = position_iterator->last_uri->next;
		position_iterator->last_icon_position_index++; 

		if (position_iterator->last_uri == NULL) {
			/* we are done, no more points left */
			return;
		}
	}

	item_uri = target_name != NULL ? target_name : source_name;

	/* apply the location to the target file */
	nautilus_file_changes_queue_schedule_position_setting (target_name, 
		position_iterator->icon_positions
			[position_iterator->last_icon_position_index]);

	/* advance to the next point for next time */
	position_iterator->last_uri = position_iterator->last_uri->next;
	position_iterator->last_icon_position_index++; 
}

/* Low-level callback, called for every copy engine operation.
 * Generates notifications about new, deleted and moved files.
 */
static int
sync_xfer_callback (GnomeVFSXferProgressInfo *progress_info, gpointer data)
{
	IconPositionIterator *position_iterator;

	position_iterator = (IconPositionIterator *)data;

	if (progress_info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OK) {
		switch (progress_info->phase) {
		case GNOME_VFS_XFER_PHASE_OPENTARGET:
			if (progress_info->top_level_item) {
				/* this is one of the selected copied or moved items -- we need
				 * to make sure it's metadata gets copied over
				 */
				g_assert (progress_info->source_name != NULL);
				nautilus_file_changes_queue_schedule_metadata_copy 
					(progress_info->source_name, progress_info->target_name);

				apply_one_position (position_iterator, progress_info->source_name,
					progress_info->target_name);
			}
			nautilus_file_changes_queue_file_added (progress_info->target_name);
			break;

		case GNOME_VFS_XFER_PHASE_MOVING:
			if (progress_info->top_level_item) {
				g_assert (progress_info->source_name != NULL);
				nautilus_file_changes_queue_schedule_metadata_move 
					(progress_info->source_name, progress_info->target_name);

				apply_one_position (position_iterator, progress_info->source_name,
					progress_info->target_name);
			}
			nautilus_file_changes_queue_file_moved (progress_info->source_name,
				progress_info->target_name);
			break;

		case GNOME_VFS_XFER_PHASE_DELETESOURCE:
			if (progress_info->top_level_item) {
				g_assert (progress_info->source_name != NULL);
				nautilus_file_changes_queue_schedule_metadata_remove 
					(progress_info->source_name);
			}
			nautilus_file_changes_queue_file_removed (progress_info->source_name);
			break;

		case GNOME_VFS_XFER_PHASE_COMPLETED:
			/* done, clean up */
			icon_position_iterator_free (position_iterator);
			break;

		default:
			break;
		}
	}
	return 1;
}

static gboolean
check_target_directory_is_or_in_trash (GnomeVFSURI *trash_dir_uri, GnomeVFSURI *target_dir_uri)
{
	g_assert (target_dir_uri != NULL);

	if (trash_dir_uri == NULL) {
		return FALSE;
	}

	return gnome_vfs_uri_equal (trash_dir_uri, target_dir_uri)
		|| gnome_vfs_uri_is_parent (trash_dir_uri, target_dir_uri, TRUE);
}


static GnomeVFSURI *
append_basename (const GnomeVFSURI *target_directory, const GnomeVFSURI *source_directory)
{
	const char *filename;

	filename =  gnome_vfs_uri_extract_short_name (source_directory);
	if (filename != NULL) {
		return gnome_vfs_uri_append_file_name (target_directory, filename);
	}
	 
	return gnome_vfs_uri_dup (target_directory);
}

void
nautilus_file_operations_copy_move (const GList *item_uris,
				    const GdkPoint *relative_item_points,
				    const char *target_dir,
				    int copy_action,
				    GtkWidget *view)
{
	const GList *p;
	GnomeVFSXferOptions move_options;
	GList *source_uri_list, *target_uri_list;
	GnomeVFSURI *source_uri, *target_uri;
	GnomeVFSURI *target_dir_uri;
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSURI *uri;

	XferInfo *xfer_info;
	GnomeVFSResult result;
	gboolean same_fs;
	gboolean is_trash_move;
	
	IconPositionIterator *icon_position_iterator;

	g_assert (item_uris != NULL);

	target_dir_uri = NULL;
	trash_dir_uri = NULL;
	icon_position_iterator = NULL;
	result = GNOME_VFS_OK;

	source_uri_list = NULL;
	target_uri_list = NULL;
	same_fs = TRUE;
	is_trash_move = FALSE;

	move_options = GNOME_VFS_XFER_RECURSIVE;

	if (target_dir == NULL) {
		/* assume duplication */
		g_assert (copy_action != GDK_ACTION_MOVE);
		move_options |= GNOME_VFS_XFER_USE_UNIQUE_NAMES;
	} else {
		if (nautilus_uri_is_trash (target_dir)) {
			is_trash_move = TRUE;
		} else {
			target_dir_uri = gnome_vfs_uri_new (target_dir);
		}
	}

	/* build the source and target URI lists and figure out if all the files are on the
	 * same disk
	 */
	for (p = item_uris; p != NULL; p = p->next) {
		source_uri = gnome_vfs_uri_new ((const char *)p->data);
		source_uri_list = g_list_prepend (source_uri_list, source_uri);

		if (target_dir != NULL) {
			if (is_trash_move) {
				gnome_vfs_find_directory (source_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
						   	  &target_dir_uri, FALSE, FALSE, 0777);				
			}
			target_uri = append_basename (target_dir_uri, source_uri);
		} else {
			/* duplication */
			target_uri = gnome_vfs_uri_ref (source_uri);
			if (target_dir_uri == NULL) {
				target_dir_uri = gnome_vfs_uri_get_parent (source_uri);
			}
		}
		target_uri_list = g_list_prepend (target_uri_list, target_uri);
		gnome_vfs_check_same_fs_uris (source_uri, target_uri, &same_fs);
	}

	source_uri_list = g_list_reverse (source_uri_list);
	target_uri_list = g_list_reverse (target_uri_list);


	if (copy_action == GDK_ACTION_MOVE) {
		move_options |= GNOME_VFS_XFER_REMOVESOURCE;
	} else if (copy_action == GDK_ACTION_LINK) {
		move_options |= GNOME_VFS_XFER_LINK_ITEMS;
	}
	
	/* set up the copy/move parameters */
	xfer_info = g_new (XferInfo, 1);
	xfer_info->parent_view = view;
	xfer_info->progress_dialog = NULL;

	if (relative_item_points != NULL) {
		icon_position_iterator = icon_position_iterator_new (relative_item_points, item_uris);
	}
	
	if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) {
		xfer_info->operation_title = _("Moving files");
		xfer_info->action_verb =_("moved");
		xfer_info->progress_verb =_("Moving");
		xfer_info->preparation_name =_("Preparing To Move...");
		xfer_info->cleanup_name = _("Finishing Move...");

		xfer_info->kind = XFER_MOVE;
		/* Do an arbitrary guess that an operation will take very little
		 * time and the progress shouldn't be shown.
		 */
		xfer_info->show_progress_dialog = 
			!same_fs || g_list_length ((GList *)item_uris) > 20;
	} else if ((move_options & GNOME_VFS_XFER_LINK_ITEMS) != 0) {
		xfer_info->operation_title = _("Creating links to files");
		xfer_info->action_verb =_("linked");
		xfer_info->progress_verb =_("Linking");
		xfer_info->preparation_name = _("Preparing to Create Links...");
		xfer_info->cleanup_name = _("Finishing Creating Links...");

		xfer_info->kind = XFER_LINK;
		xfer_info->show_progress_dialog =
			g_list_length ((GList *)item_uris) > 20;
	} else {
		xfer_info->operation_title = _("Copying files");
		xfer_info->action_verb =_("copied");
		xfer_info->progress_verb =_("Copying");
		xfer_info->preparation_name =_("Preparing To Copy...");
		xfer_info->cleanup_name = "";

		xfer_info->kind = XFER_COPY;
		/* always show progress during copy */
		xfer_info->show_progress_dialog = TRUE;
	}


	/* we'll need to check for copy into Trash and for moving/copying the Trash itself */
	gnome_vfs_find_directory (target_dir_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, FALSE, FALSE, 0777);

	if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) == 0) {
		/* don't allow copying into Trash */
		if (check_target_directory_is_or_in_trash (trash_dir_uri, target_dir_uri)) {
			nautilus_simple_dialog
				(view, 
				 _("You cannot copy items into the Trash."), 
				 _("Nautilus: Error copying"),
				 GNOME_STOCK_BUTTON_OK, NULL, NULL);			
			result = GNOME_VFS_ERROR_NOT_PERMITTED;
		}
	}

	if (result == GNOME_VFS_OK) {
		for (p = source_uri_list; p != NULL; p = p->next) {
			uri = (GnomeVFSURI *)p->data;

			/* Check that the Trash is not being moved/copied */
			if (trash_dir_uri != NULL && gnome_vfs_uri_equal (uri, trash_dir_uri)) {
				nautilus_simple_dialog
					(view, 
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					 ? _("You cannot move the Trash.")
					 : _("You cannot copy the Trash."), 
					 _("Nautilus: Error moving to Trash"),
					 GNOME_STOCK_BUTTON_OK, NULL, NULL);			

				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}

			/* Don't allow recursive move/copy into itself. 
			 * (We would get a file system error if we proceeded but it is nicer to
			 * detect and report it at this level)
			 */
			if ((move_options & GNOME_VFS_XFER_LINK_ITEMS) == 0
				&& (gnome_vfs_uri_equal (uri, target_dir_uri)
					|| gnome_vfs_uri_is_parent (uri, target_dir_uri, TRUE))) {
				nautilus_simple_dialog
					(view, 
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					 ? _("You cannot move an item into itself.")
					 : _("You cannot copy an item into itself."), 
					 _("Nautilus: Error moving into self"),
					 GNOME_STOCK_BUTTON_OK, NULL, NULL);			

				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}
		}
	}

	xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
	
	if (result == GNOME_VFS_OK) {
		gnome_vfs_async_xfer (&xfer_info->handle, source_uri_list, target_uri_list,
		      		      move_options, GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, icon_position_iterator);
	}

	gnome_vfs_uri_list_free (source_uri_list);
	gnome_vfs_uri_list_free (target_uri_list);
	if (trash_dir_uri != NULL) {
		gnome_vfs_uri_unref (trash_dir_uri);
	}
	gnome_vfs_uri_unref (target_dir_uri);
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
	char *temp_string;
	
	state = (NewFolderXferState *) data;

	switch (progress_info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		nautilus_file_changes_consume_changes (TRUE);
		(state->done_callback) (progress_info->target_name, state->data);
		g_free (state);
		return 0;

	case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:

		temp_string = progress_info->duplicate_name;

		if (progress_info->vfs_status == GNOME_VFS_ERROR_NAME_TOO_LONG) {
			/* special case an 8.3 file system */
			progress_info->duplicate_name = g_strndup (temp_string, 8);
			progress_info->duplicate_name[8] = '\0';
			g_free (temp_string);
			temp_string = progress_info->duplicate_name;
			progress_info->duplicate_name = g_strdup_printf ("%s.%d", 
				progress_info->duplicate_name,
				progress_info->duplicate_count);
		} else {
			progress_info->duplicate_name = g_strdup_printf ("%s %d", 
				progress_info->duplicate_name,
				progress_info->duplicate_count);
		}
		g_free (temp_string);
		return GNOME_VFS_XFER_ERROR_ACTION_SKIP;

	default:
		g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
			   progress_info->status);
		return 0;
	}
}


void 
nautilus_file_operations_new_folder (GtkWidget *parent_view, 
				     const char *parent_dir,
				     void (*done_callback)(const char *, gpointer),
				     gpointer data)
{
	NewFolderXferState *state;
	GList *target_uri_list;
	GnomeVFSURI *uri, *parent_uri;

	state = g_new (NewFolderXferState, 1);
	state->done_callback = done_callback;
	state->data = data;

	/* pass in the target directory and the new folder name as a destination URI */
	parent_uri = gnome_vfs_uri_new (parent_dir);
	uri = gnome_vfs_uri_append_path (parent_uri, _("untitled folder"));
	target_uri_list = g_list_append (NULL, uri);
	
	gnome_vfs_async_xfer (&state->handle, NULL, target_uri_list,
	      		      GNOME_VFS_XFER_NEW_UNIQUE_DIRECTORY,
	      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
	      		      &new_folder_xfer_callback, state,
	      		      &sync_xfer_callback, NULL);

	gnome_vfs_uri_list_free (target_uri_list);
	gnome_vfs_uri_unref (parent_uri);
}

void 
nautilus_file_operations_move_to_trash (const GList *item_uris, 
					GtkWidget *parent_view)
{
	const GList *p;
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSURI *source_uri;
	GList *source_uri_list, *target_uri_list;
	GnomeVFSResult result;
	XferInfo *xfer_info;
	gboolean bail;
	char *text;
	char *item_name;

	g_assert (item_uris != NULL);
	
	trash_dir_uri = NULL;
	source_uri_list = NULL;
	target_uri_list = NULL;

	result = GNOME_VFS_OK;

	/* build the source and uri list, checking if any of the delete itmes are Trash */
	for (p = item_uris; p != NULL; p = p->next) {
		bail = FALSE;
		source_uri = gnome_vfs_uri_new ((const char *)p->data);
		source_uri_list = g_list_prepend (source_uri_list, source_uri);

		if (trash_dir_uri == NULL) {
			GnomeVFSURI *source_dir_uri;
			
			source_dir_uri = gnome_vfs_uri_get_parent (source_uri);
			result = gnome_vfs_find_directory (source_dir_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
				&trash_dir_uri, FALSE, FALSE, 0777);
			gnome_vfs_uri_unref (source_dir_uri);
		}

		if (result != GNOME_VFS_OK) {
			break;
		}

		g_assert (trash_dir_uri != NULL);
		target_uri_list = g_list_prepend (target_uri_list, append_basename (trash_dir_uri, source_uri));
		
		if (gnome_vfs_uri_equal (source_uri, trash_dir_uri)) {
			nautilus_simple_dialog
				(parent_view, 
				 _("You cannot throw away the Trash."), 
				 _("Nautilus: Error throwing away Trash"),
				 GNOME_STOCK_BUTTON_OK, NULL, NULL);			
			bail = TRUE;
		} else if (gnome_vfs_uri_is_parent (source_uri, trash_dir_uri, TRUE)) {
			item_name = nautilus_convert_to_unescaped_string_for_display 
				(gnome_vfs_uri_extract_short_name (source_uri));
			text = g_strdup_printf
				(_("You cannot throw \"%s\" into the Trash."),
				 item_name);
			nautilus_simple_dialog
				(parent_view, text, 
				 _("Nautilus: Error moving to Trash"),
				 GNOME_STOCK_BUTTON_OK, NULL, NULL);			
			bail = TRUE;
			g_free (text);
			g_free (item_name);
		}

		if (bail) {
			result = GNOME_VFS_ERROR_NOT_PERMITTED;
			break;
		}
	}
	source_uri_list = g_list_reverse (source_uri_list);
	target_uri_list = g_list_reverse (target_uri_list);

	if (result == GNOME_VFS_OK) {
		g_assert (trash_dir_uri != NULL);

		/* set up the move parameters */
		xfer_info = g_new (XferInfo, 1);
		xfer_info->parent_view = parent_view;
		xfer_info->progress_dialog = NULL;

		/* Do an arbitrary guess that an operation will take very little
		 * time and the progress shouldn't be shown.
		 */
		xfer_info->show_progress_dialog = g_list_length ((GList *)item_uris) > 20;

		xfer_info->operation_title = _("Moving files to the Trash");
		xfer_info->action_verb =_("thrown out");
		xfer_info->progress_verb =_("Moving");
		xfer_info->preparation_name =_("Preparing to Move to Trash...");
		xfer_info->cleanup_name ="";

		xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
		xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
		xfer_info->kind = XFER_MOVE_TO_TRASH;
		
		gnome_vfs_async_xfer (&xfer_info->handle, source_uri_list, target_uri_list,
		      		      GNOME_VFS_XFER_REMOVESOURCE | GNOME_VFS_XFER_USE_UNIQUE_NAMES,
		      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, NULL);

	}

	gnome_vfs_uri_list_free (source_uri_list);
	gnome_vfs_uri_list_free (target_uri_list);
	gnome_vfs_uri_unref (trash_dir_uri);
}

void 
nautilus_file_operations_delete (const GList *item_uris, 
				 GtkWidget *parent_view)
{
	GList *uri_list;
	const GList *p;
	XferInfo *xfer_info;

	uri_list = NULL;
	for (p = item_uris; p != NULL; p = p->next) {
		uri_list = g_list_prepend (uri_list, 
			gnome_vfs_uri_new ((const char *)p->data));
	}
	uri_list = g_list_reverse (uri_list);

	xfer_info = g_new (XferInfo, 1);
	xfer_info->parent_view = parent_view;
	xfer_info->progress_dialog = NULL;
	xfer_info->show_progress_dialog = TRUE;

	xfer_info->operation_title = _("Deleting files");
	xfer_info->action_verb =_("deleted");
	xfer_info->progress_verb =_("Deleting");
	xfer_info->preparation_name =_("Preparing to Delete files...");
	xfer_info->cleanup_name ="";

	xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
	xfer_info->kind = XFER_DELETE;
	
	gnome_vfs_async_xfer (&xfer_info->handle, uri_list,  NULL,
	      		      GNOME_VFS_XFER_DELETE_ITEMS | GNOME_VFS_XFER_RECURSIVE,
	      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
	      		      &update_xfer_callback, xfer_info,
	      		      &sync_xfer_callback, NULL);

	gnome_vfs_uri_list_free (uri_list);
}

static void
do_empty_trash (GtkWidget *parent_view)
{
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSResult result;
	XferInfo *xfer_info;
	GList *trash_dir_list;

	/* FIXME bugzilla.eazel.com 638:
	 * add the different trash directories from the different volumes
	 */

	trash_dir_uri = NULL;
	trash_dir_list = NULL;

	result = gnome_vfs_find_directory (NULL, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, FALSE, FALSE, 0777);

	if (result == GNOME_VFS_OK) {

		g_assert (trash_dir_uri != NULL);


		/* set up the move parameters */
		xfer_info = g_new (XferInfo, 1);
		xfer_info->parent_view = parent_view;
		xfer_info->progress_dialog = NULL;
		xfer_info->show_progress_dialog = TRUE;

		xfer_info->operation_title = _("Emptying the Trash");
		xfer_info->action_verb =_("deleted");
		xfer_info->progress_verb =_("Deleting");
		xfer_info->preparation_name =_("Preparing to Empty the Trash...");
		xfer_info->cleanup_name ="";
		xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
		xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
		xfer_info->kind = XFER_EMPTY_TRASH;

		trash_dir_list = g_list_append (NULL, trash_dir_uri);

		gnome_vfs_async_xfer (&xfer_info->handle, trash_dir_list, NULL,
		      		      GNOME_VFS_XFER_EMPTY_DIRECTORIES,
		      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, NULL);

	}

	gnome_vfs_uri_list_free (trash_dir_list);
}

static gboolean
confirm_empty_trash (GtkWidget *parent_view)
{
	GnomeDialog *dialog;
	GtkWindow *parent_window;

	parent_window = GTK_WINDOW (gtk_widget_get_toplevel (parent_view));

	dialog = nautilus_yes_no_dialog (
		_("Are you sure you want to permanently delete "
		  "all of the items in the trash?"),
		_("Nautilus: Delete trash contents?"),
		_("Empty"),
		GNOME_STOCK_BUTTON_CANCEL,
		parent_window);

	gnome_dialog_set_default (dialog, GNOME_CANCEL);

	return gnome_dialog_run (dialog) == GNOME_OK;
}

void 
nautilus_file_operations_empty_trash (GtkWidget *parent_view)
{
	g_return_if_fail (parent_view != NULL);

	/* 
	 * I chose to use a modal confirmation dialog here. 
	 * If we used a modeless dialog, we'd have to do work to
	 * make sure that no more than one appears on screen at
	 * a time. That one probably couldn't be parented, because
	 * otherwise you'd get into weird layer-shifting problems
	 * selecting "Empty Trash" from one window when there was
	 * already a modeless "Empty Trash" dialog parented on a 
	 * different window. And if the dialog were not parented, it
	 * might show up in some weird place since window manager
	 * placement is unreliable (i.e., sucks). So modal it is.
	 */
	if (confirm_empty_trash (parent_view)) {
		do_empty_trash (parent_view);
	}
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static gboolean
test_next_duplicate_name (const char *name, const char *expected_next_name)
{
	gboolean result;
	char *next_name;

	next_name = get_duplicate_name (name, 1);
	result = strcmp (expected_next_name, next_name) == 0;
	g_free (next_name);

	return result;
}

void
nautilus_self_check_file_operations (void)
{

	/* test the next duplicate name generator */
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (" (copy)", " (another copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo", "foo (copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo foo", "foo foo (copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo.txt", "foo (copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo foo.txt", "foo foo (copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo foo.txt txt", "foo foo (copy).txt txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo...txt", "foo.. (copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo...", "foo... (copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo. (copy)", "foo. (another copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (copy)", "foo (another copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (copy).txt", "foo (another copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (another copy)", "foo (3rd copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (another copy).txt", "foo (3rd copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo foo (another copy).txt", "foo foo (3rd copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (21st copy)", "foo (22nd copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (21st copy).txt", "foo (22nd copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (22nd copy)", "foo (23rd copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (22nd copy).txt", "foo (23rd copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (23rd copy)", "foo (24th copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (23rd copy).txt", "foo (24th copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (24th copy)", "foo (25th copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo (24th copy).txt", "foo (25th copy).txt"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo foo (24th copy)", "foo foo (25th copy)"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name ("foo foo (24th copy).txt", "foo foo (25th copy).txt"), TRUE);
}

#endif
