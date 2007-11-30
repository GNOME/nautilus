/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-operations.c - Nautilus file operations.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2007 Red Hat, Inc.

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
   
   Authors: Alexander Larsson <alexl@redhat.com>
            Ettore Perazzoli <ettore@gnu.org> 
            Pavel Cisler <pavel@eazel.com> 
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>
#include <math.h>
#include "nautilus-file-operations.h"

#include "nautilus-debug-log.h"
#include "nautilus-file-operations-progress.h"
#include "nautilus-file-changes-queue.h"
#include "nautilus-lib-self-check-functions.h"

#include "nautilus-progress-info.h"

#include <eel/eel-alert-dialog.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-pango-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>

#include <glib/gstdio.h>
#include <gnome.h>
#include <gdk/gdkdnd.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkwidget.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <gio/gfile.h>
#include <glib/gurifuncs.h>
#include <gio/gioscheduler.h>
#include "nautilus-file-changes-queue.h"
#include "nautilus-file-private.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-desktop-link-monitor.h"
#include "nautilus-global-preferences.h"
#include "nautilus-link.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-file-utilities.h"

static gboolean confirm_trash_auto_value;

/* TODO:
 *  Add cancellation
 *  Implement missing functions:
 *   duplicate, new file, new folder, empty trash, set_permissions recursive
 *  Make delete handle recursive deletes
 *  Use CommonJob in trash/delete code
 * TESTING!!!
 */

typedef struct {
	GIOJob *io_job;	
	GTimer *time;
	GtkWidget *parent_window;
	NautilusProgressInfo *progress;
	GCancellable *cancellable;
	gboolean aborted;
	GHashTable *skip_files;
	GHashTable *skip_readdir_error;
	gboolean skip_all_error;
	gboolean skip_all_conflict;
	gboolean merge_all;
	gboolean replace_all;
} CommonJob;

typedef struct {
	CommonJob common;
	gboolean is_move;
	GList *files;
	GFile *destination;
	GdkPoint *icon_positions;
	int n_icon_positions;
	int screen_num;
	GHashTable *debuting_files;
	NautilusCopyCallback  done_callback;
	gpointer done_callback_data;
} CopyMoveJob;

#define SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE 15
#define NSEC_PER_SEC 1000000000
#define NSEC_PER_MSEC 1000000

#define IS_IO_ERROR(__error, KIND) (((__error)->domain == G_IO_ERROR && (__error)->code == G_IO_ERROR_ ## KIND))

#define SKIP _("_Skip")
#define SKIP_ALL _("S_kip All")
#define RETRY _("_Retry")
#define REPLACE _("_Replace")
#define REPLACE_ALL _("Replace _All")
#define MERGE _("_Merge")
#define MERGE_ALL _("Merge _All")

static char *
format_time (int seconds)
{
	int minutes;
	int hours;
	char *res;

	if (seconds < 0) {
		/* Just to make sure... */
		seconds = 0;
	}
	
	if (seconds < 60) {
		return g_strdup_printf (ngettext ("%d second","%d seconds", (int) seconds), (int) seconds);
	}

	if (seconds < 60*60) {
		minutes = (seconds + 30) / 60;
		return g_strdup_printf (ngettext (_("%d minute"), _("%d minutes"), minutes), minutes);
	}

	hours = seconds / (60*60);
	
	if (seconds < 60*60*4) {
		char *h, *m;

		minutes = (seconds - hours * 60 * 60 + 30) / 60;
		
		h = g_strdup_printf (ngettext (_("%d hour"), _("%d hours"), hours), hours);
		m = g_strdup_printf (ngettext (_("%d minute"), _("%d minutes"), minutes), minutes);
		res = g_strconcat (h, ", ", m, NULL);
		g_free (h);
		g_free (m);
		return res;
	}
	
	return g_strdup_printf (_("about %d hours"), hours);
}

static char *
custom_full_name_to_string (char *format, va_list va)
{
	GFile *file;
	
	file = va_arg (va, GFile *);
	
	return g_file_get_parse_name (file);
}

static void
custom_full_name_skip (va_list *va)
{
	va_arg (*va, GFile *);
}

static char *
custom_basename_to_string (char *format, va_list va)
{
	GFile *file;
	GFileInfo *info;
	char *name, *basename;

	file = va_arg (va, GFile *);

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STD_DISPLAY_NAME,
				  0,
				  g_cancellable_get_current (),
				  NULL);
	
	name = NULL;
	if (info) {
		name = g_strdup (g_file_info_get_display_name (info));
		g_object_unref (info);
	}
	
	if (name == NULL) {
		basename = g_file_get_basename (file);
		if (g_utf8_validate (basename, -1, NULL)) {
			name = basename;
		} else {
			name = g_uri_escape_string (basename, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
			g_free (basename);
		}
	}
	
	return name;
}

static void
custom_basename_skip (va_list *va)
{
	va_arg (*va, GFile *);
}


static char *
custom_size_to_string (char *format, va_list va)
{
	goffset size;

	size = va_arg (va, goffset);
	return g_format_file_size_for_display (size);
}

static void
custom_size_skip (va_list *va)
{
	va_arg (*va, goffset);
}

static char *
custom_time_to_string (char *format, va_list va)
{
	int secs;

	secs = va_arg (va, int);
	return format_time (secs);
}

static void
custom_time_skip (va_list *va)
{
	va_arg (*va, int);
}

static EelPrintfHandler handlers[] = {
	{ 'F', custom_full_name_to_string, custom_full_name_skip },
	{ 'B', custom_basename_to_string, custom_basename_skip },
	{ 'S', custom_size_to_string, custom_size_skip },
	{ 'T', custom_time_to_string, custom_time_skip },
	{ 0 }
};


static char *
f (const char *format, ...) {
	va_list va;
	char *res;
	
	va_start (va, format);
	res = eel_strdup_vprintf_with_custom (handlers, format, va);
	va_end (va);

	return res;
}



#ifdef GIO_CONVERSION_DONE

typedef enum   TransferKind         TransferKind;
typedef struct TransferInfo         TransferInfo;
typedef struct IconPositionIterator IconPositionIterator;

enum TransferKind {
	TRANSFER_MOVE,
	TRANSFER_COPY,
	TRANSFER_DUPLICATE,
	TRANSFER_MOVE_TO_TRASH,
	TRANSFER_EMPTY_TRASH,
	TRANSFER_DELETE,
	TRANSFER_LINK
};

/* Copy engine callback state */
struct TransferInfo {
	GnomeVFSAsyncHandle *handle;
	NautilusFileOperationsProgress *progress_dialog;
	const char *operation_title;	/* "Copying files" */
	const char *action_label;	/* "Files copied:" */
	const char *progress_verb;	/* "Copying" */
	const char *preparation_name;	/* "Preparing To Copy..." */
	const char *cleanup_name;	/* "Finishing Move..." */
	GnomeVFSXferErrorMode error_mode;
	GnomeVFSXferOverwriteMode overwrite_mode;
	GtkWidget *parent_view;
	TransferKind kind;
	void (* done_callback) (GHashTable *debuting_uris, gpointer data);
	gpointer done_callback_data;
	GHashTable *debuting_uris;
	gboolean cancelled;	
	IconPositionIterator *iterator;
};

static TransferInfo *
transfer_info_new (GtkWidget *parent_view)
{
	TransferInfo *result;
	
	result = g_new0 (TransferInfo, 1);
	result->parent_view = parent_view;
	
	eel_add_weak_pointer (&result->parent_view);
	
	return result;
}

static void
transfer_info_destroy (TransferInfo *transfer_info)
{
	eel_remove_weak_pointer (&transfer_info->parent_view);
	
	if (transfer_info->progress_dialog != NULL) {
		nautilus_file_operations_progress_done (transfer_info->progress_dialog);
	}
	
	if (transfer_info->debuting_uris != NULL) {
		g_hash_table_destroy (transfer_info->debuting_uris);
	}
	
	g_free (transfer_info);
}

#endif /* GIO_CONVERSION_DONE */

static void
setup_autos (void)
{
	static gboolean setup_autos = FALSE;
	if (!setup_autos) {
		setup_autos = TRUE;
		eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
						  &confirm_trash_auto_value);
	}
}

#ifdef GIO_CONVERSION_DONE

/* Struct used to control applying icon positions to 
 * top level items during a copy, drag, new folder creation and
 * link creation
 */
struct IconPositionIterator {
	GdkPoint *icon_positions;
	int last_icon_position_index;
	GList *uris;
	const GList *last_uri;
	int screen;
	gboolean is_source_iterator;
};

static IconPositionIterator *
icon_position_iterator_new (GArray *icon_positions,
			    const GList *uris,
			    int screen,
			    gboolean is_source_iterator)
{
	IconPositionIterator *result;
	guint index;

	g_assert (icon_positions->len == g_list_length ((GList *)uris));
	result = g_new (IconPositionIterator, 1);
	
	/* make our own copy of the icon locations */
	result->icon_positions = g_new (GdkPoint, icon_positions->len);
	for (index = 0; index < icon_positions->len; index++) {
		result->icon_positions[index] = g_array_index (icon_positions, GdkPoint, index);
	}
	result->last_icon_position_index = 0;

	result->uris = eel_g_str_list_copy ((GList *)uris);
	result->last_uri = result->uris;
	result->screen = screen;
	result->is_source_iterator = is_source_iterator;

	return result;
}

static IconPositionIterator *
icon_position_iterator_new_single (GdkPoint *icon_position,
				   const char *uri,
				   int screen,
				   gboolean is_source_iterator)
{
	IconPositionIterator *iterator;
	GArray *icon_positions;
	GList *uris;

	if (icon_position == NULL || uri == NULL) {
		return NULL;
	}

	icon_positions = g_array_sized_new (FALSE, FALSE, sizeof (GdkPoint), 1);
	g_array_insert_val (icon_positions, 0, *icon_position);

	uris = g_list_append (NULL, (char *) uri);

	iterator = icon_position_iterator_new (icon_positions, uris, screen, is_source_iterator);

	g_list_free (uris);
	g_array_free (icon_positions, TRUE);

	return iterator;
}

static void
icon_position_iterator_free (IconPositionIterator *position_iterator)
{
	if (position_iterator == NULL) {
		return;
	}
	
	g_free (position_iterator->icon_positions);
	eel_g_list_free_deep (position_iterator->uris);
	g_free (position_iterator);
}

static gboolean
icon_position_iterator_get_next (IconPositionIterator *position_iterator,
				 const char *next_source_uri,
				 const char *next_target_uri,
				 GdkPoint *point)
{
	const char *next_uri;

	if (position_iterator == NULL) {
		return FALSE;
	}

	if (position_iterator->is_source_iterator) {
		next_uri = next_source_uri;
	} else {
		next_uri = next_target_uri;
	}

	for (;;) {
		if (position_iterator->last_uri == NULL) {
			/* we are done, no more points left */
			return FALSE;
		}

		/* Scan for the next point that matches the source_name
		 * uri.
		 */
		if (strcmp ((const char *) position_iterator->last_uri->data, 
			    next_uri) == 0) {
			break;
		}
		
		/* Didn't match -- a uri must have been skipped by the copy 
		 * engine because of a name conflict. All we need to do is 
		 * skip ahead too.
		 */
		position_iterator->last_uri = position_iterator->last_uri->next;
		position_iterator->last_icon_position_index++; 
	}

	/* apply the location to the target file */
	*point = position_iterator->icon_positions
		[position_iterator->last_icon_position_index];

	/* advance to the next point for next time */
	position_iterator->last_uri = position_iterator->last_uri->next;
	position_iterator->last_icon_position_index++; 

	return TRUE;
}

static void
icon_position_iterator_update_uri (IconPositionIterator *position_iterator,
				   const char *old_uri,
				   const char *new_uri_fragment)
{
	GnomeVFSURI *uri, *parent_uri;
	GList *l;

	if (position_iterator == NULL) {
		return;
	}

	l = g_list_find_custom (position_iterator->uris,
			        old_uri,
				(GCompareFunc) strcmp);
	if (l == NULL) {
		return;
	}

	uri = gnome_vfs_uri_new (old_uri);
	parent_uri = gnome_vfs_uri_get_parent (uri);
	gnome_vfs_uri_unref (uri);

	if (parent_uri == NULL) {
		return;
	}
	
	uri = gnome_vfs_uri_append_string (parent_uri, new_uri_fragment);

	g_free (l->data);
	l->data = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);

	gnome_vfs_uri_unref (uri);
	gnome_vfs_uri_unref (parent_uri);
}

static char *
ellipsize_string_for_dialog (PangoContext *context, const char *str)
{
	int maximum_width;
	char *result;
	PangoLayout *layout;
	PangoFontMetrics *metrics;

	layout = pango_layout_new (context);

	metrics = pango_context_get_metrics (
		context, pango_context_get_font_description (context), NULL);

	maximum_width = pango_font_metrics_get_approximate_char_width (metrics) * 25 / PANGO_SCALE;

	pango_font_metrics_unref (metrics);

	eel_pango_layout_set_text_ellipsized (
		layout, str, maximum_width, EEL_ELLIPSIZE_MIDDLE);

	result = g_strdup (pango_layout_get_text (layout));

	g_object_unref (layout);

	return result;
}

static char *
format_and_ellipsize_uri_for_dialog (GtkWidget *context, const char *uri)
{
	char *unescaped, *result;

	unescaped = eel_format_uri_for_display (uri);
	result = ellipsize_string_for_dialog (
		gtk_widget_get_pango_context (context), unescaped);
	g_free (unescaped);

	return result;
}

static char *
extract_and_ellipsize_file_name_for_dialog (GtkWidget *context, const char *uri)
{
	char *basename;
	char *unescaped, *result;
	
	basename = g_path_get_basename (uri);
	g_return_val_if_fail (basename != NULL, NULL);

	unescaped = gnome_vfs_unescape_string_for_display (basename);
	result = ellipsize_string_for_dialog (
		gtk_widget_get_pango_context (context), unescaped);
	g_free (unescaped);
	g_free (basename);

	return result;
}

static GtkWidget *
parent_for_error_dialog (TransferInfo *transfer_info)
{
	if (transfer_info->progress_dialog != NULL) {
		return GTK_WIDGET (transfer_info->progress_dialog);
	}

	return transfer_info->parent_view;
}

static void
handle_response_callback (GtkDialog *dialog, int response, TransferInfo *transfer_info)
{
	transfer_info->cancelled = TRUE;
}

static void
handle_close_callback (GtkDialog *dialog, TransferInfo *transfer_info)
{
	transfer_info->cancelled = TRUE;
}

static void
create_transfer_dialog (const GnomeVFSXferProgressInfo *progress_info,
			TransferInfo *transfer_info)
{
	g_return_if_fail (transfer_info->progress_dialog == NULL);

	transfer_info->progress_dialog = nautilus_file_operations_progress_new 
		(transfer_info->operation_title, "", "", "", 0, 0, TRUE);

	/* Treat clicking on the close box or use of the escape key
	 * the same as clicking cancel.
	 */
	g_signal_connect (transfer_info->progress_dialog,
			  "response",
			  G_CALLBACK (handle_response_callback),
			  transfer_info);
	g_signal_connect (transfer_info->progress_dialog,
			  "close",
			  G_CALLBACK (handle_close_callback),
			  transfer_info);

	/* Make the progress dialog show up over the window we are copying into */
	if (transfer_info->parent_view != NULL) {
		GtkWidget *toplevel;

		/* Transient-for-desktop are visible on all desktops, we don't want
		   that. */
		toplevel = gtk_widget_get_toplevel (transfer_info->parent_view);
		if (toplevel != NULL &&
		    g_object_get_data (G_OBJECT (toplevel), "is_desktop_window") == NULL) {
			gtk_window_set_transient_for (GTK_WINDOW (transfer_info->progress_dialog), 
						      GTK_WINDOW (toplevel));
		}
	}
}

/* TODO: This should really use the gio display name */
static const char *
get_vfs_method_display_name (char *method)
{
	if (g_ascii_strcasecmp (method, "computer") == 0 ) {
		return _("Computer");
	} else if (g_ascii_strcasecmp (method, "network") == 0 ) {
		return _("Network");
	} else if (g_ascii_strcasecmp (method, "fonts") == 0 ) {
		return _("Fonts");
	} else if (g_ascii_strcasecmp (method, "themes") == 0 ) {
		return _("Themes");
	} else if (g_ascii_strcasecmp (method, "burn") == 0 ) {
		return _("CD/DVD Creator");
	} else if (g_ascii_strcasecmp (method, "smb") == 0 ) {
		return _("Windows Network");
	} else if (g_ascii_strcasecmp (method, "dns-sd") == 0 ) {
		/* translators: this is the title of the "dns-sd:///" location */
		return _("Services in");
	}
	return NULL;
}

/* TODO: This should really use the gio display name */
static char *
get_uri_shortname_for_display (GnomeVFSURI *uri)
{
	char *utf8_name, *name, *tmp;
	char *text_uri, *local_file;
	gboolean validated;
	const char *method;

	
	validated = FALSE;
	name = gnome_vfs_uri_extract_short_name (uri);
	if (name == NULL) {
		name = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
	} else if (g_ascii_strcasecmp (uri->method_string, "file") == 0) {
		text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
		local_file = g_filename_from_uri (text_uri, NULL, NULL);
		g_free (name);
		if (local_file == NULL) { /* Happens for e.g. file:///# */
			local_file = g_strdup ("/");
		}
		name = g_filename_display_basename (local_file);
		g_free (local_file);
		g_free (text_uri);
		validated = TRUE;
	} else if (!gnome_vfs_uri_has_parent (uri)) {
		/* Special-case the display name for roots that are not local files */
		method = get_vfs_method_display_name (uri->method_string);
		if (method == NULL) {
			method = uri->method_string;
		}
		
		if (name == NULL ||
		    strcmp (name, GNOME_VFS_URI_PATH_STR) == 0) {
			g_free (name);
			name = g_strdup (method);
		} else {
			tmp = name;
			name = g_strdup_printf ("%s: %s", method, name);
			g_free (tmp);
		}
	}

	if (!validated && !g_utf8_validate (name, -1, NULL)) {
		utf8_name = eel_make_valid_utf8 (name);
		g_free (name);
		name = utf8_name;
	}

	return name;
}

static void
progress_dialog_set_to_from_item_text (NautilusFileOperationsProgress *dialog,
				       const char *progress_verb,
				       const char *from_uri, const char *to_uri, 
				       gulong index, gulong size)
{
	char *item;
	char *from_path;
	char *from_text;
	char *to_path;
	char *to_text;
	char *progress_label_text;
	const char *hostname;
	const char *from_prefix;
	const char *to_prefix;
	GnomeVFSURI *uri;
	int length;

	item = NULL;
	from_text = NULL;
	to_text = NULL;
	from_prefix = "";
	to_prefix = "";
	progress_label_text = NULL;

	if (from_uri != NULL) {
		uri = gnome_vfs_uri_new (from_uri);
		item = get_uri_shortname_for_display (uri);
		from_path = gnome_vfs_uri_extract_dirname (uri);
		hostname = NULL;

		/* remove the last '/' */
		length = strlen (from_path);
		if (from_path [length - 1] == '/') {
			from_path [length - 1] = '\0';
		}

		if (strcmp (uri->method_string, "file") != 0) {
			hostname = gnome_vfs_uri_get_host_name (uri);
		}
		if (hostname) {
			from_text = g_strdup_printf (_("%s on %s"),
				from_path, hostname);
			g_free (from_path);
		} else {
			from_text = from_path;
		}
		
		gnome_vfs_uri_unref (uri);
		g_assert (progress_verb);
		progress_label_text = g_strdup_printf ("%s", progress_verb);
		/* "From" dialog label, source path gets placed next to it in the dialog */
		from_prefix = _("From:");
	}

	if (to_uri != NULL) {
		uri = gnome_vfs_uri_new (to_uri);
		to_path = gnome_vfs_uri_extract_dirname (uri);
		hostname = NULL;

		/* remove the last '/' */
		length = strlen (to_path);
		if (to_path [length - 1] == '/') {
			to_path [length - 1] = '\0';
		}

		if (strcmp (uri->method_string, "file") != 0) {
			hostname = gnome_vfs_uri_get_host_name (uri);
		}
		if (hostname) {
			to_text = g_strdup_printf (_("%s on %s"),
				to_path, hostname);
			g_free (to_path);
		} else {
			to_text = to_path;
		}

		gnome_vfs_uri_unref (uri);
		/* "To" dialog label, source path gets placed next to it in the dialog */
		to_prefix = _("To:");
	}

	nautilus_file_operations_progress_new_file
		(dialog,
		 progress_label_text != NULL ? progress_label_text : "",
		 item != NULL ? item : "",
		 from_text != NULL ? from_text : "",
		 to_text != NULL ? to_text : "",
		 from_prefix, to_prefix, index, size);

	g_free (progress_label_text);
	g_free (item);
	g_free (from_text);
	g_free (to_text);
}

static int
handle_transfer_ok (const GnomeVFSXferProgressInfo *progress_info,
		    TransferInfo *transfer_info)
{
	if (transfer_info->cancelled
		&& progress_info->phase != GNOME_VFS_XFER_PHASE_COMPLETED) {
		/* If cancelled, delete any partially copied files that are laying
		 * around and return. Don't delete the source though..
		 */
		if (progress_info->target_name != NULL
		    && progress_info->source_name != NULL
		    && strcmp (progress_info->source_name, progress_info->target_name) != 0
		    && progress_info->bytes_total != progress_info->bytes_copied) {
			GList *delete_me;
			GtkWidget *toplevel;

			delete_me = g_list_prepend (NULL, g_file_new_for_uri (progress_info->target_name));
			toplevel = gtk_widget_get_toplevel (transfer_info->parent_view);
			nautilus_file_operations_delete (delete_me, GTK_WINDOW (toplevel), NULL, NULL);
			eel_g_object_list_free (delete_me);
		}

		return 0;
	}
	
	switch (progress_info->phase) {
	case GNOME_VFS_XFER_PHASE_INITIAL:
		create_transfer_dialog (progress_info, transfer_info);
		return 1;

	case GNOME_VFS_XFER_PHASE_COLLECTING:
		if (transfer_info->progress_dialog != NULL) {
			nautilus_file_operations_progress_set_operation_string
				(transfer_info->progress_dialog,
				 transfer_info->preparation_name);
		}
		return 1;

	case GNOME_VFS_XFER_PHASE_READYTOGO:
		if (transfer_info->progress_dialog != NULL) {
			nautilus_file_operations_progress_set_operation_string
				(transfer_info->progress_dialog,
				 transfer_info->action_label);
			nautilus_file_operations_progress_set_total
				(transfer_info->progress_dialog,
				 progress_info->files_total,
				 progress_info->bytes_total);
		}
		return 1;
				 
	case GNOME_VFS_XFER_PHASE_DELETESOURCE:
		nautilus_file_changes_consume_changes (FALSE);
		if (transfer_info->progress_dialog != NULL) {
			progress_dialog_set_to_from_item_text
				(transfer_info->progress_dialog,
				 transfer_info->progress_verb,
				 progress_info->source_name,
				 NULL,
				 progress_info->file_index,
				 progress_info->file_size);

			nautilus_file_operations_progress_update_sizes
				(transfer_info->progress_dialog,
				 MIN (progress_info->bytes_copied, 
				      progress_info->bytes_total),
				 MIN (progress_info->total_bytes_copied,
				      progress_info->bytes_total));
		}
		return 1;

	case GNOME_VFS_XFER_PHASE_MOVING:
	case GNOME_VFS_XFER_PHASE_OPENSOURCE:
	case GNOME_VFS_XFER_PHASE_OPENTARGET:
		/* fall through */
	case GNOME_VFS_XFER_PHASE_COPYING:
		if (transfer_info->progress_dialog != NULL) {
			progress_dialog_set_to_from_item_text (transfer_info->progress_dialog,
							       transfer_info->progress_verb,
							       progress_info->source_name,
							       progress_info->target_name,
							       progress_info->file_index,
							       progress_info->file_size);
			nautilus_file_operations_progress_update_sizes (transfer_info->progress_dialog,
									MIN (progress_info->bytes_copied, 
									     progress_info->bytes_total),
									MIN (progress_info->total_bytes_copied,
									     progress_info->bytes_total));
		}
		return 1;

	case GNOME_VFS_XFER_PHASE_CLEANUP:
		if (transfer_info->progress_dialog != NULL) {
			nautilus_file_operations_progress_clear
				(transfer_info->progress_dialog);
			nautilus_file_operations_progress_set_operation_string
				(transfer_info->progress_dialog,
				 transfer_info->cleanup_name);
		}
		return 1;

	case GNOME_VFS_XFER_PHASE_COMPLETED:
		nautilus_file_changes_consume_changes (TRUE);
		if (transfer_info->done_callback != NULL) {
			transfer_info->done_callback (transfer_info->debuting_uris,
						      transfer_info->done_callback_data);
			/* done_callback now owns (will free) debuting_uris */
			transfer_info->debuting_uris = NULL;
		}

		transfer_info_destroy (transfer_info);
		return 1;

	default:
		return 1;
	}
}

typedef enum {
	ERROR_READ_ONLY,
	ERROR_NOT_READABLE,
	ERROR_NOT_WRITABLE,
	ERROR_NOT_ENOUGH_PERMISSIONS,
	ERROR_NO_SPACE,
	ERROR_SOURCE_IN_TARGET,
	ERROR_OTHER
} NautilusFileOperationsErrorKind;

typedef enum {
	ERROR_LOCATION_UNKNOWN,
	ERROR_LOCATION_SOURCE,
	ERROR_LOCATION_SOURCE_PARENT,
	ERROR_LOCATION_SOURCE_OR_PARENT,
	ERROR_LOCATION_TARGET
} NautilusFileOperationsErrorLocation;


static void
build_error_string (const char *source_name, const char *target_name,
		    TransferKind operation_kind,
		    NautilusFileOperationsErrorKind error_kind,
		    NautilusFileOperationsErrorLocation error_location,
		    GnomeVFSResult error,
		    char **error_string, char **detail_string)
{
	/* Avoid clever message composing here, just use brute force and
	 * duplicate the different flavors of error messages for all the
	 * possible permutations.
	 * That way localizers have an easier time and can even rearrange the
	 * order of the words in the messages easily.
	 */

	char *error_format;
	char *detail_format;
	
	error_format = NULL;
	detail_format = NULL;

	*error_string = NULL;
	*detail_string = NULL;

	if (error_location == ERROR_LOCATION_SOURCE_PARENT) {

		switch (operation_kind) {
		case TRANSFER_MOVE:
		case TRANSFER_MOVE_TO_TRASH:
			if (error_kind == ERROR_READ_ONLY) {
				*error_string = g_strdup (_("Error while moving."));
				detail_format = _("\"%s\" cannot be moved because it is on "
						  "a read-only disk.");
			}
			break;

		case TRANSFER_DELETE:
		case TRANSFER_EMPTY_TRASH:
			switch (error_kind) {
			case ERROR_NOT_ENOUGH_PERMISSIONS:
			case ERROR_NOT_WRITABLE:
				*error_string = g_strdup (_("Error while deleting."));
				detail_format = _("\"%s\" cannot be deleted because you do not have "
						  "permissions to modify its parent folder.");
				break;
			
			case ERROR_READ_ONLY:
				*error_string = g_strdup (_("Error while deleting."));
				detail_format = _("\"%s\" cannot be deleted because it is on "
						  "a read-only disk.");
				break;

			default:
				break;
			}
			break;

		default:
			g_assert_not_reached ();
			break;
		}
		
		if (detail_format != NULL && source_name != NULL) {
			*detail_string = g_strdup_printf (detail_format, source_name);
		}

	} else if (error_location == ERROR_LOCATION_SOURCE_OR_PARENT) {

		g_assert (source_name != NULL);

		/* FIXME: Would be better if we could distinguish source vs parent permissions
		 * better somehow. The GnomeVFS copy engine would have to do some snooping
		 * after the failure in this case.
		 */
		switch (operation_kind) {
		case TRANSFER_MOVE:
			switch (error_kind) {
			case ERROR_NOT_ENOUGH_PERMISSIONS:
				*error_string = g_strdup (_("Error while moving."));
				detail_format = _("\"%s\" cannot be moved because you do not have "
						  "permissions to change it or its parent folder.");
				break;
			case ERROR_SOURCE_IN_TARGET:
				*error_string = g_strdup (_("Error while moving."));
				detail_format = _("Cannot move \"%s\" because it or its parent folder "
						  "are contained in the destination.");
				break;
			default:
				break;
			}
			break;
		case TRANSFER_MOVE_TO_TRASH:
			if (error_kind == ERROR_NOT_ENOUGH_PERMISSIONS) {
				*error_string = g_strdup (_("Error while moving."));
				detail_format = _("Cannot move \"%s\" to the trash because you do not have "
						  "permissions to change it or its parent folder.");
			}
			break;

		default:
			g_assert_not_reached ();
			break;
		}

		if (detail_format != NULL && source_name != NULL) {
			*detail_string = g_strdup_printf (detail_format, source_name);
		}

	} else if (error_location == ERROR_LOCATION_SOURCE) {

		g_assert (source_name != NULL);

		switch (operation_kind) {
		case TRANSFER_COPY:
		case TRANSFER_DUPLICATE:
			if (error_kind == ERROR_NOT_READABLE) {
				*error_string = g_strdup (_("Error while copying."));
				detail_format = _("\"%s\" cannot be copied because you do not have "
						  "permissions to read it.");
			}
			break;

		default:
			g_assert_not_reached ();
			break;
		}

		if (detail_format != NULL && source_name != NULL) {
			*detail_string = g_strdup_printf (detail_format, source_name);
		}

	} else if (error_location == ERROR_LOCATION_TARGET) {

		if (error_kind == ERROR_NO_SPACE) {
			switch (operation_kind) {
			case TRANSFER_COPY:
			case TRANSFER_DUPLICATE:
				error_format = _("Error while copying to \"%s\".");
				*detail_string = g_strdup (_("There is not enough space on the destination."));
				break;
			case TRANSFER_MOVE_TO_TRASH:
			case TRANSFER_MOVE:
				error_format = _("Error while moving to \"%s\".");
				*detail_string = g_strdup (_("There is not enough space on the destination."));
				break;
			case TRANSFER_LINK:
				error_format = _("Error while creating link in \"%s\".");
				*detail_string = g_strdup (_("There is not enough space on the destination."));
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		} else {
			switch (operation_kind) {
			case TRANSFER_COPY:
			case TRANSFER_DUPLICATE:
				if (error_kind == ERROR_NOT_ENOUGH_PERMISSIONS) {
					error_format = _("Error while copying to \"%s\".");
					*detail_string = g_strdup (_("You do not have permissions to write to "
					   		            "this folder."));
				} else if (error_kind == ERROR_NOT_WRITABLE) {
					error_format = _("Error while copying to \"%s\".");
					*detail_string = g_strdup (_("The destination disk is read-only."));
				} 
				break;
			case TRANSFER_MOVE:
			case TRANSFER_MOVE_TO_TRASH:
				if (error_kind == ERROR_NOT_ENOUGH_PERMISSIONS) {
					error_format = _("Error while moving items to \"%s\".");
					*detail_string = g_strdup (_("You do not have permissions to write to "
					   		            "this folder."));
				} else if (error_kind == ERROR_NOT_WRITABLE) {
					error_format = _("Error while moving items to \"%s\".");
					*detail_string = g_strdup (_("The destination disk is read-only."));
				} 

				break;
			case TRANSFER_LINK:
				if (error_kind == ERROR_NOT_ENOUGH_PERMISSIONS) {
					error_format = _("Error while creating links in \"%s\".");
					*detail_string = g_strdup (_("You do not have permissions to write to "
					   		             "this folder."));
				} else if (error_kind == ERROR_NOT_WRITABLE) {
					error_format = _("Error while creating links in \"%s\".");
					*detail_string = g_strdup (_("The destination disk is read-only."));
				} 
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
		if (error_format != NULL && target_name != NULL) {
			*error_string = g_strdup_printf (error_format, target_name);
		}
	}
	
	if (*error_string == NULL) {
		/* None of the specific error messages apply, use a catch-all
		 * generic error
		 */
		g_message ("Hit unexpected error \"%s\" while doing a file operation.",
			   gnome_vfs_result_to_string (error));

		/* FIXMEs: we need to consider a single item
		 * move/copy and not offer to continue in that case
		 */
		if (source_name != NULL) {
			switch (operation_kind) {
			case TRANSFER_COPY:
			case TRANSFER_DUPLICATE:
				error_format = _("Error \"%s\" while copying \"%s\".");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			case TRANSFER_MOVE:
				error_format = _("Error \"%s\" while moving \"%s\".");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			case TRANSFER_LINK:
				error_format = _("Error \"%s\" while creating a link to \"%s\".");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			case TRANSFER_DELETE:
			case TRANSFER_EMPTY_TRASH:
			case TRANSFER_MOVE_TO_TRASH:
				error_format = _("Error \"%s\" while deleting \"%s\".");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			default:
				g_assert_not_reached ();
				break;
			}
	
			*error_string = g_strdup_printf (error_format, 
							 gnome_vfs_result_to_string (error),
							 source_name);
		} else {
			switch (operation_kind) {
			case TRANSFER_COPY:
			case TRANSFER_DUPLICATE:
				error_format = _("Error \"%s\" while copying.");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			case TRANSFER_MOVE:
				error_format = _("Error \"%s\" while moving.");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			case TRANSFER_LINK:
				error_format = _("Error \"%s\" while linking.");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			case TRANSFER_DELETE:
			case TRANSFER_EMPTY_TRASH:
			case TRANSFER_MOVE_TO_TRASH:
				error_format = _("Error \"%s\" while deleting.");
				*detail_string = g_strdup (_("Would you like to continue?"));
				break;
			default:
				g_assert_not_reached ();
				break;
			}
	
			*error_string = g_strdup_printf (error_format, 
						         gnome_vfs_result_to_string (error));
		}
	}
}

static int
handle_transfer_vfs_error (const GnomeVFSXferProgressInfo *progress_info,
			   TransferInfo *transfer_info)
{
	/* Notice that the error mode in `transfer_info' is the one we have been
         * requested, but the transfer is always performed in mode
         * `GNOME_VFS_XFER_ERROR_MODE_QUERY'.
         */

	int error_dialog_button_pressed;
	int error_dialog_result;
	char *text;
	char *detail;
	char *formatted_source_name;
	char *formatted_target_name;
	NautilusFileOperationsErrorKind error_kind;
	NautilusFileOperationsErrorLocation error_location;
	
	switch (transfer_info->error_mode) {
	case GNOME_VFS_XFER_ERROR_MODE_QUERY:

		/* transfer error, prompt the user to continue or cancel */

		/* stop timeout while waiting for user */
		nautilus_file_operations_progress_pause_timeout (transfer_info->progress_dialog);

		formatted_source_name = NULL;
		formatted_target_name = NULL;

		if (progress_info->source_name != NULL) {
			formatted_source_name = format_and_ellipsize_uri_for_dialog
				(parent_for_error_dialog (transfer_info),
				 progress_info->source_name);
		}

		if (progress_info->target_name != NULL) {
			formatted_target_name = format_and_ellipsize_uri_for_dialog
				(parent_for_error_dialog (transfer_info),
				 progress_info->target_name);
		}

		error_kind = ERROR_OTHER;
		error_location = ERROR_LOCATION_UNKNOWN;
		
		/* Single out a few common error conditions for which we have
		 * custom-taylored error messages.
		 */
		if ((progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM
				|| progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY)
			&& (transfer_info->kind == TRANSFER_DELETE
				|| transfer_info->kind == TRANSFER_EMPTY_TRASH)) {
			error_location = ERROR_LOCATION_SOURCE_PARENT;
			error_kind = ERROR_READ_ONLY;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED
			&& (transfer_info->kind == TRANSFER_DELETE
				|| transfer_info->kind == TRANSFER_EMPTY_TRASH)) {
			error_location = ERROR_LOCATION_SOURCE_PARENT;
			error_kind = ERROR_NOT_ENOUGH_PERMISSIONS;
		} else if ((progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM
				|| progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY)
			&& (transfer_info->kind == TRANSFER_MOVE
				|| transfer_info->kind == TRANSFER_MOVE_TO_TRASH)
			&& progress_info->phase != GNOME_VFS_XFER_CHECKING_DESTINATION) {
			error_location = ERROR_LOCATION_SOURCE_PARENT;
			error_kind = ERROR_READ_ONLY;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED
			&& transfer_info->kind == TRANSFER_MOVE
			&& progress_info->phase == GNOME_VFS_XFER_PHASE_OPENTARGET) {
			error_location = ERROR_LOCATION_TARGET;
			error_kind = ERROR_NOT_ENOUGH_PERMISSIONS;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED
			&& (transfer_info->kind == TRANSFER_MOVE
				|| transfer_info->kind == TRANSFER_MOVE_TO_TRASH)
			&& progress_info->phase != GNOME_VFS_XFER_CHECKING_DESTINATION) {
			error_location = ERROR_LOCATION_SOURCE_OR_PARENT;
			error_kind = ERROR_NOT_ENOUGH_PERMISSIONS;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED
			&& (transfer_info->kind == TRANSFER_COPY
				|| transfer_info->kind == TRANSFER_DUPLICATE)
			&& (progress_info->phase == GNOME_VFS_XFER_PHASE_OPENSOURCE
				|| progress_info->phase == GNOME_VFS_XFER_PHASE_COLLECTING
				|| progress_info->phase == GNOME_VFS_XFER_PHASE_INITIAL)) {
			error_location = ERROR_LOCATION_SOURCE;
			error_kind = ERROR_NOT_READABLE;
		} else if ((progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM
				|| progress_info->vfs_status == GNOME_VFS_ERROR_READ_ONLY)
			&& progress_info->phase == GNOME_VFS_XFER_CHECKING_DESTINATION) {
			error_location = ERROR_LOCATION_TARGET;
			error_kind = ERROR_NOT_WRITABLE;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED
			&& progress_info->phase == GNOME_VFS_XFER_CHECKING_DESTINATION) {
			error_location = ERROR_LOCATION_TARGET;
			error_kind = ERROR_NOT_ENOUGH_PERMISSIONS;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_NO_SPACE) {
			error_location = ERROR_LOCATION_TARGET;
			error_kind = ERROR_NO_SPACE;
		} else if (progress_info->vfs_status == GNOME_VFS_ERROR_DIRECTORY_NOT_EMPTY
			   && transfer_info->kind == TRANSFER_MOVE) {
			error_location = ERROR_LOCATION_SOURCE_OR_PARENT;
			error_kind = ERROR_SOURCE_IN_TARGET;
		}

		build_error_string (formatted_source_name, formatted_target_name,
				    transfer_info->kind,
				    error_kind, error_location,
				    progress_info->vfs_status,
				    &text, &detail);

		if (error_location == ERROR_LOCATION_TARGET ||
		    error_kind == ERROR_SOURCE_IN_TARGET) {
			/* We can't continue, just tell the user. */
			eel_run_simple_dialog (parent_for_error_dialog (transfer_info),
				TRUE, GTK_MESSAGE_ERROR, text, detail, GTK_STOCK_OK, NULL);
			error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_ABORT;

		} else if (progress_info->files_total == 1) {
			error_dialog_button_pressed = eel_run_simple_dialog
				(parent_for_error_dialog (transfer_info), TRUE, 
				 GTK_MESSAGE_ERROR, text, 
				 detail, GTK_STOCK_CANCEL, RETRY, NULL);

			switch (error_dialog_button_pressed) {
			case 0:
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_ABORT;
				break;
			case 1:
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_RETRY;
				break;
			default:
				g_assert_not_reached ();
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_ABORT;
			}
		} else {
			error_dialog_button_pressed = eel_run_simple_dialog
				(parent_for_error_dialog (transfer_info), TRUE, 
				 GTK_MESSAGE_ERROR, text, 
				 detail, SKIP, GTK_STOCK_CANCEL, RETRY, NULL);

			switch (error_dialog_button_pressed) {
			case 0:
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_SKIP;
				break;
			case 1:
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_ABORT;
				break;
			case 2:
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_RETRY;
				break;
			default:
				g_assert_not_reached ();
				error_dialog_result = GNOME_VFS_XFER_ERROR_ACTION_ABORT;
			}
		}

		g_free (text);
		g_free (detail);
		g_free (formatted_source_name);
		g_free (formatted_target_name);

		nautilus_file_operations_progress_resume_timeout (transfer_info->progress_dialog);

		return error_dialog_result;

	case GNOME_VFS_XFER_ERROR_MODE_ABORT:
	default:
		if (transfer_info->progress_dialog != NULL) {
			nautilus_file_operations_progress_done
				(transfer_info->progress_dialog);
		}
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	}
}

/* is_special_link
 *
 * Check and see if file is one of our special links.
 * A special link ould be one of the following:
 * 	trash, home, volume
 */
static gboolean
is_special_link (const char *uri)
{

	return eel_uri_is_desktop (uri);
}

static gboolean
is_directory (const char *uri)
{
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	gboolean is_dir;

	is_dir = FALSE;
	
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);

	if (result == GNOME_VFS_OK &&
	    info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) {
		is_dir = (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);
	}
	
	gnome_vfs_file_info_unref (info);

	return is_dir;
}


static int
handle_transfer_overwrite (const GnomeVFSXferProgressInfo *progress_info,
		           TransferInfo *transfer_info)
{
	int result;
	char *text, *primary_text, *secondary_text, *formatted_name, *base_name;
	GnomeVFSURI *file_uri, *parent_uri;
	gboolean is_merge, target_is_dir;

	nautilus_file_operations_progress_pause_timeout (transfer_info->progress_dialog);	

	/* Handle special case files such as Trash, mount links and home directory */	
	if (is_special_link (progress_info->target_name)) {
		formatted_name = extract_and_ellipsize_file_name_for_dialog
			(parent_for_error_dialog (transfer_info),
			 progress_info->target_name);
		
		if (transfer_info->kind == TRANSFER_MOVE) {
			primary_text = g_strdup_printf (_("Could not move \"%s\" to the new location."),
			                                formatted_name);
						
			secondary_text = _("The name is already used for a special item that "
					   "cannot be removed or replaced.  If you still want "
					   "to move the item, rename it and try again.");
		} else {						
			primary_text = g_strdup_printf (_("Could not copy \"%s\" to the new location."),
			                                formatted_name);
						
			secondary_text = _("The name is already used for a special item that "
					   "cannot be removed or replaced.  If you still want "
					   "to copy the item, rename it and try again.");
		}
		
		eel_run_simple_dialog (parent_for_error_dialog (transfer_info),
				       TRUE, GTK_MESSAGE_ERROR,
				       primary_text, secondary_text,
				       GTK_STOCK_OK, NULL);

		g_free (primary_text);
		g_free (formatted_name);

		nautilus_file_operations_progress_resume_timeout (transfer_info->progress_dialog);

		return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
	}
	
	/* transfer conflict, prompt the user to replace or skip */
	file_uri = gnome_vfs_uri_new (progress_info->target_name);
	base_name = gnome_vfs_uri_extract_short_path_name (file_uri);
	formatted_name = gnome_vfs_unescape_string_for_display (base_name);

	target_is_dir = is_directory (progress_info->target_name);
	if (target_is_dir) {
		text = g_strdup_printf (_("A folder named \"%s\" already exists.  Do you want to replace it?"), 
					formatted_name);
	} else {
		text = g_strdup_printf (_("A file named \"%s\" already exists.  Do you want to replace it?"), 
					formatted_name);
	}
	g_free (base_name);
	g_free (formatted_name);

	if (gnome_vfs_uri_has_parent (file_uri)) {
		parent_uri = gnome_vfs_uri_get_parent (file_uri);
		base_name = gnome_vfs_uri_extract_short_path_name (parent_uri);
		gnome_vfs_uri_unref (parent_uri);	
	} else {
		base_name = gnome_vfs_uri_extract_dirname (file_uri);
	}
	
	formatted_name = gnome_vfs_unescape_string_for_display (base_name);
	
	is_merge =  target_is_dir && is_directory (progress_info->source_name);

	if (is_merge) {
		secondary_text = g_strdup_printf (_("The folder already exists in \"%s\".  Replacing it will overwrite any files in the folder that conflict with the files being copied."), 
		                                  formatted_name);
	} else {
		secondary_text = g_strdup_printf (_("The file already exists in \"%s\".  Replacing it will overwrite its contents."), 
		                                  formatted_name);
	}
	gnome_vfs_uri_unref (file_uri);
	g_free (formatted_name);
	g_free (base_name);
	
	if (progress_info->duplicate_count == 1) {
		/* we are going to only get one duplicate alert, don't offer
		 * Replace All
		 */
		result = eel_run_simple_dialog 
			(parent_for_error_dialog (transfer_info),
			 TRUE,
			 GTK_MESSAGE_WARNING, 
			 text, 
			 secondary_text, 
			 SKIP, REPLACE, NULL);
		g_free (text);	 
		g_free (secondary_text);

		nautilus_file_operations_progress_resume_timeout (transfer_info->progress_dialog);
					 
		switch (result) {
		case 0:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
		default:
			g_assert_not_reached ();
			/* fall through */
		case 1:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
		}
	} else {
		result = eel_run_simple_dialog
			(parent_for_error_dialog (transfer_info), TRUE, GTK_MESSAGE_WARNING, text, 
			 secondary_text, 
			 SKIP_ALL, REPLACE_ALL, SKIP, REPLACE, NULL);
		g_free (text);
		g_free (secondary_text);

		nautilus_file_operations_progress_resume_timeout (transfer_info->progress_dialog);

		switch (result) {
		case 0:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP_ALL;
		case 1:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE_ALL;
		case 2:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP;
		default:
			g_assert_not_reached ();
			/* fall through */
		case 3:
			return GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE;
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
	char *unescaped_tmp_name;
	char *unescaped_result;
	char *new_file;

	const char *format;
	
	g_assert (name != NULL);

	unescaped_tmp_name = gnome_vfs_unescape_string (name, "/");
	g_free (name);

	unescaped_name = g_filename_to_utf8 (unescaped_tmp_name, -1,
					     NULL, NULL, NULL);

	if (!unescaped_name) {
		/* Couldn't convert to utf8 - probably
		 * G_BROKEN_FILENAMES not set when it should be.
		 * Try converting from the locale */
		unescaped_name = g_locale_to_utf8 (unescaped_tmp_name, -1, NULL, NULL, NULL);	

		if (!unescaped_name) {
			unescaped_name = eel_make_valid_utf8 (unescaped_tmp_name);
		}
	}

	g_free (unescaped_tmp_name);

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
			format = _("Link to %s");
			break;
		case 2:
			/* appended to new link file */
			format = _("Another link to %s");
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
	new_file = g_filename_from_utf8 (unescaped_result, -1, NULL, NULL, NULL);
	result = gnome_vfs_escape_path_string (new_file);
	
	g_free (unescaped_name);
	g_free (unescaped_result);
	g_free (new_file);

	return result;
}

/* Localizers: 
 * Feel free to leave out the st, nd, rd and th suffix or
 * make some or all of them match.
 */

/* localizers: tag used to detect the first copy of a file */
static const char untranslated_copy_duplicate_tag[] = N_(" (copy)");
/* localizers: tag used to detect the second copy of a file */
static const char untranslated_another_copy_duplicate_tag[] = N_(" (another copy)");

/* localizers: tag used to detect the x11th copy of a file */
static const char untranslated_x11th_copy_duplicate_tag[] = N_("th copy)");
/* localizers: tag used to detect the x12th copy of a file */
static const char untranslated_x12th_copy_duplicate_tag[] = N_("th copy)");
/* localizers: tag used to detect the x13th copy of a file */
static const char untranslated_x13th_copy_duplicate_tag[] = N_("th copy)");

/* localizers: tag used to detect the x1st copy of a file */
static const char untranslated_st_copy_duplicate_tag[] = N_("st copy)");
/* localizers: tag used to detect the x2nd copy of a file */
static const char untranslated_nd_copy_duplicate_tag[] = N_("nd copy)");
/* localizers: tag used to detect the x3rd copy of a file */
static const char untranslated_rd_copy_duplicate_tag[] = N_("rd copy)");

/* localizers: tag used to detect the xxth copy of a file */
static const char untranslated_th_copy_duplicate_tag[] = N_("th copy)");

#define COPY_DUPLICATE_TAG _(untranslated_copy_duplicate_tag)
#define ANOTHER_COPY_DUPLICATE_TAG _(untranslated_another_copy_duplicate_tag)
#define X11TH_COPY_DUPLICATE_TAG _(untranslated_x11th_copy_duplicate_tag)
#define X12TH_COPY_DUPLICATE_TAG _(untranslated_x12th_copy_duplicate_tag)
#define X13TH_COPY_DUPLICATE_TAG _(untranslated_x13th_copy_duplicate_tag)

#define ST_COPY_DUPLICATE_TAG _(untranslated_st_copy_duplicate_tag)
#define ND_COPY_DUPLICATE_TAG _(untranslated_nd_copy_duplicate_tag)
#define RD_COPY_DUPLICATE_TAG _(untranslated_rd_copy_duplicate_tag)
#define TH_COPY_DUPLICATE_TAG _(untranslated_th_copy_duplicate_tag)

/* localizers: appended to first file copy */
static const char untranslated_first_copy_duplicate_format[] = N_("%s (copy)%s");
/* localizers: appended to second file copy */
static const char untranslated_second_copy_duplicate_format[] = N_("%s (another copy)%s");

/* localizers: appended to x11th file copy */
static const char untranslated_x11th_copy_duplicate_format[] = N_("%s (%dth copy)%s");
/* localizers: appended to x12th file copy */
static const char untranslated_x12th_copy_duplicate_format[] = N_("%s (%dth copy)%s");
/* localizers: appended to x13th file copy */
static const char untranslated_x13th_copy_duplicate_format[] = N_("%s (%dth copy)%s");

/* localizers: appended to x1st file copy */
static const char untranslated_st_copy_duplicate_format[] = N_("%s (%dst copy)%s");
/* localizers: appended to x2nd file copy */
static const char untranslated_nd_copy_duplicate_format[] = N_("%s (%dnd copy)%s");
/* localizers: appended to x3rd file copy */
static const char untranslated_rd_copy_duplicate_format[] = N_("%s (%drd copy)%s");
/* localizers: appended to xxth file copy */
static const char untranslated_th_copy_duplicate_format[] = N_("%s (%dth copy)%s");

#define FIRST_COPY_DUPLICATE_FORMAT _(untranslated_first_copy_duplicate_format)
#define SECOND_COPY_DUPLICATE_FORMAT _(untranslated_second_copy_duplicate_format)
#define X11TH_COPY_DUPLICATE_FORMAT _(untranslated_x11th_copy_duplicate_format)
#define X12TH_COPY_DUPLICATE_FORMAT _(untranslated_x12th_copy_duplicate_format)
#define X13TH_COPY_DUPLICATE_FORMAT _(untranslated_x13th_copy_duplicate_format)

#define ST_COPY_DUPLICATE_FORMAT _(untranslated_st_copy_duplicate_format)
#define ND_COPY_DUPLICATE_FORMAT _(untranslated_nd_copy_duplicate_format)
#define RD_COPY_DUPLICATE_FORMAT _(untranslated_rd_copy_duplicate_format)
#define TH_COPY_DUPLICATE_FORMAT _(untranslated_th_copy_duplicate_format)

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
parse_previous_duplicate_name (const char *name,
			       char **name_base,
			       const char **suffix,
			       int *count)
{
	const char *tag;

	g_assert (name[0] != '\0');
	
	*suffix = strchr (name + 1, '.');
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
	tag = strstr (name, X11TH_COPY_DUPLICATE_TAG);

	if (tag == NULL) {
		tag = strstr (name, X12TH_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, X13TH_COPY_DUPLICATE_TAG);
	}

	if (tag == NULL) {
		tag = strstr (name, ST_COPY_DUPLICATE_TAG);
	}
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
		*name_base = g_strdup (name);
	}
}

static char *
make_next_duplicate_name (const char *base, const char *suffix, int count)
{
	const char *format;
	char *result;


	if (count < 1) {
		g_warning ("bad count %d in get_duplicate_name", count);
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

		/* Handle special cases for x11th - x20th.
		 */
		switch (count % 100) {
		case 11:
			format = X11TH_COPY_DUPLICATE_FORMAT;
			break;
		case 12:
			format = X12TH_COPY_DUPLICATE_FORMAT;
			break;
		case 13:
			format = X13TH_COPY_DUPLICATE_FORMAT;
			break;
		default:
			format = NULL;
			break;
		}

		if (format == NULL) {
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
	char *unescaped_tmp_name;
	char *unescaped_result;
	char *result;
	char *new_file;

	unescaped_tmp_name = gnome_vfs_unescape_string (name, "/");
	g_free (name);

	unescaped_name = g_filename_to_utf8 (unescaped_tmp_name, -1,
					     NULL, NULL, NULL);
	if (!unescaped_name) {
		/* Couldn't convert to utf8 - probably
		 * G_BROKEN_FILENAMES not set when it should be.
		 * Try converting from the locale */
		unescaped_name = g_locale_to_utf8 (unescaped_tmp_name, -1, NULL, NULL, NULL);	

		if (!unescaped_name) {
			unescaped_name = eel_make_valid_utf8 (unescaped_tmp_name);
		}
	}
		
	g_free (unescaped_tmp_name);
	
	unescaped_result = get_duplicate_name (unescaped_name, count_increment);
	g_free (unescaped_name);

	new_file = g_filename_from_utf8 (unescaped_result, -1, NULL, NULL, NULL);
	result = gnome_vfs_escape_path_string (new_file);
	g_free (unescaped_result);
	g_free (new_file);
	return result;
}

static int
handle_transfer_duplicate (GnomeVFSXferProgressInfo *progress_info,
			   TransferInfo *transfer_info)
{
	switch (transfer_info->kind) {
	case TRANSFER_LINK:
		progress_info->duplicate_name = get_link_name
			(progress_info->duplicate_name,
			 progress_info->duplicate_count);
		break;

	case TRANSFER_COPY:
	case TRANSFER_MOVE_TO_TRASH:
		progress_info->duplicate_name = get_next_duplicate_name
			(progress_info->duplicate_name,
			 progress_info->duplicate_count);
		break;
	default:
		break;
		/* For all other cases we use the name as-is. */
	}

	return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
}

static int
update_transfer_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSXferProgressInfo *progress_info,
	       gpointer data)
{
	TransferInfo *transfer_info;

	transfer_info = (TransferInfo *) data;

	switch (progress_info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		return handle_transfer_ok (progress_info, transfer_info);
	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
		return handle_transfer_vfs_error (progress_info, transfer_info);
	case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
		return handle_transfer_overwrite (progress_info, transfer_info);
	case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
		return handle_transfer_duplicate (progress_info, transfer_info);
	default:
		g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
			   progress_info->status);
		return 0;
	}
}

static void
apply_one_position (IconPositionIterator *position_iterator, 
		    const char *source_name,
		    const char *target_name)
{
	GdkPoint point;

	if (icon_position_iterator_get_next (position_iterator, source_name, target_name, &point)) {
		nautilus_file_changes_queue_schedule_position_set (target_name, point, position_iterator->screen);
	} else {
		nautilus_file_changes_queue_schedule_position_remove (target_name);
	}
}

typedef struct {
	GHashTable		*debuting_uris;
	IconPositionIterator	*iterator;
} SyncTransferInfo;

/* Low-level callback, called for every copy engine operation.
 * Generates notifications about new, deleted and moved files.
 */
static int
sync_transfer_callback (GnomeVFSXferProgressInfo *progress_info, gpointer data)
{
	GHashTable	     *debuting_uris;
	IconPositionIterator *position_iterator;
	gboolean              really_moved;

	if (data != NULL) {
		debuting_uris	  = ((SyncTransferInfo *) data)->debuting_uris;
		position_iterator = ((SyncTransferInfo *) data)->iterator;
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
				if (progress_info->source_name == NULL) {
					/* remove any old metadata */
					nautilus_file_changes_queue_schedule_metadata_remove_by_uri 
						(progress_info->target_name);
				} else {
					nautilus_file_changes_queue_schedule_metadata_copy 
						(progress_info->source_name, progress_info->target_name);

				}

				apply_one_position (position_iterator,
						    progress_info->source_name,
						    progress_info->target_name);

				if (debuting_uris != NULL) {
					g_hash_table_replace (debuting_uris,
							      g_strdup (progress_info->target_name),
							      GINT_TO_POINTER (TRUE));
				}
			}
			nautilus_file_changes_queue_file_added (progress_info->target_name);
			break;

		case GNOME_VFS_XFER_PHASE_MOVING:
			g_assert (progress_info->source_name != NULL);

			/* If the source and target are the same, that
			 * means we "moved" something in place. No
			 * actual change happened, so we really don't
			 * want to send out any change notification,
			 * but we do want to select the files as
			 * "newly moved here" so we put them into the
			 * debuting_uris set.
			 */
			really_moved = strcmp (progress_info->source_name,
					       progress_info->target_name) != 0;

			if (progress_info->top_level_item) {
				if (really_moved) {
					nautilus_file_changes_queue_schedule_metadata_move 
						(progress_info->source_name, progress_info->target_name);
				}

				apply_one_position (position_iterator,
						    progress_info->source_name,
						    progress_info->target_name);

				if (debuting_uris != NULL) {
					g_hash_table_replace (debuting_uris,
							      g_strdup (progress_info->target_name),
							      GINT_TO_POINTER (really_moved));
				}
			}
			if (really_moved) {
				nautilus_file_changes_queue_file_moved (progress_info->source_name,
									progress_info->target_name);
			}
			break;
			
		case GNOME_VFS_XFER_PHASE_DELETESOURCE:
			g_assert (progress_info->source_name != NULL);
			if (progress_info->top_level_item) {
				nautilus_file_changes_queue_schedule_metadata_remove_by_uri 
					(progress_info->source_name);
			}
			nautilus_file_changes_queue_file_removed_by_uri (progress_info->source_name);
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
append_basename (const GnomeVFSURI *target_directory,
		 const GnomeVFSURI *source_directory)
{
	char *file_name;
	GnomeVFSURI *ret;

	file_name = gnome_vfs_uri_extract_short_name (source_directory);
	if (file_name != NULL) {
		ret = gnome_vfs_uri_append_file_name (target_directory, 
						      file_name);
		g_free (file_name);
		return ret;
	}
	 
	return gnome_vfs_uri_dup (target_directory);
}

void
nautilus_file_operations_copy_move (const GList *item_uris,
				    GArray *relative_item_points,
				    const char *target_dir,
				    GdkDragAction copy_action,
				    GtkWidget *parent_view,
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

	TransferInfo *transfer_info;
	SyncTransferInfo *sync_transfer_info;
	GnomeVFSResult result;
	gboolean target_is_trash;
	gboolean duplicate;
	gboolean target_is_mapping;
	gboolean have_nonmapping_source;
	gboolean have_nonlocal_source;
	gboolean have_readonly_source;
	
	IconPositionIterator *icon_position_iterator;

	GdkScreen *screen;
	int screen_num;

	g_assert (item_uris != NULL);

	{
		const char *action_str;

		switch (copy_action) {
		case GDK_ACTION_COPY:
			action_str = "copy";
			break;

		case GDK_ACTION_MOVE:
			action_str = "move";
			break;

		case GDK_ACTION_LINK:
			action_str = "link";
			break;

		default:
			action_str = "[unknown action]";
			break;
		}

		nautilus_debug_log_with_uri_list (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER, item_uris,
						  "%s the following URIs to \"%s\":",
						  action_str,
						  target_dir ? target_dir : "[empty location]");
	}

	target_dir_uri = NULL;
	trash_dir_uri = NULL;
	result = GNOME_VFS_OK;

	target_is_trash = FALSE;
	target_is_mapping = FALSE;
	if (target_dir != NULL) {
		if (eel_uri_is_trash (target_dir)) {
			target_is_trash = TRUE;
		} else if (eel_uri_is_desktop (target_dir)) {
			char *desktop_dir_uri;

			desktop_dir_uri = nautilus_get_desktop_directory_uri ();
			target_dir_uri = gnome_vfs_uri_new (desktop_dir_uri);
			g_free (desktop_dir_uri);
		} else {
			target_dir_uri = gnome_vfs_uri_new (target_dir);
		}
		if (strncmp (target_dir, "burn:", 5) == 0) {
			target_is_mapping = TRUE;
		}
			
	}

	/* Build the source and target URI lists and figure out if all
	 * the files are on the same disk.
	 */
	source_uri_list = NULL;
	target_uri_list = NULL;
	have_nonlocal_source = FALSE;
	have_nonmapping_source = FALSE;
	have_readonly_source = FALSE;
	duplicate = copy_action != GDK_ACTION_MOVE;
	for (p = item_uris; p != NULL; p = p->next) {
		/* Filter out special Nautilus link files */
		/* FIXME bugzilla.gnome.org 45295: 
		 * This is surprising behavior -- the user drags the Trash icon (say)
		 * to a folder, releases it, and nothing whatsoever happens. Don't we want
		 * a dialog in this case?
		 */
		if (is_special_link ((const char *) p->data)) {
			continue;
		}

		source_uri = gnome_vfs_uri_new ((const char *) p->data);
		if (source_uri == NULL) {
			continue;
		}
		
		if (strcmp (source_uri->method_string, "file") != 0) {
			have_nonlocal_source = TRUE;
		}

		if (strcmp (source_uri->method_string, "burn") != 0) {
			have_nonmapping_source = TRUE;
		}

		if (!have_readonly_source) {
			GnomeVFSVolume *volume;
			char *text_uri, *path;

			text_uri = gnome_vfs_uri_to_string (source_uri, GNOME_VFS_URI_HIDE_NONE);

			path = g_filename_from_uri (text_uri, NULL, NULL);

			volume = NULL;
			if (path != NULL) {
				/* TODO should we resolve symlinks ourselves, or should
				 * gnome_vfs_volume_monitor_get_volume_for_path use lstat? */
				volume = gnome_vfs_volume_monitor_get_volume_for_path (gnome_vfs_get_volume_monitor (), path);
			}

			if (volume != NULL && gnome_vfs_volume_is_read_only (volume)) {
				have_readonly_source = TRUE;
			}

			gnome_vfs_volume_unref (volume);
			g_free (path);
			g_free (text_uri);
		}

		/* Note: this could be null if we're e.g. copying the top level file of a web site */
		source_dir_uri = gnome_vfs_uri_get_parent (source_uri);
		target_uri = NULL;
		if (target_dir != NULL) {
			if (target_is_trash) {
				result = gnome_vfs_find_directory (source_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
								   &target_dir_uri, FALSE, FALSE, 0777);
				if (result == GNOME_VFS_ERROR_NOT_FOUND && source_dir_uri != NULL) {
					/* source_uri may be a broken symlink */
					result = gnome_vfs_find_directory (source_dir_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
									   &target_dir_uri, FALSE, FALSE, 0777);
				}

				result = GNOME_VFS_OK;
			}
			if (target_dir_uri != NULL) {
				target_uri = append_basename (target_dir_uri, source_uri);
			}
		} else {
			/* duplication */
			target_uri = gnome_vfs_uri_ref (source_uri);
			if (target_dir_uri != NULL) {
				gnome_vfs_uri_unref (target_dir_uri);
			}
			target_dir_uri = gnome_vfs_uri_ref (source_dir_uri);
		}
		
		if (target_uri != NULL) {
			g_assert (target_dir_uri != NULL);

			target_uri_list = g_list_prepend (target_uri_list, target_uri);
			source_uri_list = g_list_prepend (source_uri_list, source_uri);

			if (duplicate && source_dir_uri != NULL &&
			    !gnome_vfs_uri_equal (source_dir_uri, target_dir_uri)) {
				duplicate = FALSE;
			}
		}
		if (source_dir_uri != NULL) {
			gnome_vfs_uri_unref (source_dir_uri);
		}
	}

	if (target_is_trash) {
		/* Make sure new trash directories that we don't show yet get integrated. */
		nautilus_trash_monitor_add_new_trash_directories ();
	}

	move_options = GNOME_VFS_XFER_RECURSIVE;
	if (duplicate) {
		/* Copy operation, parents match -> duplicate
		 * operation. Ask gnome-vfs to generate unique names
		 * for target files.
		 */
		move_options |= GNOME_VFS_XFER_USE_UNIQUE_NAMES;
	}

	if (have_readonly_source) {
		move_options |= GNOME_VFS_XFER_TARGET_DEFAULT_PERMS;
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

	if (target_is_mapping && have_nonmapping_source && !have_nonlocal_source && (copy_action == GDK_ACTION_COPY || copy_action == GDK_ACTION_MOVE)) {
		copy_action = GDK_ACTION_LINK;
	}
	if (copy_action == GDK_ACTION_MOVE && (!target_is_mapping || !have_nonmapping_source)) {
		move_options |= GNOME_VFS_XFER_REMOVESOURCE;
	} else if (copy_action == GDK_ACTION_LINK) {
		move_options |= GNOME_VFS_XFER_LINK_ITEMS;
	}
	
	/* set up the copy/move parameters */
	transfer_info = transfer_info_new (parent_view);
	if (relative_item_points != NULL && relative_item_points->len > 0) {
		screen = gtk_widget_get_screen (GTK_WIDGET (parent_view));
		screen_num = gdk_screen_get_number (screen);
		/* FIXME: we probably don't need an icon_position_iterator
		 * here at all.
		 */
		icon_position_iterator = icon_position_iterator_new
			(relative_item_points, item_uris, screen_num, TRUE);
	} else {
		icon_position_iterator = NULL;
	}
	
	if (target_is_trash && (move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) {
		/* when moving to trash, handle name conflicts automatically */
		move_options |= GNOME_VFS_XFER_USE_UNIQUE_NAMES;
		/* localizers: progress dialog title */
		transfer_info->operation_title = _("Moving files to the Trash");
		/* localizers: label prepended to the progress count */
		transfer_info->action_label =_("Throwing out file:");
		/* localizers: label prepended to the name of the current file moved */
		transfer_info->progress_verb =_("Moving");
		transfer_info->preparation_name =_("Preparing to Move to Trash...");

		transfer_info->kind = TRANSFER_MOVE_TO_TRASH;

	} else if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) {
		/* localizers: progress dialog title */
		transfer_info->operation_title = _("Moving files");
		/* localizers: label prepended to the progress count */
		transfer_info->action_label =_("Moving file:");
		/* localizers: label prepended to the name of the current file moved */
		transfer_info->progress_verb =_("Moving");
		transfer_info->preparation_name =_("Preparing To Move...");
		transfer_info->cleanup_name = _("Finishing Move...");

		transfer_info->kind = TRANSFER_MOVE;

	} else if ((move_options & GNOME_VFS_XFER_LINK_ITEMS) != 0) {
		/* when creating links, handle name conflicts automatically */
		move_options |= GNOME_VFS_XFER_USE_UNIQUE_NAMES;
		/* localizers: progress dialog title */
		transfer_info->operation_title = _("Creating links to files");
		/* localizers: label prepended to the progress count */
		transfer_info->action_label =_("Linking file:");
		/* localizers: label prepended to the name of the current file linked */
		transfer_info->progress_verb =_("Linking");
		transfer_info->preparation_name = _("Preparing to Create Links...");
		transfer_info->cleanup_name = _("Finishing Creating Links...");

		transfer_info->kind = TRANSFER_LINK;

	} else {
		/* localizers: progress dialog title */
		transfer_info->operation_title = _("Copying files");
		/* localizers: label prepended to the progress count */
		transfer_info->action_label =_("Copying file:");
		/* localizers: label prepended to the name of the current file copied */
		transfer_info->progress_verb =_("Copying");
		transfer_info->preparation_name =_("Preparing To Copy...");
		transfer_info->cleanup_name = "";

		transfer_info->kind = TRANSFER_COPY;
	}

	/* we'll need to check for copy into Trash and for moving/copying the Trash itself */
	gnome_vfs_find_directory (target_dir_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
				  &trash_dir_uri, FALSE, FALSE, 0777);

	if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) == 0) {
		/* don't allow copying into Trash */
		if (check_target_directory_is_or_in_trash (trash_dir_uri, target_dir_uri)) {
			eel_run_simple_dialog
				(parent_view, 
				 FALSE,
				 GTK_MESSAGE_ERROR,
				 ((move_options & GNOME_VFS_XFER_LINK_ITEMS) == 0)
				 ? _("You cannot copy items into the trash.")
				 : _("You cannot create links inside the trash."),
				 _("Files and folders can only be moved into the trash."), 
				 GTK_STOCK_OK, NULL);
			result = GNOME_VFS_ERROR_NOT_PERMITTED;
		}
	}

	if (result == GNOME_VFS_OK) {
		for (p = source_uri_list; p != NULL; p = p->next) {
			uri = (GnomeVFSURI *)p->data;

			/* Check that a trash folder is not being moved/copied (link is OK). */
			if (trash_dir_uri != NULL 
			    && ((move_options & GNOME_VFS_XFER_LINK_ITEMS) == 0) 
			    && gnome_vfs_uri_equal (uri, trash_dir_uri)) {
			    	/* Distinguish Trash file on desktop from other trash folders for
			    	 * message purposes.
			    	 */

				eel_run_simple_dialog
					(parent_view,
					 FALSE,
					 GTK_MESSAGE_ERROR,
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0)
						 ? _("You cannot move this trash folder.")
						 : _("You cannot copy this trash folder."),
					 _("A trash folder is used for storing items moved to the trash."),
					 GTK_STOCK_OK, NULL, NULL);

				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}
			
			/* FIXME:
			 * We should not have the case where a folder containing trash is moved into
			 * the trash give a generic "cannot move into itself" message, rather,
			 * we should have a trash specific message here.
			 */

			/* Don't allow recursive move/copy into itself. 
			 * (We would get a file system error if we proceeded but it is nicer to
			 * detect and report it at this level) */
			if ((move_options & GNOME_VFS_XFER_LINK_ITEMS) == 0
				&& (gnome_vfs_uri_equal (uri, target_dir_uri)
					|| gnome_vfs_uri_is_parent (uri, target_dir_uri, TRUE))) {
				eel_run_simple_dialog
					(parent_view, 
					 FALSE,
					 GTK_MESSAGE_ERROR,
					 ((move_options & GNOME_VFS_XFER_REMOVESOURCE) != 0) 
					 ? _("You cannot move a folder into itself.")
					 : _("You cannot copy a folder into itself."), 
					 _("The destination folder is inside the source folder."), 
					 GTK_STOCK_OK, NULL, NULL);

				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}
			if ((move_options & GNOME_VFS_XFER_REMOVESOURCE) == 0
			        && (move_options & GNOME_VFS_XFER_USE_UNIQUE_NAMES) == 0
				&& gnome_vfs_uri_is_parent (target_dir_uri, uri, FALSE)) {
				eel_run_simple_dialog
					(parent_view, 
					 FALSE,
					 GTK_MESSAGE_ERROR,
					 _("You cannot copy a file over itself."),
					 _("The destination and source are the same file."), 
					 GTK_STOCK_OK, NULL, NULL);			

				result = GNOME_VFS_ERROR_NOT_PERMITTED;
				break;
			}
		}
	}

	transfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
	transfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
	transfer_info->done_callback = done_callback;
	transfer_info->done_callback_data = done_callback_data;
	transfer_info->debuting_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	sync_transfer_info = g_new (SyncTransferInfo, 1);
	sync_transfer_info->iterator = icon_position_iterator;
	sync_transfer_info->debuting_uris = transfer_info->debuting_uris;

	transfer_info->iterator = sync_transfer_info->iterator;

	if (result == GNOME_VFS_OK) {
		gnome_vfs_async_xfer (&transfer_info->handle, source_uri_list, target_uri_list,
		      		      move_options, GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
				      GNOME_VFS_PRIORITY_DEFAULT,
		      		      update_transfer_callback, transfer_info,
		      		      sync_transfer_callback, sync_transfer_info);
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
	NautilusNewFolderCallback done_callback;
	gpointer data;
	GtkWidget *parent_view;
	IconPositionIterator *iterator;
} NewFolderTransferState;

static int
handle_new_folder_vfs_error (const GnomeVFSXferProgressInfo *progress_info, NewFolderTransferState *state)
{
	const char *error_string;
	char *error_string_to_free;

	error_string_to_free = NULL;

	if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
		error_string = _("You do not have permissions to write to the destination.");
	} else if (progress_info->vfs_status == GNOME_VFS_ERROR_NO_SPACE) {
		error_string = _("There is no space on the destination.");
	} else {
		error_string = g_strdup_printf (_("Error \"%s\" creating new folder."), 
						gnome_vfs_result_to_string (progress_info->vfs_status));
		error_string_to_free = (char *)error_string;
	}
	
	eel_show_error_dialog (_("Error creating new folder."), error_string,
				    GTK_WINDOW (gtk_widget_get_toplevel (state->parent_view)));
	
	g_free (error_string_to_free);
	
	return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
}

static int
new_folder_transfer_callback (GnomeVFSAsyncHandle *handle,
			      GnomeVFSXferProgressInfo *progress_info,
			      gpointer data)
{
	NewFolderTransferState *state;
	char *temp_string;
	char *new_uri;
	
	state = (NewFolderTransferState *) data;

	switch (progress_info->phase) {

	case GNOME_VFS_XFER_PHASE_COMPLETED:
		eel_remove_weak_pointer (&state->parent_view);
		g_free (state);
		return 0;

	default:
		switch (progress_info->status) {
		case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
			nautilus_file_changes_consume_changes (TRUE);
			new_uri = NULL;
			if (progress_info->vfs_status == GNOME_VFS_OK) {
				new_uri = progress_info->target_name;
			}
			(* state->done_callback) (new_uri,
						  state->data);
			return 1;
	
		case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
	
			temp_string = progress_info->duplicate_name;
	
			if (progress_info->vfs_status == GNOME_VFS_ERROR_NAME_TOO_LONG) {
				/* special case an 8.3 file system */
				progress_info->duplicate_name = g_strndup (temp_string, 8);
				progress_info->duplicate_name[8] = '\0';
				g_free (temp_string);
				temp_string = progress_info->duplicate_name;
				progress_info->duplicate_name = g_strdup_printf
					("%s.%d", 
					 progress_info->duplicate_name,
					 progress_info->duplicate_count);
			} else {
				progress_info->duplicate_name = g_strdup_printf
					("%s%%20%d", 
					 progress_info->duplicate_name,
					 progress_info->duplicate_count);
			}
			g_free (temp_string);

			icon_position_iterator_update_uri
				(state->iterator,
				 progress_info->target_name,
				 progress_info->duplicate_name);

			return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
	
		case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
			return handle_new_folder_vfs_error (progress_info, state);
		
	

		default:
			g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
				   progress_info->status);
			return 0;
		}
	}
}

void 
nautilus_file_operations_new_folder (GtkWidget *parent_view, 
				     GdkPoint *target_point,
				     const char *parent_dir,
				     NautilusNewFolderCallback done_callback,
				     gpointer data)
{
	GList *target_uri_list;
	GnomeVFSURI *uri, *parent_uri;
	char *text_uri, *dirname;
	NewFolderTransferState *state;
	SyncTransferInfo *sync_transfer_info;

	g_assert (parent_dir != NULL);

	nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "create an untitled folder in \"%s\"", parent_dir);

	/* pass in the target directory and the new folder name as a destination URI */
	if (eel_uri_is_desktop (parent_dir)) {
		char *desktop_dir_uri;

		desktop_dir_uri = nautilus_get_desktop_directory_uri ();
		parent_uri = gnome_vfs_uri_new (desktop_dir_uri);
		g_free (desktop_dir_uri);
	} else {
		parent_uri = gnome_vfs_uri_new (parent_dir);
	}

	/* localizers: the initial name of a new folder  */
	dirname = g_filename_from_utf8 (_("untitled folder"), -1, NULL, NULL, NULL);
	uri = gnome_vfs_uri_append_file_name (parent_uri, dirname);
	g_free (dirname);
	target_uri_list = g_list_prepend (NULL, uri);

	text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);

	sync_transfer_info = g_new (SyncTransferInfo, 1);
	sync_transfer_info->iterator = icon_position_iterator_new_single
		(target_point, text_uri,
		 gdk_screen_get_number (gtk_widget_get_screen (parent_view)),
		 FALSE);
	sync_transfer_info->debuting_uris = NULL;

	g_free (text_uri);

	state = g_new (NewFolderTransferState, 1);
	state->done_callback = done_callback;
	state->data = data;
	state->parent_view = parent_view;
	state->iterator = sync_transfer_info->iterator;
	eel_add_weak_pointer (&state->parent_view);

	gnome_vfs_async_xfer (&state->handle, NULL, target_uri_list,
	      		      GNOME_VFS_XFER_NEW_UNIQUE_DIRECTORY,
	      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
			      GNOME_VFS_PRIORITY_DEFAULT,
	      		      new_folder_transfer_callback, state,
	      		      sync_transfer_callback, sync_transfer_info);

	gnome_vfs_uri_list_free (target_uri_list);
	gnome_vfs_uri_unref (parent_uri);
}

typedef struct {
	GnomeVFSAsyncHandle *handle;
	NautilusNewFileCallback done_callback;
	gpointer data;
	GtkWidget *parent_view;
	GHashTable *debuting_uris;
	IconPositionIterator *iterator;
} NewFileTransferState;


static int
handle_new_file_vfs_error (const GnomeVFSXferProgressInfo *progress_info, NewFileTransferState *state)
{
	const char *error_string;
	char *error_string_to_free;

	error_string_to_free = NULL;

	if (progress_info->vfs_status == GNOME_VFS_ERROR_ACCESS_DENIED) {
		error_string = _("You do not have permissions to write to the destination.");
	} else if (progress_info->vfs_status == GNOME_VFS_ERROR_NO_SPACE) {
		error_string = _("There is no space on the destination.");
	} else {
		error_string = g_strdup_printf (_("Error \"%s\" creating new document."), 
						gnome_vfs_result_to_string (progress_info->vfs_status));
		error_string_to_free = (char *)error_string;
	}
	
	eel_show_error_dialog (_("Error creating new document."), error_string,
			       GTK_WINDOW (gtk_widget_get_toplevel (state->parent_view)));
	
	g_free (error_string_to_free);
	
	return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
}

static void
get_new_file_uri (gpointer       key,
		  gpointer       value,
		  gpointer       user_data)
{
	char *uri;
	char **uri_out;

	uri = key;
	uri_out = user_data;

	*uri_out = uri;
}


static int
new_file_transfer_callback (GnomeVFSAsyncHandle *handle,
			      GnomeVFSXferProgressInfo *progress_info,
			      gpointer data)
{
	NewFileTransferState *state;
	char *temp_string;
	char **temp_strings;
	char *uri;
	
	state = (NewFileTransferState *) data;

	switch (progress_info->phase) {

	case GNOME_VFS_XFER_PHASE_COMPLETED:
		uri = NULL;
		
		g_hash_table_foreach (state->debuting_uris,
				      get_new_file_uri, &uri);

		(* state->done_callback) (uri, state->data);
		/* uri is owned by hashtable, don't free */
		eel_remove_weak_pointer (&state->parent_view);
		g_hash_table_destroy (state->debuting_uris);
		g_free (state);
		return 0;

	default:
		switch (progress_info->status) {
		case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
			nautilus_file_changes_consume_changes (TRUE);
			return 1;
	
		case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
	
			temp_string = progress_info->duplicate_name;
	
			if (progress_info->vfs_status == GNOME_VFS_ERROR_NAME_TOO_LONG) {
				/* special case an 8.3 file system */
				progress_info->duplicate_name = g_strndup (temp_string, 8);
				progress_info->duplicate_name[8] = '\0';
				g_free (temp_string);
				temp_string = progress_info->duplicate_name;
				progress_info->duplicate_name = g_strdup_printf
					("%s.%d", 
					 progress_info->duplicate_name,
					 progress_info->duplicate_count);
			} else {
				temp_strings = g_strsplit (temp_string, ".", 2);
				if (temp_strings[1] != NULL) {
					progress_info->duplicate_name = g_strdup_printf
						("%s%%20%d.%s", 
						 temp_strings[0],
						 progress_info->duplicate_count,
						 temp_strings[1]);
				} else {
					progress_info->duplicate_name = g_strdup_printf
						("%s%%20%d", 
						 progress_info->duplicate_name,
						 progress_info->duplicate_count);
				}
				g_strfreev (temp_strings);
			}
			g_free (temp_string);

			return GNOME_VFS_XFER_ERROR_ACTION_SKIP;
	
		case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
			return handle_new_file_vfs_error (progress_info, state);
		
	

		default:
			g_warning (_("Unknown GnomeVFSXferProgressStatus %d"),
				   progress_info->status);
			return 0;
		}
	}
}

void 
nautilus_file_operations_new_file_from_template (GtkWidget *parent_view, 
						 GdkPoint *target_point,
						 const char *parent_dir,
						 const char *target_filename,
						 const char *template_uri,
						 NautilusNewFileCallback done_callback,
						 gpointer data)
{
	GList *target_uri_list;
	GList *source_uri_list;
	GnomeVFSURI *target_uri, *parent_uri, *source_uri;
	GnomeVFSXferOptions options;
	NewFileTransferState *state;
	SyncTransferInfo *sync_transfer_info;
	char *tmp;

	g_assert (parent_dir != NULL);
	g_assert (template_uri != NULL);

	nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "create new file \"%s\" from template \"%s\" in \"%s\"",
			    target_filename ? target_filename : "(none)", template_uri, parent_dir);

	/* pass in the target directory and the new folder name as a destination URI */
	if (eel_uri_is_desktop (parent_dir)) {
		tmp = nautilus_get_desktop_directory_uri ();
		parent_uri = gnome_vfs_uri_new (tmp);
		g_free (tmp);
	} else if (eel_uri_is_trash (parent_dir) ||
		   eel_uri_is_search (parent_dir)) {
		parent_uri = NULL;
	} else {
		parent_uri = gnome_vfs_uri_new (parent_dir);
	}

	if (parent_uri == NULL) {
		(*done_callback) (NULL, data);
		return;
	}

	source_uri = gnome_vfs_uri_new (template_uri);
	if (source_uri == NULL) {
		(*done_callback) (NULL, data);
		return;
	}

	if (target_filename != NULL) {
		target_uri = gnome_vfs_uri_append_file_name (parent_uri, target_filename);
	} else {
		tmp = gnome_vfs_uri_extract_short_name (source_uri);
		target_uri = gnome_vfs_uri_append_file_name (parent_uri, tmp);
		g_free (tmp);
	}

	sync_transfer_info = g_new (SyncTransferInfo, 1);
	sync_transfer_info->iterator = icon_position_iterator_new_single
		(target_point, template_uri,
		 gdk_screen_get_number (gtk_widget_get_screen (parent_view)),
		 TRUE);
	sync_transfer_info->debuting_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	state = g_new (NewFileTransferState, 1);
	state->done_callback = done_callback;
	state->data = data;
	state->parent_view = parent_view;
	state->iterator = sync_transfer_info->iterator;
	state->debuting_uris = sync_transfer_info->debuting_uris;
	eel_add_weak_pointer (&state->parent_view);

	target_uri_list = g_list_prepend (NULL, target_uri);
	source_uri_list = g_list_prepend (NULL, source_uri);

	options = GNOME_VFS_XFER_USE_UNIQUE_NAMES | GNOME_VFS_XFER_TARGET_DEFAULT_PERMS;

	gnome_vfs_async_xfer (&state->handle, source_uri_list, target_uri_list,
	      		      options,
	      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
	      		      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
			      GNOME_VFS_PRIORITY_DEFAULT,
			      new_file_transfer_callback, state,
	      		      sync_transfer_callback, sync_transfer_info);

	gnome_vfs_uri_list_free (target_uri_list);
	gnome_vfs_uri_list_free (source_uri_list);
	gnome_vfs_uri_unref (parent_uri);
}

struct NewFileData {
	char *tmp_file;
	NautilusNewFileCallback done_callback;
	gpointer callback_data;
};

static void
new_file_from_temp_callback (const char *new_file_uri,
			     gpointer    callback_data)
{
	struct NewFileData *data = callback_data;

	/* Remove the template file
	 * Gnome-vfs can do this, but it caused problem, see bug #309592
	 */
	g_remove (data->tmp_file);
	g_free (data->tmp_file);
	
	(data->done_callback) (new_file_uri, data->callback_data);

	g_free (data);
}


void 
nautilus_file_operations_new_file (GtkWidget *parent_view, 
				   GdkPoint *target_point,
				   const char *parent_dir,
				   const char *initial_contents,
				   NautilusNewFileCallback done_callback,
				   gpointer data)
{
	struct NewFileData *new_data;
	char source_file_str[] = "/tmp/nautilus-sourceXXXXXX";
	char *source_file_uri;
	FILE *source_file;
	char *target_filename;
	int fd;

	nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "create new file in \"%s\"",
			    parent_dir);

	fd = mkstemp (source_file_str);
	if (fd == -1) {
		(*done_callback) (NULL, data);
		return;
	}

	if (initial_contents != NULL) {
		source_file = fdopen (fd, "a+");

		fprintf (source_file, "%s", initial_contents);
		fclose (source_file);
	}

	close (fd);

	target_filename = g_filename_from_utf8 (_("new file"), -1, NULL, NULL, NULL);

	source_file_uri = g_filename_to_uri (source_file_str, NULL, NULL);

	new_data = g_new (struct NewFileData, 1);
	new_data->tmp_file = g_strdup (source_file_str);
	new_data->done_callback = done_callback;
	new_data->callback_data = data;
	
	nautilus_file_operations_new_file_from_template (parent_view, 
							 target_point,
							 parent_dir,
							 target_filename,
							 source_file_uri,
							 new_file_from_temp_callback,
							 new_data);

	g_free (source_file_uri);
	g_free (target_filename);
}

#endif /* GIO_CONVERSION_DONE */


typedef struct {
	GList *files;
	GtkWindow *parent_window;
	gboolean try_trash;
	gboolean delete_if_all_already_in_trash;
	NautilusDeleteCallback done_callback;
	gpointer done_callback_data;
} DeleteJob;

static gboolean
can_delete_without_confirm (GFile *file)
{
	if (g_file_has_uri_scheme (file, "burn")) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
can_delete_files_without_confirm (GList *files)
{
	g_assert (files != NULL);

	while (files != NULL) {
		if (!can_delete_without_confirm (files->data)) {
			return FALSE;
		}

		files = files->next;
	}

	return TRUE;
}

static gboolean
can_trash_file (GFile *file, GCancellable *cancellable)
{
	GFileInfo *info;
	gboolean res;

	res = FALSE;
	info = g_file_query_info (file, 
				  G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
				  0,
				  cancellable,
				  NULL);

	if (info) {
		res = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
		g_object_unref (info);
	}

	return res;
}

static char *
get_display_name (GFile *file, GCancellable *cancellable)
{
	GFileInfo *info;
	char *name, *basename;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STD_DISPLAY_NAME,
				  0, cancellable, NULL);

	name = NULL;
	if (info) {
		name = g_strdup (g_file_info_get_display_name (info));
		g_object_unref (info);
	}
	
	if (name == NULL) {
		basename = g_file_get_basename (file);
		if (g_utf8_validate (basename, -1, NULL)) {
			name = basename;
		} else {
			name = g_uri_escape_string (basename, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
			g_free (basename);
		}
	}
	
	return name;
}

static void
delete_files (GList *files, GCancellable  *cancellable, GtkWindow *parent_window)
{
	GList *l;
	GFile *file;
	GError *error;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;

		error = NULL;
		if (!g_file_delete (file, cancellable, &error)) {
			/* TODO-gio: Dialog here, and handle recursive deletes */
			g_print ("Error deleting file: %s\n", error->message);
		} else {
			nautilus_file_changes_queue_schedule_metadata_remove (file);
			nautilus_file_changes_queue_file_removed (file);
		}
	}
}

static void
trash_files (GList *files, GCancellable  *cancellable, GtkWindow *parent_window)
{
	GList *l;
	GFile *file;
	GError *error;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;

		error = NULL;
		if (!g_file_trash (file, cancellable, &error)) {
			/* TODO-gio: Dialog here, allow delete instead of trash, etc */
			g_print ("Error trashing file: %s\n", error->message);
		} else {
			nautilus_file_changes_queue_schedule_metadata_remove (file);
			nautilus_file_changes_queue_file_removed (file);
		}
	}
}

typedef struct {
	GtkWindow *parent_window;
	gboolean ignore_close_box;
	GtkMessageType message_type;
	const char *primary_text;
	const char *secondary_text;
	const char *details_text;
	const char **button_titles;
	
	int result;
} RunSimpleDialogData;

static void
do_run_simple_dialog (gpointer _data)
{
	RunSimpleDialogData *data = _data;
	const char *button_title;
        GtkWidget *dialog;
	int result;
	int response_id;

	/* Create the dialog. */
	dialog = eel_alert_dialog_new (data->parent_window, 
	                               0,
	                               data->message_type,
	                               GTK_BUTTONS_NONE,
	                               data->primary_text,
	                               data->secondary_text);

	for (response_id = 0;
	     data->button_titles[response_id] != NULL;
	     response_id++) {
		button_title = data->button_titles[response_id];
		gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, response_id);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), response_id);
	}

	if (data->details_text) {
		eel_alert_dialog_set_details_label (EEL_ALERT_DIALOG (dialog),
						    data->details_text);
	}
	
	/* Run it. */
        gtk_widget_show (dialog);
        result = gtk_dialog_run (GTK_DIALOG (dialog));
	
	while ((result == GTK_RESPONSE_NONE || result == GTK_RESPONSE_DELETE_EVENT) && data->ignore_close_box) {
		gtk_widget_show (GTK_WIDGET (dialog));
		result = gtk_dialog_run (GTK_DIALOG (dialog));
	}
	
	gtk_object_destroy (GTK_OBJECT (dialog));

	data->result = result;
}

static int
_run_simple_dialog (GIOJob *job,
		   GtkWindow *parent_window,
		   gboolean ignore_close_box,
		   GtkMessageType message_type,
		   const char *primary_text,
		   const char *secondary_text,
		   const char *details_text,
		   ...)
{
	RunSimpleDialogData *data;
	va_list varargs;
	int res;
	const char *button_title;
	GPtrArray *ptr_array;

	data = g_new0 (RunSimpleDialogData, 1);
	data->parent_window = parent_window;
	data->ignore_close_box = ignore_close_box;
	data->message_type = message_type;
	data->primary_text = primary_text;
	data->secondary_text = secondary_text;
	data->details_text = details_text;

	ptr_array = g_ptr_array_new ();
	va_start (varargs, details_text);
	while ((button_title = va_arg (varargs, const char *)) != NULL) {
		g_ptr_array_add (ptr_array, (char *)button_title);
	}
	g_ptr_array_add (ptr_array, NULL);
	data->button_titles = (const char **)g_ptr_array_free (ptr_array, FALSE);
	va_end (varargs);

	g_io_job_send_to_mainloop (job,
				   do_run_simple_dialog,
				   data,
				   NULL,
				   TRUE);

	res = data->result;

	g_free (data->button_titles);
	g_free (data);
	return res;
}

static int 
_run_alert (GIOJob *job,
	   GtkWindow *parent_window,
	   const char *primary_message,
	   const char *secondary_message,
	   const char *ok_label)
{
	return _run_simple_dialog (job, parent_window,
				  FALSE,
				  GTK_MESSAGE_WARNING,
				  primary_message,
				  secondary_message,
				  NULL,
				  GTK_STOCK_CANCEL, 
				  ok_label,
				  NULL);
}

static int
_run_yes_no_dialog (GIOJob *job,
		   const char *prompt,
		   const char *detail,
		   const char *yes_label,
		   const char *no_label,
		   GtkWindow *parent_window)
{
	return _run_simple_dialog (job, parent_window,
				  FALSE,
				  GTK_MESSAGE_QUESTION,
				  prompt,
				  detail,
				  NULL,
				  no_label,
				  yes_label,
				  NULL);
}

/* NOTE: This frees the primary / secondary strings, in order to
   avoid doing that everywhere. So, make sure they are strduped */

static int
run_simple_dialog_va (CommonJob *job,
		      gboolean ignore_close_box,
		      GtkMessageType message_type,
		      char *primary_text,
		      char *secondary_text,
		      const char *details_text,
		      va_list varargs)
{
	RunSimpleDialogData *data;
	int res;
	const char *button_title;
	GPtrArray *ptr_array;

	g_timer_stop (job->time);
	
	data = g_new0 (RunSimpleDialogData, 1);
	data->parent_window = GTK_WINDOW (job->parent_window);
	data->ignore_close_box = ignore_close_box;
	data->message_type = message_type;
	data->primary_text = primary_text;
	data->secondary_text = secondary_text;
	data->details_text = details_text;

	ptr_array = g_ptr_array_new ();
	while ((button_title = va_arg (varargs, const char *)) != NULL) {
		g_ptr_array_add (ptr_array, (char *)button_title);
	}
	g_ptr_array_add (ptr_array, NULL);
	data->button_titles = (const char **)g_ptr_array_free (ptr_array, FALSE);

	g_io_job_send_to_mainloop (job->io_job,
				   do_run_simple_dialog,
				   data,
				   NULL,
				   TRUE);

	res = data->result;

	g_free (data->button_titles);
	g_free (data);

	g_timer_continue (job->time);

	g_free (primary_text);
	g_free (secondary_text);
	
	return res;
}

#if 0 /* Not used at the moment */
static int
run_simple_dialog (CommonJob *job,
		   gboolean ignore_close_box,
		   GtkMessageType message_type,
		   char *primary_text,
		   char *secondary_text,
		   const char *details_text,
		   ...)
{
	va_list varargs;
	int res;

	va_start (varargs, details_text);
	res = run_simple_dialog_va (job,
				    ignore_close_box,
				    message_type,
				    primary_text,
				    secondary_text,
				    details_text,
				    varargs);
	va_end (varargs);
	return res;
}
#endif

static int
run_error (CommonJob *job,
	   char *primary_text,
	   char *secondary_text,
	   const char *details_text,
	   ...)
{
	va_list varargs;
	int res;

	va_start (varargs, details_text);
	res = run_simple_dialog_va (job,
				    FALSE,
				    GTK_MESSAGE_ERROR,
				    primary_text,
				    secondary_text,
				    details_text,
				    varargs);
	va_end (varargs);
	return res;
}

static int
run_warning (CommonJob *job,
	     char *primary_text,
	     char *secondary_text,
	     const char *details_text,
	     ...)
{
	va_list varargs;
	int res;

	va_start (varargs, details_text);
	res = run_simple_dialog_va (job,
				    FALSE,
				    GTK_MESSAGE_WARNING,
				    primary_text,
				    secondary_text,
				    details_text,
				    varargs);
	va_end (varargs);
	return res;
}

static gboolean
confirm_delete_from_trash (GIOJob *job,
			   GtkWindow *parent_window,
			   GList *files)
{
	char *prompt;
	char *file_name;
	int file_count;
	int response;

	/* Just Say Yes if the preference says not to confirm. */
	if (!confirm_trash_auto_value) {
		return TRUE;
	}

	file_count = g_list_length (files);
	g_assert (file_count > 0);
	
	if (file_count == 1) {
		file_name = get_display_name ((GFile *) files->data, NULL);
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\" "
					    "from the trash?"), file_name);
		g_free (file_name);
	} else {
		prompt = g_strdup_printf (ngettext("Are you sure you want to permanently delete "
						   "the %d selected item from the trash?",
						   "Are you sure you want to permanently delete "
						   "the %d selected items from the trash?",
						   file_count), 
					  file_count);
	}

	response = _run_alert (job, parent_window,
			       prompt,
			       _("If you delete an item, it will be permanently lost."),
			       GTK_STOCK_DELETE);
	
	return (response == 1);
}

static gboolean
confirm_deletion (GIOJob *job,
		  GtkWindow *parent_window,
		  GList *files,
		  gboolean all)
{
	char *prompt;
	char *detail;
	int file_count;
	GFile *file;
	char *file_name;
	int response;

	file_count = g_list_length (files);
	g_assert (file_count > 0);
	
	if (file_count == 1) {
		file = files->data;
		if (g_file_has_uri_scheme (file, "x-nautilus-desktop")) {
			/* Don't ask for desktop icons */
			return TRUE;
		}
		file_name = get_display_name (file, NULL);
		prompt = _("Cannot move file to trash, do you want to delete immediately?");
		detail = g_strdup_printf (_("The file \"%s\" cannot be moved to the trash."), file_name);
		g_free (file_name);
	} else {
		if (all) {
			prompt = _("Cannot move items to trash, do you want to delete them immediately?");
			detail = g_strdup_printf (ngettext("The selected item could not be moved to the Trash",
							   "The %d selected items could not be moved to the Trash",
							   file_count),
						  file_count);
		} else {
			prompt = _("Cannot move some items to trash, do you want to delete these immediately?");
			detail = g_strdup_printf (_("%d of the selected items cannot be moved to the Trash"), file_count);
		}
	}
	
	response = _run_yes_no_dialog (job,
				       prompt,
				       detail,
				       GTK_STOCK_DELETE, GTK_STOCK_CANCEL,
				       parent_window);
	
	g_free (detail);
	
	return (response == 1);
}

static gboolean
confirm_delete_directly (GIOJob *job,
			 GtkWindow *parent_window,
			 GList *files)
{
	char *prompt;
	char *file_name;
	int file_count;
	int response;

	/* Just Say Yes if the preference says not to confirm. */
	if (!confirm_trash_auto_value) {
		return TRUE;
	}

	file_count = g_list_length (files);
	g_assert (file_count > 0);

	if (can_delete_files_without_confirm (files)) {
		return TRUE;
	}

	if (file_count == 1) {
		file_name = get_display_name (files->data, NULL);
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\"?"), 
					  file_name);
		g_free (file_name);
	} else {
		prompt = g_strdup_printf (ngettext("Are you sure you want to permanently delete "
						   "the %d selected item?",
						   "Are you sure you want to permanently delete "
						   "the %d selected items?", file_count), file_count);
	}

	response = _run_alert (job, parent_window,
			       prompt,
			       _("If you delete an item, it will be permanently lost."),
			       GTK_STOCK_DELETE);

	return response == 1;
}


static void delete_job_done (gpointer data);

static void
delete_job (GIOJob *io_job,
	    GCancellable *cancellable,
	    gpointer user_data)
{
	DeleteJob *job = user_data;
	GList *trashable_files;
	GList *untrashable_files;
	GList *in_trash_files;
	GList *no_confirm_files;
	GList *l;
	GFile *file;
	gboolean confirmed;

	/* Collect three lists: (1) items that can be moved to trash,
	 * (2) items that can only be deleted in place, and (3) items that
	 * are already in trash. 
	 * 
	 * Always move (1) to trash if non-empty.
	 * Delete (3) only if (1) and (2) are non-empty, otherwise ignore (3).
	 * Ask before deleting (2) if non-empty.
	 * Ask before deleting (3) if non-empty.
	 */

	trashable_files = NULL;
	untrashable_files = NULL;
	in_trash_files = NULL;
	no_confirm_files = NULL;

	for (l = job->files; l != NULL; l = l->next) {
		file = l->data;
		
		if (job->try_trash &&
		    job->delete_if_all_already_in_trash &&
		    g_file_has_uri_scheme (file, "trash")) {
			in_trash_files = g_list_prepend (in_trash_files, file);
		} else if (can_delete_without_confirm (file)) {
			no_confirm_files = g_list_prepend (no_confirm_files, file);
		} else if (job->try_trash &&
			   can_trash_file (file, NULL)) {
			trashable_files = g_list_prepend (trashable_files, file);
		} else {
			untrashable_files = g_list_prepend (untrashable_files, file);
		}
	}

	if (in_trash_files != NULL && trashable_files == NULL && untrashable_files == NULL) {
		if (confirm_delete_from_trash (io_job, job->parent_window, in_trash_files)) {
			delete_files (in_trash_files, NULL, job->parent_window);
		}
	} else {
		if (no_confirm_files != NULL) {
			delete_files (no_confirm_files, NULL, job->parent_window);
		}
		if (trashable_files != NULL) {
			trash_files (trashable_files, NULL, job->parent_window);
		}
		if (untrashable_files != NULL) {
			if (job->try_trash) {
				confirmed = confirm_deletion (io_job, job->parent_window,
							      untrashable_files, trashable_files == NULL);
			} else {
				confirmed = confirm_delete_directly (io_job, job->parent_window, untrashable_files);
			}
				
			if (confirmed) {
				delete_files (untrashable_files, NULL, job->parent_window);
			}
		}
	}
	
	g_list_free (in_trash_files);
	g_list_free (trashable_files);
	g_list_free (untrashable_files);
	g_list_free (no_confirm_files);

	g_io_job_send_to_mainloop (io_job,
				   delete_job_done,
				   job,
				   NULL,
				   FALSE);

}

static void
delete_job_done (gpointer user_data)
{
	DeleteJob *job = user_data;
	GHashTable *debuting_uris;

	eel_g_object_list_free (job->files);
	if (job->parent_window) {
		g_object_unref (job->parent_window);
	}

	if (job->done_callback) {
		debuting_uris = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
		job->done_callback (debuting_uris, job->done_callback_data);
		g_hash_table_unref (debuting_uris);
	}
	g_free (job);

	nautilus_file_changes_consume_changes (TRUE);
}

static void
trash_or_delete_internal (GList                  *files,
			  GtkWindow              *parent_window,
			  gboolean                try_trash,			  
			  NautilusDeleteCallback  done_callback,
			  gpointer                done_callback_data)
{
	DeleteJob *job;

	setup_autos ();

	/* TODO: special case desktop icon link files ... */

	/* TODO: Progress dialog, cancellation */
	
	job = g_new0 (DeleteJob, 1);
	job->files = eel_g_object_list_copy (files);
	job->parent_window = g_object_ref (parent_window);
	job->try_trash = try_trash;
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	
	g_schedule_io_job (delete_job,
			   job,
			   NULL,
			   0,
			   NULL);
}

void
nautilus_file_operations_trash_or_delete (GList                  *files,
					  GtkWindow              *parent_window,
					  NautilusDeleteCallback  done_callback,
					  gpointer                done_callback_data)
{
	trash_or_delete_internal (files, parent_window,
				  TRUE,			  
				  done_callback,  done_callback_data);
}

void
nautilus_file_operations_delete (GList                  *files, 
				 GtkWindow              *parent_window,
				 NautilusDeleteCallback  done_callback,
				 gpointer                done_callback_data)
{
	trash_or_delete_internal (files, parent_window,
				  FALSE,			  
				  done_callback,  done_callback_data);
}

#ifdef GIO_CONVERSION_DONE

static void
do_empty_trash (GtkWidget *parent_view)
{
	TransferInfo *transfer_info;
	GList *trash_dir_list;

	nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "empty trash");

	/* TODO-gio: Implement */
	trash_dir_list = NULL;
	if (trash_dir_list != NULL) {
		/* set up the move parameters */
		transfer_info = transfer_info_new (parent_view);

		/* localizers: progress dialog title */
		transfer_info->operation_title = _("Emptying the Trash");
		/* localizers: label prepended to the progress count */
		transfer_info->action_label =_("Files deleted:");
		/* localizers: label prepended to the name of the current file deleted */
		transfer_info->progress_verb =_("Deleting");
		transfer_info->preparation_name =_("Preparing to Empty the Trash...");
		transfer_info->cleanup_name ="";
		transfer_info->error_mode = GNOME_VFS_XFER_ERROR_MODE_QUERY;
		transfer_info->overwrite_mode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
		transfer_info->kind = TRANSFER_EMPTY_TRASH;

		gnome_vfs_async_xfer (&transfer_info->handle, trash_dir_list, NULL,
		      		      GNOME_VFS_XFER_EMPTY_DIRECTORIES,
		      		      GNOME_VFS_XFER_ERROR_MODE_QUERY, 
		      		      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				      GNOME_VFS_PRIORITY_DEFAULT,
		      		      update_transfer_callback, transfer_info,
		      		      sync_transfer_callback, NULL);
	}

	gnome_vfs_uri_list_free (trash_dir_list);
}

static gboolean
confirm_empty_trash (GtkWidget *parent_view)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GdkScreen *screen;
	int response;

	/* Just Say Yes if the preference says not to confirm. */
	if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH)) {
		return TRUE;
	}
	
	screen = gtk_widget_get_screen (parent_view);

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Empty all of the items from "
					   "the trash?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you choose to empty "
						    "the trash, all items in "
						    "it will be permanently "
						    "lost. Please note that "
						    "you can also delete them "
						    "separately."));

	gtk_window_set_title (GTK_WINDOW (dialog), ""); /* as per HIG */
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "empty_trash",
				"Nautilus");

	/* Make transient for the window group */
        gtk_widget_realize (dialog);
	gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
				      gdk_screen_get_root_window (screen));

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);

	button = gtk_button_new_with_mnemonic (_("_Empty Trash"));
	gtk_widget_show (button);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				      GTK_RESPONSE_YES);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_YES);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
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

static gchar* 
get_trash_uri_for_volume (GnomeVFSVolume *volume)
{
	gchar *uri;
    
	uri = NULL;
	if (gnome_vfs_volume_handles_trash (volume)) {
		GnomeVFSURI     *trash_uri;
		GnomeVFSURI     *vol_uri;
		gchar           *vol_uri_str;

		vol_uri_str = gnome_vfs_volume_get_activation_uri (volume);
		vol_uri = gnome_vfs_uri_new (vol_uri_str);
		g_free (vol_uri_str);

		if (gnome_vfs_find_directory (vol_uri, 
					GNOME_VFS_DIRECTORY_KIND_TRASH,
					&trash_uri, FALSE, TRUE, 0777) == GNOME_VFS_OK)	{
			uri = gnome_vfs_uri_to_string (trash_uri, 0);
			gnome_vfs_uri_unref (trash_uri);
		}
		gnome_vfs_uri_unref (vol_uri);
	}
	return uri;
}

typedef struct {
    gpointer			               volume;
    GnomeVFSVolumeOpCallback	   callback;
    gpointer			               user_data;
} NautilusUnmountDataCallback;

static void 
delete_callback (GHashTable *debuting_uris,
                 gpointer    data)
{
	NautilusUnmountDataCallback 	*unmount_info;

	unmount_info = data;
	if (GNOME_IS_VFS_VOLUME (unmount_info->volume)) {
		gnome_vfs_volume_unmount (unmount_info->volume, 
		                          unmount_info->callback, 
		                          unmount_info->user_data);
		gnome_vfs_volume_unref (unmount_info->volume);
	} else if (GNOME_IS_VFS_DRIVE (unmount_info->volume)) {
		gnome_vfs_drive_unmount (unmount_info->volume, 
		                         unmount_info->callback, 
		                         unmount_info->user_data);
		gnome_vfs_drive_unref (unmount_info->volume);
	}
	g_free (unmount_info);
	g_hash_table_destroy (debuting_uris);
}

static gint
prompt_empty_trash (GtkWidget *parent_view)
{
	gint                    result;
	GtkWidget               *dialog;
	GdkScreen               *screen;

	screen = gtk_widget_get_screen (parent_view);

	/* Do we need to be modal ? */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			_("Do you want to empty the trash before you umount?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						_("In order to regain the "
						"free space on this device "
						"the trash must be emptied. "
						"All items in the trash "
						"will be permanently lost. "));		
	gtk_dialog_add_buttons (GTK_DIALOG (dialog), 
	                        _("Don't Empty Trash"), GTK_RESPONSE_REJECT, 
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
	                        _("Empty Trash"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_title (GTK_WINDOW (dialog), ""); /* as per HIG */
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "empty_trash",
		                                     "Nautilus");

	/* Make transient for the window group */
	gtk_widget_realize (dialog);
	gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
	gdk_screen_get_root_window (screen));

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return result;
}

#endif /* GIO_CONVERSION_DONE */


typedef struct {
	NautilusUnmountCallback callback;
	gpointer user_data;
} UnmountData;

static void
unmount_volume_callback (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	UnmountData *data = user_data;
	GError *error;

	error = NULL;
	g_volume_unmount_finish (G_VOLUME (source_object),
				 res, &error);
	if (data->callback) {
		data->callback (error, data->user_data);
	}

	if (error) {
		g_error_free (error);
	}
	
	g_free (data);
}


void
nautilus_file_operations_unmount_volume (GtkWindow                      *parent_window,
					 GVolume                        *volume,
					 NautilusUnmountCallback         callback,
					 gpointer                        user_data)
{
	/* TODO-gio: Empty trash before unmount */

#ifdef GIO_CONVERSION_DONE
	gchar              *trash_uri_str;
	gboolean           trash_is_empty;

	g_return_if_fail (parent_view != NULL);

	trash_is_empty = TRUE;
	trash_uri_str = get_trash_uri_for_volume (volume);
	if (trash_uri_str) {
		NautilusDirectory  *trash_dir;
		trash_dir = nautilus_directory_get_by_uri (trash_uri_str);	

		/* Check if the trash on this volume is empty, 
		* If the trash directory on this volume exists it's monitored 
		* by the trash monitor so this should give accurate results. */
		trash_is_empty = ! nautilus_directory_is_not_empty (trash_dir);
	}

	if (trash_is_empty) {
		/* no trash so unmount as usual */
		gnome_vfs_volume_unmount (volume, callback, user_data);
	} else {
		switch (prompt_empty_trash (parent_view)) {
			case GTK_RESPONSE_ACCEPT:
			{
				GList                         *trash_dir_list;
				NautilusUnmountDataCallback       *unmount_cb;
				GtkWidget *toplevel;

				trash_dir_list = NULL;
				unmount_cb = g_new (NautilusUnmountDataCallback, 1);
				unmount_cb->volume = gnome_vfs_volume_ref (volume);
				unmount_cb->callback = callback;
				unmount_cb->user_data = user_data;
				trash_dir_list = g_list_append (trash_dir_list, g_file_new_for_uri (trash_uri_str));
				toplevel = gtk_widget_get_toplevel (parent_view);
				nautilus_file_operations_delete (trash_dir_list, GTK_WINDOW (toplevel), 
				                             (NautilusDeleteCallback) delete_callback,
				                             unmount_cb);
				eel_g_object_list_free (trash_dir_list);
				/* volume is unmounted in the callback */
				break;
			}
			case GTK_RESPONSE_REJECT:
				gnome_vfs_volume_unmount (volume, callback, user_data);
				break;
			default:
				break;
		}
	}
	g_free (trash_uri_str);
#endif
	UnmountData *data;

	data = g_new0 (UnmountData, 1);
	data->callback = callback;
	data->user_data = user_data;
	g_volume_unmount (volume,
			  NULL,
			  unmount_volume_callback,
			  data);
}

#ifdef GIO_CONVERSION_DONE

struct RecursivePermissionsInfo {
	GnomeVFSAsyncHandle *handle;
	GnomeVFSURI *current_dir;
	GnomeVFSURI *current_file;
	GList *files;
	GList *directories;
	guint32 file_permissions;
	guint32 file_mask;
	guint32 dir_permissions;
	guint32 dir_mask;
	NautilusSetPermissionsCallback callback;
	gpointer callback_data;
};

struct FileInfo {
	char *name;
	guint32 permissions;
};

struct DirInfo {
	GnomeVFSURI *uri;
	guint32 permissions;
};

static void set_permissions_run (struct RecursivePermissionsInfo *info);

static void
set_permissions_set_file_info (GnomeVFSAsyncHandle *handle,
			       GnomeVFSResult result,
			       GnomeVFSFileInfo *old_file_info,
			       gpointer callback_data)
{
	struct RecursivePermissionsInfo *info;
	GnomeVFSFileInfo *vfs_info;
	GnomeVFSURI *uri;
	char *uri_str;
	struct FileInfo *file_info;
	
	info = callback_data;

	if (result == GNOME_VFS_OK && info->current_file != NULL) {
		uri_str = gnome_vfs_uri_to_string (info->current_file, GNOME_VFS_URI_HIDE_NONE);
		nautilus_file_changes_queue_file_changed (uri_str);
		g_free (uri_str);
	}

	if (info->current_file) {
		gnome_vfs_uri_unref (info->current_file);
	}
	if (info->files == NULL) {
		/* No more files, process more dirs */
		set_permissions_run (info);
		return;
	}

	file_info = info->files->data;
	info->files = g_list_delete_link (info->files, info->files);

	uri = gnome_vfs_uri_append_file_name (info->current_dir,
					      file_info->name);
	info->current_file = uri;
	
	vfs_info = gnome_vfs_file_info_new ();
	vfs_info->valid_fields = GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS;
	vfs_info->permissions = 
			(file_info->permissions & ~info->file_mask) |
			info->file_permissions;
	
	gnome_vfs_async_set_file_info (&info->handle, uri, vfs_info,
				       GNOME_VFS_SET_FILE_INFO_PERMISSIONS,
				       GNOME_VFS_FILE_INFO_DEFAULT,
				       GNOME_VFS_PRIORITY_DEFAULT,
				       set_permissions_set_file_info,
				       info);
	
	gnome_vfs_file_info_unref (vfs_info);
	g_free (file_info->name);
	g_free (file_info);
	
}

static void
set_permissions_got_files (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  GList *list,
			  guint entries_read,
			  gpointer callback_data)
{
	struct RecursivePermissionsInfo *info;
	GnomeVFSFileInfo *vfs_info;
	GList *l;
	
	info = callback_data;

	if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {
		for (l = list; l != NULL; l = l->next) {
			vfs_info = l->data;

			if (strcmp (vfs_info->name, ".") == 0 ||
			    strcmp (vfs_info->name, "..") == 0 ||
			    !(vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)) {
				continue;
			}

			if (vfs_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				struct DirInfo *dir_info;

				dir_info = g_new (struct DirInfo, 1);
				dir_info->uri = gnome_vfs_uri_append_file_name (info->current_dir,
										vfs_info->name);
				dir_info->permissions = vfs_info->permissions;
				info->directories = g_list_prepend (info->directories,
								    dir_info);
			} else {
				struct FileInfo *file_info;
				file_info = g_new (struct FileInfo, 1);
				file_info->name = g_strdup (vfs_info->name);
				file_info->permissions = vfs_info->permissions;
				info->files = g_list_prepend (info->files, file_info);
			}
		}
	}


	if (result != GNOME_VFS_OK) {
		/* Finished with this dir, work on the files */
		info->current_file = NULL;
		set_permissions_set_file_info (NULL, GNOME_VFS_OK, NULL, info);
	}
	
}

/* Also called for the toplevel dir */
static void
set_permissions_load_dir (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  GnomeVFSFileInfo *file_info,
			  gpointer callback_data)
{
	struct RecursivePermissionsInfo *info;
	char *uri_str;

	info = callback_data;
	
	if (result == GNOME_VFS_OK && handle != NULL) {
		uri_str = gnome_vfs_uri_to_string (info->current_dir, GNOME_VFS_URI_HIDE_NONE);
		nautilus_file_changes_queue_file_changed (uri_str);
		g_free (uri_str);
	}
	
	gnome_vfs_async_load_directory_uri (&info->handle,
					    info->current_dir,
					    GNOME_VFS_FILE_INFO_DEFAULT,
					    50,
					    GNOME_VFS_PRIORITY_DEFAULT,
					    set_permissions_got_files,
					    info);
}

static void
set_permissions_run (struct RecursivePermissionsInfo *info)
{
	struct DirInfo *dir_info;
	GnomeVFSFileInfo *vfs_info;
	
	gnome_vfs_uri_unref (info->current_dir);

	if (info->directories == NULL) {
		/* No more directories, finished! */
		info->callback (info->callback_data);
		/* All parts of info should be freed now */
		g_free (info);
		return;
	}

	dir_info = info->directories->data;
	info->directories = g_list_delete_link (info->directories, info->directories);

	info->current_dir = dir_info->uri;

	vfs_info = gnome_vfs_file_info_new ();
	vfs_info->valid_fields = GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS;
	vfs_info->permissions = 
			(dir_info->permissions & ~info->dir_mask) |
			info->dir_permissions;

	gnome_vfs_async_set_file_info (&info->handle,
				       info->current_dir,
				       vfs_info,
				       GNOME_VFS_SET_FILE_INFO_PERMISSIONS,
				       GNOME_VFS_FILE_INFO_DEFAULT,
				       GNOME_VFS_PRIORITY_DEFAULT,
				       set_permissions_load_dir,
				       info);

	gnome_vfs_file_info_unref (vfs_info);
	g_free (dir_info);
}
	
void
nautilus_file_set_permissions_recursive (const char                     *directory,
					 guint32         file_permissions,
					 guint32         file_mask,
					 guint32         dir_permissions,
					 guint32         dir_mask,
					 NautilusSetPermissionsCallback  callback,
					 gpointer                        callback_data)
{
	struct RecursivePermissionsInfo *info;

	info = g_new (struct RecursivePermissionsInfo, 1);

	info->files = NULL;
	info->directories = NULL;
	info->file_permissions = file_permissions;
	info->file_mask = file_mask;
	info->dir_permissions = dir_permissions;
	info->dir_mask = dir_mask;
	info->callback = callback;
	info->callback_data = callback_data;

	info->current_dir = gnome_vfs_uri_new (directory);

	if (info->current_dir == NULL) {
		info->callback (info->callback_data);
		g_free (info);
		return;
	}
	
	set_permissions_load_dir (NULL, GNOME_VFS_OK, NULL, info);
}

#endif /* GIO_CONVERSION_DONE */

#define op_job_new(__type, parent_window) ((__type *)(init_common (sizeof(__type), parent_window)))

static gpointer
init_common (gsize job_size,
	     GtkWindow *parent_window)
{
	CommonJob *common;

	common = g_malloc0 (job_size);
	
	common->parent_window = g_object_ref (parent_window);
	common->progress = nautilus_progress_info_new ();
	common->cancellable = nautilus_progress_info_get_cancellable (common->progress);
	common->time = g_timer_new ();

	return common;
}

static void
finalize_common (CommonJob *common)
{
	nautilus_progress_info_finish (common->progress);

	g_timer_destroy (common->time);
	
	if (common->parent_window) {
		 g_object_unref (common->parent_window);
	}
	if (common->skip_files) {
		g_hash_table_destroy (common->skip_files);
	}
	if (common->skip_readdir_error) {
		g_hash_table_destroy (common->skip_readdir_error);
	}
	g_object_unref (common->progress);
	g_object_unref (common->cancellable);
	g_free (common);
}

static void
skip_file (CommonJob *common,
	   GFile *file)
{
	if (common->skip_files == NULL) {
		common->skip_files =
			g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
	}

	g_hash_table_insert (common->skip_files, g_object_ref (file), file);
}

static void
skip_readdir_error (CommonJob *common,
		    GFile *dir)
{
	if (common->skip_readdir_error == NULL) {
		common->skip_readdir_error =
			g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
	}

	g_hash_table_insert (common->skip_readdir_error, g_object_ref (dir), dir);
}

static gboolean
should_skip_file (CommonJob *common,
		  GFile *file)
{
	if (common->skip_files != NULL) {
		return g_hash_table_lookup (common->skip_files, file) != NULL;
	}
	return FALSE;
}

static gboolean
should_skip_readdir_error (CommonJob *common,
			   GFile *dir)
{
	if (common->skip_readdir_error != NULL) {
		return g_hash_table_lookup (common->skip_readdir_error, dir) != NULL;
	}
	return FALSE;
}

typedef enum {
	OP_KIND_COPY = 0
} OpKind;

typedef struct {
	int num_files;
	goffset num_bytes;
	int num_files_since_progress;
	OpKind op;
} SourceInfo;

typedef struct {
	int num_files;
	goffset num_bytes;
	OpKind op;
	guint64 last_report_time;
} TransferInfo;

static void
report_count_progress (CommonJob *job,
		       SourceInfo *source_info)
{
	char *s;
	
	if (source_info->op == OP_KIND_COPY) {
		s = f (_("Preparing to copy %d files (%S)"),
		       source_info->num_files, source_info->num_bytes);
		nautilus_progress_info_take_details (job->progress, s);
	}

	nautilus_progress_info_pulse_progress (job->progress);
}

static void
count_file (GFileInfo *info,
	    CommonJob *job,
	    SourceInfo *source_info)
{
	source_info->num_files += 1;
	source_info->num_bytes += g_file_info_get_size (info);

	if (source_info->num_files_since_progress++ > 100) {
		report_count_progress (job, source_info);
		source_info->num_files_since_progress = 0;
	}
	
}
static void
scan_dir (GFile *dir,
	  SourceInfo *source_info,
	  CommonJob *job,
	  GQueue *dirs)
{
	GFileInfo *info;
	GError *error;
	GFile *subdir;
	GFileEnumerator *enumerator;
	char *primary, *secondary, *details;
	int response;
	SourceInfo saved_info;

	saved_info = *source_info;

 retry:
	error = NULL;
	enumerator = g_file_enumerate_children (dir,
						G_FILE_ATTRIBUTE_STD_NAME","
						G_FILE_ATTRIBUTE_STD_TYPE","
						G_FILE_ATTRIBUTE_STD_SIZE,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;
		while ((info = g_file_enumerator_next_file (enumerator, job->cancellable, &error)) != NULL) {
			count_file (info, job, source_info);

			if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
				subdir = g_file_get_child (dir,
							   g_file_info_get_name (info));
				
				/* Push to head, since we want depth-first */
				g_queue_push_head (dirs, subdir);
			}

			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		
		if (error) {
			primary = f (_("Error while copying."));
			details = NULL;
			
			if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
				secondary = f (_("Files in the folder \"%B\" cannot be copied because you do "
						 "not have permissions to read them."), dir);
			} else {
				secondary = f (_("There was an error getting information about the files in the folder \"%B\"."), dir);
				details = error->message;
			}
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						GTK_STOCK_CANCEL, RETRY, SKIP,
						NULL);
			
			g_error_free (error);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				job->aborted = TRUE;
			} else if (response == 1) {
				*source_info = saved_info;
				goto retry;
			} else if (response == 2) {
				skip_readdir_error (job, dir);
			} else {
				g_assert_not_reached ();
			}
		}
		
	} else if (job->skip_all_error) {
		skip_file (job, dir);
	} else {
		primary = f (_("Error while copying."));
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be copied because you do not have "
					 "permissions to read it."), dir);
		} else {
			secondary = f (_("There was an error reading the folder \"%B\"."), dir);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP, RETRY,
					NULL);

		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1 || response == 2) {
			if (response == 1) {
				job->skip_all_error = TRUE;
			}
			skip_file (job, dir);
		} else if (response == 3) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}
}	

static void
scan_file (GFile *file,
	   SourceInfo *source_info,
	   CommonJob *job)
{
	GFileInfo *info;
	GError *error;
	GQueue *dirs;
	GFile *dir;
	char *primary;
	char *secondary;
	char *details;
	int response;

	dirs = g_queue_new ();
	
 retry:
	error = NULL;
	info = g_file_query_info (file, 
				  G_FILE_ATTRIBUTE_STD_TYPE","
				  G_FILE_ATTRIBUTE_STD_SIZE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  job->cancellable,
				  &error);

	if (info) {
		count_file (info, job, source_info);

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			g_queue_push_head (dirs, g_object_ref (file));
		}
		
		g_object_unref (info);
	} else if (job->skip_all_error) {
		skip_file (job, file);
	} else {
		primary = f (_("Error while copying."));
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The file \"%B\" cannot be copied because you do not have "
					 "permissions to read it."), file);
		} else {
			secondary = f (_("There was an error getting information about \"%B\"."), file);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP, RETRY,
					NULL);
		
		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1 || response == 2) {
			if (response == 1) {
				job->skip_all_error = TRUE;
			}
			skip_file (job, file);
		} else if (response == 3) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}
		
	while (!job->aborted && 
	       (dir = g_queue_pop_head (dirs)) != NULL) {
		scan_dir (dir, source_info, job, dirs);
		g_object_unref (dir);
	}

	/* Free all from queue if we exited early */
	g_queue_foreach (dirs, (GFunc)g_object_unref, NULL);
	g_queue_free (dirs);
}

static void
scan_sources (GList *files,
	      SourceInfo *source_info,
	      CommonJob *job)
{
	GList *l;
	GFile *file;

	if (source_info->op == OP_KIND_COPY) {
		nautilus_progress_info_set_status (job->progress,
						   _("Preparing for copy"));
	}
	
	for (l = files; l != NULL && !job->aborted; l = l->next) {
		file = l->data;

		scan_file (file,
			   source_info,
			   job);
	}

	/* Make sure we report the final count */
	report_count_progress (job, source_info);
}

static void
verify_destination (CommonJob *job,
		    GFile *dest,
		    char **dest_fs_id,
		    goffset required_size)
{
	GFileInfo *info, *fsinfo;
	GError *error;
	guint64 free_size;
	char *primary, *secondary, *details;
	int response;
	GFileType file_type;

	*dest_fs_id = NULL;

 retry:
	
	error = NULL;
	info = g_file_query_info (dest, 
				  G_FILE_ATTRIBUTE_STD_TYPE","
				  G_FILE_ATTRIBUTE_ID_FS,
				  0,
				  job->cancellable,
				  &error);

	if (info == NULL) {
		primary = f (_("Error while copying to \"%B\"."), dest);
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("You don't have permissions to access the destination folder."));
		} else {
			secondary = f (_("There was an error getting information about the destination."));
			details = error->message;
		}

		response = run_error (job,
				      primary,
				      secondary,
				      details,
				      GTK_STOCK_CANCEL, RETRY,
				      NULL);
		
		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}

		return;
	}

	file_type = g_file_info_get_file_type (info);

	*dest_fs_id =
		g_strdup (g_file_info_get_attribute_string (info,
							    G_FILE_ATTRIBUTE_ID_FS));
	
	g_object_unref (info);
	
	if (file_type != G_FILE_TYPE_DIRECTORY) {
		primary = f (_("Error while copying to \"%B\"."), dest);
		secondary = f (_("The destination is not a folder."));

		response = run_error (job,
				      primary,
				      secondary,
				      NULL,
				      GTK_STOCK_CANCEL,
				      NULL);
		
		g_error_free (error);

		job->aborted = TRUE;
		return;
	}
	
	fsinfo = g_file_query_filesystem_info (dest,
					       G_FILE_ATTRIBUTE_FS_FREE","
					       G_FILE_ATTRIBUTE_FS_READONLY,
					       job->cancellable,
					       NULL);
	if (fsinfo == NULL) {
		/* All sorts of things can go wrong getting the fs info (like not supported)
		 * only check these things if the fs returns them
		 */
		return;
	}
	
	if (required_size > 0 &&
	    g_file_info_has_attribute (fsinfo, G_FILE_ATTRIBUTE_FS_FREE)) {
		free_size = g_file_info_get_attribute_uint64 (fsinfo,
							      G_FILE_ATTRIBUTE_FS_FREE);
		
		if (free_size < required_size) {
			primary = f (_("Error while copying to \"%B\"."), dest);
			secondary = _("There is not enough space on the destination. Try to remove files to make space.");
			
			details = f (_("There is %S availible, but %S is required."), free_size, required_size);
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						GTK_STOCK_CANCEL, RETRY,
						NULL);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				job->aborted = TRUE;
			} else if (response == 1) {
				goto retry;
			} else {
				g_assert_not_reached ();
			}
		}
	}
	
	if (!job->aborted &&
	    g_file_info_get_attribute_boolean (fsinfo,
					       G_FILE_ATTRIBUTE_FS_READONLY)) {
		primary = f (_("Error while copying to \"%B\"."), dest);
		secondary = f (_("The destination is read-only."));

		response = run_error (job,
				      primary,
				      secondary,
				      NULL,
				      GTK_STOCK_CANCEL,
				      NULL);
		
		g_error_free (error);

		job->aborted = TRUE;
	}
	
	g_object_unref (fsinfo);
}

static void
report_copy_progress (CopyMoveJob *copy_job,
		      SourceInfo *source_info,
		      TransferInfo *transfer_info)
{
	int files_left;
	goffset total_size;
	double elapsed, transfer_rate;
	int remaining_time;
	guint64 now;
	CommonJob *job;
	gboolean is_move;

	job = (CommonJob *)copy_job;

	is_move = copy_job->is_move;
	
	now = g_thread_gettime ();
	
	if (transfer_info->last_report_time != 0 &&
	    ABS (transfer_info->last_report_time - now) < 100 * NSEC_PER_MSEC) {
		return;
	}
	transfer_info->last_report_time = now;
	
	files_left = source_info->num_files - transfer_info->num_files;

	/* Races and whatnot could cause this to be negative... */
	if (files_left < 0) {
		files_left = 1;
	}

	if (source_info->num_files == 1) {
		nautilus_progress_info_take_status (job->progress,
						    f (is_move ?
						       _("Moving \"%B\" to \"%B\""):
						       _("Copying \"%B\" to \"%B\""),
						       (GFile *)copy_job->files->data,
						       copy_job->destination));
	} else if (copy_job->files != NULL &&
		   copy_job->files->next == NULL) {
		nautilus_progress_info_take_status (job->progress,
						    f (is_move?
						       _("Moving %d files (in \"%B\") to \"%B\""):
						       _("Copying %d files (in \"%B\") to \"%B\""),
						       files_left,
						       (GFile *)copy_job->files->data,
						       copy_job->destination));
	} else {
		nautilus_progress_info_take_status (job->progress,
						    f (is_move?
						       _("Moving %d files to \"%B\""):
						       _("Copying %d files to \"%B\""),
						       files_left, copy_job->destination));
	}

	total_size = MAX (source_info->num_bytes, transfer_info->num_bytes);
	
	elapsed = g_timer_elapsed (job->time, NULL);
	if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE) {
		char *s;
		s = f (_("%S of %S"), transfer_info->num_bytes, total_size);
		nautilus_progress_info_take_details (job->progress, s);
	} else {
		char *s;
		transfer_rate = transfer_info->num_bytes / elapsed;
		remaining_time = (total_size - transfer_info->num_bytes) / transfer_rate;

		s = f (_("%S of %S \xE2\x80\x94 %T left (%S/sec)"),
		       transfer_info->num_bytes, total_size,
		       remaining_time,
		       (goffset)transfer_rate);
		nautilus_progress_info_take_details (job->progress, s);
	}

	nautilus_progress_info_set_progress (job->progress, (double)transfer_info->num_bytes / total_size);
}

static GFile *
get_target_file (GFile *src,
		 GFile *dest_dir,
		 gboolean same_fs)
{
	char *basename;
	GFile *dest;
	GFileInfo *info;
	const char *copyname;

	dest = NULL;
	if (!same_fs) {
		info = g_file_query_info (src,
					  G_FILE_ATTRIBUTE_STD_COPY_NAME,
					  0, NULL, NULL);
		
		if (info) {
			copyname = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STD_COPY_NAME);

			if (copyname) {
				dest = g_file_get_child_for_display_name (dest_dir, copyname, NULL);
			}
			
			g_object_unref (info);
		}
	}

	if (dest == NULL) {
		basename = g_file_get_basename (src);
		dest = g_file_get_child (dest_dir, basename);
		g_free (basename);
	}
	
	return dest;
}

static gboolean
has_fs_id (GFile *file, const char *fs_id)
{
	const char *id;
	GFileInfo *info;
	gboolean res;

	res = FALSE;
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ID_FS,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);

	if (info) {
		id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FS);
		
		if (id && strcmp (id, fs_id) == 0) {
			res = TRUE;
		}
		
		g_object_unref (info);
	}
	
	return res;
}

static gboolean
is_dir (GFile *file)
{
	GFileInfo *info;
	gboolean res;

	res = FALSE;
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STD_TYPE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);
	if (info) {
		res = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
		g_object_unref (info);
	}
	
	return res;
}

static void copy_move_file (CopyMoveJob *job,
			    GFile *src,
			    GFile *dest_dir,
			    gboolean same_fs,
			    SourceInfo *source_info,
			    TransferInfo *transfer_info,
			    GHashTable *debuting_files,
			    GdkPoint *point,
			    gboolean overwrite,
			    gboolean *skipped_file);

static gboolean
create_dest_dir (CommonJob *job,
		 GFile *src,
		 GFile *dest)
{
	GError *error;
	char *primary, *secondary, *details;
	int response;

 retry:
	/* First create the directory, then copy stuff to it before
	   copying the attributes, because we need to be sure we can write to it */
	
	error = NULL;
	if (!g_file_make_directory (dest, job->cancellable, &error)) {
		primary = f (_("Error while copying."));
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be copied because you do not have "
					 "permissions to create it in the destination."), src);
		} else {
			secondary = f (_("There was an error creating the folder \"%B\"."), src);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP, RETRY,
					NULL);

		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) {
			/* Skip: Do Nothing  */
		} else if (response == 2) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
		return FALSE;
	}
	nautilus_file_changes_queue_file_added (dest);
	return TRUE;
}

static void
copy_move_directory (CopyMoveJob *copy_job,
		     GFile *src,
		     GFile *dest,
		     gboolean same_fs,
		     gboolean create_dest,
		     SourceInfo *source_info,
		     TransferInfo *transfer_info,
		     GHashTable *debuting_files,
		     gboolean *skipped_file)
{
	GFileInfo *info;
	GError *error;
	GFile *src_file;
	GFileEnumerator *enumerator;
	char *primary, *secondary, *details;
	int response;
	gboolean skip_error;
	gboolean local_skipped_file;
	CommonJob *job;

	job = (CommonJob *)copy_job;
	
	if (create_dest) {
		if (!create_dest_dir (job, src, dest)) {
			*skipped_file = TRUE;
			return;
		}
		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
		}

	}

	local_skipped_file = FALSE;
	
	skip_error = should_skip_readdir_error (job, src);
 retry:
	error = NULL;
	enumerator = g_file_enumerate_children (src,
						G_FILE_ATTRIBUTE_STD_NAME,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;
		
		while (!job->aborted &&
		       (info = g_file_enumerator_next_file (enumerator, job->cancellable, skip_error?NULL:&error)) != NULL) {
			src_file = g_file_get_child (src,
						     g_file_info_get_name (info));
			copy_move_file (copy_job, src_file, dest, same_fs, source_info, transfer_info, NULL, NULL, FALSE, &local_skipped_file);
			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		
		if (error) {
			if (copy_job->is_move) {
				primary = f (_("Error while moving."));
			} else {
				primary = f (_("Error while copying."));
			}
			details = NULL;
			
			if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
				secondary = f (_("Files in the folder \"%B\" cannot be copied because you do "
						 "not have permissions to read them."), src);
			} else {
				secondary = f (_("There was an error getting information about the files in the folder \"%B\"."), src);
				details = error->message;
			}
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						GTK_STOCK_CANCEL, _("_Skip files"),
						NULL);
			
			g_error_free (error);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				job->aborted = TRUE;
			} else if (response == 1) {
				/* Skip: Do Nothing */
				local_skipped_file = TRUE;
			} else {
				g_assert_not_reached ();
			}
		}

		/* Count the copied directory as a file */
		transfer_info->num_files ++;
		report_copy_progress (copy_job, source_info, transfer_info);

		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (create_dest));
		}
	} else {
		if (copy_job->is_move) {
			primary = f (_("Error while moving."));
		} else {
			primary = f (_("Error while copying."));
		}
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be copied because you do not have "
					 "permissions to read it."), src);
		} else {
			secondary = f (_("There was an error reading the folder \"%B\"."), src);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP, RETRY,
					NULL);

		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) {
			/* Skip: Do Nothing  */
			local_skipped_file = TRUE;
		} else if (response == 2) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}

	if (create_dest) {
		/* Ignore errors here. Failure to copy metadata is not a hard error */
		g_file_copy_attributes (src, dest,
					G_FILE_COPY_NOFOLLOW_SYMLINKS,
					job->cancellable, NULL);
	}

	if (!job->aborted && copy_job->is_move &&
	    /* Don't delete source if there was a skipped file */
	    !local_skipped_file) {
		if (!g_file_delete (src, job->cancellable, &error)) {
			if (job->skip_all_error) {
				goto skip;
			}
			primary = f (_("Error while moving \"%B\"."), src);
			secondary = f (_("Couldn't remove the source folder."));
			details = error->message;
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
						NULL);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				job->aborted = TRUE;
			} else if (response == 1) { /* skip all */
				job->skip_all_error = TRUE;
				local_skipped_file = TRUE;
			} else if (response == 2) { /* skip */
				local_skipped_file = TRUE;
			} else {
				g_assert_not_reached ();
			}
			
		skip:
			g_error_free (error);
		}
	}

	if (local_skipped_file) {
		*skipped_file = TRUE;
	}
}

static gboolean
remove_target_recursively (CommonJob *job,
			   GFile *src,
			   GFile *toplevel_dest,
			   GFile *file)
{
	GFileEnumerator *enumerator;
	GError *error;
	GFile *child;
	gboolean stop;
	char *primary, *secondary, *details;
	int response;
	GFileInfo *info;

	stop = FALSE;
	
	error = NULL;
	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STD_NAME,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;
		
		while (!job->aborted &&
		       (info = g_file_enumerator_next_file (enumerator, job->cancellable, &error)) != NULL) {
			child = g_file_get_child (file,
						  g_file_info_get_name (info));
			if (!remove_target_recursively (job, src, toplevel_dest, child)) {
				stop = TRUE;
				break;
			}
			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		
	} else if (IS_IO_ERROR (error, NOT_DIRECTORY)) {
		/* Not a dir, continue */
		g_error_free (error);
		
	} else {
		if (job->skip_all_error) {
			goto skip1;
		}
		
		primary = f (_("Error while copying \"%B\"."), src);
		secondary = f (_("Couldn't remove files from the already folder %F."), file);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	skip1:
		g_error_free (error);
		
		stop = TRUE;
	}

	if (stop) {
		return FALSE;
	}

	error = NULL;
	
	if (!g_file_delete (file, job->cancellable, &error)) {
		if (job->skip_all_error) {
			goto skip2;
		}
		primary = f (_("Error while copying \"%B\"."), src);
		secondary = f (_("Couldn't remove the already existing file %F."), file);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}

	skip2:
		g_error_free (error);
		
		return FALSE;
	}
	nautilus_file_changes_queue_file_removed (file);
	nautilus_file_changes_queue_schedule_metadata_remove (file);
	
	return TRUE;
	
}

typedef struct {
	CopyMoveJob *job;
	goffset last_size;
	SourceInfo *source_info;
	TransferInfo *transfer_info;
} ProgressData;

static void
copy_file_progress_callback (goffset current_num_bytes,
			     goffset total_num_bytes,
			     gpointer user_data)
{
	ProgressData *pdata;
	goffset new_size;

	pdata = user_data;
	
	new_size = current_num_bytes - pdata->last_size;

	if (new_size > 0) {
		pdata->transfer_info->num_bytes += new_size;
		pdata->last_size = current_num_bytes;
		report_copy_progress (pdata->job,
				      pdata->source_info,
				      pdata->transfer_info);
	}
}

/* Debuting files is non-NULL only for toplevel items */
static void
copy_move_file (CopyMoveJob *copy_job,
		GFile *src,
		GFile *dest_dir,
		gboolean same_fs,
		SourceInfo *source_info,
		TransferInfo *transfer_info,
		GHashTable *debuting_files,
		GdkPoint *position,
		gboolean overwrite,
		gboolean *skipped_file)
{
	GFile *dest;
	GError *error;
	GFileCopyFlags flags;
	char *primary, *secondary, *details;
	int response;
	ProgressData pdata;
	gboolean would_recurse;
	CommonJob *job;
	gboolean res;

	job = (CommonJob *)copy_job;
	
	if (should_skip_file (job, src)) {
		*skipped_file = TRUE;
		return;
	}

	dest = get_target_file (src, dest_dir, same_fs);

 retry:
	
	error = NULL;
	flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;
	if (overwrite) {
		flags |= G_FILE_COPY_OVERWRITE;
	}
	pdata.job = copy_job;
	pdata.last_size = 0;
	pdata.source_info = source_info;
	pdata.transfer_info = transfer_info;

	if (copy_job->is_move) {
		res = g_file_move (src, dest,
				   flags,
				   job->cancellable,
				   copy_file_progress_callback,
				   &pdata,
				   &error);
	} else {
		res = g_file_copy (src, dest,
				   flags,
				   job->cancellable,
				   copy_file_progress_callback,
				   &pdata,
				   &error);
	}
	
	if (res) {
		transfer_info->num_files ++;
		report_copy_progress (copy_job, source_info, transfer_info);

		if (debuting_files) {
			nautilus_file_changes_queue_schedule_metadata_copy (src, dest);
			if (position) {
				nautilus_file_changes_queue_schedule_position_set (dest, *position, copy_job->screen_num);
			} else {
				nautilus_file_changes_queue_schedule_position_remove (dest);
			}
			
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
		}
		nautilus_file_changes_queue_file_added (dest);
		g_object_unref (dest);
		return;
	}

	/* Conflict */
	if (!overwrite &&
	    IS_IO_ERROR (error, EXISTS)) {
		gboolean is_merge;

		is_merge = FALSE;
		if (is_dir (dest)) {
			if (is_dir (src)) {
				is_merge = TRUE;
				primary = f (_("A folder named \"%B\" already exists.  Do you want to merge the source folder?"), 
					     dest);
				secondary = f (_("The source folder already exists in \"%B\".  "
						 "Merging will ask for confirmation before replacing any files in the folder that conflict with the files being copied."), 
					       dest_dir);
				
			} else {
				primary = f (_("A folder named \"%B\" already exists.  Do you want to replace it?"), 
							    dest);
				secondary = f (_("The folder already exists in \"%F\".  "
						 "Replacing it will remove all files in the folder."), 
					       dest_dir);
			}
		} else {
			primary = f (_("A file named \"%B\" already exists.  Do you want to replace it?"), 
				     dest);
			secondary = f (_("The file already exists in \"%F\".  "
					 "Replacing it will overwrite its content."), 
				       dest_dir);
		}

		if ((is_merge && job->merge_all) ||
		    (!is_merge && job->replace_all)) {
			g_free (primary);
			g_free (secondary);
			g_error_free (error);
			
			overwrite = TRUE;
			goto retry;
		}

		if (job->skip_all_conflict) {
			g_free (primary);
			g_free (secondary);
			g_error_free (error);
			
			goto out;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					NULL,
					GTK_STOCK_CANCEL,
					SKIP_ALL,
					is_merge?MERGE_ALL:REPLACE_ALL,
					SKIP,
					is_merge?MERGE:REPLACE,
					NULL);
		
		g_error_free (error);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1 || response == 3) { /* skip all / skip */
			if (response == 1) {
				job->skip_all_conflict = TRUE;
			}
		} else if (response == 2 || response == 4) { /* merge/replace all  / merge/replace*/
			if (response == 2) {
				if (is_merge) {
					job->merge_all = TRUE;
				} else {
					job->replace_all = TRUE;
				}
			}
			overwrite = TRUE;
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}
	
	else if (overwrite &&
		 IS_IO_ERROR (error, IS_DIRECTORY)) {

		g_error_free (error);
		
		if (remove_target_recursively (job, src, dest, dest)) {
			goto retry;
		}
	}
	
	/* Needs to recurse */
	else if (IS_IO_ERROR (error, WOULD_RECURSE) ||
		 IS_IO_ERROR (error, WOULD_MERGE)) {
		would_recurse = error->code == G_IO_ERROR_WOULD_RECURSE;
		g_error_free (error);

		if (overwrite && would_recurse) {
			error = NULL;
			
			/* Copying a dir onto file, first remove the file */
			if (!g_file_delete (dest, job->cancellable, &error) &&
			    !IS_IO_ERROR (error, NOT_FOUND)) {
				if (job->skip_all_error) {
					goto out;
				}
				if (copy_job->is_move) {
					primary = f (_("Error while moving \"%B\"."), src);
				} else {
					primary = f (_("Error while copying \"%B\"."), src);
				}
				secondary = f (_("Couldn't remove the already existing file with the same name in %F."), dest_dir);
				details = error->message;
				
				response = run_warning (job,
							primary,
							secondary,
							details,
							GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
							NULL);
				
				g_error_free (error);
				
				if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
					job->aborted = TRUE;
				} else if (response == 1) { /* skip all */
					job->skip_all_error = TRUE;
				} else if (response == 2) { /* skip */
					/* do nothing */
				} else {
					g_assert_not_reached ();
				}
				goto out;
				
			}
			if (error) {
				g_error_free (error);
				error = NULL;
			}
			if (debuting_files) { /* Only remove metadata for toplevel items */
				nautilus_file_changes_queue_schedule_metadata_remove (dest);
			}
			nautilus_file_changes_queue_file_removed (dest);
		}

		copy_move_directory (copy_job, src, dest, same_fs,
				     would_recurse,
				     source_info, transfer_info,
				     debuting_files, skipped_file);

		g_object_unref (dest);
		return;
	}
	
	/* Other error */
	else {
		if (job->skip_all_error) {
			goto out;
		}
		primary = f (_("Error while copying \"%B\"."), src);
		secondary = f (_("There was an error copying the file into %F."), dest_dir);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		g_error_free (error);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	}
 out:
	*skipped_file = TRUE; /* Or aborted, but same-same */
	g_object_unref (dest);
}

static void
copy_files (CopyMoveJob *job,
	    const char *dest_fs_id,
	    SourceInfo *source_info,
	    TransferInfo *transfer_info)
{
	CommonJob *common;
	GList *l;
	GFile *src;
	gboolean same_fs;
	int i;
	GdkPoint *point;
	gboolean skipped_file;

	common = &job->common;

	report_copy_progress (job, source_info, transfer_info);
	
	i = 0;
	for (l = job->files;
	     l != NULL && !common->aborted ;
	     l = l->next) {
		src = l->data;

		if (i < job->n_icon_positions) {
			point = &job->icon_positions[i];
		} else {
			point = NULL;
		}

		
		same_fs = FALSE;
		if (dest_fs_id) {
			same_fs = has_fs_id (src, dest_fs_id);
		}

		skipped_file = FALSE;
		copy_move_file (job, src, job->destination,
				same_fs,
				source_info, transfer_info,
				job->debuting_files,
				point, FALSE, &skipped_file);
		i++;
	}
}

static void
copy_job_done (gpointer user_data)
{
	CopyMoveJob *job;

	job = user_data;
	if (job->done_callback) {
		job->done_callback (job->debuting_files, job->done_callback_data);
	}

	eel_g_object_list_free (job->files);
	g_object_unref (job->destination);
	g_hash_table_unref (job->debuting_files);
	g_free (job->icon_positions);
	
	finalize_common ((CommonJob *)job);

	nautilus_file_changes_consume_changes (TRUE);
}

static void
copy_job (GIOJob *io_job,
	  GCancellable *cancellable,
	  gpointer user_data)
{
	CopyMoveJob *job;
	CommonJob *common;
	SourceInfo source_info;
	TransferInfo transfer_info;
	char *dest_fs_id;

	job = user_data;
	common = &job->common;
	common->io_job = io_job;

	dest_fs_id = NULL;
	
	nautilus_progress_info_start (job->common.progress);
	
	memset (&source_info, 0, sizeof (SourceInfo));
	scan_sources (job->files,
		      &source_info,
		      common);
	if (common->aborted) {
		goto aborted;
	}

	verify_destination (&job->common,
			    job->destination,
			    &dest_fs_id,
			    source_info.num_bytes);
	if (common->aborted) {
		goto aborted;
	}

	g_timer_start (job->common.time);
	
	memset (&transfer_info, 0, sizeof (transfer_info));
	copy_files (job,
		    dest_fs_id,
		    &source_info, &transfer_info);
	
 aborted:
	
	g_free (dest_fs_id);
	
	g_io_job_send_to_mainloop (io_job,
				   copy_job_done,
				   job,
				   NULL,
				   FALSE);
}

void
nautilus_file_operations_copy (GList *files,
			       GArray *relative_item_points,
			       GFile *target_dir,
			       GtkWindow *parent_window,
			       NautilusCopyCallback  done_callback,
			       gpointer done_callback_data)
{
	CopyMoveJob *job;
	GdkScreen *screen;

	job = op_job_new (CopyMoveJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = eel_g_object_list_copy (files);
	job->destination = g_object_ref (target_dir);
	if (relative_item_points != NULL &&
	    relative_item_points->len > 0) {
		job->icon_positions =
			g_memdup (relative_item_points->data,
				  sizeof (GdkPoint) * relative_item_points->len);
		job->n_icon_positions = relative_item_points->len;
	}
	job->screen_num = 0;
	if (parent_window) {
		screen = gtk_widget_get_screen (GTK_WIDGET (parent_window));
		job->screen_num = gdk_screen_get_number (screen);
	}
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	g_schedule_io_job (copy_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}

static void
report_move_progress (CopyMoveJob *move_job, int total, int left)
{
	CommonJob *job;

	job = (CommonJob *)move_job;
	
	nautilus_progress_info_take_status (job->progress,
					    f (_("Preparing to Move to \"%B\""),
					       move_job->destination));

	nautilus_progress_info_take_details (job->progress,
					     f (_("Preparing to move %d files"), left));

	nautilus_progress_info_pulse_progress (job->progress);
}

static void
move_file_prepare (CopyMoveJob *move_job,
		   GFile *src,
		   GFile *dest_dir,
		   gboolean same_fs,
		   GHashTable *debuting_files,
		   GdkPoint *position,
		   GList **copy_files,
		   GArray *copy_positions)
{
	GFile *dest;
	GError *error;
	CommonJob *job;
	gboolean overwrite;
	char *primary, *secondary, *details;
	int response;
	GFileCopyFlags flags;
	gboolean would_recurse;

	overwrite = FALSE;

	job = (CommonJob *)move_job;
	
	dest = get_target_file (src, dest_dir, same_fs);

 retry:
	
	flags = G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_NO_FALLBACK_FOR_MOVE;
	if (overwrite) {
		flags |= G_FILE_COPY_OVERWRITE;
	}
	
	error = NULL;
	if (g_file_move (src, dest,
			 flags,
			 job->cancellable,
			 NULL,
			 NULL,
			 &error)) {
		
		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
		}
		
		nautilus_file_changes_queue_file_moved (src, dest);
		nautilus_file_changes_queue_schedule_metadata_move (src, dest);
		
		return;
	}

	/* Conflict */
	if (!overwrite &&
	    IS_IO_ERROR (error, EXISTS)) {
		gboolean is_merge;
		
		g_error_free (error);
		
		is_merge = FALSE;
		if (is_dir (dest)) {
			if (is_dir (src)) {
				is_merge = TRUE;
				primary = f (_("A folder named \"%B\" already exists.  Do you want to merge the source folder?"), 
					     dest);
				secondary = f (_("The source folder already exists in \"%B\".  "
						 "Merging will ask for confirmation before replacing any files in the folder that conflict with the files being moved."), 
					       dest_dir);
				
			} else {
				primary = f (_("A folder named \"%B\" already exists.  Do you want to replace it?"), 
							    dest);
				secondary = f (_("The folder already exists in \"%F\".  "
						 "Replacing it will remove all files in the folder."), 
					       dest_dir);
			}
		} else {
			primary = f (_("A file named \"%B\" already exists.  Do you want to replace it?"), 
				     dest);
			secondary = f (_("The file already exists in \"%F\".  "
					 "Replacing it will overwrite its content."), 
				       dest_dir);
		}

		if ((is_merge && job->merge_all) ||
		    (!is_merge && job->replace_all)) {
			g_free (primary);
			g_free (secondary);
			g_error_free (error);
			
			overwrite = TRUE;
			goto retry;
		}

		if (job->skip_all_conflict) {
			g_free (primary);
			g_free (secondary);
			g_error_free (error);
			
			goto out;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					NULL,
					GTK_STOCK_CANCEL,
					SKIP_ALL,
					is_merge?MERGE_ALL:REPLACE_ALL,
					SKIP,
					is_merge?MERGE:REPLACE,
					NULL);
		
		g_error_free (error);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1 || response == 3) { /* skip all / skip */
			if (response == 1) {
				job->skip_all_conflict = TRUE;
			}
		} else if (response == 2 || response == 4) { /* merge/replace all  / merge/replace*/
			if (response == 2) {
				if (is_merge) {
					job->merge_all = TRUE;
				} else {
					job->replace_all = TRUE;
				}
			}
			overwrite = TRUE;
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}

	else if (IS_IO_ERROR (error, WOULD_RECURSE) ||
		 IS_IO_ERROR (error, WOULD_MERGE) ||
		 IS_IO_ERROR (error, NOT_SUPPORTED)) {
		gboolean delete_dest;
		would_recurse = error->code == G_IO_ERROR_WOULD_RECURSE;
		g_error_free (error);

		delete_dest = FALSE;
		if (overwrite && would_recurse) {
			delete_dest = TRUE;
			
			error = NULL;
		}

		*copy_files = g_list_prepend (*copy_files, src);
		if (position) {
			g_array_append_val (copy_positions, *position);
		}
	}

	/* Other error */
	else {
		if (job->skip_all_error) {
			goto out;
		}
		primary = f (_("Error while moving \"%B\"."), src);
		secondary = f (_("There was an error moving the file into %F."), dest_dir);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		g_error_free (error);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			job->aborted = TRUE;
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	}
	
 out:
	g_object_unref (dest);
}

static void
move_files_prepare (CopyMoveJob *job,
		    const char *dest_fs_id,
		    GList **copy_files,
		    GArray *copy_positions)
{
	CommonJob *common;
	GList *l;
	GFile *src;
	gboolean same_fs;
	int i;
	GdkPoint *point;
	int total, left;

	common = &job->common;

	total = left = g_list_length (job->files);

	report_move_progress (job, total, left);

	i = 0;
	for (l = job->files;
	     l != NULL && !common->aborted ;
	     l = l->next) {
		src = l->data;

		if (i < job->n_icon_positions) {
			point = &job->icon_positions[i];
		} else {
			point = NULL;
		}

		
		same_fs = FALSE;
		if (dest_fs_id) {
			same_fs = has_fs_id (src, dest_fs_id);
		}
		
		move_file_prepare (job, src, job->destination,
				   same_fs,
				   job->debuting_files,
				   point,
				   copy_files,
				   copy_positions);
		i++;
	}

}

static void
move_files (CopyMoveJob *job,
	    GList *files,
	    GArray *icon_positions,
	    const char *dest_fs_id,
	    SourceInfo *source_info,
	    TransferInfo *transfer_info)
{
	CommonJob *common;
	GList *l;
	GFile *src;
	gboolean same_fs;
	int i;
	GdkPoint *point;
	gboolean skipped_file;

	common = &job->common;

	report_copy_progress (job, source_info, transfer_info);
	
	i = 0;
	for (l = files;
	     l != NULL && !common->aborted ;
	     l = l->next) {
		src = l->data;

		if (i < icon_positions->len) {
			point = &g_array_index (icon_positions, GdkPoint, i);
		} else {
			point = NULL;
		}
		
		same_fs = FALSE;
		if (dest_fs_id) {
			same_fs = has_fs_id (src, dest_fs_id);
		}

		/* Set overwrite to true, as the user has
		   selected overwrite on all toplevel items */
		skipped_file = FALSE;
		copy_move_file (job, src, job->destination,
				same_fs,
				source_info, transfer_info,
				job->debuting_files,
				point, TRUE, &skipped_file);
		i++;
	}
}


static void
move_job_done (gpointer user_data)
{
	CopyMoveJob *job;

	job = user_data;
	if (job->done_callback) {
		job->done_callback (job->debuting_files, job->done_callback_data);
	}

	eel_g_object_list_free (job->files);
	g_object_unref (job->destination);
	g_hash_table_unref (job->debuting_files);
	g_free (job->icon_positions);
	
	finalize_common ((CommonJob *)job);

	nautilus_file_changes_consume_changes (TRUE);
}

static void
move_job (GIOJob *io_job,
	  GCancellable *cancellable,
	  gpointer user_data)
{
	CopyMoveJob *job;
	CommonJob *common;
	GList *copy_files;
	GArray *copy_positions;
	SourceInfo source_info;
	TransferInfo transfer_info;
	char *dest_fs_id;

	job = user_data;
	common = &job->common;
	common->io_job = io_job;

	dest_fs_id = NULL;

	copy_files = NULL;
	copy_positions = NULL;
	
	nautilus_progress_info_start (job->common.progress);
	
	verify_destination (&job->common,
			    job->destination,
			    &dest_fs_id,
			    -1);
	if (common->aborted) {
		goto aborted;
	}

	copy_files = NULL;
	copy_positions = g_array_new (FALSE, TRUE, sizeof (GdkPoint));

	/* This moves all files that we can do without copy + delete */
	move_files_prepare (job, dest_fs_id, &copy_files, copy_positions);
	if (common->aborted) {
		goto aborted;
	}

	copy_files = g_list_reverse (copy_files);

	/* The rest we need to do deep copy + delete behind on,
	   so scan for size */
	
	memset (&source_info, 0, sizeof (SourceInfo));
	scan_sources (copy_files,
		      &source_info,
		      common);
	if (common->aborted) {
		goto aborted;
	}

	verify_destination (&job->common,
			    job->destination,
			    &dest_fs_id,
			    source_info.num_bytes);
	if (common->aborted) {
		goto aborted;
	}

	memset (&transfer_info, 0, sizeof (transfer_info));
	move_files (job,
		    copy_files,
		    copy_positions,
		    dest_fs_id,
		    &source_info, &transfer_info);

 aborted:
	g_list_free (copy_files);
	if (copy_positions) {
		g_array_free (copy_positions, TRUE);
	}

	g_free (dest_fs_id);
	
	g_io_job_send_to_mainloop (io_job,
				   move_job_done,
				   job,
				   NULL,
				   FALSE);
}

void
nautilus_file_operations_move (GList *files,
			       GArray *relative_item_points,
			       GFile *target_dir,
			       GtkWindow *parent_window,
			       NautilusCopyCallback  done_callback,
			       gpointer done_callback_data)
{
	CopyMoveJob *job;
	GdkScreen *screen;

	job = op_job_new (CopyMoveJob, parent_window);
	job->is_move = TRUE;
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = eel_g_object_list_copy (files);
	job->destination = g_object_ref (target_dir);
	if (relative_item_points != NULL &&
	    relative_item_points->len > 0) {
		job->icon_positions =
			g_memdup (relative_item_points->data,
				  sizeof (GdkPoint) * relative_item_points->len);
		job->n_icon_positions = relative_item_points->len;
	}
	job->screen_num = 0;
	if (parent_window) {
		screen = gtk_widget_get_screen (GTK_WIDGET (parent_window));
		job->screen_num = gdk_screen_get_number (screen);
	}
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	g_schedule_io_job (move_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}



static void
not_supported_yet (void)
{
	eel_show_warning_dialog ("This operation isn't supported with the gio-based nautilus",
				 "This is work in progress. Please be patient",
				 NULL);
}

void
nautilus_file_set_permissions_recursive (const char                     *directory,
					 guint32         file_permissions,
					 guint32         file_mask,
					 guint32         dir_permissions,
					 guint32         dir_mask,
					 NautilusSetPermissionsCallback  callback,
					 gpointer                        callback_data)
{
	/* TODO-gio: Implement */
	not_supported_yet ();
}

static GList *
location_list_from_uri_list (const GList *uris)
{
	const GList *l;
	GList *files;
	GFile *f;

	files = NULL;
	for (l = uris; l != NULL; l = l->next) {
		f = g_file_new_for_uri (l->data);
		files = g_list_prepend (files, f);
	}

	return g_list_reverse (files);
}

void
nautilus_file_operations_copy_move (const GList *item_uris,
				    GArray *relative_item_points,
				    const char *target_dir,
				    GdkDragAction copy_action,
				    GtkWidget *parent_view,
				    NautilusCopyCallback  done_callback,
				    gpointer done_callback_data)
{
	GList *locations;
	GFile *dest;
	GtkWindow *parent_window;
	
	dest = g_file_new_for_uri (target_dir);
	locations = location_list_from_uri_list (item_uris);

	parent_window = NULL;
	if (parent_view) {
		parent_window = (GtkWindow *)gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
	}
	
	if (copy_action == GDK_ACTION_COPY) {
		nautilus_file_operations_copy (locations,
					       relative_item_points,
					       dest,
					       parent_window,
					       done_callback, done_callback_data);
		
	} else if (copy_action == GDK_ACTION_MOVE) {

		if (g_file_has_uri_scheme (dest, "trash")) {
			nautilus_file_operations_trash_or_delete (locations,
								  parent_window,
								  done_callback, done_callback_data);
		} else {
			nautilus_file_operations_move (locations,
						       relative_item_points,
						       dest,
						       parent_window,
						       done_callback, done_callback_data);
		}
	} else {
		/* TODO-gio: Implement link */
		not_supported_yet ();
	}
	
	eel_g_object_list_free (locations);
	g_object_unref (dest);
}

void 
nautilus_file_operations_new_folder (GtkWidget *parent_view, 
				     GdkPoint *target_point,
				     const char *parent_dir,
				     NautilusNewFolderCallback done_callback,
				     gpointer data)
{
	/* TODO-gio: Implement */
	not_supported_yet ();
}

void 
nautilus_file_operations_new_file_from_template (GtkWidget *parent_view, 
						 GdkPoint *target_point,
						 const char *parent_dir,
						 const char *target_filename,
						 const char *template_uri,
						 NautilusNewFileCallback done_callback,
						 gpointer data)
{
	/* TODO-gio: Implement */
	not_supported_yet ();
}

void 
nautilus_file_operations_new_file (GtkWidget *parent_view, 
				   GdkPoint *target_point,
				   const char *parent_dir,
				   const char *initial_contents,
				   NautilusNewFileCallback done_callback,
				   gpointer data)
{
	/* TODO-gio: Implement */
	not_supported_yet ();
}


void 
nautilus_file_operations_empty_trash (GtkWidget *parent_view)
{
	/* TODO-gio: Implement */
	not_supported_yet ();
}


#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_operations (void)
{
	setlocale (LC_MESSAGES, "C");

#ifdef GIO_CONVERSION_DONE
	
	/* test the next duplicate name generator */
	EEL_CHECK_STRING_RESULT (get_duplicate_name (" (copy)", 1), " (another copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo", 1), "foo (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name (".bashrc", 1), ".bashrc (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name (".foo.txt", 1), ".foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo", 1), "foo foo (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo.txt", 1), "foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo.txt", 1), "foo foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo.txt txt", 1), "foo foo (copy).txt txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo...txt", 1), "foo (copy)...txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo...", 1), "foo (copy)...");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo. (copy)", 1), "foo. (another copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (copy)", 1), "foo (another copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (copy).txt", 1), "foo (another copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (another copy)", 1), "foo (3rd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (another copy).txt", 1), "foo (3rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (another copy).txt", 1), "foo foo (3rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (13th copy)", 1), "foo (14th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (13th copy).txt", 1), "foo (14th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (21st copy)", 1), "foo (22nd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (21st copy).txt", 1), "foo (22nd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (22nd copy)", 1), "foo (23rd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (22nd copy).txt", 1), "foo (23rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (23rd copy)", 1), "foo (24th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (23rd copy).txt", 1), "foo (24th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (24th copy)", 1), "foo (25th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (24th copy).txt", 1), "foo (25th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (24th copy)", 1), "foo foo (25th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (24th copy).txt", 1), "foo foo (25th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (100000000000000th copy).txt", 1), "foo foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (10th copy)", 1), "foo (11th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (10th copy).txt", 1), "foo (11th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (11th copy)", 1), "foo (12th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (11th copy).txt", 1), "foo (12th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (12th copy)", 1), "foo (13th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (12th copy).txt", 1), "foo (13th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (110th copy)", 1), "foo (111th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (110th copy).txt", 1), "foo (111th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (122nd copy)", 1), "foo (123rd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (122nd copy).txt", 1), "foo (123rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (123rd copy)", 1), "foo (124th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (123rd copy).txt", 1), "foo (124th copy).txt");

#endif /* GIO_CONVERSION_DONE */
	
	setlocale (LC_MESSAGES, "");
}

#endif
