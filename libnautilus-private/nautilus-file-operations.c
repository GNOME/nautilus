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
#include <gtk/gtklabel.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#include "nautilus-file-operations.h"
#include "nautilus-file-operations-progress.h"
#include "nautilus-lib-self-check-functions.h"
#include <libnautilus-extensions/nautilus-file-changes-queue.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-link.h>

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
	const char *action_label;	/* "Files copied:" */
	const char *progress_verb;	/* "Copying" */
	const char *preparation_name;	/* "Preparing To Copy..." */
	const char *cleanup_name;	/* "Finishing Move..." */
	GnomeVFSXferErrorMode error_mode;
	GnomeVFSXferOverwriteMode overwrite_mode;
	GtkWidget *parent_view;
	XferKind kind;
	gboolean show_progress_dialog;
	void (*done_callback) (GHashTable *debuting_uris, gpointer data);
	gpointer done_callback_data;
	GHashTable *debuting_uris;	
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
icon_position_iterator_new (GArray *icon_positions, const GList *uris)
{
	IconPositionIterator *result;
	guint index;

	g_assert (icon_positions->len == g_list_length ((GList *)uris));
	result = g_new (IconPositionIterator, 1);
	
	result->icon_positions = g_new (GdkPoint, icon_positions->len);

	/* make our own copy of the icon locations */
	for (index = 0; index < icon_positions->len; index++) {
		result->icon_positions[index] = g_array_index (icon_positions, GdkPoint, index);
	}

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

/* Hack to get the GdkFont used by a GtkLabel in an error dialog.
 * We need to do this because the string truncation needs to be
 * done before a dialog is instantiated.
 * 
 * This is probably not super fast but it is not a problem in the
 * context we are using it, truncating a string while displaying an
 * error dialog.
 */
static GdkFont *
get_label_font (void)
{
	GtkWidget *label;
	GtkStyle *style;

	label = gtk_label_new ("");
	style = gtk_widget_get_style (label);
	
	gdk_font_ref (style->font);
	gtk_widget_unref (label);

	return style->font;
}

static char *
nautilus_format_name_for_display (const char *escaped_uri)
{
	char *unescaped;
	char *result;
	GdkFont *font;
	int truncate_to_length;

	unescaped = gnome_vfs_unescape_string_for_display (escaped_uri);

	/* get the font the text will be displayed in */
	font = get_label_font ();

	/* get a nice length to truncate to, based on the current font */
	truncate_to_length = gdk_string_width (font, "MMMMMMMMMMMMMMMMMMMMMM");

	/* truncate the result */
	result = nautilus_string_ellipsize_start (unescaped, font, truncate_to_length);

	g_free (unescaped);
	gdk_font_unref (font);

	return result;
}

static char *
nautilus_convert_to_formatted_name_for_display (char *escaped_uri)
{
	char *result;

	if (escaped_uri == NULL) {
		return NULL;
	}
	result = nautilus_format_name_for_display (escaped_uri);
	g_free (escaped_uri);
	return result;
}

static GtkWidget *
parent_for_error_dialog (XferInfo *xfer_info)
{
	if (xfer_info->progress_dialog != NULL) {
		return xfer_info->progress_dialog;
	}

	return xfer_info->parent_view;
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
fit_rect_on_screen (GdkRectangle *rect)
{
	if (rect->x + rect->width > gdk_screen_width ()) {
		rect->x = gdk_screen_width () - rect->width;
	}

	if (rect->y + rect->height > gdk_screen_height ()) {
		rect->y = gdk_screen_height () - rect->height;
	}

	if (rect->x < 0) {
		rect->x = 0;
	}

	if (rect->y < 0) {
		rect->y = 0;
	}
}

static void
center_dialog_over_rect (GtkWindow *window, GdkRectangle rect)
{
	g_return_if_fail (GTK_WINDOW (window) != NULL);

	fit_rect_on_screen (&rect);

	gtk_widget_set_uposition (GTK_WIDGET (window), 
		rect.x + rect.width / 2 
			- GTK_WIDGET (window)->allocation.width / 2,
		rect.y + rect.height / 2
			- GTK_WIDGET (window)->allocation.height / 2);
}

static void
center_dialog_over_window (GtkWindow *window, GtkWindow *over)
{
	GdkRectangle rect;
	int x, y, w, h;

	g_return_if_fail (GTK_WINDOW (window) != NULL);
	g_return_if_fail (GTK_WINDOW (over) != NULL);

	gdk_window_get_root_origin (GTK_WIDGET (over)->window, &x, &y);
	gdk_window_get_size (GTK_WIDGET (over)->window, &w, &h);
	rect.x = x;
	rect.y = y;
	rect.width = w;
	rect.height = h;

	center_dialog_over_rect (window, rect);
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

	/* Make the progress dialog show up over the window we are copying into */
	if (xfer_info->parent_view != NULL) {
		center_dialog_over_window (GTK_WINDOW (xfer_info->progress_dialog), 
			GTK_WINDOW (gtk_widget_get_toplevel (xfer_info->parent_view)));
	}
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
		/* "From" dialog label, source path gets placed next to it in the dialog */
		from_prefix = _("From:");
	}

	if (to_uri != NULL) {
		uri = gnome_vfs_uri_new (to_uri);
		to_path = gnome_vfs_uri_extract_dirname (uri);

		/* remove the last '/' */
		length = strlen (to_path);
		if (to_path [length - 1] == '/') {
			to_path [length - 1] = '\0';
		}

		gnome_vfs_uri_unref (uri);
		/* "To" dialog label, source path gets placed next to it in the dialog */
		to_prefix = _("To:");
	}

	nautilus_file_operations_progress_new_file (dialog,
		progress_label_text != NULL ? progress_label_text : "",
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
			nautilus_file_operations_progress_set_operation_string (
				NAUTILUS_FILE_OPERATIONS_PROGRESS (xfer_info->progress_dialog),
					 xfer_info->action_label);
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
		if (xfer_info->done_callback != NULL) {
			xfer_info->done_callback (xfer_info->debuting_uris, xfer_info->done_callback_data);
			/* done_callback now owns (will free) debuting_uris
			 */
		} else {
			if (xfer_info->debuting_uris != NULL) {
				nautilus_g_hash_table_destroy_deep (xfer_info->debuting_uris);
			}
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
	const char *dialog_title;
	const char *error_string;

	switch (xfer_info->error_mode) {
	case GNOME_VFS_XFER_ERROR_MODE_QUERY:

		/* transfer error, prompt the user to continue or stop */

		unescaped_name = NULL;

		if (progress_info->source_name != NULL) {
			unescaped_name = nautilus_format_name_for_display (progress_info->source_name);
		}

		/* Resist the temptation to do clever message composing here, just
		 * use brute force and duplicate the different flavors of error messages.
		 * That way localizers have an easier time and can even rearrange the
		 * order of the words in the messages easily.
		 */

		switch (xfer_info->kind) {
		case XFER_COPY:
		case XFER_DUPLICATE:
			dialog_title = _("Error while copying.");
			break;
		case XFER_MOVE:
			dialog_title = _("Error while moving.");
			break;
		case XFER_LINK:
			dialog_title = _("Error while linking.");
			break;
		case XFER_DELETE:
		case XFER_EMPTY_TRASH:
		case XFER_MOVE_TO_TRASH:
			dialog_title = _("Error while deleting.");
			break;
		default:
			dialog_title = NULL;
			break;
		}

		/* special case read only target errors or non-readable sources
		 * and other predictable failures
		 */
		
		if (progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM
		    || progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY
		    || progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {

			if ((xfer_info->kind == XFER_MOVE || xfer_info->kind == XFER_MOVE_TO_TRASH)
				&& progress_info->phase != GNOME_VFS_XFER_CHECKING_DESTINATION) {
				/* we are failing because we are moving from a directory that
				 * is not writable
				 */
				if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
					error_string = _("Error while moving.\n"
							 "%s cannot be moved because you do not have "
							 "permissions to change it's parent folder.");
				} else {
					error_string = _("Error while moving.\n"
							 "%s cannot be moved because its parent folder "
							 "is read-only.");
				}
			} else if (progress_info->phase == GNOME_VFS_XFER_PHASE_OPENSOURCE
					|| progress_info->phase == GNOME_VFS_XFER_PHASE_COLLECTING
					|| progress_info->phase == GNOME_VFS_XFER_PHASE_INITIAL) {
				
				if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
					switch (xfer_info->kind) {
					case XFER_COPY:
					case XFER_DUPLICATE:
						error_string = _("Error while copying.\n"
								 "You do not have permissions to read %s.");
						break;
					case XFER_MOVE:
						error_string = _("Error while moving.\n"
								 "You do not have permissions to read %s.");
						break;
					case XFER_LINK:
						error_string = _("Error while linking.\n"
								 "You do not have permissions to read %s.");
						break;
					case XFER_DELETE:
					case XFER_EMPTY_TRASH:
					case XFER_MOVE_TO_TRASH:
						error_string = _("Error while deleting.\n"
								 "You do not have permissions to read %s.");
						break;
					default:
						error_string = "";
						break;
					}
				} else {
					switch (xfer_info->kind) {
					case XFER_COPY:
					case XFER_DUPLICATE:
						error_string = _("Error while copying.\n"
								 "%s is not readable.");
						break;
					case XFER_MOVE:
						error_string = _("Error while moving.\n"
								 "%s is not readable.");
						break;
					case XFER_LINK:
						error_string = _("Error while linking.\n"
								 "%s is not readable.");
						break;
					case XFER_DELETE:
					case XFER_EMPTY_TRASH:
					case XFER_MOVE_TO_TRASH:
						error_string = _("Error while deleting.\n"
								 "%s is not readable.");
						break;
					default:
						error_string = "";
						break;
					}
				}

			} else {
				if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
					switch (xfer_info->kind) {
					case XFER_COPY:
					case XFER_DUPLICATE:
						error_string = _("Error while copying items to \"%s\".\n"
						   		 "You do not have permissions to write to the destination.");
						break;
					case XFER_MOVE_TO_TRASH:
					case XFER_MOVE:
						error_string = _("Error while moving items to \"%s\".\n"
						   		 "You do not have permissions to write to the destination.");
						break;
					case XFER_LINK:
						error_string = _("Error while linking items to \"%s\".\n"
						   		 "You do not have permissions to write to the destination.");
						break;
					default:
						g_assert_not_reached ();
						error_string = "";
						break;
					}
				} else {
					switch (xfer_info->kind) {
					case XFER_COPY:
					case XFER_DUPLICATE:
						error_string = _("Error while copying items to \"%s\".\n"
						   		 "The destination is not writable.");
						break;
					case XFER_MOVE_TO_TRASH:
					case XFER_MOVE:
						error_string = _("Error while moving items to \"%s\".\n"
						   		 "The destination is not writable.");
						break;
					case XFER_LINK:
						error_string = _("Error while linking items to \"%s\".\n"
						   		 "The destination is not writable.");
						break;
					default:
						g_assert_not_reached ();
						error_string = "";
						break;
					}
				}

				if (progress_info->target_name != NULL) {
					g_free (unescaped_name);
					unescaped_name = nautilus_format_name_for_display (
						progress_info->target_name);
				}
			}
			text = g_strdup_printf (error_string, unescaped_name);
			g_free (unescaped_name);
			
			result = nautilus_simple_dialog
				(parent_for_error_dialog (xfer_info), TRUE, text, 
				dialog_title, _("Stop"), NULL);
			g_free (text);

			return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
		}
		/* special case read only target errors */
		if (progress_info->vfs_status == GNOME_VFS_ERROR_NO_SPACE) {
			
			if (unescaped_name != NULL) {
				switch (xfer_info->kind) {
				case XFER_COPY:
				case XFER_DUPLICATE:
					error_string = _("Error while copying \"%s\".\n"
					   		 "There is no space on the destination.");
					break;
				case XFER_MOVE_TO_TRASH:
				case XFER_MOVE:
					error_string = _("Error while moving \"%s\".\n"
					   		 "There is no space on the destination.");
					break;
				case XFER_LINK:
					error_string = _("Error while linking \"%s\".\n"
					   		 "There is no space on the destination.");
					break;
				default:
					g_assert_not_reached ();
					error_string = "";
					break;
				}
				text = g_strdup (error_string);
			} else {
				switch (xfer_info->kind) {
				case XFER_COPY:
				case XFER_DUPLICATE:
					error_string = _("Error while copying.\n"
					   		 "There is no space on the destination.");
					break;
				case XFER_MOVE_TO_TRASH:
				case XFER_MOVE:
					error_string = _("Error while moving.\n"
					   		 "There is no space on the destination.");
					break;
				case XFER_LINK:
					error_string = _("Error while linking.\n"
					   		 "There is no space on the destination.");
					break;
				default:
					g_assert_not_reached ();
					error_string = "";
					break;
				}
				text = g_strdup_printf (error_string, unescaped_name);
			}
			g_free (unescaped_name);
			
			result = nautilus_simple_dialog
				(parent_for_error_dialog (xfer_info), TRUE, text, 
				dialog_title, _("Stop"), NULL);
			g_free (text);

			return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
		}

		if (unescaped_name != NULL) {
		
			switch (xfer_info->kind) {
			case XFER_COPY:
			case XFER_DUPLICATE:
				error_string = _("Error \"%s\" while copying.\n"
						 "Would you like to continue?");
				break;
			case XFER_MOVE:
				error_string = _("Error \"%s\" while moving.\n"
						 "Would you like to continue?");
				break;
			case XFER_LINK:
				error_string = _("Error \"%s\" while linking.\n"
						 "Would you like to continue?");
				break;
			case XFER_DELETE:
			case XFER_EMPTY_TRASH:
			case XFER_MOVE_TO_TRASH:
				error_string = _("Error \"%s\" while deleting.\n"
						 "Would you like to continue?");
				break;
			default:
				error_string = "";
				break;
			}
	
			text = g_strdup_printf (error_string, 
				 gnome_vfs_result_to_string (progress_info->vfs_status));
		} else {

			switch (xfer_info->kind) {
			case XFER_COPY:
			case XFER_DUPLICATE:
				error_string = _("Error \"%s\" while copying \"%s\".\n"
						 "Would you like to continue?");
				break;
			case XFER_MOVE:
				error_string = _("Error \"%s\" while moving \"%s\".\n"
						 "Would you like to continue?");
				break;
			case XFER_LINK:
				error_string = _("Error \"%s\" while linking \"%s\".\n"
						 "Would you like to continue?");
				break;
			case XFER_DELETE:
			case XFER_EMPTY_TRASH:
			case XFER_MOVE_TO_TRASH:
				error_string = _("Error \"%s\" while deleting \"%s\".\n"
						 "Would you like to continue?");
				break;
			default:
				error_string = "";
				break;
			}
	
			text = g_strdup_printf (error_string, 
				 gnome_vfs_result_to_string (progress_info->vfs_status),
				 unescaped_name);
		}
		g_free (unescaped_name);

		result = nautilus_simple_dialog
			(parent_for_error_dialog (xfer_info), TRUE, text, 
			dialog_title,
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

	unescaped_name = nautilus_format_name_for_display (progress_info->target_name);
	text = g_strdup_printf (_("File \"%s\" already exists.\n"
				  "Would you like to replace it?"), 
				unescaped_name);
	g_free (unescaped_name);

	if (progress_info->duplicate_count == 1) {
		/* we are going to only get one duplicate alert, don't offer
		 * Replace All
		 */
		result = nautilus_simple_dialog
			(parent_for_error_dialog (xfer_info), TRUE, text, 
			 _("Conflict while copying"),
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
			(parent_for_error_dialog (xfer_info), TRUE, text, 
			 _("Conflict while copying"),
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
	char *result;
	char *unescaped_name;
	char *unescaped_result;

	const char *format;
	
	g_assert (name != NULL);

	unescaped_name = gnome_vfs_unescape_string (name, "/");
	g_free (name);

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
			/* appended to new link file */
			format = _("link to %s");
			break;
		case 2:
			/* appended to new link file */
			format = _("another link to %s");
			break;
		}
		unescaped_result = g_strdup_printf (format, unescaped_name);

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
			/* appended to new link file */
			format = _("%dnd link to %s");
			break;
		case 3:
			/* appended to new link file */
			format = _("%drd link to %s");
			break;
		default:
			/* appended to new link file */
			format = _("%dth link to %s");
			break;
		}
		unescaped_result = g_strdup_printf (format, count, unescaped_name);
	}

	result = gnome_vfs_escape_path_string (unescaped_result);
	
	g_free (unescaped_name);
	g_free (unescaped_result);

	return result;
}

/* Localizers: 
 * Feel free to leave out the st, nd, rd and th suffix or
 * make some or all of them match.
 */

/* localizers: tag used to detect the first copy of a file */
#define COPY_DUPLICATE_TAG _(" (copy)")
/* localizers: tag used to detect the second copy of a file */
#define ANOTHER_COPY_DUPLICATE_TAG _(" (another copy)")
/* localizers: tag used to detect the x1st copy of a file */
#define ST_COPY_DUPLICATE_TAG _("st copy)")
/* localizers: tag used to detect the x2nd copy of a file */
#define ND_COPY_DUPLICATE_TAG _("nd copy)")
/* localizers: tag used to detect the x3rd copy of a file */
#define RD_COPY_DUPLICATE_TAG _("rd copy)")
/* localizers: tag used to detect the xxth copy of a file */
#define TH_COPY_DUPLICATE_TAG _("th copy)")

/* localizers: appended to first file copy */
#define FIRST_COPY_DUPLICATE_FORMAT _("%s (copy)%s")
/* localizers: appended to second file copy */
#define SECOND_COPY_DUPLICATE_FORMAT _("%s (another copy)%s")
/* localizers: appended to x1st file copy */
#define ST_COPY_DUPLICATE_FORMAT _("%s (%dst copy)%s")
/* localizers: appended to x2nd file copy */
#define ND_COPY_DUPLICATE_FORMAT _("%s (%dnd copy)%s")
/* localizers: appended to x3rd file copy */
#define RD_COPY_DUPLICATE_FORMAT _("%s (%drd copy)%s")
/* localizers: appended to xxth file copy */
#define TH_COPY_DUPLICATE_FORMAT _("%s (%dth copy)%s")


static char *
extract_string_until (const char *original, const char *until_substring)
{
	char *result;
	
	g_assert ((int) strlen (original) >= until_substring - original);
	g_assert (until_substring - original >= 0);

	result = g_malloc (until_substring - original + 1);
	strncpy (result, original, until_substring - original);
	result[until_substring - original] = '\0';
	
	return result;
}

/* Dismantle a file name, separating the base name, the file suffix and removing any
 * (xxxcopy), etc. string. Figure out the count that corresponds to the given
 * (xxxcopy) substring.
 */
static void
parse_previous_duplicate_name (const char *name, char **name_base, const char **suffix,
	int *count)
{
	const char *tag;

	g_assert (name[0] != '\0');
	
	*suffix = strrchr (name + 1, '.');
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


	/* Check to see if we got one of st, nd, rd, th. */

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

	/* If we got one of st, nd, rd, th, fish out the duplicate number. */
	if (tag != NULL) {
		/* localizers: opening parentheses to match the "th copy)" string */
		tag = strstr (name, _(" ("));
		if (tag != NULL) {
			if (tag > *suffix) {
				/* handle case "foo. (22nd copy)" */
				*suffix = "";
			}
			*name_base = extract_string_until (name, tag);
			/* localizers: opening parentheses of the "th copy)" string */
			if (sscanf (tag, _(" (%d"), count) == 1) {
				if (*count < 1 || *count > 1000000) {
					/* keep the count within a reasonable range */
					*count = 0;
				}
				return;
			}
			*count = 0;
			return;
		}
	}

	
	*count = 0;
	if (**suffix != '\0') {
		*name_base = extract_string_until (name, *suffix);
	} else {
		*name_base = strdup (name);
	}
}

static char *
make_next_duplicate_name (const char *base, const char *suffix, int count)
{
	const char *format;
	char *result;


	if (count < 1) {
		char buffer [256];
		sprintf(buffer, "bad count %d in get_duplicate_name", count);
		g_warning (buffer);
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
			format = FIRST_COPY_DUPLICATE_FORMAT;
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
	char *unescaped_name;
	char *unescaped_result;
	char *result;

	unescaped_name = gnome_vfs_unescape_string (name, "/");
	g_free (name);

	unescaped_result = get_duplicate_name (unescaped_name, count_increment);
	g_free (unescaped_name);

	result = gnome_vfs_escape_path_string (unescaped_result);
	g_free (unescaped_result);
	
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
	case XFER_MOVE_TO_TRASH:
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

typedef struct {
	GHashTable		*debuting_uris;
	IconPositionIterator	*iterator;
} SyncXferInfo;

/* Low-level callback, called for every copy engine operation.
 * Generates notifications about new, deleted and moved files.
 */
static int
sync_xfer_callback (GnomeVFSXferProgressInfo *progress_info, gpointer data)
{
	GHashTable	     *debuting_uris;
	IconPositionIterator *position_iterator;

	if (data != NULL) {
		debuting_uris	  = ((SyncXferInfo *) data)->debuting_uris;
		position_iterator = ((SyncXferInfo *) data)->iterator;
	} else {
		debuting_uris     = NULL;
		position_iterator = NULL;
	}

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

				if (debuting_uris != NULL) {
					g_hash_table_insert (debuting_uris, g_strdup (progress_info->target_name), NULL);
				}
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

				if (debuting_uris != NULL) {
					g_hash_table_insert (debuting_uris, g_strdup (progress_info->target_name), NULL);
				}
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
			/* SyncXferInfo doesn't own the debuting_uris hash table - don't free it here.
			 */
			g_free (data);
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

/* is_special_link
 *
 * Check and see if file is one of our special links.
 * A special link ould be one of the following:
 * 	trash, home, volume
 */
static gboolean 
is_special_link (const GnomeVFSURI *uri)
{
	const char *local_path;
	char *escaped_path;
	
	if (!gnome_vfs_uri_is_local (uri)) {
		return FALSE;
	}
	
	local_path = gnome_vfs_uri_get_path (uri);
	if (local_path == NULL) {
		return FALSE;
	}
	
	escaped_path = gnome_vfs_unescape_string_for_display (local_path);
	if (escaped_path == NULL) {
		return FALSE;
	}

	if (nautilus_link_local_is_trash_link (escaped_path)) {
		g_free(escaped_path);
		return TRUE;
	}

	if (nautilus_link_local_is_home_link (escaped_path)) {
		g_free(escaped_path);
		return TRUE;
	}

	if (nautilus_link_local_is_volume_link (escaped_path)) {
		g_free(escaped_path);
		return TRUE;
	}

	g_free(escaped_path);
	return FALSE;
}

void
nautilus_file_operations_copy_move (const GList *item_uris,
				    GArray *relative_item_points,
				    const char *target_dir,
				    int copy_action,
				    GtkWidget *view,
				    void (*done_callback) (GHashTable *debuting_uris, gpointer data),
				    gpointer done_callback_data)
{
	const GList *p;
	GnomeVFSXferOptions move_options;
	GList *source_uri_list, *target_uri_list;
	GnomeVFSURI *source_uri, *target_uri;
	GnomeVFSURI *source_dir_uri, *target_dir_uri;
	GnomeVFSURI *trash_dir_uri;
	GnomeVFSURI *uri;

	XferInfo *xfer_info;
	SyncXferInfo *sync_xfer_info;
	GnomeVFSResult result;
	gboolean same_fs;
	gboolean is_trash_move;
	gboolean duplicate;
	
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
	
	duplicate = copy_action != GDK_ACTION_MOVE;
	move_options = GNOME_VFS_XFER_RECURSIVE;

	if (target_dir != NULL) {
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
		/* Filter out special Nautilus link files */
		if (!is_special_link (source_uri)) {
			source_uri_list = g_list_prepend (source_uri_list, source_uri);
			source_dir_uri = gnome_vfs_uri_get_parent (source_uri);
			
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
					target_dir_uri = gnome_vfs_uri_ref (source_dir_uri);
				}
			}
			target_uri_list = g_list_prepend (target_uri_list, target_uri);
			gnome_vfs_check_same_fs_uris (source_uri, target_uri, &same_fs);

			g_assert (target_dir_uri != NULL);
			duplicate &= same_fs;
			duplicate &= gnome_vfs_uri_equal (source_dir_uri, target_dir_uri);

			gnome_vfs_uri_unref (source_dir_uri);
		}
	}

	if (duplicate) {
		/* Copy operation, parents match -> duplicate operation. Ask gnome-vfs 
		 * to generate unique names for target files
		 */
		move_options |= GNOME_VFS_XFER_USE_UNIQUE_NAMES;
	}

	/* List may be NULL if we filtered all items out */
	if (source_uri_list == NULL) {
		if (target_dir_uri != NULL) {
			gnome_vfs_uri_unref (target_dir_uri);
		}
		if (target_uri_list != NULL) {
			gnome_vfs_uri_list_free (target_uri_list);
		}
		return;
	}
	
	source_uri_list = g_list_reverse (source_uri_list);
	target_uri_list = g_list_reverse (target_uri_list);


	if (copy_action == GDK_ACTION_MOVE) {
		move_options |= GNOME_VFS_XFER_REMOVESOURCE;
	} else if (copy_action == GDK_ACTION_LINK) {
		move_options |= GNOME_VFS_XFER_LINK_ITEMS;
	}
	
	/* set up the copy/move parameters */
	xfer_info = g_new0 (XferInfo, 1);
	xfer_info->parent_view = view;
	xfer_info->progress_dialog = NULL;

	if (relative_item_points->len > 0) {
		icon_position_iterator = icon_position_iterator_new (relative_item_points, item_uris);
	}
	
	if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) {
		/* localizers: progress dialog title */
		xfer_info->operation_title = _("Moving files");
		/* localizers: label prepended to the progress count */
		xfer_info->action_label =_("Files moved:");
		/* localizers: label prepended to the name of the current file moved */
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
		/* localizers: progress dialog title */
		xfer_info->operation_title = _("Creating links to files");
		/* localizers: label prepended to the progress count */
		xfer_info->action_label =_("Files linked:");
		/* localizers: label prepended to the name of the current file linked */
		xfer_info->progress_verb =_("Linking");
		xfer_info->preparation_name = _("Preparing to Create Links...");
		xfer_info->cleanup_name = _("Finishing Creating Links...");

		xfer_info->kind = XFER_LINK;
		xfer_info->show_progress_dialog =
			g_list_length ((GList *)item_uris) > 20;
	} else {
		/* localizers: progress dialog title */
		xfer_info->operation_title = _("Copying files");
		/* localizers: label prepended to the progress count */
		xfer_info->action_label =_("Files copied:");
		/* localizers: label prepended to the name of the current file copied */
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
				 FALSE,
				 _("You cannot copy items into the Trash."), 
				 _("Can't Copy to Trash"),
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
					 FALSE,
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					 ? _("The Trash must remain on the desktop.")
					 : _("You cannot copy the Trash."), 
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0)
					 ? _("Can't Change Trash Location")
					 : _("Can't Copy Trash"),
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
					 FALSE,
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					 ? _("You cannot move a folder into itself.")
					 : _("You cannot copy a folder into itself."), 
					 _("Can't Move Into Self"),
					 GNOME_STOCK_BUTTON_OK, NULL, NULL);			

				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}
		}
	}

	xfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	xfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
	xfer_info->done_callback = done_callback;
	xfer_info->done_callback_data = done_callback_data;
	xfer_info->debuting_uris = g_hash_table_new (g_str_hash, g_str_equal);

	sync_xfer_info = g_new (SyncXferInfo, 1);
	sync_xfer_info->iterator = icon_position_iterator;
	sync_xfer_info->debuting_uris = xfer_info->debuting_uris;

	if (result == GNOME_VFS_OK) {
		gnome_vfs_async_xfer (&xfer_info->handle, source_uri_list, target_uri_list,
		      		      move_options, GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
		      		      &update_xfer_callback, xfer_info,
		      		      &sync_xfer_callback, sync_xfer_info);
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
	GtkWidget *parent_view;
} NewFolderXferState;

static int
handle_new_folder_vfs_error (const GnomeVFSXferProgressInfo *progress_info, NewFolderXferState *state)
{
	const char *error_string;
	char *error_string_to_free;

	error_string_to_free = NULL;

	if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
		error_string = _("Error creating new folder.\n"
				"You do not have permissions to write to the destination.");
	} else if (progress_info->vfs_status == GNOME_VFS_ERROR_NO_SPACE) {
		error_string = _("Error creating new folder.\n"
				"There is no space on the destination.");
	} else {
		error_string = g_strdup_printf (_("Error \"%s\" creating new folder."), 
			gnome_vfs_result_to_string(progress_info->vfs_status));
	}

	nautilus_error_dialog (error_string, _("Error creating new folder"), GTK_WINDOW (state->parent_view));

	g_free (error_string_to_free);

	return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
}

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
			progress_info->duplicate_name = g_strdup_printf ("%s%%20%d", 
				progress_info->duplicate_name,
				progress_info->duplicate_count);
		}
		g_free (temp_string);
		return GNOME_VFS_XFER_ERROR_ACTION_SKIP;

	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
		return handle_new_folder_vfs_error (progress_info, state);

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
	state->parent_view = parent_view;

	/* pass in the target directory and the new folder name as a destination URI */
	parent_uri = gnome_vfs_uri_new (parent_dir);
	/* localizers: the initial name of a new folder  */
	uri = gnome_vfs_uri_append_file_name (parent_uri, _("untitled folder"));
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
				 FALSE,
				 _("The Trash must remain on the desktop."), 
				 _("Can't Change Trash Location"),
				 GNOME_STOCK_BUTTON_OK, NULL, NULL);			
			bail = TRUE;
		} else if (gnome_vfs_uri_is_parent (source_uri, trash_dir_uri, TRUE)) {
			item_name = nautilus_convert_to_formatted_name_for_display 
				(gnome_vfs_uri_extract_short_name (source_uri));
			text = g_strdup_printf
				(_("You cannot throw \"%s\" into the Trash."),
				 item_name);
			nautilus_simple_dialog
				(parent_view, FALSE, text,
				 _("Error Moving to Trash"),
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
		xfer_info = g_new0 (XferInfo, 1);
		xfer_info->parent_view = parent_view;
		xfer_info->progress_dialog = NULL;

		/* Do an arbitrary guess that an operation will take very little
		 * time and the progress shouldn't be shown.
		 */
		xfer_info->show_progress_dialog = g_list_length ((GList *)item_uris) > 20;

		/* localizers: progress dialog title */
		xfer_info->operation_title = _("Moving files to the Trash");
		/* localizers: label prepended to the progress count */
		xfer_info->action_label =_("Files thrown out:");
		/* localizers: label prepended to the name of the current file moved */
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

	xfer_info = g_new0 (XferInfo, 1);
	xfer_info->parent_view = parent_view;
	xfer_info->progress_dialog = NULL;
	xfer_info->show_progress_dialog = TRUE;

	/* localizers: progress dialog title */
	xfer_info->operation_title = _("Deleting files");
	/* localizers: label prepended to the progress count */
	xfer_info->action_label =_("Files deleted:");
	/* localizers: label prepended to the name of the current file deleted */
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
		xfer_info = g_new0 (XferInfo, 1);
		xfer_info->parent_view = parent_view;
		xfer_info->progress_dialog = NULL;
		xfer_info->show_progress_dialog = TRUE;

		/* localizers: progress dialog title */
		xfer_info->operation_title = _("Emptying the Trash");
		/* localizers: label prepended to the progress count */
		xfer_info->action_label =_("Files deleted:");
		/* localizers: label prepended to the name of the current file deleted */
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

	/* Just Say Yes if the preference says not to confirm. */
	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH, TRUE)) {
		return TRUE;
	}
	
	parent_window = GTK_WINDOW (gtk_widget_get_toplevel (parent_view));

	dialog = nautilus_yes_no_dialog (
		_("Are you sure you want to permanently delete "
		  "all of the items in the trash?"),
		_("Delete Trash Contents?"),
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
	if (!result) {
		printf("expected %s, got %s\n", expected_next_name, next_name);
	}
	g_free (next_name);

	return result;
}

void
nautilus_self_check_file_operations (void)
{

	/* test the next duplicate name generator */
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_(" (copy)"), _(" (another copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo"), _("foo (copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_(".bashrc"), _(".bashrc (copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_(".foo.txt"), _(".foo (copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo"), _("foo foo (copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo.txt"), _("foo (copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo.txt"), _("foo foo (copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo.txt txt"), _("foo foo (copy).txt txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo...txt"), _("foo.. (copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo..."), _("foo... (copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo. (copy)"), _("foo. (another copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (copy)"), _("foo (another copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (copy).txt"), _("foo (another copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (another copy)"), _("foo (3rd copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (another copy).txt"), _("foo (3rd copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo (another copy).txt"), _("foo foo (3rd copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (21st copy)"), _("foo (22nd copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (21st copy).txt"), _("foo (22nd copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (22nd copy)"), _("foo (23rd copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (22nd copy).txt"), _("foo (23rd copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (23rd copy)"), _("foo (24th copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (23rd copy).txt"), _("foo (24th copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (24th copy)"), _("foo (25th copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo (24th copy).txt"), _("foo (25th copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo (24th copy)"), _("foo foo (25th copy)")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo (24th copy).txt"), _("foo foo (25th copy).txt")), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (test_next_duplicate_name (_("foo foo (100000000000000th copy).txt"), _("foo foo (copy).txt")), TRUE);
}

#endif
