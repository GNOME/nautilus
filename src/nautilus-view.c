/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-view.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundation
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ettore Perazzoli,
 *          John Sullivan <sullivan@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          Pavel Cisler <pavel@eazel.com>,
 *          David Emory Watson <dwatson@cs.ucr.edu>
 */

#include <config.h>

#include "nautilus-view.h"

#include "nautilus-application.h"
#include "nautilus-desktop-canvas-view.h"
#include "nautilus-error-reporting.h"
#include "nautilus-list-view.h"
#include "nautilus-mime-actions.h"
#include "nautilus-previewer.h"
#include "nautilus-properties-window.h"
#include "nautilus-window.h"
#include "nautilus-toolbar.h"

#if ENABLE_EMPTY_VIEW
#include "nautilus-empty-view.h"
#endif

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-clipboard-monitor.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>
#include <libnautilus-private/nautilus-desktop-directory.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-changes-queue.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-private.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-recent.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-file-undo-manager.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <gdesktop-enums.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_DIRECTORY_VIEW
#include <libnautilus-private/nautilus-debug.h>

/* Minimum starting update inverval */
#define UPDATE_INTERVAL_MIN 100
/* Maximum update interval */
#define UPDATE_INTERVAL_MAX 2000
/* Amount of miliseconds the update interval is increased */
#define UPDATE_INTERVAL_INC 250
/* Interval at which the update interval is increased */
#define UPDATE_INTERVAL_TIMEOUT_INTERVAL 250
/* Milliseconds that have to pass without a change to reset the update interval */
#define UPDATE_INTERVAL_RESET 1000

#define SILENT_WINDOW_OPEN_LIMIT 5

#define DUPLICATE_HORIZONTAL_ICON_OFFSET 70
#define DUPLICATE_VERTICAL_ICON_OFFSET   30

#define MAX_QUEUED_UPDATES 500

#define MAX_MENU_LEVELS 5
#define TEMPLATE_LIMIT 30

enum {
	ADD_FILE,
	BEGIN_FILE_CHANGES,
	BEGIN_LOADING,
	CLEAR,
	END_FILE_CHANGES,
	END_LOADING,
	FILE_CHANGED,
	MOVE_COPY_ITEMS,
	REMOVE_FILE,
	SELECTION_CHANGED,
	TRASH,
	DELETE,
	LAST_SIGNAL
};

enum {
	PROP_WINDOW_SLOT = 1,
	PROP_SUPPORTS_ZOOMING,
	NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GdkAtom copied_files_atom;

static char *scripts_directory_uri = NULL;
static int scripts_directory_uri_length;

struct NautilusViewDetails
{
	NautilusWindowSlot *slot;
	NautilusDirectory *model;
	NautilusFile *directory_as_file;
	NautilusFile *pathbar_popup_directory_as_file;
	GdkEventButton *pathbar_popup_event;
	guint dir_merge_id;

	gboolean supports_zooming;

	GList *scripts_directory_list;
	GList *templates_directory_list;
	
	guint display_selection_idle_id;
	guint update_context_menus_timeout_id;
	guint update_status_idle_id;
	guint reveal_selection_idle_id;

	guint display_pending_source_id;
	guint changes_timeout_id;

	guint update_interval;
 	guint64 last_queued;
	
	guint files_added_handler_id;
	guint files_changed_handler_id;
	guint load_error_handler_id;
	guint done_loading_handler_id;
	guint file_changed_handler_id;

	guint delayed_rename_file_id;

	GList *new_added_files;
	GList *new_changed_files;

	GHashTable *non_ready_files;

	GList *old_added_files;
	GList *old_changed_files;

	GList *pending_selection;

	/* whether we are in the active slot */
	gboolean active;

	/* loading indicates whether this view has begun loading a directory.
	 * This flag should need not be set inside subclasses. NautilusView automatically
	 * sets 'loading' to TRUE before it begins loading a directory's contents and to FALSE
	 * after it finishes loading the directory and its view.
	 */
	gboolean loading;
	gboolean templates_present;
	gboolean scripts_present;

	/* flag to indicate that no file updates should be dispatched to subclasses.
	 * This is a workaround for bug #87701 that prevents the list view from
	 * losing focus when the underlying GtkTreeView is updated.
	 */
	gboolean updates_frozen;
	guint	 updates_queued;
	gboolean needs_reload;

	gboolean is_renaming;

	gboolean sort_directories_first;

	gboolean show_foreign_files;
	gboolean show_hidden_files;
	gboolean ignore_hidden_file_preferences;

	gboolean batching_selection_level;
	gboolean selection_changed_while_batched;

	gboolean selection_was_removed;

	gboolean metadata_for_directory_as_file_pending;
	gboolean metadata_for_files_in_directory_pending;

	GList *subdirectory_list;

	GdkPoint context_menu_position;

	GMenu *selection_menu;
	GMenu *background_menu;
	GMenu *pathbar_menu;

	GActionGroup *view_action_group;
};

typedef struct {
	NautilusFile *file;
	NautilusDirectory *directory;
} FileAndDirectory;

/* forward declarations */

static gboolean display_selection_info_idle_callback           (gpointer              data);
static void     trash_or_delete_files                          (GtkWindow            *parent_window,
								const GList          *files,
								NautilusView      *view);
static void     load_directory                                 (NautilusView      *view,
								NautilusDirectory    *directory);
static void     clipboard_changed_callback                     (NautilusClipboardMonitor *monitor,
								NautilusView      *view);
static void     open_one_in_new_window                         (gpointer              data,
								gpointer              callback_data);
static void     schedule_update_context_menus                  (NautilusView      *view);
static void     remove_update_context_menus_timeout_callback   (NautilusView      *view);
static void     schedule_update_status                          (NautilusView      *view);
static void     remove_update_status_idle_callback             (NautilusView *view); 
static void     reset_update_interval                          (NautilusView      *view);
static void     schedule_idle_display_of_pending_files         (NautilusView      *view);
static void     unschedule_display_of_pending_files            (NautilusView      *view);
static void     disconnect_model_handlers                      (NautilusView      *view);
static void     metadata_for_directory_as_file_ready_callback  (NautilusFile         *file,
								gpointer              callback_data);
static void     metadata_for_files_in_directory_ready_callback (NautilusDirectory    *directory,
								GList                *files,
								gpointer              callback_data);
static void     nautilus_view_trash_state_changed_callback     (NautilusTrashMonitor *trash,
							        gboolean              state,
							        gpointer              callback_data);
static void     nautilus_view_select_file                      (NautilusView      *view,
							        NautilusFile         *file);

static void     update_templates_directory                     (NautilusView *view);

static void unschedule_pop_up_pathbar_context_menu (NautilusView *view);

G_DEFINE_TYPE (NautilusView, nautilus_view, GTK_TYPE_SCROLLED_WINDOW);

static char *
real_get_backing_uri (NautilusView *view)
{
	NautilusDirectory *directory;
	char *uri;
       
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	if (view->details->model == NULL) {
		return NULL;
	}
       
	directory = view->details->model;
       
	if (NAUTILUS_IS_DESKTOP_DIRECTORY (directory)) {
		directory = nautilus_desktop_directory_get_real_directory (NAUTILUS_DESKTOP_DIRECTORY (directory));
	} else {
		nautilus_directory_ref (directory);
	}
       
	uri = nautilus_directory_get_uri (directory);

	nautilus_directory_unref (directory);

	return uri;
}

/**
 *
 * nautilus_view_get_backing_uri:
 *
 * Returns the URI for the target location of new directory, new file, new
 * link and paste operations.
 */

char *
nautilus_view_get_backing_uri (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_backing_uri (view);
}

/**
 * nautilus_view_select_all:
 *
 * select all the items in the view
 * 
 **/
static void
nautilus_view_select_all (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->select_all (view);
}

static void
nautilus_view_select_first (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->select_first (view);
}

static void
nautilus_view_call_set_selection (NautilusView *view, GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->set_selection (view, selection);
}

static GList *
nautilus_view_get_selection_for_file_transfer (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selection_for_file_transfer (view);
}

static void
nautilus_view_invert_selection (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->invert_selection (view);
}

/**
 * nautilus_view_reveal_selection:
 *
 * Scroll as necessary to reveal the selected items.
 **/
static void
nautilus_view_reveal_selection (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->reveal_selection (view);
}

static gboolean
nautilus_view_using_manual_layout (NautilusView  *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return 	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->using_manual_layout (view);
}

/**
 * nautilus_view_can_rename_file
 *
 * Determine whether a file can be renamed.
 * @file: A NautilusFile
 * 
 * Return value: TRUE if @file can be renamed, FALSE otherwise.
 * 
 **/
static gboolean
nautilus_view_can_rename_file (NautilusView *view, NautilusFile *file)
{
	return 	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_rename_file (view, file);
}

static gboolean
nautilus_view_is_read_only (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return 	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->is_read_only (view);
}

static gboolean
showing_trash_directory (NautilusView *view)
{
	NautilusFile *file;

	file = nautilus_view_get_directory_as_file (view);
	if (file != NULL) {
		return nautilus_file_is_in_trash (file);
	}
	return FALSE;
}

static gboolean
showing_network_directory (NautilusView *view)
{
	NautilusFile *file;

	file = nautilus_view_get_directory_as_file (view);
	if (file != NULL) {
		return nautilus_file_is_in_network (file);
	}
	return FALSE;
}

static gboolean
showing_recent_directory (NautilusView *view)
{
	NautilusFile *file;

	file = nautilus_view_get_directory_as_file (view);
	if (file != NULL) {
		return nautilus_file_is_in_recent (file);
	}
	return FALSE;
}

static gboolean
nautilus_view_supports_creating_files (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return !nautilus_view_is_read_only (view)
		&& !showing_trash_directory (view)
		&& !showing_recent_directory (view);
}

static gboolean
nautilus_view_is_empty (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return 	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->is_empty (view);
}

/**
 * nautilus_view_bump_zoom_level:
 *
 * bump the current zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
nautilus_view_bump_zoom_level (NautilusView *view,
			       int zoom_increment)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	if (!nautilus_view_supports_zooming (view)) {
		return;
	}

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->bump_zoom_level (view, zoom_increment);
}

/**
 * nautilus_view_can_zoom_in:
 *
 * Determine whether the view can be zoomed any closer.
 * @view: The zoomable NautilusView.
 * 
 * Return value: TRUE if @view can be zoomed any closer, FALSE otherwise.
 * 
 **/
gboolean
nautilus_view_can_zoom_in (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	if (!nautilus_view_supports_zooming (view)) {
		return FALSE;
	}

	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_zoom_in (view);
}

/**
 * nautilus_view_can_zoom_out:
 *
 * Determine whether the view can be zoomed any further away.
 * @view: The zoomable NautilusView.
 * 
 * Return value: TRUE if @view can be zoomed any further away, FALSE otherwise.
 * 
 **/
gboolean
nautilus_view_can_zoom_out (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	if (!nautilus_view_supports_zooming (view)) {
		return FALSE;
	}

	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_zoom_out (view);
}

gboolean
nautilus_view_supports_zooming (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return view->details->supports_zooming;
}

/**
 * nautilus_view_restore_default_zoom_level:
 *
 * restore to the default zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
nautilus_view_restore_default_zoom_level (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	if (!nautilus_view_supports_zooming (view)) {
		return;
	}

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->restore_default_zoom_level (view);
}

const char *
nautilus_view_get_view_id (NautilusView *view)
{
	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_view_id (view);
}

char *
nautilus_view_get_first_visible_file (NautilusView *view)
{
	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_first_visible_file (view);
}

void
nautilus_view_scroll_to_file (NautilusView *view,
			      const char *uri)
{
	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->scroll_to_file (view, uri);
}

/**
 * nautilus_view_get_selection:
 *
 * Get a list of NautilusFile pointers that represents the
 * currently-selected items in this view. Subclasses must override
 * the signal handler for the 'get_selection' signal. Callers are
 * responsible for g_free-ing the list (but not its data).
 * @view: NautilusView whose selected items are of interest.
 * 
 * Return value: GList of NautilusFile pointers representing the selection.
 * 
 **/
GList *
nautilus_view_get_selection (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selection (view);
}

typedef struct {
	NautilusFile *file;
	NautilusView *directory_view;
} ScriptLaunchParameters;

typedef struct {
	NautilusFile *file;
	NautilusView *directory_view;
} CreateTemplateParameters;

static GList *
file_and_directory_list_to_files (GList *fad_list)
{
	GList *res, *l;
	FileAndDirectory *fad;

	res = NULL;
	for (l = fad_list; l != NULL; l = l->next) {
		fad = l->data;
		res = g_list_prepend (res, nautilus_file_ref (fad->file));
	}
	return g_list_reverse (res);
}


static GList *
file_and_directory_list_from_files (NautilusDirectory *directory, GList *files)
{
	GList *res, *l;
	FileAndDirectory *fad;

	res = NULL;
	for (l = files; l != NULL; l = l->next) {
		fad = g_new0 (FileAndDirectory, 1);
		fad->directory = nautilus_directory_ref (directory);
		fad->file = nautilus_file_ref (l->data);
		res = g_list_prepend (res, fad);
	}
	return g_list_reverse (res);
}

static void
file_and_directory_free (FileAndDirectory *fad)
{
	nautilus_directory_unref (fad->directory);
	nautilus_file_unref (fad->file);
	g_free (fad);
}


static void
file_and_directory_list_free (GList *list)
{
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		file_and_directory_free (l->data);
	}

	g_list_free (list);
}

static gboolean
file_and_directory_equal (gconstpointer  v1,
			  gconstpointer  v2)
{
	const FileAndDirectory *fad1, *fad2;
	fad1 = v1;
	fad2 = v2;

	return (fad1->file == fad2->file &&
		fad1->directory == fad2->directory);
}

static guint
file_and_directory_hash  (gconstpointer  v)
{
	const FileAndDirectory *fad;

	fad = v;
	return GPOINTER_TO_UINT (fad->file) ^ GPOINTER_TO_UINT (fad->directory);
}




static ScriptLaunchParameters *
script_launch_parameters_new (NautilusFile *file,
			      NautilusView *directory_view)
{
	ScriptLaunchParameters *result;

	result = g_new0 (ScriptLaunchParameters, 1);
	g_object_ref (directory_view);
	result->directory_view = directory_view;
	nautilus_file_ref (file);
	result->file = file;

	return result;
}

static void
script_launch_parameters_free (ScriptLaunchParameters *parameters)
{
	g_object_unref (parameters->directory_view);
	nautilus_file_unref (parameters->file);
	g_free (parameters);
}			      

static CreateTemplateParameters *
create_template_parameters_new (NautilusFile *file,
				NautilusView *directory_view)
{
	CreateTemplateParameters *result;

	result = g_new0 (CreateTemplateParameters, 1);
	g_object_ref (directory_view);
	result->directory_view = directory_view;
	nautilus_file_ref (file);
	result->file = file;

	return result;
}

static void
create_templates_parameters_free (CreateTemplateParameters *parameters)
{
	g_object_unref (parameters->directory_view);
	nautilus_file_unref (parameters->file);
	g_free (parameters);
}			      

NautilusWindow *
nautilus_view_get_window (NautilusView  *view)
{
	return nautilus_window_slot_get_window (view->details->slot);
}

NautilusWindowSlot *
nautilus_view_get_nautilus_window_slot (NautilusView  *view)
{
	g_assert (view->details->slot != NULL);

	return view->details->slot;
}

/* Returns the GtkWindow that this directory view occupies, or NULL
 * if at the moment this directory view is not in a GtkWindow or the
 * GtkWindow cannot be determined. Primarily used for parenting dialogs.
 */
static GtkWindow *
nautilus_view_get_containing_window (NautilusView *view)
{
	GtkWidget *window;

	g_assert (NAUTILUS_IS_VIEW (view));
	
	window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
	if (window == NULL) {
		return NULL;
	}

	return GTK_WINDOW (window);
}

static gboolean
nautilus_view_confirm_multiple (GtkWindow *parent_window,
				int count,
				gboolean tabs)
{
	GtkDialog *dialog;
	char *prompt;
	char *detail;
	int response;

	if (count <= SILENT_WINDOW_OPEN_LIMIT) {
		return TRUE;
	}

	prompt = _("Are you sure you want to open all files?");
	if (tabs) {
		detail = g_strdup_printf (ngettext("This will open %'d separate tab.",
						   "This will open %'d separate tabs.", count), count);
	} else {
		detail = g_strdup_printf (ngettext("This will open %'d separate window.",
						   "This will open %'d separate windows.", count), count);
	}
	dialog = eel_show_yes_no_dialog (prompt, detail, 
					 _("_OK"), _("_Cancel"),
					 parent_window);
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	return response == GTK_RESPONSE_YES;
}

static gboolean
selection_contains_one_item_in_menu_callback (NautilusView *view, GList *selection)
{
	if (g_list_length (selection) == 1) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
selection_not_empty_in_menu_callback (NautilusView *view, GList *selection)
{
	if (selection != NULL) {
		return TRUE;
	}

	return FALSE;
}

static char *
get_view_directory (NautilusView *view)
{
	char *uri, *path;
	GFile *f;
	
	uri = nautilus_directory_get_uri (view->details->model);
	if (eel_uri_is_desktop (uri)) {
		g_free (uri);
		uri = nautilus_get_desktop_directory_uri ();
		
	}
	f = g_file_new_for_uri (uri);
	path = g_file_get_path (f);
	g_object_unref (f);
	g_free (uri);
	
	return path;
}

void
nautilus_view_preview_files (NautilusView *view,
			     GList *files,
			     GArray *locations)
{
	gchar *uri;
	guint xid;
	GtkWidget *toplevel;

	uri = nautilus_file_get_uri (files->data);
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

	xid = gdk_x11_window_get_xid (gtk_widget_get_window (toplevel));
	nautilus_previewer_call_show_file (uri, xid, TRUE);

	g_free (uri);
}

void
nautilus_view_activate_selection (NautilusView *view)
{
	GList *selection;

	selection = nautilus_view_get_selection (view);
	nautilus_view_activate_files (view,
				      selection,
				      0,
				      TRUE);
	nautilus_file_list_free (selection);
}

void
nautilus_view_activate_files (NautilusView *view,
			      GList *files,
			      NautilusWindowOpenFlags flags,
			      gboolean confirm_multiple)
{
	char *path;

	path = get_view_directory (view);
	nautilus_mime_activate_files (nautilus_view_get_containing_window (view),
				      view->details->slot,
				      files,
				      path,
				      flags,
				      confirm_multiple);

	g_free (path);
}

static void
nautilus_view_activate_file (NautilusView *view,
			     NautilusFile *file,
			     NautilusWindowOpenFlags flags)
{
	char *path;

	path = get_view_directory (view);
	nautilus_mime_activate_file (nautilus_view_get_containing_window (view),
				     view->details->slot,
				     file,
				     path,
				     flags);

	g_free (path);
}

static void
action_open_with_default_application (GSimpleAction *action,
				      GVariant      *state,
				      gpointer       user_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (user_data);
	nautilus_view_activate_selection (view);
}
static void
action_open_file_and_close_window (GSimpleAction *action,
				   GVariant      *state,
				   gpointer       user_data)
{
	GList *selection;
	NautilusView *view;

	view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection (view);
	nautilus_view_activate_files (view,
				      selection,
				      NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND,
				      TRUE);
	nautilus_file_list_free (selection);
}

static void
action_open_item_location (GSimpleAction *action,
			   GVariant      *state,
			   gpointer       user_data)
{
	NautilusView *view;
	GList *selection;
	NautilusFile *item;
	GFile *activation_location;
	NautilusFile *activation_file;
	NautilusFile *location;

	view = NAUTILUS_VIEW (user_data);
	selection = nautilus_view_get_selection (view);

	if (!selection)
		return;

	item = NAUTILUS_FILE (selection->data);
	activation_location = nautilus_file_get_activation_location (item);
	activation_file = nautilus_file_get (activation_location);
	location = nautilus_file_get_parent (activation_file);

	nautilus_view_activate_file (view, location, 0);

	nautilus_file_unref (location);
	nautilus_file_unref (activation_file);
	g_object_unref (activation_location);
	nautilus_file_list_free (selection);
}

static void
action_open_item_new_tab (GSimpleAction *action,
			  GVariant      *state,
			  gpointer       user_data)
{
	NautilusView *view;
	GList *selection;
	GtkWindow *window;

	view = NAUTILUS_VIEW (user_data);
	selection = nautilus_view_get_selection (view);

	window = nautilus_view_get_containing_window (view);

	if (nautilus_view_confirm_multiple (window, g_list_length (selection), TRUE)) {
		nautilus_view_activate_files (view,
					      selection,
					      NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB,
					      FALSE);
	}

	nautilus_file_list_free (selection);
}

static void
action_pathbar_open_item_new_tab (GSimpleAction *action,
				  GVariant      *state,
				  gpointer       user_data)
{
	NautilusView *view;
	NautilusFile *file;

	view = NAUTILUS_VIEW (user_data);

	file = view->details->pathbar_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	nautilus_view_activate_file (view,
				     file,
				     NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB);
}

static void
app_chooser_dialog_response_cb (GtkDialog *dialog,
				gint response_id,
				gpointer user_data)
{
	GtkWindow *parent_window;
	NautilusFile *file;
	GAppInfo *info;
	GList files;

	parent_window = user_data;

	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		return;
	}

	info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));
	file = g_object_get_data (G_OBJECT (dialog), "directory-view:file");

	g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");

	files.next = NULL;
	files.prev = NULL;
	files.data = file;
	nautilus_launch_application (info, &files, parent_window);

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_object_unref (info);
}

static void
choose_program (NautilusView *view,
		NautilusFile *file)
{
	GtkWidget *dialog;
	GFile *location;
	GtkWindow *parent_window;

	g_assert (NAUTILUS_IS_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_file_ref (file);
	location = nautilus_file_get_location (file);
	parent_window = nautilus_view_get_containing_window (view);

	dialog = gtk_app_chooser_dialog_new (parent_window,
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
					     location);
	g_object_set_data_full (G_OBJECT (dialog), 
				"directory-view:file",
				g_object_ref (file),
				(GDestroyNotify)g_object_unref);
	gtk_widget_show (dialog);

	g_signal_connect_object (dialog, "response", 
				 G_CALLBACK (app_chooser_dialog_response_cb),
				 parent_window, 0);

	g_object_unref (location);
	nautilus_file_unref (file);	
}

static void
open_with_other_program (NautilusView *view)
{
        GList *selection;

	g_assert (NAUTILUS_IS_VIEW (view));

       	selection = nautilus_view_get_selection (view);

	if (selection_contains_one_item_in_menu_callback (view, selection)) {
		choose_program (view, NAUTILUS_FILE (selection->data));
	}

	nautilus_file_list_free (selection);
}

static void
action_open_with_other_application (GSimpleAction *action,
				    GVariant      *state,
				    gpointer       user_data)
{
	g_assert (NAUTILUS_IS_VIEW (user_data));

	open_with_other_program (NAUTILUS_VIEW (user_data));
}

static void
trash_or_delete_selected_files (NautilusView *view)
{
        GList *selection;

	/* This might be rapidly called multiple times for the same selection
	 * when using keybindings. So we remember if the current selection
	 * was already removed (but the view doesn't know about it yet).
	 */
	if (!view->details->selection_was_removed) {
		selection = nautilus_view_get_selection_for_file_transfer (view);
		trash_or_delete_files (nautilus_view_get_containing_window (view),
				       selection,
				       view);
		nautilus_file_list_free (selection);
		view->details->selection_was_removed = TRUE;
	}
}

static void
action_move_to_trash (GSimpleAction *action,
		      GVariant      *state,
		      gpointer       user_data)
{
        trash_or_delete_selected_files (NAUTILUS_VIEW (user_data));
}

static void
delete_selected_files (NautilusView *view)
{
        GList *selection;
	GList *node;
	GList *locations;

	selection = nautilus_view_get_selection_for_file_transfer (view);
	if (selection == NULL) {
		return;
	}

	locations = NULL;
	for (node = selection; node != NULL; node = node->next) {
		locations = g_list_prepend (locations,
					    nautilus_file_get_location ((NautilusFile *) node->data));
	}
	locations = g_list_reverse (locations);

	nautilus_file_operations_delete (locations, nautilus_view_get_containing_window (view), NULL, NULL);

	g_list_free_full (locations, g_object_unref);
        nautilus_file_list_free (selection);
}

static void
action_delete (GSimpleAction *action,
	       GVariant      *state,
	       gpointer       user_data)
{
	delete_selected_files (NAUTILUS_VIEW (user_data));
}

static void
action_restore_from_trash (GSimpleAction *action,
			   GVariant      *state,
			   gpointer       user_data)
{
	NautilusView *view;
	GList *selection;

	view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection_for_file_transfer (view);
	nautilus_restore_files_from_trash (selection,
					   nautilus_view_get_containing_window (view));

	nautilus_file_list_free (selection);

}

static void
action_select_all (GSimpleAction *action,
		   GVariant      *state,
		   gpointer       user_data)
{
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);

	nautilus_view_select_all (view);
}

static void
action_invert_selection (GSimpleAction *action,
			 GVariant      *state,
			 gpointer       user_data)
{
	g_assert (NAUTILUS_IS_VIEW (user_data));

	nautilus_view_invert_selection (user_data);
}

static void
pattern_select_response_cb (GtkWidget *dialog, int response, gpointer user_data)
{
	NautilusView *view;
	NautilusDirectory *directory;
	GtkWidget *entry;
	GList *selection;

	view = NAUTILUS_VIEW (user_data);

	switch (response) {
	case GTK_RESPONSE_OK :
		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		directory = nautilus_view_get_model (view);
		selection = nautilus_directory_match_pattern (directory,
							      gtk_entry_get_text (GTK_ENTRY (entry)));
			
		if (selection) {
			nautilus_view_call_set_selection (view, selection);
			nautilus_file_list_free (selection);

			nautilus_view_reveal_selection(view);
		}
		/* fall through */
	case GTK_RESPONSE_NONE :
	case GTK_RESPONSE_DELETE_EVENT :
	case GTK_RESPONSE_CANCEL :
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
select_pattern (NautilusView *view)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *example;
	GtkWidget *grid;
	GtkWidget *entry;
	char *example_pattern;

	dialog = gtk_dialog_new_with_buttons (_("Select Items Matching"),
					      nautilus_view_get_containing_window (view),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
					      _("_Cancel"),
					      GTK_RESPONSE_CANCEL,
					      _("_Select"),
					      GTK_RESPONSE_OK,
					      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

	label = gtk_label_new_with_mnemonic (_("_Pattern:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);

	example = gtk_label_new (NULL);
	gtk_widget_set_halign (example, GTK_ALIGN_START);
	example_pattern = g_strdup_printf ("%s<i>%s</i> ", 
					   _("Examples: "),
					   "*.png, file\?\?.txt, pict*.\?\?\?");
	gtk_label_set_markup (GTK_LABEL (example), example_pattern);
	g_free (example_pattern);

	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_set_hexpand (entry, TRUE);

	grid = gtk_grid_new ();
	g_object_set (grid,
		      "orientation", GTK_ORIENTATION_VERTICAL,
		      "border-width", 6,
		      "row-spacing", 6,
		      "column-spacing", 12,
		      NULL);

	gtk_container_add (GTK_CONTAINER (grid), label);
	gtk_grid_attach_next_to (GTK_GRID (grid), entry, label,
				 GTK_POS_RIGHT, 1, 1);
	gtk_grid_attach_next_to (GTK_GRID (grid), example, entry,
				 GTK_POS_BOTTOM, 1, 1);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show_all (grid);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (pattern_select_response_cb),
			  view);
	gtk_widget_show_all (dialog);
}

static void
action_select_pattern (GSimpleAction *action,
		       GVariant      *state,
		       gpointer       user_data)
{
	g_assert (NAUTILUS_IS_VIEW (user_data));

	select_pattern(user_data);
}

typedef struct {
	NautilusView *view;
	NautilusFile *new_file;
} RenameData;

static gboolean
delayed_rename_file_hack_callback (RenameData *data)
{
	NautilusView *view;
	NautilusFile *new_file;

	view = data->view;
	new_file = data->new_file;

	if (view->details->slot != NULL &&
	    view->details->active) {
		NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->start_renaming_file (view, new_file, FALSE);
		nautilus_view_reveal_selection (view);
	}

	return FALSE;
}

static void
delayed_rename_file_hack_removed (RenameData *data)
{
	g_object_unref (data->view);
	nautilus_file_unref (data->new_file);
	g_free (data);
}


static void
rename_file (NautilusView *view, NautilusFile *new_file)
{
	RenameData *data;

	/* HACK!!!!
	   This is a work around bug in listview. After the rename is
	   enabled we will get file changes due to info about the new
	   file being read, which will cause the model to change. When
	   the model changes GtkTreeView clears the editing. This hack just
	   delays editing for some time to try to avoid this problem.
	   A major problem is that the selection of the row causes us
	   to load the slow mimetype for the file, which leads to a
	   file_changed. So, before we delay we select the row.
	*/
	if (NAUTILUS_IS_LIST_VIEW (view)) {
		nautilus_view_select_file (view, new_file);
		
		data = g_new (RenameData, 1);
		data->view = g_object_ref (view);
		data->new_file = nautilus_file_ref (new_file);
		if (view->details->delayed_rename_file_id != 0) {
			g_source_remove (view->details->delayed_rename_file_id);
		}
		view->details->delayed_rename_file_id = 
			g_timeout_add_full (G_PRIORITY_DEFAULT,
					    100, (GSourceFunc)delayed_rename_file_hack_callback,
					    data, (GDestroyNotify) delayed_rename_file_hack_removed);
		
		return;
	}

	/* no need to select because start_renaming_file selects
	 * nautilus_view_select_file (view, new_file);
	 */
	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->start_renaming_file (view, new_file, FALSE);
	nautilus_view_reveal_selection (view);
}

static void
reveal_newly_added_folder (NautilusView *view, NautilusFile *new_file,
			   NautilusDirectory *directory, GFile *target_location)
{
	GFile *location;

	location = nautilus_file_get_location (new_file);
	if (g_file_equal (location, target_location)) {
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (reveal_newly_added_folder),
						      (void *) target_location);
		rename_file (view, new_file);
	}
	g_object_unref (location);
}

typedef struct {
	NautilusView *directory_view;
	GHashTable *added_locations;
	GList *selection;
} NewFolderData;

typedef struct {
	NautilusView *directory_view;
	GHashTable *to_remove_locations;
	NautilusFile *new_folder;
} NewFolderSelectionData;

static void
rename_newly_added_folder (NautilusView *view, NautilusFile *removed_file,
			   NautilusDirectory *directory, NewFolderSelectionData *data);

static void
rename_newly_added_folder (NautilusView *view, NautilusFile *removed_file,
			   NautilusDirectory *directory, NewFolderSelectionData *data)
{
	GFile *location;

	location = nautilus_file_get_location (removed_file);
	if (!g_hash_table_remove (data->to_remove_locations, location)) {
		g_assert_not_reached ();
	}
	g_object_unref (location);
	if (g_hash_table_size (data->to_remove_locations) == 0) {
		nautilus_view_set_selection (data->directory_view, NULL);
		g_signal_handlers_disconnect_by_func (data->directory_view,
						      G_CALLBACK (rename_newly_added_folder),
						      (void *) data);

		rename_file (data->directory_view, data->new_folder);
		g_object_unref (data->new_folder);
		g_hash_table_destroy (data->to_remove_locations);
		g_free (data);
	}
}

static void
track_newly_added_locations (NautilusView *view, NautilusFile *new_file,
			     NautilusDirectory *directory, gpointer user_data)
{
	NewFolderData *data;

	data = user_data;

	g_hash_table_insert (data->added_locations, nautilus_file_get_location (new_file), NULL);
}

static void
new_folder_done (GFile *new_folder, 
		 gboolean success,
		 gpointer user_data)
{
	NautilusView *directory_view;
	NautilusFile *file;
	char screen_string[32];
	GdkScreen *screen;
	NewFolderData *data;

	data = (NewFolderData *)user_data;

	directory_view = data->directory_view;

	if (directory_view == NULL) {
		goto fail;
	}

	g_signal_handlers_disconnect_by_func (directory_view,
					      G_CALLBACK (track_newly_added_locations),
					      (void *) data);

	if (new_folder == NULL) {
		goto fail;
	}
	
	screen = gtk_widget_get_screen (GTK_WIDGET (directory_view));
	g_snprintf (screen_string, sizeof (screen_string), "%d", gdk_screen_get_number (screen));

	
	file = nautilus_file_get (new_folder);
	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_SCREEN,
		 NULL,
		 screen_string);

	if (data->selection != NULL) {
		NewFolderSelectionData *sdata;
		GList *uris, *l;
		char *target_uri;

		sdata = g_new (NewFolderSelectionData, 1);
		sdata->directory_view = directory_view;
		sdata->to_remove_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
								    g_object_unref, NULL);
		sdata->new_folder = g_object_ref (file);

		uris = NULL;
		for (l = data->selection; l != NULL; l = l->next) {
			GFile *old_location;
			GFile *new_location;
			char *basename;

			uris = g_list_prepend (uris, nautilus_file_get_uri ((NautilusFile *) l->data));

			old_location = nautilus_file_get_location (l->data);
			basename = g_file_get_basename (old_location);
			new_location = g_file_resolve_relative_path (new_folder, basename);
			g_hash_table_insert (sdata->to_remove_locations, new_location, NULL);
			g_free (basename);
			g_object_unref (old_location);
		}
		uris = g_list_reverse (uris);

		target_uri = nautilus_file_get_uri (file);

		g_signal_connect_data (directory_view,
				       "remove-file",
				       G_CALLBACK (rename_newly_added_folder),
				       sdata,
				       (GClosureNotify)NULL,
				       G_CONNECT_AFTER);

		nautilus_view_move_copy_items (directory_view,
					       uris,
					       NULL,
					       target_uri,
					       GDK_ACTION_MOVE,
					       0, 0);
		g_list_free_full (uris, g_free);
		g_free (target_uri);
	} else {
		if (g_hash_table_lookup_extended (data->added_locations, new_folder, NULL, NULL)) {
			/* The file was already added */
			rename_file (directory_view, file);
		} else {
			/* We need to run after the default handler adds the folder we want to
			 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
			 * must use connect_after.
			 */
			g_signal_connect_data (directory_view,
					       "add-file",
					       G_CALLBACK (reveal_newly_added_folder),
					       g_object_ref (new_folder),
					       (GClosureNotify)g_object_unref,
					       G_CONNECT_AFTER);
		}
	}

	nautilus_file_unref (file);

 fail:
	g_hash_table_destroy (data->added_locations);

	if (data->directory_view != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
					      (gpointer *) &data->directory_view);
	}

        nautilus_file_list_free (data->selection);
	g_free (data);
}


static NewFolderData *
new_folder_data_new (NautilusView *directory_view,
		     gboolean      with_selection)
{
	NewFolderData *data;

	data = g_new (NewFolderData, 1);
	data->directory_view = directory_view;
	data->added_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
						       g_object_unref, NULL);
	if (with_selection) {
		data->selection = nautilus_view_get_selection_for_file_transfer (directory_view);
	} else {
		data->selection = NULL;
	}
	g_object_add_weak_pointer (G_OBJECT (data->directory_view),
				   (gpointer *) &data->directory_view);

	return data;
}

static GdkPoint *
context_menu_to_file_operation_position (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	if (nautilus_view_using_manual_layout (view)
	    && view->details->context_menu_position.x >= 0
	    && view->details->context_menu_position.y >= 0) {
		NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->widget_to_file_operation_position
			(view, &view->details->context_menu_position);
		return &view->details->context_menu_position;
	} else {
		return NULL;
	}
}

static void
nautilus_view_new_folder (NautilusView *directory_view,
			  gboolean      with_selection)
{
	char *parent_uri;
	NewFolderData *data;
	GdkPoint *pos;

	data = new_folder_data_new (directory_view, with_selection);

	g_signal_connect_data (directory_view,
			       "add-file",
			       G_CALLBACK (track_newly_added_locations),
			       data,
			       (GClosureNotify)NULL,
			       G_CONNECT_AFTER);

	pos = context_menu_to_file_operation_position (directory_view);

	parent_uri = nautilus_view_get_backing_uri (directory_view);
	nautilus_file_operations_new_folder (GTK_WIDGET (directory_view),
					     pos, parent_uri,
					     new_folder_done, data);

	g_free (parent_uri);
}

static NewFolderData *
setup_new_folder_data (NautilusView *directory_view)
{
	NewFolderData *data;

	data = new_folder_data_new (directory_view, FALSE);

	g_signal_connect_data (directory_view,
			       "add-file",
			       G_CALLBACK (track_newly_added_locations),
			       data,
			       (GClosureNotify)NULL,
			       G_CONNECT_AFTER);

	return data;
}

void
nautilus_view_new_file_with_initial_contents (NautilusView *view,
					      const char *parent_uri,
					      const char *filename,
					      const char *initial_contents,
					      int length,
					      GdkPoint *pos)
{
	NewFolderData *data;

	g_assert (parent_uri != NULL);

	data = setup_new_folder_data (view);

	if (pos == NULL) {
		pos = context_menu_to_file_operation_position (view);
	}

	nautilus_file_operations_new_file (GTK_WIDGET (view),
					   pos, parent_uri, filename,
					   initial_contents, length,
					   new_folder_done, data);
}

static void
nautilus_view_new_file (NautilusView *directory_view,
			const char *parent_uri,
			NautilusFile *source)
{
	GdkPoint *pos;
	NewFolderData *data;
	char *source_uri;
	char *container_uri;

	container_uri = NULL;
	if (parent_uri == NULL) {
		container_uri = nautilus_view_get_backing_uri (directory_view);
		g_assert (container_uri != NULL);
	}

	if (source == NULL) {
		nautilus_view_new_file_with_initial_contents (directory_view,
							      parent_uri != NULL ? parent_uri : container_uri,
							      NULL,
							      NULL,
							      0,
							      NULL);
		g_free (container_uri);
		return;
	}

	g_return_if_fail (nautilus_file_is_local (source));

	pos = context_menu_to_file_operation_position (directory_view);

	data = setup_new_folder_data (directory_view);

	source_uri = nautilus_file_get_uri (source);

	nautilus_file_operations_new_file_from_template (GTK_WIDGET (directory_view),
							 pos,
							 parent_uri != NULL ? parent_uri : container_uri,
							 NULL,
							 source_uri,
							 new_folder_done, data);

	g_free (source_uri);
	g_free (container_uri);
}

static void
action_new_folder (GSimpleAction *action,
		   GVariant      *state,
		   gpointer       user_data)
{
	g_assert (NAUTILUS_IS_VIEW (user_data));

	nautilus_view_new_folder (NAUTILUS_VIEW (user_data), FALSE);
}

static void
action_new_folder_with_selection (GSimpleAction *action,
				  GVariant      *state,
				  gpointer       user_data)
{                
        g_assert (NAUTILUS_IS_VIEW (user_data));

	nautilus_view_new_folder (NAUTILUS_VIEW (user_data), TRUE);
}

static void
action_properties (GSimpleAction *action,
		   GVariant      *state,
		   gpointer       user_data)
{
        NautilusView *view;
        GList *selection;
	GList *files;
        
        g_assert (NAUTILUS_IS_VIEW (user_data));

        view = NAUTILUS_VIEW (user_data);
	selection = nautilus_view_get_selection (view);
	if (g_list_length (selection) == 0) {
		if (view->details->directory_as_file != NULL) {
			files = g_list_append (NULL, nautilus_file_ref (view->details->directory_as_file));

			nautilus_properties_window_present (files, GTK_WIDGET (view), NULL);

			nautilus_file_list_free (files);
		}
	} else {
		nautilus_properties_window_present (selection, GTK_WIDGET (view), NULL);
	}
        nautilus_file_list_free (selection);
}

static void
action_pathbar_properties (GSimpleAction *action,
			   GVariant      *state,
			   gpointer       user_data)
{
	NautilusView *view;
	GList           *files;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);
	g_assert (NAUTILUS_IS_FILE (view->details->pathbar_popup_directory_as_file));

	files = g_list_append (NULL, nautilus_file_ref (view->details->pathbar_popup_directory_as_file));

	nautilus_properties_window_present (files, GTK_WIDGET (view), NULL);

	nautilus_file_list_free (files);
}

static void
nautilus_view_set_show_hidden_files (NautilusView *view,
				     gboolean show_hidden)
{
	if (view->details->ignore_hidden_file_preferences) {
		return;
	}

	if (show_hidden != view->details->show_hidden_files) {
		view->details->show_hidden_files = show_hidden;
		if (view->details->model != NULL) {
			load_directory (view, view->details->model);
		}
	}
}

static void
action_show_hidden_files (GSimpleAction *action,
			  GVariant      *state,
			  gpointer       user_data)
{
	gboolean show_hidden;
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);
	show_hidden = g_variant_get_boolean (state);

	nautilus_view_set_show_hidden_files (view, show_hidden);

	g_simple_action_set_state (action, state);
}

static void
action_undo (GSimpleAction *action,
	     GVariant      *state,
	     gpointer       user_data)
{
	GtkWidget *toplevel;
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
	nautilus_file_undo_manager_undo (GTK_WINDOW (toplevel));
}

static void
action_redo (GSimpleAction *action,
	     GVariant      *state,
	     gpointer       user_data)
{
	GtkWidget *toplevel;
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
	nautilus_file_undo_manager_redo (GTK_WINDOW (toplevel));
}

static void
action_zoom_in (GSimpleAction *action,
		GVariant      *state,
		gpointer       user_data)
{
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);

	nautilus_view_bump_zoom_level (view, 1);
}

static void
action_zoom_out (GSimpleAction *action,
		 GVariant      *state,
		 gpointer       user_data)
{
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);

	nautilus_view_bump_zoom_level (view, -1);
}

static void
action_zoom_default (GSimpleAction *action,
		     GVariant      *state,
		     gpointer       user_data)
{
	nautilus_view_restore_default_zoom_level (user_data);
}

static void
action_open_item_new_window (GSimpleAction *action,
			     GVariant      *state,
			     gpointer       user_data)
{
	NautilusView *view;
	GtkWindow *window;
	GList *selection;

	view = NAUTILUS_VIEW (user_data);
	selection = nautilus_view_get_selection (view);
	window = GTK_WINDOW (nautilus_view_get_containing_window (view));

	if (nautilus_view_confirm_multiple (window, g_list_length (selection), TRUE)) {
		g_list_foreach (selection, open_one_in_new_window, view);
	}

	nautilus_file_list_free (selection);
}

static void
action_pathbar_open_item_new_window (GSimpleAction *action,
				     GVariant      *state,
				     gpointer       user_data)
{
	NautilusView *view;
	NautilusFile *file;

	view = NAUTILUS_VIEW (user_data);

	file = view->details->pathbar_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	nautilus_view_activate_file (view,
				     file,
				     NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
paste_clipboard_data (NautilusView *view,
		      GtkSelectionData *selection_data,
		      char *destination_uri)
{
	gboolean cut;
	GList *item_uris;

	cut = FALSE;
	item_uris = nautilus_clipboard_get_uri_list_from_selection_data (selection_data, &cut,
									 copied_files_atom);

	if (item_uris != NULL && destination_uri != NULL) {
		nautilus_view_move_copy_items (view, item_uris, NULL, destination_uri,
					       cut ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
					       0, 0);

		/* If items are cut then remove from clipboard */
		if (cut) {
			gtk_clipboard_clear (nautilus_clipboard_get (GTK_WIDGET (view)));
		}

		g_list_free_full (item_uris, g_free);
	}
}

static void
paste_clipboard_received_callback (GtkClipboard     *clipboard,
				   GtkSelectionData *selection_data,
				   gpointer          data)
{
	NautilusView *view;
	char *view_uri;

	view = NAUTILUS_VIEW (data);

	view_uri = nautilus_view_get_backing_uri (view);

	if (view->details->slot != NULL) {
		paste_clipboard_data (view, selection_data, view_uri);
	}

	g_free (view_uri);

	g_object_unref (view);
}

static void
action_paste_files (GSimpleAction *action,
		    GVariant      *state,
		    gpointer       user_data)
{
	NautilusView *view;

	g_assert (NAUTILUS_IS_VIEW (user_data));

	view = NAUTILUS_VIEW (user_data);

	g_object_ref (view);
	gtk_clipboard_request_contents (nautilus_clipboard_get (GTK_WIDGET (view)),
					copied_files_atom,
					paste_clipboard_received_callback,
					view);
}

static void
click_policy_changed_callback (gpointer callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->click_policy_changed (view);
}

gboolean
nautilus_view_should_sort_directories_first (NautilusView *view)
{
	return view->details->sort_directories_first;
}

static void
sort_directories_first_changed_callback (gpointer callback_data)
{
	NautilusView *view;
	gboolean preference_value;

	view = NAUTILUS_VIEW (callback_data);

	preference_value =
		g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);

	if (preference_value != view->details->sort_directories_first) {
		view->details->sort_directories_first = preference_value;
		NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->sort_directories_first_changed (view);
	}
}

static void
show_hidden_files_changed_callback (gpointer callback_data)
{
	NautilusView *view;
	gboolean preference_value;

	view = NAUTILUS_VIEW (callback_data);

	preference_value =
		g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

	nautilus_view_set_show_hidden_files (view, preference_value);
}

static gboolean
set_up_scripts_directory_global (void)
{
	char *old_scripts_directory_path;
	char *scripts_directory_path;
	const char *override;

	if (scripts_directory_uri != NULL) {
		return TRUE;
	}

	scripts_directory_path = nautilus_get_scripts_directory_path ();

	override = g_getenv ("GNOME22_USER_DIR");

	if (override) {
		old_scripts_directory_path = g_build_filename (override,
							       "nautilus-scripts",
							       NULL);
	} else {
		old_scripts_directory_path = g_build_filename (g_get_home_dir (),
							       ".gnome2",
							       "nautilus-scripts",
							       NULL);
	}

	if (g_file_test (old_scripts_directory_path, G_FILE_TEST_IS_DIR)
	    && !g_file_test (scripts_directory_path, G_FILE_TEST_EXISTS)) {
		char *updated;
		const char *message;

		/* test if we already attempted to migrate first */
		updated = g_build_filename (old_scripts_directory_path, "DEPRECATED-DIRECTORY", NULL);
		message = _("Nautilus 3.6 deprecated this directory and tried migrating "
			    "this configuration to ~/.local/share/nautilus");
		if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
			char *parent_dir;

			parent_dir = g_path_get_dirname (scripts_directory_path);
			if (g_mkdir_with_parents (parent_dir, 0700) == 0) {
				int fd, res;

				/* rename() works fine if the destination directory is
				 * empty.
				 */
				res = g_rename (old_scripts_directory_path, scripts_directory_path);
				if (res == -1) {
					fd = g_creat (updated, 0600);
					if (fd != -1) {
						res = write (fd, message, strlen (message));
						close (fd);
					}
				}
			}
			g_free (parent_dir);
		}

		g_free (updated);
	}

	if (g_mkdir_with_parents (scripts_directory_path, 0700) == 0) {
		scripts_directory_uri = g_filename_to_uri (scripts_directory_path, NULL, NULL);
		scripts_directory_uri_length = strlen (scripts_directory_uri);
	}

	g_free (scripts_directory_path);
	g_free (old_scripts_directory_path);

	return (scripts_directory_uri != NULL) ? TRUE : FALSE;
}

static void
scripts_added_or_changed_callback (NautilusDirectory *directory,
				   GList *files,
				   gpointer callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	if (view->details->active) {
		schedule_update_context_menus (view);
	}
}

static void
templates_added_or_changed_callback (NautilusDirectory *directory,
				     GList *files,
				     gpointer callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	if (view->details->active) {
		schedule_update_context_menus (view);
	}
}

static void
add_directory_to_directory_list (NautilusView *view,
				 NautilusDirectory *directory,
				 GList **directory_list,
				 GCallback changed_callback)
{
	NautilusFileAttributes attributes;

	if (g_list_find (*directory_list, directory) == NULL) {
		nautilus_directory_ref (directory);

		attributes =
			NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
			NAUTILUS_FILE_ATTRIBUTE_INFO |
			NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

		nautilus_directory_file_monitor_add (directory, directory_list,
						     FALSE, attributes,
						     (NautilusDirectoryCallback)changed_callback, view);

		g_signal_connect_object (directory, "files-added",
					 G_CALLBACK (changed_callback), view, 0);
		g_signal_connect_object (directory, "files-changed",
					 G_CALLBACK (changed_callback), view, 0);

		*directory_list = g_list_append	(*directory_list, directory);
	}
}

static void
remove_directory_from_directory_list (NautilusView *view,
				      NautilusDirectory *directory,
				      GList **directory_list,
				      GCallback changed_callback)
{
	*directory_list = g_list_remove	(*directory_list, directory);

	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (changed_callback),
					      view);

	nautilus_directory_file_monitor_remove (directory, directory_list);

	nautilus_directory_unref (directory);
}


static void
add_directory_to_scripts_directory_list (NautilusView *view,
					 NautilusDirectory *directory)
{
	add_directory_to_directory_list (view, directory,
					 &view->details->scripts_directory_list,
					 G_CALLBACK (scripts_added_or_changed_callback));
}

static void
remove_directory_from_scripts_directory_list (NautilusView *view,
					      NautilusDirectory *directory)
{
	remove_directory_from_directory_list (view, directory,
					      &view->details->scripts_directory_list,
					      G_CALLBACK (scripts_added_or_changed_callback));
}

static void
add_directory_to_templates_directory_list (NautilusView *view,
					   NautilusDirectory *directory)
{
	add_directory_to_directory_list (view, directory,
					 &view->details->templates_directory_list,
					 G_CALLBACK (templates_added_or_changed_callback));
}

static void
remove_directory_from_templates_directory_list (NautilusView *view,
						NautilusDirectory *directory)
{
	remove_directory_from_directory_list (view, directory,
					      &view->details->templates_directory_list,
					      G_CALLBACK (templates_added_or_changed_callback));
}

static void
slot_active (NautilusWindowSlot *slot,
	     NautilusView *view)
{
	if (view->details->active) {
		return;
	}

	view->details->active = TRUE;

	/* Avoid updating the toolbar withouth making sure the toolbar
	 * zoom slider has the correct adjustment that changes when the
	 * view mode changes
	 */
	nautilus_window_slot_sync_view_mode (slot);
	nautilus_view_update_context_menus(view);
	nautilus_view_update_toolbar_menus (view);

	schedule_update_context_menus (view);
}

static void
slot_inactive (NautilusWindowSlot *slot,
	       NautilusView *view)
{
	if (!view->details->active) {
		return;
	}

	view->details->active = FALSE;

	remove_update_context_menus_timeout_callback (view);
}

void
nautilus_view_grab_focus (NautilusView *view)
{
	/* focus the child of the scrolled window if it exists */
	GtkWidget *child;
	child = gtk_bin_get_child (GTK_BIN (view));
	if (child) {
		gtk_widget_grab_focus (GTK_WIDGET (child));
	}
}

int
nautilus_view_get_selection_count (NautilusView *view)
{
	/* FIXME: This could be faster if we special cased it in subclasses */
	GList *files;
	int len;

	files = nautilus_view_get_selection (NAUTILUS_VIEW (view));
	len = g_list_length (files);
	nautilus_file_list_free (files);
	
	return len;
}

static void
undo_manager_changed (NautilusFileUndoManager* manager,
		      NautilusView *view)
{
	if (!view->details->active) {
		return;
	}

	nautilus_view_update_toolbar_menus (view);
}

void
nautilus_view_set_selection (NautilusView *nautilus_view,
			     GList *selection)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (nautilus_view);

	if (!view->details->loading) {
		/* If we aren't still loading, set the selection right now,
		 * and reveal the new selection.
		 */
		nautilus_view_call_set_selection (view, selection);
		nautilus_view_reveal_selection (view);
	} else {
		/* If we are still loading, set the list of pending URIs instead.
		 * done_loading() will eventually select the pending URIs and reveal them.
		 */
		g_list_free_full (view->details->pending_selection, g_object_unref);
		view->details->pending_selection =
			g_list_copy_deep (selection, (GCopyFunc) g_object_ref, NULL);
	}
}

static char *
get_bulk_rename_tool ()
{
	char *bulk_rename_tool;
	g_settings_get (nautilus_preferences, NAUTILUS_PREFERENCES_BULK_RENAME_TOOL, "^ay", &bulk_rename_tool);
	return g_strstrip (bulk_rename_tool);
}

static gboolean
have_bulk_rename_tool ()
{
	char *bulk_rename_tool;
	gboolean have_tool;

	bulk_rename_tool = get_bulk_rename_tool ();
	have_tool = ((bulk_rename_tool != NULL) && (*bulk_rename_tool != '\0'));
	g_free (bulk_rename_tool);
	return have_tool;
}

static void
nautilus_view_destroy (GtkWidget *object)
{
	NautilusView *view;
	GList *node, *next;

	view = NAUTILUS_VIEW (object);

	disconnect_model_handlers (view);

	nautilus_view_stop_loading (view);

	for (node = view->details->scripts_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_scripts_directory_list (view, node->data);
	}

	for (node = view->details->templates_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_templates_directory_list (view, node->data);
	}

	while (view->details->subdirectory_list != NULL) {
		nautilus_view_remove_subdirectory (view,
						   view->details->subdirectory_list->data);
	}

	remove_update_context_menus_timeout_callback (view);
	remove_update_status_idle_callback (view);

	if (view->details->display_selection_idle_id != 0) {
		g_source_remove (view->details->display_selection_idle_id);
		view->details->display_selection_idle_id = 0;
	}

	if (view->details->reveal_selection_idle_id != 0) {
		g_source_remove (view->details->reveal_selection_idle_id);
		view->details->reveal_selection_idle_id = 0;
	}

	if (view->details->delayed_rename_file_id != 0) {
		g_source_remove (view->details->delayed_rename_file_id);
		view->details->delayed_rename_file_id = 0;
	}

	if (view->details->model) {
		nautilus_directory_unref (view->details->model);
		view->details->model = NULL;
	}
	
	if (view->details->directory_as_file) {
		nautilus_file_unref (view->details->directory_as_file);
		view->details->directory_as_file = NULL;
	}

	/* We don't own the slot, so no unref */
	view->details->slot = NULL;

	GTK_WIDGET_CLASS (nautilus_view_parent_class)->destroy (object);
}

static void
nautilus_view_finalize (GObject *object)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (object);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      schedule_update_context_menus, view);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      click_policy_changed_callback, view);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      sort_directories_first_changed_callback, view);
	g_signal_handlers_disconnect_by_func (gtk_filechooser_preferences,
					      show_hidden_files_changed_callback, view);
	g_signal_handlers_disconnect_by_func (nautilus_window_state,
					      nautilus_view_display_selection_info, view);

	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      schedule_update_context_menus, view);

	unschedule_pop_up_pathbar_context_menu (view);
	if (view->details->pathbar_popup_event != NULL) {
		gdk_event_free ((GdkEvent *) view->details->pathbar_popup_event);
	}

	g_hash_table_destroy (view->details->non_ready_files);

	G_OBJECT_CLASS (nautilus_view_parent_class)->finalize (object);
}

/**
 * nautilus_view_display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: NautilusView for which to display selection info.
 *
 **/
void
nautilus_view_display_selection_info (NautilusView *view)
{
	GList *selection;
	goffset non_folder_size;
	gboolean non_folder_size_known;
	guint non_folder_count, folder_count, folder_item_count;
	gboolean folder_item_count_known;
	guint file_item_count;
	GList *p;
	char *first_item_name;
	char *non_folder_count_str;
	char *non_folder_item_count_str;
	char *folder_count_str;
	char *folder_item_count_str;
	char *primary_status;
	char *detail_status;
	NautilusFile *file;

	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	selection = nautilus_view_get_selection (view);
	
	folder_item_count_known = TRUE;
	folder_count = 0;
	folder_item_count = 0;
	non_folder_count = 0;
	non_folder_size_known = FALSE;
	non_folder_size = 0;
	first_item_name = NULL;
	folder_count_str = NULL;
	folder_item_count_str = NULL;
	non_folder_count_str = NULL;
	non_folder_item_count_str = NULL;
	
	for (p = selection; p != NULL; p = p->next) {
		file = p->data;
		if (nautilus_file_is_directory (file)) {
			folder_count++;
			if (nautilus_file_get_directory_item_count (file, &file_item_count, NULL)) {
				folder_item_count += file_item_count;
			} else {
				folder_item_count_known = FALSE;
			}
		} else {
			non_folder_count++;
			if (!nautilus_file_can_get_size (file)) {
				non_folder_size_known = TRUE;
				non_folder_size += nautilus_file_get_size (file);
			}
		}

		if (first_item_name == NULL) {
			first_item_name = nautilus_file_get_display_name (file);
		}
	}
	
	nautilus_file_list_free (selection);
	
	/* Break out cases for localization's sake. But note that there are still pieces
	 * being assembled in a particular order, which may be a problem for some localizers.
	 */

	if (folder_count != 0) {
		if (folder_count == 1 && non_folder_count == 0) {
			folder_count_str = g_strdup_printf (_("“%s” selected"), first_item_name);
		} else {
			folder_count_str = g_strdup_printf (ngettext("%'d folder selected", 
								     "%'d folders selected", 
								     folder_count), 
							    folder_count);
		}

		if (folder_count == 1) {
			if (!folder_item_count_known) {
				folder_item_count_str = g_strdup ("");
			} else {
				folder_item_count_str = g_strdup_printf (ngettext("(containing %'d item)",
										  "(containing %'d items)",
										  folder_item_count), 
									 folder_item_count);
			}
		}
		else {
			if (!folder_item_count_known) {
				folder_item_count_str = g_strdup ("");
			} else {
				/* translators: this is preceded with a string of form 'N folders' (N more than 1) */
				folder_item_count_str = g_strdup_printf (ngettext("(containing a total of %'d item)",
										  "(containing a total of %'d items)",
										  folder_item_count), 
									 folder_item_count);
			}
			
		}
	}

	if (non_folder_count != 0) {
		if (folder_count == 0) {
			if (non_folder_count == 1) {
				non_folder_count_str = g_strdup_printf (_("“%s” selected"),
									first_item_name);
			} else {
				non_folder_count_str = g_strdup_printf (ngettext("%'d item selected",
										 "%'d items selected",
										 non_folder_count),
									non_folder_count);
			}
		} else {
			/* Folders selected also, use "other" terminology */
			non_folder_count_str = g_strdup_printf (ngettext("%'d other item selected",
									 "%'d other items selected",
									 non_folder_count),
								non_folder_count);
		}

		if (non_folder_size_known) {
			char *size_string;

			size_string = g_format_size (non_folder_size);
			/* This is marked for translation in case a localiser
			 * needs to use something other than parentheses. The
			 * the message in parentheses is the size of the selected items.
			 */
			non_folder_item_count_str = g_strdup_printf (_("(%s)"), size_string);
			g_free (size_string);
		} else {
			non_folder_item_count_str = g_strdup ("");
		}
	}

	if (folder_count == 0 && non_folder_count == 0)	{
		primary_status = NULL;
		detail_status = NULL;
	} else if (folder_count == 0) {
		primary_status = g_strdup (non_folder_count_str);
		detail_status = g_strdup (non_folder_item_count_str);
	} else if (non_folder_count == 0) {
		primary_status = g_strdup (folder_count_str);
		detail_status  = g_strdup (folder_item_count_str);
	} else {
		/* This is marked for translation in case a localizer
		 * needs to change ", " to something else. The comma
		 * is between the message about the number of folders
		 * and the number of items in those folders and the
		 * message about the number of other items and the
		 * total size of those items.
		 */
		primary_status = g_strdup_printf (_("%s %s, %s %s"),
						  folder_count_str,
						  folder_item_count_str,
						  non_folder_count_str,
						  non_folder_item_count_str);
		detail_status = NULL;
	}

	g_free (first_item_name);
	g_free (folder_count_str);
	g_free (folder_item_count_str);
	g_free (non_folder_count_str);
	g_free (non_folder_item_count_str);

	nautilus_window_slot_set_status (view->details->slot,
					 primary_status, detail_status);

	g_free (primary_status);
	g_free (detail_status);
}

static void
nautilus_view_send_selection_change (NautilusView *view)
{
	g_signal_emit (view, signals[SELECTION_CHANGED], 0);
}

void
nautilus_view_load_location (NautilusView *nautilus_view,
			     GFile        *location)
{
	NautilusDirectory *directory;
	NautilusView *directory_view;

	directory_view = NAUTILUS_VIEW (nautilus_view);
	nautilus_profile_start (NULL);
	directory = nautilus_directory_get (location);
	load_directory (directory_view, directory);
	nautilus_directory_unref (directory);
	nautilus_profile_end (NULL);
}

static gboolean
reveal_selection_idle_callback (gpointer data)
{
	NautilusView *view;
	
	view = NAUTILUS_VIEW (data);

	view->details->reveal_selection_idle_id = 0;
	nautilus_view_reveal_selection (view);

	return FALSE;
}

static void
done_loading (NautilusView *view,
	      gboolean all_files_seen)
{
	GList *selection;
	gboolean do_reveal = FALSE;
	NautilusWindow *window;

	if (!view->details->loading) {
		return;
	}

	nautilus_profile_start (NULL);

	window = nautilus_view_get_window (view);

	/* This can be called during destruction, in which case there
	 * is no NautilusWindow any more.
	 */
	if (window != NULL) {
		nautilus_view_update_toolbar_menus (view);
		schedule_update_context_menus (view);
		schedule_update_status (view);
		reset_update_interval (view);

		selection = view->details->pending_selection;

		if (NAUTILUS_IS_SEARCH_DIRECTORY (view->details->model)
		    && all_files_seen) {
			nautilus_view_select_first (view);
			do_reveal = TRUE;
		} else if (selection != NULL && all_files_seen) {
			view->details->pending_selection = NULL;

			nautilus_view_call_set_selection (view, selection);
			g_list_free_full (selection, g_object_unref);
			do_reveal = TRUE;
		}

		if (do_reveal) {
			if (NAUTILUS_IS_LIST_VIEW (view)) {
				/* HACK: We should be able to directly call reveal_selection here,
				 * but at this point the GtkTreeView hasn't allocated the new nodes
				 * yet, and it has a bug in the scroll calculation dealing with this
				 * special case. It would always make the selection the top row, even
				 * if no scrolling would be neccessary to reveal it. So we let it
				 * allocate before revealing.
				 */
				if (view->details->reveal_selection_idle_id != 0) {
					g_source_remove (view->details->reveal_selection_idle_id);
				}
				view->details->reveal_selection_idle_id = 
					g_idle_add (reveal_selection_idle_callback, view);
			} else {
				nautilus_view_reveal_selection (view);
			}
		}
		nautilus_view_display_selection_info (view);
	}

	view->details->loading = FALSE;
	g_signal_emit (view, signals[END_LOADING], 0, all_files_seen);

	nautilus_profile_end (NULL);
}


typedef struct {
	GHashTable *debuting_files;
	GList	   *added_files;
} DebutingFilesData;

static void
debuting_files_data_free (DebutingFilesData *data)
{
	g_hash_table_unref (data->debuting_files);
	nautilus_file_list_free (data->added_files);
	g_free (data);
}
 
/* This signal handler watch for the arrival of the icons created
 * as the result of a file operation. Once the last one is detected
 * it selects and reveals them all.
 */
static void
debuting_files_add_file_callback (NautilusView *view,
				  NautilusFile *new_file,
				  NautilusDirectory *directory,
				  DebutingFilesData *data)
{
	GFile *location;

	nautilus_profile_start (NULL);

	location = nautilus_file_get_location (new_file);

	if (g_hash_table_remove (data->debuting_files, location)) {
		nautilus_file_ref (new_file);
		data->added_files = g_list_prepend (data->added_files, new_file);

		if (g_hash_table_size (data->debuting_files) == 0) {
			nautilus_view_call_set_selection (view, data->added_files);
			nautilus_view_reveal_selection (view);
			g_signal_handlers_disconnect_by_func (view,
							      G_CALLBACK (debuting_files_add_file_callback),
							      data);
		}
	}

	nautilus_profile_end (NULL);

	g_object_unref (location);
}

typedef struct {
	GList		*added_files;
	NautilusView *directory_view;
} CopyMoveDoneData;

static void
copy_move_done_data_free (CopyMoveDoneData *data)
{
	g_assert (data != NULL);
	
	if (data->directory_view != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
					      (gpointer *) &data->directory_view);
	}

	nautilus_file_list_free (data->added_files);
	g_free (data);
}

static void
pre_copy_move_add_file_callback (NautilusView *view,
				 NautilusFile *new_file,
				 NautilusDirectory *directory,
				 CopyMoveDoneData *data)
{
	nautilus_file_ref (new_file);
	data->added_files = g_list_prepend (data->added_files, new_file);
}

/* This needs to be called prior to nautilus_file_operations_copy_move.
 * It hooks up a signal handler to catch any icons that get added before
 * the copy_done_callback is invoked. The return value should  be passed
 * as the data for uri_copy_move_done_callback.
 */
static CopyMoveDoneData *
pre_copy_move (NautilusView *directory_view)
{
	CopyMoveDoneData *copy_move_done_data;

	copy_move_done_data = g_new0 (CopyMoveDoneData, 1);
	copy_move_done_data->directory_view = directory_view;

	g_object_add_weak_pointer (G_OBJECT (copy_move_done_data->directory_view),
				   (gpointer *) &copy_move_done_data->directory_view);

	/* We need to run after the default handler adds the folder we want to
	 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
	 * must use connect_after.
	 */
	g_signal_connect (directory_view, "add-file",
			  G_CALLBACK (pre_copy_move_add_file_callback), copy_move_done_data);

	return copy_move_done_data;
}

/* This function is used to pull out any debuting uris that were added
 * and (as a side effect) remove them from the debuting uri hash table.
 */
static gboolean
copy_move_done_partition_func (gpointer data, gpointer callback_data)
{
 	GFile *location;
 	gboolean result;
 	
	location = nautilus_file_get_location (NAUTILUS_FILE (data));
	result = g_hash_table_remove ((GHashTable *) callback_data, location);
	g_object_unref (location);

	return result;
}

static gboolean
remove_not_really_moved_files (gpointer key,
			       gpointer value,
			       gpointer callback_data)
{
	GList **added_files;
	GFile *loc;

	loc = key;

	if (GPOINTER_TO_INT (value)) {
		return FALSE;
	}
	
	added_files = callback_data;
	*added_files = g_list_prepend (*added_files,
				       nautilus_file_get (loc));
	return TRUE;
}

/* When this function is invoked, the file operation is over, but all
 * the icons may not have been added to the directory view yet, so
 * we can't select them yet.
 * 
 * We're passed a hash table of the uri's to look out for, we hook
 * up a signal handler to await their arrival.
 */
static void
copy_move_done_callback (GHashTable *debuting_files, 
			 gboolean success,
			 gpointer data)
{
	NautilusView  *directory_view;
	CopyMoveDoneData *copy_move_done_data;
	DebutingFilesData  *debuting_files_data;

	copy_move_done_data = (CopyMoveDoneData *) data;
	directory_view = copy_move_done_data->directory_view;

	if (directory_view != NULL) {
		g_assert (NAUTILUS_IS_VIEW (directory_view));
	
		debuting_files_data = g_new (DebutingFilesData, 1);
		debuting_files_data->debuting_files = g_hash_table_ref (debuting_files);
		debuting_files_data->added_files = eel_g_list_partition
			(copy_move_done_data->added_files,
			 copy_move_done_partition_func,
			 debuting_files,
			 &copy_move_done_data->added_files);

		/* We're passed the same data used by pre_copy_move_add_file_callback, so disconnecting
		 * it will free data. We've already siphoned off the added_files we need, and stashed the
		 * directory_view pointer.
		 */
		g_signal_handlers_disconnect_by_func (directory_view,
						      G_CALLBACK (pre_copy_move_add_file_callback),
						      data);
	
		/* Any items in the debuting_files hash table that have
		 * "FALSE" as their value aren't really being copied
		 * or moved, so we can't wait for an add_file signal
		 * to come in for those.
		 */
		g_hash_table_foreach_remove (debuting_files,
					     remove_not_really_moved_files,
					     &debuting_files_data->added_files);
		
		if (g_hash_table_size (debuting_files) == 0) {
			/* on the off-chance that all the icons have already been added */
			if (debuting_files_data->added_files != NULL) {
				nautilus_view_call_set_selection (directory_view,
								  debuting_files_data->added_files);
				nautilus_view_reveal_selection (directory_view);
			}
			debuting_files_data_free (debuting_files_data);
		} else {
			/* We need to run after the default handler adds the folder we want to
			 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
			 * must use connect_after.
			 */
			g_signal_connect_data (directory_view,
					       "add-file",
					       G_CALLBACK (debuting_files_add_file_callback),
					       debuting_files_data,
					       (GClosureNotify) debuting_files_data_free,
					       G_CONNECT_AFTER);
		}
		/* Schedule menu update for undo items */
		schedule_update_context_menus (directory_view);
	}

	copy_move_done_data_free (copy_move_done_data);
}

static gboolean
view_file_still_belongs (NautilusView *view,
			 NautilusFile *file,
			 NautilusDirectory *directory)
{
	if (view->details->model != directory &&
	    g_list_find (view->details->subdirectory_list, directory) == NULL) {
		return FALSE;
	}
	
	return nautilus_directory_contains_file (directory, file);
}

static gboolean
still_should_show_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	return nautilus_view_should_show_file (view, file) &&
		view_file_still_belongs (view, file, directory);
}

static gboolean
ready_to_load (NautilusFile *file)
{
	return nautilus_file_check_if_ready (file,
					     NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
}

static int
compare_files_cover (gconstpointer a, gconstpointer b, gpointer callback_data)
{
	const FileAndDirectory *fad1, *fad2;
	NautilusView *view;
	
	view = callback_data;
	fad1 = a; fad2 = b;

	if (fad1->directory < fad2->directory) {
		return -1;
	} else if (fad1->directory > fad2->directory) {
		return 1;
	} else {
		return NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->compare_files (view, fad1->file, fad2->file);
	}
}
static void
sort_files (NautilusView *view, GList **list)
{
	*list = g_list_sort_with_data (*list, compare_files_cover, view);
	
}

/* Go through all the new added and changed files.
 * Put any that are not ready to load in the non_ready_files hash table.
 * Add all the rest to the old_added_files and old_changed_files lists.
 * Sort the old_*_files lists if anything was added to them.
 */
static void
process_new_files (NautilusView *view)
{
	GList *new_added_files, *new_changed_files, *old_added_files, *old_changed_files;
	GHashTable *non_ready_files;
	GList *node, *next;
	FileAndDirectory *pending;
	gboolean in_non_ready;

	new_added_files = view->details->new_added_files;
	view->details->new_added_files = NULL;
	new_changed_files = view->details->new_changed_files;
	view->details->new_changed_files = NULL;

	non_ready_files = view->details->non_ready_files;

	old_added_files = view->details->old_added_files;
	old_changed_files = view->details->old_changed_files;

	/* Newly added files go into the old_added_files list if they're
	 * ready, and into the hash table if they're not.
	 */
	for (node = new_added_files; node != NULL; node = next) {
		next = node->next;
		pending = (FileAndDirectory *)node->data;
		in_non_ready = g_hash_table_lookup (non_ready_files, pending) != NULL;
		if (nautilus_view_should_show_file (view, pending->file)) {
			if (ready_to_load (pending->file)) {
				if (in_non_ready) {
					g_hash_table_remove (non_ready_files, pending);
				}
				new_added_files = g_list_delete_link (new_added_files, node);
				old_added_files = g_list_prepend (old_added_files, pending);
			} else {
				if (!in_non_ready) {
					new_added_files = g_list_delete_link (new_added_files, node);
					g_hash_table_insert (non_ready_files, pending, pending);
				}
			}
		}
	}
	file_and_directory_list_free (new_added_files);

	/* Newly changed files go into the old_added_files list if they're ready
	 * and were seen non-ready in the past, into the old_changed_files list
	 * if they are read and were not seen non-ready in the past, and into
	 * the hash table if they're not ready.
	 */
	for (node = new_changed_files; node != NULL; node = next) {
		next = node->next;
		pending = (FileAndDirectory *)node->data;
		if (!still_should_show_file (view, pending->file, pending->directory) || ready_to_load (pending->file)) {
			if (g_hash_table_lookup (non_ready_files, pending) != NULL) {
				g_hash_table_remove (non_ready_files, pending);
				if (still_should_show_file (view, pending->file, pending->directory)) {
					new_changed_files = g_list_delete_link (new_changed_files, node);
					old_added_files = g_list_prepend (old_added_files, pending);
				}
			} else if (nautilus_view_should_show_file (view, pending->file)) {
				new_changed_files = g_list_delete_link (new_changed_files, node);
				old_changed_files = g_list_prepend (old_changed_files, pending);
			}
		}
	}
	file_and_directory_list_free (new_changed_files);

	/* If any files were added to old_added_files, then resort it. */
	if (old_added_files != view->details->old_added_files) {
		view->details->old_added_files = old_added_files;
		sort_files (view, &view->details->old_added_files);
	}

	/* Resort old_changed_files too, since file attributes
	 * relevant to sorting could have changed.
	 */
	if (old_changed_files != view->details->old_changed_files) {
		view->details->old_changed_files = old_changed_files;
		sort_files (view, &view->details->old_changed_files);
	}

}

static void
process_old_files (NautilusView *view)
{
	GList *files_added, *files_changed, *node;
	FileAndDirectory *pending;
	GList *selection, *files;
	gboolean send_selection_change;

	files_added = view->details->old_added_files;
	files_changed = view->details->old_changed_files;
	
	send_selection_change = FALSE;

	if (files_added != NULL || files_changed != NULL) {
		g_signal_emit (view, signals[BEGIN_FILE_CHANGES], 0);

		for (node = files_added; node != NULL; node = node->next) {
			pending = node->data;
			g_signal_emit (view,
				       signals[ADD_FILE], 0, pending->file, pending->directory);
		}

		for (node = files_changed; node != NULL; node = node->next) {
			pending = node->data;
			g_signal_emit (view,
				       signals[still_should_show_file (view, pending->file, pending->directory)
					       ? FILE_CHANGED : REMOVE_FILE], 0,
				       pending->file, pending->directory);
		}

		g_signal_emit (view, signals[END_FILE_CHANGES], 0);

		if (files_changed != NULL) {
			selection = nautilus_view_get_selection (view);
			files = file_and_directory_list_to_files (files_changed);
			send_selection_change = eel_g_lists_sort_and_check_for_intersection
				(&files, &selection);
			nautilus_file_list_free (files);
			nautilus_file_list_free (selection);
		}
		
		file_and_directory_list_free (view->details->old_added_files);
		view->details->old_added_files = NULL;

		file_and_directory_list_free (view->details->old_changed_files);
		view->details->old_changed_files = NULL;
	}

	if (send_selection_change) {
		/* Send a selection change since some file names could
		 * have changed.
		 */
		nautilus_view_send_selection_change (view);
	}
}

static void
display_pending_files (NautilusView *view)
{

	/* Don't dispatch any updates while the view is frozen. */
	if (view->details->updates_frozen) {
		return;
	}

	process_new_files (view);
	process_old_files (view);

	if (view->details->model != NULL
	    && nautilus_directory_are_all_files_seen (view->details->model)
	    && g_hash_table_size (view->details->non_ready_files) == 0) {
		done_loading (view, TRUE);
	}
}

void
nautilus_view_freeze_updates (NautilusView *view)
{
	view->details->updates_frozen = TRUE;
	view->details->updates_queued = 0;
	view->details->needs_reload = FALSE;
}

void
nautilus_view_unfreeze_updates (NautilusView *view)
{
	view->details->updates_frozen = FALSE;

	if (view->details->needs_reload) {
		view->details->needs_reload = FALSE;
		if (view->details->model != NULL) {
			load_directory (view, view->details->model);
		}
	} else {
		schedule_idle_display_of_pending_files (view);
	}
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
	NautilusView *view;
	
	view = NAUTILUS_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->display_selection_idle_id = 0;
	nautilus_view_display_selection_info (view);
	nautilus_view_send_selection_change (view);

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static void
remove_update_context_menus_timeout_callback (NautilusView *view)
{
	if (view->details->update_context_menus_timeout_id != 0) {
		g_source_remove (view->details->update_context_menus_timeout_id);
		view->details->update_context_menus_timeout_id = 0;
	}
}

static void
update_context_menus_if_pending (NautilusView *view)
{
	remove_update_context_menus_timeout_callback (view);

	nautilus_view_update_context_menus(view);
}

static gboolean
update_context_menus_timeout_callback (gpointer data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->update_context_menus_timeout_id = 0;
	nautilus_view_update_context_menus(view);

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static gboolean
display_pending_callback (gpointer data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->display_pending_source_id = 0;

	display_pending_files (view);

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static void
schedule_idle_display_of_pending_files (NautilusView *view)
{
	/* Get rid of a pending source as it might be a timeout */
	unschedule_display_of_pending_files (view);

	/* We want higher priority than the idle that handles the relayout
	   to avoid a resort on each add. But we still want to allow repaints
	   and other hight prio events while we have pending files to show. */
	view->details->display_pending_source_id =
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
				 display_pending_callback, view, NULL);
}

static void
schedule_timeout_display_of_pending_files (NautilusView *view, guint interval)
{
 	/* No need to schedule an update if there's already one pending. */
	if (view->details->display_pending_source_id != 0) {
 		return;
	}
 
	view->details->display_pending_source_id =
		g_timeout_add (interval, display_pending_callback, view);
}

static void
unschedule_display_of_pending_files (NautilusView *view)
{
	/* Get rid of source if it's active. */
	if (view->details->display_pending_source_id != 0) {
		g_source_remove (view->details->display_pending_source_id);
		view->details->display_pending_source_id = 0;
	}
}

static void
queue_pending_files (NautilusView *view,
		     NautilusDirectory *directory,
		     GList *files,
		     GList **pending_list)
{
	if (files == NULL) {
		return;
	}

	/* Don't queue any more updates if we need to reload anyway */
	if (view->details->needs_reload) {
		return;
	}

	if (view->details->updates_frozen) {
		view->details->updates_queued += g_list_length (files);
		/* Mark the directory for reload when there are too much queued
		 * changes to prevent the pending list from growing infinitely.
		 */
		if (view->details->updates_queued > MAX_QUEUED_UPDATES) {
			view->details->needs_reload = TRUE;
			return;
		}
	}

	

	*pending_list = g_list_concat (file_and_directory_list_from_files (directory, files),
				       *pending_list);

	if (! view->details->loading || nautilus_directory_are_all_files_seen (directory)) {
		schedule_timeout_display_of_pending_files (view, view->details->update_interval);
	}
}

static void
remove_changes_timeout_callback (NautilusView *view) 
{
	if (view->details->changes_timeout_id != 0) {
		g_source_remove (view->details->changes_timeout_id);
		view->details->changes_timeout_id = 0;
	}
}

static void
reset_update_interval (NautilusView *view)
{
	view->details->update_interval = UPDATE_INTERVAL_MIN;
	remove_changes_timeout_callback (view);
	/* Reschedule a pending timeout to idle */
	if (view->details->display_pending_source_id != 0) {
		schedule_idle_display_of_pending_files (view);
	}
}

static gboolean
changes_timeout_callback (gpointer data)
{
	gint64 now;
	gint64 time_delta;
	gboolean ret;
	NautilusView *view;

	view = NAUTILUS_VIEW (data);

	g_object_ref (G_OBJECT (view));

	now = g_get_monotonic_time ();
	time_delta = now - view->details->last_queued;

	if (time_delta < UPDATE_INTERVAL_RESET*1000) {
		if (view->details->update_interval < UPDATE_INTERVAL_MAX &&
		    view->details->loading) {
			/* Increase */
			view->details->update_interval += UPDATE_INTERVAL_INC;
		}
		ret = TRUE;
	} else {
		/* Reset */
		reset_update_interval (view);
		ret = FALSE;
	}

	g_object_unref (G_OBJECT (view));

	return ret;
}

static void
schedule_changes (NautilusView *view)
{
	/* Remember when the change was queued */
	view->details->last_queued = g_get_monotonic_time ();

	/* No need to schedule if there are already changes pending or during loading */
	if (view->details->changes_timeout_id != 0 ||
	    view->details->loading) {
		return;
	}

	view->details->changes_timeout_id = 
		g_timeout_add (UPDATE_INTERVAL_TIMEOUT_INTERVAL, changes_timeout_callback, view);
}

static void
files_added_callback (NautilusDirectory *directory,
		      GList *files,
		      gpointer callback_data)
{
	NautilusView *view;
	GtkWindow *window;
	char *uri;

	view = NAUTILUS_VIEW (callback_data);

	nautilus_profile_start (NULL);

	window = nautilus_view_get_containing_window (view);
	uri = nautilus_view_get_uri (view);
	DEBUG_FILES (files, "Files added in window %p: %s",
		     window, uri ? uri : "(no directory)");
	g_free (uri);

	schedule_changes (view);

	queue_pending_files (view, directory, files, &view->details->new_added_files);

	/* The number of items could have changed */
	schedule_update_status (view);

	nautilus_profile_end (NULL);
}

static void
files_changed_callback (NautilusDirectory *directory,
			GList *files,
			gpointer callback_data)
{
	NautilusView *view;
	GtkWindow *window;
	char *uri;
	
	view = NAUTILUS_VIEW (callback_data);

	window = nautilus_view_get_containing_window (view);
	uri = nautilus_view_get_uri (view);
	DEBUG_FILES (files, "Files changed in window %p: %s",
		     window, uri ? uri : "(no directory)");
	g_free (uri);

	schedule_changes (view);

	queue_pending_files (view, directory, files, &view->details->new_changed_files);
	
	/* The free space or the number of items could have changed */
	schedule_update_status (view);

	/* A change in MIME type could affect the Open with menu, for
	 * one thing, so we need to update menus when files change.
	 */
	schedule_update_context_menus (view);
}

static void
done_loading_callback (NautilusDirectory *directory,
		       gpointer callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	nautilus_profile_start (NULL);
	process_new_files (view);
	if (g_hash_table_size (view->details->non_ready_files) == 0) {
		/* Unschedule a pending update and schedule a new one with the minimal
		 * update interval. This gives the view a short chance at gathering the
		 * (cached) deep counts.
		 */
		unschedule_display_of_pending_files (view);
		schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);
	}
	nautilus_profile_end (NULL);
}

static void
load_error_callback (NautilusDirectory *directory,
		     GError *error,
		     gpointer callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	/* FIXME: By doing a stop, we discard some pending files. Is
	 * that OK?
	 */
	nautilus_view_stop_loading (view);

	nautilus_report_error_loading_directory
		(nautilus_view_get_directory_as_file (view),
		 error,
		 nautilus_view_get_containing_window (view));
}

void
nautilus_view_add_subdirectory (NautilusView  *view,
				NautilusDirectory*directory)
{
	NautilusFileAttributes attributes;

	g_assert (!g_list_find (view->details->subdirectory_list, directory));
	
	nautilus_directory_ref (directory);

	attributes =
		NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
		NAUTILUS_FILE_ATTRIBUTE_MOUNT |
		NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

	nautilus_directory_file_monitor_add (directory,
					     &view->details->model,
					     view->details->show_hidden_files,
					     attributes,
					     files_added_callback, view);
	
	g_signal_connect
		(directory, "files-added",
		 G_CALLBACK (files_added_callback), view);
	g_signal_connect
		(directory, "files-changed",
		 G_CALLBACK (files_changed_callback), view);
	
	view->details->subdirectory_list = g_list_prepend (
							   view->details->subdirectory_list, directory);
}

void
nautilus_view_remove_subdirectory (NautilusView  *view,
				   NautilusDirectory*directory)
{
	g_assert (g_list_find (view->details->subdirectory_list, directory));
	
	view->details->subdirectory_list = g_list_remove (
							  view->details->subdirectory_list, directory);

	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (files_added_callback),
					      view);
	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (files_changed_callback),
					      view);

	nautilus_directory_file_monitor_remove (directory, &view->details->model);

	nautilus_directory_unref (directory);
}

/**
 * nautilus_view_get_loading:
 * @view: an #NautilusView.
 *
 * Return value: #gboolean inicating whether @view is currently loaded.
 * 
 **/
gboolean
nautilus_view_get_loading (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return view->details->loading;
}

/**
 * nautilus_view_get_model:
 *
 * Get the model for this NautilusView.
 * @view: NautilusView of interest.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/
NautilusDirectory *
nautilus_view_get_model (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);

	return view->details->model;
}

GdkAtom
nautilus_view_get_copied_files_atom (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), GDK_NONE);
	
	return copied_files_atom;
}

static void
offset_drop_points (GArray *relative_item_points,
		    int x_offset, int y_offset)
{
	guint index;

	if (relative_item_points == NULL) {
		return;
	}

	for (index = 0; index < relative_item_points->len; index++) {
		g_array_index (relative_item_points, GdkPoint, index).x += x_offset;
		g_array_index (relative_item_points, GdkPoint, index).y += y_offset;
	}
}

/* special_link_in_selection
 * 
 * Return TRUE if one of our special links is in the selection.
 * Special links include the following: 
 *	 NAUTILUS_DESKTOP_LINK_TRASH, NAUTILUS_DESKTOP_LINK_HOME, NAUTILUS_DESKTOP_LINK_MOUNT
 */
 
static gboolean
special_link_in_selection (GList *selection)
{
	gboolean saw_link;
	GList *node;
	NautilusFile *file;

	saw_link = FALSE;

	for (node = selection; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		saw_link = NAUTILUS_IS_DESKTOP_ICON_FILE (file);
		
		if (saw_link) {
			break;
		}
	}
	
	return saw_link;
}

/* desktop_or_home_dir_in_selection
 * 
 * Return TRUE if either the desktop or the home directory is in the selection.
 */
 
static gboolean
desktop_or_home_dir_in_selection (GList *selection)
{
	gboolean saw_desktop_or_home_dir;
	GList *node;
	NautilusFile *file;

	saw_desktop_or_home_dir = FALSE;

	for (node = selection; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		saw_desktop_or_home_dir =
			nautilus_file_is_home (file)
			|| nautilus_file_is_desktop_directory (file);
		
		if (saw_desktop_or_home_dir) {
			break;
		}
	}
	
	return saw_desktop_or_home_dir;
}

static void
trash_or_delete_done_cb (GHashTable *debuting_uris,
			 gboolean user_cancel,
			 NautilusView *view)
{
	if (user_cancel) {
		view->details->selection_was_removed = FALSE;
	}
}

static void
trash_or_delete_files (GtkWindow *parent_window,
		       const GList *files,
		       NautilusView *view)
{
	GList *locations;
	const GList *node;
	
	locations = NULL;
	for (node = files; node != NULL; node = node->next) {
		locations = g_list_prepend (locations,
					    nautilus_file_get_location ((NautilusFile *) node->data));
	}
	
	locations = g_list_reverse (locations);

	nautilus_file_operations_trash_or_delete (locations,
						  parent_window,
						  (NautilusDeleteCallback) trash_or_delete_done_cb,
						  view);
	g_list_free_full (locations, g_object_unref);
}

static gboolean
can_rename_file (NautilusView *view, NautilusFile *file)
{
	return nautilus_file_can_rename (file);
}

gboolean
nautilus_view_get_is_renaming (NautilusView *view)
{
	return view->details->is_renaming;
}

void
nautilus_view_set_is_renaming (NautilusView *view,
			       gboolean      is_renaming)
{
	view->details->is_renaming = is_renaming;
}

static void
start_renaming_file (NautilusView *view,
		     NautilusFile *file,
		     gboolean select_all)
{
	view->details->is_renaming = TRUE;

	if (file !=  NULL) {
		nautilus_view_select_file (view, file);
	}
}

static void
open_one_in_new_window (gpointer data, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (NAUTILUS_IS_VIEW (callback_data));

	nautilus_view_activate_file (NAUTILUS_VIEW (callback_data),
				     NAUTILUS_FILE (data),
				     NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
update_context_menu_position_from_event (NautilusView *view,
					 GdkEventButton  *event)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	if (event != NULL) {
		view->details->context_menu_position.x = event->x;
		view->details->context_menu_position.y = event->y;
	} else {
		view->details->context_menu_position.x = -1;
		view->details->context_menu_position.y = -1;
	}
}

NautilusFile *
nautilus_view_get_directory_as_file (NautilusView *view)
{
	g_assert (NAUTILUS_IS_VIEW (view));

	return view->details->directory_as_file; 
}

static GdkPixbuf *
get_menu_icon_for_file (NautilusFile *file,
			GtkWidget    *widget)
{
	NautilusIconInfo *info;
	GdkPixbuf *pixbuf;
	int size, scale;

	size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	scale = gtk_widget_get_scale_factor (widget);
	
	info = nautilus_file_get_icon (file, size, scale, 0);
	pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (info, size);
	g_object_unref (info);
	
	return pixbuf;
}

static GList *
get_extension_selection_menu_items (NautilusView *view)
{
	NautilusWindow *window;
	GList *items;
	GList *providers;
	GList *l;
	GList *selection;

	window = nautilus_view_get_window (view);
	selection = nautilus_view_get_selection (view);
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;

		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_file_items (provider,
								    GTK_WIDGET (window),
								    selection);
		items = g_list_concat (items, file_items);
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

static GList *
get_extension_background_menu_items (NautilusView *view)
{
	NautilusWindow *window;
	NautilusFile *file;
	GList *items;
	GList *providers;
	GList *l;

	window = nautilus_view_get_window (view);
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	file = nautilus_window_slot_get_file (view->details->slot);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;

		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_background_items (provider,
									  GTK_WIDGET (window),
									  file);
		items = g_list_concat (items, file_items);
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

typedef struct
{
	NautilusMenuItem *item;
	GList *selection;
	GAction *action;
} ExtensionActionCallbackData;

static void
extension_action_callback_data_free (ExtensionActionCallbackData *data)
{
	g_object_unref (data->item);
	nautilus_file_list_free (data->selection);

	g_free (data);
}

static void
extension_action_callback (GSimpleAction *action,
			   GVariant      *state,
			   gpointer       user_data)
{
	ExtensionActionCallbackData *data;

	data = user_data;
	nautilus_menu_item_activate (data->item);
}

static void
add_extension_action (NautilusView *view,
		      NautilusMenuItem *item)
{
	char *name, *label, *parsed_name;
	gboolean sensitive;
	GAction *action;
	ExtensionActionCallbackData *data;

	g_object_get (G_OBJECT (item),
		      "name", &name, "label", &label,
		      "sensitive", &sensitive,
		      NULL);

	parsed_name = nautilus_escape_action_name (name, "extension_");
	action = G_ACTION (g_simple_action_new (parsed_name, NULL));

	data = g_new0 (ExtensionActionCallbackData, 1);
	data->item = g_object_ref (item);
	data->action = action;

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (extension_action_callback),
			       data,
			       (GClosureNotify)extension_action_callback_data_free, 0);

	g_action_map_add_action  (G_ACTION_MAP (view->details->view_action_group),
				  action);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
	g_object_unref (action);

	g_free (name);
	g_free (parsed_name);
	g_free (label);
}

static GMenu *
add_extension_menu_items (NautilusView *view,
			  GList        *menu_items,
			  GMenu        *insertion_menu,
			  GMenuItem    *submenu_parent)
{
	GList *l;
	GMenuItem *menu_item;
	GMenu *gmenu, *children_menu;
	char *name, *parsed_name, *label, *detailed_action_name;
	gboolean sensitive;

	gmenu = g_menu_new ();

	for (l = menu_items; l; l = l->next) {
		NautilusMenuItem *item;
		NautilusMenu *menu;

		item = NAUTILUS_MENU_ITEM (l->data);

		g_object_get (item, "menu", &menu, NULL);

		g_object_get (G_OBJECT (item),
			      "name", &name, "label", &label,
			      "sensitive", &sensitive,
			      NULL);

		add_extension_action (view, item);
		parsed_name = nautilus_escape_action_name (name, "extension_");
		detailed_action_name =  g_strconcat ("view.", parsed_name, NULL);
		menu_item = g_menu_item_new (label, detailed_action_name);
		 if (menu != NULL) {
			GList *children;

			children = nautilus_menu_get_items (menu);

			children_menu = add_extension_menu_items (view,
								  children,
								  insertion_menu,
								  menu_item);

			nautilus_menu_item_list_free (children);
			g_menu_item_set_submenu (menu_item, G_MENU_MODEL (children_menu));
		}

		g_menu_append_item (gmenu, menu_item);
	}

	if (submenu_parent) {
		g_menu_item_set_submenu (submenu_parent, G_MENU_MODEL (gmenu));
	} else {
		nautilus_gmenu_merge (insertion_menu,
				      gmenu,
				      "extensions",
				      FALSE);
	}

	g_free (name);
	g_free (parsed_name);
	g_free (label);

	return gmenu;
}

static void
update_extensions_menus (NautilusView *view)
{
	GList *selection_items, *background_items;

	selection_items = get_extension_selection_menu_items (view);
	background_items = get_extension_background_menu_items (view);
	if (selection_items != NULL) {
		add_extension_menu_items (view,
					  selection_items,
					  view->details->selection_menu,
					  NULL);

		nautilus_menu_item_list_free (selection_items);
	}

	if (background_items != NULL) {
		add_extension_menu_items (view,
					  background_items,
					  view->details->background_menu,
					  NULL);

		nautilus_menu_item_list_free (background_items);
	}
}

static char *
change_to_view_directory (NautilusView *view)
{
	char *path;
	char *old_path;

	old_path = g_get_current_dir ();

	path = get_view_directory (view);

	/* FIXME: What to do about non-local directories? */
	if (path != NULL) {
		g_chdir (path);
	}

	g_free (path);

	return old_path;
}

static char **
get_file_names_as_parameter_array (GList *selection,
				   NautilusDirectory *model)
{
	NautilusFile *file;
	char **parameters;
	GList *node;
	GFile *file_location;
	GFile *model_location;
	int i;

	if (model == NULL) {
		return NULL;
	}

	parameters = g_new (char *, g_list_length (selection) + 1);

	model_location = nautilus_directory_get_location (model);

	for (node = selection, i = 0; node != NULL; node = node->next, i++) {
		file = NAUTILUS_FILE (node->data);

		if (!nautilus_file_is_local (file)) {
			parameters[i] = NULL;
			g_strfreev (parameters);
			return NULL;
		}

		file_location = nautilus_file_get_location (NAUTILUS_FILE (node->data));
		parameters[i] = g_file_get_relative_path (model_location, file_location);
		if (parameters[i] == NULL) {
			parameters[i] = g_file_get_path (file_location);
		}
		g_object_unref (file_location);
	}

	g_object_unref (model_location);

	parameters[i] = NULL;
	return parameters;
}

static char *
get_file_paths_or_uris_as_newline_delimited_string (GList *selection, gboolean get_paths)
{
	char *path;
	char *uri;
	char *result;
	NautilusDesktopLink *link;
	GString *expanding_string;
	GList *node;
	GFile *location;

	expanding_string = g_string_new ("");
	for (node = selection; node != NULL; node = node->next) {
		uri = NULL;
		if (NAUTILUS_IS_DESKTOP_ICON_FILE (node->data)) {
			link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (node->data));
			if (link != NULL) {
				location = nautilus_desktop_link_get_activation_location (link);
				uri = g_file_get_uri (location);
				g_object_unref (location);
				g_object_unref (G_OBJECT (link));
			}
		} else {
			uri = nautilus_file_get_uri (NAUTILUS_FILE (node->data));
		}
		if (uri == NULL) {
			continue;
		}

		if (get_paths) {
			path = g_filename_from_uri (uri, NULL, NULL);
			if (path != NULL) {
				g_string_append (expanding_string, path);
				g_free (path);
				g_string_append (expanding_string, "\n");
			}
		} else {
			g_string_append (expanding_string, uri);
			g_string_append (expanding_string, "\n");
		}
		g_free (uri);
	}

	result = expanding_string->str;
	g_string_free (expanding_string, FALSE);

	return result;
}

static char *
get_file_paths_as_newline_delimited_string (GList *selection)
{
	return get_file_paths_or_uris_as_newline_delimited_string (selection, TRUE);
}

static char *
get_file_uris_as_newline_delimited_string (GList *selection)
{
	return get_file_paths_or_uris_as_newline_delimited_string (selection, FALSE);
}

/* returns newly allocated strings for setting the environment variables */
static void
get_strings_for_environment_variables (NautilusView *view, GList *selected_files,
				       char **file_paths, char **uris, char **uri)
{
	char *directory_uri;

	/* We need to check that the directory uri starts with "file:" since
	 * nautilus_directory_is_local returns FALSE for nfs.
	 */
	directory_uri = nautilus_directory_get_uri (view->details->model);
	if (g_str_has_prefix (directory_uri, "file:") ||
	    eel_uri_is_desktop (directory_uri) ||
	    eel_uri_is_trash (directory_uri)) {
		*file_paths = get_file_paths_as_newline_delimited_string (selected_files);
	} else {
		*file_paths = g_strdup ("");
	}
	g_free (directory_uri);

	*uris = get_file_uris_as_newline_delimited_string (selected_files);

	*uri = nautilus_directory_get_uri (view->details->model);
	if (eel_uri_is_desktop (*uri)) {
		g_free (*uri);
		*uri = nautilus_get_desktop_directory_uri ();
	}
}

/*
 * Set up some environment variables that scripts can use
 * to take advantage of the current Nautilus state.
 */
static void
set_script_environment_variables (NautilusView *view, GList *selected_files)
{
	char *file_paths;
	char *uris;
	char *uri;
	char *geometry_string;

	get_strings_for_environment_variables (view, selected_files,
					       &file_paths, &uris, &uri);

	g_setenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS", file_paths, TRUE);
	g_free (file_paths);

	g_setenv ("NAUTILUS_SCRIPT_SELECTED_URIS", uris, TRUE);
	g_free (uris);

	g_setenv ("NAUTILUS_SCRIPT_CURRENT_URI", uri, TRUE);
	g_free (uri);

	geometry_string = eel_gtk_window_get_geometry_string
		(GTK_WINDOW (nautilus_view_get_containing_window (view)));
	g_setenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY", geometry_string, TRUE);
	g_free (geometry_string);
}

/* Unset all the special script environment variables. */
static void
unset_script_environment_variables (void)
{
	g_unsetenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS");
	g_unsetenv ("NAUTILUS_SCRIPT_SELECTED_URIS");
	g_unsetenv ("NAUTILUS_SCRIPT_CURRENT_URI");
	g_unsetenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY");
}

static void
run_script (GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
	ScriptLaunchParameters *launch_parameters;
	GdkScreen *screen;
	GList *selected_files;
	char *file_uri;
	char *local_file_path;
	char *quoted_path;
	char *old_working_dir;
	char **parameters;
	
	launch_parameters = (ScriptLaunchParameters *) user_data;

	file_uri = nautilus_file_get_uri (launch_parameters->file);
	local_file_path = g_filename_from_uri (file_uri, NULL, NULL);
	g_assert (local_file_path != NULL);
	g_free (file_uri);

	quoted_path = g_shell_quote (local_file_path);
	g_free (local_file_path);

	old_working_dir = change_to_view_directory (launch_parameters->directory_view);

	selected_files = nautilus_view_get_selection (launch_parameters->directory_view);
	set_script_environment_variables (launch_parameters->directory_view, selected_files);
	 
	parameters = get_file_names_as_parameter_array (selected_files,
						        launch_parameters->directory_view->details->model);

	screen = gtk_widget_get_screen (GTK_WIDGET (launch_parameters->directory_view));

	DEBUG ("run_script, script_path=“%s” (omitting script parameters)",
	       local_file_path);

	nautilus_launch_application_from_command_array (screen, quoted_path, FALSE,
							(const char * const *) parameters);
	g_strfreev (parameters);

	nautilus_file_list_free (selected_files);
	unset_script_environment_variables ();
	g_chdir (old_working_dir);		
	g_free (old_working_dir);
	g_free (quoted_path);
}

static void
add_script_to_scripts_menus (NautilusView *view,
			     NautilusFile *file,
			     GMenu *menu)
{
	gchar *name;
	GdkPixbuf *mimetype_icon;
	gchar *action_name, *detailed_action_name;
	ScriptLaunchParameters *launch_parameters;
	GAction *action;
	GMenuItem *menu_item;

	launch_parameters = script_launch_parameters_new (file, view);

	name = nautilus_file_get_display_name (file);
	action_name = nautilus_escape_action_name (name, "script_");

	action = G_ACTION (g_simple_action_new (action_name, NULL));
	
	g_signal_connect_data (action, "activate",
			       G_CALLBACK (run_script),
			       launch_parameters,
			       (GClosureNotify)script_launch_parameters_free, 0);

	g_action_map_add_action (G_ACTION_MAP (view->details->view_action_group), action);
	
	g_object_unref (action);
	
	detailed_action_name =  g_strconcat ("view.", action_name, NULL);
	menu_item = g_menu_item_new (name, detailed_action_name);

	mimetype_icon = get_menu_icon_for_file (file, GTK_WIDGET (view));
	if (mimetype_icon != NULL) {
		g_menu_item_set_icon (menu_item, G_ICON (mimetype_icon));
		g_object_unref (mimetype_icon);
	}

	g_menu_append_item (menu, menu_item);

	g_free (name);
	g_free (action_name);
	g_free (detailed_action_name);
	g_object_unref (menu_item);
}

static gboolean
directory_belongs_in_scripts_menu (const char *uri)
{
	int num_levels;
	int i;

	if (!g_str_has_prefix (uri, scripts_directory_uri)) {
		return FALSE;
	}

	num_levels = 0;
	for (i = scripts_directory_uri_length; uri[i] != '\0'; i++) {
		if (uri[i] == '/') {
			num_levels++;
		}
	}

	if (num_levels > MAX_MENU_LEVELS) {
		return FALSE;
	}

	return TRUE;
}

static GMenu *
update_directory_in_scripts_menu (NautilusView *view,
				  NautilusDirectory *directory)
{
	GList *file_list, *filtered, *node;
	GMenu *menu, *children_menu;
	GMenuItem *menu_item;
	gboolean any_scripts;
	NautilusFile *file;
	NautilusDirectory *dir;
	char *uri;
	int num;

	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	file_list = nautilus_directory_get_file_list (directory);
	filtered = nautilus_file_list_filter_hidden (file_list, FALSE);
	nautilus_file_list_free (file_list);
	menu = g_menu_new ();

	file_list = nautilus_file_list_sort_by_display_name (filtered);

	num = 0;
	any_scripts = FALSE;
	for (node = file_list; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++) {
		file = node->data;
		if (nautilus_file_is_directory (file)) {
			uri = nautilus_file_get_uri (file);
			if (directory_belongs_in_scripts_menu (uri)) {
				dir = nautilus_directory_get_by_uri (uri);
				add_directory_to_scripts_directory_list (view, dir);

				children_menu = update_directory_in_scripts_menu (view, dir);

				if (children_menu != NULL) {
					menu_item = g_menu_item_new_submenu (nautilus_file_get_display_name (file),
									     G_MENU_MODEL (children_menu));
					g_menu_append_item (menu, menu_item);
					any_scripts = TRUE;
					g_object_unref (menu_item);
					g_object_unref (children_menu);
				}

				nautilus_directory_unref (dir);
			}
			g_free (uri);
		} else if (nautilus_file_is_launchable (file)) {
			add_script_to_scripts_menus (view, file, menu);
			any_scripts = TRUE;
		}
	}

	nautilus_file_list_free (file_list);

	if (!any_scripts) {
		g_object_unref (menu);
		menu = NULL;
	}

	return menu;
}



static void
update_scripts_menu (NautilusView *view)
{
	GList *sorted_copy, *node;
	NautilusDirectory *directory;
	GMenu *submenu;
	char *uri;

	sorted_copy = nautilus_directory_list_sort_by_uri
		(nautilus_directory_list_copy (view->details->scripts_directory_list));

	for (node = sorted_copy; node != NULL; node = node->next) {
		directory = node->data;

		uri = nautilus_directory_get_uri (directory);
		if (!directory_belongs_in_scripts_menu (uri)) {
			remove_directory_from_scripts_directory_list (view, directory);
		}
		g_free (uri);
	}
	nautilus_directory_list_free (sorted_copy);

	directory = nautilus_directory_get_by_uri (scripts_directory_uri);
	submenu = update_directory_in_scripts_menu (view, directory);
	if (submenu != NULL) {
		nautilus_gmenu_merge (view->details->selection_menu,
				      submenu,
				      "scripts-submenu",
				      TRUE);
	}
	
	view->details->scripts_present = submenu != NULL;
}

static void
create_template (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{

	CreateTemplateParameters *parameters;

	parameters = user_data;
	
	nautilus_view_new_file (parameters->directory_view, NULL, parameters->file);
}

static void
add_template_to_templates_menus (NautilusView *view,
				 NautilusFile *file,
				 GMenu *menu)
{
	char *tmp, *uri, *name;
	char *escaped_label;
	GdkPixbuf *mimetype_icon;
	char *action_name, *detailed_action_name;
	CreateTemplateParameters *parameters;
	GAction *action;
	GMenuItem *menu_item;

	tmp = nautilus_file_get_display_name (file);
	name = eel_filename_strip_extension (tmp);
	g_free (tmp);

	uri = nautilus_file_get_uri (file);

	action_name = nautilus_escape_action_name (uri, "template_");
	escaped_label = eel_str_double_underscores (name);
	
	parameters = create_template_parameters_new (file, view);

	action = G_ACTION (g_simple_action_new (action_name, NULL));
	
	g_signal_connect_data (action, "activate",
			       G_CALLBACK (create_template),
			       parameters, 
			       (GClosureNotify)create_templates_parameters_free, 0);

	g_action_map_add_action (G_ACTION_MAP (view->details->view_action_group), action);
	
	g_object_unref (action);
	
	detailed_action_name =  g_strconcat ("view.", action_name, NULL);
	menu_item = g_menu_item_new (name, detailed_action_name);

	mimetype_icon = get_menu_icon_for_file (file, GTK_WIDGET (view));
	if (mimetype_icon != NULL) {
		g_menu_item_set_icon (menu_item, G_ICON (mimetype_icon));
		g_object_unref (mimetype_icon);
	}

	g_menu_append_item (menu, menu_item);

	g_free (escaped_label);
	g_free (name);
	g_free (uri);
	g_free (action_name);
	g_free (detailed_action_name);
}

static void
update_templates_directory (NautilusView *view)
{
	NautilusDirectory *templates_directory;
	GList *node, *next;
	char *templates_uri;

	for (node = view->details->templates_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_templates_directory_list (view, node->data);
	}
	
	if (nautilus_should_use_templates_directory ()) {
		templates_uri = nautilus_get_templates_directory_uri ();
		templates_directory = nautilus_directory_get_by_uri (templates_uri);
		g_free (templates_uri);
		add_directory_to_templates_directory_list (view, templates_directory);
		nautilus_directory_unref (templates_directory);
	}
}

static gboolean
directory_belongs_in_templates_menu (const char *templates_directory_uri,
				     const char *uri)
{
	int num_levels;
	int i;

	if (templates_directory_uri == NULL) {
		return FALSE;
	}
	
	if (!g_str_has_prefix (uri, templates_directory_uri)) {
		return FALSE;
	}

	num_levels = 0;
	for (i = strlen (templates_directory_uri); uri[i] != '\0'; i++) {
		if (uri[i] == '/') {
			num_levels++;
		}
	}

	if (num_levels > MAX_MENU_LEVELS) {
		return FALSE;
	}

	return TRUE;
}

static GMenu *
update_directory_in_templates_menu (NautilusView *view,
				    NautilusDirectory *directory)
{
	GList *file_list, *filtered, *node;
	GMenu *menu, *children_menu;
	GMenuItem *menu_item;
	gboolean any_templates;
	NautilusFile *file;
	NautilusDirectory *dir;
	char *uri;
	char *templates_directory_uri;
	int num;

	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	file_list = nautilus_directory_get_file_list (directory);
	filtered = nautilus_file_list_filter_hidden (file_list, FALSE);
	nautilus_file_list_free (file_list);
	templates_directory_uri = nautilus_get_templates_directory_uri ();
	menu = g_menu_new ();

	file_list = nautilus_file_list_sort_by_display_name (filtered);

	num = 0;
	any_templates = FALSE;
	for (node = file_list; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++) {
		file = node->data;
		if (nautilus_file_is_directory (file)) {
			uri = nautilus_file_get_uri (file);
			if (directory_belongs_in_templates_menu (templates_directory_uri, uri)) {
				dir = nautilus_directory_get_by_uri (uri);
				add_directory_to_templates_directory_list (view, dir);

				children_menu = update_directory_in_templates_menu (view, dir);

				if (children_menu != NULL) {
					menu_item = g_menu_item_new_submenu (nautilus_file_get_display_name (file),
									     G_MENU_MODEL (children_menu));
					g_menu_append_item (menu, menu_item);
					any_templates = TRUE;
					g_object_unref (menu_item);
					g_object_unref (children_menu);
				}

				nautilus_directory_unref (dir);
			}
			g_free (uri);
		} else if (nautilus_file_can_read (file)) {
			add_template_to_templates_menus (view, file, menu);
			any_templates = TRUE;
		}
	}

	nautilus_file_list_free (file_list);
	g_free (templates_directory_uri);

	if (!any_templates) {
		g_object_unref (menu);
		menu = NULL;
	}

	return menu;
}



static void
update_templates_menu (NautilusView *view)
{
	GList *sorted_copy, *node;
	NautilusDirectory *directory;
	GMenu *submenu;
	char *uri;
	char *templates_directory_uri;

	if (nautilus_should_use_templates_directory ()) {
		templates_directory_uri = nautilus_get_templates_directory_uri ();
	} else {
		view->details->templates_present = FALSE;
		return;
	}


	sorted_copy = nautilus_directory_list_sort_by_uri
		(nautilus_directory_list_copy (view->details->templates_directory_list));

	for (node = sorted_copy; node != NULL; node = node->next) {
		directory = node->data;

		uri = nautilus_directory_get_uri (directory);
		if (!directory_belongs_in_templates_menu (templates_directory_uri, uri)) {
			remove_directory_from_templates_directory_list (view, directory);
		}
		g_free (uri);
	}
	nautilus_directory_list_free (sorted_copy);

	directory = nautilus_directory_get_by_uri (templates_directory_uri);
	submenu = update_directory_in_templates_menu (view, directory);
	if (submenu != NULL) {
		nautilus_gmenu_merge (view->details->background_menu,
				      submenu,
				      "templates-submenu",
				      FALSE);
	}
	
	view->details->templates_present = submenu != NULL;

	g_free (templates_directory_uri);
}


static void
action_open_scripts_folder (GSimpleAction *action,
			    GVariant      *state,
                            gpointer       user_data)
{
	NautilusView *view;
	static GFile *location = NULL;

	if (location == NULL) {
		location = g_file_new_for_uri (scripts_directory_uri);
	}

	view = NAUTILUS_VIEW (user_data);
	nautilus_window_slot_open_location (view->details->slot, location, 0);
}

typedef struct _CopyCallbackData {
	NautilusView   *view;
	GtkFileChooser *chooser;
	GHashTable     *locations;
	GList          *selection;
	gboolean        is_move;
} CopyCallbackData;

static void
add_bookmark_for_uri (CopyCallbackData *data,
		      const char       *uri)
{
	GError *error = NULL;
	int count;

	count = GPOINTER_TO_INT (g_hash_table_lookup (data->locations, uri));
	if (count == 0) {
		gtk_file_chooser_add_shortcut_folder_uri (data->chooser,
							  uri,
							  &error);
		if (error != NULL) {
			DEBUG ("Unable to add location '%s' to file selector: %s", uri, error->message);
			g_clear_error (&error);
		}
	}
	g_hash_table_replace (data->locations, g_strdup (uri), GINT_TO_POINTER (count + 1));
}

static void
remove_bookmark_for_uri (CopyCallbackData *data,
			 const char       *uri)
{
	GError *error = NULL;
	int count;

	count = GPOINTER_TO_INT (g_hash_table_lookup (data->locations, uri));
	if (count == 1) {
		gtk_file_chooser_remove_shortcut_folder_uri (data->chooser,
							     uri,
							     &error);
		if (error != NULL) {
			DEBUG ("Unable to remove location '%s' to file selector: %s", uri, error->message);
			g_clear_error (&error);
		}
		g_hash_table_remove (data->locations, uri);
	} else {
		g_hash_table_replace (data->locations, g_strdup (uri), GINT_TO_POINTER (count - 1));
	}
}

static void
add_bookmarks_for_window_slot (CopyCallbackData   *data,
			       NautilusWindowSlot *slot)
{
	char *uri;

	uri = nautilus_window_slot_get_location_uri (slot);
	if (uri != NULL) {
		add_bookmark_for_uri (data, uri);
	}
	g_free (uri);
}

static void
remove_bookmarks_for_window_slot (CopyCallbackData   *data,
				  NautilusWindowSlot *slot)
{
	char *uri;

	uri = nautilus_window_slot_get_location_uri (slot);
	if (uri != NULL) {
		remove_bookmark_for_uri (data, uri);
	}
	g_free (uri);
}

static void
on_slot_location_changed (NautilusWindowSlot *slot,
			  const char         *from,
			  const char         *to,
			  CopyCallbackData   *data)
{
	if (from != NULL) {
		remove_bookmark_for_uri (data, from);
	}

	if (to != NULL) {
		add_bookmark_for_uri (data, to);
	}
}

static void
on_slot_added (NautilusWindow     *window,
	       NautilusWindowSlot *slot,
	       CopyCallbackData   *data)
{
	add_bookmarks_for_window_slot (data, slot);
	g_signal_connect (slot, "location-changed", G_CALLBACK (on_slot_location_changed), data);
}

static void
on_slot_removed (NautilusWindow     *window,
		 NautilusWindowSlot *slot,
		 CopyCallbackData   *data)
{
	remove_bookmarks_for_window_slot (data, slot);
	g_signal_handlers_disconnect_by_func (slot,
					      G_CALLBACK (on_slot_location_changed),
					      data);
}

static void
add_bookmarks_for_window (CopyCallbackData *data,
			  NautilusWindow   *window)
{
	GList *s;
	GList *slots;

	slots = nautilus_window_get_slots (window);
	for (s = slots; s != NULL; s = s->next) {
		NautilusWindowSlot *slot = s->data;
		add_bookmarks_for_window_slot (data, slot);
		g_signal_connect (slot, "location-changed", G_CALLBACK (on_slot_location_changed), data);
	}
	g_signal_connect (window, "slot-added", G_CALLBACK (on_slot_added), data);
	g_signal_connect (window, "slot-removed", G_CALLBACK (on_slot_removed), data);
}

static void
remove_bookmarks_for_window (CopyCallbackData *data,
			     NautilusWindow   *window)
{
	GList *s;
	GList *slots;

	slots = nautilus_window_get_slots (window);
	for (s = slots; s != NULL; s = s->next) {
		NautilusWindowSlot *slot = s->data;
		remove_bookmarks_for_window_slot (data, slot);
		g_signal_handlers_disconnect_by_func (slot,
						      G_CALLBACK (on_slot_location_changed),
						      data);
	}
	g_signal_handlers_disconnect_by_func (window,
					      G_CALLBACK (on_slot_added),
					      data);
	g_signal_handlers_disconnect_by_func (window,
					      G_CALLBACK (on_slot_removed),
					      data);
}

static void
on_app_window_added (GtkApplication   *application,
		     GtkWindow        *window,
		     CopyCallbackData *data)
{
	add_bookmarks_for_window (data, NAUTILUS_WINDOW (window));
}

static void
on_app_window_removed (GtkApplication   *application,
		       GtkWindow        *window,
		       CopyCallbackData *data)
{
	remove_bookmarks_for_window (data, NAUTILUS_WINDOW (window));
}

static void
copy_data_free (CopyCallbackData *data)
{
	NautilusApplication *application;
	GList *windows;
	GList *w;

	application = NAUTILUS_APPLICATION (g_application_get_default ());
	g_signal_handlers_disconnect_by_func (application,
					      G_CALLBACK (on_app_window_added),
					      data);
	g_signal_handlers_disconnect_by_func (application,
					      G_CALLBACK (on_app_window_removed),
					      data);

	windows = nautilus_application_get_windows (application);
	for (w = windows; w != NULL; w = w->next) {
		NautilusWindow *window = w->data;
		GList *slots;
		GList *s;

		slots = nautilus_window_get_slots (window);
		for (s = slots; s != NULL; s = s->next) {
			NautilusWindowSlot *slot = s->data;
			g_signal_handlers_disconnect_by_func (slot, G_CALLBACK (on_slot_location_changed), data);
		}
		g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_slot_added), data);
		g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_slot_removed), data);
	}

	nautilus_file_list_free (data->selection);
	g_hash_table_destroy (data->locations);
	g_free (data);
}

static gboolean
uri_is_parent_of_selection (GList *selection,
			    const char *uri)
{
	gboolean found;
	GList *l;
	GFile *file;

	found = FALSE;

	file = g_file_new_for_uri (uri);
	for (l = selection; !found && l != NULL; l = l->next) {
		GFile *parent;
		parent = nautilus_file_get_parent_location (l->data);
		found = g_file_equal (file, parent);
		g_object_unref (parent);
	}
	g_object_unref (file);
	return found;
}

static void
on_destination_dialog_folder_changed (GtkFileChooser *chooser,
				      gpointer        user_data)
{
	CopyCallbackData *copy_data = user_data;
	char *uri;
	gboolean found;

	uri = gtk_file_chooser_get_current_folder_uri (chooser);
	found = uri_is_parent_of_selection (copy_data->selection, uri);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_OK, !found);
	g_free (uri);
}

static void
on_destination_dialog_response (GtkDialog *dialog,
				gint       response_id,
				gpointer   user_data)
{
	CopyCallbackData *copy_data = user_data;

	if (response_id == GTK_RESPONSE_OK) {
		char *target_uri;
		GList *uris, *l;

		target_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

		uris = NULL;
		for (l = copy_data->selection; l != NULL; l = l->next) {
			uris = g_list_prepend (uris,
					       nautilus_file_get_uri ((NautilusFile *) l->data));
		}
		uris = g_list_reverse (uris);

		nautilus_view_move_copy_items (copy_data->view, uris, NULL, target_uri,
					       copy_data->is_move ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
					       0, 0);

		g_list_free_full (uris, g_free);
		g_free (target_uri);
	}

	copy_data_free (copy_data);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
destination_dialog_filter_cb (const GtkFileFilterInfo *filter_info,
			      gpointer                 user_data)
{
	GList *selection = user_data;
	GList *l;

	for (l = selection; l != NULL; l = l->next) {
		char *uri;
		uri = nautilus_file_get_uri (l->data);
		if (strcmp (uri, filter_info->uri) == 0) {
			g_free (uri);
			return FALSE;
		}
		g_free (uri);
	}

	return TRUE;
}

static GList *
get_selected_folders (GList *selection)
{
	GList *folders;
	GList *l;

	folders = NULL;
	for (l = selection; l != NULL; l = l->next) {
		if (nautilus_file_is_directory (l->data))
			folders = g_list_prepend (folders, nautilus_file_ref (l->data));
	}
	return g_list_reverse (folders);
}

static void
add_window_location_bookmarks (CopyCallbackData *data)
{
	NautilusApplication *application;
	GList *windows;
	GList *w;

	application = NAUTILUS_APPLICATION (g_application_get_default ());
	windows = nautilus_application_get_windows (application);
	g_signal_connect (application, "window-added", G_CALLBACK (on_app_window_added), data);
	g_signal_connect (application, "window-removed", G_CALLBACK (on_app_window_removed), data);

	for (w = windows; w != NULL; w = w->next) {
		NautilusWindow *window = w->data;
		add_bookmarks_for_window (data, window);
	}
}

static void
copy_or_move_selection (NautilusView *view,
			gboolean      is_move)
{
	GtkWidget *dialog;
	char *uri;
	CopyCallbackData *copy_data;
	GList *selection;
	const gchar *title;

	if (is_move) {
		title = _("Select Move Destination");
	} else {
		title = _("Select Copy Destination");
	}

	selection = nautilus_view_get_selection_for_file_transfer (view);

	dialog = gtk_file_chooser_dialog_new (title,
					      GTK_WINDOW (nautilus_view_get_window (view)),
					      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					      _("_Cancel"), GTK_RESPONSE_CANCEL,
					      _("_Select"), GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	copy_data = g_new0 (CopyCallbackData, 1);
	copy_data->view = view;
	copy_data->selection = selection;
	copy_data->is_move = is_move;
	copy_data->chooser = GTK_FILE_CHOOSER (dialog);
	copy_data->locations = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	add_window_location_bookmarks (copy_data);

	if (selection != NULL) {
		GtkFileFilter *filter;
		GList *folders;

		folders = get_selected_folders (selection);

		filter = gtk_file_filter_new ();
		gtk_file_filter_add_custom (filter,
					    GTK_FILE_FILTER_URI,
					    destination_dialog_filter_cb,
					    folders,
					    (GDestroyNotify)nautilus_file_list_free);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
	}

	uri = nautilus_directory_get_uri (view->details->model);
	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog), uri);
	g_free (uri);
	g_signal_connect (dialog, "current-folder-changed",
			  G_CALLBACK (on_destination_dialog_folder_changed),
			  copy_data);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (on_destination_dialog_response),
			  copy_data);

	gtk_widget_show_all (dialog);
}

static void
copy_or_cut_files (NautilusView *view,
		   GList           *clipboard_contents,
		   gboolean         cut)
{
	NautilusClipboardInfo info;
        GtkTargetList *target_list;
        GtkTargetEntry *targets;
        int n_targets;

	info.files = clipboard_contents;
	info.cut = cut;

        target_list = gtk_target_list_new (NULL, 0);
        gtk_target_list_add (target_list, copied_files_atom, 0, 0);
        gtk_target_list_add_uri_targets (target_list, 0);
        gtk_target_list_add_text_targets (target_list, 0);

        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_target_list_unref (target_list);

	gtk_clipboard_set_with_data (nautilus_clipboard_get (GTK_WIDGET (view)),
				     targets, n_targets,
				     nautilus_get_clipboard_callback, nautilus_clear_clipboard_callback,
				     NULL);
        gtk_target_table_free (targets, n_targets);

	nautilus_clipboard_monitor_set_clipboard_info (nautilus_clipboard_monitor_get (), &info);
}

static void
action_copy (GSimpleAction *action,
	     GVariant      *state,
	     gpointer       user_data)
{
	NautilusView *view;
	GList *selection;

	view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection_for_file_transfer (view);
	copy_or_cut_files (view, selection, FALSE);
	nautilus_file_list_free (selection);
}

static void
action_cut (GSimpleAction *action,
	    GVariant      *state,
	    gpointer       user_data)
{
	NautilusView *view;
	GList *selection;

	view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection_for_file_transfer (view);
	copy_or_cut_files (view, selection, TRUE);
	nautilus_file_list_free (selection);
}

static void
action_copy_to (GSimpleAction *action,
		GVariant      *state,
		gpointer       user_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (user_data);
	copy_or_move_selection (view, FALSE);
}

static void
action_move_to (GSimpleAction *action,
		GVariant      *state,
		gpointer       user_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (user_data);
	copy_or_move_selection (view, TRUE);
}

typedef struct {
	NautilusView *view;
	NautilusFile *target;
} PasteIntoData;

static void
paste_into_clipboard_received_callback (GtkClipboard     *clipboard,
					GtkSelectionData *selection_data,
					gpointer          callback_data)
{
	PasteIntoData *data;
	NautilusView *view;
	char *directory_uri;

	data = (PasteIntoData *) callback_data;

	view = NAUTILUS_VIEW (data->view);

	if (view->details->slot != NULL) {
		directory_uri = nautilus_file_get_activation_uri (data->target);

		paste_clipboard_data (view, selection_data, directory_uri);

		g_free (directory_uri);
	}

	g_object_unref (view);
	nautilus_file_unref (data->target);
	g_free (data);
}

static void
paste_into (NautilusView *view,
	    NautilusFile *target)
{
	PasteIntoData *data;

	g_assert (NAUTILUS_IS_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (target));

	data = g_new (PasteIntoData, 1);

	data->view = g_object_ref (view);
	data->target = nautilus_file_ref (target);

	gtk_clipboard_request_contents (nautilus_clipboard_get (GTK_WIDGET (view)),
					copied_files_atom,
					paste_into_clipboard_received_callback,
					data);
}

static void
action_paste_files_into (GSimpleAction *action,
			 GVariant      *state,
			 gpointer       user_data)
{
	NautilusView *view;
	GList *selection;

	view = NAUTILUS_VIEW (user_data);
	selection = nautilus_view_get_selection (view);
	if (selection != NULL) {
		paste_into (view, NAUTILUS_FILE (selection->data));
		nautilus_file_list_free (selection);
	}

}

static void
invoke_external_bulk_rename_utility (NautilusView *view,
				     GList *selection)
{
	GString *cmd;
	char *parameter;
	char *quoted_parameter;
	char *bulk_rename_tool;
	GList *walk;
	NautilusFile *file;

	/* assemble command line */
	bulk_rename_tool = get_bulk_rename_tool ();
	cmd = g_string_new (bulk_rename_tool);
	g_free (bulk_rename_tool);
	for (walk = selection; walk; walk = walk->next) {
		file = walk->data;
		parameter = nautilus_file_get_uri (file);
		quoted_parameter = g_shell_quote (parameter);
		g_free (parameter);
		cmd = g_string_append (cmd, " ");
		cmd = g_string_append (cmd, quoted_parameter);
		g_free (quoted_parameter);
	}

	/* spawning and error handling */
	nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (view)),
						  cmd->str, FALSE, NULL);
	g_string_free (cmd, TRUE);
}

static void
real_action_rename (NautilusView *view,
		    gboolean select_all)
{
	NautilusFile *file;
	GList *selection;

	g_assert (NAUTILUS_IS_VIEW (view));

	selection = nautilus_view_get_selection (view);

	if (selection_not_empty_in_menu_callback (view, selection)) {
		/* If there is more than one file selected, invoke a batch renamer */
		if (selection->next != NULL) {
			if (have_bulk_rename_tool ()) {
				invoke_external_bulk_rename_utility (view, selection);
			}
		} else {
			file = NAUTILUS_FILE (selection->data);
			if (!select_all) {
				/* directories don't have a file extension, so
				 * they are always pre-selected as a whole */
				select_all = nautilus_file_is_directory (file);
			}
			NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->start_renaming_file (view, file, select_all);
		}
	}

	nautilus_file_list_free (selection);
}

static void
action_rename (GSimpleAction *action,
	       GVariant      *state,
	       gpointer       user_data)
{
	real_action_rename (NAUTILUS_VIEW (user_data), TRUE);
}

#define BG_KEY_PRIMARY_COLOR      "primary-color"
#define BG_KEY_SECONDARY_COLOR    "secondary-color"
#define BG_KEY_COLOR_TYPE         "color-shading-type"
#define BG_KEY_PICTURE_PLACEMENT  "picture-options"
#define BG_KEY_PICTURE_URI        "picture-uri"

static void
set_uri_as_wallpaper (const char *uri)
{
	GSettings *settings;

	settings = gnome_background_preferences;

	g_settings_delay (settings);

	if (uri == NULL)
		uri = "";

	g_settings_set_string (settings, BG_KEY_PICTURE_URI, uri);
	g_settings_set_string (settings, BG_KEY_PRIMARY_COLOR, "#000000");
	g_settings_set_string (settings, BG_KEY_SECONDARY_COLOR, "#000000");
	g_settings_set_enum (settings, BG_KEY_COLOR_TYPE, G_DESKTOP_BACKGROUND_SHADING_SOLID);
	g_settings_set_enum (settings, BG_KEY_PICTURE_PLACEMENT, G_DESKTOP_BACKGROUND_STYLE_ZOOM);

	/* Apply changes atomically. */
	g_settings_apply (settings);
}

static void
wallpaper_copy_done_callback (GHashTable *debuting_files,
			      gboolean success,
			      gpointer data)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, debuting_files);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		char *uri;
		uri = g_file_get_uri (G_FILE (key));
		set_uri_as_wallpaper (uri);
		g_free (uri);
		break;
	}
}

static gboolean
can_set_wallpaper (GList *selection)
{
	NautilusFile *file;

	if (g_list_length (selection) != 1) {
		return FALSE;
	}

	file = NAUTILUS_FILE (selection->data);
	if (!nautilus_file_is_mime_type (file, "image/*")) {
		return FALSE;
	}

	/* FIXME: check file size? */

	return TRUE;
}

static void
action_set_as_wallpaper (GSimpleAction *action,
			 GVariant      *state,
			 gpointer       user_data)
{
	GList *selection;

	/* Copy the item to Pictures/Wallpaper since it may be
	   remote. Then set it as the current wallpaper. */

	g_assert (NAUTILUS_IS_VIEW (user_data));

	selection = nautilus_view_get_selection (user_data);

	if (can_set_wallpaper (selection)
	    && selection_not_empty_in_menu_callback (user_data, selection)) {
		NautilusFile *file;
		char *target_uri;
		GList *uris;
		GFile *parent;
		GFile *target;

		file = NAUTILUS_FILE (selection->data);

		parent = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
		target = g_file_get_child (parent, "Wallpapers");
		g_object_unref (parent);
		g_file_make_directory_with_parents (target, NULL, NULL);
		target_uri = g_file_get_uri (target);
		g_object_unref (target);
		uris = g_list_prepend (NULL, nautilus_file_get_uri (file));
		nautilus_file_operations_copy_move (uris,
						    NULL,
						    target_uri,
						    GDK_ACTION_COPY,
						    GTK_WIDGET (user_data),
						    wallpaper_copy_done_callback,
						    NULL);
		g_free (target_uri);
		g_list_free_full (uris, g_free);
	}

	nautilus_file_list_free (selection);
}

static void
file_mount_callback (NautilusFile  *file,
		     GFile         *result_location,
		     GError        *error,
		     gpointer       callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED &&
	      error->code != G_IO_ERROR_ALREADY_MOUNTED))) {
		char *text;
		char *name;
		name = nautilus_file_get_display_name (file);
		/* Translators: %s is a file name formatted for display */
		text = g_strdup_printf (_("Unable to access “%s”"), name);
		eel_show_error_dialog (text, error->message,
				       GTK_WINDOW (nautilus_view_get_window (view)));
		g_free (text);
		g_free (name);
	}
}

static void
file_unmount_callback (NautilusFile  *file,
		       GFile         *result_location,
		       GError        *error,
		       gpointer       callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);
	g_object_unref (view);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED))) {
		char *text;
		char *name;
		name = nautilus_file_get_display_name (file);
		/* Translators: %s is a file name formatted for display */
		text = g_strdup_printf (_("Unable to remove “%s”"), name);
		eel_show_error_dialog (text, error->message,
				       GTK_WINDOW (nautilus_view_get_window (view)));
		g_free (text);
		g_free (name);
	}
}

static void
file_eject_callback (NautilusFile  *file,
		     GFile         *result_location,
		     GError        *error,
		     gpointer       callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);
	g_object_unref (view);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED))) {
		char *text;
		char *name;
		name = nautilus_file_get_display_name (file);
		/* Translators: %s is a file name formatted for display */
		text = g_strdup_printf (_("Unable to eject “%s”"), name);
		eel_show_error_dialog (text, error->message,
				       GTK_WINDOW (nautilus_view_get_window (view)));
		g_free (text);
		g_free (name);
	}
}

static void
file_stop_callback (NautilusFile  *file,
		    GFile         *result_location,
		    GError        *error,
		    gpointer       callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED))) {
		eel_show_error_dialog (_("Unable to stop drive"),
				       error->message,
				       GTK_WINDOW (nautilus_view_get_window (view)));
	}
}

static void
action_mount_volume (GSimpleAction *action,
		     GVariant      *state,
		     gpointer       user_data)
{
	NautilusFile *file;
	GList *selection, *l;
	NautilusView *view;
	GMountOperation *mount_op;

        view = NAUTILUS_VIEW (user_data);
	
	selection = nautilus_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		
		if (nautilus_file_can_mount (file)) {
			mount_op = gtk_mount_operation_new (nautilus_view_get_containing_window (view));
			g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
			nautilus_file_mount (file, mount_op, NULL,
					     file_mount_callback,
					     view);
			g_object_unref (mount_op);
		}
	}
	nautilus_file_list_free (selection);
}

static void
action_unmount_volume (GSimpleAction *action,
		       GVariant      *state,
		       gpointer       user_data)
{
	NautilusFile *file;
	GList *selection, *l;
	NautilusView *view;

        view = NAUTILUS_VIEW (user_data);
	
	selection = nautilus_view_get_selection (view);

	for (l = selection; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		if (nautilus_file_can_unmount (file)) {
			GMountOperation *mount_op;
			mount_op = gtk_mount_operation_new (nautilus_view_get_containing_window (view));
			nautilus_file_unmount (file, mount_op, NULL,
					       file_unmount_callback, g_object_ref (view));
			g_object_unref (mount_op);
		}
	}
	nautilus_file_list_free (selection);
}

static void
action_eject_volume (GSimpleAction *action,
		     GVariant      *state,
		     gpointer       user_data)
{
	NautilusFile *file;
	GList *selection, *l;
	NautilusView *view;

        view = NAUTILUS_VIEW (user_data);
	
	selection = nautilus_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		
		if (nautilus_file_can_eject (file)) {
			GMountOperation *mount_op;
			mount_op = gtk_mount_operation_new (nautilus_view_get_containing_window (view));
			nautilus_file_eject (file, mount_op, NULL,
					     file_eject_callback, g_object_ref (view));
			g_object_unref (mount_op);
		}
	}	
	nautilus_file_list_free (selection);
}

static void
file_start_callback (NautilusFile  *file,
		     GFile         *result_location,
		     GError        *error,
		     gpointer       callback_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (callback_data);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED &&
	      error->code != G_IO_ERROR_ALREADY_MOUNTED))) {
		char *text;
		char *name;
		name = nautilus_file_get_display_name (file);
		/* Translators: %s is a file name formatted for display */
		text = g_strdup_printf (_("Unable to start “%s”"), name);
		eel_show_error_dialog (text, error->message,
				       GTK_WINDOW (nautilus_view_get_window (view)));
		g_free (text);
		g_free (name);
	}
}

static void
action_start_volume (GSimpleAction *action,
		     GVariant      *state,
		     gpointer       user_data)
{
	NautilusFile *file;
	GList *selection, *l;
	NautilusView *view;
	GMountOperation *mount_op;

	view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (nautilus_file_can_start (file) || nautilus_file_can_start_degraded (file)) {
			mount_op = gtk_mount_operation_new (nautilus_view_get_containing_window (view));
			nautilus_file_start (file, mount_op, NULL,
					     file_start_callback, view);
			g_object_unref (mount_op);
		}
	}
	nautilus_file_list_free (selection);
}

static void
action_stop_volume (GSimpleAction *action,
		    GVariant      *state,
		    gpointer       user_data)
{
	NautilusFile *file;
	GList *selection, *l;
	NautilusView *view;

        view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (nautilus_file_can_stop (file)) {
			GMountOperation *mount_op;
			mount_op = gtk_mount_operation_new (nautilus_view_get_containing_window (view));
			nautilus_file_stop (file, mount_op, NULL,
					    file_stop_callback, view);
			g_object_unref (mount_op);
		}
	}
	nautilus_file_list_free (selection);
}

static void
action_detect_media (GSimpleAction *action,
		     GVariant      *state,
		     gpointer       user_data)
{
	NautilusFile *file;
	GList *selection, *l;
	NautilusView *view;

        view = NAUTILUS_VIEW (user_data);

	selection = nautilus_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (nautilus_file_can_poll_for_media (file) && !nautilus_file_is_media_check_automatic (file)) {
			nautilus_file_poll_for_media (file);
		}
	}
	nautilus_file_list_free (selection);
}

gboolean
nautilus_view_get_show_hidden_files (NautilusView *view)
{
	return view->details->show_hidden_files;
}

const GActionEntry view_entries[] = {
	/* Toolbar menu */
	{ "zoom-in",  action_zoom_in },
	{ "zoom-out", action_zoom_out },
	{ "zoom-default", action_zoom_default },
	{ "undo", action_undo },
	{ "redo", action_redo },
	{ "show-hidden-files", NULL, NULL, "true", action_show_hidden_files },
	/* Background menu */
	{ "new-folder", action_new_folder },
	{ "select-all", action_select_all },
	{ "paste", action_paste_files },
	{ "new-document" },
	/* Selection menu */
	{ "scripts" },
	{ "new-folder-with-selection", action_new_folder_with_selection },
	{ "open-scripts-folder", action_open_scripts_folder },
	{ "open-item-location", action_open_item_location },
	{ "open-with-default-application", action_open_with_default_application },
	{ "open-with-other-application", action_open_with_other_application },
	{ "open-item-new-window", action_open_item_new_window },
	{ "open-item-new-tab", action_open_item_new_tab },
	{ "cut", action_cut},
	{ "copy", action_copy},
	{ "move-to", action_move_to},
	{ "copy-to", action_copy_to},
	{ "move-to-trash", action_move_to_trash},
	{ "delete", action_delete},
	{ "restore-from-trash", action_restore_from_trash},
	{ "paste-into", action_paste_files_into },
	{ "rename", action_rename},
	{ "properties", action_properties},
	{ "set-as-wallpaper", action_set_as_wallpaper },
	{ "mount-volume", action_mount_volume },
	{ "unmount-volume", action_unmount_volume },
	{ "eject-volume", action_eject_volume },
	{ "start-volume", action_start_volume },
	{ "stop-volume", action_stop_volume },
	{ "detect-media", action_detect_media },
	/* Pathbar menu */
	{ "pathbar-open-item-new-window", action_pathbar_open_item_new_window },
	{ "pathbar-open-item-new-tab", action_pathbar_open_item_new_tab },
	{ "pathbar-properties", action_pathbar_properties},
	/* Only accesible by shorcuts */
	{ "select-pattern", action_select_pattern },
	{ "invert-selection", action_invert_selection },
	{ "open-file-and-close-window", action_open_file_and_close_window }
};

static gboolean
can_paste_into_file (NautilusFile *file)
{
	if (nautilus_file_is_directory (file) &&
	    nautilus_file_can_write (file)) {
		return TRUE;
	}
	if (nautilus_file_has_activation_uri (file)) {
		GFile *location;
		NautilusFile *activation_file;
		gboolean res;
		
		location = nautilus_file_get_activation_location (file);
		activation_file = nautilus_file_get (location);
		g_object_unref (location);
	
		/* The target location might not have data for it read yet,
		   and we can't want to do sync I/O, so treat the unknown
		   case as can-write */
		res = (nautilus_file_get_file_type (activation_file) == G_FILE_TYPE_UNKNOWN) ||
			(nautilus_file_get_file_type (activation_file) == G_FILE_TYPE_DIRECTORY &&
			 nautilus_file_can_write (activation_file));

		nautilus_file_unref (activation_file);
		
		return res;
	}
	
	return FALSE;
}

static void
clipboard_targets_received (GtkClipboard     *clipboard,
                            GdkAtom          *targets,
                            int               n_targets,
			    gpointer          user_data)
{
	NautilusView *view;
	gboolean can_paste;
	int i;
	GAction *action;

	view = NAUTILUS_VIEW (user_data);
	can_paste = FALSE;

	if (view->details->slot == NULL ||
	    !view->details->active) {
		/* We've been destroyed or became inactive since call */
		g_object_unref (view);
		return;
	}

	if (targets) {
		for (i = 0; i < n_targets; i++) {
			if (targets[i] == copied_files_atom) {
				can_paste = TRUE;
			}
		}
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (view->details->view_action_group),
					     "paste");
	/* Take into account if the action was previously disabled for other reasons,
	 * like the directory not being writabble */
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_paste && g_action_get_enabled (action));

	action = g_action_map_lookup_action (G_ACTION_MAP (view->details->view_action_group),
					     "paste-into");

	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_paste && g_action_get_enabled (action));
	
	g_object_unref (view);
}

static void
file_should_show_foreach (NautilusFile        *file,
			  gboolean            *show_mount,
			  gboolean            *show_unmount,
			  gboolean            *show_eject,
			  gboolean            *show_start,
			  gboolean            *show_stop,
			  gboolean            *show_poll,
			  GDriveStartStopType *start_stop_type)
{
	*show_mount = FALSE;
	*show_unmount = FALSE;
	*show_eject = FALSE;
	*show_start = FALSE;
	*show_stop = FALSE;
	*show_poll = FALSE;

	if (nautilus_file_can_eject (file)) {
		*show_eject = TRUE;
	}

	if (nautilus_file_can_mount (file)) {
		*show_mount = TRUE;
	}

	if (nautilus_file_can_start (file) || nautilus_file_can_start_degraded (file)) {
		*show_start = TRUE;
	}

	if (nautilus_file_can_stop (file)) {
		*show_stop = TRUE;
	}

	/* Dot not show both Unmount and Eject/Safe Removal; too confusing to
	 * have too many menu entries */
	if (nautilus_file_can_unmount (file) && !*show_eject && !*show_stop) {
		*show_unmount = TRUE;
	}

	if (nautilus_file_can_poll_for_media (file) && !nautilus_file_is_media_check_automatic (file)) {
		*show_poll = TRUE;
	}

	*start_stop_type = nautilus_file_get_start_stop_type (file);
}

static gboolean
can_restore_from_trash (GList *files)
{
	NautilusFile *original_file;
	NautilusFile *original_dir;
	GHashTable *original_dirs_hash;
	GList *original_dirs;
	gboolean can_restore;

	original_file = NULL;
	original_dir = NULL;
	original_dirs = NULL;
	original_dirs_hash = NULL;

	if (files != NULL) {
		if (g_list_length (files) == 1) {
			original_file = nautilus_file_get_trash_original_file (files->data);
		} else {
			original_dirs_hash = nautilus_trashed_files_get_original_directories (files, NULL);
			if (original_dirs_hash != NULL) {
				original_dirs = g_hash_table_get_keys (original_dirs_hash);
				if (g_list_length (original_dirs) == 1) {
					original_dir = nautilus_file_ref (NAUTILUS_FILE (original_dirs->data));
				}
			}
		}
	}

	can_restore = original_file != NULL || original_dirs != NULL;

	nautilus_file_unref (original_file);
	nautilus_file_unref (original_dir);
	g_list_free (original_dirs);

	if (original_dirs_hash != NULL) {
		g_hash_table_destroy (original_dirs_hash);
	}
	return can_restore;
}

static void
clipboard_changed_callback (NautilusClipboardMonitor *monitor, NautilusView *view)
{
	/* Update paste menu item */
	nautilus_view_update_context_menus (view);
}

static gboolean
can_delete_all (GList *files)
{
	NautilusFile *file;
	GList *l;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;
		if (!nautilus_file_can_delete (file)) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
can_trash_all (GList *files)
{
	NautilusFile *file;
	GList *l;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;
		if (!nautilus_file_can_trash (file)) {
			return FALSE;
		}
	}
	return TRUE;
}

GActionGroup *
nautilus_view_get_action_group (NautilusView *view)
{
	g_assert (NAUTILUS_IS_VIEW (view));

	return view->details->view_action_group;
}

static void
real_update_actions_state (NautilusView *view)
{
	GList *selection, *l;
	NautilusFile *file;
	gint selection_count;
	gboolean selection_contains_special_link;
	gboolean selection_contains_desktop_or_home_dir;
	gboolean selection_contains_recent;
	gboolean selection_contains_search;
	gboolean selection_is_read_only;
	gboolean can_create_files;
	gboolean can_delete_files;
	gboolean can_move_files;
	gboolean can_trash_files;
	gboolean can_copy_files;
	gboolean can_paste_files_into;
	gboolean show_separate_delete_command;
	gboolean show_app, show_run;
	gboolean item_opens_in_view;
	gboolean is_read_only;
	GAction *action;
	GAppInfo *app;
	gboolean show_properties;
	GActionGroup *view_action_group;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_start;
	gboolean show_stop;
	gboolean show_detect_media;
	GDriveStartStopType start_stop_type;
	NautilusFileUndoInfo *info;
	NautilusFileUndoManagerState undo_state;
	gboolean undo_active, redo_active;
	gboolean is_undo;

	view_action_group = view->details->view_action_group;

	selection = nautilus_view_get_selection (view);
	selection_count = g_list_length (selection);
	selection_contains_special_link = special_link_in_selection (selection);
	selection_contains_desktop_or_home_dir = desktop_or_home_dir_in_selection (selection);
	selection_contains_recent = showing_recent_directory (view);
	selection_contains_search = view->details->model &&
		NAUTILUS_IS_SEARCH_DIRECTORY (view->details->model);
	selection_is_read_only = selection_count == 1 &&
		(!nautilus_file_can_write (NAUTILUS_FILE (selection->data)) &&
		 !nautilus_file_has_activation_uri (NAUTILUS_FILE (selection->data)));

	is_read_only = nautilus_view_is_read_only (view);
	can_create_files = nautilus_view_supports_creating_files (view);
	can_delete_files =
		can_delete_all (selection) &&
		selection_count != 0 &&
		!selection_contains_special_link &&
		!selection_contains_desktop_or_home_dir;
	can_trash_files =
		can_trash_all (selection) &&
		selection_count != 0 &&
		!selection_contains_special_link &&
		!selection_contains_desktop_or_home_dir;
	can_copy_files = selection_count != 0
		&& !selection_contains_special_link;
	can_move_files = can_delete_files && !selection_contains_recent;
	can_paste_files_into = (!selection_contains_recent &&
				selection_count == 1 &&
	                        can_paste_into_file (NAUTILUS_FILE (selection->data)));
	show_properties = !showing_network_directory (view) &&
			  (!NAUTILUS_IS_DESKTOP_CANVAS_VIEW (view) || selection_count > 0);

	/* Right click actions */
	/* Selection menu actions */
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "new-folder-with-selection");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_create_files && can_delete_files && (selection_count > 1) && !selection_contains_recent);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "rename");
	if (selection_count > 1) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
					     have_bulk_rename_tool ());
	} else {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
					     selection_count == 1 &&
					     nautilus_view_can_rename_file (view, selection->data));
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "open-item-location");

	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     selection_count == 1 &&
				     (selection_contains_recent || selection_contains_search));

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "new-folder");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_create_files);


	selection = nautilus_view_get_selection (view);
	selection_count = g_list_length (selection);

	show_app = show_run = item_opens_in_view = selection_count != 0;

	for (l = selection; l != NULL; l = l->next) {
		NautilusFile *file;

		file = NAUTILUS_FILE (selection->data);

		if (!nautilus_mime_file_opens_in_external_app (file)) {
			show_app = FALSE;
		}

		if (!nautilus_mime_file_launches (file)) {
			show_run = FALSE;
		}

		if (!nautilus_mime_file_opens_in_view (file)) {
			item_opens_in_view = FALSE;
		}

		if (!show_app && !show_run && !item_opens_in_view) {
			break;
		}
	}

	/* Open With <App> menu item */
	app = NULL;
	if (show_app) {
		app = nautilus_mime_get_default_application_for_files (selection);
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "open-with-default-application");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), selection_count != 0);

	/* Allow to select a different application to open the item */
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "open-with-other-application");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), app != NULL);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "open-item-new-tab");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), item_opens_in_view);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "open-item-new-window");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), item_opens_in_view);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "set-as-wallpaper");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), 	can_set_wallpaper (selection));
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "restore-from-trash");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_restore_from_trash (selection));

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "move-to-trash");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_trash_files);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "delete");
	/* Only show it in trash folder or if the setting to include a delete
	 * menu item is enabled */
	show_separate_delete_command = g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ENABLE_DELETE);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_delete_files &&
				     (!can_trash_files || show_separate_delete_command));

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "cut");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_move_files && !selection_contains_recent);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "copy");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_copy_files);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "copy-to");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_copy_files);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "move-to");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_move_files && !selection_contains_recent);

	/* Drive menu */
	show_mount = show_unmount = show_eject = show_start = show_stop = show_detect_media = FALSE;
	for (l = selection; l != NULL && (show_mount || show_unmount
					  || show_eject
                                          || show_start || show_stop
					  || show_detect_media);
	     l = l->next) {
		gboolean show_mount_one;
		gboolean show_unmount_one;
		gboolean show_eject_one;
		gboolean show_start_one;
		gboolean show_stop_one;
		gboolean show_detect_media_one;

		file = NAUTILUS_FILE (l->data);
		file_should_show_foreach (file,
					  &show_mount_one,
					  &show_unmount_one,
					  &show_eject_one,
                                          &show_start_one,
                                          &show_stop_one,
					  &show_detect_media_one,
					  &start_stop_type);

		show_mount &= show_mount_one;
		show_unmount &= show_unmount_one;
		show_eject &= show_eject_one;
		show_start &= show_start_one;
		show_stop &= show_stop_one;
		show_detect_media &= show_detect_media_one;
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "mount-volume");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_mount);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "unmount-volume");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_unmount);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "eject-volume");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_eject);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "start-volume");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_start);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "stop-volume");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_stop);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "detect-media");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_detect_media);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "scripts");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     view->details->scripts_present);

	/* Background menu actions */
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "new-folder");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_create_files);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "paste");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     !is_read_only && !selection_contains_recent);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "paste-into");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     !selection_is_read_only && !selection_contains_recent &&
				     can_paste_files_into);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "properties");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     show_properties);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "new-document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     can_create_files &&
				     !selection_contains_recent &&
				     view->details->templates_present);

	/* Ask the clipboard */
	g_object_ref (view); /* Need to keep the object alive until we get the reply */
	gtk_clipboard_request_targets (nautilus_clipboard_get (GTK_WIDGET (view)),
				       clipboard_targets_received,
				       view);

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "select-all");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     !nautilus_view_is_empty (view));

	/* Toolbar menu actions */
	/* Undo and Redo */
	info = nautilus_file_undo_manager_get_action ();
	undo_state = nautilus_file_undo_manager_get_state ();
	undo_active = redo_active = FALSE;
	if (info != NULL &&
	    (undo_state > NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE)) {
		is_undo = (undo_state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO);
		undo_active = is_undo;
		redo_active = !is_undo;
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "undo");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), undo_active);
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "redo");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), redo_active);

	g_action_group_change_action_state (view_action_group,
					    "show-hidden-files",
					    g_variant_new_boolean (view->details->show_hidden_files));

	/* Zoom */
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "zoom-in");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     nautilus_view_can_zoom_in (view));
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "zoom-out");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     nautilus_view_can_zoom_out (view));
	action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
					     "zoom-default");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     nautilus_view_supports_zooming (view));
}

/* Convenience function to be called when updating menus,
 * so children can subclass it and it will be called when
 * they chain up to the parent in update_context_menus
 * or update_toolbar_menus
 */
void
nautilus_view_update_actions_state (NautilusView *view)
{
	g_assert(NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_actions_state (view);
}

static void
update_selection_menu (NautilusView *view)
{
	GList *selection, *l;
	NautilusFile *file;
	gint selection_count;
	gboolean show_app, show_run;
	gboolean item_opens_in_view;
	gchar *item_label;
	GAppInfo *app;
	GIcon *app_icon;
	GMenuItem *menu_item;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_start;
	gboolean show_stop;
	gboolean show_detect_media;
	GDriveStartStopType start_stop_type;

	selection = nautilus_view_get_selection (view);
	selection_count = g_list_length (selection);

	show_mount = (selection != NULL);
	show_unmount = (selection != NULL);
	show_eject = (selection != NULL);
	show_start = (selection != NULL && selection_count == 1);
	show_stop = (selection != NULL && selection_count == 1);
	show_detect_media = (selection != NULL && selection_count == 1);

	item_label = g_strdup_printf (_("New Folder with Selection (%'d Items)"),
				      selection_count);
	menu_item = g_menu_item_new (item_label, "view.new-folder-with-selection");
	g_menu_item_set_attribute (menu_item, "hidden-when", "s", "action-disabled");
	nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
					     menu_item,
					     "new-folder-with-selection-section",
					     FALSE);
	g_object_unref (menu_item);
	g_free (item_label);

	/* Open With <App> menu item */
	show_app = show_run = item_opens_in_view = selection_count != 0;
	for (l = selection; l != NULL; l = l->next) {
		NautilusFile *file;

		file = NAUTILUS_FILE (selection->data);

		if (!nautilus_mime_file_opens_in_external_app (file)) {
			show_app = FALSE;
		}

		if (!nautilus_mime_file_launches (file)) {
			show_run = FALSE;
		}

		if (!nautilus_mime_file_opens_in_view (file)) {
			item_opens_in_view = FALSE;
		}

		if (!show_app && !show_run && !item_opens_in_view) {
			break;
		}
	}

	item_label = NULL;
	app = NULL;
	app_icon = NULL;
	if (show_app) {
		app = nautilus_mime_get_default_application_for_files (selection);
	}

	char *escaped_app;

	if (app != NULL) {
		escaped_app = eel_str_double_underscores (g_app_info_get_name (app));
		item_label = g_strdup_printf (_("Open With %s"), escaped_app);

		app_icon = g_app_info_get_icon (app);
		if (app_icon != NULL) {
			g_object_ref (app_icon);
		}
		g_free (escaped_app);
		g_object_unref (app);
	} else if (show_run) {
		item_label = g_strdup (_("Run"));
	} else {
		item_label = g_strdup (_("Open"));
	}

	menu_item = g_menu_item_new (item_label, "view.open-with-default-application");
	if (app_icon != NULL)
		g_menu_item_set_icon (menu_item, app_icon);

	nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
					     menu_item,
					     "open-with-default-application-section",
					     FALSE);

	g_free (item_label);
	g_object_unref (menu_item);

	/* Drives */
	for (l = selection; l != NULL && (show_mount || show_unmount
					  || show_eject
                                          || show_start || show_stop
					  || show_detect_media);
	     l = l->next) {
		gboolean show_mount_one;
		gboolean show_unmount_one;
		gboolean show_eject_one;
		gboolean show_start_one;
		gboolean show_stop_one;
		gboolean show_detect_media_one;

		file = NAUTILUS_FILE (l->data);
		file_should_show_foreach (file,
					  &show_mount_one,
					  &show_unmount_one,
					  &show_eject_one,
                                          &show_start_one,
                                          &show_stop_one,
					  &show_detect_media_one,
					  &start_stop_type);

		show_mount &= show_mount_one;
		show_unmount &= show_unmount_one;
		show_eject &= show_eject_one;
		show_start &= show_start_one;
		show_stop &= show_stop_one;
		show_detect_media &= show_detect_media_one;
	}

	if (show_start) {
		switch (start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			item_label = _("_Start");
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			item_label = _("_Connect");
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			item_label = _("_Start Multi-disk Drive");
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			item_label = _("U_nlock Drive");
			break;
		}

		menu_item = g_menu_item_new (item_label, "view.start-volume");
		nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
						     menu_item,
						     "drive-section",
						     FALSE);
		g_object_unref (menu_item);
	}

	if (show_stop) {
		switch (start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			item_label = _("Stop Drive");
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			item_label = _("_Safely Remove Drive");
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			item_label = _("_Disconnect");
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			item_label = _("_Stop Multi-disk Drive");
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			item_label = _("_Lock Drive");
			break;
		}

		menu_item = g_menu_item_new (item_label, "view.stop-volume");
		nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
						     menu_item,
						     "drive-section",
						     FALSE);
		g_object_unref (menu_item);
	}

	update_scripts_menu (view);
}

static void
update_background_menu (NautilusView *view)
{

	if (nautilus_view_supports_creating_files (view) &&
	    !showing_recent_directory (view))
		update_templates_menu (view);
}

static void
real_update_context_menus (NautilusView *view)
{
	g_clear_object (&view->details->background_menu);
	g_clear_object (&view->details->selection_menu);
	g_clear_object (&view->details->pathbar_menu);

	GtkBuilder *builder;
	builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/nautilus-view-context-menus.xml");
	view->details->background_menu = g_object_ref (G_MENU (gtk_builder_get_object (builder, "background-menu")));
	view->details->selection_menu = g_object_ref (G_MENU (gtk_builder_get_object (builder, "selection-menu")));
	view->details->pathbar_menu = g_object_ref (G_MENU (gtk_builder_get_object (builder, "pathbar-menu")));
	g_object_unref (builder);

	update_selection_menu (view);
	update_background_menu (view);
	update_extensions_menus (view);

	nautilus_view_update_actions_state (view);
}

/* Convenience function to reset the context menus owned by the view and update
 * them with the current state.
 * Children can subclass it and add items on the menu after chaining up to the
 * parent, so menus are already reseted.
 * It will also update the actions state, which will also update children
 * actions state if the children subclass nautilus_view_update_actions_state
 */
void
nautilus_view_update_context_menus (NautilusView *view)
{
	g_assert(NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_context_menus (view);
}

static void
real_update_toolbar_menus (NautilusView *view)
{
	NautilusToolbar *toolbar;
	NautilusWindow *window;
	NautilusFileUndoInfo *info;
	NautilusFileUndoManagerState undo_state;
	gboolean undo_active, redo_active;
	gchar *undo_label, *undo_description, *redo_label, *redo_description;
	GMenuItem *undo_menu_item, *redo_menu_item;
	gboolean is_undo;

	undo_label = undo_description = redo_label = redo_description = NULL;

	toolbar = NAUTILUS_TOOLBAR (nautilus_window_get_toolbar (nautilus_view_get_window (view)));
	window = nautilus_view_get_window (view);
	nautilus_toolbar_reset_menus (toolbar);
	nautilus_window_reset_menus (window);

	/* Undo and Redo */
	info = nautilus_file_undo_manager_get_action ();
	undo_state = nautilus_file_undo_manager_get_state ();
	undo_active = redo_active = FALSE;
	if (info != NULL &&
	    (undo_state > NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE)) {
		is_undo = (undo_state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO);
		undo_active = is_undo;
		redo_active = !is_undo;
		nautilus_file_undo_info_get_strings (info,
						     &undo_label, &undo_description,
						     &redo_label, &redo_description);
	}

	undo_label = undo_active ? undo_label : _("Undo");
	redo_label = redo_active ? redo_label : _("Redo");
	undo_menu_item = g_menu_item_new (undo_label, "view.undo");
	redo_menu_item = g_menu_item_new (redo_label, "view.redo");
	nautilus_toolbar_action_menu_add_item (toolbar, undo_menu_item, "undo-redo-section");
	nautilus_toolbar_action_menu_add_item (toolbar, redo_menu_item, "undo-redo-section");

	nautilus_view_update_actions_state (view);

	g_object_unref (undo_menu_item);
	g_object_unref (redo_menu_item);
}

/* Convenience function to reset the menus owned by the but that are managed on
 * the toolbar and update them with the current state.
 * Children can subclass it and add items on the menu after chaining up to the
 * parent, so menus are already reseted.
 * It will also update the actions state, which will also update children
 * actions state if the children subclass nautilus_view_update_actions_state
 */
void
nautilus_view_update_toolbar_menus (NautilusView *view)
{
	g_assert(NAUTILUS_IS_VIEW (view));

	NAUTILUS_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_toolbar_menus (view);
}

/**
 * nautilus_view_pop_up_selection_context_menu
 *
 * Pop up a context menu appropriate to the selected items.
 * @view: NautilusView of interest.
 * @event: The event that triggered this context menu.
 * 
 **/
void 
nautilus_view_pop_up_selection_context_menu  (NautilusView *view, 
					      GdkEventButton  *event)
{
	g_assert (NAUTILUS_IS_VIEW (view));

	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_context_menus_if_pending (view);

	update_context_menu_position_from_event (view, event);

	nautilus_pop_up_context_menu (GTK_WIDGET (view), view->details->selection_menu, event);
}

/**
 * nautilus_view_pop_up_background_context_menu
 *
 * Pop up a context menu appropriate to the view globally at the last right click location.
 * @view: NautilusView of interest.
 *
 **/
void 
nautilus_view_pop_up_background_context_menu (NautilusView *view, 
					      GdkEventButton  *event)
{
	g_assert (NAUTILUS_IS_VIEW (view));

	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_context_menus_if_pending (view);

	update_context_menu_position_from_event (view, event);

	nautilus_pop_up_context_menu (GTK_WIDGET (view), view->details->background_menu, event);
}

static void
real_pop_up_pathbar_context_menu (NautilusView *view)
{
	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_context_menus_if_pending (view);

	update_context_menu_position_from_event (view, view->details->pathbar_popup_event);

	nautilus_pop_up_context_menu (GTK_WIDGET (view), view->details->pathbar_menu, view->details->pathbar_popup_event);
}

static void
pathbar_popup_file_attributes_ready (NautilusFile *file,
				     gpointer      data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (data);
	g_assert (NAUTILUS_IS_VIEW (view));

	g_assert (file == view->details->pathbar_popup_directory_as_file);

	real_pop_up_pathbar_context_menu (view);
}

static void
unschedule_pop_up_pathbar_context_menu (NautilusView *view)
{
	if (view->details->pathbar_popup_directory_as_file != NULL) {
		g_assert (NAUTILUS_IS_FILE (view->details->pathbar_popup_directory_as_file));
		nautilus_file_cancel_call_when_ready (view->details->pathbar_popup_directory_as_file,
						      pathbar_popup_file_attributes_ready,
						      view);
		nautilus_file_unref (view->details->pathbar_popup_directory_as_file);
		view->details->pathbar_popup_directory_as_file = NULL;
	}
}

static void
schedule_pop_up_pathbar_context_menu (NautilusView *view,
				      GdkEventButton  *event,
				      NautilusFile    *file)
{
	g_assert (NAUTILUS_IS_FILE (file));

	if (view->details->pathbar_popup_event != NULL) {
		gdk_event_free ((GdkEvent *) view->details->pathbar_popup_event);
	}
	view->details->pathbar_popup_event = (GdkEventButton *) gdk_event_copy ((GdkEvent *)event);

	if (file == view->details->pathbar_popup_directory_as_file) {
		if (nautilus_file_check_if_ready (file, NAUTILUS_FILE_ATTRIBUTE_INFO |
						  NAUTILUS_FILE_ATTRIBUTE_MOUNT |
						  NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO)) {
			real_pop_up_pathbar_context_menu (view);
		}
	} else {
		unschedule_pop_up_pathbar_context_menu (view);

		view->details->pathbar_popup_directory_as_file = nautilus_file_ref (file);
		nautilus_file_call_when_ready (view->details->pathbar_popup_directory_as_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT |
					       NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO,
					       pathbar_popup_file_attributes_ready,
					       view);
	}
}

/**
 * nautilus_view_pop_up_pathbar_context_menu
 *
 * Pop up a context menu appropriate to the view globally.
 * @view: NautilusView of interest.
 * @event: GdkEventButton triggering the popup.
 * @location: The location the popup-menu should be created for,
 * or NULL for the currently displayed location.
 *
 **/
void 
nautilus_view_pop_up_pathbar_context_menu (NautilusView *view, 
					   GdkEventButton  *event,
					   const char      *location)
{
	NautilusFile *file;

	g_assert (NAUTILUS_IS_VIEW (view));

	if (location != NULL) {
		file = nautilus_file_get_by_uri (location);
	} else {
		file = nautilus_file_ref (view->details->directory_as_file);
	}

	if (file != NULL) {
		schedule_pop_up_pathbar_context_menu (view, event, file);
		nautilus_file_unref (file);
	}
}

static void
schedule_update_context_menus (NautilusView *view) 
{
	g_assert (NAUTILUS_IS_VIEW (view));

	/* Don't schedule updates after destroy (#349551),
 	 * or if we are not active.
	 */
	if (view->details->slot == NULL ||
	    !view->details->active) {
		return;
	}
	
	/* Schedule a menu update with the current update interval */
	if (view->details->update_context_menus_timeout_id == 0) {
		view->details->update_context_menus_timeout_id
			= g_timeout_add (view->details->update_interval, update_context_menus_timeout_callback, view);
	}
}

static void
remove_update_status_idle_callback (NautilusView *view) 
{
	if (view->details->update_status_idle_id != 0) {
		g_source_remove (view->details->update_status_idle_id);
		view->details->update_status_idle_id = 0;
	}
}

static gboolean
update_status_idle_callback (gpointer data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (data);
	nautilus_view_display_selection_info (view);
	view->details->update_status_idle_id = 0;
	return FALSE;
}

static void
schedule_update_status (NautilusView *view) 
{
	g_assert (NAUTILUS_IS_VIEW (view));

	/* Make sure we haven't already destroyed it */
	if (view->details->slot == NULL) {
		return;
	}

	if (view->details->loading) {
		/* Don't update status bar while loading the dir */
		return;
	}

	if (view->details->update_status_idle_id == 0) {
		view->details->update_status_idle_id =
			g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
					 update_status_idle_callback, view, NULL);
	}
}

/**
 * nautilus_view_notify_selection_changed:
 * 
 * Notify this view that the selection has changed. This is normally
 * called only by subclasses.
 * @view: NautilusView whose selection has changed.
 * 
 **/
void
nautilus_view_notify_selection_changed (NautilusView *view)
{
	GtkWindow *window;
	GList *selection;
	
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	selection = nautilus_view_get_selection (view);
	window = nautilus_view_get_containing_window (view);
	DEBUG_FILES (selection, "Selection changed in window %p", window);
	nautilus_file_list_free (selection);

	view->details->selection_was_removed = FALSE;

	/* Schedule a display of the new selection. */
	if (view->details->display_selection_idle_id == 0) {
		view->details->display_selection_idle_id
			= g_idle_add (display_selection_info_idle_callback,
				      view);
	}

	if (view->details->batching_selection_level != 0) {
		view->details->selection_changed_while_batched = TRUE;
	} else {
		/* Here is the work we do only when we're not
		 * batching selection changes. In other words, it's the slower
		 * stuff that we don't want to slow down selection techniques
		 * such as rubberband-selecting in icon view.
		 */

		/* Schedule an update of menu item states to match selection */
		schedule_update_context_menus (view);
	}
}

static void
file_changed_callback (NautilusFile *file, gpointer callback_data)
{
	NautilusView *view = NAUTILUS_VIEW (callback_data);

	schedule_changes (view);

	schedule_update_context_menus (view);
	schedule_update_status (view);
}

/**
 * load_directory:
 * 
 * Switch the displayed location to a new uri. If the uri is not valid,
 * the location will not be switched; user feedback will be provided instead.
 * @view: NautilusView whose location will be changed.
 * @uri: A string representing the uri to switch to.
 * 
 **/
static void
load_directory (NautilusView *view,
		NautilusDirectory *directory)
{
	NautilusDirectory *old_directory;
	NautilusFile *old_file;
	NautilusFileAttributes attributes;

	g_assert (NAUTILUS_IS_VIEW (view));
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	nautilus_profile_start (NULL);

	nautilus_view_stop_loading (view);
	g_signal_emit (view, signals[CLEAR], 0);

	view->details->loading = TRUE;

	/* Update menus when directory is empty, before going to new
	 * location, so they won't have any false lingering knowledge
	 * of old selection.
	 */
	schedule_update_context_menus (view);
	
	while (view->details->subdirectory_list != NULL) {
		nautilus_view_remove_subdirectory (view,
						   view->details->subdirectory_list->data);
	}

	old_directory = view->details->model;
	disconnect_model_handlers (view);

	nautilus_directory_ref (directory);
	view->details->model = directory;
	nautilus_directory_unref (old_directory);

	old_file = view->details->directory_as_file;
	view->details->directory_as_file =
		nautilus_directory_get_corresponding_file (directory);
	nautilus_file_unref (old_file);

	/* FIXME bugzilla.gnome.org 45062: In theory, we also need to monitor metadata here (as
         * well as doing a call when ready), in case external forces
         * change the directory's file metadata.
	 */
	attributes = 
		NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_MOUNT |
		NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO;
	view->details->metadata_for_directory_as_file_pending = TRUE;
	view->details->metadata_for_files_in_directory_pending = TRUE;
	nautilus_file_call_when_ready
		(view->details->directory_as_file,
		 attributes,
		 metadata_for_directory_as_file_ready_callback, view);
	nautilus_directory_call_when_ready
		(view->details->model,
		 attributes,
		 FALSE,
		 metadata_for_files_in_directory_ready_callback, view);

	/* If capabilities change, then we need to update the menus
	 * because of New Folder, and relative emblems.
	 */
	attributes = 
		NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO;
	nautilus_file_monitor_add (view->details->directory_as_file,
				   &view->details->directory_as_file,
				   attributes);

	view->details->file_changed_handler_id = g_signal_connect
		(view->details->directory_as_file, "changed",
		 G_CALLBACK (file_changed_callback), view);

	nautilus_profile_end (NULL);
}

static void
finish_loading (NautilusView *view)
{
	NautilusFileAttributes attributes;

	nautilus_profile_start (NULL);

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	nautilus_profile_start ("BEGIN_LOADING");
	g_signal_emit (view, signals[BEGIN_LOADING], 0);
	nautilus_profile_end ("BEGIN_LOADING");

	/* Assume we have now all information to show window */
	nautilus_window_view_visible  (nautilus_view_get_window (view), NAUTILUS_VIEW (view));

	if (nautilus_directory_are_all_files_seen (view->details->model)) {
		/* Unschedule a pending update and schedule a new one with the minimal
		 * update interval. This gives the view a short chance at gathering the
		 * (cached) deep counts.
		 */
		unschedule_display_of_pending_files (view);
		schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);
	}
	
	/* Start loading. */

	/* Connect handlers to learn about loading progress. */
	view->details->done_loading_handler_id = g_signal_connect
		(view->details->model, "done-loading",
		 G_CALLBACK (done_loading_callback), view);
	view->details->load_error_handler_id = g_signal_connect
		(view->details->model, "load-error",
		 G_CALLBACK (load_error_callback), view);

	/* Monitor the things needed to get the right icon. Also
	 * monitor a directory's item count because the "size"
	 * attribute is based on that, and the file's metadata
	 * and possible custom name.
	 */
	attributes =
		NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
		NAUTILUS_FILE_ATTRIBUTE_MOUNT |
		NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

	nautilus_directory_file_monitor_add (view->details->model,
					     &view->details->model,
					     view->details->show_hidden_files,
					     attributes,
					     files_added_callback, view);

    	view->details->files_added_handler_id = g_signal_connect
		(view->details->model, "files-added",
		 G_CALLBACK (files_added_callback), view);
	view->details->files_changed_handler_id = g_signal_connect
		(view->details->model, "files-changed",
		 G_CALLBACK (files_changed_callback), view);

	nautilus_profile_end (NULL);
}

static void
finish_loading_if_all_metadata_loaded (NautilusView *view)
{
	if (!view->details->metadata_for_directory_as_file_pending &&
	    !view->details->metadata_for_files_in_directory_pending) {
		finish_loading (view);
	}
}

static void
metadata_for_directory_as_file_ready_callback (NautilusFile *file,
			      		       gpointer callback_data)
{
	NautilusView *view;

	view = callback_data;

	g_assert (NAUTILUS_IS_VIEW (view));
	g_assert (view->details->directory_as_file == file);
	g_assert (view->details->metadata_for_directory_as_file_pending);

	nautilus_profile_start (NULL);

	view->details->metadata_for_directory_as_file_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
	nautilus_profile_end (NULL);
}

static void
metadata_for_files_in_directory_ready_callback (NautilusDirectory *directory,
				   		GList *files,
			           		gpointer callback_data)
{
	NautilusView *view;

	view = callback_data;

	g_assert (NAUTILUS_IS_VIEW (view));
	g_assert (view->details->model == directory);
	g_assert (view->details->metadata_for_files_in_directory_pending);

	nautilus_profile_start (NULL);

	view->details->metadata_for_files_in_directory_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
	nautilus_profile_end (NULL);
}

static void
disconnect_handler (GObject *object, guint *id)
{
	if (*id != 0) {
		g_signal_handler_disconnect (object, *id);
		*id = 0;
	}
}

static void
disconnect_directory_handler (NautilusView *view, guint *id)
{
	disconnect_handler (G_OBJECT (view->details->model), id);
}

static void
disconnect_directory_as_file_handler (NautilusView *view, guint *id)
{
	disconnect_handler (G_OBJECT (view->details->directory_as_file), id);
}

static void
disconnect_model_handlers (NautilusView *view)
{
	if (view->details->model == NULL) {
		return;
	}
	disconnect_directory_handler (view, &view->details->files_added_handler_id);
	disconnect_directory_handler (view, &view->details->files_changed_handler_id);
	disconnect_directory_handler (view, &view->details->done_loading_handler_id);
	disconnect_directory_handler (view, &view->details->load_error_handler_id);
	disconnect_directory_as_file_handler (view, &view->details->file_changed_handler_id);
	nautilus_file_cancel_call_when_ready (view->details->directory_as_file,
					      metadata_for_directory_as_file_ready_callback,
					      view);
	nautilus_directory_cancel_callback (view->details->model,
					    metadata_for_files_in_directory_ready_callback,
					    view);
	nautilus_directory_file_monitor_remove (view->details->model,
						&view->details->model);
	nautilus_file_monitor_remove (view->details->directory_as_file,
				      &view->details->directory_as_file);
}

static void
nautilus_view_select_file (NautilusView *view, NautilusFile *file)
{
	GList file_list;

	file_list.data = file;
	file_list.next = NULL;
	file_list.prev = NULL;
	nautilus_view_call_set_selection (view, &file_list);
}

static gboolean
remove_all (gpointer key, gpointer value, gpointer callback_data)
{
	return TRUE;
}

/**
 * nautilus_view_stop_loading:
 * 
 * Stop the current ongoing process, such as switching to a new uri.
 * @view: NautilusView in question.
 * 
 **/
void
nautilus_view_stop_loading (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	unschedule_display_of_pending_files (view);
	reset_update_interval (view);

	/* Free extra undisplayed files */
	file_and_directory_list_free (view->details->new_added_files);
	view->details->new_added_files = NULL;

	file_and_directory_list_free (view->details->new_changed_files);
	view->details->new_changed_files = NULL;

	g_hash_table_foreach_remove (view->details->non_ready_files, remove_all, NULL);

	file_and_directory_list_free (view->details->old_added_files);
	view->details->old_added_files = NULL;

	file_and_directory_list_free (view->details->old_changed_files);
	view->details->old_changed_files = NULL;

	g_list_free_full (view->details->pending_selection, g_object_unref);
	view->details->pending_selection = NULL;

	if (view->details->model != NULL) {
		nautilus_directory_file_monitor_remove (view->details->model, view);
	}
	done_loading (view, FALSE);
}

gboolean
nautilus_view_is_editable (NautilusView *view)
{
	NautilusDirectory *directory;

	directory = nautilus_view_get_model (view);

	if (directory != NULL) {
		return nautilus_directory_is_editable (directory);
	}

	return TRUE;
}

static gboolean
real_is_read_only (NautilusView *view)
{
	NautilusFile *file;
	
	if (!nautilus_view_is_editable (view)) {
		return TRUE;
	}
	
	file = nautilus_view_get_directory_as_file (view);
	if (file != NULL) {
		return !nautilus_file_can_write (file);
	}
	return FALSE;
}

/**
 * nautilus_view_should_show_file
 * 
 * Returns whether or not this file should be displayed based on
 * current filtering options.
 */
gboolean
nautilus_view_should_show_file (NautilusView *view, NautilusFile *file)
{
	return nautilus_file_should_show (file,
					  view->details->show_hidden_files,
					  view->details->show_foreign_files);
}

static gboolean
real_using_manual_layout (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);

	return FALSE;
}

void
nautilus_view_ignore_hidden_file_preferences (NautilusView *view)
{
	g_return_if_fail (view->details->model == NULL);

	if (view->details->ignore_hidden_file_preferences) {
		return;
	}

	view->details->show_hidden_files = FALSE;
	view->details->ignore_hidden_file_preferences = TRUE;
}

void
nautilus_view_set_show_foreign (NautilusView *view,
				gboolean show_foreign)
{
	view->details->show_foreign_files = show_foreign;
}

char *
nautilus_view_get_uri (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	if (view->details->model == NULL) {
		return NULL;
	}
	return nautilus_directory_get_uri (view->details->model);
}

void
nautilus_view_move_copy_items (NautilusView *view,
			       const GList *item_uris,
			       GArray *relative_item_points,
			       const char *target_uri,
			       int copy_action,
			       int x, int y)
{
	NautilusFile *target_file;
	
	g_assert (relative_item_points == NULL
		  || relative_item_points->len == 0 
		  || g_list_length ((GList *)item_uris) == relative_item_points->len);

	/* add the drop location to the icon offsets */
	offset_drop_points (relative_item_points, x, y);

	target_file = nautilus_file_get_existing_by_uri (target_uri);
	/* special-case "command:" here instead of starting a move/copy */
	if (target_file != NULL && nautilus_file_is_launcher (target_file)) {
		nautilus_file_unref (target_file);
		nautilus_launch_desktop_file (
					      gtk_widget_get_screen (GTK_WIDGET (view)),
					      target_uri, item_uris,
					      nautilus_view_get_containing_window (view));
		return;
	} else if (copy_action == GDK_ACTION_COPY &&
		   nautilus_is_file_roller_installed () &&
		   target_file != NULL &&
		   nautilus_file_is_archive (target_file)) {
		char *command, *quoted_uri, *tmp;
		const GList *l;
		GdkScreen  *screen;

		/* Handle dropping onto a file-roller archiver file, instead of starting a move/copy */

		nautilus_file_unref (target_file);

		quoted_uri = g_shell_quote (target_uri);
		command = g_strconcat ("file-roller -a ", quoted_uri, NULL);
		g_free (quoted_uri);

		for (l = item_uris; l != NULL; l = l->next) {
			quoted_uri = g_shell_quote ((char *) l->data);

			tmp = g_strconcat (command, " ", quoted_uri, NULL);
			g_free (command);
			command = tmp;

			g_free (quoted_uri);
		} 

		screen = gtk_widget_get_screen (GTK_WIDGET (view));
		if (screen == NULL) {
			screen = gdk_screen_get_default ();
		}

		nautilus_launch_application_from_command (screen, command, FALSE, NULL);
		g_free (command);

		return;
	}
	nautilus_file_unref (target_file);

	nautilus_file_operations_copy_move
		(item_uris, relative_item_points, 
		 target_uri, copy_action, GTK_WIDGET (view),
		 copy_move_done_callback, pre_copy_move (view));
}

static void
nautilus_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
					    gboolean state, gpointer callback_data)
{
	NautilusView *view;

	view = (NautilusView *) callback_data;
	g_assert (NAUTILUS_IS_VIEW (view));
	
	schedule_update_context_menus (view);
}

void
nautilus_view_start_batching_selection_changes (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	++view->details->batching_selection_level;
	view->details->selection_changed_while_batched = FALSE;
}

void
nautilus_view_stop_batching_selection_changes (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	g_return_if_fail (view->details->batching_selection_level > 0);

	if (--view->details->batching_selection_level == 0) {
		if (view->details->selection_changed_while_batched) {
			nautilus_view_notify_selection_changed (view);
		}
	}
}

gboolean
nautilus_view_get_active (NautilusView *view)
{
	g_assert (NAUTILUS_IS_VIEW (view));
	return view->details->active;
}

static GArray *
real_get_selected_icon_locations (NautilusView *view)
{
        /* By default, just return an empty list. */
        return g_array_new (FALSE, TRUE, sizeof (GdkPoint));
}

static void
nautilus_view_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
	NautilusView *directory_view;
	NautilusWindowSlot *slot;
  
	directory_view = NAUTILUS_VIEW (object);

	switch (prop_id)  {
	case PROP_WINDOW_SLOT:
		g_assert (directory_view->details->slot == NULL);

		slot = NAUTILUS_WINDOW_SLOT (g_value_get_object (value));
		directory_view->details->slot = slot;

		g_signal_connect_object (directory_view->details->slot,
					 "active", G_CALLBACK (slot_active),
					 directory_view, 0);
		g_signal_connect_object (directory_view->details->slot,
					 "inactive", G_CALLBACK (slot_inactive),
					 directory_view, 0);
		break;
	case PROP_SUPPORTS_ZOOMING:
		directory_view->details->supports_zooming = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


gboolean
nautilus_view_handle_scroll_event (NautilusView *directory_view,
				   GdkEventScroll *event)
{
	static gdouble total_delta_y = 0;
	gdouble delta_x, delta_y;

	if (event->state & GDK_CONTROL_MASK) {
		switch (event->direction) {
		case GDK_SCROLL_UP:
			/* Zoom In */
			nautilus_view_bump_zoom_level (directory_view, 1);
			return TRUE;

		case GDK_SCROLL_DOWN:
			/* Zoom Out */
			nautilus_view_bump_zoom_level (directory_view, -1);
			return TRUE;

		case GDK_SCROLL_SMOOTH:
			gdk_event_get_scroll_deltas ((const GdkEvent *) event,
						     &delta_x, &delta_y);

			/* try to emulate a normal scrolling event by summing deltas */
			total_delta_y += delta_y;

			if (total_delta_y >= 1) {
				total_delta_y = 0;
				/* emulate scroll down */
				nautilus_view_bump_zoom_level (directory_view, -1);
				return TRUE;
			} else if (total_delta_y <= - 1) {
				total_delta_y = 0;
				/* emulate scroll up */
				nautilus_view_bump_zoom_level (directory_view, 1);
				return TRUE;				
			} else {
				/* eat event */
				return TRUE;
			}

		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_RIGHT:
			break;

		default:
			g_assert_not_reached ();
		}
	}

	return FALSE;
}

/* handle Shift+Scroll, which will cause a zoom-in/out */
static gboolean
nautilus_view_scroll_event (GtkWidget *widget,
			    GdkEventScroll *event)
{
	NautilusView *directory_view;

	directory_view = NAUTILUS_VIEW (widget);
	if (nautilus_view_handle_scroll_event (directory_view, event)) {
		return TRUE;
	}

	return GTK_WIDGET_CLASS (nautilus_view_parent_class)->scroll_event (widget, event);
}


static void
nautilus_view_parent_set (GtkWidget *widget,
			  GtkWidget *old_parent)
{
	NautilusView *view;
	GtkWidget *parent;

	view = NAUTILUS_VIEW (widget);

	parent = gtk_widget_get_parent (widget);
	g_assert (parent == NULL || old_parent == NULL);

	if (GTK_WIDGET_CLASS (nautilus_view_parent_class)->parent_set != NULL) {
		GTK_WIDGET_CLASS (nautilus_view_parent_class)->parent_set (widget, old_parent);
	}

	if (parent != NULL) {
		g_assert (old_parent == NULL);

		if (view->details->slot == 
		    nautilus_window_get_active_slot (nautilus_view_get_window (view))) {
			view->details->active = TRUE;
		}
	} else {
		remove_update_context_menus_timeout_callback (view);
	}
}

static void
nautilus_view_class_init (NautilusViewClass *klass)
{
	GObjectClass *oclass;
	GtkWidgetClass *widget_class;
	GtkScrolledWindowClass *scrolled_window_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	scrolled_window_class = GTK_SCROLLED_WINDOW_CLASS (klass);
	oclass = G_OBJECT_CLASS (klass);

	oclass->finalize = nautilus_view_finalize;
	oclass->set_property = nautilus_view_set_property;

	widget_class->destroy = nautilus_view_destroy;
	widget_class->scroll_event = nautilus_view_scroll_event;
	widget_class->parent_set = nautilus_view_parent_set;

	g_type_class_add_private (klass, sizeof (NautilusViewDetails));

	/* Get rid of the strange 3-pixel gap that GtkScrolledWindow
	 * uses by default. It does us no good.
	 */
	scrolled_window_class->scrollbar_spacing = 0;

	signals[ADD_FILE] =
		g_signal_new ("add-file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, add_file),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NAUTILUS_TYPE_FILE, NAUTILUS_TYPE_DIRECTORY);
	signals[BEGIN_FILE_CHANGES] =
		g_signal_new ("begin-file-changes",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, begin_file_changes),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[BEGIN_LOADING] =
		g_signal_new ("begin-loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, begin_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[CLEAR] =
		g_signal_new ("clear",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, clear),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[END_FILE_CHANGES] =
		g_signal_new ("end-file-changes",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, end_file_changes),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[END_LOADING] =
		g_signal_new ("end-loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, end_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__BOOLEAN,
		              G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals[FILE_CHANGED] =
		g_signal_new ("file-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, file_changed),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NAUTILUS_TYPE_FILE, NAUTILUS_TYPE_DIRECTORY);
	signals[REMOVE_FILE] =
		g_signal_new ("remove-file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusViewClass, remove_file),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NAUTILUS_TYPE_FILE, NAUTILUS_TYPE_DIRECTORY);
	signals[SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	klass->get_selected_icon_locations = real_get_selected_icon_locations;
	klass->is_read_only = real_is_read_only;
	klass->can_rename_file = can_rename_file;
	klass->start_renaming_file = start_renaming_file;
	klass->get_backing_uri = real_get_backing_uri;
	klass->using_manual_layout = real_using_manual_layout;
	klass->get_window = nautilus_view_get_window;
	klass->get_action_group = nautilus_view_get_action_group;
	klass->update_context_menus = real_update_context_menus;
	klass->update_actions_state = real_update_actions_state;
	klass->update_toolbar_menus = real_update_toolbar_menus;

	copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);

	properties[PROP_WINDOW_SLOT] =
		g_param_spec_object ("window-slot",
				     "Window Slot",
				     "The parent window slot reference",
				     NAUTILUS_TYPE_WINDOW_SLOT,
				     G_PARAM_WRITABLE |
				     G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_ZOOMING] =
		g_param_spec_boolean ("supports-zooming",
				      "Supports zooming",
				      "Whether the view supports zooming",
				      TRUE,
				      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
				      G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
nautilus_view_init (NautilusView *view)
{
	AtkObject *atk_object;
	NautilusDirectory *scripts_directory;
	NautilusDirectory *templates_directory;
	gchar *templates_uri;
	GApplication *app;

	nautilus_profile_start (NULL);

	view->details = G_TYPE_INSTANCE_GET_PRIVATE (view, NAUTILUS_TYPE_VIEW,
						     NautilusViewDetails);

	/* Default to true; desktop-icon-view sets to false */
	view->details->show_foreign_files = TRUE;

	view->details->non_ready_files =
		g_hash_table_new_full (file_and_directory_hash,
				       file_and_directory_equal,
				       (GDestroyNotify)file_and_directory_free,
				       NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (view)),
					      GTK_JUNCTION_TOP | GTK_JUNCTION_LEFT);

	if (set_up_scripts_directory_global ()) {
		scripts_directory = nautilus_directory_get_by_uri (scripts_directory_uri);
		add_directory_to_scripts_directory_list (view, scripts_directory);
		nautilus_directory_unref (scripts_directory);
	} else {
		g_warning ("Ignoring scripts directory, it may be a broken link\n");
	}

	if (nautilus_should_use_templates_directory ()) {
		templates_uri = nautilus_get_templates_directory_uri ();
		templates_directory = nautilus_directory_get_by_uri (templates_uri);
		g_free (templates_uri);
		add_directory_to_templates_directory_list (view, templates_directory);
		nautilus_directory_unref (templates_directory);
	}
	update_templates_directory (view);

	view->details->sort_directories_first =
		g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
	view->details->show_hidden_files =
		g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

	g_signal_connect_object (nautilus_trash_monitor_get (), "trash-state-changed",
				 G_CALLBACK (nautilus_view_trash_state_changed_callback), view, 0);

	/* React to clipboard changes */
	g_signal_connect_object (nautilus_clipboard_monitor_get (), "clipboard-changed",
				 G_CALLBACK (clipboard_changed_callback), view, 0);

	/* Register to menu provider extension signal managing menu updates */
	g_signal_connect_object (nautilus_signaller_get_current (), "popup-menu-changed",
				 G_CALLBACK (schedule_update_context_menus), view, G_CONNECT_SWAPPED);

	gtk_widget_show (GTK_WIDGET (view));

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ENABLE_DELETE,
				  G_CALLBACK (schedule_update_context_menus), view);
	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_CLICK_POLICY,
				  G_CALLBACK (click_policy_changed_callback),
				  view);
	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
				  G_CALLBACK (sort_directories_first_changed_callback), view);
	g_signal_connect_swapped (gtk_filechooser_preferences,
				  "changed::" NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
				  G_CALLBACK (show_hidden_files_changed_callback), view);
	g_signal_connect_swapped (gnome_lockdown_preferences,
				  "changed::" NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE,
				  G_CALLBACK (schedule_update_context_menus), view);

	g_signal_connect_object (nautilus_file_undo_manager_get (), "undo-changed",
				 G_CALLBACK (undo_manager_changed), view, 0);

	/* Accessibility */
	atk_object = gtk_widget_get_accessible (GTK_WIDGET (view));
	atk_object_set_name (atk_object, _("Content View"));
	atk_object_set_description (atk_object, _("View of the current folder"));

	view->details->view_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
	g_action_map_add_action_entries (G_ACTION_MAP (view->details->view_action_group),
					view_entries,
					G_N_ELEMENTS (view_entries),
					view);
	gtk_widget_insert_action_group (GTK_WIDGET (view),
					"view",
					G_ACTION_GROUP (view->details->view_action_group));

	app = g_application_get_default ();

	/* Toolbar menu */
	nautilus_application_add_accelerator (app, "view.zoom-in", "<control>plus");
	nautilus_application_add_accelerator (app, "view.zoom-out", "<control>minus");
	nautilus_application_add_accelerator (app, "view.undo", "<control>z");
	nautilus_application_add_accelerator (app, "view.redo", "<shift><control>z");
	nautilus_application_add_accelerator (app, "view.show-hidden-files", "<control>h");
	/* Background menu */
	nautilus_application_add_accelerator (app, "view.select-all", "<control>a");
	nautilus_application_add_accelerator (app, "view.paste", "<control>v");
	/* Selection menu */
	nautilus_application_add_accelerator (app, "view.open-with-default-application", "<control>o");
	nautilus_application_add_accelerator (app, "view.open-item-new-tab", "<shift><control>t");
	nautilus_application_add_accelerator (app, "view.open-item-new-window", "<shift><control>w");
	nautilus_application_add_accelerator (app, "view.move-to-trash", "<control>Delete");
	nautilus_application_add_accelerator (app, "view.delete", "<shift>Delete");
	nautilus_application_add_accelerator (app, "view.properties", "<control>i");
	nautilus_application_add_accelerator (app, "view.open-item-location", "<control><alt>o");
	nautilus_application_add_accelerator (app, "view.rename", "F2");
	nautilus_application_add_accelerator (app, "view.cut", "<control>x");
	nautilus_application_add_accelerator (app, "view.copy", "<control>c");
	nautilus_application_add_accelerator (app, "view.delete", "<shift>Delete");
	/* Only accesible by shorcuts */
	nautilus_application_add_accelerator (app, "view.select-pattern", "<control>s");
	nautilus_application_add_accelerator (app, "view.zoom-default", "<control>0");
	nautilus_application_add_accelerator (app, "view.invert-selection", "<shift><control>i");
	nautilus_application_add_accelerator (app, "view.open-file-and-close-window", "<control><shift>Down");

	nautilus_profile_end (NULL);
}

NautilusView *
nautilus_view_new (const gchar		*id,
		   NautilusWindowSlot	*slot)
{
	NautilusView *view = NULL;

	if (g_strcmp0 (id, NAUTILUS_CANVAS_VIEW_ID) == 0) {
		view = nautilus_canvas_view_new (slot);
	} else if (g_strcmp0 (id, NAUTILUS_LIST_VIEW_ID) == 0) {
		view = nautilus_list_view_new (slot);
	} else if (g_strcmp0 (id, NAUTILUS_DESKTOP_CANVAS_VIEW_ID) == 0) {
		view = nautilus_desktop_canvas_view_new (slot);
	}
#if ENABLE_EMPTY_VIEW
	else if (g_strcmp0 (id, NAUTILUS_EMPTY_VIEW_ID) == 0) {
		view = nautilus_empty_view_new (slot);
	}
#endif

	if (view == NULL) {
		g_critical ("Unknown view type ID: %s", id);
	} else if (g_object_is_floating (view)) {
		g_object_ref_sink (view);
	}

	return view;
}
