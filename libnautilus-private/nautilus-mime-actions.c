/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-actions.c - uri-specific versions of mime action functions

   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>
#include "nautilus-mime-actions.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-mount-operation.h>
#include <glib/gi18n.h>
#include <string.h>
#include <unistd.h>

#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-autorun.h"
#include "nautilus-file-operations.h"
#include "nautilus-metadata.h"
#include "nautilus-program-choosing.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-debug-log.h"

typedef enum {
	ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE,
	ACTIVATION_ACTION_ASK,
	ACTIVATION_ACTION_LAUNCH,
	ACTIVATION_ACTION_LAUNCH_IN_TERMINAL,
	ACTIVATION_ACTION_OPEN_IN_VIEW,
	ACTIVATION_ACTION_OPEN_IN_APPLICATION,
	ACTIVATION_ACTION_DO_NOTHING,
} ActivationAction;

typedef struct {
	GAppInfo *application;
	GList *files;
} ApplicationLaunchParameters;

typedef struct {
	NautilusWindowInfo *window_info;
	GtkWindow *parent_window;
	GCancellable *cancellable;
	GList *files;
	GList *mountables;
	GList *not_mounted;
	NautilusWindowOpenMode mode;
	NautilusWindowOpenFlags flags;
	char *timed_wait_prompt;
	gboolean timed_wait_active;
	NautilusFileListHandle *files_handle;
	gboolean tried_mounting;
	char *activation_directory;
} ActivateParameters;

/* Number of seconds until cancel dialog shows up */
#define DELAY_UNTIL_CANCEL_MSECS 5000

#define RESPONSE_RUN 1000
#define RESPONSE_DISPLAY 1001
#define RESPONSE_RUN_IN_TERMINAL 1002

#define SILENT_WINDOW_OPEN_LIMIT 5

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

static void cancel_activate_callback                (gpointer            callback_data);
static void activate_activation_uris_ready_callback (GList              *files,
						     gpointer            callback_data);
static void activation_mount_mountables             (ActivateParameters *parameters);
static void activate_callback                       (GList              *files,
						     gpointer            callback_data);
static void activation_mount_not_mounted            (ActivateParameters *parameters);




static ApplicationLaunchParameters *
application_launch_parameters_new (GAppInfo *application,
			      	   GList *files)
{
	ApplicationLaunchParameters *result;

	result = g_new0 (ApplicationLaunchParameters, 1);
	result->application = g_object_ref (application);
	result->files = nautilus_file_list_copy (files);

	return result;
}

static void
application_launch_parameters_free (ApplicationLaunchParameters *parameters)
{
	g_object_unref (parameters->application);
	nautilus_file_list_free (parameters->files);

	g_free (parameters);
}			      

static GList*
filter_nautilus_handler (GList *apps)
{
	GList *l, *next;
	GAppInfo *application;
	const char *id;

	l = apps;
	while (l != NULL) {
		application = (GAppInfo *) l->data;
		next = l->next;

		id = g_app_info_get_id (application);
		if (id != NULL &&
		    strcmp (id,
			    "nautilus-folder-handler.desktop") == 0) {
			g_object_unref (application);
			apps = g_list_delete_link (apps, l); 
		}

		l = next;
	}

	return apps;
}

static GList*
filter_non_uri_apps (GList *apps)
{
	GList *l, *next;
	GAppInfo *app;

	for (l = apps; l != NULL; l = next) {
		app = l->data;
		next = l->next;
		
		if (!g_app_info_supports_uris (app)) {
			apps = g_list_delete_link (apps, l);
			g_object_unref (app);
		}
	}
	return apps;
}


static gboolean
nautilus_mime_actions_check_if_required_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_required_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

NautilusFileAttributes 
nautilus_mime_actions_get_required_file_attributes (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
		NAUTILUS_FILE_ATTRIBUTE_METADATA;
}

static gboolean
file_has_local_path (NautilusFile *file)
{
	GFile *location;
	char *path;
	gboolean res;

	
	/* Don't only check _is_native, because we want to support
	   using the fuse path */
	location = nautilus_file_get_location (file);
	if (g_file_is_native (location)) {
		res = TRUE;
	} else {
		path = g_file_get_path (location);
		
		res = path != NULL;
		
		g_free (path);
	}
	g_object_unref (location);
	
	return res;
}

GAppInfo *
nautilus_mime_get_default_application_for_file (NautilusFile *file)
{
	GAppInfo *app;
	char *mime_type;
	char *uri_scheme;

	if (!nautilus_mime_actions_check_if_required_attributes_ready (file)) {
		return NULL;
	}

	mime_type = nautilus_file_get_mime_type (file);
	app = g_app_info_get_default_for_type (mime_type, !file_has_local_path (file));
	g_free (mime_type);

	if (app == NULL) {
		uri_scheme = nautilus_file_get_uri_scheme (file);
		if (uri_scheme != NULL) {
			app = g_app_info_get_default_for_uri_scheme (uri_scheme);
			g_free (uri_scheme);
		}
	}
	
	return app;
}

static int
file_compare_by_mime_type (NautilusFile *file_a,
			   NautilusFile *file_b)
{
	char *mime_type_a, *mime_type_b;
	int ret;
	
	mime_type_a = nautilus_file_get_mime_type (file_a);
	mime_type_b = nautilus_file_get_mime_type (file_b);
	
	ret = strcmp (mime_type_a, mime_type_b);
	
	g_free (mime_type_a);
	g_free (mime_type_b);
	
	return ret;
}

static int
file_compare_by_parent_uri (NautilusFile *file_a,
			    NautilusFile *file_b) {
	char *parent_uri_a, *parent_uri_b;
	int ret;

	parent_uri_a = nautilus_file_get_parent_uri (file_a);
	parent_uri_b = nautilus_file_get_parent_uri (file_b);

	ret = strcmp (parent_uri_a, parent_uri_b);

	g_free (parent_uri_a);
	g_free (parent_uri_b);

	return ret;
}

static int
application_compare_by_name (const GAppInfo *app_a,
			     const GAppInfo *app_b)
{
	return g_utf8_collate (g_app_info_get_name ((GAppInfo *)app_a),
			       g_app_info_get_name ((GAppInfo *)app_b));
}

static int
application_compare_by_id (const GAppInfo *app_a,
			   const GAppInfo *app_b)
{
	const char *id_a, *id_b;

	id_a = g_app_info_get_id ((GAppInfo *)app_a);
	id_b = g_app_info_get_id ((GAppInfo *)app_b);

	if (id_a == NULL && id_b == NULL) {
		if (g_app_info_equal ((GAppInfo *)app_a, (GAppInfo *)app_b)) {
			return 0;
		}
		if ((gsize)app_a < (gsize) app_b) {
			return -1;
		}
		return 1;
	}

	if (id_a == NULL) {
		return -1;
	}
	
	if (id_b == NULL) {
		return 1;
	}
	
	
	return strcmp (id_a, id_b);
}

GList *
nautilus_mime_get_applications_for_file (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *result;
	GAppInfo *uri_handler;

	if (!nautilus_mime_actions_check_if_required_attributes_ready (file)) {
		return NULL;
	}
	mime_type = nautilus_file_get_mime_type (file);
	result = g_app_info_get_all_for_type (mime_type);

	uri_scheme = nautilus_file_get_uri_scheme (file);
	if (uri_scheme != NULL) {
		uri_handler = g_app_info_get_default_for_uri_scheme (uri_scheme);
		if (uri_handler) {
			result = g_list_prepend (result, uri_handler);
		}
		g_free (uri_scheme);
	}
	
	if (!file_has_local_path (file)) {
		/* Filter out non-uri supporting apps */
		result = filter_non_uri_apps (result);
	}
	
	result = g_list_sort (result, (GCompareFunc) application_compare_by_name);
	g_free (mime_type);

	return filter_nautilus_handler (result);
}

gboolean
nautilus_mime_has_any_applications_for_file (NautilusFile *file)
{
	GList *apps;
	char *mime_type;
	gboolean result;
	char *uri_scheme;
	GAppInfo *uri_handler;

	mime_type = nautilus_file_get_mime_type (file);
	
	apps = g_app_info_get_all_for_type (mime_type);

	uri_scheme = nautilus_file_get_uri_scheme (file);
	if (uri_scheme != NULL) {
		uri_handler = g_app_info_get_default_for_uri_scheme (uri_scheme);
		if (uri_handler) {
			apps = g_list_prepend (apps, uri_handler);
		}
		g_free (uri_scheme);
	}
	
	if (!file_has_local_path (file)) {
		/* Filter out non-uri supporting apps */
		apps = filter_non_uri_apps (apps);
	}
	apps = filter_nautilus_handler (apps);
		
	if (apps) {
		result = TRUE;
		eel_g_object_list_free (apps);
	} else {
		result = FALSE;
	}
	
	g_free (mime_type);

	return result;
}

GAppInfo *
nautilus_mime_get_default_application_for_files (GList *files)
{
	GList *l, *sorted_files;
	NautilusFile *file;
	GAppInfo *app, *one_app;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	app = NULL;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = l->data;

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		one_app = nautilus_mime_get_default_application_for_file (file);
		if (one_app == NULL || (app != NULL && !g_app_info_equal (app, one_app))) {
			if (app) {
				g_object_unref (app);
			}
			if (one_app) {
				g_object_unref (one_app);
			}
			app = NULL;
			break;
		}

		if (app == NULL) {
			app = one_app;
		} else {
			g_object_unref (one_app);
		}
	}

	g_list_free (sorted_files);

	return app;
}

/* returns an intersection of two mime application lists,
 * and returns a new list, freeing a, b and all applications
 * that are not in the intersection set.
 * The lists are assumed to be pre-sorted by their IDs */
static GList *
intersect_application_lists (GList *a,
			     GList *b)
{
	GList *l, *m;
	GList *ret;
	GAppInfo *a_app, *b_app;
	int cmp;

	ret = NULL;

	l = a;
	m = b;

	while (l != NULL && m != NULL) {
		a_app = (GAppInfo *) l->data;
		b_app = (GAppInfo *) m->data;

		cmp = application_compare_by_id (a_app, b_app);
		if (cmp > 0) {
			g_object_unref (b_app);
			m = m->next;
		} else if (cmp < 0) {
			g_object_unref (a_app);
			l = l->next;
		} else {
			g_object_unref (b_app);
			ret = g_list_prepend (ret, a_app);
			l = l->next;
			m = m->next;
		}
	}

	g_list_foreach (l, (GFunc) g_object_unref, NULL);
	g_list_foreach (m, (GFunc) g_object_unref, NULL);

	g_list_free (a);
	g_list_free (b);

	return g_list_reverse (ret);
}

GList *
nautilus_mime_get_applications_for_files (GList *files)
{
	GList *l, *sorted_files;
	NautilusFile *file;
	GList *one_ret, *ret;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	ret = NULL;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = l->data;

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		one_ret = nautilus_mime_get_applications_for_file (file);
		one_ret = g_list_sort (one_ret, (GCompareFunc) application_compare_by_id);
		if (ret != NULL) {
			ret = intersect_application_lists (ret, one_ret);
		} else {
			ret = one_ret;
		}

		if (ret == NULL) {
			break;
		}
	}

	g_list_free (sorted_files);

	ret = g_list_sort (ret, (GCompareFunc) application_compare_by_name);
	
	return ret;
}

gboolean
nautilus_mime_has_any_applications_for_files (GList *files)
{
	GList *l, *sorted_files;
	NautilusFile *file;
	gboolean ret;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	ret = TRUE;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		if (!nautilus_mime_has_any_applications_for_file (file)) {
			ret = FALSE;
			break;
		}
	}

	g_list_free (sorted_files);

	return ret;
}



static void
trash_or_delete_files (GtkWindow *parent_window,
		       const GList *files,
		       gboolean delete_if_all_already_in_trash)
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
						  NULL, NULL);
	eel_g_object_list_free (locations);
}

static void
report_broken_symbolic_link (GtkWindow *parent_window, NautilusFile *file)
{
	char *target_path;
	char *display_name;
	char *prompt;
	char *detail;
	GtkDialog *dialog;
	GList file_as_list;
	int response;
	
	g_assert (nautilus_file_is_broken_symbolic_link (file));

	display_name = nautilus_file_get_display_name (file);
	if (nautilus_file_is_in_trash (file)) {
		prompt = g_strdup_printf (_("The Link \"%s\" is Broken."), display_name);
	} else {
		prompt = g_strdup_printf (_("The Link \"%s\" is Broken. Move it to Trash?"), display_name);
	}
	g_free (display_name);

	target_path = nautilus_file_get_symbolic_link_target_path (file);
	if (target_path == NULL) {
		detail = g_strdup (_("This link cannot be used, because it has no target."));
	} else {
		detail = g_strdup_printf (_("This link cannot be used, because its target "
					    "\"%s\" doesn't exist."), target_path);
	}
	
	if (nautilus_file_is_in_trash (file)) {
		eel_run_simple_dialog (GTK_WIDGET (parent_window), FALSE, GTK_MESSAGE_WARNING,
				       prompt, detail, GTK_STOCK_CANCEL, NULL);
		goto out;
	}

	dialog = eel_show_yes_no_dialog (prompt, detail, _("Mo_ve to Trash"), GTK_STOCK_CANCEL,
					 parent_window);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_YES);

	/* Make this modal to avoid problems with reffing the view & file
	 * to keep them around in case the view changes, which would then
	 * cause the old view not to be destroyed, which would cause its
	 * merged Bonobo items not to be un-merged. Maybe we need to unmerge
	 * explicitly when disconnecting views instead of relying on the
	 * unmerge in Destroy. But since BonoboUIHandler is probably going
	 * to change wildly, I don't want to mess with this now.
	 */

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	if (response == GTK_RESPONSE_YES) {
		file_as_list.data = file;
		file_as_list.next = NULL;
		file_as_list.prev = NULL;
	        trash_or_delete_files (parent_window, &file_as_list, TRUE);
	}

out:
	g_free (prompt);
	g_free (target_path);
	g_free (detail);
}

static ActivationAction
get_executable_text_file_action (GtkWindow *parent_window, NautilusFile *file)
{
	GtkDialog *dialog;
	char *file_name;
	char *prompt;
	char *detail;
	int preferences_value;
	int response;

	g_assert (nautilus_file_contains_text (file));

	preferences_value = eel_preferences_get_enum 
		(NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION);
	switch (preferences_value) {
	case NAUTILUS_EXECUTABLE_TEXT_LAUNCH:
		return ACTIVATION_ACTION_LAUNCH;
	case NAUTILUS_EXECUTABLE_TEXT_DISPLAY:
		return ACTIVATION_ACTION_OPEN_IN_APPLICATION;
	case NAUTILUS_EXECUTABLE_TEXT_ASK:
		break;
	default:
		/* Complain non-fatally, since preference data can't be trusted */
		g_warning ("Unknown value %d for NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION",
			   preferences_value);
		
	}


	file_name = nautilus_file_get_display_name (file);
	prompt = g_strdup_printf (_("Do you want to run \"%s\", or display its contents?"), 
	                            file_name);
	detail = g_strdup_printf (_("\"%s\" is an executable text file."),
				    file_name);
	g_free (file_name);

	dialog = eel_create_question_dialog (prompt,
					     detail,
					     _("Run in _Terminal"), RESPONSE_RUN_IN_TERMINAL,
     					     _("_Display"), RESPONSE_DISPLAY,
					     parent_window);
	gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (dialog, _("_Run"), RESPONSE_RUN);
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CANCEL);
	gtk_widget_show (GTK_WIDGET (dialog));
	
	g_free (prompt);
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));
	
	switch (response) {
	case RESPONSE_RUN:
		return ACTIVATION_ACTION_LAUNCH;
	case RESPONSE_RUN_IN_TERMINAL:
		return ACTIVATION_ACTION_LAUNCH_IN_TERMINAL;
	case RESPONSE_DISPLAY:
		return ACTIVATION_ACTION_OPEN_IN_APPLICATION;
	default:
		return ACTIVATION_ACTION_DO_NOTHING;
	}
}

static ActivationAction
get_default_executable_text_file_action (void)
{
	int preferences_value;

	preferences_value = eel_preferences_get_enum 
		(NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION);
	switch (preferences_value) {
	case NAUTILUS_EXECUTABLE_TEXT_LAUNCH:
		return ACTIVATION_ACTION_LAUNCH;
	case NAUTILUS_EXECUTABLE_TEXT_DISPLAY:
		return ACTIVATION_ACTION_OPEN_IN_APPLICATION;
	case NAUTILUS_EXECUTABLE_TEXT_ASK:
	default:
		return ACTIVATION_ACTION_ASK;
	}
}

gboolean
nautilus_mime_file_opens_in_view (NautilusFile *file)
{
  return (nautilus_file_is_directory (file) ||
	  NAUTILUS_IS_DESKTOP_ICON_FILE (file) ||
	  nautilus_file_is_nautilus_link (file));
}

static ActivationAction
get_activation_action (NautilusFile *file)
{
	ActivationAction action;
	char *activation_uri;

	if (nautilus_file_is_launcher (file)) {
		return ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE;
	}
	
	activation_uri = nautilus_file_get_activation_uri (file);
	if (activation_uri == NULL) {
		activation_uri = nautilus_file_get_uri (file);
	}

	action = ACTIVATION_ACTION_DO_NOTHING;
	if (nautilus_file_is_launchable (file)) {
		char *executable_path;
		
		action = ACTIVATION_ACTION_LAUNCH;
		
		executable_path = g_filename_from_uri (activation_uri, NULL, NULL);
		if (!executable_path) {
			action = ACTIVATION_ACTION_DO_NOTHING;
		} else if (nautilus_file_contains_text (file)) {
			action = get_default_executable_text_file_action ();
		}
		g_free (executable_path);
	} 

	if (action == ACTIVATION_ACTION_DO_NOTHING) {
		if (nautilus_mime_file_opens_in_view (file)) {
			action = ACTIVATION_ACTION_OPEN_IN_VIEW;
		} else {
			action = ACTIVATION_ACTION_OPEN_IN_APPLICATION;
		}
	}
	g_free (activation_uri);

	return action;
}

gboolean
nautilus_mime_file_opens_in_external_app (NautilusFile *file)
{
  ActivationAction activation_action;
  
  activation_action = get_activation_action (file);
  
  return (activation_action == ACTIVATION_ACTION_OPEN_IN_APPLICATION);
}


static unsigned int
mime_application_hash (GAppInfo *app)
{
	const char *id;

	id = g_app_info_get_id (app);

	if (id == NULL) {
		return GPOINTER_TO_UINT(app);
	}

	return g_str_hash (id);
}

static void
list_to_parameters_foreach (GAppInfo *application,
			    GList *files,
			    GList **ret)
{
	ApplicationLaunchParameters *parameters;

	files = g_list_reverse (files);

	parameters = application_launch_parameters_new
		(application, files);
	*ret = g_list_prepend (*ret, parameters);
}


/**
 * fm_directory_view_make_activation_parameters
 *
 * Construct a list of ApplicationLaunchParameters from a list of NautilusFiles,
 * where files that have the same default application are put into the same
 * launch parameter, and others are put into the unhandled_files list.
 *
 * @files: Files to use for construction.
 * @unhandled_files: Files without any default application will be put here.
 * 
 * Return value: Newly allocated list of ApplicationLaunchParameters.
 **/
static GList *
fm_directory_view_make_activation_parameters (GList *files,
					      GList **unhandled_files)
{
	GList *ret, *l, *app_files;
	NautilusFile *file;
	GAppInfo *app, *old_app;
	GHashTable *app_table;

	ret = NULL;
	*unhandled_files = NULL;

	app_table = g_hash_table_new_full
		((GHashFunc) mime_application_hash,
		 (GEqualFunc) g_app_info_equal,
		 (GDestroyNotify) g_object_unref,
		 (GDestroyNotify) g_list_free);

	for (l = files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		app = nautilus_mime_get_default_application_for_file (file);
		if (app != NULL) {
			app_files = NULL;

			if (g_hash_table_lookup_extended (app_table, app,
							  (gpointer *) &old_app,
							  (gpointer *) &app_files)) {
				g_hash_table_steal (app_table, old_app);

				app_files = g_list_prepend (app_files, file);

				g_object_unref (app);
				app = old_app;
			} else {
				app_files = g_list_prepend (NULL, file);
			}

			g_hash_table_insert (app_table, app, app_files);
		} else {
			*unhandled_files = g_list_prepend (*unhandled_files, file);
		}
	}

	g_hash_table_foreach (app_table,
			      (GHFunc) list_to_parameters_foreach,
			      &ret);

	g_hash_table_destroy (app_table);

	*unhandled_files = g_list_reverse (*unhandled_files);

	return g_list_reverse (ret);
}

static gboolean
file_was_cancelled (NautilusFile *file)
{
	GError *error;
	
	error = nautilus_file_get_file_info_error (file);
	return
		error != NULL &&
		error->domain == G_IO_ERROR &&
		error->code == G_IO_ERROR_CANCELLED;
}

static gboolean
file_was_not_mounted (NautilusFile *file)
{
	GError *error;
	
	error = nautilus_file_get_file_info_error (file);
	return
		error != NULL &&
		error->domain == G_IO_ERROR &&
		error->code == G_IO_ERROR_NOT_MOUNTED;
}

static void
activation_parameters_free (ActivateParameters *parameters)
{
	if (parameters->timed_wait_active) {
		eel_timed_wait_stop (cancel_activate_callback, parameters);
	}
	
	if (parameters->window_info) {
		g_object_remove_weak_pointer (G_OBJECT (parameters->window_info), (gpointer *)&parameters->window_info);
	}
	if (parameters->parent_window) {
		g_object_remove_weak_pointer (G_OBJECT (parameters->parent_window), (gpointer *)&parameters->parent_window);
	}
	g_object_unref (parameters->cancellable);
	nautilus_file_list_free (parameters->files);
	nautilus_file_list_free (parameters->mountables);
	nautilus_file_list_free (parameters->not_mounted);
	g_free (parameters->activation_directory);
	g_free (parameters->timed_wait_prompt);
	g_assert (parameters->files_handle == NULL);
	g_free (parameters);
}

static void
cancel_activate_callback (gpointer callback_data)
{
	ActivateParameters *parameters = callback_data;

	parameters->timed_wait_active = FALSE;
	
	g_cancellable_cancel (parameters->cancellable);

	if (parameters->files_handle) {
		nautilus_file_list_cancel_call_when_ready (parameters->files_handle);
		parameters->files_handle = NULL;
		activation_parameters_free (parameters);
	}
}

static void
activation_start_timed_cancel (ActivateParameters *parameters)
{
	parameters->timed_wait_active = TRUE;
	eel_timed_wait_start_with_duration
		(DELAY_UNTIL_CANCEL_MSECS,
		 cancel_activate_callback,
		 parameters,
		 parameters->timed_wait_prompt,
		 parameters->parent_window);
}

static void
activate_mount_op_active (EelMountOperation *operation,
			  gboolean is_active,
			  ActivateParameters *parameters)
{
	if (is_active) {
		if (parameters->timed_wait_active) {
			eel_timed_wait_stop (cancel_activate_callback, parameters);
			parameters->timed_wait_active = FALSE;
		}
	} else {
		if (!parameters->timed_wait_active) {
			activation_start_timed_cancel (parameters);
		}
	}
}

static gboolean
confirm_multiple_windows (GtkWindow *parent_window, int count)
{
	GtkDialog *dialog;
	char *prompt;
	char *detail;
	int response;

	if (count <= SILENT_WINDOW_OPEN_LIMIT) {
		return TRUE;
	}

	prompt = _("Are you sure you want to open all files?");
	detail = g_strdup_printf (ngettext("This will open %d separate window.",
					   "This will open %d separate windows.", count), count);
	dialog = eel_show_yes_no_dialog (prompt, detail, 
					 GTK_STOCK_OK, GTK_STOCK_CANCEL,
					 parent_window);
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
}

static void
activate_files (ActivateParameters *parameters)
{
	NautilusFile *file;
	GList *launch_desktop_files;
	GList *launch_files;
	GList *launch_in_terminal_files;
	GList *open_in_app_files;
	GList *open_in_app_parameters;
	GList *unhandled_open_in_app_files;
	ApplicationLaunchParameters *one_parameters;
	GList *open_in_view_files;
	GList *l;
	int count;
	char *uri;
	char *executable_path, *quoted_path, *name;
	char *old_working_dir;
	ActivationAction action;
	GdkScreen *screen;
	
	screen = gtk_widget_get_screen (GTK_WIDGET (parameters->parent_window));

	launch_desktop_files = NULL;
	launch_files = NULL;
	launch_in_terminal_files = NULL;
	open_in_app_files = NULL;
	open_in_view_files = NULL;

	for (l = parameters->files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (file_was_cancelled (file)) {
			continue;
		}

		action = get_activation_action (file);
		if (action == ACTIVATION_ACTION_ASK) {
			/* Special case for executable text files, since it might be
			 * dangerous & unexpected to launch these.
			 */
			action = get_executable_text_file_action (parameters->parent_window, file);
		}

		switch (action) {
		case ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE :
			launch_desktop_files = g_list_prepend (launch_desktop_files, file);
			break;
		case ACTIVATION_ACTION_LAUNCH :
			launch_files = g_list_prepend (launch_files, file);
			break;
		case ACTIVATION_ACTION_LAUNCH_IN_TERMINAL :
			launch_in_terminal_files = g_list_prepend (launch_in_terminal_files, file);
			break;
		case ACTIVATION_ACTION_OPEN_IN_VIEW :
			open_in_view_files = g_list_prepend (open_in_view_files, file);
			break;
		case ACTIVATION_ACTION_OPEN_IN_APPLICATION :
			open_in_app_files = g_list_prepend (open_in_app_files, file);
			break;
		case ACTIVATION_ACTION_DO_NOTHING :
			break;
		case ACTIVATION_ACTION_ASK :
			g_assert_not_reached ();
			break;
		}
	}

	launch_desktop_files = g_list_reverse (launch_desktop_files);
	for (l = launch_desktop_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		
		uri = nautilus_file_get_uri (file);
		nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
				    "directory view activate_callback launch_desktop_file window=%p: %s",
				    parameters->parent_window, uri);
		nautilus_launch_desktop_file (screen, uri, NULL,
					      parameters->parent_window);
		g_free (uri);
	}

	old_working_dir = NULL;
	if (parameters->activation_directory &&
	    (launch_files != NULL || launch_in_terminal_files != NULL)) {
		old_working_dir = g_get_current_dir ();
		chdir (parameters->activation_directory);
		
	}

	launch_files = g_list_reverse (launch_files);
	for (l = launch_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		uri = nautilus_file_get_activation_uri (file);
		executable_path = g_filename_from_uri (uri, NULL, NULL);
		quoted_path = g_shell_quote (executable_path);
		name = nautilus_file_get_name (file);

		nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
				    "directory view activate_callback launch_file window=%p: %s",
				    parameters->parent_window, quoted_path);

		nautilus_launch_application_from_command (screen, name, quoted_path, NULL, FALSE);
		g_free (name);
		g_free (quoted_path);
		g_free (executable_path);
		g_free (uri);
			
	}

	launch_in_terminal_files = g_list_reverse (launch_in_terminal_files);
	for (l = launch_in_terminal_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		uri = nautilus_file_get_activation_uri (file);
		executable_path = g_filename_from_uri (uri, NULL, NULL);
		quoted_path = g_shell_quote (executable_path);
		name = nautilus_file_get_name (file);

		nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
				    "directory view activate_callback launch_in_terminal window=%p: %s",
				    parameters->parent_window, quoted_path);

		nautilus_launch_application_from_command (screen, name, quoted_path, NULL, TRUE);
		g_free (name);
		g_free (quoted_path);
		g_free (executable_path);
		g_free (uri);
	}

	if (old_working_dir != NULL) {
		chdir (old_working_dir);
		g_free (old_working_dir);
	}

	open_in_view_files = g_list_reverse (open_in_view_files);
	count = g_list_length (open_in_view_files);
	if (parameters->window_info != NULL &&
	    confirm_multiple_windows (parameters->parent_window, count)) {
		NautilusWindowOpenFlags flags;

		flags = parameters->flags;
		if (count > 1) {
			flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}

		for (l = open_in_view_files; l != NULL; l = l->next) {
			GFile *f;
			/* The ui should ask for navigation or object windows
			 * depending on what the current one is */
			file = NAUTILUS_FILE (l->data);

			uri = nautilus_file_get_activation_uri (file);
			f = g_file_new_for_uri (uri);
			nautilus_window_info_open_location (parameters->window_info,
							    f, parameters->mode, flags, NULL);
			g_object_unref (f);
			g_free (uri);
		}
	}

	open_in_app_parameters = NULL;
	unhandled_open_in_app_files = NULL;

	if (open_in_app_files != NULL) {
		open_in_app_files = g_list_reverse (open_in_app_files);

		open_in_app_parameters = fm_directory_view_make_activation_parameters
			(open_in_app_files, &unhandled_open_in_app_files);
	}

	for (l = open_in_app_parameters; l != NULL; l = l->next) {
		one_parameters = l->data;

		nautilus_launch_application (one_parameters->application,
					     one_parameters->files,
					     parameters->parent_window);
		application_launch_parameters_free (one_parameters);
	}

	for (l = unhandled_open_in_app_files; l != NULL; l = l->next) {
		GFile *location;
		char *full_uri_for_display;
		char *uri_for_display;
		char *error_message;
		
		file = NAUTILUS_FILE (l->data);

		location = nautilus_file_get_location (file);
		full_uri_for_display = g_file_get_parse_name (location);
		g_object_unref (location);

		/* Truncate the URI so it doesn't get insanely wide. Note that even
		 * though the dialog uses wrapped text, if the URI doesn't contain
		 * white space then the text-wrapping code is too stupid to wrap it.
		 */
		uri_for_display = eel_str_middle_truncate
			(full_uri_for_display, MAX_URI_IN_DIALOG_LENGTH);
		g_free (full_uri_for_display);
	
		error_message = g_strdup_printf (_("Could not display \"%s\"."),
						 uri_for_display);
		
		g_free (uri_for_display);

		eel_show_error_dialog (error_message,
				       _("There is no application installed for this file type"),
				       parameters->parent_window);
		g_free (error_message);
	}

	if (open_in_app_parameters != NULL ||
	    unhandled_open_in_app_files != NULL) {
		if ((parameters->flags & NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND) != 0 &&
		    parameters->window_info != NULL && 
		     nautilus_window_info_get_window_type (parameters->window_info) == NAUTILUS_WINDOW_SPATIAL) {
			nautilus_window_info_close (parameters->window_info);
		}
	}

	g_list_free (launch_desktop_files);
	g_list_free (launch_files);
	g_list_free (launch_in_terminal_files);
	g_list_free (open_in_view_files);
	g_list_free (open_in_app_files);
	g_list_free (open_in_app_parameters);
	g_list_free (unhandled_open_in_app_files);
	
	activation_parameters_free (parameters);
}

static void 
activation_mount_not_mounted_callback (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	ActivateParameters *parameters = user_data;
	GError *error;
	NautilusFile *file;
	GFile *location;

	file = parameters->not_mounted->data;
		
	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error)) {
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_CANCELLED &&
		     error->code != G_IO_ERROR_FAILED_HANDLED &&
		     error->code != G_IO_ERROR_ALREADY_MOUNTED)) {
			eel_show_error_dialog (_("Unable to mount location"),
					       error->message, NULL);
		}

		if (error->domain != G_IO_ERROR ||
		    error->code != G_IO_ERROR_ALREADY_MOUNTED) {
			parameters->files = g_list_remove (parameters->files, file); 
			nautilus_file_unref (file);
		}

		g_error_free (error);
	} else {
		location = nautilus_file_get_location (file);
		g_object_unref (G_OBJECT (location));
	}
	
	parameters->not_mounted = g_list_delete_link (parameters->not_mounted,
						      parameters->not_mounted);
	nautilus_file_unref (file);

	activation_mount_not_mounted (parameters);
}

static void
activation_mount_not_mounted (ActivateParameters *parameters)
{
	NautilusFile *file;
	GFile *location;
	GMountOperation *mount_op;

	if (parameters->not_mounted != NULL) {
		file = parameters->not_mounted->data;
		mount_op = eel_mount_operation_new (parameters->parent_window);
		g_signal_connect (mount_op, "active_changed", (GCallback)activate_mount_op_active, parameters);
		location = nautilus_file_get_location (file);
		g_file_mount_enclosing_volume (location, 0, mount_op, parameters->cancellable,
					       activation_mount_not_mounted_callback, parameters);
		g_object_unref (location);
		g_object_unref (mount_op);
		return;
	}

	parameters->tried_mounting = TRUE;

	if (parameters->files == NULL) {
		activation_parameters_free (parameters);
		return;
	}
	
	nautilus_file_list_call_when_ready
		(parameters->files,
		 nautilus_mime_actions_get_required_file_attributes () | NAUTILUS_FILE_ATTRIBUTE_LINK_INFO,
		 &parameters->files_handle,
		 activate_callback, parameters);
}


static void
activate_callback (GList *files, gpointer callback_data)
{
	ActivateParameters *parameters = callback_data;
	GList *l, *next;
	NautilusFile *file;

	parameters->files_handle = NULL;

	for (l = parameters->files; l != NULL; l = next) {
		file = NAUTILUS_FILE (l->data);
		next = l->next;

		if (file_was_cancelled (file)) {
			nautilus_file_unref (file);
			parameters->files = g_list_delete_link (parameters->files, l);
			continue;
		}

		if (file_was_not_mounted (file)) {
			if (parameters->tried_mounting) {
				nautilus_file_unref (file);
				parameters->files = g_list_delete_link (parameters->files, l);
			} else {
				parameters->not_mounted = g_list_prepend (parameters->not_mounted,
									  nautilus_file_ref (file));
			}
			continue;
		}
	}


	if (parameters->not_mounted != NULL) {
		activation_mount_not_mounted (parameters);
	} else {
		activate_files (parameters);
	}
}

static void
activate_activation_uris_ready_callback (GList *files_ignore,
					 gpointer callback_data)
{
	ActivateParameters *parameters = callback_data;
	GList *l, *next;
	NautilusFile *file;

	parameters->files_handle = NULL;
	
	for (l = parameters->files; l != NULL; l = next) {
		file = NAUTILUS_FILE (l->data);
		next = l->next;

		if (file_was_cancelled (file)) {
			nautilus_file_unref (file);
			parameters->files = g_list_delete_link (parameters->files, l);
			continue;
		}

		if (nautilus_file_is_broken_symbolic_link (file)) {
			nautilus_file_unref (file);
			parameters->files = g_list_delete_link (parameters->files, l);
			report_broken_symbolic_link (parameters->parent_window, file);
			continue;
		}

		if (nautilus_file_get_file_type (file) == G_FILE_TYPE_MOUNTABLE &&
		    !nautilus_file_has_activation_uri (file)) {
			/* Don't launch these... There is nothing we
			   can do */
			nautilus_file_unref (file);
			parameters->files = g_list_delete_link (parameters->files, l);
			continue;
		}
		
	}

	if (parameters->files == NULL) {
		activation_parameters_free (parameters);
		return;
	}

	/* Convert the files to the actual activation uri files */
	for (l = parameters->files; l != NULL; l = l->next) {
		char *uri;
		file = NAUTILUS_FILE (l->data);

		/* We want the file for the activation URI since we care
		 * about the attributes for that, not for the original file.
		 */
		uri = nautilus_file_get_activation_uri (file);
		if (uri != NULL) {
			NautilusFile *actual_file;
			
			actual_file = nautilus_file_get_by_uri (uri);
			if (actual_file != NULL) {
				nautilus_file_unref (file);
				l->data = actual_file;
			}
		}
		g_free (uri);
	}


	/* get the parameters for the actual files */	
	nautilus_file_list_call_when_ready
		(parameters->files,
		 nautilus_mime_actions_get_required_file_attributes () | NAUTILUS_FILE_ATTRIBUTE_LINK_INFO,
		 &parameters->files_handle,
		 activate_callback, parameters);
}

static void
activation_get_activation_uris (ActivateParameters *parameters)
{
	GList *l;
	NautilusFile *file;

	/* link target info might be stale, re-read it */
	for (l = parameters->files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (file_was_cancelled (file)) {
			nautilus_file_unref (file);
			parameters->files = g_list_delete_link (parameters->files, l);
			continue;
		}
		
		if (nautilus_file_is_symbolic_link (file)) {
			nautilus_file_invalidate_attributes 
				(file,
				 NAUTILUS_FILE_ATTRIBUTE_INFO |
				 NAUTILUS_FILE_ATTRIBUTE_LINK_INFO);
		}
	}

	if (parameters->files == NULL) {
		activation_parameters_free (parameters);
		return;
	}
	
	nautilus_file_list_call_when_ready
		(parameters->files,
		 NAUTILUS_FILE_ATTRIBUTE_INFO |
		 NAUTILUS_FILE_ATTRIBUTE_LINK_INFO,
		 &parameters->files_handle,
		 activate_activation_uris_ready_callback, parameters);
}


static void
activation_mountable_mounted (NautilusFile  *file,
			      GFile         *result_location,
			      GError        *error,
			      gpointer       callback_data)
{
	ActivateParameters *parameters = callback_data;
	NautilusFile *target_file;

	/* Remove from list of files that have to be mounted */
	parameters->mountables = g_list_remove (parameters->mountables, file); 
	nautilus_file_unref (file);

	if (error == NULL) {
		/* Replace file with the result of the mount */
		
		target_file = nautilus_file_get (result_location);
		
		parameters->files = g_list_remove (parameters->files, file); 
		nautilus_file_unref (file);
		
		parameters->files = g_list_prepend (parameters->files, target_file);
	} else {
		/* Remove failed file */
		
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_FAILED_HANDLED &&
		     error->code != G_IO_ERROR_ALREADY_MOUNTED)) {
			parameters->files = g_list_remove (parameters->files, file); 
			nautilus_file_unref (file);
		}
		
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_CANCELLED &&
		     error->code != G_IO_ERROR_FAILED_HANDLED &&
		     error->code != G_IO_ERROR_ALREADY_MOUNTED)) {
			eel_show_error_dialog (_("Unable to mount location"),
					       error->message, NULL);
		}

		if (error->code == G_IO_ERROR_CANCELLED) {
			activation_parameters_free (parameters);
			return;
		}
	}

	/* Mount more mountables */
	activation_mount_mountables (parameters);
}


static void
activation_mount_mountables (ActivateParameters *parameters)
{
	NautilusFile *file;
	GMountOperation *mount_op;

	if (parameters->mountables != NULL) {
		file = parameters->mountables->data;
		mount_op = eel_mount_operation_new (parameters->parent_window);
		g_signal_connect (mount_op, "active_changed", (GCallback)activate_mount_op_active, parameters);
		nautilus_file_mount (file,
				     mount_op,
				     parameters->cancellable,
				     activation_mountable_mounted,
				     parameters);
		g_object_unref (mount_op);
		return;
	}

	activation_get_activation_uris (parameters);
}


/**
 * fm_directory_view_activate_files:
 * 
 * Activate a list of files. Each one might launch with an application or
 * with a component. This is normally called only by subclasses.
 * @view: FMDirectoryView in question.
 * @files: A GList of NautilusFiles to activate.
 * 
 **/
void
nautilus_mime_activate_files (GtkWindow *parent_window,
			      NautilusWindowInfo *window_info,
			      GList *files,
			      const char *launch_directory,
			      NautilusWindowOpenMode mode,
			      NautilusWindowOpenFlags flags)
{
	ActivateParameters *parameters;
	char *file_name;
	int file_count;
	GList *l, *next;
	NautilusFile *file;

	if (files == NULL) {
		return;
	}

	nautilus_debug_log_with_file_list (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER, files,
					   "fm_directory_view_activate_files window=%p",
					   parent_window);

	parameters = g_new0 (ActivateParameters, 1);
	parameters->window_info = window_info;
	g_object_add_weak_pointer (G_OBJECT (parameters->window_info), (gpointer *)&parameters->window_info);
	if (parent_window) {
		parameters->parent_window = parent_window;
		g_object_add_weak_pointer (G_OBJECT (parameters->parent_window), (gpointer *)&parameters->parent_window);
	}
	parameters->cancellable = g_cancellable_new ();
	parameters->activation_directory = g_strdup (launch_directory);
	parameters->files = nautilus_file_list_copy (files);
	parameters->mode = mode;
	parameters->flags = flags;

	file_count = g_list_length (files);
	if (file_count == 1) {
		file_name = nautilus_file_get_display_name (files->data);
		parameters->timed_wait_prompt = g_strdup_printf (_("Opening \"%s\"."), file_name);
		g_free (file_name);
	} else {
		parameters->timed_wait_prompt = g_strdup_printf (ngettext ("Opening %d item.",
									   "Opening %d items.",
									   file_count),
								 file_count);
	}

	
	for (l = parameters->files; l != NULL; l = next) {
		file = NAUTILUS_FILE (l->data);
		next = l->next;
		
		if (nautilus_file_can_mount (file)) {
			parameters->mountables = g_list_prepend (parameters->mountables,
								 nautilus_file_ref (file));
		}
	}
	
	activation_start_timed_cancel (parameters);
	activation_mount_mountables (parameters);
}

/**
 * fm_directory_view_activate_file:
 * 
 * Activate a file in this view. This might involve switching the displayed
 * location for the current window, or launching an application.
 * @view: FMDirectoryView in question.
 * @file: A NautilusFile representing the file in this view to activate.
 * @use_new_window: Should this item be opened in a new window?
 * 
 **/

void
nautilus_mime_activate_file (GtkWindow *parent_window,
			     NautilusWindowInfo *window_info,
			     NautilusFile *file,
			     const char *launch_directory,
			     NautilusWindowOpenMode mode,
			     NautilusWindowOpenFlags flags)
{
	GList *files;

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	files = g_list_prepend (NULL, file);
	nautilus_mime_activate_files (parent_window, window_info, files, launch_directory, mode, flags);
	g_list_free (files);
}
