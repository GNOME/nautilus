/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view.c
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ettore Perazzoli,
 *          John Sullivan <sullivan@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          Pavel Cisler <pavel@eazel.com>,
 *          David Emory Watson <dwatson@cs.ucr.edu>
 */

#include <config.h>
#include <math.h>
#include "fm-directory-view.h"
#include "fm-list-view.h"

#include "fm-actions.h"
#include "fm-error-reporting.h"
#include "fm-properties-window.h"
#include <libgnome/gnome-url.h>
#include <eel/eel-alert-dialog.h>
#include <eel/eel-background.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-open-with-dialog.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-marshal.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkselection.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkbindings.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-recent.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-clipboard-monitor.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>
#include <libnautilus-private/nautilus-desktop-directory.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-private.h> /* for nautilus_file_get_existing */
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-trash-directory.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <unistd.h>

/* Number of seconds until cancel dialog shows up */
#define DELAY_UNTIL_CANCEL_MSECS 5000

#define SILENT_WINDOW_OPEN_LIMIT 5

#define DUPLICATE_HORIZONTAL_ICON_OFFSET 70
#define DUPLICATE_VERTICAL_ICON_OFFSET   30

#define RESPONSE_RUN 1000
#define RESPONSE_DISPLAY 1001
#define RESPONSE_RUN_IN_TERMINAL 1002

#define FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER   	"/MenuBar/File/Open Placeholder/Open With/Applications Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER    	"/MenuBar/File/Open Placeholder/Applications Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER    		"/MenuBar/File/Open Placeholder/Scripts/Scripts Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_EXTENSION_ACTIONS_PLACEHOLDER       "/MenuBar/Edit/Extension Actions"
#define FM_DIRECTORY_VIEW_MENU_PATH_NEW_DOCUMENTS_PLACEHOLDER  		"/MenuBar/File/New Items Placeholder/New Documents/New Documents Placeholder"

#define FM_DIRECTORY_VIEW_POPUP_PATH_SELECTION				"/selection"
#define FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER  	"/selection/Open Placeholder/Open With/Applications Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER    	"/selection/Open Placeholder/Applications Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER    		"/selection/Open Placeholder/Scripts/Scripts Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_EXTENSION_ACTIONS			"/selection/Extension Actions"

#define FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND				"/background"
#define FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND_SCRIPTS_PLACEHOLDER	"/background/Before Zoom Items/New Object Items/Scripts/Scripts Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND_NEW_DOCUMENTS_PLACEHOLDER "/background/Before Zoom Items/New Object Items/New Documents/New Documents Placeholder"

#define MAX_MENU_LEVELS 5

enum {
	ADD_FILE,
	BEGIN_FILE_CHANGES,
	BEGIN_LOADING,
	CLEAR,
	END_FILE_CHANGES,
	FLUSH_ADDED_FILES,
	END_LOADING,
	FILE_CHANGED,
	LOAD_ERROR,
	MOVE_COPY_ITEMS,
	REMOVE_FILE,
	TRASH,
	DELETE,
	LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_WINDOW
};


static guint signals[LAST_SIGNAL];

static GdkAtom copied_files_atom;
static GdkAtom utf8_string_atom;

static gboolean show_delete_command_auto_value;
static gboolean confirm_trash_auto_value;

static char *scripts_directory_uri;
static int scripts_directory_uri_length;

static char *templates_directory_uri;
static int templates_directory_uri_length;

struct FMDirectoryViewDetails
{
	NautilusWindowInfo *window;
	NautilusDirectory *model;
	NautilusFile *directory_as_file;
	GtkActionGroup *dir_action_group;
	guint dir_merge_id;

	GList *scripts_directory_list;
	GtkActionGroup *scripts_action_group;
	guint scripts_merge_id;
	
	GList *templates_directory_list;
	GtkActionGroup *templates_action_group;
	guint templates_merge_id;

	GtkActionGroup *extensions_menu_action_group;
	guint extensions_menu_merge_id;
	
	guint display_selection_idle_id;
	guint update_menus_timeout_id;
	guint update_status_idle_id;
	
	guint display_pending_idle_id;
	
	guint files_added_handler_id;
	guint files_changed_handler_id;
	guint load_error_handler_id;
	guint done_loading_handler_id;
	guint file_changed_handler_id;

	GList *new_added_files;
	GList *new_changed_files;

	GHashTable *non_ready_files;

	GList *old_added_files;
	GList *old_changed_files;

	GList *pending_uris_selected;

	/* loading indicates whether this view has begun loading a directory.
	 * This flag should need not be set inside subclasses. FMDirectoryView automatically
	 * sets 'loading' to TRUE before it begins loading a directory's contents and to FALSE
	 * after it finishes loading the directory and its view.
	 */
	gboolean loading;
	gboolean menu_states_untrustworthy;
	gboolean scripts_invalid;
	gboolean templates_invalid;
	gboolean reported_load_error;

	gboolean sort_directories_first;

	gboolean show_hidden_files;
	gboolean show_backup_files;
	gboolean ignore_hidden_file_preferences;

	gboolean batching_selection_level;
	gboolean selection_changed_while_batched;

	gboolean metadata_for_directory_as_file_pending;
	gboolean metadata_for_files_in_directory_pending;

	gboolean selection_change_is_due_to_shell;
	gboolean send_selection_change_to_shell;

	NautilusFile *file_monitored_for_open_with;
	GtkActionGroup *open_with_action_group;
	guint open_with_merge_id;
};

typedef enum {
	ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE,
	ACTIVATION_ACTION_LAUNCH_APPLICATION_FROM_COMMAND,
	ACTIVATION_ACTION_ASK,
	ACTIVATION_ACTION_LAUNCH,
	ACTIVATION_ACTION_LAUNCH_IN_TERMINAL,
	ACTIVATION_ACTION_OPEN_IN_VIEW,
	ACTIVATION_ACTION_OPEN_IN_APPLICATION,
	ACTIVATION_ACTION_DO_NOTHING,
} ActivationAction;

typedef struct {
	FMDirectoryView *view;
	NautilusFile *file;
	NautilusWindowOpenMode mode;
	NautilusWindowOpenFlags flags;
	NautilusFileCallback callback;
	gboolean mounted;
	gboolean mounting;
	gboolean cancelled;
} ActivateParameters;

enum {
	GNOME_COPIED_FILES,
	UTF8_STRING
};

static const GtkTargetEntry clipboard_targets[] = {
	{ "x-special/gnome-copied-files", 0, GNOME_COPIED_FILES },
	{ "UTF8_STRING", 0, UTF8_STRING }
};

/* forward declarations */

static void     cancel_activate_callback                       (gpointer              callback_data);
static gboolean display_selection_info_idle_callback           (gpointer              data);
static gboolean file_is_launchable                             (NautilusFile         *file);
static void     fm_directory_view_class_init                   (FMDirectoryViewClass *klass);
static void     fm_directory_view_init                         (FMDirectoryView      *view);
static void     fm_directory_view_duplicate_selection          (FMDirectoryView      *view,
								GList                *files,
								GArray               *item_locations);
static gboolean fm_directory_view_confirm_deletion             (FMDirectoryView      *view,
								GList                *uris,
								gboolean              all);
static void     fm_directory_view_create_links_for_files       (FMDirectoryView      *view,
								GList                *files,
								GArray               *item_locations);
static void     trash_or_delete_files                          (FMDirectoryView      *view,
								const GList          *files);
static void     fm_directory_view_activate_file                (FMDirectoryView      *view,
								NautilusFile         *file,
								NautilusWindowOpenMode mode,
								NautilusWindowOpenFlags flags);
static void     load_directory                                 (FMDirectoryView      *view,
								NautilusDirectory    *directory);
static void     fm_directory_view_merge_menus                  (FMDirectoryView      *view);
static void     fm_directory_view_init_show_hidden_files       (FMDirectoryView      *view);
static char *   file_name_from_uri                             (const char           *uri);
static void     fm_directory_view_load_location                (NautilusView         *nautilus_view,
								const char           *location);
static void     fm_directory_view_stop_loading                 (NautilusView         *nautilus_view);
static void     clipboard_changed_callback                     (NautilusClipboardMonitor *monitor,
								FMDirectoryView      *view);
static void     open_one_in_new_window                         (gpointer              data,
								gpointer              callback_data);
static void     schedule_update_menus                          (FMDirectoryView      *view);
static void     schedule_update_menus_callback                 (gpointer              callback_data);
static void     remove_update_menus_timeout_callback           (FMDirectoryView      *view);
static void     schedule_update_status                          (FMDirectoryView      *view);
static void     remove_update_status_idle_callback             (FMDirectoryView *view); 
static void     schedule_idle_display_of_pending_files         (FMDirectoryView      *view);
static void     unschedule_idle_display_of_pending_files       (FMDirectoryView      *view);
static void     unschedule_display_of_pending_files            (FMDirectoryView      *view);
static void     disconnect_model_handlers                      (FMDirectoryView      *view);
static void     filtering_changed_callback                     (gpointer              callback_data);
static void     metadata_for_directory_as_file_ready_callback  (NautilusFile         *file,
								gpointer              callback_data);
static void     metadata_for_files_in_directory_ready_callback (NautilusDirectory    *directory,
								GList                *files,
								gpointer              callback_data);
static void     fm_directory_view_trash_state_changed_callback (NautilusTrashMonitor *trash,
								gboolean              state,
								gpointer              callback_data);
static void     fm_directory_view_select_file                  (FMDirectoryView      *view,
								NautilusFile         *file);
static void     monitor_file_for_open_with                     (FMDirectoryView      *view,
								NautilusFile         *file);
static void     create_scripts_directory                       (void);
static void     activate_activation_uri_ready_callback         (NautilusFile         *file,
								gpointer              callback_data);
static gboolean can_show_default_app                           (FMDirectoryView *view,
								NautilusFile *file);

static gboolean activate_check_mime_types                      (FMDirectoryView *view,
								NautilusFile *file,
								gboolean warn_on_mismatch);


static void action_open_scripts_folder_callback    (GtkAction *action,
						    gpointer   callback_data);
static void action_cut_files_callback              (GtkAction *action,
						    gpointer   callback_data);
static void action_copy_files_callback             (GtkAction *action,
						    gpointer   callback_data);
static void action_paste_files_callback            (GtkAction *action,
						    gpointer   callback_data);
static void action_rename_callback                 (GtkAction *action,
						    gpointer   callback_data);
static void action_show_hidden_files_callback      (GtkAction *action,
						    gpointer   callback_data);
static void action_reset_background_callback       (GtkAction *action,
						    gpointer   callback_data);
static void action_paste_files_into_callback       (GtkAction *action,
						    gpointer   callback_data);
static void action_connect_to_server_link_callback (GtkAction *action,
						    gpointer   data);
static void action_mount_volume_callback           (GtkAction *action,
						    gpointer   data);
static void action_unmount_volume_callback         (GtkAction *action,
						    gpointer   data);

EEL_CLASS_BOILERPLATE (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)

EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_file)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, bump_zoom_level)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_in)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_out)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, file_changed)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_background_widget)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_item_count)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, is_empty)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, reset_to_defaults)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, restore_default_zoom_level)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, select_all)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, set_selection)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, zoom_to_level)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_zoom_level)

typedef struct {
	GnomeVFSMimeApplication *application;
	NautilusFile *file;
	FMDirectoryView *directory_view;
} ApplicationLaunchParameters;

typedef struct {
	NautilusFile *file;
	FMDirectoryView *directory_view;
} ScriptLaunchParameters;

typedef struct {
	NautilusFile *file;
	FMDirectoryView *directory_view;
} CreateTemplateParameters;


static ApplicationLaunchParameters *
application_launch_parameters_new (GnomeVFSMimeApplication *application,
			      	   NautilusFile *file,
			           FMDirectoryView *directory_view)
{
	ApplicationLaunchParameters *result;

	result = g_new0 (ApplicationLaunchParameters, 1);
	result->application = gnome_vfs_mime_application_copy (application);
	g_object_ref (directory_view);
	result->directory_view = directory_view;
	nautilus_file_ref (file);
	result->file = file;

	return result;
}

static void
application_launch_parameters_free (ApplicationLaunchParameters *parameters)
{
	gnome_vfs_mime_application_free (parameters->application);
	g_object_unref (parameters->directory_view);
	nautilus_file_unref (parameters->file);
	g_free (parameters);
}			      


static ScriptLaunchParameters *
script_launch_parameters_new (NautilusFile *file,
			      FMDirectoryView *directory_view)
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
				FMDirectoryView *directory_view)
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

NautilusWindowInfo *
fm_directory_view_get_nautilus_window (FMDirectoryView  *view)
{
	g_assert (view->details->window != NULL);

	return view->details->window;
}



/* Returns the GtkWindow that this directory view occupies, or NULL
 * if at the moment this directory view is not in a GtkWindow or the
 * GtkWindow cannot be determined. Primarily used for parenting dialogs.
 */
GtkWindow *
fm_directory_view_get_containing_window (FMDirectoryView *view)
{
	GtkWidget *window;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
	if (window == NULL) {
		return NULL;
	}

	return GTK_WINDOW (window);
}

gboolean
fm_directory_view_confirm_multiple_windows (FMDirectoryView *view, int count)
{
	GtkDialog *dialog;
	char *prompt;
	char *title;
	char *detail;
	int response;

	if (count <= SILENT_WINDOW_OPEN_LIMIT) {
		return TRUE;
	}

	title = g_strdup_printf (ngettext("Open %d Window?", "Open %d Windows?", count), count);
	prompt = _("Are you sure you want to open all files?");
	detail = g_strdup_printf (ngettext("This will open %d separate window.",
					   "This will open %d separate windows.", count), count);
	dialog = eel_show_yes_no_dialog (prompt, detail, title, 
					 GTK_STOCK_OK, GTK_STOCK_CANCEL,
					 fm_directory_view_get_containing_window (view));
	g_free (detail);
	g_free (title);

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
}

static gboolean
selection_contains_one_item_in_menu_callback (FMDirectoryView *view, GList *selection)
{
	if (eel_g_list_exactly_one_item (selection)) {
		return TRUE;
	}

	/* If we've requested a menu update that hasn't yet occurred, then
	 * the mismatch here doesn't surprise us, and we won't complain.
	 * Otherwise, we will complain.
	 */
	if (!view->details->menu_states_untrustworthy) {
		g_warning ("Expected one selected item, found %d. No action will be performed.", 	
			   g_list_length (selection));
	}

	return FALSE;
}

static gboolean
selection_not_empty_in_menu_callback (FMDirectoryView *view, GList *selection)
{
	if (selection != NULL) {
		return TRUE;
	}

	/* If we've requested a menu update that hasn't yet occurred, then
	 * the mismatch here doesn't surprise us, and we won't complain.
	 * Otherwise, we will complain.
	 */
	if (!view->details->menu_states_untrustworthy) {
		g_warning ("Empty selection found when selection was expected. No action will be performed.");
	}

	return FALSE;
}

static void
action_open_callback (GtkAction *action,
		      gpointer callback_data)
{
	GList *selection;
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	selection = fm_directory_view_get_selection (view);
	fm_directory_view_activate_files (view, selection,
					  NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
					  0);
	nautilus_file_list_free (selection);
}

static void
action_open_close_parent_callback (GtkAction *action,
				   gpointer callback_data)
{
	GList *selection;
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	selection = fm_directory_view_get_selection (view);
	fm_directory_view_activate_files (view, selection,
					  NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
					  NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND);
	nautilus_file_list_free (selection);
}


static void
action_open_alternate_callback (GtkAction *action,
				gpointer callback_data)
{
	FMDirectoryView *view;
	GList *selection;

	view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);

	if (fm_directory_view_confirm_multiple_windows (view, g_list_length (selection))) {
		g_list_foreach (selection, open_one_in_new_window, view);
	}

	nautilus_file_list_free (selection);
}

static void
fm_directory_view_launch_application (GnomeVFSMimeApplication *application,
				      NautilusFile *file,
				      FMDirectoryView *directory_view)
{
	char *uri;
	GnomeVFSURI *vfs_uri;

	g_assert (application != NULL);
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));

	nautilus_launch_application
			(application, file, 
			 fm_directory_view_get_containing_window (directory_view));

	uri = nautilus_file_get_uri (file);

	/* Only add real gnome-vfs uris to recent. Not things like
	   trash:// and x-nautilus-desktop:// */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri != NULL) {
		egg_recent_model_add (nautilus_recent_get_model (), uri);
		gnome_vfs_uri_unref (vfs_uri);
	}

	g_free (uri);
}				      

#if NEW_MIME_COMPLETE
static void
fm_directory_view_chose_application_callback (GnomeVFSMimeApplication *application, 
					      gpointer callback_data)
{
	ApplicationLaunchParameters *launch_parameters;

	g_assert (callback_data != NULL);

	launch_parameters = (ApplicationLaunchParameters *)callback_data;
	g_assert (launch_parameters->application == NULL);

	if (application != NULL) {
		fm_directory_view_launch_application 
			(application, /* NOT the (empty) application in launch_parameters */
			 launch_parameters->file,
			 launch_parameters->directory_view);
	}

	application_launch_parameters_free (launch_parameters);
}
#endif

static void
open_location (FMDirectoryView *directory_view, 
	       const char *new_uri, 
	       NautilusWindowOpenMode mode,
	       NautilusWindowOpenFlags flags)
{
	NautilusFile *file;

	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));
	g_assert (new_uri != NULL);

	/* We want to avoid reloading the mime list for the
	 * file if its invalidated when force-reload opening.
	 * eventually the open will cause the file to change, and we'll re-set
	 * the monitor for the selected file then.
	 */
	file = nautilus_file_get (new_uri);
	if (file == directory_view->details->file_monitored_for_open_with) {
		monitor_file_for_open_with (directory_view, NULL);
	}
	nautilus_file_unref (file);
	
	nautilus_window_info_open_location (directory_view->details->window,
					    new_uri, mode, flags, NULL);
}

static void
application_selected_cb (EelOpenWithDialog *dialog,
			 GnomeVFSMimeApplication *app,
			 gpointer user_data)
{
	FMDirectoryView *view;
	NautilusFile *file;

	view = FM_DIRECTORY_VIEW (user_data);
	
	file = g_object_get_data (G_OBJECT (dialog), "directory-view:file");

	fm_directory_view_launch_application (app, file, view);
}

static void
choose_program (FMDirectoryView *view,
		NautilusFile *file)
{
	GtkWidget *dialog;
	char *uri;
	char *mime_type;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_file_ref (file);
	uri = nautilus_file_get_uri (file);
	mime_type = nautilus_file_get_mime_type (file);

	dialog = eel_open_with_dialog_new (uri, mime_type);
	g_object_set_data_full (G_OBJECT (dialog), 
				"directory-view:file",
				g_object_ref (file),
				(GDestroyNotify)g_object_unref);
	
	gtk_window_set_screen (GTK_WINDOW (dialog), 
			       gtk_widget_get_screen (GTK_WIDGET (view)));
	gtk_widget_show (dialog);

	g_signal_connect_object (dialog, 
				 "application_selected", 
				 G_CALLBACK (application_selected_cb),
				 view,
				 0);
			  
 	g_free (uri);
	g_free (mime_type);
	nautilus_file_unref (file);	
}

static void
open_with_other_program (FMDirectoryView *view)
{
        GList *selection;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

       	selection = fm_directory_view_get_selection (view);

	if (selection_contains_one_item_in_menu_callback (view, selection)) {
		choose_program (view, NAUTILUS_FILE (selection->data));
	}

	nautilus_file_list_free (selection);
}

static void
action_other_application_callback (GtkAction *action,
				   gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	open_with_other_program (FM_DIRECTORY_VIEW (callback_data));
}

static void
trash_or_delete_selected_files (FMDirectoryView *view)
{
        GList *selection;
        
	selection = fm_directory_view_get_selection (view);
	trash_or_delete_files (view, selection);					 
        nautilus_file_list_free (selection);
}

static gboolean
real_trash (FMDirectoryView *view)
{
        trash_or_delete_selected_files (view);
	return TRUE;
}

static void
action_trash_callback (GtkAction *action,
		       gpointer callback_data)
{
        trash_or_delete_selected_files (FM_DIRECTORY_VIEW (callback_data));
}

static gboolean
confirm_delete_directly (FMDirectoryView *view, 
			 GList *uris)
{
	GtkDialog *dialog;
	char *prompt;
	char *file_name;
	int uri_count;
	int response;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Just Say Yes if the preference says not to confirm. */
	if (!confirm_trash_auto_value) {
		return TRUE;
	}

	uri_count = g_list_length (uris);
	g_assert (uri_count > 0);

	if (uri_count == 1) {
		file_name = file_name_from_uri ((char *) uris->data);
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\"?"), 
					  file_name);
		g_free (file_name);
	} else {
		prompt = g_strdup_printf (ngettext("Are you sure you want to permanently delete "
						   "the %d selected item?",
						   "Are you sure you want to permanently delete "
						   "the %d selected items?", uri_count), uri_count);
	}

	dialog = eel_show_yes_no_dialog
		(prompt,
		 _("If you delete an item, it is permanently lost."), 
		 _("Delete?"), GTK_STOCK_DELETE, GTK_STOCK_CANCEL,
		 fm_directory_view_get_containing_window (view));

	g_free (prompt);

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
}

static void
delete_selected_files (FMDirectoryView *view)
{
        GList *selection;
	GList *node;
	GList *file_uris;

	selection = fm_directory_view_get_selection (view);
	if (selection == NULL) {
		return;
	}

	file_uris = NULL;
	for (node = selection; node != NULL; node = node->next) {
		file_uris = g_list_prepend (file_uris,
					    nautilus_file_get_uri ((NautilusFile *) node->data));
	}
	
	if (confirm_delete_directly (view, 
				     file_uris)) {
		nautilus_file_operations_delete (file_uris, GTK_WIDGET (view));
	}
	
	eel_g_list_free_deep (file_uris);
        nautilus_file_list_free (selection);
}

static void
action_delete_callback (GtkAction *action,
			gpointer callback_data)
{
	if (!show_delete_command_auto_value) {
		return;
	}
        delete_selected_files (FM_DIRECTORY_VIEW (callback_data));
}

static gboolean
real_delete (FMDirectoryView *view)
{
	if (!show_delete_command_auto_value) {
		return FALSE;
	}
        delete_selected_files (view);
	return TRUE;
}

static void
action_duplicate_callback (GtkAction *action,
			   gpointer callback_data)
{
        FMDirectoryView *view;
        GList *selection;
        GArray *selected_item_locations;
 
        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		/* FIXME bugzilla.gnome.org 45061:
		 * should change things here so that we use a get_icon_locations (view, selection).
		 * Not a problem in this case but in other places the selection may change by
		 * the time we go and retrieve the icon positions, relying on the selection
		 * staying intact to ensure the right sequence and count of positions is fragile.
		 */
		selected_item_locations = fm_directory_view_get_selected_icon_locations (view);
	        fm_directory_view_duplicate_selection (view, selection, selected_item_locations);
	        g_array_free (selected_item_locations, TRUE);
	}

        nautilus_file_list_free (selection);
}

static void
action_create_link_callback (GtkAction *action,
			     gpointer callback_data)
{
        FMDirectoryView *view;
        GList *selection;
        GArray *selected_item_locations;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		selected_item_locations = fm_directory_view_get_selected_icon_locations (view);
	        fm_directory_view_create_links_for_files (view, selection, selected_item_locations);
	        g_array_free (selected_item_locations, TRUE);
	}

        nautilus_file_list_free (selection);
}

static void
action_select_all_callback (GtkAction *action, 
			    gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_select_all (callback_data);
}

static void
pattern_select_response_cb (GtkWidget *dialog, int response, gpointer user_data)
{
	FMDirectoryView *view;
	GtkWidget *entry;

	view = FM_DIRECTORY_VIEW (user_data);
	
	if (response == GTK_RESPONSE_OK) {
		NautilusDirectory *directory;
		GList *selection;

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		directory = fm_directory_view_get_model (view);
		selection = nautilus_directory_match_pattern (directory,
					gtk_entry_get_text (GTK_ENTRY (entry)));
			
		if (selection) {
			fm_directory_view_set_selection (view, selection);
			nautilus_file_list_free (selection);

			fm_directory_view_reveal_selection(view);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
select_pattern (FMDirectoryView *view)
{
	GtkWidget *dialog;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *entry;
	GList *ret;

	ret = NULL;
	dialog = gtk_dialog_new_with_buttons (_("Select Pattern"),
			fm_directory_view_get_containing_window (view),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK,
			GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	box = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (box), 5);
	label = gtk_label_new_with_mnemonic (_("_Pattern:"));
	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_box_pack_start_defaults (GTK_BOX (box), label);
	gtk_box_pack_start_defaults (GTK_BOX (box), entry);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show_all (box);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), box);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (pattern_select_response_cb),
			  view);
	gtk_widget_show_all (dialog);
}

static void
action_select_pattern_callback (GtkAction *action, 
				gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	select_pattern(callback_data);
}

static void
action_reset_to_defaults_callback (GtkAction *action, 
				   gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_reset_to_defaults (callback_data);
}


static void
action_show_hidden_files_callback (GtkAction *action, 
				   gpointer callback_data)
{
	FMDirectoryView	*directory_view;
	NautilusWindowShowHiddenFilesMode mode;

	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));
	directory_view = FM_DIRECTORY_VIEW (callback_data);
	
	directory_view->details->show_hidden_files = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	
	if (directory_view->details->show_hidden_files) {
		mode = NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_ENABLE;
	} else {
		mode = NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DISABLE;
	}
	nautilus_window_info_set_hidden_files_mode (directory_view->details->window, mode);
	if (directory_view->details->model != NULL) {
		load_directory (directory_view, directory_view->details->model);
	}
}

static void
action_empty_trash_callback (GtkAction *action,
			     gpointer callback_data)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	nautilus_file_operations_empty_trash (GTK_WIDGET (callback_data));
}

static void
action_new_folder_callback (GtkAction *action,
			    gpointer callback_data)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_new_folder (FM_DIRECTORY_VIEW (callback_data));
}

static void
action_new_empty_file_callback (GtkAction *action,
				gpointer callback_data)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_new_file (FM_DIRECTORY_VIEW (callback_data), NULL);
}

static void
action_new_launcher_callback (GtkAction *action,
			      gpointer callback_data)
{
	char *parent_uri;
	FMDirectoryView *view;

	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	view = FM_DIRECTORY_VIEW (callback_data);

	parent_uri = fm_directory_view_get_backing_uri (view);

	nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (view)),
						  "gnome-desktop-item-edit", 
						  "gnome-desktop-item-edit --create-new",
						  parent_uri, 
						  FALSE);

	g_free (parent_uri);
}

static void
action_properties_callback (GtkAction *action,
			    gpointer callback_data)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);

	fm_properties_window_present (selection, GTK_WIDGET (view));

        nautilus_file_list_free (selection);
}

static gboolean
all_files_in_trash (GList *files)
{
	GList *node;

	/* Result is ambiguous if called on NULL, so disallow. */
	g_return_val_if_fail (files != NULL, FALSE);

	for (node = files; node != NULL; node = node->next) {
		if (!nautilus_file_is_in_trash (NAUTILUS_FILE (node->data))) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
all_selected_items_in_trash (FMDirectoryView *view)
{
	GList *selection;
	gboolean result;

	/* If the contents share a parent directory, we need only
	 * check that parent directory. Otherwise we have to inspect
	 * each selected item.
	 */
	selection = fm_directory_view_get_selection (view);
	result = (selection == NULL) ? FALSE : all_files_in_trash (selection);
	nautilus_file_list_free (selection);

	return result;
}

static gboolean
we_are_in_vfolder_desktop_dir (FMDirectoryView *view)
{
	NautilusFile *file;
	char *mime_type;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	if (view->details->model == NULL) {
		return FALSE;
	}

	file = nautilus_directory_get_corresponding_file (view->details->model);
	mime_type = nautilus_file_get_mime_type (file);
	nautilus_file_unref (file);

	if (mime_type != NULL
	    && strcmp (mime_type, "x-directory/vfolder-desktop") == 0) {
		g_free (mime_type);
		return TRUE;
	} else {
		g_free (mime_type);
		return FALSE;
	}
}

/* Preferences changed callbacks */
static void
text_attribute_names_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 text_attribute_names_changed, (view));
}

static void
image_display_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 image_display_policy_changed, (view));
}

static void
click_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 click_policy_changed, (view));
}

gboolean
fm_directory_view_should_sort_directories_first (FMDirectoryView *view)
{
	return view->details->sort_directories_first;
}

static void
sort_directories_first_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;
	gboolean preference_value;

	view = FM_DIRECTORY_VIEW (callback_data);

	preference_value = 
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);

	if (preference_value != view->details->sort_directories_first) {
		view->details->sort_directories_first = preference_value;
		EEL_CALL_METHOD
			(FM_DIRECTORY_VIEW_CLASS, view,
			 sort_directories_first_changed, (view));
	}
}

static void
set_up_scripts_directory_global (void)
{
	char *scripts_directory_path;

	if (scripts_directory_uri != NULL) {
		return;
	}

	scripts_directory_path = gnome_util_home_file ("nautilus-scripts");

	scripts_directory_uri = gnome_vfs_get_uri_from_local_path (scripts_directory_path);
	scripts_directory_uri_length = strlen (scripts_directory_uri);

	if (!g_file_test (scripts_directory_path, G_FILE_TEST_EXISTS)) {
		create_scripts_directory ();
	}
	
	g_free (scripts_directory_path);
}

static void
set_up_templates_directory_global (void)
{
	if (templates_directory_uri != NULL) {
		return;
	}
	
	templates_directory_uri = nautilus_get_templates_directory_uri ();
	templates_directory_uri_length = strlen (templates_directory_uri);
}

static void
create_scripts_directory (void)
{
	char *gnome1_path, *gnome1_uri_str;
	GnomeVFSURI *gnome1_uri, *scripts_uri;

	scripts_uri = gnome_vfs_uri_new (scripts_directory_uri);
	/* try to migrate nautilus 1 scripts */
	gnome1_path = g_strconcat (g_get_home_dir(), "/.gnome/nautilus-scripts", NULL);

	if (g_file_test (gnome1_path, G_FILE_TEST_EXISTS)) {
		gnome1_uri_str = gnome_vfs_get_uri_from_local_path (gnome1_path);
		gnome1_uri = gnome_vfs_uri_new (gnome1_uri_str);
		g_free (gnome1_uri_str);
		if (gnome_vfs_xfer_uri (gnome1_uri, scripts_uri,
					GNOME_VFS_XFER_DEFAULT,
					GNOME_VFS_XFER_ERROR_MODE_ABORT,
					GNOME_VFS_XFER_OVERWRITE_MODE_SKIP,
					NULL, NULL) != GNOME_VFS_OK) {
			g_warning ("Failed to migrate Nautilus1 scripts\n");
		}
		gnome_vfs_uri_unref (gnome1_uri);
	}
	g_free (gnome1_path);

	/* make sure scripts directory is created */
	gnome_vfs_make_directory_for_uri (scripts_uri, 
					  GNOME_VFS_PERM_USER_ALL | GNOME_VFS_PERM_GROUP_ALL | GNOME_VFS_PERM_OTHER_READ);

	gnome_vfs_uri_unref (scripts_uri);
}

static void
scripts_added_or_changed_callback (NautilusDirectory *directory,
				   GList *files,
				   gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	view->details->scripts_invalid = TRUE;
	schedule_update_menus (view);
}

static void
templates_added_or_changed_callback (NautilusDirectory *directory,
				     GList *files,
				     gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	view->details->templates_invalid = TRUE;
	schedule_update_menus (view);
}

static void
icons_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	view->details->scripts_invalid = TRUE;
	view->details->templates_invalid = TRUE;
	schedule_update_menus (view);
}

static void
add_directory_to_directory_list (FMDirectoryView *view,
				 NautilusDirectory *directory,
				 GList **directory_list,
				 GCallback changed_callback)
{
	NautilusFileAttributes attributes;

	if (g_list_find (*directory_list, directory) == NULL) {
		nautilus_directory_ref (directory);

		attributes = nautilus_icon_factory_get_required_file_attributes ();
		attributes |= NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES |
			NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;
 
		nautilus_directory_file_monitor_add (directory, directory_list,
						     FALSE, FALSE, attributes,
						     (NautilusDirectoryCallback)changed_callback, view);

		g_signal_connect_object (directory, "files_added",
					 G_CALLBACK (changed_callback), view, 0);
		g_signal_connect_object (directory, "files_changed",
					 G_CALLBACK (changed_callback), view, 0);

		*directory_list = g_list_append	(*directory_list, directory);
	}
}

static void
remove_directory_from_directory_list (FMDirectoryView *view,
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
add_directory_to_scripts_directory_list (FMDirectoryView *view,
					 NautilusDirectory *directory)
{
	add_directory_to_directory_list (view, directory,
					 &view->details->scripts_directory_list,
					 G_CALLBACK (scripts_added_or_changed_callback));
}

static void
remove_directory_from_scripts_directory_list (FMDirectoryView *view,
					      NautilusDirectory *directory)
{
	remove_directory_from_directory_list (view, directory,
					      &view->details->scripts_directory_list,
					      G_CALLBACK (scripts_added_or_changed_callback));
}

static void
add_directory_to_templates_directory_list (FMDirectoryView *view,
					   NautilusDirectory *directory)
{
	add_directory_to_directory_list (view, directory,
					 &view->details->templates_directory_list,
					 G_CALLBACK (templates_added_or_changed_callback));
}

static void
remove_directory_from_templates_directory_list (FMDirectoryView *view,
						NautilusDirectory *directory)
{
	remove_directory_from_directory_list (view, directory,
					      &view->details->templates_directory_list,
					      G_CALLBACK (templates_added_or_changed_callback));
}

static void
fm_directory_view_set_parent_window (FMDirectoryView *directory_view,
				     NautilusWindowInfo *window)
{
	
	directory_view->details->window = window;

	/* Add new menu items and perhaps whole menus */
	fm_directory_view_merge_menus (directory_view);
	
	/* Set initial sensitivity, wording, toggle state, etc. */       
	fm_directory_view_update_menus (directory_view);

	/* initialise show hidden mode */
	fm_directory_view_init_show_hidden_files (directory_view);
}

static GtkWidget *
fm_directory_view_get_widget (NautilusView *view)
{
	return GTK_WIDGET (view);
}

static int
fm_directory_view_get_selection_count (NautilusView *view)
{
	/* FIXME: This could be faster if we special cased it in subclasses */
	GList *files;
	int len;

	files = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));
	len = g_list_length (files);
	nautilus_file_list_free (files);
	
	return len;
}

static GList *
fm_directory_view_get_selection_uris (NautilusView *view)
{
	GList *files;
	GList *uris;
	char *uri;
	GList *l;

	files = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));
	uris = NULL;
	for (l = files; l != NULL; l = l->next) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));
		uris = g_list_prepend (uris, uri);
	}
	nautilus_file_list_free (files);
	
	return g_list_reverse (uris);
}

static GList *
file_list_from_uri_list (GList *uri_list)
{
	GList *file_list, *node;

	file_list = NULL;
	for (node = uri_list; node != NULL; node = node->next) {
		file_list = g_list_prepend
			(file_list,
			 nautilus_file_get (node->data));
	}
	return g_list_reverse (file_list);
}

static void
fm_directory_view_set_selection_uris (NautilusView *nautilus_view,
				      GList *selection_uris)
{
	GList *selection;
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (nautilus_view);

	if (!view->details->loading) {
		/* If we aren't still loading, set the selection right now,
		 * and reveal the new selection.
		 */
		selection = file_list_from_uri_list (selection_uris);
		view->details->selection_change_is_due_to_shell = TRUE;
		fm_directory_view_set_selection (view, selection);
		view->details->selection_change_is_due_to_shell = FALSE;
		fm_directory_view_reveal_selection (view);
		nautilus_file_list_free (selection);
	} else {
		/* If we are still loading, set the list of pending URIs instead.
		 * done_loading() will eventually select the pending URIs and reveal them.
		 */
		eel_g_list_free_deep (view->details->pending_uris_selected);
		view->details->pending_uris_selected = NULL;

		view->details->pending_uris_selected =
			g_list_concat (view->details->pending_uris_selected,
				       eel_g_str_list_copy (selection_uris));
	}
}


void
fm_directory_view_init_view_iface (NautilusViewIface *iface)
{
	iface->get_widget = fm_directory_view_get_widget;
  	iface->load_location = fm_directory_view_load_location;
	iface->stop_loading = fm_directory_view_stop_loading;

	iface->get_selection_count = fm_directory_view_get_selection_count;
	iface->get_selection = fm_directory_view_get_selection_uris;
	iface->set_selection = fm_directory_view_set_selection_uris;
	
	iface->supports_zooming = (gpointer)fm_directory_view_supports_zooming;
	iface->bump_zoom_level = (gpointer)fm_directory_view_bump_zoom_level;
        iface->zoom_to_level = (gpointer)fm_directory_view_zoom_to_level;
        iface->restore_default_zoom_level = (gpointer)fm_directory_view_restore_default_zoom_level;
        iface->can_zoom_in = (gpointer)fm_directory_view_can_zoom_in;
        iface->can_zoom_out = (gpointer)fm_directory_view_can_zoom_out;
	iface->get_zoom_level = (gpointer)fm_directory_view_get_zoom_level;
}

static void
fm_directory_view_init (FMDirectoryView *view)
{
	static gboolean setup_autos = FALSE;
	NautilusDirectory *scripts_directory;
	NautilusDirectory *templates_directory;

	if (!setup_autos) {
		setup_autos = TRUE;
		eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
						  &confirm_trash_auto_value);
		eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_ENABLE_DELETE,
						  &show_delete_command_auto_value);
	}

	view->details = g_new0 (FMDirectoryViewDetails, 1);

	view->details->non_ready_files = g_hash_table_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	set_up_scripts_directory_global ();
	scripts_directory = nautilus_directory_get (scripts_directory_uri);
	add_directory_to_scripts_directory_list (view, scripts_directory);
	nautilus_directory_unref (scripts_directory);

	set_up_templates_directory_global ();
	templates_directory = nautilus_directory_get (templates_directory_uri);
	add_directory_to_templates_directory_list (view, templates_directory);
	nautilus_directory_unref (templates_directory);

	view->details->sort_directories_first = 
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);

	g_signal_connect_object (nautilus_trash_monitor_get (), "trash_state_changed",
				 G_CALLBACK (fm_directory_view_trash_state_changed_callback), view, 0);

	/* React to icon theme changes. */
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
				 G_CALLBACK (icons_changed_callback),
				 view, G_CONNECT_SWAPPED);

	/* React to clipboard changes */
	g_signal_connect_object (nautilus_clipboard_monitor_get (), "clipboard_changed",
				 G_CALLBACK (clipboard_changed_callback), view, 0);
	
	gtk_widget_show (GTK_WIDGET (view));
	
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
				      schedule_update_menus_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_ENABLE_DELETE,
				      schedule_update_menus_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
				      text_attribute_names_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
				      image_display_policy_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
				      click_policy_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST, 
				      sort_directories_first_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					 filtering_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					 filtering_changed_callback, view);
}

static void
unmerge_ui (FMDirectoryView *view)
{
	GtkUIManager *ui_manager;

	if (view->details->window == NULL) {
		return;
	}
	
	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);

	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->dir_merge_id,
				&view->details->dir_action_group);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->extensions_menu_merge_id,
				&view->details->extensions_menu_action_group);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->open_with_merge_id,
				&view->details->open_with_action_group);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->scripts_merge_id,
				&view->details->scripts_action_group);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->templates_merge_id,
				&view->details->templates_action_group);
}

static void
fm_directory_view_destroy (GtkObject *object)
{
	FMDirectoryView *view;
	GList *node, *next;

	view = FM_DIRECTORY_VIEW (object);

	disconnect_model_handlers (view);

	unmerge_ui (view);
	
	/* We don't own the window, so no unref */
	view->details->window = NULL;
	
	monitor_file_for_open_with (view, NULL);

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	for (node = view->details->scripts_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_scripts_directory_list (view, node->data);
	}

	for (node = view->details->templates_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_templates_directory_list (view, node->data);
	}

	remove_update_menus_timeout_callback (view);
	remove_update_status_idle_callback (view);

	if (view->details->display_selection_idle_id != 0) {
		g_source_remove (view->details->display_selection_idle_id);
		view->details->display_selection_idle_id = 0;
	}

	if (view->details->model) {
		nautilus_directory_unref (view->details->model);
		view->details->model = NULL;
	}
	
	if (view->details->directory_as_file) {
		nautilus_file_unref (view->details->directory_as_file);
		view->details->directory_as_file = NULL;
	}

	fm_directory_view_ignore_hidden_file_preferences (view);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
fm_directory_view_finalize (GObject *object)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (object);

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					 filtering_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					 filtering_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
					 schedule_update_menus_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_ENABLE_DELETE,
					 schedule_update_menus_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
					 text_attribute_names_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					 image_display_policy_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					 click_policy_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
					 sort_directories_first_changed_callback, view);

	g_hash_table_destroy (view->details->non_ready_files);

	g_free (view->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

/**
 * fm_directory_view_display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: FMDirectoryView for which to display selection info.
 * 
 **/
void
fm_directory_view_display_selection_info (FMDirectoryView *view)
{
	GList *selection;
	GnomeVFSFileSize non_folder_size;
	guint non_folder_count, folder_count, folder_item_count;
	gboolean folder_item_count_known;
	guint file_item_count;
	GList *p;
	char *first_item_name;
	char *non_folder_str;
	char *folder_count_str;
	char *folder_item_count_str;
	char *status_string;
	NautilusFile *file;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	selection = fm_directory_view_get_selection (view);
	
	folder_item_count_known = TRUE;
	folder_count = 0;
	folder_item_count = 0;
	non_folder_count = 0;
	non_folder_size = 0;
	first_item_name = NULL;
	folder_count_str = NULL;
	non_folder_str = NULL;
	folder_item_count_str = NULL;
	
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
			non_folder_size += nautilus_file_get_size (file);
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
			folder_count_str = g_strdup_printf (_("\"%s\" selected"), first_item_name);
		} else {
			folder_count_str = g_strdup_printf (ngettext("%d folder selected", 
								     "%d folders selected", 
								     folder_count), 
							    folder_count);
		}

		if (folder_count == 1) {
			if (!folder_item_count_known) {
				folder_item_count_str = g_strdup ("");
			} else {
				folder_item_count_str = g_strdup_printf (ngettext(" (containing %d item)",
										  " (containing %d items)",
										  folder_item_count), 
									 folder_item_count);
			}
		}
		else {
			if (!folder_item_count_known) {
				folder_item_count_str = g_strdup ("");
			} else {
				/* translators: this is preceded with a string of form 'N folders' (N more than 1) */
				folder_item_count_str = g_strdup_printf (ngettext(" (containing a total of %d item)",
										  " (containing a total of %d items)",
										  folder_item_count), 
									 folder_item_count);
			}
			
		}
	}

	if (non_folder_count != 0) {
		char *size_string;

		size_string = gnome_vfs_format_file_size_for_display (non_folder_size);

		if (folder_count == 0) {
			if (non_folder_count == 1) {
				non_folder_str = g_strdup_printf (_("\"%s\" selected (%s)"), 
								  first_item_name,
								  size_string);
			} else {
				non_folder_str = g_strdup_printf (ngettext("%d item selected (%s)",
									   "%d items selected (%s)",
									   non_folder_count), 
								  non_folder_count, 
								  size_string);
			}
		} else {
			/* Folders selected also, use "other" terminology */
			non_folder_str = g_strdup_printf (ngettext("%d other item selected (%s)",
								   "%d other items selected (%s)",
								   non_folder_count), 
							  non_folder_count, 
							  size_string);
		}

		g_free (size_string);
	}

	if (folder_count == 0 && non_folder_count == 0)	{
		char *free_space_str;
		char *item_count_str;
		guint item_count;

		item_count = fm_directory_view_get_item_count (view);
		
		item_count_str = g_strdup_printf (ngettext ("%u item", "%u items", item_count), item_count);

		free_space_str = nautilus_file_get_volume_free_space (view->details->directory_as_file);
		if (free_space_str != NULL) {
			status_string = g_strdup_printf (_("%s, Free space: %s"), item_count_str, free_space_str);
			g_free (free_space_str);
			g_free (item_count_str);
		} else {
			status_string = item_count_str;
		}

	} else if (folder_count == 0) {
		status_string = g_strdup (non_folder_str);
	} else if (non_folder_count == 0) {
		/* No use marking this for translation, since you
		 * can't reorder the strings, which is the main thing
		 * you'd want to do.
		 */
		status_string = g_strdup_printf ("%s%s",
						 folder_count_str, 
						 folder_item_count_str);
	} else {
		/* This is marked for translation in case a localizer
		 * needs to change ", " to something else. The comma
		 * is between the message about the number of folders
		 * and the number of items in those folders and the
		 * message about the number of other items and the
		 * total size of those items.
		 */
		status_string = g_strdup_printf (_("%s%s, %s"), 
						 folder_count_str, 
						 folder_item_count_str,
						 non_folder_str);
	}

	g_free (first_item_name);
	g_free (folder_count_str);
	g_free (folder_item_count_str);
	g_free (non_folder_str);

	nautilus_window_info_set_status (view->details->window,
					 status_string);
	g_free (status_string);
}

void
fm_directory_view_send_selection_change (FMDirectoryView *view)
{
	nautilus_window_info_report_selection_changed (view->details->window);

	view->details->send_selection_change_to_shell = FALSE;
}

static void
fm_directory_view_load_location (NautilusView *nautilus_view,
				 const char *location)
{
	NautilusDirectory *directory;
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (nautilus_view);

	directory = nautilus_directory_get (location);
	load_directory (directory_view, directory);
	nautilus_directory_unref (directory);
}

static void
fm_directory_view_stop_loading (NautilusView *nautilus_view)
{
	fm_directory_view_stop (FM_DIRECTORY_VIEW (nautilus_view));
}

static void
fm_directory_view_file_limit_reached (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	EEL_CALL_METHOD (FM_DIRECTORY_VIEW_CLASS, view,
		 	       file_limit_reached, (view));
}

static void
real_file_limit_reached (FMDirectoryView *view)
{
	NautilusFile *file;
	GtkDialog *dialog;
	char *directory_name;
	char *message;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	file = fm_directory_view_get_directory_as_file (view);
	directory_name = nautilus_file_get_display_name (file);

	/* Note that the number of items actually displayed varies somewhat due
	 * to the way files are collected in batches. So you can't assume that
	 * no more than the constant limit are displayed.
	 */
	message = g_strdup_printf (_("The folder \"%s\" contains more files than "
			             "Nautilus can handle."), 
			           directory_name);
	g_free (directory_name);

	dialog = eel_show_warning_dialog (message,
					  _("Some files will not be displayed."),
					  _("Too Many Files"),
					  fm_directory_view_get_containing_window (view));
	g_free (message);
}

static void
check_for_directory_hard_limit (FMDirectoryView *view)
{
	if (nautilus_directory_file_list_length_reached (view->details->model)) {
		fm_directory_view_file_limit_reached (view);
	}
}


static void
done_loading (FMDirectoryView *view)
{
	GList *uris_selected, *selection;

	if (!view->details->loading) {
		return;
	}

	/* This can be called during destruction, in which case there
	 * is no NautilusWindowInfo any more.
	 */
	if (view->details->window != NULL) {
		nautilus_window_info_report_load_complete (view->details->window, NAUTILUS_VIEW (view));
		schedule_update_menus (view);
		schedule_update_status (view);
		check_for_directory_hard_limit (view);

		uris_selected = view->details->pending_uris_selected;
		if (uris_selected != NULL) {
			view->details->pending_uris_selected = NULL;
			
			selection = file_list_from_uri_list (uris_selected);
			eel_g_list_free_deep (uris_selected);
			

			view->details->selection_change_is_due_to_shell = TRUE;
			fm_directory_view_set_selection (view, selection);
			view->details->selection_change_is_due_to_shell = FALSE;
			fm_directory_view_reveal_selection (view);
			
			nautilus_file_list_free (selection);
		}
		fm_directory_view_display_selection_info (view);
	}

	fm_directory_view_end_loading (view);

	view->details->loading = FALSE;
}

static void
action_reset_background_callback (GtkAction *action,
				  gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	eel_background_reset 
		(fm_directory_view_get_background 
			(FM_DIRECTORY_VIEW (callback_data)));
}

typedef struct {
	GHashTable *debuting_uris;
	GList	   *added_files;
} DebutingUriData;

static void
debuting_uri_data_free (DebutingUriData *data)
{
	g_hash_table_destroy (data->debuting_uris);
	nautilus_file_list_free (data->added_files);
	g_free (data);
}
 
/* This signal handler watch for the arrival of the icons created
 * as the result of a file operation. Once the last one is detected
 * it selects and reveals them all.
 */
static void
debuting_uri_add_file_callback (FMDirectoryView *view,
				NautilusFile *new_file,
				DebutingUriData *data)
{
	char *uri;

	uri = nautilus_file_get_uri (new_file);

	if (g_hash_table_remove (data->debuting_uris, uri)) {
		g_object_ref (new_file);
		data->added_files = g_list_prepend (data->added_files, new_file);

		if (g_hash_table_size (data->debuting_uris) == 0) {
			fm_directory_view_set_selection (view, data->added_files);
			fm_directory_view_reveal_selection (view);
			g_signal_handlers_disconnect_by_func (view,
							      G_CALLBACK (debuting_uri_add_file_callback),
							      data);
		}
	}
	
	g_free (uri);
}

typedef struct {
	GList		*added_files;
	FMDirectoryView *directory_view;
} CopyMoveDoneData;

static void
copy_move_done_data_free (CopyMoveDoneData *data)
{
	g_assert (data != NULL);
	
	eel_remove_weak_pointer (&data->directory_view);
	nautilus_file_list_free (data->added_files);
	g_free (data);
}

static void
pre_copy_move_add_file_callback (FMDirectoryView *view, NautilusFile *new_file, CopyMoveDoneData *data)
{
	g_object_ref (new_file);
	data->added_files = g_list_prepend (data->added_files, new_file);
}

/* This needs to be called prior to nautilus_file_operations_copy_move.
 * It hooks up a signal handler to catch any icons that get added before
 * the copy_done_callback is invoked. The return value should  be passed
 * as the data for copy_move_done_callback.
 */
static CopyMoveDoneData *
pre_copy_move (FMDirectoryView *directory_view)
{
	CopyMoveDoneData *copy_move_done_data;

	copy_move_done_data = g_new0 (CopyMoveDoneData, 1);
	copy_move_done_data->directory_view = directory_view;

	eel_add_weak_pointer (&copy_move_done_data->directory_view);

	/* We need to run after the default handler adds the folder we want to
	 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
	 * must use connect_after.
	 */
	g_signal_connect (directory_view, "add_file",
			  G_CALLBACK (pre_copy_move_add_file_callback), copy_move_done_data);

	return copy_move_done_data;
}

/* This function is used to pull out any debuting uris that were added
 * and (as a side effect) remove them from the debuting uri hash table.
 */
static gboolean
copy_move_done_partition_func (gpointer data, gpointer callback_data)
{
 	char *uri;
 	gboolean result;
 	
	uri = nautilus_file_get_uri (NAUTILUS_FILE (data));
	result = g_hash_table_remove ((GHashTable *) callback_data, uri);
	g_free (uri);

	return result;
}

static gboolean
remove_not_really_moved_files (gpointer key,
			       gpointer value,
			       gpointer callback_data)
{
	GList **added_files;

	if (GPOINTER_TO_INT (value)) {
		return FALSE;
	}
	
	added_files = callback_data;
	*added_files = g_list_prepend (*added_files,
				       nautilus_file_get (key));
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
copy_move_done_callback (GHashTable *debuting_uris, gpointer data)
{
	FMDirectoryView  *directory_view;
	CopyMoveDoneData *copy_move_done_data;
	DebutingUriData  *debuting_uri_data;

	copy_move_done_data = (CopyMoveDoneData *) data;
	directory_view = copy_move_done_data->directory_view;

	if (directory_view != NULL) {
		g_assert (FM_IS_DIRECTORY_VIEW (directory_view));
	
		debuting_uri_data = g_new (DebutingUriData, 1);
		debuting_uri_data->debuting_uris = debuting_uris;
		debuting_uri_data->added_files = eel_g_list_partition
			(copy_move_done_data->added_files,
			 copy_move_done_partition_func,
			 debuting_uris,
			 &copy_move_done_data->added_files);

		/* We're passed the same data used by pre_copy_move_add_file_callback, so disconnecting
		 * it will free data. We've already siphoned off the added_files we need, and stashed the
		 * directory_view pointer.
		 */
		g_signal_handlers_disconnect_by_func (directory_view,
						      G_CALLBACK (pre_copy_move_add_file_callback),
						      data);
	
		/* Any items in the debuting_uris hash table that have
		 * "FALSE" as their value aren't really being copied
		 * or moved, so we can't wait for an add_file signal
		 * to come in for those.
		 */
		g_hash_table_foreach_remove (debuting_uris,
					     remove_not_really_moved_files,
					     &debuting_uri_data->added_files);
		
		if (g_hash_table_size (debuting_uris) == 0) {
			/* on the off-chance that all the icons have already been added */
			if (debuting_uri_data->added_files != NULL) {
				fm_directory_view_set_selection (directory_view,
								 debuting_uri_data->added_files);
				fm_directory_view_reveal_selection (directory_view);
			}
			debuting_uri_data_free (debuting_uri_data);
		} else {
			/* We need to run after the default handler adds the folder we want to
			 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
			 * must use connect_after.
			 */
			g_signal_connect_data (GTK_OBJECT (directory_view),
					       "add_file",
					       G_CALLBACK (debuting_uri_add_file_callback),
					       debuting_uri_data,
					       (GClosureNotify) debuting_uri_data_free,
					       G_CONNECT_AFTER);
		}
	}

	copy_move_done_data_free (copy_move_done_data);
}

static gboolean
real_file_still_belongs (FMDirectoryView *view, NautilusFile *file)
{
	return nautilus_directory_contains_file (view->details->model, file);
}

static gboolean
still_should_show_file (FMDirectoryView *view, NautilusFile *file)
{
	return fm_directory_view_should_show_file (view, file)
		&& EEL_INVOKE_METHOD (FM_DIRECTORY_VIEW_CLASS, view, file_still_belongs, (view, file));
}

static gboolean
ready_to_load (NautilusFile *file)
{
	return nautilus_icon_factory_is_icon_ready_for_file (file);
}

/* Go through all the new added and changed files.
 * Put any that are not ready to load in the non_ready_files hash table.
 * Add all the rest to the old_added_files and old_changed_files lists.
 * Sort the old_*_files lists if anything was added to them.
 */
static void
process_new_files (FMDirectoryView *view)
{
	GList *new_added_files, *new_changed_files, *old_added_files, *old_changed_files;
	GHashTable *non_ready_files;
	GList *node;
	NautilusFile *file;
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
	for (node = new_added_files; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);
		in_non_ready = g_hash_table_lookup (non_ready_files, file) != NULL;
		if (fm_directory_view_should_show_file (view, file)) {
			if (ready_to_load (file)) {
				if (in_non_ready) {
					g_hash_table_remove (non_ready_files, file);
					nautilus_file_unref (file);
				}
				nautilus_file_ref (file);
				old_added_files = g_list_prepend (old_added_files, file);
			} else {
				if (!in_non_ready) {
					nautilus_file_ref (file);
					g_hash_table_insert (non_ready_files, file, file);
				}
			}
		}
	}
	nautilus_file_list_free (new_added_files);

	/* Newly changed files go into the old_added_files list if they're ready
	 * and were seen non-ready in the past, into the old_changed_files list
	 * if they are read and were not seen non-ready in the past, and into
	 * the hash table if they're not ready.
	 */
	for (node = new_changed_files; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);
		if (ready_to_load (file) || !still_should_show_file (view, file)) {
			if (g_hash_table_lookup (non_ready_files, file) != NULL) {
				g_hash_table_remove (non_ready_files, file);
				nautilus_file_unref (file);
				if (still_should_show_file (view, file)) {
					nautilus_file_ref (file);
					old_added_files = g_list_prepend (old_added_files, file);
				}
			} else if (fm_directory_view_should_show_file(view, file)) {
				nautilus_file_ref (file);
				old_changed_files = g_list_prepend 
					(old_changed_files, file);
			}
		}
	}
	nautilus_file_list_free (new_changed_files);

	/* If any files were added to old_added_files, then resort it. */
	if (old_added_files != view->details->old_added_files) {
		view->details->old_added_files = old_added_files;
		EEL_INVOKE_METHOD (FM_DIRECTORY_VIEW_CLASS, view, sort_files,
				   (view, &view->details->old_added_files));
	}

	/* Resort old_changed_files too, since file attributes
	 * relevant to sorting could have changed.
	 */
	if (old_changed_files != view->details->old_changed_files) {
		view->details->old_changed_files = old_changed_files;
		EEL_INVOKE_METHOD (FM_DIRECTORY_VIEW_CLASS, view, sort_files,
				   (view, &view->details->old_changed_files));
	}

}

static void
process_old_files (FMDirectoryView *view)
{
	GList *files_added, *files_changed, *node;
	NautilusFile *file;
	GList *selection;
	gboolean send_selection_change;

	files_added = view->details->old_added_files;
	files_changed = view->details->old_changed_files;
	
	send_selection_change = FALSE;

	if (files_added != NULL || files_changed != NULL) {
		g_signal_emit (view, signals[BEGIN_FILE_CHANGES], 0);

		for (node = files_added; node != NULL; node = node->next) {
			file = NAUTILUS_FILE (node->data);
			g_signal_emit (view,
				       signals[ADD_FILE], 0, file);
		}

		for (node = files_changed; node != NULL; node = node->next) {
			file = NAUTILUS_FILE (node->data);
			
			g_signal_emit (view,
				       signals[still_should_show_file (view, file)
					       ? FILE_CHANGED : REMOVE_FILE], 0,
				       file);
		}

		g_signal_emit (view, signals[END_FILE_CHANGES], 0);

		if (files_changed != NULL) {
			selection = fm_directory_view_get_selection (view);
			send_selection_change = eel_g_lists_sort_and_check_for_intersection
				(&files_changed, &selection);
			nautilus_file_list_free (selection);
		}

		nautilus_file_list_free (view->details->old_added_files);
		view->details->old_added_files = NULL;

		nautilus_file_list_free (view->details->old_changed_files);
		view->details->old_changed_files = NULL;
	}

	if (send_selection_change) {
		/* Send a selection change since some file names could
		 * have changed.
		 */
		fm_directory_view_send_selection_change (view);
	}
}

static void
display_pending_files (FMDirectoryView *view)
{
	process_new_files (view);
	process_old_files (view);

	if (view->details->model != NULL
	    && nautilus_directory_are_all_files_seen (view->details->model)
	    && g_hash_table_size (view->details->non_ready_files) == 0) {
		done_loading (view);
	}
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->display_selection_idle_id = 0;
	fm_directory_view_display_selection_info (view);
	if (view->details->send_selection_change_to_shell) {
		fm_directory_view_send_selection_change (view);
	}

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static void
remove_update_menus_timeout_callback (FMDirectoryView *view) 
{
	if (view->details->update_menus_timeout_id != 0) {
		g_source_remove (view->details->update_menus_timeout_id);
		view->details->update_menus_timeout_id = 0;
	}
}

static void
update_menus_if_pending (FMDirectoryView *view)
{
	GList *selection;

	/* We need to monitor the mime list for the open with file
	 * so we can get the menu right, but we only do this
	 * on actual menu popup since this can do I/O.
	 */
	selection = fm_directory_view_get_selection (view);
	if (eel_g_list_exactly_one_item (selection)) {
		monitor_file_for_open_with (view, NAUTILUS_FILE (selection->data));
	}
	nautilus_file_list_free (selection);

	
	if (!view->details->menu_states_untrustworthy) {
		return;
	}

	remove_update_menus_timeout_callback (view);
	fm_directory_view_update_menus (view);
}

static gboolean
update_menus_timeout_callback (gpointer data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->update_menus_timeout_id = 0;
	fm_directory_view_update_menus (view);

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static gboolean
display_pending_idle_callback (gpointer data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);

	g_object_ref (G_OBJECT (view));

	display_pending_files (view);

	view->details->display_pending_idle_id = 0;

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static void
schedule_idle_display_of_pending_files (FMDirectoryView *view)
{
	/* No need to schedule an idle if there's already one pending. */
	if (view->details->display_pending_idle_id != 0) {
		return;
	}

	/* We want higher priority than the idle that handles the relayout
	   to avoid a resort on each add. But we still want to allow repaints
	   and other hight prio events while we have pending files to show. */
	view->details->display_pending_idle_id =
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
				 display_pending_idle_callback, view, NULL);
}

static void
unschedule_idle_display_of_pending_files (FMDirectoryView *view)
{
	/* Get rid of idle if it's active. */
	if (view->details->display_pending_idle_id != 0) {
		g_source_remove (view->details->display_pending_idle_id);
		view->details->display_pending_idle_id = 0;
	}
}

static void
unschedule_display_of_pending_files (FMDirectoryView *view)
{
	unschedule_idle_display_of_pending_files (view);
}

static void
queue_pending_files (FMDirectoryView *view,
		     GList *files,
		     GList **pending_list)
{
	if (files == NULL) {
		return;
	}

	*pending_list = g_list_concat (nautilus_file_list_copy (files),
				       *pending_list);

	if (! view->details->loading || nautilus_directory_are_all_files_seen (view->details->model)) {
		schedule_idle_display_of_pending_files (view);
	}
}

static void
files_added_callback (NautilusDirectory *directory,
		      GList *files,
		      gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->new_added_files);

	/* The number of items could have changed */
	schedule_update_status (view);
}

static void
files_changed_callback (NautilusDirectory *directory,
			GList *files,
			gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->new_changed_files);
	
	/* The free space or the number of items could have changed */
	schedule_update_status (view);

	/* A change in MIME type could affect the Open with menu, for
	 * one thing, so we need to update menus when files change.
	 */
	schedule_update_menus (view);
}

static void
done_loading_callback (NautilusDirectory *directory,
		       gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	
	process_new_files (view);
	if (g_hash_table_size (view->details->non_ready_files) == 0) {
		schedule_idle_display_of_pending_files (view);
	}
}

static void
load_error_callback (NautilusDirectory *directory,
		     GnomeVFSResult load_error_code,
		     gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	/* FIXME: By doing a stop, we discard some pending files. Is
	 * that OK?
	 */
	fm_directory_view_stop (view);

	/* Emit a signal to tell subclasses that a load error has
	 * occurred, so they can handle it in the UI.
	 */
	g_signal_emit (view,
			 signals[LOAD_ERROR], 0, load_error_code);
}

static void
real_load_error (FMDirectoryView *view, GnomeVFSResult result)
{
	g_assert (result != GNOME_VFS_OK);

	/* Report only one error per failed directory load (from the UI
	 * point of view, not from the NautilusDirectory point of view).
	 * Otherwise you can get multiple identical errors caused by 
	 * unrelated code that just happens to try to iterate this
	 * directory.
	 */
	if (!view->details->reported_load_error) {
		fm_report_error_loading_directory 
			(fm_directory_view_get_directory_as_file (view), 
			 result,
			 fm_directory_view_get_containing_window (view));
	}
	view->details->reported_load_error = TRUE;
}

/**
 * fm_directory_queue_notice_file_change
 * 
 * Called by a subclass to put a file into the queue of files to update.
 * This is only necessary when the subclass is monitoring files other than
 * the ones in the directory for this location.
 */
void
fm_directory_view_queue_file_change (FMDirectoryView *view, NautilusFile *file)
{
	GList singleton_list;

	singleton_list.data = file;
	singleton_list.next = NULL;
	singleton_list.prev = NULL;
	queue_pending_files (view, &singleton_list, &view->details->new_changed_files);
}

/**
 * fm_directory_view_clear:
 *
 * Emit the signal to clear the contents of the view. Subclasses must
 * override the signal handler for this signal. This is normally called
 * only by FMDirectoryView.
 * @view: FMDirectoryView to empty.
 * 
 **/
void
fm_directory_view_clear (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	g_signal_emit (view, signals[CLEAR], 0);
}

/**
 * fm_directory_view_begin_loading:
 *
 * Emit the signal to prepare for loading the contents of a new location. 
 * Subclasses might want to override the signal handler for this signal. 
 * This is normally called only by FMDirectoryView.
 * @view: FMDirectoryView that is switching to view a new location.
 * 
 **/
void
fm_directory_view_begin_loading (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	g_signal_emit (view, signals[BEGIN_LOADING], 0);
}

/**
 * fm_directory_view_end_loading:
 *
 * Emit the signal after loading the contents of a new location. 
 * Subclasses might want to override the signal handler for this signal. 
 * This is normally called only by FMDirectoryView.
 * @view: FMDirectoryView that is switching to view a new location.
 * 
 **/
void
fm_directory_view_end_loading (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	g_signal_emit (view, signals[END_LOADING], 0);
}

/**
 * fm_directory_view_bump_zoom_level:
 *
 * bump the current zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
fm_directory_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!fm_directory_view_supports_zooming (view)) {
		return;
	}

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 bump_zoom_level, (view, zoom_increment));
}

/**
 * fm_directory_view_zoom_to_level:
 *
 * Set the current zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
fm_directory_view_zoom_to_level (FMDirectoryView *view,
				 NautilusZoomLevel zoom_level)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!fm_directory_view_supports_zooming (view)) {
		return;
	}

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 zoom_to_level, (view, zoom_level));
}


NautilusZoomLevel
fm_directory_view_get_zoom_level (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);

	if (!fm_directory_view_supports_zooming (view)) {
		return NAUTILUS_ZOOM_LEVEL_STANDARD;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_zoom_level, (view));
}

/**
 * fm_directory_view_restore_default_zoom_level:
 *
 * restore to the default zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
fm_directory_view_restore_default_zoom_level (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!fm_directory_view_supports_zooming (view)) {
		return;
	}

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 restore_default_zoom_level, (view));
}

/**
 * fm_directory_view_can_zoom_in:
 *
 * Determine whether the view can be zoomed any closer.
 * @view: The zoomable FMDirectoryView.
 * 
 * Return value: TRUE if @view can be zoomed any closer, FALSE otherwise.
 * 
 **/
gboolean
fm_directory_view_can_zoom_in (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	if (!fm_directory_view_supports_zooming (view)) {
		return FALSE;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 can_zoom_in, (view));
}

/**
 * fm_directory_view_can_rename_file
 *
 * Determine whether a file can be renamed.
 * @file: A NautilusFile
 * 
 * Return value: TRUE if @file can be renamed, FALSE otherwise.
 * 
 **/
static gboolean
fm_directory_view_can_rename_file (FMDirectoryView *view, NautilusFile *file)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 can_rename_file, (view, file));
}

/**
 * fm_directory_view_can_zoom_out:
 *
 * Determine whether the view can be zoomed any further away.
 * @view: The zoomable FMDirectoryView.
 * 
 * Return value: TRUE if @view can be zoomed any further away, FALSE otherwise.
 * 
 **/
gboolean
fm_directory_view_can_zoom_out (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	if (!fm_directory_view_supports_zooming (view)) {
		return FALSE;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 can_zoom_out, (view));
}

GtkWidget *
fm_directory_view_get_background_widget (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_background_widget, (view));
}

EelBackground *
fm_directory_view_get_background (FMDirectoryView *view)
{
	return eel_get_widget_background (fm_directory_view_get_background_widget (view));
}

/**
 * fm_directory_view_get_selection:
 *
 * Get a list of NautilusFile pointers that represents the
 * currently-selected items in this view. Subclasses must override
 * the signal handler for the 'get_selection' signal. Callers are
 * responsible for g_free-ing the list (but not its data).
 * @view: FMDirectoryView whose selected items are of interest.
 * 
 * Return value: GList of NautilusFile pointers representing the selection.
 * 
 **/
GList *
fm_directory_view_get_selection (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_selection, (view));
}

guint
fm_directory_view_get_item_count (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), 0);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_item_count, (view));
}

GtkUIManager *
fm_directory_view_get_ui_manager (FMDirectoryView  *view)
{
	if (view->details->window == NULL) {
		return NULL;
	}
	return nautilus_window_info_get_ui_manager (view->details->window);	
}

/**
 * fm_directory_view_get_model:
 *
 * Get the model for this FMDirectoryView.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/
NautilusDirectory *
fm_directory_view_get_model (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return view->details->model;
}

static void
prepend_uri_one (gpointer data, gpointer callback_data)
{
	NautilusFile *file;
	GList **result;
	
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (callback_data != NULL);

	result = (GList **) callback_data;
	file = (NautilusFile *) data;
	*result = g_list_prepend (*result, nautilus_file_get_uri (file));
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

static void
fm_directory_view_create_links_for_files (FMDirectoryView *view, GList *files,
					  GArray *relative_item_points)
{
	GList *uris;
	CopyMoveDoneData *copy_move_done_data;
	g_assert (relative_item_points->len == 0
		  || g_list_length (files) == relative_item_points->len);
	
        g_assert (FM_IS_DIRECTORY_VIEW (view));
        g_assert (files != NULL);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, prepend_uri_one, &uris);
	uris = g_list_reverse (uris);

        g_assert (g_list_length (uris) == g_list_length (files));

	/* offset the drop locations a bit so that we don't pile
	 * up the icons on top of each other
	 */
	offset_drop_points (relative_item_points,
			    DUPLICATE_HORIZONTAL_ICON_OFFSET,
			    DUPLICATE_VERTICAL_ICON_OFFSET);

        copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move (uris, relative_item_points, NULL, GDK_ACTION_LINK, 
		GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	eel_g_list_free_deep (uris);
}

static void
fm_directory_view_duplicate_selection (FMDirectoryView *view, GList *files,
				       GArray *relative_item_points)
{
	GList *uris;
	CopyMoveDoneData *copy_move_done_data;

        g_assert (FM_IS_DIRECTORY_VIEW (view));
        g_assert (files != NULL);
	g_assert (g_list_length (files) == relative_item_points->len
		|| relative_item_points->len == 0);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, prepend_uri_one, &uris);
	uris = g_list_reverse (uris);

        g_assert (g_list_length (uris) == g_list_length (files));
        
	/* offset the drop locations a bit so that we don't pile
	 * up the icons on top of each other
	 */
	offset_drop_points (relative_item_points,
			    DUPLICATE_HORIZONTAL_ICON_OFFSET,
			    DUPLICATE_VERTICAL_ICON_OFFSET);

        copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move (uris, relative_item_points, NULL, GDK_ACTION_COPY,
		GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	eel_g_list_free_deep (uris);
}

/* special_link_in_selection
 * 
 * Return TRUE if one of our special links is in the selection.
 * Special links include the following: 
 *	 NAUTILUS_DESKTOP_LINK_TRASH, NAUTILUS_DESKTOP_LINK_HOME, NAUTILUS_DESKTOP_LINK_MOUNT
 */
 
static gboolean
special_link_in_selection (FMDirectoryView *view)
{
	gboolean saw_link;
	GList *selection, *node;
	NautilusFile *file;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	saw_link = FALSE;

	selection = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));

	for (node = selection; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		saw_link = NAUTILUS_IS_DESKTOP_ICON_FILE (file);
		
		if (saw_link) {
			break;
		}
	}
	
	nautilus_file_list_free (selection);
	
	return saw_link;
}

static gboolean
can_move_uri_to_trash (FMDirectoryView *view, const char *file_uri_string)
{
	/* Return TRUE if we can get a trash directory on the same volume as this file. */
	GnomeVFSURI *file_uri;
	GnomeVFSURI *directory_uri;
	GnomeVFSURI *trash_dir_uri;
	gboolean result;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);
	g_return_val_if_fail (file_uri_string != NULL, FALSE);

	file_uri = gnome_vfs_uri_new (file_uri_string);

	if (file_uri == NULL) {
		return FALSE;
	}

	/* FIXME: Why can't we just pass file_uri to gnome_vfs_find_directory? */
	directory_uri = gnome_vfs_uri_get_parent (file_uri);
	gnome_vfs_uri_unref (file_uri);

	if (directory_uri == NULL) {
		return FALSE;
	}

	/*
	 * Create a new trash if needed but don't go looking for an old Trash.
	 * Passing 0 permissions as gnome-vfs would override the permissions 
	 * passed with 700 while creating .Trash directory
	 */
	result = gnome_vfs_find_directory (directory_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
					   &trash_dir_uri, TRUE, FALSE, 0) == GNOME_VFS_OK;
	if (result) {
		gnome_vfs_uri_unref (trash_dir_uri);
	}
	gnome_vfs_uri_unref (directory_uri);

	return result;
}

static gboolean
can_delete_uri_without_confirm (const char *file_uri_string)
{
	if (eel_istr_has_prefix (file_uri_string, "burn:") != FALSE) {
		return TRUE;
	}

	return FALSE;
}

static char *
file_name_from_uri (const char *uri)
{
	NautilusFile *file;
	char *file_name;
	
	file = nautilus_file_get (uri);
	file_name = nautilus_file_get_display_name (file);
	nautilus_file_unref (file);

	return file_name;	
}

static gboolean
fm_directory_view_confirm_deletion (FMDirectoryView *view, GList *uris, gboolean all)
{
	GtkDialog *dialog;
	char *prompt;
	char *detail;
	int uri_count;
	char *uri;
	char *file_name;
	int response;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	uri_count = g_list_length (uris);
	g_assert (uri_count > 0);
	
	if (uri_count == 1) {
		uri = (char *) uris->data;
		if (eel_uri_is_desktop (uri)) {
			/* Don't ask for desktop icons */
			return TRUE;
		}
		file_name = file_name_from_uri (uri);
		prompt = _("Cannot move file to trash, do you want to delete immediately?");
		detail = g_strdup_printf (_("The file \"%s\" cannot be moved to the trash."), file_name);
		g_free (file_name);
	} else {
		if (all) {
			prompt = _("Cannot move items to trash, do you want to delete them immediately?");
			detail = g_strdup_printf ("None of the %d selected items can be moved to the Trash", uri_count);
		} else {
			prompt = _("Cannot move some items to trash, do you want to delete these immediately?");
			detail = g_strdup_printf ("%d of the selected items cannot be moved to the Trash", uri_count);
		}
	}

	dialog = eel_show_yes_no_dialog
		(prompt,
		 detail, _("Delete Immediately?"), GTK_STOCK_DELETE, GTK_STOCK_CANCEL,
		 fm_directory_view_get_containing_window (view));
	
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
}

static gboolean
confirm_delete_from_trash (FMDirectoryView *view, GList *uris)
{
	GtkDialog *dialog;
	char *prompt;
	char *file_name;
	int uri_count;
	int response;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Just Say Yes if the preference says not to confirm. */
	if (!confirm_trash_auto_value) {
		return TRUE;
	}

	uri_count = g_list_length (uris);
	g_assert (uri_count > 0);

	if (uri_count == 1) {
		file_name = file_name_from_uri ((char *) uris->data);
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\" "
					    "from the trash?"), file_name);
		g_free (file_name);
	} else {
		prompt = g_strdup_printf (ngettext("Are you sure you want to permanently delete "
						   "the %d selected item from the trash?",
						   "Are you sure you want to permanently delete "
						   "the %d selected items from the trash?",
						   uri_count), 
					  uri_count);
	}

	dialog = eel_show_yes_no_dialog (
		prompt, _("If you delete an item, it will be permanently lost."), 
		_("Delete From Trash?"), GTK_STOCK_DELETE, GTK_STOCK_CANCEL,
		fm_directory_view_get_containing_window (view));

	g_free (prompt);
	
	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
}

static void
trash_or_delete_files_common (FMDirectoryView *view,
			      const GList *file_uris,
			      GArray *relative_item_points,
			      gboolean delete_if_all_already_in_trash)
{
	const GList *file_node;
	char *file_uri;
	GList *moveable_uris;
	GList *unmoveable_uris;
	GList *in_trash_uris;
	GList *no_confirm_uris;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Collect three lists: (1) items that can be moved to trash,
	 * (2) items that can only be deleted in place, and (3) items that
	 * are already in trash. 
	 * 
	 * Always move (1) to trash if non-empty.
	 * Delete (3) only if (1) and (2) are non-empty, otherwise ignore (3).
	 * Ask before deleting (2) if non-empty.
	 * Ask before deleting (3) if non-empty.
	 */

	moveable_uris = NULL;
	unmoveable_uris = NULL;
	in_trash_uris = NULL;
	no_confirm_uris = NULL;
	
	for (file_node = file_uris; file_node != NULL; file_node = file_node->next) {
		file_uri = (char *) file_node->data;
		
		if (delete_if_all_already_in_trash && eel_uri_is_in_trash (file_uri)) {
			in_trash_uris = g_list_prepend (in_trash_uris, g_strdup (file_uri));
		} else if (can_delete_uri_without_confirm (file_uri)) {
			no_confirm_uris = g_list_prepend (no_confirm_uris, g_strdup (file_uri));
		} else if (can_move_uri_to_trash (view, file_uri)) {
			moveable_uris = g_list_prepend (moveable_uris, g_strdup (file_uri));
		} else {
			unmoveable_uris = g_list_prepend (unmoveable_uris, g_strdup (file_uri));
		}
	}

	if (in_trash_uris != NULL && moveable_uris == NULL && unmoveable_uris == NULL) {
		if (confirm_delete_from_trash (view, in_trash_uris)) {
			nautilus_file_operations_delete (in_trash_uris, GTK_WIDGET (view));
		}
	} else {
		if (no_confirm_uris != NULL) {
			nautilus_file_operations_delete (no_confirm_uris,
							 GTK_WIDGET (view));
		}
		if (moveable_uris != NULL) {
			nautilus_file_operations_copy_move (moveable_uris, relative_item_points, 
							    EEL_TRASH_URI, GDK_ACTION_MOVE, GTK_WIDGET (view),
							    copy_move_done_callback, pre_copy_move (view));
		}
		if (unmoveable_uris != NULL) {
			if (fm_directory_view_confirm_deletion (view, 
								unmoveable_uris,
								moveable_uris == NULL)) {
				nautilus_file_operations_delete (unmoveable_uris, GTK_WIDGET (view));
			}
		}
	}
	
	eel_g_list_free_deep (in_trash_uris);
	eel_g_list_free_deep (moveable_uris);
	eel_g_list_free_deep (unmoveable_uris);
	eel_g_list_free_deep (no_confirm_uris);
}

static void
trash_or_delete_files (FMDirectoryView *view,
		       const GList *files)
{
	GList *file_uris;
	const GList *node;
	
	file_uris = NULL;
	for (node = files; node != NULL; node = node->next) {
		file_uris = g_list_prepend (file_uris,
					    nautilus_file_get_uri ((NautilusFile *) node->data));
	}
	
	file_uris = g_list_reverse (file_uris);
	trash_or_delete_files_common (view, file_uris, NULL, TRUE);
	eel_g_list_free_deep (file_uris);
}

static gboolean
can_rename_file (FMDirectoryView *view, NautilusFile *file)
{
	return nautilus_file_can_rename (file);
}

static void
start_renaming_file (FMDirectoryView *view, NautilusFile *file)
{
	if (file !=  NULL) {
		fm_directory_view_select_file (view, file);
	}
}

typedef struct {
	FMDirectoryView *view;
	NautilusFile *new_file;
} RenameData;

static gboolean
delayed_rename_file_hack_callback (RenameData *data)
{
	FMDirectoryView *view;
	NautilusFile *new_file;

	view = data->view;
	new_file = data->new_file;

	EEL_CALL_METHOD (FM_DIRECTORY_VIEW_CLASS, view, start_renaming_file, (view, new_file));
	fm_directory_view_reveal_selection (view);
	
	g_object_unref (data->view);
	nautilus_file_unref (data->new_file);
	g_free (data);

	return FALSE;
}

static void
rename_file (FMDirectoryView *view, NautilusFile *new_file)
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
	if (FM_IS_LIST_VIEW (view)) {
		fm_directory_view_select_file (view, new_file);
		
		data = g_new (RenameData, 1);
		data->view = g_object_ref (view);
		data->new_file = nautilus_file_ref (new_file);
		g_timeout_add (100, (GSourceFunc)delayed_rename_file_hack_callback,
			       data);
		
		return;
	}
	
	/* no need to select because start_renaming_file selects
	 * fm_directory_view_select_file (view, new_file);
	 */
	EEL_CALL_METHOD (FM_DIRECTORY_VIEW_CLASS, view, start_renaming_file, (view, new_file));
	fm_directory_view_reveal_selection (view);
}

static void
reveal_newly_added_folder (FMDirectoryView *view, NautilusFile *new_file, const char *target_uri)
{
	if (nautilus_file_matches_uri (new_file, target_uri)) {
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (reveal_newly_added_folder),
						      (void *) target_uri);
		rename_file (view, new_file);
	}
}

typedef struct {
	FMDirectoryView *directory_view;
	GHashTable *added_uris;
} NewFolderData;


static void
track_newly_added_uris (FMDirectoryView *view, NautilusFile *new_file, gpointer user_data)
{
	NewFolderData *data;

	data = user_data;

	g_hash_table_insert (data->added_uris, nautilus_file_get_uri (new_file), NULL);
}

static void
new_folder_done (const char *new_folder_uri, gpointer user_data)
{
	FMDirectoryView *directory_view;
	NautilusFile *file;
	char *screen_string;
	GdkScreen *screen;
	NewFolderData *data;

	data = (NewFolderData *)user_data;
	
	directory_view = data->directory_view;
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));

	g_signal_handlers_disconnect_by_func (directory_view,
					      G_CALLBACK (track_newly_added_uris),
					      (void *) data);

	if (new_folder_uri == NULL) {
		goto fail;
	}
	
	screen = gtk_widget_get_screen (GTK_WIDGET (directory_view));
	screen_string = g_strdup_printf ("%d", gdk_screen_get_number (screen));

	file = nautilus_file_get (new_folder_uri);
	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_SCREEN,
		 NULL,
		 screen_string);
	g_free (screen_string);

	if (g_hash_table_lookup_extended (data->added_uris, new_folder_uri, NULL, NULL)) {
		/* The file was already added */
		rename_file (directory_view, file);
	} else {
		/* We need to run after the default handler adds the folder we want to
		 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
		 * must use connect_after.
		 */
		g_signal_connect_data (directory_view,
				       "add_file",
				       G_CALLBACK (reveal_newly_added_folder),
				       g_strdup (new_folder_uri),
				       (GClosureNotify)g_free,
				       G_CONNECT_AFTER);
	}

 fail:
	g_hash_table_destroy (data->added_uris);
	g_free (data);
}

void
fm_directory_view_new_folder (FMDirectoryView *directory_view)
{
	char *parent_uri;
	NewFolderData *data;

	data = g_new (NewFolderData, 1);
	data->directory_view = directory_view;
	data->added_uris = g_hash_table_new_full (g_str_hash, g_str_equal,
						  g_free, NULL);
	
	g_signal_connect_data (directory_view,
			       "add_file",
			       G_CALLBACK (track_newly_added_uris),
			       data,
			       (GClosureNotify)NULL,
			       G_CONNECT_AFTER);

	parent_uri = fm_directory_view_get_backing_uri (directory_view);
	nautilus_file_operations_new_folder (GTK_WIDGET (directory_view),
					     parent_uri,
					     new_folder_done, data);

	g_free (parent_uri);
}

void
fm_directory_view_new_file (FMDirectoryView *directory_view,
			    NautilusFile *source)
{
	char *parent_uri;
	char *source_uri;
	NewFolderData *data;

	data = g_new (NewFolderData, 1);
	data->directory_view = directory_view;
	data->added_uris = g_hash_table_new_full (g_str_hash, g_str_equal,
						  g_free, NULL);

	g_signal_connect_data (directory_view,
			       "add_file",
			       G_CALLBACK (track_newly_added_uris),
			       data,
			       (GClosureNotify)NULL,
			       G_CONNECT_AFTER);

	
	source_uri = NULL;
	if (source != NULL) {
		source_uri = nautilus_file_get_uri (source);
	}
	parent_uri = fm_directory_view_get_backing_uri (directory_view);
	nautilus_file_operations_new_file (GTK_WIDGET (directory_view),
					   parent_uri,
					   source_uri,
					   new_folder_done, data);
	g_free (parent_uri);
	g_free (source_uri);
}


/* handle the open command */

static void
open_one_in_new_window (gpointer data, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_activate_file (FM_DIRECTORY_VIEW (callback_data),
					 NAUTILUS_FILE (data),
					 NAUTILUS_WINDOW_OPEN_IN_NAVIGATION,
					 0);
}

NautilusFile *
fm_directory_view_get_directory_as_file (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	return view->details->directory_as_file; 
}

static void
open_with_launch_application_callback (GtkAction *action,
				       gpointer callback_data)
{
	ApplicationLaunchParameters *launch_parameters;
	
	launch_parameters = (ApplicationLaunchParameters *) callback_data;
	fm_directory_view_launch_application 
		(launch_parameters->application,
		 launch_parameters->file,
		 launch_parameters->directory_view);
}

static char *
escape_action_name (const char *action_name,
		    const char *prefix)
{
	GString *s;

	if (action_name == NULL) {
		return NULL;
	}
	
	s = g_string_new (prefix);

	while (*action_name != 0) {
		switch (*action_name) {
		case '\\':
			g_string_append (s, "\\\\");
			break;
		case '/':
			g_string_append (s, "\\s");
			break;
		case '&':
			g_string_append (s, "\\a");
			break;
		case '"':
			g_string_append (s, "\\q");
			break;
		default:
			g_string_append_c (s, *action_name);
		}

		action_name ++;
	}
	return g_string_free (s, FALSE);
}

static char *
escape_action_path (const char *action_path)
{
	GString *s;

	if (action_path == NULL) {
		return NULL;
	}
	
	s = g_string_sized_new (strlen (action_path) + 2);

	while (*action_path != 0) {
		switch (*action_path) {
		case '\\':
			g_string_append (s, "\\\\");
			break;
		case '&':
			g_string_append (s, "\\a");
			break;
		case '"':
			g_string_append (s, "\\q");
			break;
		default:
			g_string_append_c (s, *action_path);
		}

		action_path ++;
	}
	return g_string_free (s, FALSE);
}


static void
add_submenu (GtkUIManager *ui_manager,
	     GtkActionGroup *action_group,
	     guint merge_id,
	     const char *parent_path,
	     const char *uri,
	     const char *label,
	     GdkPixbuf *pixbuf)
{
	char *escaped_label;
	char *action_name;
	char *submenu_name;
	char *escaped_submenu_name;
	GtkAction *action;
	
	if (parent_path != NULL) {
		action_name = escape_action_name (uri, "submenu_");
		submenu_name = g_path_get_basename (uri);
		escaped_submenu_name = escape_action_path (submenu_name);
		escaped_label = eel_str_double_underscores (label);

		action = gtk_action_new (action_name,
					 escaped_label,
					 NULL,
					 NULL);
		g_object_set_data_full (G_OBJECT (action), "menu-icon",
					g_object_ref (pixbuf),
					g_object_unref);

		g_object_set (action, "hide-if-empty", FALSE, NULL);
		
		gtk_action_group_add_action (action_group,
					     action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       parent_path,
				       escaped_submenu_name,
				       action_name,
				       GTK_UI_MANAGER_MENU,
				       FALSE);
		g_free (action_name);
		g_free (escaped_label);
		g_free (submenu_name);
		g_free (escaped_submenu_name);
	}
}

static void
add_application_to_open_with_menu (FMDirectoryView *view,
				   GnomeVFSMimeApplication *application, 
				   NautilusFile *file,
				   int index,
				   const char *menu_placeholder,
				   const char *popup_placeholder)
{
	ApplicationLaunchParameters *launch_parameters;
	char *tip;
	char *label;
	char *action_name;
	GtkAction *action;

	launch_parameters = application_launch_parameters_new 
		(application, file, view);
	label = g_strdup_printf (_("Open with \"%s\""), application->name);
	tip = g_strdup_printf (_("Use \"%s\" to open the selected item"), application->name);

	action_name = g_strdup_printf ("open_with_%d", index);
	
	action = gtk_action_new (action_name,
				 label,
				 tip,
				 NULL);
	
	g_signal_connect_data (action, "activate",
			       G_CALLBACK (open_with_launch_application_callback),
			       launch_parameters, 
			       (GClosureNotify)application_launch_parameters_free, 0);

	gtk_action_group_add_action (view->details->open_with_action_group,
				     action);
	g_object_unref (action);
	
	gtk_ui_manager_add_ui (nautilus_window_info_get_ui_manager (view->details->window),
			       view->details->open_with_merge_id,
			       menu_placeholder,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	gtk_ui_manager_add_ui (nautilus_window_info_get_ui_manager (view->details->window),
			       view->details->open_with_merge_id,
			       popup_placeholder,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_free (action_name);
	g_free (label);
	g_free (tip);
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

static ActivationAction
get_executable_text_file_action (FMDirectoryView *view, NautilusFile *file)
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
					     _("Run or Display?"),
					     _("Run in _Terminal"), RESPONSE_RUN_IN_TERMINAL,
     					     _("_Display"), RESPONSE_DISPLAY,
					     fm_directory_view_get_containing_window (view));
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

static gboolean
can_use_component_for_file (NautilusFile *file)
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
	
	activation_uri = nautilus_file_get_activation_uri (file);
	if (activation_uri == NULL) {
		activation_uri = nautilus_file_get_uri (file);
	}

	action = ACTIVATION_ACTION_DO_NOTHING;
	
	if (eel_str_has_prefix (activation_uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER)) {
		action = ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE;
	} else if (eel_str_has_prefix (activation_uri, NAUTILUS_COMMAND_SPECIFIER)) {
		action = ACTIVATION_ACTION_LAUNCH_APPLICATION_FROM_COMMAND;
	} else if (file_is_launchable (file)) {
		char *executable_path;
		
		action = ACTIVATION_ACTION_LAUNCH;
		
		executable_path = gnome_vfs_get_local_path_from_uri (activation_uri);
		if (!executable_path) {
			action = ACTIVATION_ACTION_DO_NOTHING;
		} else if (nautilus_file_contains_text (file)) {
			action = get_default_executable_text_file_action ();
		}
		g_free (executable_path);
	} 

	if (action == ACTIVATION_ACTION_DO_NOTHING) {
		if (can_use_component_for_file (file)) {
			action = ACTIVATION_ACTION_OPEN_IN_VIEW;
		} else {
			action = ACTIVATION_ACTION_OPEN_IN_APPLICATION;
		}
	}
	g_free (activation_uri);

	return action;
}

static void
reset_open_with_menu (FMDirectoryView *view, GList *selection)
{
	GList *applications, *node;
	NautilusFile *file;
	gboolean submenu_visible;
	char *uri;
	int num_applications;
	int index;
	gboolean other_applications_visible;
	GtkUIManager *ui_manager;
	GtkAction *action;
	
	/* Clear any previous inserted items in the applications and viewers placeholders */

	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->open_with_merge_id,
				&view->details->open_with_action_group);
	
	nautilus_ui_prepare_merge_ui (ui_manager,
				      "OpenWithGroup",
				      &view->details->open_with_merge_id,
				      &view->details->open_with_action_group);
	
	num_applications = 0;

	/* This menu is only displayed when there's one selected item. */
	if (!eel_g_list_exactly_one_item (selection)) {
		submenu_visible = FALSE;
		other_applications_visible = FALSE;
	} else {		
		GnomeVFSMimeApplication *default_app;
		ActivationAction action;
		
		file = NAUTILUS_FILE (selection->data);
		
		uri = nautilus_file_get_uri (file);

		other_applications_visible = !can_use_component_for_file (file);

		action = get_activation_action (file);
		/* Only use the default app for open if there is not
		   a mime mismatch, otherwise we can't use it in the
		   open with menu */
		if (action == ACTIVATION_ACTION_OPEN_IN_APPLICATION &&
		    can_show_default_app (view, file)) {
			default_app = nautilus_mime_get_default_application_for_file (file);
		} else {
			default_app = NULL;
		}
		
		applications = NULL;
		if (other_applications_visible) {
			applications = nautilus_mime_get_open_with_applications_for_file (NAUTILUS_FILE (selection->data));
		}

		num_applications = g_list_length (applications);
		
		for (node = applications, index = 0; node != NULL; node = node->next, index++) {
			GnomeVFSMimeApplication *application;
			char *menu_path;
			char *popup_path;
			
			application = node->data;

			if (default_app && !strcmp (default_app->id, application->id)) {
				continue;
			}

			if (num_applications > 3) {
				menu_path = FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER;
				popup_path = FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER;
			} else {
				menu_path = FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER;
				popup_path = FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER;
			}

			gtk_ui_manager_add_ui (nautilus_window_info_get_ui_manager (view->details->window),
					       view->details->open_with_merge_id,
					       menu_path,
					       "separator",
					       NULL,
					       GTK_UI_MANAGER_SEPARATOR,
					       FALSE);
					       
			add_application_to_open_with_menu (view, 
							   node->data, 
							   file, 
							   index, 
							   menu_path, popup_path);
		}
		gnome_vfs_mime_application_list_free (applications);
		
		g_free (uri);

		submenu_visible = (num_applications > 3);
	}

	if (submenu_visible) {
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      FM_ACTION_OTHER_APPLICATION1);
		gtk_action_set_visible (action, other_applications_visible);
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      FM_ACTION_OTHER_APPLICATION2);
		gtk_action_set_visible (action, FALSE);
	} else {
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      FM_ACTION_OTHER_APPLICATION1);
		gtk_action_set_visible (action, FALSE);
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      FM_ACTION_OTHER_APPLICATION2);
		gtk_action_set_visible (action, other_applications_visible);
	}
}

static GList *
get_all_extension_menu_items (GtkWidget *window,
			      GList *selection)
{
	GList *items;
	GList *providers;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;
		
		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_file_items (provider,
								    window,
								    selection);
		items = g_list_concat (items, file_items);		
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

typedef struct 
{
	NautilusMenuItem *item;
	FMDirectoryView *view;
	GList *selection;
} ExtensionActionCallbackData;


static void
extension_action_callback_data_free (ExtensionActionCallbackData *data)
{
	g_object_unref (data->item);
	nautilus_file_list_free (data->selection);
	
	g_free (data);
}

static void
extension_action_slow_mime_types_ready_callback (GList *selection, 
						 gpointer callback_data)
{
	ExtensionActionCallbackData *data;
	char *item_name;
	gboolean is_valid;
	GList *l;
	GList *items;

	data = callback_data;

	/* Make sure the selected menu item is valid for the final sniffed
	 * mime type */
	g_object_get (data->item, "name", &item_name, NULL);
	items = get_all_extension_menu_items (gtk_widget_get_toplevel (GTK_WIDGET (data->view)), 
					      data->selection);
	
	is_valid = FALSE;
	for (l = items; l != NULL; l = l->next) {
		char *name;
		
		g_object_get (l->data, "name", &name, NULL);
		
		if (strcmp (name, item_name) == 0) {
			is_valid = TRUE;
			g_free (name);
			break;
		}
		g_free (name);
	}

	for (l = items; l != NULL; l = l->next) {
		g_object_unref (l->data);
	}
	g_list_free (items);
	
	g_free (item_name);

	if (is_valid) {
		nautilus_menu_item_activate (data->item);
	}
}

static void
extension_action_callback (GtkAction *action,
			   gpointer callback_data)
{
	ExtensionActionCallbackData *data;

	data = callback_data;

	nautilus_file_list_call_when_ready
		(data->selection,
		 NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE,
		 extension_action_slow_mime_types_ready_callback,
		 callback_data);
}

static GtkAction *
add_extension_action_for_files (FMDirectoryView *view, 
				NautilusMenuItem *item,
				GList *files)
{
	char *name, *label, *tip, *icon;
	gboolean sensitive, priority;
	GtkAction *action;
	GdkPixbuf *pixbuf;
	ExtensionActionCallbackData *data;
	
	g_object_get (G_OBJECT (item), 
		      "name", &name, "label", &label, 
		      "tip", &tip, "icon", &icon,
		      "sensitive", &sensitive,
		      "priority", &priority,
		      NULL);

	action = gtk_action_new (name,
				 label,
				 tip,
				 icon);

	/* TODO: This should really use themed icons, but that
	   doesn't work here yet */
	if (icon != NULL) {
		pixbuf = nautilus_icon_factory_get_pixbuf_from_name 
			(icon,
			 NULL,
			 NAUTILUS_ICON_SIZE_FOR_MENUS,
			 NULL);
		if (pixbuf != NULL) {
			g_object_set_data_full (G_OBJECT (action), "menu-icon",
						pixbuf,
						g_object_unref);
		}
	}

	gtk_action_set_sensitive (action, sensitive);
	g_object_set (action, "is-important", priority, NULL);

	data = g_new0 (ExtensionActionCallbackData, 1);
	data->item = g_object_ref (item);
	data->view = view;
	data->selection = nautilus_file_list_copy (files);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (extension_action_callback),
			       data,
			       (GClosureNotify)extension_action_callback_data_free, 0);
		
	gtk_action_group_add_action (view->details->extensions_menu_action_group,
				     GTK_ACTION (action));
	g_object_unref (action);
	
	g_free (name);
	g_free (label);
	g_free (tip);
	g_free (icon);

	return action;
}

static void
warn_mismatched_mime_types_response_cb (GtkWidget *dialog,
					int response,
					gpointer user_data)
{
	gtk_widget_destroy (dialog);
}

static void
warn_mismatched_mime_types (FMDirectoryView *view,
			    NautilusFile *file)
{
	GtkWidget *dialog;
	char *guessed_mime_type;
	char *mime_type;
	const char *guessed_description;
	const char *real_description;
	char *primary;
	char *secondary;
	char *name;
	
	guessed_mime_type = nautilus_file_get_guessed_mime_type (file);
	mime_type = nautilus_file_get_mime_type (file);

	guessed_description = gnome_vfs_mime_get_description (guessed_mime_type);
	real_description = gnome_vfs_mime_get_description (mime_type);

	name = nautilus_file_get_name (file);

	primary = g_strdup_printf (_("Cannot open %s"), name);

	secondary = g_strdup_printf 
		(_("The filename \"%s\" indicates that this file is of type \"%s\". " 
		   "The contents of the file indicate that the file is of type \"%s\". If "
		   "you open this file, the file might present a security risk to your system.\n\n"
		   "Do not open the file unless you created the file yourself, or received "
		   "the file from a trusted source. To open the file, rename the file to the "
		   "correct extension for \"%s\", then open the file normally. "
		   "Alternatively, use the Open With menu to choose a specific application "
		   "for the file. "),
		 name, 
		 guessed_description ? guessed_description : guessed_mime_type, 
		 real_description ? real_description : mime_type,
		 real_description ? real_description : mime_type);

	g_free (guessed_mime_type);
	g_free (mime_type);

	dialog = eel_alert_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       0,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_NONE,
				       primary,
				       secondary,
				       primary);

	g_free (primary);
	g_free (secondary);
	g_free (name);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), 
					 GTK_RESPONSE_CANCEL);

	g_signal_connect (dialog, 
			  "response",
			  G_CALLBACK (warn_mismatched_mime_types_response_cb),
			  file);

	gtk_widget_show (dialog);
}

static gboolean 
can_show_default_app (FMDirectoryView *view, NautilusFile *file)
{
	return (!nautilus_file_check_if_ready (file, NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE) || activate_check_mime_types (view, file, FALSE));

}

static gboolean
activate_check_mime_types (FMDirectoryView *view,
			   NautilusFile *file,
			   gboolean warn_on_mismatch)
{
	char *guessed_mime_type;
	char *mime_type;
	gboolean ret;
	GnomeVFSMimeApplication *default_app;
	GnomeVFSMimeApplication *guessed_default_app;

	if (!nautilus_file_check_if_ready (file, NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE)) {
		return FALSE;
	}

	ret = TRUE;

	guessed_mime_type = nautilus_file_get_guessed_mime_type (file);
	mime_type = nautilus_file_get_mime_type (file);

	if (strcmp (guessed_mime_type, mime_type) != 0) {
		default_app = gnome_vfs_mime_get_default_application
			(mime_type);
		guessed_default_app = gnome_vfs_mime_get_default_application
			(guessed_mime_type);
		if (default_app == NULL ||
		    default_app->id == NULL ||
		    guessed_default_app == NULL ||
		    guessed_default_app->id == NULL ||
		    strcmp (default_app->id, guessed_default_app->id) != 0) {
			if (warn_on_mismatch) {
				warn_mismatched_mime_types (view, file);
			}
			ret = FALSE;
		}
	}

	g_free (guessed_mime_type);
	g_free (mime_type);
	
	return ret;
}

static void
add_extension_menu_items (FMDirectoryView *view,
			  GList *files,
			  GList *menu_items)
{
	GtkUIManager *ui_manager;
	GList *l;

	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);
	
	for (l = menu_items; l; l = l->next) {
		NautilusMenuItem *item;
		GtkAction *action;
		
		item = NAUTILUS_MENU_ITEM (l->data);
		
		action = add_extension_action_for_files (view, item, files);

		gtk_ui_manager_add_ui (ui_manager,
				       view->details->extensions_menu_merge_id,
				       FM_DIRECTORY_VIEW_POPUP_PATH_EXTENSION_ACTIONS,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		gtk_ui_manager_add_ui (ui_manager,
				       view->details->extensions_menu_merge_id,
				       FM_DIRECTORY_VIEW_MENU_PATH_EXTENSION_ACTIONS_PLACEHOLDER,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);
	}
}

static gboolean
has_file_in_list (GList *list, NautilusFile *file)
{
	gboolean ret = FALSE;
	char *mime;
       
	mime = nautilus_file_get_mime_type (file);
	
	for (; list; list = list->next) {
		NautilusFile *tmp_file = list->data;
		char *tmp_mime = nautilus_file_get_mime_type (tmp_file);

		if (strcmp (tmp_mime, mime) == 0) {
			ret = TRUE;
			g_free (tmp_mime);
			break;
		}

		g_free (tmp_mime);
	}
	
	g_free (mime);
	return ret;
}

static GList *
get_unique_files (GList *selection)
{
	GList *result;

	result = NULL;
	for (; selection; selection = selection->next) {
		if (!has_file_in_list (result,
				       NAUTILUS_FILE (selection->data))) {
			result = g_list_prepend (result, selection->data);
		}
	}	

	return g_list_reverse (result);
}


static void
reset_extension_actions_menu (FMDirectoryView *view, GList *selection)
{
	GList *unique_selection;
	GList *items;
	GList *l;
	GtkUIManager *ui_manager;
	
	/* Clear any previous inserted items in the extension actions placeholder */
	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);

	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->extensions_menu_merge_id,
				&view->details->extensions_menu_action_group);
	
	nautilus_ui_prepare_merge_ui (ui_manager,
				      "DirExtensionsMenuGroup",
				      &view->details->extensions_menu_merge_id,
				      &view->details->extensions_menu_action_group);

	/* only query for the unique files */
	unique_selection = get_unique_files (selection);
	items = get_all_extension_menu_items (gtk_widget_get_toplevel (GTK_WIDGET (view)), 
					      selection);
	
	if (items) {
		add_extension_menu_items (view, unique_selection, items);
	
		for (l = items; l != NULL; l = l->next) {
			g_object_unref (l->data);
		}
		
		g_list_free (items);
	}

	g_list_free (unique_selection);
}

static char *
change_to_view_directory (FMDirectoryView *view)
{
	char *uri;
	char *path;
	char *old_path;

	old_path = g_get_current_dir ();

	uri = nautilus_directory_get_uri (view->details->model);
	path = gnome_vfs_get_local_path_from_uri (uri);

	/* FIXME: What to do about non-local directories? */
	if (path != NULL) {
		chdir (path);
	}

	g_free (uri);
	g_free (path);

	return old_path;
}

static char *
get_file_names_as_parameter_string (GList *selection)
{
	char *name, *quoted_name;
	char *result;
	GString *parameter_string;
	GList *node;

	parameter_string = g_string_new ("");
	for (node = selection; node != NULL; node = node->next) {
		name = nautilus_file_get_name (NAUTILUS_FILE (node->data));
		quoted_name = g_shell_quote (name);
		g_string_append (parameter_string, quoted_name);
		g_string_append (parameter_string, " ");
		g_free (name);
		g_free (quoted_name);
	}

	result = parameter_string->str;
	g_string_free (parameter_string, FALSE);

	return result;
}

static char *
get_file_paths_or_uris_as_newline_delimited_string (GList *selection, gboolean get_paths)
{
	char *path;
	char *uri;
	char *result;
	GString *expanding_string;
	GList *node;

	expanding_string = g_string_new ("");
	for (node = selection; node != NULL; node = node->next) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (node->data));

		if (get_paths) {
			path = gnome_vfs_get_local_path_from_uri (uri);
			g_string_append (expanding_string, path);
			g_free (path);
		} else {
			g_string_append (expanding_string, uri);
		}
		g_string_append (expanding_string, "\n");
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

/*
 * Set up some environment variables that scripts can use
 * to take advantage of the current Nautilus state.
 */
static void
set_script_environment_variables (FMDirectoryView *view, GList *selected_files)
{
	char *file_paths;
	char *uris;
	char *uri;
	char *geometry_string;
	char *directory_uri;

	/* We need to check that the directory uri starts with "file:" since
	 * nautilus_directory_is_local returns FALSE for nfs.
	 */
	directory_uri = nautilus_directory_get_uri (view->details->model);
	if (eel_str_has_prefix (directory_uri, "file:")) {
		file_paths = get_file_paths_as_newline_delimited_string (selected_files);
	} else {
		file_paths = g_strdup ("");
	}
	g_free (directory_uri);
	
	eel_setenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS", file_paths, TRUE);
	g_free (file_paths);

	uris = get_file_uris_as_newline_delimited_string (selected_files);
	eel_setenv ("NAUTILUS_SCRIPT_SELECTED_URIS", uris, TRUE);
	g_free (uris);

	uri = nautilus_directory_get_uri (view->details->model);
	eel_setenv ("NAUTILUS_SCRIPT_CURRENT_URI", uri, TRUE);
	g_free (uri);

	geometry_string = eel_gtk_window_get_geometry_string 
		(GTK_WINDOW (fm_directory_view_get_containing_window (view)));
	eel_setenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY", geometry_string, TRUE);
	g_free (geometry_string);
}

/* Unset all the special script environment variables. */
static void
unset_script_environment_variables (void)
{
	eel_unsetenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS");
	eel_unsetenv ("NAUTILUS_SCRIPT_SELECTED_URIS");
	eel_unsetenv ("NAUTILUS_SCRIPT_CURRENT_URI");
	eel_unsetenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY");
}

static void
run_script_callback (GtkAction *action, gpointer callback_data)
{
	ScriptLaunchParameters *launch_parameters;
	GdkScreen *screen;
	GList *selected_files;
	char *file_uri;
	char *local_file_path;
	char *quoted_path;
	char *old_working_dir;
	char *parameters, *command, *name;
	
	launch_parameters = (ScriptLaunchParameters *) callback_data;

	file_uri = nautilus_file_get_uri (launch_parameters->file);
	local_file_path = gnome_vfs_get_local_path_from_uri (file_uri);
	g_assert (local_file_path != NULL);
	g_free (file_uri);

	quoted_path = g_shell_quote (local_file_path);
	g_free (local_file_path);

	old_working_dir = change_to_view_directory (launch_parameters->directory_view);

	selected_files = fm_directory_view_get_selection (launch_parameters->directory_view);
	set_script_environment_variables (launch_parameters->directory_view, selected_files);
	 
	if (nautilus_directory_is_local (launch_parameters->directory_view->details->model)) {
		parameters = get_file_names_as_parameter_string (selected_files);

		/* FIXME: must append command and parameters here, because nautilus_launch_application_from_command
		 * quotes all parameters as if they are a single parameter. Should add or change API in
		 * nautilus-program-choosing.c to support multiple parameters.
		 */
		command = g_strconcat (quoted_path, " ", parameters, NULL);
		g_free (parameters);
	} else {
		/* We pass no parameters in the remote case. It's up to scripts to be smart
		 * and check the environment variables. 
		 */
		command = g_strdup (quoted_path);
	}

	screen = gtk_widget_get_screen (GTK_WIDGET (launch_parameters->directory_view));

	name = nautilus_file_get_name (launch_parameters->file);
	/* FIXME: handle errors with dialog? Or leave up to each script? */
	nautilus_launch_application_from_command (screen, name, command, NULL, FALSE);
	g_free (name);
	g_free (command);

	nautilus_file_list_free (selected_files);
	unset_script_environment_variables ();
	chdir (old_working_dir);		
	g_free (old_working_dir);
	g_free (quoted_path);
}

static void
add_script_to_scripts_menus (FMDirectoryView *directory_view,
			     NautilusFile *file,
			     const char *menu_path,
			     const char *popup_path, 
			     const char *popup_bg_path)
{
	ScriptLaunchParameters *launch_parameters;
	char *tip;
	char *name;
	char *uri;
	char *action_name;
	char *escaped_label;
	GdkPixbuf *pixbuf;
	GtkUIManager *ui_manager;
	GtkAction *action;

	name = nautilus_file_get_display_name (file);
	uri = nautilus_file_get_uri (file);
	tip = g_strdup_printf (_("Run \"%s\" on any selected items"), name);

	launch_parameters = script_launch_parameters_new (file, directory_view);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_file 
		(file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS);

	action_name = escape_action_name (uri, "script_");
	escaped_label = eel_str_double_underscores (name);

	action = gtk_action_new (action_name,
				 escaped_label,
				 tip,
				 NULL);
	
	g_object_set_data_full (G_OBJECT (action), "menu-icon",
				g_object_ref (pixbuf),
				g_object_unref);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (run_script_callback),
			       launch_parameters, 
			       (GClosureNotify)script_launch_parameters_free, 0);
	
	gtk_action_group_add_action (directory_view->details->scripts_action_group,
				     action);
	g_object_unref (action);
	
	ui_manager = nautilus_window_info_get_ui_manager (directory_view->details->window);

	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->scripts_merge_id,
			       menu_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->scripts_merge_id,
			       popup_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->scripts_merge_id,
			       popup_bg_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);


	g_object_unref (pixbuf);
	g_free (name);
	g_free (uri);
	g_free (tip);
	g_free (action_name);
	g_free (escaped_label);
}

static void
add_submenu_to_directory_menus (FMDirectoryView *directory_view,
				GtkActionGroup *action_group,
				guint merge_id,
				NautilusFile *file,
				const char *menu_path,
				const char *popup_path,
				const char *popup_bg_path)
{
	char *name;
	GdkPixbuf *pixbuf;
	char *uri;
	GtkUIManager *ui_manager;

	ui_manager = nautilus_window_info_get_ui_manager (directory_view->details->window);
	uri = nautilus_file_get_uri (file);
	name = nautilus_file_get_display_name (file);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_file 
		(file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS);
	add_submenu (ui_manager, action_group, merge_id, menu_path, uri, name, pixbuf);
	add_submenu (ui_manager, action_group, merge_id, popup_path, uri, name, pixbuf);
	add_submenu (ui_manager, action_group, merge_id, popup_bg_path, uri, name, pixbuf);
	g_object_unref (pixbuf);
	g_free (name);
	g_free (uri);
}

static gboolean
directory_belongs_in_scripts_menu (const char *uri)
{
	int num_levels;
	int i;

	if (!eel_str_has_prefix (uri, scripts_directory_uri)) {
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

static gboolean
update_directory_in_scripts_menu (FMDirectoryView *view, NautilusDirectory *directory)
{
	char *menu_path, *popup_path, *popup_bg_path;
	GList *file_list, *filtered, *node;
	gboolean any_scripts;
	NautilusFile *file;
	NautilusDirectory *dir;
	char *uri;
	char *escaped_path;
	
	uri = nautilus_directory_get_uri (directory);
	escaped_path = escape_action_path (uri + scripts_directory_uri_length);
	g_free (uri);
	menu_path = g_strconcat (FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER,
				 escaped_path,
				 NULL);
	popup_path = g_strconcat (FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER,
				  escaped_path,
				  NULL);
	popup_bg_path = g_strconcat (FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND_SCRIPTS_PLACEHOLDER,
				  escaped_path,
				  NULL);
	g_free (escaped_path);

	file_list = nautilus_directory_get_file_list (directory);
	filtered = nautilus_file_list_filter_hidden_and_backup (file_list, FALSE, FALSE);
	nautilus_file_list_free (file_list);

	file_list = nautilus_file_list_sort_by_display_name (filtered);

	any_scripts = FALSE;
	for (node = file_list; node != NULL; node = node->next) {
		file = node->data;

		if (file_is_launchable (file)) {
			add_script_to_scripts_menus (view, file, menu_path, popup_path, popup_bg_path);
			any_scripts = TRUE;
		} else if (nautilus_file_is_directory (file)) {
			uri = nautilus_file_get_uri (file);
			if (directory_belongs_in_scripts_menu (uri)) {
				dir = nautilus_directory_get (uri);
				add_directory_to_scripts_directory_list (view, dir);
				nautilus_directory_unref (dir);

				add_submenu_to_directory_menus (view,
								view->details->scripts_action_group,
								view->details->scripts_merge_id,
								file, menu_path, popup_path, popup_bg_path);

				any_scripts = TRUE;
			}
			g_free (uri);
		}
	}

	nautilus_file_list_free (file_list);

	g_free (popup_path);
	g_free (popup_bg_path);
	g_free (menu_path);

	return any_scripts;
}

static void
update_scripts_menu (FMDirectoryView *view)
{
	gboolean any_scripts;
	GList *sorted_copy, *node;
	NautilusDirectory *directory;
	char *uri;
	GtkUIManager *ui_manager;
	GtkAction *action;

	/* There is a race condition here.  If we don't mark the scripts menu as
	   valid before we begin our task then we can lose script menu updates that
	   occur before we finish. */
	view->details->scripts_invalid = FALSE;

	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->scripts_merge_id,
				&view->details->scripts_action_group);
	
	nautilus_ui_prepare_merge_ui (ui_manager,
				      "ScriptsGroup",
				      &view->details->scripts_merge_id,
				      &view->details->scripts_action_group);

	/* As we walk through the directories, remove any that no longer belong. */
	any_scripts = FALSE;
	sorted_copy = nautilus_directory_list_sort_by_uri
		(nautilus_directory_list_copy (view->details->scripts_directory_list));
	for (node = sorted_copy; node != NULL; node = node->next) {
		directory = node->data;

		uri = nautilus_directory_get_uri (directory);
		if (!directory_belongs_in_scripts_menu (uri)) {
			remove_directory_from_scripts_directory_list (view, directory);
		} else if (update_directory_in_scripts_menu (view, directory)) {
			any_scripts = TRUE;
		}
		g_free (uri);
	}
	nautilus_directory_list_free (sorted_copy);

	action = gtk_action_group_get_action (view->details->dir_action_group, FM_ACTION_SCRIPTS);
	gtk_action_set_visible (action, any_scripts);
}

static void
create_template_callback (GtkAction *action, gpointer callback_data)
{
	CreateTemplateParameters *parameters;

	parameters = callback_data;
	
	fm_directory_view_new_file (parameters->directory_view, parameters->file);
}

static void
add_template_to_templates_menus (FMDirectoryView *directory_view,
				 NautilusFile *file,
				 const char *menu_path,
				 const char *popup_bg_path)
{
	char *tip, *uri, *name;
	char *dot, *escaped_label;
	GdkPixbuf *pixbuf;
	char *action_name;
	CreateTemplateParameters *parameters;
	GtkUIManager *ui_manager;
	GtkAction *action;


	name = nautilus_file_get_display_name (file);
	uri = nautilus_file_get_uri (file);
	tip = g_strdup_printf (_("Create Document from template \"%s\""), name);

	/* Remove extension */
	dot = strrchr (name, '.');
	if (dot != NULL) {
		*dot = 0;
	}

	pixbuf = nautilus_icon_factory_get_pixbuf_for_file 
		(file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS);

	action_name = escape_action_name (uri, "template_");
	escaped_label = eel_str_double_underscores (name);
	
	parameters = create_template_parameters_new (file, directory_view);

	action = gtk_action_new (action_name,
				 escaped_label,
				 tip,
				 NULL);
	
	g_object_set_data_full (G_OBJECT (action), "menu-icon",
				g_object_ref (pixbuf),
				g_object_unref);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (create_template_callback),
			       parameters, 
			       (GClosureNotify)create_templates_parameters_free, 0);
	
	gtk_action_group_add_action (directory_view->details->templates_action_group,
				     action);
	g_object_unref (action);

	ui_manager = nautilus_window_info_get_ui_manager (directory_view->details->window);

	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->templates_merge_id,
			       menu_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	
	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->templates_merge_id,
			       popup_bg_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	
	g_object_unref (pixbuf);
	g_free (escaped_label);
	g_free (name);
	g_free (tip);
	g_free (uri);
	g_free (action_name);
}


static gboolean
directory_belongs_in_templates_menu (const char *uri)
{
	int num_levels;
	int i;

	if (!eel_str_has_prefix (uri, templates_directory_uri)) {
		return FALSE;
	}

	num_levels = 0;
	for (i = templates_directory_uri_length; uri[i] != '\0'; i++) {
		if (uri[i] == '/') {
			num_levels++;
		}
	}

	if (num_levels > MAX_MENU_LEVELS) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
update_directory_in_templates_menu (FMDirectoryView *view, NautilusDirectory *directory)
{
	char *menu_path, *popup_bg_path;
	GList *file_list, *filtered, *node;
	gboolean any_templates;
	NautilusFile *file;
	NautilusDirectory *dir;
	char *escaped_path;
	char *uri;
	
	uri = nautilus_directory_get_uri (directory);
	escaped_path = escape_action_path (uri + templates_directory_uri_length);
	g_free (uri);
	menu_path = g_strconcat (FM_DIRECTORY_VIEW_MENU_PATH_NEW_DOCUMENTS_PLACEHOLDER,
				 escaped_path,
				 NULL);
	popup_bg_path = g_strconcat (FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND_NEW_DOCUMENTS_PLACEHOLDER,
				     escaped_path,
				     NULL);
	g_free (escaped_path);

	file_list = nautilus_directory_get_file_list (directory);
	filtered = nautilus_file_list_filter_hidden_and_backup (file_list, FALSE, FALSE);
	nautilus_file_list_free (file_list);

	file_list = nautilus_file_list_sort_by_display_name (filtered);

	any_templates = FALSE;
	for (node = file_list; node != NULL; node = node->next) {
		file = node->data;

		if (nautilus_file_is_directory (file)) {
			uri = nautilus_file_get_uri (file);
			if (directory_belongs_in_templates_menu (uri)) {
				dir = nautilus_directory_get (uri);
				add_directory_to_templates_directory_list (view, dir);
				nautilus_directory_unref (dir);

				add_submenu_to_directory_menus (view,
								view->details->templates_action_group,
								view->details->templates_merge_id,
								file, menu_path, NULL, popup_bg_path);

				any_templates = TRUE;
			}
			g_free (uri);
		} else if (nautilus_file_can_read (file)) {
			add_template_to_templates_menus (view, file, menu_path, popup_bg_path);
			any_templates = TRUE;
		}
	}

	nautilus_file_list_free (file_list);

	g_free (popup_bg_path);
	g_free (menu_path);

	return any_templates;
}



static void
update_templates_menu (FMDirectoryView *view)
{
	gboolean any_templates;
	GList *sorted_copy, *node;
	NautilusDirectory *directory;
	GtkUIManager *ui_manager;
	char *uri;
	GtkAction *action;

	/* There is a race condition here.  If we don't mark the scripts menu as
	   valid before we begin our task then we can lose template menu updates that
	   occur before we finish. */
	view->details->templates_invalid = FALSE;

	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);
	nautilus_ui_unmerge_ui (ui_manager,
				&view->details->templates_merge_id,
				&view->details->templates_action_group);

	nautilus_ui_prepare_merge_ui (ui_manager,
				      "TemplatesGroup",
				      &view->details->templates_merge_id,
				      &view->details->templates_action_group);

	/* As we walk through the directories, remove any that no longer belong. */
	any_templates = FALSE;
	sorted_copy = nautilus_directory_list_sort_by_uri
		(nautilus_directory_list_copy (view->details->templates_directory_list));
	for (node = sorted_copy; node != NULL; node = node->next) {
		directory = node->data;

		uri = nautilus_directory_get_uri (directory);
		if (!directory_belongs_in_templates_menu (uri)) {
			remove_directory_from_templates_directory_list (view, directory);
		} else if (update_directory_in_templates_menu (view, directory)) {
			any_templates = TRUE;
		}
		g_free (uri);
	}
	nautilus_directory_list_free (sorted_copy);

	action = gtk_action_group_get_action (view->details->dir_action_group, FM_ACTION_NO_TEMPLATES);
	gtk_action_set_visible (action, !any_templates);
}


static void
action_open_scripts_folder_callback (GtkAction *action, 
				     gpointer callback_data)
{      
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	open_location (view, scripts_directory_uri, NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE, 0);
	
	eel_show_info_dialog_with_details 
		(_("All executable files in this folder will appear in the "
		   "Scripts menu."),
		 _("Choosing a script from the menu will run "
		   "that script with any selected items as input."), 
		 _("About Scripts"),
		 _("All executable files in this folder will appear in the "
		   "Scripts menu. Choosing a script from the menu will run "
		   "that script.\n\n"
		   "When executed from a local folder, scripts will be passed "
		   "the selected file names. When executed from a remote folder "
		   "(e.g. a folder showing web or ftp content), scripts will "
		   "be passed no parameters.\n\n"
		   "In all cases, the following environment variables will be "
		   "set by Nautilus, which the scripts may use:\n\n"
		   "NAUTILUS_SCRIPT_SELECTED_FILE_PATHS: newline-delimited paths for selected files (only if local)\n\n"
		   "NAUTILUS_SCRIPT_SELECTED_URIS: newline-delimited URIs for selected files\n\n"
		   "NAUTILUS_SCRIPT_CURRENT_URI: URI for current location\n\n"
		   "NAUTILUS_SCRIPT_WINDOW_GEOMETRY: position and size of current window"),
		 fm_directory_view_get_containing_window (view));
}

static void
popup_menu_hidden (FMDirectoryView *view)
{
	monitor_file_for_open_with (view, NULL);
}

static GtkMenu *
create_popup_menu (FMDirectoryView *view, const char *popup_path)
{
	GtkWidget *menu;
	
	menu = gtk_ui_manager_get_widget (nautilus_window_info_get_ui_manager (view->details->window),
					  popup_path);
	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_widget_get_screen (GTK_WIDGET (view)));
	gtk_widget_show (GTK_WIDGET (menu));

	g_signal_connect_object (menu, "hide",
				 G_CALLBACK (popup_menu_hidden), G_OBJECT (view), G_CONNECT_SWAPPED);
	
	return GTK_MENU (menu);
}

typedef struct {
	GList *file_uris;
	gboolean cut;
} ClipboardInfo;

static char *
convert_file_list_to_string (GList *files,
			     gboolean format_for_text,
			     gboolean cut)
{
	GString *uris;
	GList *node;
	char *uri, *tmp;

	if (format_for_text) {
		uris = g_string_new (NULL);
	} else {
		uris = g_string_new (cut ? "cut" : "copy");
	}
	
	for (node = files; node != NULL; node = node->next) {
		uri = node->data;
		
		if (format_for_text) {
			tmp = eel_format_uri_for_display (uri);
			
			if (tmp != NULL) {
				g_string_append (uris, tmp);
				g_free (tmp);
			} else {
				g_string_append (uris, uri);
			}
			g_string_append_c (uris, '\n');
			
		} else {
			g_string_append_c (uris, '\n');
			g_string_append (uris, uri);
		}
	}

	return g_string_free (uris, FALSE);
}

static void
get_clipboard_callback (GtkClipboard     *clipboard,
			GtkSelectionData *selection_data,
			guint             info,
			gpointer          user_data_or_owner)
{
	ClipboardInfo *clipboard_info = user_data_or_owner;
	char *str;

	str = convert_file_list_to_string (clipboard_info->file_uris,
					   info == UTF8_STRING,
					   clipboard_info->cut);


	gtk_selection_data_set (selection_data,
				selection_data->target,
				8,
				str,
				strlen (str));
	
	g_free (str);
}

static void
clear_clipboard_callback (GtkClipboard *clipboard,
			  gpointer      user_data_or_owner)
{
	ClipboardInfo *info = user_data_or_owner;
	
	eel_g_list_free_deep (info->file_uris);

	g_free (info);
}

static GtkClipboard *
get_clipboard (FMDirectoryView *view)
{
	return gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (view)),
					      GDK_SELECTION_CLIPBOARD);
}

static GList *
convert_file_list_to_uri_list (GList *files)
{
	GList *tmp = NULL;
	
	while (files != NULL) {
		tmp = g_list_prepend (tmp, nautilus_file_get_uri (files->data));

		files = files->next;
	}

	return g_list_reverse (tmp);
}
	
static void
copy_or_cut_files (FMDirectoryView *view,
		   gboolean cut)
{
	int count;
	char *status_string, *name;
	GList *clipboard_contents;
	ClipboardInfo *info;
	
	clipboard_contents = fm_directory_view_get_selection (view);

	info = g_new0 (ClipboardInfo, 1);
	info->file_uris = convert_file_list_to_uri_list (clipboard_contents);
	info->cut = cut;
	
	gtk_clipboard_set_with_data (get_clipboard (view),
				     clipboard_targets, G_N_ELEMENTS (clipboard_targets),
				     get_clipboard_callback, clear_clipboard_callback,
				     info);
	nautilus_clipboard_monitor_emit_changed ();
	
	
	count = g_list_length (clipboard_contents);
	if (count == 1) {
		name = nautilus_file_get_display_name (clipboard_contents->data);
		if (cut) {
			status_string = g_strdup_printf (_("\"%s\" will be moved "
							   "if you select the Paste Files command"),
							 name);
		} else {
			status_string = g_strdup_printf (_("\"%s\" will be copied "
							   "if you select the Paste Files command"),
							 name);
		}
		g_free (name);
	} else {
		if (cut) {
			status_string = g_strdup_printf (ngettext("The %d selected item will be moved "
								  "if you select the Paste Files command",
								  "The %d selected items will be moved "
								  "if you select the Paste Files command",
								  count),
							 count);
		} else {
			status_string = g_strdup_printf (ngettext("The %d selected item will be copied "
								  "if you select the Paste Files command",
								  "The %d selected items will be copied "
								  "if you select the Paste Files command",
								  count),
							 count);
		}
	}

	nautilus_file_list_free (clipboard_contents);
	
	nautilus_window_info_set_status (view->details->window,
					 status_string);
	g_free (status_string);
}

static void
action_copy_files_callback (GtkAction *action,
			    gpointer callback_data)
{
	copy_or_cut_files (callback_data, FALSE);
}

static void
action_cut_files_callback (GtkAction *action,
			   gpointer callback_data)
{
	copy_or_cut_files (callback_data, TRUE);
}

static GList *
convert_lines_to_str_list (char **lines, gboolean *cut)
{
	int i;
	GList *result;

	if (lines[0] == NULL) {
		return NULL;
	}

	if (strcmp (lines[0], "cut") == 0) {
		*cut = TRUE;
	} else if (strcmp (lines[0], "copy") == 0) {
		*cut = FALSE;
	} else {
		return NULL;
	}

	result = NULL;
	for (i = 1; lines[i] != NULL; i++) {
		result = g_list_prepend (result, g_strdup (lines[i]));
	}
	return g_list_reverse (result);
}

static void
paste_clipboard_data (FMDirectoryView *view,
		      GtkSelectionData *selection_data,
		      char *destination_uri)
{
	char **lines;
	gboolean cut;
	GList *item_uris;
	
	if (selection_data->type != copied_files_atom
	    || selection_data->length <= 0) {
		item_uris = NULL;
	} else {
		/* Not sure why it's legal to assume there's an extra byte
		 * past the end of the selection data that it's safe to write
		 * to. But gtk_editable_selection_received does this, so I
		 * think it is OK.
		 */
		selection_data->data[selection_data->length] = '\0';
		lines = g_strsplit (selection_data->data, "\n", 0);
		item_uris = convert_lines_to_str_list (lines, &cut);
		g_strfreev (lines);
	}

	if (item_uris == NULL|| destination_uri == NULL) {
		nautilus_window_info_set_status (view->details->window,
						 _("There is nothing on the clipboard to paste."));
	} else {
		fm_directory_view_move_copy_items (item_uris, NULL, destination_uri,
						   cut ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
						   0, 0,
						   view);

		/* If items are cut then remove from clipboard */
		if (cut) {
			gtk_clipboard_clear (get_clipboard (view));
		}
	}
}

static void
paste_clipboard_received_callback (GtkClipboard     *clipboard,
				   GtkSelectionData *selection_data,
				   gpointer          data)
{
	FMDirectoryView *view;
	char *view_uri;

	view = FM_DIRECTORY_VIEW (data);

	view_uri = fm_directory_view_get_backing_uri (view);

	paste_clipboard_data (view, selection_data, view_uri);

	g_free (view_uri);
}

static void
paste_into_clipboard_received_callback (GtkClipboard     *clipboard,
					GtkSelectionData *selection_data,
					gpointer          data)
{
	GList *selection;
	FMDirectoryView *view;
	char *directory_uri;

	view = FM_DIRECTORY_VIEW (data);

	selection = fm_directory_view_get_selection (view);

	directory_uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));

	paste_clipboard_data (view, selection_data, directory_uri);

	g_free (directory_uri);
	nautilus_file_list_free (selection);
}

static void
action_paste_files_callback (GtkAction *action,
			     gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	
	gtk_clipboard_request_contents (get_clipboard (view),
					copied_files_atom,
					paste_clipboard_received_callback,
					callback_data);
}

static void
action_paste_files_into_callback (GtkAction *action,
				  gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	
	gtk_clipboard_request_contents (get_clipboard (view),
					copied_files_atom,
					paste_into_clipboard_received_callback,
					callback_data);
}

static void
action_rename_callback (GtkAction *action,
			gpointer callback_data)
{
	FMDirectoryView *view;
	NautilusFile *file;
	GList *selection;
	
	view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);

	if (selection_not_empty_in_menu_callback (view, selection)) {
		file = NAUTILUS_FILE (selection->data);
		EEL_CALL_METHOD (FM_DIRECTORY_VIEW_CLASS, view, start_renaming_file, (view, file));
	}

	nautilus_file_list_free (selection);
}

static void
drive_mounted_callback (gboolean succeeded,
			char *error,
			char *detailed_error,
			gpointer data)
{
	if (!succeeded) {
		eel_show_error_dialog_with_details (error, NULL,
						    _("Mount Error"), detailed_error, NULL);
	}
}


static void
action_mount_volume_callback (GtkAction *action,
			      gpointer data)
{
	NautilusFile *file;
	GList *selection;
	GnomeVFSDrive *drive;
	FMDirectoryView *view;

        view = FM_DIRECTORY_VIEW (data);
	
	selection = fm_directory_view_get_selection (view);

	if (!eel_g_list_exactly_one_item (selection)) {
		nautilus_file_list_free (selection);
		return;
	}

	file = NAUTILUS_FILE (selection->data);
	
	if (nautilus_file_has_drive (file)) {
		drive = nautilus_file_get_drive (file);
		if (drive != NULL) {
			gnome_vfs_drive_mount (drive, drive_mounted_callback, NULL);
		}
	}

	nautilus_file_list_free (selection);
}

static gboolean
eject_for_type (GnomeVFSDeviceType type)
{
	switch (type) {
	case GNOME_VFS_DEVICE_TYPE_CDROM:
	case GNOME_VFS_DEVICE_TYPE_ZIP:
	case GNOME_VFS_DEVICE_TYPE_JAZ:
		return TRUE;
	default:
		return FALSE;
	}
}


static void
volume_or_drive_unmounted_callback (gboolean succeeded,
				    char *error,
				    char *detailed_error,
				    gpointer data)
{
	gboolean eject;

	eject = GPOINTER_TO_INT (data);
	if (!succeeded) {
		if (eject) {
			eel_show_error_dialog_with_details (error, NULL, 
			                                    _("Eject Error"), detailed_error, NULL);
		} else {
			eel_show_error_dialog_with_details (error, NULL, 
			                                    _("Unmount Error"), detailed_error, NULL);
		}
	}
}


static void
action_unmount_volume_callback (GtkAction *action,
				gpointer data)
{
	NautilusFile *file;
	GList *selection;
	GnomeVFSDrive *drive;
	GnomeVFSVolume *volume;
	FMDirectoryView *view;

        view = FM_DIRECTORY_VIEW (data);
	
	selection = fm_directory_view_get_selection (view);

	if (!eel_g_list_exactly_one_item (selection)) {
		nautilus_file_list_free (selection);
		return;
	}

	file = NAUTILUS_FILE (selection->data);
	
	if (nautilus_file_has_volume (file)) {
		volume = nautilus_file_get_volume (file);
		if (volume != NULL) {
			if (eject_for_type (gnome_vfs_volume_get_device_type (volume))) {
				gnome_vfs_volume_eject (volume, volume_or_drive_unmounted_callback, GINT_TO_POINTER (TRUE));
			} else {
				gnome_vfs_volume_unmount (volume, volume_or_drive_unmounted_callback, GINT_TO_POINTER (FALSE));
			}
		}
	} else if (nautilus_file_has_drive (file)) {
		drive = nautilus_file_get_drive (file);
		if (drive != NULL) {
			if (eject_for_type (gnome_vfs_drive_get_device_type (drive))) {
				gnome_vfs_drive_eject (drive, volume_or_drive_unmounted_callback, GINT_TO_POINTER (TRUE));
			} else {
				gnome_vfs_drive_unmount (drive, volume_or_drive_unmounted_callback, GINT_TO_POINTER (FALSE));
			}
		}
	}
	
	nautilus_file_list_free (selection);
}

static void
connect_to_server_response_callback (GtkDialog *dialog,
				     int response_id,
				     gpointer data)
{
	GtkEntry *entry;
	char *uri;
	const char *name;
	char *icon;

	entry = GTK_ENTRY (data);
	
	switch (response_id) {
	case GTK_RESPONSE_OK:
		uri = g_object_get_data (G_OBJECT (dialog), "link-uri");
		icon = g_object_get_data (G_OBJECT (dialog), "link-icon");
		name = gtk_entry_get_text (entry);
		gnome_vfs_connect_to_server (uri, (char *)name, icon);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
entry_activate_callback (GtkEntry *entry,
			 gpointer user_data)
{
	GtkDialog *dialog;
	
	dialog = GTK_DIALOG (user_data);
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
action_connect_to_server_link_callback (GtkAction *action,
					gpointer data)
{
	NautilusFile *file;
	GList *selection;
	FMDirectoryView *view;
	char *uri;
	char *icon;
	char *name;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *box;
	char *title;

        view = FM_DIRECTORY_VIEW (data);
	
	selection = fm_directory_view_get_selection (view);

	if (!eel_g_list_exactly_one_item (selection)) {
		nautilus_file_list_free (selection);
		return;
	}

	file = NAUTILUS_FILE (selection->data);

	uri = nautilus_file_get_activation_uri (file);
	icon = nautilus_icon_factory_get_icon_for_file (file, FALSE);
	name = nautilus_file_get_display_name (file);

	if (uri != NULL) {
		title = g_strdup_printf (_("Connect to Server %s"), name);
		dialog = gtk_dialog_new_with_buttons (title,
						      fm_directory_view_get_containing_window (view),
						      GTK_DIALOG_NO_SEPARATOR,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      _("_Connect"), GTK_RESPONSE_OK,
						      NULL);

		g_object_set_data_full (G_OBJECT (dialog), "link-uri", g_strdup (uri), g_free);
		g_object_set_data_full (G_OBJECT (dialog), "link-icon", g_strdup (icon), g_free);
		
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

		box = gtk_hbox_new (FALSE, 12);
		gtk_widget_show (box);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
				    box, TRUE, TRUE, 0);
		
		label = gtk_label_new_with_mnemonic (_("Link _name:"));
		gtk_widget_show (label);

		gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 12);
		
		entry = gtk_entry_new ();
		if (name) {
			gtk_entry_set_text (GTK_ENTRY (entry), name);
		}
		g_signal_connect (entry,
				  "activate", 
				  G_CALLBACK (entry_activate_callback),
				  dialog);
		
		gtk_widget_show (entry);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
		
		gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 12);
		
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (connect_to_server_response_callback),
				  entry);
		gtk_widget_show (dialog);
	}
	
	g_free (uri);
	g_free (icon);
	g_free (name);
}

static void
fm_directory_view_init_show_hidden_files (FMDirectoryView *view)
{
	NautilusWindowShowHiddenFilesMode mode;
	gboolean show_hidden_changed;
	gboolean show_hidden_default_setting;
	GtkAction *action;

	if (view->details->ignore_hidden_file_preferences) {
		return;
	}
	
	show_hidden_changed = FALSE;
	mode = nautilus_window_info_get_hidden_files_mode (view->details->window);
	
	if (mode == NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT) {
		show_hidden_default_setting = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
		if (show_hidden_default_setting != view->details->show_hidden_files) {
			view->details->show_hidden_files = show_hidden_default_setting;
			view->details->show_backup_files = show_hidden_default_setting;
			show_hidden_changed = TRUE;
		}
	} else {
		if (mode == NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_ENABLE) {
			show_hidden_changed = !view->details->show_hidden_files;
			view->details->show_hidden_files = TRUE;
			view->details->show_backup_files = TRUE;
		} else {
			show_hidden_changed = view->details->show_hidden_files;
			view->details->show_hidden_files = FALSE;
			view->details->show_backup_files = FALSE;
		}
	}
 
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_SHOW_HIDDEN_FILES);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      view->details->show_hidden_files);
	
	if (show_hidden_changed && (view->details->model != NULL)) {
		load_directory (view, view->details->model);	
	}

}

static GtkActionEntry directory_view_entries[] = {
  { "New Documents", NULL, N_("Create _Document") },               /* name, stock id, label */
  { "Open With", NULL, N_("Open Wit_h"),               /* name, stock id, label */
    NULL, N_("Choose a program with which to open the selected item") },
  { "Scripts", NULL, N_("_Scripts"),               /* name, stock id, label */
    NULL, N_("Run or manage scripts from ~/.gnome2/nautilus-scripts") },
  { "Properties", GTK_STOCK_PROPERTIES,                  /* name, stock id */
    N_("_Properties"), "<alt>Return",                /* label, accelerator */
    N_("View or modify the properties of each selected item"),                   /* tooltip */ 
    G_CALLBACK (action_properties_callback) },
  { "PropertiesAccel", NULL,                  /* name, stock id */
    "PropertiesAccel", "<control>I",                /* label, accelerator */
    NULL,                   /* tooltip */ 
    G_CALLBACK (action_properties_callback) },
  { "New Folder", NULL,                  /* name, stock id */
    N_("Create _Folder"), "<control><shift>N",                /* label, accelerator */
    N_("Create a new empty folder inside this folder"),                   /* tooltip */ 
    G_CALLBACK (action_new_folder_callback) },
  { "No Templates", NULL, N_("No templates Installed") },               /* name, stock id, label */
  { "New Empty File", NULL,                  /* name, stock id */
    N_("_Empty File"), NULL,                /* label, accelerator */
    N_("Create a new empty file inside this folder"),                   /* tooltip */ 
    G_CALLBACK (action_new_empty_file_callback) },
  { "New Launcher", NULL,                  /* name, stock id */
    N_("Create L_auncher"), NULL,                /* label, accelerator */
    N_("Create a new launcher"),                   /* tooltip */ 
    G_CALLBACK (action_new_launcher_callback) },
  { "Open", GTK_STOCK_OPEN,                  /* name, stock id */
    N_("_Open"), "<control>o",                /* label, accelerator */
    N_("Open the selected item in this window"),                   /* tooltip */ 
    G_CALLBACK (action_open_callback) },
  { "OpenAccel", NULL,                  /* name, stock id */
    "OpenAccel", "<alt>Down",                /* label, accelerator */
    NULL,                   /* tooltip */ 
    G_CALLBACK (action_open_callback) },
  { "OpenAlternate", NULL,                  /* name, stock id */
    N_("Open in Navigation Window"), "<control><shift>o",                /* label, accelerator */
    N_("Open each selected item in a navigation window"),                   /* tooltip */ 
    G_CALLBACK (action_open_alternate_callback) },
  { "OtherApplication1", NULL,                  /* name, stock id */
    N_("Open with Other _Application..."), NULL,                /* label, accelerator */
    N_("Choose another application with which to open the selected item"),                   /* tooltip */ 
    G_CALLBACK (action_other_application_callback) },
  { "OtherApplication2", NULL,                  /* name, stock id */
    N_("Open with Other _Application..."), NULL,                /* label, accelerator */
    N_("Choose another application with which to open the selected item"),                   /* tooltip */ 
    G_CALLBACK (action_other_application_callback) },
  { "Open Scripts Folder", NULL,                  /* name, stock id */
    N_("_Open Scripts Folder"), NULL,                /* label, accelerator */
    N_("Show the folder containing the scripts that appear in this menu"),                   /* tooltip */ 
    G_CALLBACK (action_open_scripts_folder_callback) },
  { "Empty Trash", NULL,                  /* name, stock id */
    N_("_Empty Trash"), NULL,                /* label, accelerator */
    N_("Delete all items in the Trash"),                   /* tooltip */ 
    G_CALLBACK (action_empty_trash_callback) },
  { "Cut", GTK_STOCK_CUT,                  /* name, stock id */
    N_("Cu_t Files"), "<control>x",                /* label, accelerator */
    N_("Prepare the selected files to be moved with a Paste Files command"),                   /* tooltip */ 
    G_CALLBACK (action_cut_files_callback) },
  { "Copy", GTK_STOCK_COPY,                  /* name, stock id */
    N_("_Copy Files"), "<control>c",                /* label, accelerator */
    N_("Prepare the selected files to be copied with a Paste Files command"),                   /* tooltip */ 
    G_CALLBACK (action_copy_files_callback) },
  { "Paste", GTK_STOCK_PASTE,                  /* name, stock id */
    N_("_Paste Files"), "<control>v",                /* label, accelerator */
    N_("Move or copy files previously selected by a Cut Files or Copy Files command"),                   /* tooltip */ 
    G_CALLBACK (action_paste_files_callback) },
  /* We make accelerator "" instead of null here to not inherit the stock
     accelerator for paste */
  { "Paste Files Into", GTK_STOCK_PASTE,                  /* name, stock id */
    N_("_Paste Files Into Folder"), "",                /* label, accelerator */
    N_("Move or copy files previously selected by a Cut Files or Copy Files command into the selected folder"),                   /* tooltip */ 
    G_CALLBACK (action_paste_files_into_callback) },
  { "Select All", NULL,                  /* name, stock id */
    N_("Select _All Files"), "<control>A",                /* label, accelerator */
    N_("Select all items in this window"),                   /* tooltip */ 
    G_CALLBACK (action_select_all_callback) },
  { "Select Pattern", NULL,                  /* name, stock id */
    N_("Select _Pattern"), "<control>S",                /* label, accelerator */
    N_("Select items in this window matching a given pattern"),                   /* tooltip */ 
    G_CALLBACK (action_select_pattern_callback) },
  { "Duplicate", NULL,                  /* name, stock id */
    N_("D_uplicate"), NULL,                /* label, accelerator */
    N_("Duplicate each selected item"),                   /* tooltip */ 
    G_CALLBACK (action_duplicate_callback) },
  { "Create Link", NULL,                  /* name, stock id */
    N_("Ma_ke Link"), "<control>M",                /* label, accelerator */
    N_("Create a symbolic link for each selected item"),                   /* tooltip */ 
    G_CALLBACK (action_create_link_callback) },
  { "Rename", NULL,                  /* name, stock id */
    N_("_Rename..."), "F2",                /* label, accelerator */
    N_("Rename selected item"),                   /* tooltip */ 
    G_CALLBACK (action_rename_callback) },
  { "Trash", GTK_STOCK_DELETE,                  /* name, stock id */
    N_("Mo_ve to Trash"), "<control>T",                /* label, accelerator */
    N_("Move each selected item to the Trash"),                   /* tooltip */ 
    G_CALLBACK (action_trash_callback) },
  { "Delete", NULL,                  /* name, stock id */
    N_("_Delete"), "<shift>Delete",                /* label, accelerator */
    N_("Delete each selected item, without moving to the Trash"),                   /* tooltip */ 
    G_CALLBACK (action_delete_callback) },
  { "Reset to Defaults", NULL,                  /* name, stock id */
    N_("Reset View to _Defaults"), NULL,                /* label, accelerator */
    N_("Reset sorting order and zoom level to match preferences for this view"),                   /* tooltip */ 
    G_CALLBACK (action_reset_to_defaults_callback) },
  { "Reset Background", NULL,                  /* name, stock id */
    N_("Use _Default Background"), NULL,                /* label, accelerator */
    N_("Use the default background for this location"),                   /* tooltip */ 
    G_CALLBACK (action_reset_background_callback) },
  { "Connect To Server Link", NULL,                  /* name, stock id */
    N_("Connect To This Server"), NULL,                /* label, accelerator */
    N_("Make a permanent connection to this server"),                   /* tooltip */ 
    G_CALLBACK (action_connect_to_server_link_callback) },
  { "Mount Volume", NULL,                  /* name, stock id */
    N_("_Mount Volume"), NULL,                /* label, accelerator */
    N_("Mount the selected volume"),                   /* tooltip */ 
    G_CALLBACK (action_mount_volume_callback) },
  { "Unmount Volume", NULL,                  /* name, stock id */
    N_("_Unmount Volume"), NULL,                /* label, accelerator */
    N_("Unmount the selected volume"),                   /* tooltip */ 
    G_CALLBACK (action_unmount_volume_callback) },
  { "OpenCloseParent", NULL,                  /* name, stock id */
    N_("Open File and Close window"), "<alt><shift>Down",                /* label, accelerator */
    NULL,                   /* tooltip */ 
    G_CALLBACK (action_open_close_parent_callback) },
};

static GtkToggleActionEntry directory_view_toggle_entries[] = {
  { "Show Hidden Files", NULL,                  /* name, stock id */
    N_("Show _Hidden Files"), NULL,                /* label, accelerator */
    N_("Toggles the display of hidden files in the current window"),                   /* tooltip */ 
    G_CALLBACK (action_show_hidden_files_callback),
    TRUE },
};

static void
real_merge_menus (FMDirectoryView *view)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	const char *ui;

	ui_manager = nautilus_window_info_get_ui_manager (view->details->window);

	action_group = gtk_action_group_new ("DirViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	view->details->dir_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      directory_view_entries, G_N_ELEMENTS (directory_view_entries),
				      view);
	gtk_action_group_add_toggle_actions (action_group, 
					     directory_view_toggle_entries, G_N_ELEMENTS (directory_view_toggle_entries),
					     view);

	action = gtk_action_group_get_action (action_group, FM_ACTION_NO_TEMPLATES);
	gtk_action_set_sensitive (action, FALSE);

	/* Insert action group at end so clipboard action group ends up before it */
	gtk_ui_manager_insert_action_group (ui_manager, action_group, -1);
	g_object_unref (action_group); /* owned by ui manager */

	ui = nautilus_ui_string_get ("nautilus-directory-view-ui.xml");
	view->details->dir_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);
	g_signal_connect_object (fm_directory_view_get_background (view), "settings_changed",
				 G_CALLBACK (schedule_update_menus), G_OBJECT (view),
				 G_CONNECT_SWAPPED);
	
	view->details->scripts_invalid = TRUE;
	view->details->templates_invalid = TRUE;
}

static void
clipboard_targets_received (GtkClipboard     *clipboard,
			    GtkSelectionData *selection_data,
			    gpointer          user_data)
{
	FMDirectoryView *view;
	gboolean can_paste;
	GdkAtom *targets;
	int n_targets;
	int i;
	GList *selection;
	int count;
	GtkAction *action;

	view = FM_DIRECTORY_VIEW (user_data);
	can_paste = FALSE;

	if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets)) {
		for (i=0; i < n_targets; i++) {
			if (targets[i] == copied_files_atom) {
				can_paste = TRUE;
			}
		}

		g_free (targets);
	}
	
	
	selection = fm_directory_view_get_selection (view);
	count = g_list_length (selection);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_PASTE);
	gtk_action_set_sensitive (action,
				  can_paste && !fm_directory_view_is_read_only (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_PASTE_FILES_INTO);
	gtk_action_set_sensitive (action,
				  can_paste && count == 1 &&
				  nautilus_file_is_directory (NAUTILUS_FILE (selection->data)) &&
				  nautilus_file_can_write (NAUTILUS_FILE (selection->data)));
	
	nautilus_file_list_free (selection);
	
	g_object_unref (view);
}

static gboolean
showing_trash_directory (FMDirectoryView *view)
{
	NautilusFile *file;

	file = fm_directory_view_get_directory_as_file (view);
	if (file != NULL) {
		return nautilus_file_is_in_trash (file);
	}
	return FALSE;
}

static gboolean
should_show_empty_trash (FMDirectoryView *view)
{
	return (showing_trash_directory (view) || nautilus_window_info_get_window_type (view->details->window) == NAUTILUS_WINDOW_NAVIGATION);
}

static gboolean
file_list_all_are_folders (GList *file_list)
{
	GList *l;
	NautilusFile *file, *linked_file;
	char *activation_uri;
	gboolean is_dir;
	
	for (l = file_list; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		if (nautilus_file_is_nautilus_link (file) &&
		    !NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
			activation_uri = nautilus_file_get_activation_uri (file);
			
			if (activation_uri == NULL ||
			    eel_str_has_prefix (activation_uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER) ||
			    eel_str_has_prefix (activation_uri, NAUTILUS_COMMAND_SPECIFIER)) {
				g_free (activation_uri);
				return FALSE;
			}

			linked_file = nautilus_file_get_existing (activation_uri);

			/* We might not actually know the type of the linked file yet,
			 * however we don't want to schedule a read, since that might do things
			 * like ask for password etc. This is a bit unfortunate, but I don't
			 * know any way around it, so we do various heuristics here
			 * to get things mostly right 
			 */
			is_dir =
				(linked_file != NULL &&
				 nautilus_file_is_directory (linked_file)) ||
				nautilus_file_has_volume (file) ||
				nautilus_file_has_drive (file) ||
				(activation_uri != NULL &&
				 activation_uri[strlen (activation_uri) - 1] == '/');
			
			nautilus_file_unref (linked_file);
			g_free (activation_uri);
			
			if (!is_dir) {
				return FALSE;
			}
		} else if (!(nautilus_file_is_directory (file) ||
			     NAUTILUS_IS_DESKTOP_ICON_FILE (file))) {
			return FALSE;
		}
	}
	return TRUE;
}

static void
real_update_menus_volumes (FMDirectoryView *view,
			   GList *selection,
			   gint selection_count)
{
	NautilusFile *file;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean unmount_is_eject;
	gboolean show_connect;
	GnomeVFSVolume *volume;
	GnomeVFSDrive *drive;
	GtkAction *action;
	char *uri;

	show_mount = FALSE;
	show_unmount = FALSE;
	unmount_is_eject = FALSE;
	show_connect = FALSE;

	if (selection_count == 1) {
		file = NAUTILUS_FILE (selection->data);

		if (nautilus_file_has_volume (file)) {
			show_unmount = TRUE;

			volume = nautilus_file_get_volume (file);
			unmount_is_eject = eject_for_type (gnome_vfs_volume_get_device_type (volume));
		} else if (nautilus_file_has_drive (file)) {
			drive = nautilus_file_get_drive (file);
			if (gnome_vfs_drive_is_mounted (drive)) {
				show_unmount = TRUE;
				unmount_is_eject = eject_for_type (gnome_vfs_drive_get_device_type (drive));
			} else {
				show_mount = TRUE;
			}
		} else if (nautilus_file_is_nautilus_link (file)) {
			uri = nautilus_file_get_activation_uri (file);
			if (uri != NULL &&
			    (eel_istr_has_prefix (uri, "ftp:") ||
			     eel_istr_has_prefix (uri, "dav:") ||
			     eel_istr_has_prefix (uri, "davs:"))) {
				show_connect = TRUE;
				g_free (uri);
			}
		} else if (nautilus_file_is_mime_type (file,
						       "x-directory/smb-share")) {
			show_connect = TRUE;
		}
										      
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_CONNECT_TO_SERVER_LINK);
	gtk_action_set_visible (action, show_connect);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_MOUNT_VOLUME);
	gtk_action_set_visible (action, show_mount);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_UNMOUNT_VOLUME);
	gtk_action_set_visible (action, show_unmount);
	
	if (show_unmount) {
		g_object_set (action, "label", 
			      unmount_is_eject? _("E_ject"):_("_Unmount Volume"),
			      NULL);
	}

}

static void
real_update_paste_menu (FMDirectoryView *view,
			GList *selection,
			gint selection_count)
{
	gboolean can_paste_files_into;
	gboolean selection_is_read_only;
	gboolean is_read_only;
	GtkAction *action;

	selection_is_read_only = selection_count == 1
		&& !nautilus_file_can_write (NAUTILUS_FILE (selection->data));
	
	is_read_only = fm_directory_view_is_read_only (view);
	
	can_paste_files_into = selection_count == 1 && 
		nautilus_file_is_directory (NAUTILUS_FILE (selection->data));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_PASTE);
	gtk_action_set_sensitive (action, !is_read_only);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_PASTE_FILES_INTO);
	gtk_action_set_visible (action, can_paste_files_into);
	gtk_action_set_sensitive (action, !selection_is_read_only);
	if (!selection_is_read_only || !is_read_only) {
		/* Ask the clipboard */
		g_object_ref (view); /* Need to keep the object alive until we get the reply */
		gtk_clipboard_request_contents (get_clipboard (view),
						gdk_atom_intern ("TARGETS", FALSE),
						clipboard_targets_received,
						view);
	}
}

static void
clipboard_changed_callback (NautilusClipboardMonitor *monitor, FMDirectoryView *view)
{
	GList *selection;
	gint selection_count;
	
	selection = fm_directory_view_get_selection (view);
	selection_count = g_list_length (selection);

	real_update_paste_menu (view, selection, selection_count);

	nautilus_file_list_free (selection);
	
}

static void
real_update_menus (FMDirectoryView *view)
{
	GList *selection;
	gint selection_count;
	const char *tip, *label;
	char *label_with_underscore;
	gboolean selection_contains_special_link;
	gboolean is_read_only;
	gboolean can_create_files;
	gboolean can_delete_files;
	gboolean can_copy_files;
	gboolean can_link_files;
	gboolean can_duplicate_files;
	gboolean show_separate_delete_command;
	gboolean vfolder_directory;
	gboolean show_open_alternate;
	gboolean can_open;
	ActivationAction activation_action;
	EelBackground *background;
	GtkAction *action;

	selection = fm_directory_view_get_selection (view);
	selection_count = g_list_length (selection);

	selection_contains_special_link = special_link_in_selection (view);
	is_read_only = fm_directory_view_is_read_only (view);

	can_create_files = fm_directory_view_supports_creating_files (view);
	can_delete_files = !is_read_only
		&& selection_count != 0
		&& !selection_contains_special_link;
	can_copy_files = selection_count != 0
		&& !selection_contains_special_link;	

	can_duplicate_files = can_create_files && can_copy_files;
	can_link_files = can_create_files && can_copy_files;
	
	vfolder_directory = we_are_in_vfolder_desktop_dir (view);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_RENAME);
	gtk_action_set_sensitive (action,
				  selection_count == 1 &&
				  fm_directory_view_can_rename_file (view, selection->data));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_NEW_FOLDER);
	gtk_action_set_sensitive (action, can_create_files);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_OPEN);
	gtk_action_set_sensitive (action,  selection_count != 0);
	
	label_with_underscore = NULL;
	can_open = TRUE;
	if (selection_count == 1) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (selection->data);
		
		activation_action = get_activation_action (file);
		
		/* Only use the default app for open if there is not
		   a mime mismatch, otherwise we can't use it in the
		   open with menu */
		if (activation_action == ACTIVATION_ACTION_OPEN_IN_APPLICATION &&
		    can_show_default_app (view, file)) {
			GnomeVFSMimeApplication *app;

			app = nautilus_mime_get_default_application_for_file (file);
			if (app) {
				label_with_underscore = g_strdup_printf (_("_Open with \"%s\""),
									 app->name);
				gnome_vfs_mime_application_free (app);
			} else {
				can_open = FALSE;
			}
		}
	} 

	g_object_set (action, "label", 
		      label_with_underscore ? label_with_underscore : _("_Open"),
		      NULL);
	gtk_action_set_visible (action, can_open);
	
	g_free (label_with_underscore);

	show_open_alternate = file_list_all_are_folders (selection);
	if (nautilus_window_info_get_window_type (view->details->window) == NAUTILUS_WINDOW_NAVIGATION) {
		if (selection_count == 0) {
			label_with_underscore = g_strdup (_("Open in New Window"));
		} else {
			label_with_underscore = g_strdup_printf (ngettext("Open in New Window",
									  "Open in %d New Windows",
									  selection_count), 
								 selection_count);
		}
	} else {
		if (selection_count <= 1) {
			label_with_underscore = g_strdup (_("Browse Folder"));
		} else {
			label_with_underscore = g_strdup_printf (_("Browse Folders"));
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_OPEN_ALTERNATE);
	g_object_set (action, "label", 
		      label_with_underscore,
		      NULL);
	g_free (label_with_underscore);
	
	gtk_action_set_sensitive (action,  selection_count != 0);
	gtk_action_set_visible (action, show_open_alternate);
	
	/* Broken into its own function just for convenience */
	reset_open_with_menu (view, selection);
	reset_extension_actions_menu (view, selection);

	if (all_selected_items_in_trash (view)) {
		label = _("_Delete from Trash");
		tip = _("Delete all selected items permanently");
		show_separate_delete_command = FALSE;
	} else {
		label = _("Mo_ve to Trash");
		tip = _("Move each selected item to the Trash");
		show_separate_delete_command = show_delete_command_auto_value;
	}
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_TRASH);
	g_object_set (action,
		      "label", label,
		      "tooltip", tip,
		      NULL);
	gtk_action_set_sensitive (action, can_delete_files);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_DELETE);
	gtk_action_set_visible (action, show_separate_delete_command);
	
	if (show_separate_delete_command) {
		g_object_set (action,
			      "label", _("_Delete"),
			      NULL);
		gtk_action_set_sensitive (action, can_delete_files);
	}
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_DUPLICATE);
	gtk_action_set_sensitive (action, can_duplicate_files);

	background = fm_directory_view_get_background (view);
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_RESET_BACKGROUND);
	gtk_action_set_sensitive (action, 
				  background != NULL &&
				  nautilus_file_background_is_set (background));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_CREATE_LINK);
	gtk_action_set_sensitive (action, can_link_files);
	g_object_set (action, "label",
		      selection_count > 1
		      ? _("Ma_ke Links")
		      : _("Ma_ke Link"),
		      NULL);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_PROPERTIES);
	gtk_action_set_sensitive (action,
				  selection_count != 0 &&
				  fm_directory_view_supports_properties (view));
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_PROPERTIES_ACCEL);
	gtk_action_set_sensitive (action,
				  selection_count != 0 &&
				  fm_directory_view_supports_properties (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_EMPTY_TRASH);
	g_object_set (action,
		      "label", _("_Empty Trash"),
		      NULL);
	gtk_action_set_sensitive (action, !nautilus_trash_monitor_is_empty ());
	gtk_action_set_visible (action, should_show_empty_trash (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_SELECT_ALL);
	gtk_action_set_sensitive (action, !fm_directory_view_is_empty (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_CUT);
	g_object_set (action, "label",
		      selection_count == 1
		      ? _("Cu_t File")
		      : _("Cu_t Files"),
		      NULL);
	gtk_action_set_sensitive (action, can_delete_files);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_COPY);
	g_object_set (action, "label",
		      selection_count == 1
		      ? _("_Copy File")
		      : _("_Copy Files"),
		      NULL);
	gtk_action_set_sensitive (action, can_copy_files);

	real_update_paste_menu (view, selection, selection_count);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_NEW_LAUNCHER);
	gtk_action_set_visible (action, vfolder_directory);
	gtk_action_set_sensitive (action, can_create_files);

	real_update_menus_volumes (view, selection, selection_count);

	nautilus_file_list_free (selection);

	if (view->details->scripts_invalid) {
		update_scripts_menu (view);
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      FM_ACTION_NEW_DOCUMENTS);
	gtk_action_set_sensitive (action, can_create_files);
	
	if (can_create_files && view->details->templates_invalid) {
		update_templates_menu (view);
	}
}

/**
 * fm_directory_view_pop_up_selection_context_menu
 *
 * Pop up a context menu appropriate to the selected items.
 * @view: FMDirectoryView of interest.
 * @event: The event that triggered this context menu.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/
void 
fm_directory_view_pop_up_selection_context_menu  (FMDirectoryView *view, 
						  GdkEventButton *event)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_menus_if_pending (view);

	eel_pop_up_context_menu (create_popup_menu 
				      	(view, FM_DIRECTORY_VIEW_POPUP_PATH_SELECTION),
				      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      event);
}

/**
 * fm_directory_view_pop_up_background_context_menu
 *
 * Pop up a context menu appropriate to the view globally at the last right click location.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/
void 
fm_directory_view_pop_up_background_context_menu (FMDirectoryView *view, 
						  GdkEventButton *event)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_menus_if_pending (view);

	eel_pop_up_context_menu (create_popup_menu 
				      (view, FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND),
				      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      event);
}

static void
schedule_update_menus (FMDirectoryView *view) 
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Make sure we haven't already destroyed it */
	/*g_assert (view->details->window != NULL);*/

	view->details->menu_states_untrustworthy = TRUE;

	if (view->details->update_menus_timeout_id == 0) {
		view->details->update_menus_timeout_id
			= g_timeout_add (300, update_menus_timeout_callback, view);
	}
}

static void
remove_update_status_idle_callback (FMDirectoryView *view) 
{
	if (view->details->update_status_idle_id != 0) {
		g_source_remove (view->details->update_status_idle_id);
		view->details->update_status_idle_id = 0;
	}
}

static gboolean
update_status_idle_callback (gpointer data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);
	fm_directory_view_display_selection_info (view);
	view->details->update_status_idle_id = 0;
	return FALSE;
}

static void
schedule_update_status (FMDirectoryView *view) 
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Make sure we haven't already destroyed it */
	g_assert (view->details->window != NULL);

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
 * fm_directory_view_notify_selection_changed:
 * 
 * Notify this view that the selection has changed. This is normally
 * called only by subclasses.
 * @view: FMDirectoryView whose selection has changed.
 * 
 **/
void
fm_directory_view_notify_selection_changed (FMDirectoryView *view)
{
	NautilusFile *file;
	GList *selection;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!view->details->selection_change_is_due_to_shell) {
		view->details->send_selection_change_to_shell = TRUE;
	}

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
		schedule_update_menus (view);

		/* If there's exactly one item selected we sniff the slower attributes needed
		 * to activate a file ahead of time to improve interactive response.
		 */
		selection = fm_directory_view_get_selection (view);

		if (eel_g_list_exactly_one_item (selection)) {
			file = NAUTILUS_FILE (selection->data);
			
			if (nautilus_file_needs_slow_mime_type (file)) {
				nautilus_file_call_when_ready
					(file,
					 NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE,
					 NULL,
					 NULL);
			}

			nautilus_file_call_when_ready 
				(file, 
				 NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI,
				 NULL, 
				 NULL);
		}

		nautilus_file_list_free (selection);
	}
}

static gboolean
file_is_launchable (NautilusFile *file)
{
	char *mime_type;
	gboolean type_can_be_executable;

	mime_type = nautilus_file_get_mime_type (file);
	type_can_be_executable = gnome_vfs_mime_can_be_executable (mime_type);
	g_free (mime_type);

	return type_can_be_executable 
		&& nautilus_file_can_get_permissions (file)
		&& nautilus_file_can_execute (file)
		&& nautilus_file_is_executable (file) 
		&& !nautilus_file_is_directory (file);
}

static void
report_broken_symbolic_link (FMDirectoryView *view, NautilusFile *file)
{
	char *target_path;
	char *prompt;
	char *detail;
	GtkDialog *dialog;
	GList file_as_list;
	int response;
	
	g_assert (nautilus_file_is_broken_symbolic_link (file));

	target_path = nautilus_file_get_symbolic_link_target_path (file);
	prompt = _("The link is broken, do you want to move it to the Trash?");
	if (target_path == NULL) {
		detail = g_strdup (_("This link can't be used, because it has no target."));
	} else {
		detail = g_strdup_printf (_("This link can't be used, because its target "
					    "\"%s\" doesn't exist."), target_path);
	}

	dialog = eel_show_yes_no_dialog (prompt,
					 detail, _("Broken Link"), _("Mo_ve to Trash"), GTK_STOCK_CANCEL,
					 fm_directory_view_get_containing_window (view));

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
	        trash_or_delete_files (view, &file_as_list);					 
	}

	g_free (target_path);
	g_free (detail);
}

static void
activate_weak_notify (gpointer user_data, 
		      GObject *object)
{
	eel_timed_wait_stop (cancel_activate_callback, user_data);
	cancel_activate_callback (user_data);
}	      

static void
stop_activate (ActivateParameters *parameters)
{
	eel_timed_wait_stop (cancel_activate_callback, parameters);
	g_object_weak_unref (G_OBJECT (parameters->view), 
			     activate_weak_notify, 
			     parameters);
}


static void
activate_callback (NautilusFile *file, gpointer callback_data)
{
	ActivateParameters *parameters;
	FMDirectoryView *view;
	char *orig_uri, *uri, *file_uri;
	char *executable_path, *quoted_path, *name;
	char *old_working_dir;
	ActivationAction action;
	GdkScreen *screen;

	parameters = callback_data;

	stop_activate (parameters);

	view = FM_DIRECTORY_VIEW (parameters->view);

	if (!activate_check_mime_types (view, file, TRUE)) {
		nautilus_file_unref (file);
		g_free (parameters);
		
		return;
	}

	orig_uri = uri = nautilus_file_get_activation_uri (file);

	action = get_activation_action (file);

	screen = gtk_widget_get_screen (GTK_WIDGET (view));

	if (action == ACTIVATION_ACTION_ASK) {
		/* Special case for executable text files, since it might be
		 * dangerous & unexpected to launch these.
		 */
		action = get_executable_text_file_action (view, file);
	}
	
	switch (action) {
	case ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE :
		file_uri = nautilus_file_get_uri (file);
		nautilus_launch_desktop_file (
				screen, file_uri, NULL,
				fm_directory_view_get_containing_window (view));
		g_free (file_uri);		 
		break;
	case ACTIVATION_ACTION_LAUNCH_APPLICATION_FROM_COMMAND :
		uri += strlen (NAUTILUS_COMMAND_SPECIFIER);
		nautilus_launch_application_from_command (screen, NULL, uri, NULL, FALSE);
		break;
	case ACTIVATION_ACTION_LAUNCH :
	case ACTIVATION_ACTION_LAUNCH_IN_TERMINAL :
		old_working_dir = change_to_view_directory (view);

		executable_path = gnome_vfs_get_local_path_from_uri (uri);
		quoted_path = g_shell_quote (executable_path);
		name = nautilus_file_get_name (file);
		nautilus_launch_application_from_command 
			(screen, name, quoted_path, NULL,
			 (action == ACTIVATION_ACTION_LAUNCH_IN_TERMINAL) /* use terminal */ );
		g_free (name);
		g_free (quoted_path);

		chdir (old_working_dir);
		g_free (old_working_dir);
		g_free (executable_path);
		
		break;
	case ACTIVATION_ACTION_OPEN_IN_VIEW :
		open_location (view, uri, parameters->mode, parameters->flags);
		break;
	case ACTIVATION_ACTION_OPEN_IN_APPLICATION :
		nautilus_launch_show_file
			(file, fm_directory_view_get_containing_window (view));
		
		if ((parameters->flags & NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND) != 0) {
			if (nautilus_window_info_get_window_type (view->details->window) == NAUTILUS_WINDOW_SPATIAL) {
				nautilus_window_info_close (view->details->window);
			}
		}
		
		/* We should not add trash and directory uris.*/
		if ((!nautilus_file_is_in_trash (file)) && 
		    (!nautilus_file_is_directory (file))) {
			file_uri = nautilus_file_get_uri (file);
			egg_recent_model_add (nautilus_recent_get_model (), file_uri);
			g_free (file_uri);
		}
		break;
	case ACTIVATION_ACTION_DO_NOTHING :
		break;
	case ACTIVATION_ACTION_ASK :
		g_assert_not_reached ();
		break;
	}

	nautilus_file_unref (file);

	g_free (orig_uri);
	g_free (parameters);
}

static void
activation_drive_mounted_callback (gboolean succeeded,
				   char *error,
				   char *detailed_error,
				   gpointer callback_data)
{
	ActivateParameters *parameters;

	parameters = callback_data;

	parameters->mounted = TRUE;
	parameters->mounting = FALSE;
	
	if (succeeded && !parameters->cancelled) {
		activate_activation_uri_ready_callback (parameters->file,
							parameters);
	} else {
		if (!parameters->cancelled) {
			stop_activate (parameters);

			eel_show_error_dialog_with_details (error, NULL,
							    _("Mount Error"),
			                                    detailed_error, 
							    NULL);
		}
		
		nautilus_file_unref (parameters->file);
		
		g_free (parameters);
	}
}


static void
activate_activation_uri_ready_callback (NautilusFile *file, gpointer callback_data)
{
	ActivateParameters *parameters;
	NautilusFile *actual_file;
	NautilusFileAttributes attributes;
	GnomeVFSDrive *drive;
	char *uri;
	
	parameters = callback_data;

	if (nautilus_file_is_broken_symbolic_link (file)) {
		stop_activate (parameters);
		report_broken_symbolic_link (parameters->view, file);
		nautilus_file_unref (parameters->file);
		g_free (parameters);
		return;
	}

	if (!parameters->mounted && nautilus_file_has_drive (file)) {
		drive = nautilus_file_get_drive (file);
		if (drive != NULL &&
		    !gnome_vfs_drive_is_mounted (drive)) {
			parameters->mounting = TRUE;
			gnome_vfs_drive_mount (drive, activation_drive_mounted_callback, callback_data);
			return;
		}
	}
	
	/* We want the file for the activation URI since we care
	 * about the attributes for that, not for the original file.
	 */
	actual_file = NULL;
	uri = nautilus_file_get_activation_uri (file);
	if (!(eel_str_has_prefix (uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER) ||
	      eel_str_has_prefix (uri, NAUTILUS_COMMAND_SPECIFIER))) {
		actual_file = nautilus_file_get (uri);
		nautilus_file_unref (file);
	}
	g_free (uri);
	
	if (actual_file == NULL) {
		actual_file = file;
	}
	
	/* get the parameters for the actual file */	
	attributes = nautilus_mime_actions_get_minimum_file_attributes () | 
		NAUTILUS_FILE_ATTRIBUTE_FILE_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI;

	parameters->file = actual_file;
	parameters->callback = activate_callback;

	nautilus_file_call_when_ready
		(actual_file, attributes, activate_callback, parameters);
}

static void
cancel_activate_callback (gpointer callback_data)
{
	ActivateParameters *parameters;

	parameters = (ActivateParameters *) callback_data;

	parameters->cancelled = TRUE;
	if (!parameters->mounting) {
		nautilus_file_cancel_call_when_ready (parameters->file, 
						      parameters->callback, 
						      parameters);
		
		nautilus_file_unref (parameters->file);
		
		g_free (parameters);
	}
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
static void
fm_directory_view_activate_file (FMDirectoryView *view, 
				 NautilusFile *file,
				 NautilusWindowOpenMode mode,
				 NautilusWindowOpenFlags flags)
{
	ActivateParameters *parameters;
	NautilusFileAttributes attributes;
	char *file_name;
	char *timed_wait_prompt;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	/* link target info might be stale, re-read it */
	if (nautilus_file_is_symbolic_link (file)) {
		nautilus_file_invalidate_attributes 
			(file, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	}

	nautilus_file_ref (file);

	/* Might have to read some of the file to activate it. */
	attributes = NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI |
		NAUTILUS_FILE_ATTRIBUTE_VOLUMES;

	parameters = g_new (ActivateParameters, 1);
	parameters->view = view;
	parameters->file = file;
	parameters->mode = mode;
	parameters->flags = flags;
	parameters->callback = activate_activation_uri_ready_callback;
	parameters->mounted = FALSE;
	parameters->mounting = FALSE;
	parameters->cancelled = FALSE;

	file_name = nautilus_file_get_display_name (file);
	timed_wait_prompt = g_strdup_printf (_("Opening \"%s\"."), file_name);
	g_free (file_name);
	
	eel_timed_wait_start_with_duration
		(DELAY_UNTIL_CANCEL_MSECS,
		 cancel_activate_callback,
		 parameters,
		 _("Cancel Open?"),
		 timed_wait_prompt,
		 fm_directory_view_get_containing_window (view));
	g_free (timed_wait_prompt);

	g_object_weak_ref (G_OBJECT (view), activate_weak_notify, parameters);

	nautilus_file_call_when_ready
		(file, attributes, activate_activation_uri_ready_callback, parameters);
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
fm_directory_view_activate_files (FMDirectoryView *view, 
				  GList *files,
				  NautilusWindowOpenMode mode,
				  NautilusWindowOpenFlags flags)
{
	GList *node;
	int file_count;
	gboolean use_new_window;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	/* If there's a single file to activate, check user's preference whether
	 * to open it in this window or a new window. If there is more than one
	 * file to activate, open each one in a new window. Don't try to choose
	 * one special one to replace the current window's contents; we tried this
	 * but it proved mysterious in practice.
	 */
	file_count = g_list_length (files);
	use_new_window = file_count > 1;

	if (use_new_window && mode == NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE) {
#if !NEW_UI_COMPLETE
		/* Match the current window type */
		mode = NAUTILUS_WINDOW_OPEN_IN_SPATIAL;
#endif
	}
	
	if (!use_new_window || fm_directory_view_confirm_multiple_windows (view, file_count)) {
		for (node = files; node != NULL; node = node->next) {  	
			/* The ui should ask for navigation or object windows
			 * depending on what the current one is */
			fm_directory_view_activate_file
				(view, node->data, mode, flags);
		}
	}
}

static void
file_changed_callback (NautilusFile *file, gpointer callback_data)
{
	FMDirectoryView *view = FM_DIRECTORY_VIEW (callback_data);

	schedule_update_menus (view);
	schedule_update_status (view);

	/* We might have different capabilities, so we need to update
	   relative icon emblems . (Writeable etc) */
	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view, emblems_changed, (view));
}

/**
 * load_directory:
 * 
 * Switch the displayed location to a new uri. If the uri is not valid,
 * the location will not be switched; user feedback will be provided instead.
 * @view: FMDirectoryView whose location will be changed.
 * @uri: A string representing the uri to switch to.
 * 
 **/
static void
load_directory (FMDirectoryView *view,
		NautilusDirectory *directory)
{
	NautilusDirectory *old_directory;
	NautilusFile *old_file;
	NautilusFileAttributes attributes;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	view->details->loading = TRUE;

	/* Update menus when directory is empty, before going to new
	 * location, so they won't have any false lingering knowledge
	 * of old selection.
	 */
	schedule_update_menus (view);

	disconnect_model_handlers (view);

	old_directory = view->details->model;
	nautilus_directory_ref (directory);
	view->details->model = directory;
	nautilus_directory_unref (old_directory);

	old_file = view->details->directory_as_file;
	view->details->directory_as_file =
		nautilus_directory_get_corresponding_file (directory);
	nautilus_file_unref (old_file);

	view->details->reported_load_error = FALSE;

	/* FIXME bugzilla.gnome.org 45062: In theory, we also need to monitor metadata here (as
         * well as doing a call when ready), in case external forces
         * change the directory's file metadata.
	 */
	attributes = NAUTILUS_FILE_ATTRIBUTE_METADATA;
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
	attributes = NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES;
	nautilus_file_monitor_add (view->details->directory_as_file,
				   &view->details->directory_as_file,
				   attributes);

	view->details->file_changed_handler_id = g_signal_connect
		(view->details->directory_as_file, "changed",
		 G_CALLBACK (file_changed_callback), view);
}

static void
finish_loading (FMDirectoryView *view)
{
	NautilusFileAttributes attributes;

	nautilus_window_info_report_load_underway (view->details->window,
						   NAUTILUS_VIEW (view));

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	fm_directory_view_begin_loading (view);

	if (nautilus_directory_are_all_files_seen (view->details->model)) {
		schedule_idle_display_of_pending_files (view);		
	}
	
	/* Start loading. */

	/* Connect handlers to learn about loading progress. */
	view->details->done_loading_handler_id = g_signal_connect
		(view->details->model, "done_loading",
		 G_CALLBACK (done_loading_callback), view);
	view->details->load_error_handler_id = g_signal_connect
		(view->details->model, "load_error",
		 G_CALLBACK (load_error_callback), view);

	/* Monitor the things needed to get the right icon. Also
	 * monitor a directory's item count because the "size"
	 * attribute is based on that, and the file's metadata
	 * and possible custom name.
	 */
	attributes = nautilus_icon_factory_get_required_file_attributes ();
	attributes |= NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NAUTILUS_FILE_ATTRIBUTE_METADATA |
		NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME |
		NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

	nautilus_directory_file_monitor_add (view->details->model,
					     &view->details->model,
					     view->details->show_hidden_files,
					     view->details->show_backup_files,
					     attributes,
					     files_added_callback, view);

    	view->details->files_added_handler_id = g_signal_connect
		(view->details->model, "files_added",
		 G_CALLBACK (files_added_callback), view);
	view->details->files_changed_handler_id = g_signal_connect
		(view->details->model, "files_changed",
		 G_CALLBACK (files_changed_callback), view);
}

static void
finish_loading_if_all_metadata_loaded (FMDirectoryView *view)
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
	FMDirectoryView *view;

	view = callback_data;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (view->details->directory_as_file == file);
	g_assert (view->details->metadata_for_directory_as_file_pending);

	view->details->metadata_for_directory_as_file_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
}

static void
metadata_for_files_in_directory_ready_callback (NautilusDirectory *directory,
				   		GList *files,
			           		gpointer callback_data)
{
	FMDirectoryView *view;

	view = callback_data;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (view->details->model == directory);
	g_assert (view->details->metadata_for_files_in_directory_pending);

	view->details->metadata_for_files_in_directory_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
}

EelStringList *
fm_directory_view_get_emblem_names_to_exclude (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_emblem_names_to_exclude, (view));
}

static void
fm_directory_view_add_relative_emblems_to_exclude (FMDirectoryView *view,
						   EelStringList *list)
{
	if (!nautilus_file_can_write (view->details->directory_as_file)) {
		eel_string_list_prepend (list, NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE);
		eel_string_list_remove_duplicates (list);
	}
}

static EelStringList *
real_get_emblem_names_to_exclude (FMDirectoryView *view)
{
	EelStringList *list;
	
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	list = eel_string_list_new_from_string (NAUTILUS_FILE_EMBLEM_NAME_TRASH, TRUE);

	fm_directory_view_add_relative_emblems_to_exclude (view, list);

	return list;
}

/**
 * fm_directory_view_merge_menus:
 * 
 * Add this view's menus to the window's menu bar.
 * @view: FMDirectoryView in question.
 */
static void
fm_directory_view_merge_menus (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 merge_menus, (view));
}

static void
disconnect_handler (GObject *object, int *id)
{
	if (*id != 0) {
		g_signal_handler_disconnect (object, *id);
		*id = 0;
	}
}

static void
disconnect_directory_handler (FMDirectoryView *view, int *id)
{
	disconnect_handler (G_OBJECT (view->details->model), id);
}

static void
disconnect_directory_as_file_handler (FMDirectoryView *view, int *id)
{
	disconnect_handler (G_OBJECT (view->details->directory_as_file), id);
}

static void
disconnect_model_handlers (FMDirectoryView *view)
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

/**
 * fm_directory_view_reset_to_defaults:
 *
 * set sorting order, zoom level, etc. to match defaults
 * 
 **/
void
fm_directory_view_reset_to_defaults (FMDirectoryView *view)
{
	NautilusWindowShowHiddenFilesMode mode;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	
	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 reset_to_defaults, (view));
	mode = nautilus_window_info_get_hidden_files_mode (view->details->window);
	if (mode != NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT) {
		nautilus_window_info_set_hidden_files_mode (view->details->window,
							    NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT);
		fm_directory_view_init_show_hidden_files (view);
	}
}

/**
 * fm_directory_view_select_all:
 *
 * select all the items in the view
 * 
 **/
void
fm_directory_view_select_all (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 select_all, (view));
}

/**
 * fm_directory_view_set_selection:
 *
 * set the selection to the items identified in @selection. @selection
 * should be a list of NautilusFiles
 * 
 **/
void
fm_directory_view_set_selection (FMDirectoryView *view, GList *selection)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 set_selection, (view, selection));
}

static void
fm_directory_view_select_file (FMDirectoryView *view, NautilusFile *file)
{
	GList file_list;

	file_list.data = file;
	file_list.next = NULL;
	file_list.prev = NULL;
	fm_directory_view_set_selection (view, &file_list);
}

/**
 * fm_directory_view_get_selected_icon_locations:
 *
 * return an array of locations of selected icons if available
 * Return value: GArray of GdkPoints
 * 
 **/
GArray *
fm_directory_view_get_selected_icon_locations (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_selected_icon_locations, (view));
}

/**
 * fm_directory_view_reveal_selection:
 *
 * Scroll as necessary to reveal the selected items.
 **/
void
fm_directory_view_reveal_selection (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 reveal_selection, (view));
}

static gboolean
unref_key_and_remove (gpointer key, gpointer value, gpointer callback_data)
{
	nautilus_file_unref (key);
	return TRUE;
}

/**
 * fm_directory_view_stop:
 * 
 * Stop the current ongoing process, such as switching to a new uri.
 * @view: FMDirectoryView in question.
 * 
 **/
void
fm_directory_view_stop (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	unschedule_display_of_pending_files (view);

	/* Free extra undisplayed files */
	nautilus_file_list_free (view->details->new_added_files);
	view->details->new_added_files = NULL;
	nautilus_file_list_free (view->details->new_changed_files);
	view->details->new_changed_files = NULL;
	g_hash_table_foreach_remove (view->details->non_ready_files, unref_key_and_remove, NULL);
	nautilus_file_list_free (view->details->old_added_files);
	view->details->old_added_files = NULL;
	nautilus_file_list_free (view->details->old_changed_files);
	view->details->old_changed_files = NULL;
	eel_g_list_free_deep (view->details->pending_uris_selected);
	view->details->pending_uris_selected = NULL;

	if (view->details->model != NULL) {
		nautilus_directory_file_monitor_remove (view->details->model, view);
	}
	done_loading (view);
}

gboolean
fm_directory_view_is_read_only (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 is_read_only, (view));
}

gboolean
fm_directory_view_is_empty (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 is_empty, (view));
}

static gboolean
real_is_read_only (FMDirectoryView *view)
{
	NautilusFile *file;

	file = fm_directory_view_get_directory_as_file (view);
	if (file != NULL) {
		return !nautilus_file_can_write (file);
	}
	return FALSE;
}

gboolean
fm_directory_view_supports_creating_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 supports_creating_files, (view));
}

gboolean
fm_directory_view_accepts_dragged_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 accepts_dragged_files, (view));
}

/**
 * fm_directory_view_should_show_file
 * 
 * Returns whether or not this file should be displayed based on
 * current filtering options.
 */
gboolean
fm_directory_view_should_show_file (FMDirectoryView *view, NautilusFile *file)
{
	return nautilus_file_should_show (file, 
					  view->details->show_hidden_files, 
					  view->details->show_backup_files);
}

static gboolean
real_supports_creating_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return !fm_directory_view_is_read_only (view) && !showing_trash_directory (view);
}

static gboolean
real_accepts_dragged_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return TRUE;
}

gboolean
fm_directory_view_supports_properties (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 supports_properties, (view));
}

static gboolean
real_supports_properties (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return TRUE;
}

gboolean
fm_directory_view_supports_zooming (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 supports_zooming, (view));
}

static gboolean
real_supports_zooming (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return TRUE;
}

/**
 * fm_directory_view_update_menus:
 * 
 * Update the sensitivity and wording of dynamic menu items.
 * @view: FMDirectoryView in question.
 */
void
fm_directory_view_update_menus (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	EEL_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 update_menus, (view));

	view->details->menu_states_untrustworthy = FALSE;
}

static void
schedule_update_menus_callback (gpointer callback_data)
{
	schedule_update_menus (FM_DIRECTORY_VIEW (callback_data));
}

static void
filtering_changed_callback (gpointer callback_data)
{
	FMDirectoryView	*directory_view;
	gboolean new_show_hidden;
	NautilusWindowShowHiddenFilesMode mode;
	GtkAction *action;

	directory_view = FM_DIRECTORY_VIEW (callback_data);
	new_show_hidden = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
	mode = nautilus_window_info_get_hidden_files_mode (directory_view->details->window);

	/* only apply global show hidden files pref if local setting has not been set for this window */
	if (new_show_hidden != directory_view->details->show_hidden_files
	    && mode == NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT) {
		directory_view->details->show_hidden_files = new_show_hidden;
		directory_view->details->show_backup_files = new_show_hidden;
		
		action = gtk_action_group_get_action (directory_view->details->dir_action_group,
						      FM_ACTION_SHOW_HIDDEN_FILES);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      directory_view->details->show_hidden_files);

		/* Reload the current uri so that the filtering changes take place. */
		if (directory_view->details->model != NULL) {
			load_directory (directory_view, directory_view->details->model);
		}
	}
}

void
fm_directory_view_ignore_hidden_file_preferences (FMDirectoryView *view)
{
	g_return_if_fail (view->details->model == NULL);

	if (view->details->ignore_hidden_file_preferences) {
		return;
	}

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					      filtering_changed_callback,
					      view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					      filtering_changed_callback,
					      view);

	view->details->show_hidden_files = FALSE;
	view->details->show_backup_files = FALSE;
	view->details->ignore_hidden_file_preferences = TRUE;
}

char *
fm_directory_view_get_uri (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);
	if (view->details->model == NULL) {
		return NULL;
	}
	return nautilus_directory_get_uri (view->details->model);
}

/* Get the real directory where files will be stored and created */
char *
fm_directory_view_get_backing_uri (FMDirectoryView *view)
{
	NautilusDirectory *directory;
	char *uri;
	
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

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

void
fm_directory_view_move_copy_items (const GList *item_uris,
				   GArray *relative_item_points,
				   const char *target_uri,
				   int copy_action,
				   int x, int y,
				   FMDirectoryView *view)
{
	char *parameters, *temp;
	GList *p;
	
	g_assert (relative_item_points == NULL
		  || relative_item_points->len == 0 
		  || g_list_length ((GList *)item_uris) == relative_item_points->len);

	/* add the drop location to the icon offsets */
	offset_drop_points (relative_item_points, x, y);

	/* special-case "command:" here instead of starting a move/copy */
	if (eel_str_has_prefix (target_uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER)) {
		nautilus_launch_desktop_file (
				gtk_widget_get_screen (GTK_WIDGET (view)),
				target_uri, item_uris,
				fm_directory_view_get_containing_window (view));
		return;
	} else if (eel_str_has_prefix (target_uri, NAUTILUS_COMMAND_SPECIFIER)) {
		parameters = NULL;
		for (p = (GList *) item_uris; p != NULL; p = p->next) {
			temp = g_strconcat ((char *) p->data, " ", parameters, NULL);
			if (parameters != NULL) {
				g_free (parameters);
			}
			parameters = temp;
		}

		target_uri += strlen (NAUTILUS_COMMAND_SPECIFIER);

		nautilus_launch_application_from_command (
				gtk_widget_get_screen (GTK_WIDGET (view)),
				NULL, target_uri, parameters, FALSE);
		g_free (parameters);
		
		return;
	}

	if (eel_uri_is_trash (target_uri) && copy_action == GDK_ACTION_MOVE) {
		trash_or_delete_files_common (view, item_uris, relative_item_points, FALSE);
	} else {
		nautilus_file_operations_copy_move
			(item_uris, relative_item_points, 
			 target_uri, copy_action, GTK_WIDGET (view),
			 copy_move_done_callback, pre_copy_move (view));
	}
}

gboolean
fm_directory_view_can_accept_item (NautilusFile *target_item,
				   const char *item_uri,
				   FMDirectoryView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (target_item), FALSE);
	g_return_val_if_fail (item_uri != NULL, FALSE);
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return nautilus_drag_can_accept_item (target_item, item_uri);
}

static void
fm_directory_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
						gboolean state, gpointer callback_data)
{
	FMDirectoryView *view;

	view = (FMDirectoryView *) callback_data;
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	schedule_update_menus (view);
}

void
fm_directory_view_start_batching_selection_changes (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	++view->details->batching_selection_level;
	view->details->selection_changed_while_batched = FALSE;
}

void
fm_directory_view_stop_batching_selection_changes (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (view->details->batching_selection_level > 0);

	if (--view->details->batching_selection_level == 0) {
		if (view->details->selection_changed_while_batched) {
			fm_directory_view_notify_selection_changed (view);
		}
	}
}

static void
monitor_file_for_open_with (FMDirectoryView *view, NautilusFile *file)
{
	NautilusFile **file_spot;
	NautilusFile *old_file;
	NautilusFileAttributes attributes;

	/* Quick out when not changing. */
	file_spot = &view->details->file_monitored_for_open_with;
	old_file = *file_spot;
	if (old_file == file) {
		return;
	}

	/* Point at the new file. */
	nautilus_file_ref (file);
	*file_spot = file;

	/* Stop monitoring the old file. */
	if (old_file != NULL) {
		nautilus_file_monitor_remove (old_file, file_spot);
		nautilus_file_unref (old_file);
	}

	/* Start monitoring the new file. */
	if (file != NULL) {
		attributes = nautilus_mime_actions_get_full_file_attributes ();
		nautilus_file_monitor_add (file, file_spot, attributes);
	}
}


static void
real_sort_files (FMDirectoryView *view, GList **files)
{
}

static GArray *
real_get_selected_icon_locations (FMDirectoryView *view)
{
        /* By default, just return an empty list. */
        return g_array_new (FALSE, TRUE, sizeof (GdkPoint));
}

static void
fm_directory_view_set_property (GObject         *object,
				guint            prop_id,
				const GValue    *value,
				GParamSpec      *pspec)
{
  FMDirectoryView *directory_view;
  
  directory_view = FM_DIRECTORY_VIEW (object);

  switch (prop_id)  {
  case PROP_WINDOW:
	  g_assert (directory_view->details->window == NULL);
	  fm_directory_view_set_parent_window (directory_view, NAUTILUS_WINDOW_INFO (g_value_get_object (value)));
		  
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
fm_directory_view_class_init (FMDirectoryViewClass *klass)
{
	GtkWidgetClass *widget_class;
	GtkScrolledWindowClass *scrolled_window_class;
	GtkBindingSet *binding_set;

	widget_class = GTK_WIDGET_CLASS (klass);
	scrolled_window_class = GTK_SCROLLED_WINDOW_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = fm_directory_view_finalize;
	G_OBJECT_CLASS (klass)->set_property = fm_directory_view_set_property;

	GTK_OBJECT_CLASS (klass)->destroy = fm_directory_view_destroy;

	/* Get rid of the strange 3-pixel gap that GtkScrolledWindow
	 * uses by default. It does us no good.
	 */
	scrolled_window_class->scrollbar_spacing = 0;

	signals[ADD_FILE] =
		g_signal_new ("add_file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, add_file),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1, NAUTILUS_TYPE_FILE);
	signals[BEGIN_FILE_CHANGES] =
		g_signal_new ("begin_file_changes",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, begin_file_changes),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[BEGIN_LOADING] =
		g_signal_new ("begin_loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, begin_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[CLEAR] =
		g_signal_new ("clear",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, clear),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[END_FILE_CHANGES] =
		g_signal_new ("end_file_changes",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, end_file_changes),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[FLUSH_ADDED_FILES] =
		g_signal_new ("flush_added_files",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, flush_added_files),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[END_LOADING] =
		g_signal_new ("end_loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, end_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[FILE_CHANGED] =
		g_signal_new ("file_changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, file_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1, NAUTILUS_TYPE_FILE);
	signals[LOAD_ERROR] =
		g_signal_new ("load_error",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, load_error),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT);
	signals[REMOVE_FILE] =
		g_signal_new ("remove_file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FMDirectoryViewClass, remove_file),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1, NAUTILUS_TYPE_FILE);

	klass->accepts_dragged_files = real_accepts_dragged_files;
	klass->file_limit_reached = real_file_limit_reached;
	klass->file_still_belongs = real_file_still_belongs;
	klass->get_emblem_names_to_exclude = real_get_emblem_names_to_exclude;
	klass->get_selected_icon_locations = real_get_selected_icon_locations;
	klass->is_read_only = real_is_read_only;
	klass->load_error = real_load_error;
	klass->sort_files = real_sort_files;
	klass->can_rename_file = can_rename_file;
	klass->start_renaming_file = start_renaming_file;
	klass->supports_creating_files = real_supports_creating_files;
	klass->supports_properties = real_supports_properties;
	klass->supports_zooming = real_supports_zooming;
        klass->merge_menus = real_merge_menus;
        klass->update_menus = real_update_menus;

	/* Function pointers that subclasses must override */
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, add_file);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, bump_zoom_level);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_in);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_out);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, clear);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, file_changed);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_background_widget);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selection);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_item_count);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, is_empty);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, reset_to_defaults);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, restore_default_zoom_level);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, select_all);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, set_selection);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, zoom_to_level);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_zoom_level);

	copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);
	utf8_string_atom = gdk_atom_intern ("UTF8_STRING", FALSE);

	g_object_class_install_property (G_OBJECT_CLASS (klass),
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The parent NautilusWindowInfo reference",
							      NAUTILUS_TYPE_WINDOW_INFO,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	signals[TRASH] =
		g_signal_new ("trash",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (FMDirectoryViewClass, trash),
			      g_signal_accumulator_true_handled, NULL,
			      eel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN, 0);
	signals[DELETE] =
		g_signal_new ("delete",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (FMDirectoryViewClass, trash),
			      g_signal_accumulator_true_handled, NULL,
			      eel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN, 0);
	
	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK,
				      "trash", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0,
				      "trash", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, 0,
				      "trash", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, GDK_SHIFT_MASK,
				      "delete", 0);

	klass->trash = real_trash;
	klass->delete = real_delete;
}
