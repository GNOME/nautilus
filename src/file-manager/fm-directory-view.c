/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
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
 *          Darin Adler <darin@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#include <config.h>
#include "fm-directory-view.h"

#include "fm-desktop-icon-view.h"
#include "fm-error-reporting.h"
#include "fm-properties-window.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-zoomable.h>
#include <bonobo/bonobo-win.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-geometry.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-file-operations.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-mime-actions.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-trash-monitor.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <math.h>

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 700
#define SILENT_WINDOW_OPEN_LIMIT	5

#define DUPLICATE_HORIZONTAL_ICON_OFFSET 70
#define DUPLICATE_VERTICAL_ICON_OFFSET   30

#define NAUTILUS_COMMAND_SPECIFIER "command:"

/* Paths to use when referring to bonobo menu items. */
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW        		"/menu/File/Open Placeholder/OpenNew"
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH				"/menu/File/Open Placeholder/Open With"
#define FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS				"/menu/File/Open Placeholder/Scripts"
#define FM_DIRECTORY_VIEW_MENU_PATH_TRASH                    		"/menu/File/File Items Placeholder/Trash"
#define FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH                    	"/menu/File/Global File Items Placeholder/Empty Trash"
#define FM_DIRECTORY_VIEW_MENU_PATH_CREATE_LINK                	 	"/menu/File/File Items Placeholder/Create Link"
#define FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS			"/menu/Edit/Edit Items Placeholder/Remove Custom Icons"
#define FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER    	"/menu/File/Open Placeholder/Open With/Applications Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_VIEWERS_PLACEHOLDER    		"/menu/File/Open Placeholder/Open With/Viewers Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER    		"/menu/File/Open Placeholder/Scripts/Scripts Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_SEPARATOR    		"/menu/File/Open Placeholder/Scripts/After Scripts"

#define FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND				"/popups/background"
#define FM_DIRECTORY_VIEW_POPUP_PATH_SELECTION				"/popups/selection"

#define FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER    	"/popups/selection/Open Placeholder/Open With/Applications Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_VIEWERS_PLACEHOLDER    		"/popups/selection/Open Placeholder/Open With/Viewers Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER    		"/popups/selection/Open Placeholder/Scripts/Scripts Placeholder"
#define FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_SEPARATOR    		"/popups/selection/Open Placeholder/Scripts/After Scripts"
#define FM_DIRECTORY_VIEW_POPUP_PATH_OPEN_WITH				"/popups/selection/Open Placeholder/Open With"
#define FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS				"/popups/selection/Open Placeholder/Scripts"

enum {
	ADD_FILE,
	BEGIN_ADDING_FILES,
	BEGIN_LOADING,
	END_LOADING,
	LOAD_ERROR,
	CLEAR,
	DONE_ADDING_FILES,
	FILE_CHANGED,
	MOVE_COPY_ITEMS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct FMDirectoryViewDetails
{
	NautilusView *nautilus_view;
	BonoboZoomable *zoomable;

	NautilusDirectory *model;
	NautilusFile *directory_as_file;
	BonoboUIComponent *ui;

	NautilusDirectory *scripts_directory;
	guint scripts_added_handler_id;
	guint scripts_changed_handler_id;

	guint display_selection_idle_id;
	guint update_menus_timeout_id;
	
	guint display_pending_timeout_id;
	guint display_pending_idle_id;
	
	guint files_added_handler_id;
	guint files_changed_handler_id;
	guint load_error_handler_id;
	guint done_loading_handler_id;
	guint file_changed_handler_id;
	
	GList *pending_files_added;
	GList *pending_files_changed;
	GList *pending_uris_selected;

	gboolean loading;
	gboolean menus_merged;
	gboolean menu_states_untrustworthy;
	gboolean scripts_invalid;
	gboolean reported_load_error;

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
	NautilusDirectory *directory_monitored_for_activation;
};

typedef enum {
	RESPECT_PREFERENCE,
	PREFER_EXISTING_WINDOW,
	FORCE_NEW_WINDOW
} WindowChoice;

typedef enum {
	ACTIVATION_ACTION_LAUNCH,
	ACTIVATION_ACTION_DISPLAY,
	ACTIVATION_ACTION_DO_NOTHING
} ActivationAction;

typedef struct {
	FMDirectoryView *view;
	NautilusFile *file;
	WindowChoice choice;
} ActivateParameters;

/* forward declarations */

static void		   cancel_activate_callback 				  (gpointer 	         callback_data);
static gboolean            display_selection_info_idle_callback                   (gpointer              data);
static gboolean            file_is_launchable                                     (NautilusFile         *file);
static void                fm_directory_view_initialize_class                     (FMDirectoryViewClass *klass);
static void                fm_directory_view_initialize                           (FMDirectoryView      *view);
static void                fm_directory_view_duplicate_selection                  (FMDirectoryView      *view,
										   GList                *files,
										   GArray		*item_locations);
static void                fm_directory_view_create_links_for_files               (FMDirectoryView      *view,
										   GList                *files,
										   GArray		*item_locations);
static void                fm_directory_view_trash_or_delete_files                (const GList		*files,
										   FMDirectoryView 	*view);
static void                fm_directory_view_destroy                              (GtkObject            *object);
static void                fm_directory_view_activate_file                        (FMDirectoryView      *view,
										   NautilusFile         *file,
										   WindowChoice          choice);
static void                load_directory                                         (FMDirectoryView      *view,
										   NautilusDirectory    *directory);
static void                fm_directory_view_merge_menus                          (FMDirectoryView      *view);
static void                real_file_limit_reached                                (FMDirectoryView      *view);
static gboolean            real_display_pending_files                             (FMDirectoryView      *view,
										   GList                **pending_files_added,
										   GList                **pending_files_changed);
static void		   real_load_error					  (FMDirectoryView 	*view,
										   GnomeVFSResult	 result);
static void                real_merge_menus                                       (FMDirectoryView      *view);
static void                real_update_menus                                      (FMDirectoryView      *view);
static gboolean            real_is_read_only                                      (FMDirectoryView      *view);
static gboolean            real_supports_creating_files                           (FMDirectoryView      *view);
static gboolean            real_accepts_dragged_files                             (FMDirectoryView      *view);
static gboolean            real_supports_zooming                                  (FMDirectoryView      *view);
static gboolean            real_supports_properties                               (FMDirectoryView      *view);
static BonoboControl *     get_bonobo_control                                     (FMDirectoryView      *view);
static void                stop_loading_callback                                  (NautilusView         *nautilus_view,
										   FMDirectoryView      *directory_view);
static void                load_location_callback                                 (NautilusView         *nautilus_view,
										   const char           *location,
										   FMDirectoryView      *directory_view);
static void                selection_changed_callback                             (NautilusView         *nautilus_view,
										   GList                *selection,
										   FMDirectoryView      *directory_view);
static void                open_one_in_new_window                                 (gpointer              data,
										   gpointer              callback_data);
static void                open_one_properties_window                             (gpointer              data,
										   gpointer              callback_data);
static void                zoomable_set_zoom_level_callback                       (BonoboZoomable       *zoomable,
										   float                 level,
										   FMDirectoryView      *view);
static void                zoomable_zoom_in_callback                              (BonoboZoomable       *zoomable,
										   FMDirectoryView      *directory_view);
static void                zoomable_zoom_out_callback                             (BonoboZoomable       *zoomable,
										   FMDirectoryView      *directory_view);
static void                zoomable_zoom_to_fit_callback                          (BonoboZoomable       *zoomable,
										   FMDirectoryView      *directory_view);
static void                schedule_update_menus                                  (FMDirectoryView      *view);
static void                schedule_update_menus_callback                         (gpointer              callback_data);
static void		   remove_update_menus_timeout_callback 		  (FMDirectoryView      *view);
static void                schedule_idle_display_of_pending_files                 (FMDirectoryView      *view);
static void                unschedule_idle_display_of_pending_files               (FMDirectoryView      *view);
static void                schedule_timeout_display_of_pending_files              (FMDirectoryView      *view);
static void                unschedule_timeout_display_of_pending_files            (FMDirectoryView      *view);
static void                unschedule_display_of_pending_files                    (FMDirectoryView      *view);
static void                disconnect_model_handlers                              (FMDirectoryView      *view);
static void                disconnect_script_handlers                             (FMDirectoryView      *view);
static void                filtering_changed_callback                             (gpointer              callback_data);
static NautilusStringList *real_get_emblem_names_to_exclude                       (FMDirectoryView      *view);
static void                start_renaming_item                                    (FMDirectoryView      *view,
										   const char           *uri);
static void                metadata_for_directory_as_file_ready_callback          (NautilusFile         *file,
										   gpointer              callback_data);
static void                metadata_for_files_in_directory_ready_callback         (NautilusDirectory    *directory,
										   GList		*files,
										   gpointer              callback_data);
static void                fm_directory_view_trash_state_changed_callback         (NautilusTrashMonitor *trash,
										   gboolean              state,
										   gpointer              callback_data);
static void                fm_directory_view_select_file                          (FMDirectoryView      *view,
										   NautilusFile         *file);
static void                monitor_file_for_open_with                             (FMDirectoryView      *view,
										   NautilusFile         *file);
static void                monitor_file_for_activation                            (FMDirectoryView      *view,
										   NautilusFile         *file);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_file)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, bump_zoom_level)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, zoom_to_level)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, restore_default_zoom_level)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_in)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_out)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_background_widget)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, file_changed)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, is_empty)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, select_all)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, set_selection)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selected_icon_locations)

static void
fm_directory_view_initialize_class (FMDirectoryViewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = fm_directory_view_destroy;

	signals[CLEAR] =
		gtk_signal_new ("clear",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, clear),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	signals[BEGIN_ADDING_FILES] =
		gtk_signal_new ("begin_adding_files",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, begin_adding_files),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	signals[ADD_FILE] =
		gtk_signal_new ("add_file",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, add_file),
		    		gtk_marshal_NONE__BOXED,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
	signals[FILE_CHANGED] =
		gtk_signal_new ("file_changed",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, file_changed),
		    		gtk_marshal_NONE__BOXED,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);
	signals[DONE_ADDING_FILES] =
		gtk_signal_new ("done_adding_files",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, done_adding_files),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	signals[BEGIN_LOADING] =
		gtk_signal_new ("begin_loading",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, begin_loading),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	signals[END_LOADING] =
		gtk_signal_new ("end_loading",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, end_loading),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	signals[LOAD_ERROR] =
		gtk_signal_new ("load_error",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FMDirectoryViewClass, load_error),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

        klass->merge_menus = real_merge_menus;
        klass->update_menus = real_update_menus;
	klass->display_pending_files = real_display_pending_files;
	klass->get_emblem_names_to_exclude = real_get_emblem_names_to_exclude;
	klass->start_renaming_item = start_renaming_item;
	klass->is_read_only = real_is_read_only;
	klass->supports_creating_files = real_supports_creating_files;
	klass->accepts_dragged_files = real_accepts_dragged_files;
	klass->supports_zooming = real_supports_zooming;
	klass->supports_properties = real_supports_properties;
	klass->file_limit_reached = real_file_limit_reached;
	klass->load_error = real_load_error;
	klass->reveal_selection = NULL;

	/* Function pointers that subclasses must override */
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, add_file);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, bump_zoom_level);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, zoom_to_level);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, restore_default_zoom_level);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_in);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_out);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_background_widget);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, clear);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, file_changed);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selection);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, is_empty);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, select_all);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, set_selection);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selected_icon_locations);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

}

typedef struct {
	GnomeVFSMimeApplication *application;
	NautilusFile *file;
	FMDirectoryView *directory_view;
} ApplicationLaunchParameters;

typedef struct {
	NautilusViewIdentifier *identifier;
	char *uri;
	FMDirectoryView *directory_view;
} ViewerLaunchParameters;

typedef struct {
	NautilusFile *file;
	FMDirectoryView *directory_view;
} ScriptLaunchParameters;

static ApplicationLaunchParameters *
application_launch_parameters_new (GnomeVFSMimeApplication *application,
			      	   NautilusFile *file,
			           FMDirectoryView *directory_view)
{
	ApplicationLaunchParameters *result;

	result = g_new0 (ApplicationLaunchParameters, 1);
	result->application = gnome_vfs_mime_application_copy (application);
	gtk_widget_ref (GTK_WIDGET (directory_view));
	result->directory_view = directory_view;
	nautilus_file_ref (file);
	result->file = file;

	return result;
}

static void
application_launch_parameters_free (ApplicationLaunchParameters *parameters)
{
	gnome_vfs_mime_application_free (parameters->application);
	gtk_widget_unref (GTK_WIDGET (parameters->directory_view));
	nautilus_file_unref (parameters->file);
	g_free (parameters);
}			      

static ViewerLaunchParameters *
viewer_launch_parameters_new (NautilusViewIdentifier *identifier,
			      const char *uri,
			      FMDirectoryView *directory_view)
{
	ViewerLaunchParameters *result;

	result = g_new0 (ViewerLaunchParameters, 1);
	result->identifier = nautilus_view_identifier_copy (identifier);
	gtk_widget_ref (GTK_WIDGET (directory_view));
	result->directory_view = directory_view;
	result->uri = g_strdup (uri);

	return result;
}

static void
viewer_launch_parameters_free (ViewerLaunchParameters *parameters)
{
	nautilus_view_identifier_free (parameters->identifier);
	gtk_widget_unref (GTK_WIDGET (parameters->directory_view));
	g_free (parameters->uri);
	g_free (parameters);
}			      

static ScriptLaunchParameters *
script_launch_parameters_new (NautilusFile *file,
			      FMDirectoryView *directory_view)
{
	ScriptLaunchParameters *result;

	result = g_new0 (ScriptLaunchParameters, 1);
	gtk_widget_ref (GTK_WIDGET (directory_view));
	result->directory_view = directory_view;
	nautilus_file_ref (file);
	result->file = file;

	return result;
}

static void
script_launch_parameters_free (ScriptLaunchParameters *parameters)
{
	gtk_widget_unref (GTK_WIDGET (parameters->directory_view));
	nautilus_file_unref (parameters->file);
	g_free (parameters);
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
	GnomeDialog *dialog;
	char *prompt;
	char *title;

	if (count <= SILENT_WINDOW_OPEN_LIMIT) {
		return TRUE;
	}

	prompt = g_strdup_printf (_("This will open %d separate windows. "
				    "Are you sure you want to do this?"), count);
	title = g_strdup_printf (_("Open %d Windows?"), count);
	dialog = nautilus_show_yes_no_dialog (prompt, title, 
					      GNOME_STOCK_BUTTON_OK, 
					      GNOME_STOCK_BUTTON_CANCEL, 
					      fm_directory_view_get_containing_window (view));
	g_free (prompt);
	g_free (title);

	return gnome_dialog_run (dialog) == GNOME_OK;
}

static gboolean
selection_contains_one_item_in_menu_callback (FMDirectoryView *view, GList *selection)
{
	if (nautilus_g_list_exactly_one_item (selection)) {
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
open_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless exactly
         * one item is selected.
         */
        if (selection_contains_one_item_in_menu_callback (view, selection)) {
		fm_directory_view_activate_file (view, 
		                                 NAUTILUS_FILE (selection->data), 
		                                 RESPECT_PREFERENCE);        
        }        

	nautilus_file_list_free (selection);
}

static void
open_in_new_window_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

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
	g_assert (application != NULL);
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));

	nautilus_launch_application
		(application, file, fm_directory_view_get_containing_window (directory_view));
	
}				      

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

static void
open_location (FMDirectoryView *directory_view, 
	       const char *new_uri, 
	       WindowChoice choice)
{
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));
	g_assert (new_uri != NULL);

	switch (choice) {
	case RESPECT_PREFERENCE:
		if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW)) {
			nautilus_view_open_location_prefer_existing_window
				(directory_view->details->nautilus_view, new_uri);
		} else {
			nautilus_view_open_location_in_this_window
				(directory_view->details->nautilus_view, new_uri);
		}
		break;
	case PREFER_EXISTING_WINDOW:
		nautilus_view_open_location_prefer_existing_window
			(directory_view->details->nautilus_view, new_uri);
		break;
	case FORCE_NEW_WINDOW:
		nautilus_view_open_location_force_new_window
			(directory_view->details->nautilus_view, new_uri, NULL);
		break;
	}
}

static void
switch_location_and_view (NautilusViewIdentifier *identifier, 
			  const char *new_uri, 
			  FMDirectoryView *directory_view)
{
	NautilusFile *file;

	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));
	g_assert (identifier != NULL);
	g_assert (new_uri != NULL);

	/* User has explicitly chosen a viewer other than the default, so
	 * make it the default and then switch locations.
	 */
	/* FIXME bugzilla.eazel.com 1053: We might want an atomic operation
	 * for switching location and viewer together, so we don't have to
	 * rely on metadata for holding the default location.
	 */
	file = nautilus_file_get (new_uri);
	nautilus_mime_set_default_component_for_file (file, identifier->iid);
	nautilus_file_unref (file);

	open_location (directory_view, new_uri, RESPECT_PREFERENCE);
}

static void
fm_directory_view_chose_component_callback (NautilusViewIdentifier *identifier, 
					    gpointer callback_data)
{
	ViewerLaunchParameters *launch_parameters;

	g_assert (callback_data != NULL);

	launch_parameters = (ViewerLaunchParameters *)callback_data;
	g_assert (launch_parameters->identifier == NULL);

	if (identifier != NULL) {
		switch_location_and_view (identifier, /* NOT the (empty) identifier in launch_parameters */
					  launch_parameters->uri, 
					  launch_parameters->directory_view);
	}

	viewer_launch_parameters_free (launch_parameters);
}

static void
choose_program (FMDirectoryView *view,
		NautilusFile *file,
		GnomeVFSMimeActionType type)
{
	char *uri;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT ||
		  type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);

	nautilus_file_ref (file);
	uri = nautilus_file_get_uri (file);

	if (type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		nautilus_choose_component_for_file 
			(file,
			 fm_directory_view_get_containing_window (view),
			 fm_directory_view_chose_component_callback,
			 viewer_launch_parameters_new
			 	(NULL, uri, view));
	} else {
		nautilus_choose_application_for_file 
			(file,
			 fm_directory_view_get_containing_window (view),
			 fm_directory_view_chose_application_callback,
			 application_launch_parameters_new
			 	(NULL, file, view));
	}

	g_free (uri);
	nautilus_file_unref (file);	
}

static void
open_with_other_program (FMDirectoryView *view, GnomeVFSMimeActionType action_type)
{
        GList *selection;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION ||
		  action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);

       	selection = fm_directory_view_get_selection (view);

	if (selection_contains_one_item_in_menu_callback (view, selection)) {
		choose_program (view, NAUTILUS_FILE (selection->data), action_type);
	}

	nautilus_file_list_free (selection);
}

static void
other_application_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	open_with_other_program (FM_DIRECTORY_VIEW (callback_data), 
				 GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
}

static void
other_viewer_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	open_with_other_program (FM_DIRECTORY_VIEW (callback_data), 
				 GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
}

static void
trash_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
        FMDirectoryView *view;
        GList *selection;
        
        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
	        fm_directory_view_trash_or_delete_files (selection, view);					 
	}

        nautilus_file_list_free (selection);
}

static void
duplicate_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
        FMDirectoryView *view;
        GList *selection;
        GArray *selected_item_locations;
 
        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		/* FIXME bugzilla.eazel.com 5061:
		 * should change things here so that we use a get_icon_locations (view, selection).
		 * Not a problem in this case but in other places the selection may change by
		 * the time we go and retrieve the icon positions, relying on the selection
		 * staying intact to ensure the right sequence and count of positions is fragile.
		 */
		selected_item_locations = fm_directory_get_selected_icon_locations (view);
	        fm_directory_view_duplicate_selection (view, selection, selected_item_locations);
	        g_array_free (selected_item_locations, TRUE);
	}

        nautilus_file_list_free (selection);
}

static void
create_link_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
        FMDirectoryView *view;
        GList *selection;
        GArray *selected_item_locations;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		selected_item_locations = fm_directory_get_selected_icon_locations (view);
	        fm_directory_view_create_links_for_files (view, selection, selected_item_locations);
	        g_array_free (selected_item_locations, TRUE);
	}

        nautilus_file_list_free (selection);
}

static void
bonobo_menu_select_all_callback (BonoboUIComponent *component, 
				 gpointer callback_data, 
				 const char *verb)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_select_all (callback_data);
}

static void
show_trash_callback (BonoboUIComponent *component, 
		     gpointer callback_data, 
		     const char *verb)
{      
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);          

	open_location (view, NAUTILUS_TRASH_URI, RESPECT_PREFERENCE);
}

static void
bonobo_menu_empty_trash_callback (BonoboUIComponent *component, 
				  gpointer callback_data, 
				  const char *verb)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	nautilus_file_operations_empty_trash (GTK_WIDGET (FM_DIRECTORY_VIEW (callback_data)));
}

static void
new_folder_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_new_folder (FM_DIRECTORY_VIEW (callback_data));
}

static void
open_properties_window_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		if (fm_directory_view_confirm_multiple_windows (view, g_list_length (selection))) {
			g_list_foreach (selection, open_one_properties_window, view);
		}
	}

        nautilus_file_list_free (selection);
}

static BonoboControl *
get_bonobo_control (FMDirectoryView *view)
{
        return BONOBO_CONTROL (nautilus_view_get_bonobo_control
			       (view->details->nautilus_view));
}

static void
bonobo_control_activate_callback (BonoboObject *control, gboolean state, gpointer callback_data)
{
        FMDirectoryView *view;

        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);

        if (state) {
                /* Add new menu items and perhaps whole menus */
                fm_directory_view_merge_menus (view);

	        /* Set initial sensitivity, wording, toggle state, etc. */       
                fm_directory_view_update_menus (view);
        }

        /* 
         * Nothing to do on deactivate case, which never happens because
         * of the way Nautilus content views are handled.
         */
}

/* Preferences changed callbacks */
static void
text_attribute_names_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 text_attribute_names_changed, (view));
}

static void
image_display_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 image_display_policy_changed, (view));
}

static void
directory_view_font_family_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 font_family_changed, (view));
}

static void
directory_view_smooth_font_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 smooth_font_changed, (view));
}

static void
click_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 click_policy_changed, (view));
}

static void
smooth_graphics_mode_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 smooth_graphics_mode_changed, (view));
}

static float fm_directory_view_preferred_zoom_levels[] = {
	(float) NAUTILUS_ICON_SIZE_SMALLEST	/ NAUTILUS_ICON_SIZE_STANDARD,
	(float) NAUTILUS_ICON_SIZE_SMALLER	/ NAUTILUS_ICON_SIZE_STANDARD,
	(float) NAUTILUS_ICON_SIZE_SMALL	/ NAUTILUS_ICON_SIZE_STANDARD,
	(float) NAUTILUS_ICON_SIZE_STANDARD	/ NAUTILUS_ICON_SIZE_STANDARD,
	(float) NAUTILUS_ICON_SIZE_LARGE	/ NAUTILUS_ICON_SIZE_STANDARD,
	(float) NAUTILUS_ICON_SIZE_LARGER	/ NAUTILUS_ICON_SIZE_STANDARD,
	(float) NAUTILUS_ICON_SIZE_LARGEST	/ NAUTILUS_ICON_SIZE_STANDARD
};

static NautilusDirectory *
get_scripts_directory (void)
{
	char *main_directory_path;
	char *scripts_directory_path;
	NautilusDirectory *directory;
	
	main_directory_path = nautilus_get_user_main_directory ();
	/* Localizers: This is the name of a directory that's created in ~/Nautilus, and
	 * stores the scripts that appear in the Scripts submenu.
	 */
	scripts_directory_path = nautilus_make_path (main_directory_path, _("scripts"));
	g_free (main_directory_path);

	if (!g_file_exists (scripts_directory_path)) {
		mkdir (scripts_directory_path, 
		       GNOME_VFS_PERM_USER_ALL | GNOME_VFS_PERM_GROUP_ALL | GNOME_VFS_PERM_OTHER_READ);
	}

	if (!g_file_exists (scripts_directory_path)) {
		return NULL;
	}
	
	directory = nautilus_directory_get (scripts_directory_path);

	g_free (scripts_directory_path);
	
	return directory;
}

static void
scripts_added_or_changed_callback (NautilusDirectory *directory,
		        GList *files,
		        gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	g_assert (directory == view->details->scripts_directory);

	view->details->scripts_invalid = TRUE;
	schedule_update_menus (view);
}

static void
connect_script_handlers (FMDirectoryView *view)
{
	if (view->details->scripts_directory == NULL) {
		return;
	}

	nautilus_directory_file_monitor_add (view->details->scripts_directory,
					     &view->details->scripts_directory,
					     FALSE, FALSE, NULL);

    	view->details->scripts_added_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->scripts_directory),
		 "files_added",
		 scripts_added_or_changed_callback,
		 view);
	view->details->scripts_changed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->scripts_directory), 
		 "files_changed",
		 scripts_added_or_changed_callback,
		 view);
}



static void
fm_directory_view_initialize (FMDirectoryView *view)
{
	view->details = g_new0 (FMDirectoryViewDetails, 1);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));

	view->details->scripts_directory = get_scripts_directory ();
	connect_script_handlers (view);

	view->details->zoomable = bonobo_zoomable_new ();
	bonobo_zoomable_set_parameters_full (view->details->zoomable,
					     0.0, .25, 4.0, TRUE, TRUE, FALSE,
					     fm_directory_view_preferred_zoom_levels, NULL,
					     NAUTILUS_N_ELEMENTS (fm_directory_view_preferred_zoom_levels));
	bonobo_object_add_interface (BONOBO_OBJECT (view->details->nautilus_view),
				     BONOBO_OBJECT (view->details->zoomable));

	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "stop_loading",
			    GTK_SIGNAL_FUNC (stop_loading_callback),
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (load_location_callback), 
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "selection_changed",
			    GTK_SIGNAL_FUNC (selection_changed_callback), 
			    view);

        gtk_signal_connect (GTK_OBJECT (get_bonobo_control (view)),
                            "activate",
                            bonobo_control_activate_callback,
                            view);

	gtk_signal_connect (GTK_OBJECT (view->details->zoomable), 
			    "zoom_in",
			    zoomable_zoom_in_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->zoomable), 
			    "zoom_out", 
			    zoomable_zoom_out_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->zoomable), 
			    "set_zoom_level", 
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_callback),
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->zoomable), 
			    "zoom_to_fit", 
			    zoomable_zoom_to_fit_callback,
			    view);
	gtk_signal_connect_while_alive (GTK_OBJECT (nautilus_trash_monitor_get ()),
				        "trash_state_changed",
				        fm_directory_view_trash_state_changed_callback,
				        view,
				        GTK_OBJECT (view));

	gtk_widget_show (GTK_WIDGET (view));

	filtering_changed_callback (view);
	
	/* Keep track of changes in this pref to filter files accordingly. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					   filtering_changed_callback,
					   view);
	
	/* Keep track of changes in this pref to filter files accordingly. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					   filtering_changed_callback,
					   view);
	
	/* Keep track of changes in this pref to display menu names correctly. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
					   schedule_update_menus_callback,
					   view);
	
	/* Keep track of changes in text attribute names */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_CAPTIONS,
					   text_attribute_names_changed_callback,
					   view);

	/* Keep track of changes in image display policy */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					   image_display_policy_changed_callback,
					   view);

	/* Keep track of changes in the font family */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
					   directory_view_font_family_changed_callback, 
					   view);

	/* Keep track of changes in the smooth font */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_SMOOTH_FONT,
					   directory_view_smooth_font_changed_callback, 
					   view);

	/* Keep track of changes in clicking policy */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					   click_policy_changed_callback,
					   view);
	
	/* Keep track of changes in graphics trade offs */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
					   smooth_graphics_mode_changed_callback, 
					   view);
}

static void
fm_directory_view_destroy (GtkObject *object)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (object);

	/* Since we are owned by the NautilusView, if we're going it's
	 * gone. It would be even better to NULL this out when the
	 * NautilusView goes away, but this is good enough for our
	 * purposes.
	 */
	view->details->nautilus_view = NULL;

	monitor_file_for_activation (view, NULL);

	monitor_file_for_open_with (view, NULL);

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	disconnect_script_handlers (view);
	nautilus_directory_unref (view->details->scripts_directory);

	disconnect_model_handlers (view);
	nautilus_directory_unref (view->details->model);
	view->details->model = NULL;
	nautilus_file_unref (view->details->directory_as_file);

	if (view->details->display_selection_idle_id != 0) {
		gtk_idle_remove (view->details->display_selection_idle_id);
	}

	remove_update_menus_timeout_callback (view);

	fm_directory_view_ignore_hidden_file_preferences (view);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
					      schedule_update_menus_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_ICON_CAPTIONS,
					      text_attribute_names_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					      image_display_policy_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
					      directory_view_font_family_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_SMOOTH_FONT,
					      directory_view_smooth_font_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					      click_policy_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
					      smooth_graphics_mode_changed_callback,
					      view);

	g_free (view->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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
	guint item_count;
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
			if (nautilus_file_get_directory_item_count (file, &item_count, NULL)) {
				folder_item_count += item_count;
			} else {
				folder_item_count_known = FALSE;
			}
		} else {
			non_folder_count++;
			non_folder_size += nautilus_file_get_size (file);
		}

		if (first_item_name == NULL) {
			first_item_name = nautilus_file_get_name (file);
		}
	}
	
	nautilus_file_list_free (selection);
	
	/* Break out cases for localization's sake. But note that there are still pieces
	 * being assembled in a particular order, which may be a problem for some localizers.
	 */

	if (folder_count != 0) {
		if (folder_count == 1) {
			if (non_folder_count == 0) {
				folder_count_str = g_strdup_printf (_("\"%s\" selected"), first_item_name);
			} else {
				folder_count_str = g_strdup (_("1 folder selected"));
			}
		} else {
			folder_count_str = g_strdup_printf (_("%d folders selected"), folder_count);
		}

		if (!folder_item_count_known) {
			folder_item_count_str = g_strdup ("");
		} else if (folder_item_count == 0) {
			folder_item_count_str = g_strdup (_(" (containing 0 items)"));
		} else if (folder_item_count == 1) {
			folder_item_count_str = g_strdup (_(" (containing 1 item)"));
		} else {
			folder_item_count_str = g_strdup_printf (_(" (containing %d items)"), folder_item_count);
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
				non_folder_str = g_strdup_printf (_("%d items selected (%s)"), 
								  non_folder_count, 
								  size_string);
			}
		} else {
			/* Folders selected also, use "other" terminology */
			if (non_folder_count == 1) {
				non_folder_str = g_strdup_printf (_("1 other item selected (%s)"), 
								  size_string);
			} else {
				non_folder_str = g_strdup_printf (_("%d other items selected (%s)"), 
								  non_folder_count, 
								  size_string);
			}
		}

		g_free (size_string);
	}

	if (folder_count == 0 && non_folder_count == 0)	{
		status_string = g_strdup ("");
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

	nautilus_view_report_status (view->details->nautilus_view,
				     status_string);
	g_free (status_string);
}

void
fm_directory_view_send_selection_change (FMDirectoryView *view)
{
	GList *selection, *uris, *p;

	/* Collect a list of URIs. */
	selection = fm_directory_view_get_selection (view);
	uris = NULL;
	for (p = selection; p != NULL; p = p->next) {
		uris = g_list_prepend (uris, nautilus_file_get_uri (p->data));
	}
	nautilus_file_list_free (selection);

	/* Send the selection change. */
	nautilus_view_report_selection_change (view->details->nautilus_view,
					       uris);

	/* Free the URIs. */
	nautilus_g_list_free_deep (uris);

	view->details->send_selection_change_to_shell = FALSE;
}

static void
load_location_callback (NautilusView *nautilus_view,
			const char *location,
			FMDirectoryView *directory_view)
{
	NautilusDirectory *directory;

	directory = nautilus_directory_get (location);
	load_directory (directory_view, directory);
	nautilus_directory_unref (directory);
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
selection_changed_callback (NautilusView *nautilus_view,
			    GList *selection_uris,
			    FMDirectoryView *view)
{
	GList *selection;

	if (view->details->loading) {
		nautilus_g_list_free_deep (view->details->pending_uris_selected);
		view->details->pending_uris_selected = NULL;
	}

	if (!view->details->loading) {
		/* If we aren't still loading, set the selection right now. */
		selection = file_list_from_uri_list (selection_uris);
		view->details->selection_change_is_due_to_shell = TRUE;
		fm_directory_view_set_selection (view, selection);
		view->details->selection_change_is_due_to_shell = FALSE;
		nautilus_file_list_free (selection);
	} else {
		/* If we are still loading, add to the list of pending URIs instead. */
		view->details->pending_uris_selected =
			g_list_concat (view->details->pending_uris_selected,
				       nautilus_g_str_list_copy (selection_uris));
	}
}

static void
stop_loading_callback (NautilusView *nautilus_view,
		       FMDirectoryView *view)
{
	fm_directory_view_stop (view);
}

static void
fm_directory_view_file_limit_reached (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	NAUTILUS_CALL_METHOD (FM_DIRECTORY_VIEW_CLASS, view,
		 	       file_limit_reached, (view));
}

static void
real_file_limit_reached (FMDirectoryView *view)
{
	NautilusFile *file;
	GnomeDialog *dialog;
	char *directory_name;
	char *message;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	file = fm_directory_view_get_directory_as_file (view);
	directory_name = nautilus_file_get_name (file);
	nautilus_file_unref (file);

	/* Note that the number of items actually displayed varies somewhat due
	 * to the way files are collected in batches. So you can't assume that
	 * no more than the constant limit are displayed.
	 */
	message = g_strdup_printf (_("The folder \"%s\" contains more files than "
			             "Nautilus can handle. Some files will not be "
			             "displayed."), 
			           directory_name);
	g_free (directory_name);

	dialog = nautilus_show_warning_dialog (message,
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
	 * is no NautilusView any more.
	 */
	if (view->details->nautilus_view != NULL) {
		nautilus_view_report_load_complete (view->details->nautilus_view);
		schedule_update_menus (view);
		check_for_directory_hard_limit (view);

		uris_selected = view->details->pending_uris_selected;
		if (uris_selected != NULL) {
			view->details->pending_uris_selected = NULL;
			
			selection = file_list_from_uri_list (uris_selected);
			nautilus_g_list_free_deep (uris_selected);
			
			view->details->selection_change_is_due_to_shell = TRUE;
			fm_directory_view_set_selection (view, selection);
			view->details->selection_change_is_due_to_shell = FALSE;
			fm_directory_view_reveal_selection (view);
			
			nautilus_file_list_free (selection);
		}
	}

	fm_directory_view_end_loading (view);

	view->details->loading = FALSE;
}

static void
reset_background_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	nautilus_background_reset 
		(fm_directory_view_get_background 
			(FM_DIRECTORY_VIEW (callback_data)));
}

static void
zoomable_zoom_in_callback (BonoboZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoomable_zoom_out_callback (BonoboZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);
}

static NautilusZoomLevel
nautilus_zoom_level_from_float(float zoom_level)
{
	int icon_size = floor(zoom_level * NAUTILUS_ICON_SIZE_STANDARD + 0.5);
	
	if (icon_size <= NAUTILUS_ICON_SIZE_SMALLEST) {
		return NAUTILUS_ZOOM_LEVEL_SMALLEST;
	} else if (icon_size <= NAUTILUS_ICON_SIZE_SMALLER) {
		return NAUTILUS_ZOOM_LEVEL_SMALLER;
	} else if (icon_size <= NAUTILUS_ICON_SIZE_SMALL) {
		return NAUTILUS_ZOOM_LEVEL_SMALL;
	} else if (icon_size <= NAUTILUS_ICON_SIZE_STANDARD) {
		return NAUTILUS_ZOOM_LEVEL_STANDARD;
	} else if (icon_size <= NAUTILUS_ICON_SIZE_LARGE) {
		return NAUTILUS_ZOOM_LEVEL_LARGE;
	} else if (icon_size <= NAUTILUS_ICON_SIZE_LARGER) {
		return NAUTILUS_ZOOM_LEVEL_LARGER;
	} else {
		return NAUTILUS_ZOOM_LEVEL_LARGEST;
	}
}

static void
zoomable_set_zoom_level_callback (BonoboZoomable *zoomable, float level, FMDirectoryView *view)
{
	fm_directory_view_zoom_to_level (view, nautilus_zoom_level_from_float (level));
}

static void
zoomable_zoom_to_fit_callback (BonoboZoomable *zoomable, FMDirectoryView *view)
{
	/* FIXME bugzilla.eazel.com 2388:
	 * Need to really implement "zoom to fit"
	 */
	fm_directory_view_restore_default_zoom_level (view);
}

typedef struct {
	GHashTable *debuting_uris;
	GList	   *added_files;
} DebutingUriData;

static void
debuting_uri_data_free (DebutingUriData *data)
{
	if (data != NULL) {
		nautilus_g_hash_table_destroy_deep (data->debuting_uris);
		nautilus_g_list_free_deep_custom (data->added_files, (GFunc) gtk_object_unref, NULL);
		g_free (data);
	}
}
 
/* This signal handler watch for the arrival of the icons created
 * as the result of a file operation. Once the last one is detected
 * it selects and reveals them all.
 */
static void
debuting_uri_add_file_callback (FMDirectoryView *view, NautilusFile *new_file, DebutingUriData *data)
{
	char *uri;

	uri = nautilus_file_get_uri (new_file);

	if (nautilus_g_hash_table_remove_deep (data->debuting_uris, uri)) {
		gtk_object_ref (GTK_OBJECT (new_file));
		data->added_files = g_list_prepend (data->added_files, new_file);

		if (g_hash_table_size (data->debuting_uris) == 0) {
			fm_directory_view_set_selection (view, data->added_files);
			fm_directory_view_reveal_selection (view);
			gtk_signal_disconnect_by_func (GTK_OBJECT (view), &debuting_uri_add_file_callback, data);
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
	
	nautilus_nullify_cancel (&data->directory_view);
	nautilus_g_list_free_deep_custom (data->added_files, (GFunc) gtk_object_unref, NULL);
	g_free (data);
}

static void
pre_copy_move_add_file_callback (FMDirectoryView *view, NautilusFile *new_file, CopyMoveDoneData *data)
{
	gtk_object_ref (GTK_OBJECT (new_file));
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

	nautilus_nullify_when_destroyed (&copy_move_done_data->directory_view);

	/* We need to run after the default handler adds the folder we want to
	 * operate on. The ADD_FILE signal is registered as GTK_RUN_LAST, so we
	 * must use connect_after.
	 */
	gtk_signal_connect (GTK_OBJECT (directory_view),
			    "add_file",
			    pre_copy_move_add_file_callback,
			    copy_move_done_data);

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

	result = nautilus_g_hash_table_remove_deep ((GHashTable *) callback_data, uri);
	
	g_free (uri);

	return result;
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
		debuting_uri_data->added_files	 = nautilus_g_list_partition (copy_move_done_data->added_files,
									      copy_move_done_partition_func,
									      debuting_uris,
									      &copy_move_done_data->added_files);
									      
		/* We're passed the same data used by pre_copy_move_add_file_callback, so disconnecting
		 * it will free data. We've already siphoned off the added_files we need, and stashed the
		 * directory_view pointer.
		 */
		gtk_signal_disconnect_by_func (GTK_OBJECT (directory_view), &pre_copy_move_add_file_callback, data);
	
		if (g_hash_table_size (debuting_uris) == 0) {
			/* on the off-chance that all the icons have already been added ...
			 */
			if (debuting_uri_data->added_files != NULL) {
				fm_directory_view_set_selection (directory_view,
								 debuting_uri_data->added_files);
				fm_directory_view_reveal_selection (directory_view);
			}
			debuting_uri_data_free (debuting_uri_data);
		} else {
			/* We need to run after the default handler adds the folder we want to
			 * operate on. The ADD_FILE signal is registered as GTK_RUN_LAST, so we
			 * must use connect_after.
			 */
			gtk_signal_connect_full (GTK_OBJECT (directory_view),
						 "add_file",
						 debuting_uri_add_file_callback,
						 NULL,
						 debuting_uri_data,
						 (GtkDestroyNotify) debuting_uri_data_free,
						 FALSE,
						 TRUE);
		}
	}

	copy_move_done_data_free (copy_move_done_data);
}


static void
finish_displaying_pending_files (FMDirectoryView *view)
{
	if (view->details->model != NULL
		    && nautilus_directory_are_all_files_seen (view->details->model)) {
		done_loading (view);
	}
}

static gboolean
display_pending_files (FMDirectoryView *view)
{
	gboolean call_timeout_again;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	call_timeout_again = NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 display_pending_files, 
		 (view, &view->details->pending_files_added, 
		  &view->details->pending_files_changed));

	finish_displaying_pending_files (view);

	return call_timeout_again;
}

static gboolean
real_display_pending_files (FMDirectoryView *view,
			    GList **pending_files_added,
			    GList **pending_files_changed)
{
	GList *files_added, *files_changed, *node;
	NautilusFile *file;
	GList *selection;
	gboolean send_selection_change;

	send_selection_change = FALSE;

	g_assert (*pending_files_added == view->details->pending_files_added);
	g_assert (*pending_files_changed == view->details->pending_files_changed);
	
	/* Deliver all available files right now */
	files_added = *pending_files_added;
	files_changed = *pending_files_changed;

	if (files_added != NULL || files_changed != NULL) {
		*pending_files_added = NULL;
		*pending_files_changed = NULL;

		gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_ADDING_FILES]);
		
		for (node = files_added; node != NULL; node = node->next) {
			file = NAUTILUS_FILE (node->data);
			
			if (nautilus_directory_contains_file (view->details->model, file)) {
				gtk_signal_emit (GTK_OBJECT (view),
						 signals[ADD_FILE],
						 file);
			}
		}
		
		for (node = files_changed; node != NULL; node = node->next) {
			file = NAUTILUS_FILE (node->data);
			
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[FILE_CHANGED],
					 file);
		}
		
		gtk_signal_emit (GTK_OBJECT (view), signals[DONE_ADDING_FILES]);

		if (files_changed != NULL) {
			selection = fm_directory_view_get_selection (view);
			send_selection_change = nautilus_g_lists_sort_and_check_for_intersection
				(&files_changed, &selection);
			nautilus_file_list_free (selection);
		}

		nautilus_file_list_free (files_added);
		nautilus_file_list_free (files_changed);
	}

	if (send_selection_change) {
		/* Send a selection change since some file names could
		 * have changed.
		 */
		fm_directory_view_send_selection_change (view);
	}

	return TRUE;
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	view->details->display_selection_idle_id = 0;
	fm_directory_view_display_selection_info (view);
	if (view->details->send_selection_change_to_shell) {
		fm_directory_view_send_selection_change (view);
	}

	return FALSE;
}

static void
remove_update_menus_timeout_callback (FMDirectoryView *view) 
{
	if (view->details->update_menus_timeout_id != 0) {
		gtk_timeout_remove (view->details->update_menus_timeout_id);
		view->details->update_menus_timeout_id = 0;
	}
}

static void
update_menus_if_pending (FMDirectoryView *view)
{
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

	view->details->update_menus_timeout_id = 0;
	fm_directory_view_update_menus (view);

	return FALSE;
}

static gboolean
display_pending_idle_callback (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	view->details->display_pending_idle_id = 0;
	display_pending_files (view);

	/* Don't do another idle until we receive more files. */

	return FALSE;
}

static gboolean
display_pending_timeout_callback (gpointer data)
{
	FMDirectoryView *view;
	gboolean no_files_are_left;

	view = FM_DIRECTORY_VIEW (data);

	/* Do another timeout if we displayed some files. Once we get
	 * all the files, we'll start using idle instead.
	 */
	no_files_are_left = display_pending_files (view);

	if (no_files_are_left) {
		view->details->display_pending_timeout_id = 0;
	}

	return !no_files_are_left;
}

static void
schedule_idle_display_of_pending_files (FMDirectoryView *view)
{
	/* No need to schedule an idle if there's already one pending. */
	if (view->details->display_pending_idle_id != 0) {
		return;
	}

	/* An idle takes precedence over a timeout. */
	unschedule_timeout_display_of_pending_files (view);

	view->details->display_pending_idle_id =
		gtk_idle_add (display_pending_idle_callback, view);
}

static void
schedule_timeout_display_of_pending_files (FMDirectoryView *view)
{
	/* No need to schedule a timeout if there's already one pending. */
	if (view->details->display_pending_timeout_id != 0) {
		return;
	}

	/* An idle takes precedence over a timeout. */
	if (view->details->display_pending_idle_id != 0) {
		return;
	}

	view->details->display_pending_timeout_id =
		gtk_timeout_add (DISPLAY_TIMEOUT_INTERVAL_MSECS,
				 display_pending_timeout_callback, view);
}

static void
unschedule_idle_display_of_pending_files (FMDirectoryView *view)
{
	/* Get rid of idle if it's active. */
	if (view->details->display_pending_idle_id != 0) {
		g_assert (view->details->display_pending_timeout_id == 0);
		gtk_idle_remove (view->details->display_pending_idle_id);
		view->details->display_pending_idle_id = 0;
	}
}

static void
unschedule_timeout_display_of_pending_files (FMDirectoryView *view)
{
	/* Get rid of timeout if it's active. */
	if (view->details->display_pending_timeout_id != 0) {
		g_assert (view->details->display_pending_idle_id == 0);
		gtk_timeout_remove (view->details->display_pending_timeout_id);
		view->details->display_pending_timeout_id = 0;
	}
}

static void
unschedule_display_of_pending_files (FMDirectoryView *view)
{
	unschedule_idle_display_of_pending_files (view);
	unschedule_timeout_display_of_pending_files (view);
}

static void
queue_pending_files (FMDirectoryView *view,
		     GList *files,
		     GList **pending_list)
{
	GList *filtered_files;

	/* Filter out hidden files if needed */
	filtered_files = nautilus_file_list_filter_hidden_and_backup
		(files,
		 view->details->show_hidden_files,
		 view->details->show_backup_files);

	/* Put the files on the pending list if there are any. */
	if (filtered_files != NULL) {
		*pending_list = g_list_concat (*pending_list, filtered_files);
		schedule_timeout_display_of_pending_files (view);
	}
}

static void
files_added_callback (NautilusDirectory *directory,
		      GList *files,
		      gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_added);
}

static void
files_changed_callback (NautilusDirectory *directory,
			GList *files,
			gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_changed);
	
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
	
	schedule_idle_display_of_pending_files (view);
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
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[LOAD_ERROR], load_error_code);
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
	GList *singleton_list;

	singleton_list = g_list_prepend (NULL, file);
	queue_pending_files (view, singleton_list, &view->details->pending_files_changed);
	g_list_free (singleton_list);
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

	gtk_signal_emit (GTK_OBJECT (view), signals[CLEAR]);
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

	gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_LOADING]);
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

	gtk_signal_emit (GTK_OBJECT (view), signals[END_LOADING]);
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

	NAUTILUS_CALL_METHOD
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
fm_directory_view_zoom_to_level (FMDirectoryView *view, int zoom_level)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!fm_directory_view_supports_zooming (view)) {
		return;
	}

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 zoom_to_level, (view, zoom_level));
}


void
fm_directory_view_set_zoom_level (FMDirectoryView *view, int zoom_level)
{
	float new_zoom_level;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!fm_directory_view_supports_zooming (view)) {
		return;
	}

	new_zoom_level = (float) nautilus_get_icon_size_for_zoom_level (zoom_level)
		/ NAUTILUS_ICON_SIZE_STANDARD;

	bonobo_zoomable_report_zoom_level_changed (view->details->zoomable, new_zoom_level);
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

	NAUTILUS_CALL_METHOD
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

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 can_zoom_in, (view));
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

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 can_zoom_out, (view));
}

GtkWidget *
fm_directory_view_get_background_widget (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_background_widget, (view));
}

NautilusBackground *
fm_directory_view_get_background (FMDirectoryView *view)
{
	return nautilus_get_widget_background (fm_directory_view_get_background_widget (view));
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

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_selection, (view));
}

/**
 * fm_directory_view_get_bonobo_ui_container:
 *
 * Get the BonoboUIContainer for this FMDirectoryView.
 * This is normally called only by subclasses in order to
 * install and modify bonobo menus and such.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: BonoboUIContainer for this view.
 * 
 **/
Bonobo_UIContainer
fm_directory_view_get_bonobo_ui_container (FMDirectoryView *view)
{
        g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);
        
        return bonobo_control_get_remote_ui_container (get_bonobo_control (view));
}

/**
 * fm_directory_view_get_bonobo_control:
 *
 * Get the BonoboControl for this FMDirectoryView.
 * This is normally called only by subclasses in order to
 * help editables interact with the clipboard ui component
 * @view: FMDirectoryView of interest.
 * 
 * Return value: BonoboUIContainer for this view.
 * 
 **/
BonoboControl *
fm_directory_view_get_bonobo_control (FMDirectoryView *view)
{
	NautilusView *nautilus_view;

	nautilus_view = fm_directory_view_get_nautilus_view (view);
	return nautilus_view_get_bonobo_control (nautilus_view);
}


/**
 * fm_directory_view_get_nautilus_view:
 *
 * Get the NautilusView for this FMDirectoryView.
 * This is normally called only by the embedding framework.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusView for this view.
 * 
 **/
NautilusView *
fm_directory_view_get_nautilus_view (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return view->details->nautilus_view;
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
append_uri_one (gpointer data, gpointer callback_data)
{
	NautilusFile *file;
	GList **result;
	
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (callback_data != NULL);

	result = (GList **) callback_data;
	file = (NautilusFile *) data;
	*result = g_list_append (*result, nautilus_file_get_uri (file));
}

static void
offset_drop_points (GArray *relative_item_points,
		    int x_offset, int y_offset)
{
	guint index;

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
	g_list_foreach (files, append_uri_one, &uris);    

        g_assert (g_list_length (uris) == g_list_length (files));

	/* offset the drop locations a bit so that we don't pile
	 * up the icons on top of each other
	 */
	offset_drop_points (relative_item_points, DUPLICATE_HORIZONTAL_ICON_OFFSET,
		DUPLICATE_VERTICAL_ICON_OFFSET);

        copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move (uris, relative_item_points, NULL, GDK_ACTION_LINK, 
		GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	nautilus_g_list_free_deep (uris);
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
	g_list_foreach (files, append_uri_one, &uris);    

        g_assert (g_list_length (uris) == g_list_length (files));
        
	/* offset the drop locations a bit so that we don't pile
	 * up the icons on top of each other
	 */
	offset_drop_points (relative_item_points, DUPLICATE_HORIZONTAL_ICON_OFFSET,
		DUPLICATE_VERTICAL_ICON_OFFSET);

        copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move (uris, relative_item_points, NULL, GDK_ACTION_COPY,
		GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	nautilus_g_list_free_deep (uris);
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
fm_directory_all_selected_items_in_trash (FMDirectoryView *view)
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

gboolean
fm_directory_link_type_in_selection (FMDirectoryView *view,
				     NautilusLinkType link_type)
{
	gboolean saw_link;
	GList *selection, *node;
	NautilusFile *file;
	char *uri, *path;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	saw_link = FALSE;

	selection = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));

	for (node = selection; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		uri = nautilus_file_get_uri (file);
		path = gnome_vfs_get_local_path_from_uri (uri);
		/* FIXME: This reads the link file every single time. */
		saw_link = path != NULL
			&& nautilus_file_is_nautilus_link (file)
			&& nautilus_link_local_get_link_type (path) == link_type;
		
		g_free (path);
		g_free (uri);
		
		if (saw_link) {
			break;
		}
	}
	
	nautilus_file_list_free (selection);
	
	return saw_link;
}

static gboolean
is_link_type_special (NautilusLinkType type)
{
	switch (type) {
	case NAUTILUS_LINK_TRASH:
	case NAUTILUS_LINK_HOME:
	case NAUTILUS_LINK_MOUNT:
		return TRUE;
	case NAUTILUS_LINK_GENERIC:
		return FALSE;
	}
	return FALSE;
}

/* special_link_in_selection
 * 
 * Return TRUE is one of our special links is the selection.
 * Special links include the following: 
 *	 NAUTILUS_LINK_TRASH, NAUTILUS_LINK_HOME, NAUTILUS_LINK_MOUNT
 */
 
static gboolean
special_link_in_selection (FMDirectoryView *view)
{
	gboolean saw_link;
	GList *selection, *node;
	NautilusFile *file;
	char *uri, *path;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	saw_link = FALSE;

	selection = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));

	for (node = selection; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		uri = nautilus_file_get_uri (file);
		path = gnome_vfs_get_local_path_from_uri (uri);

		/* FIXME: This reads the link file every single time. */
		saw_link = path != NULL
			&& nautilus_file_is_nautilus_link (file)
			&& is_link_type_special (nautilus_link_local_get_link_type (path));
		
		g_free (path);
		g_free (uri);
		
		if (saw_link) {
			break;
		}
	}
	
	nautilus_file_list_free (selection);
	
	return saw_link;
}

static gboolean
fm_directory_view_can_move_uri_to_trash (FMDirectoryView *view, const char *file_uri_string)
{
	/* Return TRUE if we can get a trash directory on the same volume as this file. */
	GnomeVFSURI *file_uri;
	GnomeVFSURI *directory_uri;
	GnomeVFSURI *trash_dir_uri;
	gboolean result;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);
	g_return_val_if_fail (file_uri_string != NULL, FALSE);

	file_uri = gnome_vfs_uri_new (file_uri_string);

	g_return_val_if_fail (file_uri != NULL, FALSE);

	directory_uri = gnome_vfs_uri_get_parent (file_uri);
	gnome_vfs_uri_unref (file_uri);

	if (directory_uri == NULL) {
		return FALSE;
	}

	/* Create a new trash if needed but don't go looking for an old Trash.
	 */
	result = gnome_vfs_find_directory (directory_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
					   &trash_dir_uri, TRUE, FALSE, 0777) == GNOME_VFS_OK;
	if (result) {
		if (gnome_vfs_uri_equal (trash_dir_uri, directory_uri)
		    || gnome_vfs_uri_is_parent (trash_dir_uri, directory_uri, TRUE)) {
			/* don't allow trashing items already in the Trash */
			result = FALSE;
		}
		gnome_vfs_uri_unref (trash_dir_uri);
	}
	gnome_vfs_uri_unref (directory_uri);

	return result;
}

static char *
file_name_from_uri (const char *uri)
{
	NautilusFile *file;
	char *file_name;
	
	file = nautilus_file_get (uri);
	file_name = nautilus_file_get_name (file);
	nautilus_file_unref (file);

	return file_name;	
}

static gboolean
fm_directory_view_confirm_deletion (FMDirectoryView *view, GList *uris, gboolean all)
{
	GnomeDialog *dialog;
	char *prompt;
	int uri_count;
	char *file_name;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	uri_count = g_list_length (uris);
	g_assert (uri_count > 0);
	
	if (uri_count == 1) {
		file_name = file_name_from_uri ((char *)uris->data);

		prompt = g_strdup_printf (_("\"%s\" cannot be moved to the Trash. Do "
					    "you want to delete it immediately?"), file_name);
		g_free (file_name);
	} else {
		if (all) {
			prompt = g_strdup_printf (_("The %d selected items cannot be moved "
						    "to the Trash. Do you want to delete them "
						    "immediately?"), uri_count);
		} else {
			prompt = g_strdup_printf (_("%d of the selected items cannot be moved "
						    "to the Trash. Do you want to delete those "
						    "%d items immediately?"), uri_count, uri_count);
		}
	}

	dialog = nautilus_show_yes_no_dialog (
		prompt,
		_("Delete Immediately?"),
		_("Delete"),
		GNOME_STOCK_BUTTON_CANCEL,
		fm_directory_view_get_containing_window (view));

	g_free (prompt);

	return gnome_dialog_run (dialog) == GNOME_OK;
}

static gboolean
confirm_delete_from_trash (FMDirectoryView *view, GList *uris)
{
	GnomeDialog *dialog;
	char *prompt;
	char *file_name;
	int uri_count;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Just Say Yes if the preference says not to confirm. */
	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH)) {
		return TRUE;
	}

	uri_count = g_list_length (uris);
	g_assert (uri_count > 0);

	if (uri_count == 1) {
		file_name = file_name_from_uri ((char *)uris->data);

		prompt = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\" "
					    "from the Trash?"), file_name);
		g_free (file_name);
	} else {
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete "
		  			    "the %d selected items from the Trash?"), uri_count);
	}

	dialog = nautilus_show_yes_no_dialog (
		prompt,
		_("Delete From Trash?"),
		_("Delete"),
		GNOME_STOCK_BUTTON_CANCEL,
		fm_directory_view_get_containing_window (view));

	g_free (prompt);

	return gnome_dialog_run (dialog) == GNOME_OK;
}

static void
trash_or_delete_files_common (const GList *file_uris,
			      GArray *relative_item_points,
			      const char *target_uri,
			      int copy_action,
			      int x, int y,
			      FMDirectoryView *view)
{
	const GList *file_node;
	char *file_uri;
	GList *moveable_uris;
	GList *unmoveable_uris;
	GList *in_trash_uris;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (file_uris != NULL);

	/* Collect three lists: (1) items that can be moved to trash,
	 * (2) items that can only be deleted in place, and (3) items that
	 * are already in trash. 
	 * 
	 * Always move (1) to trash if non-empty.
	 * Delete (3) only if (1) and (2) are non-empty, otherwise ignore (3).
	 * Ask before deleting (2) if non-empty.
	 */
	moveable_uris = NULL;
	unmoveable_uris = NULL;
	in_trash_uris = NULL;
	
	for (file_node = file_uris; file_node != NULL; file_node = file_node->next) {
		file_uri = (char *)file_node->data;
		
		if (fm_directory_view_can_move_uri_to_trash (view, file_uri)) {
			moveable_uris = g_list_prepend (moveable_uris, g_strdup (file_uri));
		} else if (nautilus_uri_is_in_trash (file_uri)) {
			in_trash_uris = g_list_prepend (in_trash_uris, g_strdup (file_uri));
		} else {
			unmoveable_uris = g_list_prepend (unmoveable_uris, g_strdup (file_uri));
		}
	}

	if (moveable_uris != NULL) {
		nautilus_file_operations_copy_move (moveable_uris, relative_item_points, 
			 target_uri, copy_action, GTK_WIDGET (view),
			 copy_move_done_callback, pre_copy_move (view));
	}

	if (in_trash_uris != NULL && moveable_uris == NULL && unmoveable_uris == NULL) {
		/* Don't confirm if the preference says not to. */
		if (confirm_delete_from_trash (view, in_trash_uris)) {
			nautilus_file_operations_delete (in_trash_uris, GTK_WIDGET (view));
		}		
	}

	if (unmoveable_uris != NULL) {
		if (fm_directory_view_confirm_deletion (view, 
							unmoveable_uris,
							moveable_uris == NULL)) {
			nautilus_file_operations_delete (unmoveable_uris, GTK_WIDGET (view));
		}
	}
	
	nautilus_g_list_free_deep (moveable_uris);
	nautilus_g_list_free_deep (unmoveable_uris);
	nautilus_g_list_free_deep (in_trash_uris);
}

static void
fm_directory_view_trash_or_delete_files (const GList *files,
					 FMDirectoryView *view)
{
	GList *file_uris;
	const GList *node;
	
	file_uris = NULL;
	for (node = files; node != NULL; node = node->next) {
		file_uris = g_list_prepend (file_uris,
					    nautilus_file_get_uri ((NautilusFile *) node->data));
	}
	
	file_uris = g_list_reverse (file_uris);
	trash_or_delete_files_common (file_uris, NULL, NAUTILUS_TRASH_URI,
				      GDK_ACTION_MOVE, 0, 0, view);					 
	nautilus_g_list_free_deep (file_uris);
}

static void
start_renaming_item (FMDirectoryView *view, const char *uri)
{
	NautilusFile *file;
	file = nautilus_file_get (uri);
	if (file !=  NULL) {
		fm_directory_view_select_file (view, file);
	}
}

static void
reveal_newly_added_folder (FMDirectoryView *view, NautilusFile *new_file, char* target_uri)
{
	if (nautilus_file_matches_uri (new_file, target_uri)) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (view), &reveal_newly_added_folder, target_uri);
		/* no need to select because start_renaming_item selects
		 * fm_directory_view_select_file (view, new_file);
		 */
		NAUTILUS_CALL_METHOD (FM_DIRECTORY_VIEW_CLASS, view, start_renaming_item, (view, target_uri));
		fm_directory_view_reveal_selection (view);
	}
}

static void
new_folder_done (const char *new_folder_uri, gpointer data)
{
	FMDirectoryView *directory_view;

	directory_view = (FMDirectoryView *) data;
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));

	/* We need to run after the default handler adds the folder we want to
	 * operate on. The ADD_FILE signal is registered as GTK_RUN_LAST, so we
	 * must use connect_after.
	 */
	gtk_signal_connect_full (GTK_OBJECT (directory_view),
				 "add_file",
				 reveal_newly_added_folder,
				 NULL,
				 g_strdup (new_folder_uri),
				 g_free,
				 FALSE,
				 TRUE);
}

void
fm_directory_view_new_folder (FMDirectoryView *directory_view)
{
	char *parent_uri;

	parent_uri = fm_directory_view_get_uri (directory_view);
	nautilus_file_operations_new_folder (GTK_WIDGET (directory_view), parent_uri, new_folder_done, directory_view);

	g_free (parent_uri);
}

/* handle the open command */

static void
open_one_in_new_window (gpointer data, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_activate_file (FM_DIRECTORY_VIEW (callback_data),
					 NAUTILUS_FILE (data),
					 FORCE_NEW_WINDOW);
}

static void
open_one_properties_window (gpointer data, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_properties_window_present (data, callback_data);
}

static void
remove_custom_icon (gpointer file, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (callback_data == NULL);

	nautilus_file_set_metadata (NAUTILUS_FILE (file),
				    NAUTILUS_METADATA_KEY_ICON_SCALE,
				    NULL, NULL);
	nautilus_file_set_metadata (NAUTILUS_FILE (file),
				    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
				    NULL, NULL);
}

NautilusFile *
fm_directory_view_get_directory_as_file (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	return view->details->directory_as_file; 
}

static gboolean
files_have_any_custom_images (GList *files)
{
	GList *p;
	char *uri;

	for (p = files; p != NULL; p = p->next) {
		uri = nautilus_file_get_metadata (NAUTILUS_FILE (p->data),
						  NAUTILUS_METADATA_KEY_CUSTOM_ICON,
						  NULL);
		if (uri != NULL) {
			g_free (uri);
			return TRUE;
		}
	}

	return FALSE;
}

static void
remove_custom_icons_callback (BonoboUIComponent *component, gpointer callback_data, const char *verb)
{
	GList *selection;

	selection = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (callback_data));
	g_list_foreach (selection, remove_custom_icon, NULL);
	nautilus_file_list_free (selection);

        /* Update menus because Remove Custom Icons item has changed state */
	schedule_update_menus (FM_DIRECTORY_VIEW (callback_data));
}

static void
bonobo_launch_application_callback (BonoboUIComponent *component, gpointer callback_data, const char *path)
{
	ApplicationLaunchParameters *launch_parameters;
	
	launch_parameters = (ApplicationLaunchParameters *) callback_data;
	fm_directory_view_launch_application 
		(launch_parameters->application,
		 launch_parameters->file,
		 launch_parameters->directory_view);
}				    

static void
bonobo_open_location_with_viewer_callback (BonoboUIComponent *component, gpointer callback_data, const char *path)
{
	ViewerLaunchParameters *launch_parameters;

	launch_parameters = (ViewerLaunchParameters *) callback_data;

	switch_location_and_view (launch_parameters->identifier,
				  launch_parameters->uri,
				  launch_parameters->directory_view);
}

static void
add_numbered_menu_item (BonoboUIComponent *ui,
			const char *parent_path,
			const char *label,
			const char *tip,
			int index,
			GdkPixbuf *pixbuf,
			gpointer callback,
			gpointer callback_data,
			GDestroyNotify destroy_notify)
{
	char *escaped_label, *verb_name, *item_path;
	
	escaped_label = nautilus_str_double_underscores (label);

	nautilus_bonobo_add_numbered_menu_item 
		(ui, 
		 parent_path,
		 index,
		 escaped_label, 
		 pixbuf);
	g_free (escaped_label);

	item_path = nautilus_bonobo_get_numbered_menu_item_path
		(ui, parent_path, index);

	nautilus_bonobo_set_tip (ui, item_path, tip);
	g_free (item_path);

	verb_name = nautilus_bonobo_get_numbered_menu_item_command 
		(ui, parent_path, index);	
	bonobo_ui_component_add_verb_full (ui, verb_name, callback, callback_data, destroy_notify);	   
	g_free (verb_name);
}				 

static void
add_application_to_bonobo_menu (FMDirectoryView *directory_view,
				GnomeVFSMimeApplication *application, 
				NautilusFile *file,
				int index)
{
	ApplicationLaunchParameters *launch_parameters;
	char *tip;

	launch_parameters = application_launch_parameters_new 
		(application, file, directory_view);
	tip = g_strdup_printf (_("Use \"%s\" to open the selected item"), application->name);

	add_numbered_menu_item (directory_view->details->ui, 
				FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER,
				application->name,
				tip,
				index,
				NULL,
				bonobo_launch_application_callback,
				launch_parameters,
				(GDestroyNotify) application_launch_parameters_free);
	/* Use same launch parameters and no DestroyNotify for popup item, which has same
	 * lifetime as the item in the File menu in the menu bar.
	 */
	add_numbered_menu_item (directory_view->details->ui, 
				FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER,
				application->name,
				tip,
				index,
				NULL,
				bonobo_launch_application_callback,
				launch_parameters,
				NULL);
	g_free (tip);
}

static void
add_component_to_bonobo_menu (FMDirectoryView *directory_view,
			      OAF_ServerInfo *content_view, 
			      const char *uri,
			      int index)
{
	NautilusViewIdentifier *identifier;
	ViewerLaunchParameters *launch_parameters;
	char *tip;
	char *label;
	
	identifier = nautilus_view_identifier_new_from_content_view (content_view);
	launch_parameters = viewer_launch_parameters_new (identifier, uri, directory_view);
	nautilus_view_identifier_free (identifier);

	label = g_strdup (launch_parameters->identifier->viewer_label);
	tip = g_strdup_printf (_("Use \"%s\" to open the selected item"), label);

	add_numbered_menu_item (directory_view->details->ui, 
				FM_DIRECTORY_VIEW_MENU_PATH_VIEWERS_PLACEHOLDER,
				label,
				tip,
				index,
				NULL,
				bonobo_open_location_with_viewer_callback,
				launch_parameters,
				(GDestroyNotify) viewer_launch_parameters_free);
	/* Use same launch parameters and no DestroyNotify for popup item, which has same
	 * lifetime as the item in the File menu in the menu bar.
	 */
 	add_numbered_menu_item (directory_view->details->ui, 
				FM_DIRECTORY_VIEW_POPUP_PATH_VIEWERS_PLACEHOLDER,
				label,
				tip,
				index,
				NULL,
				bonobo_open_location_with_viewer_callback,
				launch_parameters,
				NULL);
	g_free (tip);
	g_free (label);
}


static void
reset_bonobo_open_with_menu (FMDirectoryView *view, GList *selection)
{
	GList *applications, *components, *node;
	NautilusFile *file;
	gboolean sensitive;
	char *uri;
	int index;
	
	/* Clear any previous inserted items in the applications and viewers placeholders */
	nautilus_bonobo_remove_menu_items_and_commands
		(view->details->ui, FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands 
		(view->details->ui, FM_DIRECTORY_VIEW_MENU_PATH_VIEWERS_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands 
		(view->details->ui, FM_DIRECTORY_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands 
		(view->details->ui, FM_DIRECTORY_VIEW_POPUP_PATH_VIEWERS_PLACEHOLDER);
	
	/* This menu is only displayed when there's one selected item. */
	if (!nautilus_g_list_exactly_one_item (selection)) {
		sensitive = FALSE;
		monitor_file_for_open_with (view, NULL);
	} else {
		sensitive = TRUE;
		file = NAUTILUS_FILE (selection->data);
		
		monitor_file_for_open_with (view, file);

		uri = nautilus_file_get_uri (file);
		
		applications = nautilus_mime_get_short_list_applications_for_file (NAUTILUS_FILE (selection->data));
		for (node = applications, index = 0; node != NULL; node = node->next, index++) {
			add_application_to_bonobo_menu (view, node->data, file, index);
		}
		gnome_vfs_mime_application_list_free (applications); 
		
		components = nautilus_mime_get_short_list_components_for_file (NAUTILUS_FILE (selection->data));
		for (node = components, index = 0; node != NULL; node = node->next, index++) {
			add_component_to_bonobo_menu (view, node->data, uri, index);
		}
		gnome_vfs_mime_component_list_free (components); 

		g_free (uri);
	}

	/* It's OK to set the sensitivity of the menu items (rather than the verbs)
	 * here because these are submenu titles, not items with verbs.
	 */
	nautilus_bonobo_set_sensitive (view->details->ui,
				       FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH,
				       sensitive);
	nautilus_bonobo_set_sensitive (view->details->ui,
				       FM_DIRECTORY_VIEW_POPUP_PATH_OPEN_WITH,
				       sensitive);
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
		quoted_name = nautilus_shell_quote (name);
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

	if (nautilus_directory_is_local (view->details->model)) {
		file_paths = get_file_paths_as_newline_delimited_string (selected_files);
	} else {
		file_paths = g_strdup ("");
	}
	nautilus_setenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS", file_paths, TRUE);
	g_free (file_paths);

	uris = get_file_uris_as_newline_delimited_string (selected_files);
	nautilus_setenv ("NAUTILUS_SCRIPT_SELECTED_URIS", uris, TRUE);
	g_free (uris);

	uri = nautilus_directory_get_uri (view->details->model);
	nautilus_setenv ("NAUTILUS_SCRIPT_CURRENT_URI", uri, TRUE);
	g_free (uri);

	geometry_string = gnome_geometry_string 
		(GTK_WIDGET (fm_directory_view_get_containing_window (view))->window);
	nautilus_setenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY", geometry_string, TRUE);
	g_free (geometry_string);
}

/* Unset all the special script environment variables. */
static void
unset_script_environment_variables (void)
{
	nautilus_unsetenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS");
	nautilus_unsetenv ("NAUTILUS_SCRIPT_SELECTED_URIS");
	nautilus_unsetenv ("NAUTILUS_SCRIPT_CURRENT_URI");
	nautilus_unsetenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY");
}

static void
run_script_callback (BonoboUIComponent *component, gpointer callback_data, const char *path)
{
	ScriptLaunchParameters *launch_parameters;
	GList *selected_files;
	char *file_uri;
	char *local_file_path;
	char *quoted_path;
	char *old_working_dir;
	char *parameters, *command;
	
	launch_parameters = (ScriptLaunchParameters *) callback_data;

	file_uri = nautilus_file_get_uri (launch_parameters->file);
	local_file_path = gnome_vfs_get_local_path_from_uri (file_uri);
	g_assert (local_file_path != NULL);
	g_free (file_uri);

	quoted_path = nautilus_shell_quote (local_file_path);
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

	/* FIXME: handle errors with dialog? Or leave up to each script? */
	nautilus_launch_application_from_command (command, NULL, FALSE);
	g_free (command);

	nautilus_file_list_free (selected_files);
	unset_script_environment_variables ();
	chdir (old_working_dir);		
	g_free (old_working_dir);
	g_free (quoted_path);
}				    

static void
add_script_to_menus (FMDirectoryView *directory_view,
		     NautilusFile *file,
		     int index)
{
	ScriptLaunchParameters *launch_parameters;
	char *tip;
	char *name;
	GdkPixbuf *pixbuf;

	name = nautilus_file_get_name (file);
	tip = g_strdup_printf (_("Run \"%s\" on any selected items"), name);
	
	launch_parameters = script_launch_parameters_new (file, directory_view);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_file 
		(file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS, TRUE);

 	add_numbered_menu_item (directory_view->details->ui, 
				FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER,
				name,
				tip,
				index,
				pixbuf,
				run_script_callback,
				launch_parameters,
				(GDestroyNotify) script_launch_parameters_free);

	/* Use same launch parameters and no DestroyNotify for popup item, which has same
	 * lifetime as the item in the File menu in the menu bar.
	 */
 	add_numbered_menu_item (directory_view->details->ui, 
				FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER,
				name,
				tip,
				index,
				pixbuf,
				run_script_callback,
				launch_parameters,
				NULL);

	gdk_pixbuf_unref (pixbuf);
	g_free (name);
	g_free (tip);
}

static void
reset_scripts_menu (FMDirectoryView *view, GList *all_files)
{
	GList *node;
	NautilusFile *file;
	int index;
	gboolean any_scripts;
	
	nautilus_bonobo_remove_menu_items_and_commands
		(view->details->ui, FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands 
		(view->details->ui, FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER);

	all_files = nautilus_file_list_sort_by_name (all_files);

	any_scripts = FALSE;
	for (node = all_files, index = 0; node != NULL; node = node->next, ++index) {
		file = node->data;

		if (file_is_launchable (file)) {
			add_script_to_menus (view, file, index);
			any_scripts = TRUE;
		}
	}

	nautilus_bonobo_set_hidden (view->details->ui, 
				    FM_DIRECTORY_VIEW_MENU_PATH_SCRIPTS_SEPARATOR, 
				    !any_scripts);
	nautilus_bonobo_set_hidden (view->details->ui, 
				    FM_DIRECTORY_VIEW_POPUP_PATH_SCRIPTS_SEPARATOR, 
				    !any_scripts);

	view->details->scripts_invalid = FALSE;
}

static void
reset_scripts_menu_callback (NautilusDirectory *directory,
			     GList *files,
			     gpointer callback_data)
{
	FMDirectoryView *view;

	view = callback_data;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (view->details->scripts_directory == directory);

	reset_scripts_menu (view, files);
}

static gboolean
any_valid_scripts (GList *files)
{
	GList *node;

	for (node = files; node != NULL; node = node->next) {
		if (file_is_launchable (NAUTILUS_FILE (node->data))) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
notify_user_if_no_scripts_callback (NautilusDirectory *directory,
			     	    GList *files,
			     	    gpointer callback_data)
{
	FMDirectoryView *view;

	view = callback_data;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (view->details->scripts_directory == directory);

	if (any_valid_scripts (files)) {
		return;
	}

	nautilus_show_info_dialog_with_details 
		(_("All executable files in this folder will appear in the "
		   "Scripts menu. Choosing a script from the menu will run "
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
call_when_ready_on_scripts_directory (FMDirectoryView *view,
				      NautilusDirectoryCallback scripts_directory_callback)
{
	GList *attributes;

	if (view->details->scripts_directory == NULL) {
		return;
	}

	nautilus_directory_cancel_callback (view->details->scripts_directory,
				    	    scripts_directory_callback,
				    	    view);

	/* Later we may want to add more attributes here to get icon, etc. */
	attributes = nautilus_icon_factory_get_required_file_attributes ();
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
	nautilus_directory_call_when_ready (view->details->scripts_directory,
					    attributes,
					    scripts_directory_callback,
					    view);
	g_list_free (attributes);
}

static void
schedule_reset_scripts_menu (FMDirectoryView *view)
{
	call_when_ready_on_scripts_directory (view, reset_scripts_menu_callback);
}

static void
open_scripts_folder_callback (BonoboUIComponent *component, 
		     	      gpointer callback_data, 
		     	      const char *verb)
{      
	FMDirectoryView *view;
	char *uri;

	view = FM_DIRECTORY_VIEW (callback_data);          

	if (view->details->scripts_directory != NULL) {
		uri = nautilus_directory_get_uri (view->details->scripts_directory);
		open_location (view, uri, RESPECT_PREFERENCE);
		g_free (uri);

		call_when_ready_on_scripts_directory (view, notify_user_if_no_scripts_callback);
	} else {
		/* This shouldn't happen. If it does, get_scripts_directory was unable to
		 * mkdir the scripts directory, and this case should be debugged.
		 */
		nautilus_show_error_dialog (_("Nautilus was unable to create the directory %s."),
					    _("No Scripts Folder"),
					    fm_directory_view_get_containing_window (view));
	}
}



static BonoboWindow *
get_bonobo_window (FMDirectoryView *view)
{
	GtkWidget *window;
	
	/* Note: This works only because we are in the same process
	 * as the Nautilus shell. Bonobo components in their own
	 * processes can't do this.
	 */
	window = gtk_widget_get_ancestor (GTK_WIDGET (view), BONOBO_TYPE_WINDOW);
	g_assert (window != NULL);

	return BONOBO_WINDOW (window);
}

static GtkMenu *
create_popup_menu (FMDirectoryView *view, const char *popup_path)
{
	GtkMenu *menu;
	
	menu = GTK_MENU (gtk_menu_new ());
	gtk_widget_show (GTK_WIDGET (menu));

	bonobo_window_add_popup (get_bonobo_window (view), menu, popup_path);

	return menu;
}

static void
real_merge_menus (FMDirectoryView *view)
{
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("New Folder", new_folder_callback),
		BONOBO_UI_VERB ("Open", open_callback),
		BONOBO_UI_VERB ("OpenNew", open_in_new_window_callback),
		BONOBO_UI_VERB ("OtherApplication", other_application_callback),
		BONOBO_UI_VERB ("OtherViewer", other_viewer_callback),
		BONOBO_UI_VERB ("Show Properties", open_properties_window_callback),
		BONOBO_UI_VERB ("Trash", trash_callback),
		BONOBO_UI_VERB ("Duplicate", duplicate_callback),
		BONOBO_UI_VERB ("Create Link", create_link_callback),
		BONOBO_UI_VERB ("Show Trash", show_trash_callback),
		BONOBO_UI_VERB ("Empty Trash", bonobo_menu_empty_trash_callback),
		BONOBO_UI_VERB ("Select All", bonobo_menu_select_all_callback),
		BONOBO_UI_VERB ("Remove Custom Icons", remove_custom_icons_callback),
		BONOBO_UI_VERB ("Reset Background", reset_background_callback),
		BONOBO_UI_VERB ("Open Scripts Folder", open_scripts_folder_callback),
		BONOBO_UI_VERB_END
	};

	/* This BonoboUIComponent is made automatically, and its lifetime is
	 * controlled automatically. We don't need to explicitly ref or unref it.
	 */
	view->details->ui = nautilus_view_set_up_ui (view->details->nautilus_view,
						     DATADIR,
						     "nautilus-directory-view-ui.xml",
						     "nautilus");

	bonobo_ui_component_add_verb_list_with_data (view->details->ui, verbs, view);

	gtk_signal_connect_object (GTK_OBJECT (fm_directory_view_get_background (view)),
			    	   "settings_changed",
			    	   schedule_update_menus,
			    	   GTK_OBJECT (view));

	/* Do one-time state changes here; context-dependent ones go in update_menus */
	if (!fm_directory_view_supports_zooming (view)) {
		nautilus_bonobo_set_hidden 
			(view->details->ui, NAUTILUS_POPUP_PATH_ZOOM_ITEMS_PLACEHOLDER, TRUE);
	}

	view->details->scripts_invalid = TRUE;
}

static gboolean confirm_trash = TRUE;

static void
confirm_trash_changed_callback (gpointer callback_data)
{
	confirm_trash = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH);
}

static void
real_update_menus (FMDirectoryView *view)
{
	static gboolean confirm_trash_changed_callback_installed = FALSE;
	GList *selection;
	gint selection_count;
	char *label_with_underscore;
	gboolean selection_contains_special_link;
	gboolean can_create_files;
	NautilusBackground *background;
	
	selection = fm_directory_view_get_selection (view);
	selection_count = g_list_length (selection);

	/* Add the callback once for the life of our process */
	if (!confirm_trash_changed_callback_installed) {
		nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
						   confirm_trash_changed_callback,
						   NULL);
		confirm_trash_changed_callback_installed = TRUE;
		
		/* Peek for the first time */
		confirm_trash_changed_callback (NULL);
	}
	
	selection_contains_special_link = special_link_in_selection (view);
	can_create_files = fm_directory_view_supports_creating_files (view);

	bonobo_ui_component_freeze (view->details->ui, NULL);


	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_NEW_FOLDER,
				       can_create_files);

	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_OPEN,
				       selection_count == 1);
	
	if (selection_count <= 1) {
		label_with_underscore = g_strdup (_("Open _in New Window"));
	} else {
		label_with_underscore = g_strdup_printf (_("Open _in %d New Windows"), selection_count);
	}
	nautilus_bonobo_set_label_for_menu_item_and_command 
		(view->details->ui,
		 FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW,
		 FM_DIRECTORY_VIEW_COMMAND_OPEN_IN_NEW_WINDOW,
		 label_with_underscore);
	g_free (label_with_underscore);
				   
	/* If the only selected item is launchable, dim out "Open in New Window"
	 * to avoid confusion about how it differs from "Open" in this case (it
	 * doesn't differ; they would do the same thing).
	 */
	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_OPEN_IN_NEW_WINDOW,
				       selection_count == 1
				        ? !file_is_launchable (NAUTILUS_FILE (selection->data))
				        : selection_count != 0);

	/* Broken into its own function just for convenience */
	reset_bonobo_open_with_menu (view, selection);


	if (fm_directory_all_selected_items_in_trash (view)) {
		nautilus_bonobo_set_tip (view->details->ui, 
					 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
					 _("Delete all selected items permanently"));
		nautilus_bonobo_set_accelerator (view->details->ui, 
						 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
						 "");
		label_with_underscore = g_strdup (confirm_trash 
						   ? _("Delete from _Trash...") 
						   : _("Delete from _Trash"));
	} else {
		nautilus_bonobo_set_tip (view->details->ui, 
					 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
					 _("Move all selected items to the Trash"));
		nautilus_bonobo_set_accelerator (view->details->ui, 
						 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
						 "*Control*t");
		label_with_underscore = g_strdup (_("Move to _Trash"));
	}
	nautilus_bonobo_set_label_for_menu_item_and_command 
		(view->details->ui,
		 FM_DIRECTORY_VIEW_MENU_PATH_TRASH,
		 FM_DIRECTORY_VIEW_COMMAND_TRASH,
		 label_with_underscore);
	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_TRASH,
				       !fm_directory_view_is_read_only (view)
			     	       	&& selection_count != 0
			      		&& !selection_contains_special_link);

	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_DUPLICATE,
				       can_create_files
			     	       	&& selection_count != 0
			      		&& !selection_contains_special_link);

	background = fm_directory_view_get_background (view);
	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_RESET_BACKGROUND,
				       background != NULL
				       && nautilus_file_background_is_set (background));

	nautilus_bonobo_set_label_for_menu_item_and_command 
		(view->details->ui,
		 FM_DIRECTORY_VIEW_MENU_PATH_CREATE_LINK,
		 FM_DIRECTORY_VIEW_COMMAND_CREATE_LINK,
		 selection_count > 1
			? _("Create _Links")
			: _("Create _Link"));
	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_CREATE_LINK,
				       can_create_files
			     	       	&& selection_count != 0
			      		&& !selection_contains_special_link);

	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_SHOW_PROPERTIES,
				       selection_count != 0
			      		&& fm_directory_view_supports_properties (view));


	nautilus_bonobo_set_label_for_menu_item_and_command 
		(view->details->ui,
		 FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH,
		 FM_DIRECTORY_VIEW_COMMAND_EMPTY_TRASH,
		 confirm_trash
			? _("_Empty Trash...")
			: _("_Empty Trash"));
	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_EMPTY_TRASH,
				       !nautilus_trash_monitor_is_empty ());


	nautilus_bonobo_set_label_for_menu_item_and_command 
		(view->details->ui,
		 FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS,
		 FM_DIRECTORY_VIEW_COMMAND_REMOVE_CUSTOM_ICONS,
		 selection_count > 1
			? _("R_emove Custom Images")
			: _("R_emove Custom Image"));
	nautilus_bonobo_set_sensitive (view->details->ui, 
				       FM_DIRECTORY_VIEW_COMMAND_REMOVE_CUSTOM_ICONS,
				       files_have_any_custom_images (selection));

	nautilus_bonobo_set_sensitive (view->details->ui, 
				       NAUTILUS_COMMAND_SELECT_ALL,
				       !fm_directory_view_is_empty (view));


	bonobo_ui_component_thaw (view->details->ui, NULL);

	nautilus_file_list_free (selection);

	if (view->details->scripts_invalid) {
		schedule_reset_scripts_menu (view);
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

	nautilus_pop_up_context_menu (create_popup_menu 
				      	(view, FM_DIRECTORY_VIEW_POPUP_PATH_SELECTION),
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
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

	nautilus_pop_up_context_menu (create_popup_menu 
				      (view, FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND),
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      event);
}

static void
schedule_update_menus (FMDirectoryView *view) 
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Make sure we haven't already destroyed it */
	g_assert (view->details->nautilus_view != NULL);

	view->details->menu_states_untrustworthy = TRUE;
	
	if (view->details->menus_merged
	    && view->details->update_menus_timeout_id == 0) {
		view->details->update_menus_timeout_id
			= gtk_timeout_add (300, update_menus_timeout_callback, view);
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
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!view->details->selection_change_is_due_to_shell) {
		view->details->send_selection_change_to_shell = TRUE;
	}

	/* Schedule a display of the new selection. */
	if (view->details->display_selection_idle_id == 0) {
		view->details->display_selection_idle_id
			= gtk_idle_add (display_selection_info_idle_callback,
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
		&& nautilus_file_is_executable (file);
}

static void
report_broken_symbolic_link (FMDirectoryView *view, NautilusFile *file)
{
	char *target_path;
	char *prompt;
	GnomeDialog *dialog;
	GList file_as_list;
	
	g_assert (nautilus_file_is_broken_symbolic_link (file));

	target_path = nautilus_file_get_symbolic_link_target_path (file);
	if (target_path == NULL) {
		prompt = g_strdup_printf (_("This link can't be used, because it has no target. "
					    "Do you want to put this link in the Trash?"));
	} else {
		prompt = g_strdup_printf (_("This link can't be used, because its target \"%s\" doesn't exist. "
				 	    "Do you want to put this link in the Trash?"),
					  target_path);
	}

	dialog = nautilus_show_yes_no_dialog (prompt,
				  	      _("Broken Link"),
					      _("Throw Away"),
					      GNOME_STOCK_BUTTON_CANCEL,
					      fm_directory_view_get_containing_window (view));

	gnome_dialog_set_default (dialog, GNOME_CANCEL);

	/* Make this modal to avoid problems with reffing the view & file
	 * to keep them around in case the view changes, which would then
	 * cause the old view not to be destroyed, which would cause its
	 * merged Bonobo items not to be un-merged. Maybe we need to unmerge
	 * explicitly when disconnecting views instead of relying on the
	 * unmerge in Destroy. But since BonoboUIHandler is probably going
	 * to change wildly, I don't want to mess with this now.
	 */
	if (gnome_dialog_run (dialog) == GNOME_OK) {
		file_as_list.data = file;
		file_as_list.next = NULL;
		file_as_list.prev = NULL;
	        fm_directory_view_trash_or_delete_files (&file_as_list, view);					 
	}

	g_free (target_path);
	g_free (prompt);
}

static ActivationAction
get_executable_text_file_action (FMDirectoryView *view, NautilusFile *file)
{
	GnomeDialog *dialog;
	char *file_name;
	char *prompt;
	int preferences_value;

	g_assert (nautilus_file_contains_text (file));

	preferences_value = nautilus_preferences_get_integer 
		(NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION);
	switch (preferences_value) {
	case NAUTILUS_EXECUTABLE_TEXT_LAUNCH:
		return ACTIVATION_ACTION_LAUNCH;
	case NAUTILUS_EXECUTABLE_TEXT_DISPLAY:
		return ACTIVATION_ACTION_DISPLAY;
	case NAUTILUS_EXECUTABLE_TEXT_ASK:
		break;
	default:
		/* Complain non-fatally, since preference data can't be trusted */
		g_warning ("Unknown value %d for NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION",
			   preferences_value);
		
	}


	file_name = nautilus_file_get_name (file);
	prompt = g_strdup_printf (_("\"%s\" is an executable text file. "
				    "Do you want to run it, or display its contents?"),
				  file_name);
	g_free (file_name);

	dialog = nautilus_create_question_dialog (prompt,
					 	  _("Run or Display?"),
					 	  _("Run"),
					 	  _("Display"),
					 	  fm_directory_view_get_containing_window (view));

	gnome_dialog_append_button (dialog, _("Cancel"));
	gtk_widget_show (GTK_WIDGET (dialog));
	
	g_free (prompt);
	
	switch (gnome_dialog_run (dialog)) {
	case 0:
		return ACTIVATION_ACTION_LAUNCH;
	case 1:
		return ACTIVATION_ACTION_DISPLAY;
	default:
		return ACTIVATION_ACTION_DO_NOTHING;
	}
}

static void
activate_callback (NautilusFile *file, gpointer callback_data)
{
	ActivateParameters *parameters;
	FMDirectoryView *view;
	char *uri, *command, *executable_path, *quoted_path;
	GnomeVFSMimeApplication *application;
	ActivationAction action;
	gboolean need_to_continue_monitoring_file_for_activation;
	
	parameters = callback_data;

	need_to_continue_monitoring_file_for_activation = FALSE;

	nautilus_timed_wait_stop (cancel_activate_callback, parameters);

	view = FM_DIRECTORY_VIEW (parameters->view);

	uri = nautilus_file_get_activation_uri (file);

	action = ACTIVATION_ACTION_DISPLAY;

	/* Note that we check for FILE_TYPE_SYMBOLIC_LINK only here,
	 * not specifically for broken-ness, because the file type
	 * will be the target's file type in the non-broken case.
	 */
	if (nautilus_file_is_broken_symbolic_link (file)) {
		report_broken_symbolic_link (view, file);
		action = ACTIVATION_ACTION_DO_NOTHING;
	} else if (nautilus_istr_has_prefix (uri, NAUTILUS_COMMAND_SPECIFIER)) {
		/* Don't allow command execution from remote locations
		 * to partially mitigate the security risk of
		 * executing arbitrary commands.
		 */
		if (!nautilus_file_is_local (file)) {
			nautilus_show_error_dialog
				(_("Sorry, but you can't execute commands from "
				   "a remote site due to security considerations."), 
				 _("Can't execute remote links"),
				 fm_directory_view_get_containing_window (view));
			action = ACTIVATION_ACTION_DO_NOTHING;
		} else {
			/* As an additional precaution, only execute
			 * commands without any parameters, which is
			 * enforced by using a call that uses
			 * fork/execlp instead of system.
			 */
			command = uri + strlen (NAUTILUS_COMMAND_SPECIFIER);
			nautilus_gnome_shell_execute (command);
			action = ACTIVATION_ACTION_DO_NOTHING;
		}
	}

	if (action != ACTIVATION_ACTION_DO_NOTHING && file_is_launchable (file)) {

		action = ACTIVATION_ACTION_LAUNCH;
		
		/* FIXME bugzilla.eazel.com 2391: This should check if
		 * the activation URI points to something launchable,
		 * not the original file. Also, for symbolic links we
		 * need to check the X bit on the target file, not on
		 * the original.
		 */

		/* Launch executables to activate them. */
		executable_path = gnome_vfs_get_local_path_from_uri (uri);

		/* Non-local executables don't get launched. They act like non-executables. */
		if (executable_path == NULL) {
			action = ACTIVATION_ACTION_DISPLAY;
		} else if (nautilus_file_contains_text (file)) {
			/* Special case for executable text files, since it might be
			 * dangerous & unexpected to launch these.
			 */
			action = get_executable_text_file_action (view, file);
		}

		if (action == ACTIVATION_ACTION_LAUNCH) {
			quoted_path = nautilus_shell_quote (executable_path);
			/* FIXME bugzilla.eazel.com 1773: This is a
			 * lame way to run command-line tools, since
			 * there's no terminal for the output.
			 */
			nautilus_launch_application_from_command (quoted_path, NULL, FALSE);
			g_free (quoted_path);
		}
		
		g_free (executable_path);
	}

	if (action == ACTIVATION_ACTION_DISPLAY) {
		if (nautilus_mime_get_default_action_type_for_file (file)
		    == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
			application = nautilus_mime_get_default_application_for_file (file);
		} else {
			/* If the action type is unspecified, treat it like
			 * the component case. This is most likely to happen
			 * (only happens?) when there are no registered
			 * viewers or apps, or there are errors in the
			 * mime.keys files.
			 */
			application = NULL;
		}

		if (application != NULL) {
			fm_directory_view_launch_application (application, file, view);
			gnome_vfs_mime_application_free (application);
		} else {
			open_location (view, uri, parameters->choice);
			need_to_continue_monitoring_file_for_activation = TRUE;
		}
	}

	if (!need_to_continue_monitoring_file_for_activation) {
		monitor_file_for_activation (view, NULL);
	}

	g_free (uri);
	g_free (parameters);
}

static void
cancel_activate_callback (gpointer callback_data)
{
	ActivateParameters *parameters;

	parameters = (ActivateParameters *) callback_data;

	nautilus_file_cancel_call_when_ready (parameters->file, 
					      activate_callback, 
					      parameters);

	monitor_file_for_activation (parameters->view, NULL);

	g_free (parameters);
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
				 WindowChoice choice)
{
	ActivateParameters *parameters;
	GList *attributes;
	char *file_name;
	char *timed_wait_prompt;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	monitor_file_for_activation (view, file);

	/* Might have to read some of the file to activate it. */
	attributes = nautilus_mime_actions_get_full_file_attributes ();
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FILE_TYPE);
	parameters = g_new (ActivateParameters, 1);
	parameters->view = view;
	parameters->file = file;
	parameters->choice = choice;

	file_name = nautilus_file_get_name (file);
	timed_wait_prompt = g_strdup_printf (_("Opening \"%s\""), file_name);
	g_free (file_name);
	
	nautilus_timed_wait_start
		(cancel_activate_callback,
		 parameters,
		 _("Cancel Open?"),
		 timed_wait_prompt,
		 fm_directory_view_get_containing_window (view));
	g_free (timed_wait_prompt);
	nautilus_file_call_when_ready
		(file, attributes, activate_callback, parameters);

	g_list_free (attributes);
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
				  GList *files)
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
	use_new_window = file_count > 1
		||  nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW);
	
	if (!use_new_window || fm_directory_view_confirm_multiple_windows (view, file_count)) {
		for (node = files; node != NULL; node = node->next) {  	
			fm_directory_view_activate_file 
				(view, node->data,
				 file_count == 1
				 ? RESPECT_PREFERENCE
				 : PREFER_EXISTING_WINDOW);
		}
	}
}

static void
file_changed_callback (NautilusFile *file, gpointer callback_data)
{
	schedule_update_menus (FM_DIRECTORY_VIEW (callback_data));
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
	GList *attributes;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

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

	/* FIXME bugzilla.eazel.com 5062: In theory, we also need to monitor metadata here (as
         * well as doing a call when ready), in case external forces
         * change the directory's file metadata.
	 */
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
	view->details->metadata_for_directory_as_file_pending = TRUE;
	view->details->metadata_for_files_in_directory_pending = TRUE;
	nautilus_file_call_when_ready
		(view->details->directory_as_file,
		 attributes,
		 metadata_for_directory_as_file_ready_callback, view);
	nautilus_directory_call_when_ready
		(view->details->model,
		 attributes,
		 metadata_for_files_in_directory_ready_callback, view);
	g_list_free (attributes);

	/* If capabilities change, then we need to update the menus
	 * because of New Folder.
	 */
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES);
	nautilus_file_monitor_add (view->details->directory_as_file,
				   &view->details->directory_as_file,
				   attributes);
	g_list_free (attributes);

	view->details->file_changed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->directory_as_file), 
		 "changed",
		 file_changed_callback,
		 view);
}

static void
finish_loading (FMDirectoryView *view)
{
	GList *attributes;

	nautilus_view_report_load_underway (view->details->nautilus_view);

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	fm_directory_view_begin_loading (view);

	schedule_timeout_display_of_pending_files (view);
	view->details->loading = TRUE;

	/* Start loading. */

	/* Connect handlers to see files as they are added. */
    	view->details->files_added_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model),
		 "files_added",
		 files_added_callback,
		 view);
	view->details->files_changed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model), 
		 "files_changed",
		 files_changed_callback,
		 view);
	view->details->done_loading_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model),
		 "done_loading",
		 done_loading_callback,
		 view);
	view->details->load_error_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model),
		 "load_error",
		 load_error_callback,
		 view);

	/* Monitor the things needed to get the right
	 * icon. Also monitor a directory's item count because
	 * the "size" attribute is based on that, and the file's metadata.
	 */
	attributes = nautilus_icon_factory_get_required_file_attributes ();
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
	attributes = g_list_prepend (attributes, 
				     NAUTILUS_FILE_ATTRIBUTE_METADATA);
	attributes = g_list_prepend (attributes, 
				     NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE);

	nautilus_directory_file_monitor_add (view->details->model,
					     &view->details->model,
					     view->details->show_hidden_files,
					     view->details->show_backup_files,
					     attributes);

	g_list_free (attributes);
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
	g_assert (view->details->metadata_for_directory_as_file_pending = TRUE);

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
	g_assert (view->details->metadata_for_files_in_directory_pending = TRUE);

	view->details->metadata_for_files_in_directory_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
}

NautilusStringList *
fm_directory_view_get_emblem_names_to_exclude (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 get_emblem_names_to_exclude, (view));
}

static NautilusStringList *
real_get_emblem_names_to_exclude (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	return nautilus_string_list_new_from_string (NAUTILUS_FILE_EMBLEM_NAME_TRASH, TRUE);
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

	/* Remember that the menus have been merged so that we
	 * won't try to update them before merging them.
	 */
	view->details->menus_merged = TRUE;

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 merge_menus, (view));
}

static void
disconnect_handler (GtkObject *object, int *id)
{
	if (*id != 0) {
		gtk_signal_disconnect (object, *id);
		*id = 0;
	}
}

static void
disconnect_directory_handler (FMDirectoryView *view, int *id)
{
	disconnect_handler (GTK_OBJECT (view->details->model), id);
}

static void
disconnect_scripts_directory_handler (FMDirectoryView *view, int *id)
{
	disconnect_handler (GTK_OBJECT (view->details->scripts_directory), id);
}

static void
disconnect_directory_as_file_handler (FMDirectoryView *view, int *id)
{
	disconnect_handler (GTK_OBJECT (view->details->directory_as_file), id);
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
	nautilus_directory_file_monitor_remove (view->details->model,
						&view->details->model);
	nautilus_file_cancel_call_when_ready (view->details->directory_as_file,
					      metadata_for_directory_as_file_ready_callback,
					      view);
	nautilus_directory_cancel_callback (view->details->model,
					    metadata_for_files_in_directory_ready_callback,
					    view);
	nautilus_file_monitor_remove (view->details->directory_as_file,
				      &view->details->directory_as_file);
}

static void
disconnect_script_handlers (FMDirectoryView *view)
{
	if (view->details->scripts_directory == NULL) {
		return;
	}

	disconnect_scripts_directory_handler (view, &view->details->scripts_added_handler_id);
	disconnect_scripts_directory_handler (view, &view->details->scripts_changed_handler_id);

	nautilus_directory_file_monitor_remove (view->details->scripts_directory,
						&view->details->scripts_directory);

	nautilus_directory_cancel_callback (view->details->scripts_directory,
					    reset_scripts_menu_callback,
					    view);

	nautilus_directory_cancel_callback (view->details->scripts_directory,
					    notify_user_if_no_scripts_callback,
					    view);
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

	NAUTILUS_CALL_METHOD
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

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 set_selection, (view, selection));
}

static void
fm_directory_view_select_file (FMDirectoryView *view, NautilusFile *file)
{
	GList file_list;
	file_list.data = file;
	file_list.next = NULL;
	fm_directory_view_set_selection (view, &file_list);
}

/**
 * fm_directory_get_selected_icon_locations:
 *
 * return an array of locations of selected icons if available
 * Return value: GArray of GdkPoints
 * 
 **/
GArray *
fm_directory_get_selected_icon_locations (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
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

	NAUTILUS_CALL_METHOD
		(FM_DIRECTORY_VIEW_CLASS, view,
		 reveal_selection, (view));
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
	nautilus_file_list_free (view->details->pending_files_added);
	view->details->pending_files_added = NULL;
	nautilus_file_list_free (view->details->pending_files_changed);
	view->details->pending_files_changed = NULL;
	nautilus_g_list_free_deep (view->details->pending_uris_selected);
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

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 is_read_only, (view));
}

gboolean
fm_directory_view_is_empty (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 is_empty, (view));
}

static gboolean
real_is_read_only (FMDirectoryView *view)
{
	return !nautilus_file_can_write (fm_directory_view_get_directory_as_file (view));
}

gboolean
fm_directory_view_supports_creating_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 supports_creating_files, (view));
}

gboolean
fm_directory_view_accepts_dragged_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
		(FM_DIRECTORY_VIEW_CLASS, view,
		 accepts_dragged_files, (view));
}

static gboolean
showing_trash_directory (FMDirectoryView *view)
{
	return nautilus_file_is_in_trash (fm_directory_view_get_directory_as_file (view));
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

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
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

	return NAUTILUS_CALL_METHOD_WITH_RETURN_VALUE
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

	NAUTILUS_CALL_METHOD
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

	directory_view = FM_DIRECTORY_VIEW (callback_data);

	directory_view->details->show_hidden_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
	
	directory_view->details->show_backup_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES);

	/* Reload the current uri so that the filtering changes take place. */
	if (directory_view->details->model != NULL) {
		load_directory (directory_view,
				directory_view->details->model);
	}
}

void
fm_directory_view_ignore_hidden_file_preferences (FMDirectoryView *view)
{
	g_return_if_fail (view->details->model == NULL);

	if (view->details->ignore_hidden_file_preferences) {
		return;
	}

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					      filtering_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
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

void
fm_directory_view_move_copy_items (const GList *item_uris,
				   GArray *relative_item_points,
				   const char *target_uri,
				   int copy_action,
				   int x, int y,
				   FMDirectoryView *view)
{
	char *command_string, *scanner;
	int length;
	const GList *p;
	
	g_assert (relative_item_points->len == 0 
		|| g_list_length ((GList *)item_uris) == relative_item_points->len);

	/* add the drop location to the icon offsets */
	offset_drop_points (relative_item_points, x, y);

	/* special-case "command:" here instead of starting a move/copy */
	if (nautilus_str_has_prefix (target_uri, NAUTILUS_COMMAND_SPECIFIER)) {
		/* execute command, passing it the dragged uris */
		
		/* strip the leading "command:" */
		target_uri += strlen (NAUTILUS_COMMAND_SPECIFIER);

		/* how long will the command string be? */
		length = strlen (target_uri) + 1;
		for (p = item_uris; p != NULL; p = p->next) {
			length += strlen ((const char *) p->data) + 1;
		}
		
		command_string = g_malloc (length);
		scanner = command_string;
		
		/* copy the command string */
		strcpy (scanner, target_uri);
		scanner += strlen (scanner);
		
		/* copy the uris */
		for (p = item_uris; p != NULL; p = p->next) {
			sprintf (scanner, " %s", (const char *) p->data);
			scanner += strlen (scanner);
		}
		
		/* execute the command */
		nautilus_gnome_shell_execute (command_string);
		
		g_free (command_string);
		return;
	}
	
	if (nautilus_uri_is_trash (target_uri)) {
		trash_or_delete_files_common (item_uris, relative_item_points, 
				              target_uri, copy_action,
				              x, y, view);
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
	GList *attributes;

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
		g_list_free (attributes);
	}
}

static void
monitor_file_for_activation (FMDirectoryView *view,
			     NautilusFile *file)
{
	char *uri;
	NautilusDirectory **directory_spot;
	NautilusDirectory *directory, *old_directory;

	if (file == NULL) {
		directory = NULL;
	} else {
		uri = nautilus_file_get_uri (file);
		directory = nautilus_directory_get (uri);
		g_free (uri);
	}

	/* Quick out when not changing. */
	directory_spot = &view->details->directory_monitored_for_activation;
	old_directory = *directory_spot;
	if (old_directory == directory) {
		nautilus_directory_unref (directory);
		return;
	}

	/* Point at the new directory. */
	view->details->directory_monitored_for_activation = directory;

	/* Stop monitoring the old directory. */
	if (old_directory != NULL) {
		nautilus_directory_file_monitor_remove (old_directory, directory_spot);
		nautilus_directory_unref (old_directory);
	}

	/* Start monitoring the new directory. */
	if (directory != NULL) {
		nautilus_directory_file_monitor_add (directory, directory_spot,
						     TRUE, TRUE, NULL);
	}
}
