/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
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
#include "fm-properties-window.h"
#include "nautilus-trash-monitor.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-win.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-directory-list.h>
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
#include <libnautilus-extensions/nautilus-view-identifier.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-zoomable.h>
#include <math.h>
#include <src/nautilus-application.h>

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500
#define SILENT_WINDOW_OPEN_LIMIT	5

/* Paths to use when referring to bonobo menu items. */
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN                      		"/menu/File/Open Placeholder/Open"
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW        		"/menu/File/Open Placeholder/OpenNew"
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH				"/menu/File/Open Placeholder/Open With"
#define FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER				"/menu/File/New Items Placeholder/New Folder"
#define FM_DIRECTORY_VIEW_MENU_PATH_DELETE                    		"/menu/File/File Items Placeholder/Delete"
#define FM_DIRECTORY_VIEW_MENU_PATH_TRASH                    		"/menu/File/File Items Placeholder/Trash"
#define FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH                    	"/menu/File/Global File Items Placeholder/Empty Trash"
#define FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE                	 	"/menu/File/File Items Placeholder/Duplicate"
#define FM_DIRECTORY_VIEW_MENU_PATH_CREATE_LINK                	 	"/menu/File/File Items Placeholder/Create Link"
#define FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES         	   	"/menu/File/File Items Placeholder/Show Properties"
#define FM_DIRECTORY_VIEW_MENU_PATH_RESET_BACKGROUND			"/menu/Edit/Global Edit Items Placeholder/Reset Background"
#define FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS			"/menu/Edit/Edit Items Placeholder/Remove Custom Icons"
#define FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER    	"/menu/File/Open Placeholder/Open With/Applications Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION    		"/menu/File/Open Placeholder/Open With/OtherApplication"
#define FM_DIRECTORY_VIEW_MENU_PATH_VIEWERS_PLACEHOLDER    		"/menu/File/Open Placeholder/Open With/Viewers Placeholder"
#define FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER	   		"/menu/File/Open Placeholder/Open With/OtherViewer"

#define FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND				"/popups/background"

enum {
	ADD_FILE,
	CREATE_BACKGROUND_CONTEXT_MENU_ITEMS,
	CREATE_SELECTION_CONTEXT_MENU_ITEMS,
	BEGIN_ADDING_FILES,
	BEGIN_LOADING,
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
	NautilusZoomable *zoomable;

	NautilusDirectory *model;
	NautilusFile *directory_as_file;
	BonoboUIComponent *ui;

	guint display_selection_idle_id;
	guint update_menus_idle_id;
	
	guint display_pending_timeout_id;
	guint display_pending_idle_id;
	
	guint files_added_handler_id;
	guint files_changed_handler_id;
	
	GList *pending_files_added;
	GList *pending_files_changed;
	GList *pending_uris_selected;

	gboolean force_reload;
	gboolean loading;
	gboolean menus_merged;
	gboolean menu_states_untrustworthy;

	gboolean show_hidden_files;
	gboolean show_backup_files;

	gboolean batching_selection_level;
	gboolean selection_changed_while_batched;

	NautilusFile *file_monitored_for_open_with;
};

/* forward declarations */

static int                 display_selection_info_idle_callback                   (gpointer              data);
static gboolean            file_is_launchable                                     (NautilusFile         *file);
static void                fm_directory_view_initialize_class                     (FMDirectoryViewClass *klass);
static void                fm_directory_view_initialize                           (FMDirectoryView      *view);
static void                fm_directory_view_duplicate_selection                  (FMDirectoryView      *view,
										   GList                *files);
static void                fm_directory_view_create_links_for_files               (FMDirectoryView      *view,
										   GList                *files);
static void                fm_directory_view_trash_or_delete_files                (FMDirectoryView      *view,
										   GList                *files);
static void                fm_directory_view_destroy                              (GtkObject            *object);
static void                fm_directory_view_activate_file                        (FMDirectoryView      *view,
										   NautilusFile         *file,
										   gboolean              use_new_window);
static void                fm_directory_view_create_background_context_menu_items (FMDirectoryView      *view,
										   GtkMenu              *menu);
static void                load_directory                                         (FMDirectoryView      *view,
										   NautilusDirectory    *directory,
										   gboolean              force_reload);
static void                fm_directory_view_merge_menus                          (FMDirectoryView      *view);
static void                real_create_background_context_menu_items              (FMDirectoryView      *view,
										   GtkMenu              *menu);
static void                real_create_selection_context_menu_items               (FMDirectoryView      *view,
										   GtkMenu              *menu,
										   GList                *files);
static void                real_merge_menus                                       (FMDirectoryView      *view);
static void                real_update_menus                                      (FMDirectoryView      *view);
static gboolean            real_is_read_only                                      (FMDirectoryView      *view);
static gboolean            real_supports_creating_files                           (FMDirectoryView      *view);
static gboolean            real_accepts_dragged_files                             (FMDirectoryView      *view);
static gboolean            real_supports_zooming                                  (FMDirectoryView      *view);
static gboolean            real_supports_properties                               (FMDirectoryView      *view);
static GtkMenu *           create_selection_context_menu                          (FMDirectoryView      *view);
static GtkMenu *           create_background_context_menu                         (FMDirectoryView      *view);
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
static void                zoomable_set_zoom_level_callback                       (NautilusZoomable     *zoomable,
										   double                level,
										   FMDirectoryView      *view);
static void                zoomable_zoom_in_callback                              (NautilusZoomable     *zoomable,
										   FMDirectoryView      *directory_view);
static void                zoomable_zoom_out_callback                             (NautilusZoomable     *zoomable,
										   FMDirectoryView      *directory_view);
static void                zoomable_zoom_to_fit_callback                          (NautilusZoomable     *zoomable,
										   FMDirectoryView      *directory_view);
static void                schedule_update_menus                                  (FMDirectoryView      *view);
static void                schedule_update_menus_callback                         (gpointer              callback_data);
static void                schedule_idle_display_of_pending_files                 (FMDirectoryView      *view);
static void                unschedule_idle_display_of_pending_files               (FMDirectoryView      *view);
static void                schedule_timeout_display_of_pending_files              (FMDirectoryView      *view);
static void                unschedule_timeout_display_of_pending_files            (FMDirectoryView      *view);
static void                unschedule_display_of_pending_files                    (FMDirectoryView      *view);
static void                disconnect_model_handlers                              (FMDirectoryView      *view);
static void                filtering_changed_callback                             (gpointer              callback_data);
static NautilusStringList *real_get_emblem_names_to_exclude                       (FMDirectoryView      *view);
static void                start_renaming_item                                    (FMDirectoryView      *view,
										   const char           *uri);
static void                metadata_ready_callback                                (NautilusFile         *file,
										   gpointer              callback_data);
static void                fm_directory_view_trash_state_changed_callback         (NautilusTrashMonitor *trash,
										   gboolean              state,
										   gpointer              callback_data);
static void                fm_directory_view_select_file                          (FMDirectoryView      *view,
										   NautilusFile         *file);
static void                monitor_file_for_open_with                             (FMDirectoryView      *view,
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
	signals[CREATE_SELECTION_CONTEXT_MENU_ITEMS] =
		gtk_signal_new ("create_selection_context_menu_items",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, create_selection_context_menu_items),
		    		nautilus_gtk_marshal_NONE__BOXED_BOXED,
		    		GTK_TYPE_NONE, 2, GTK_TYPE_BOXED, GTK_TYPE_BOXED);
	signals[CREATE_BACKGROUND_CONTEXT_MENU_ITEMS] =
		gtk_signal_new ("create_background_context_menu_items",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, create_background_context_menu_items),
		    		gtk_marshal_NONE__BOXED,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);

	klass->create_selection_context_menu_items = real_create_selection_context_menu_items;
	klass->create_background_context_menu_items = real_create_background_context_menu_items;
        klass->merge_menus = real_merge_menus;
        klass->update_menus = real_update_menus;
	klass->get_emblem_names_to_exclude = real_get_emblem_names_to_exclude;
	klass->start_renaming_item = start_renaming_item;
	klass->is_read_only = real_is_read_only;
	klass->supports_creating_files = real_supports_creating_files;
	klass->accepts_dragged_files = real_accepts_dragged_files;
	klass->supports_zooming = real_supports_zooming;
	klass->supports_properties = real_supports_properties;
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

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

}

typedef struct {
	GnomeVFSMimeApplication *application;
	char *uri;
	FMDirectoryView *directory_view;
} ApplicationLaunchParameters;

typedef struct {
	NautilusViewIdentifier *identifier;
	char *uri;
	FMDirectoryView *directory_view;
} ViewerLaunchParameters;

static ApplicationLaunchParameters *
application_launch_parameters_new (GnomeVFSMimeApplication *application,
			      	   const char *uri,
			           FMDirectoryView *directory_view)
{
	ApplicationLaunchParameters *result;

	result = g_new0 (ApplicationLaunchParameters, 1);
	result->application = gnome_vfs_mime_application_copy (application);
	gtk_widget_ref (GTK_WIDGET (directory_view));
	result->directory_view = directory_view;
	result->uri = g_strdup (uri);

	return result;
}

static void
application_launch_parameters_free (ApplicationLaunchParameters *parameters)
{
	gnome_vfs_mime_application_free (parameters->application);
	gtk_widget_unref (GTK_WIDGET (parameters->directory_view));
	g_free (parameters->uri);
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

static GtkWindow *
get_containing_window (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	return GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));
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
	dialog = nautilus_yes_no_dialog (prompt, title, 
					 GNOME_STOCK_BUTTON_OK, 
					 GNOME_STOCK_BUTTON_CANCEL, 
					 get_containing_window (view));
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

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
open_callback (gpointer ignored, gpointer callback_data)
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
		                                 FALSE);        
        }        

	nautilus_file_list_free (selection);
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
open_in_new_window_callback (gpointer ignored, gpointer callback_data)
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
				      const char *uri,
				      FMDirectoryView *directory_view)
{
	g_assert (application != NULL);
	g_assert (uri != NULL);
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));

	nautilus_launch_application
		(application, uri, get_containing_window (directory_view));
	
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
			 launch_parameters->uri, 
			 launch_parameters->directory_view);
	}

	application_launch_parameters_free (launch_parameters);
}

static void
fm_directory_view_switch_location (FMDirectoryView *directory_view, 
				   const char *new_uri, 
				   gboolean use_new_window)
{
	g_assert (FM_IS_DIRECTORY_VIEW (directory_view));
	g_assert (new_uri != NULL);

	if (use_new_window) {
		nautilus_view_open_location_in_new_window
			(directory_view->details->nautilus_view, new_uri, NULL);
	} else {
		nautilus_view_open_location
			(directory_view->details->nautilus_view, new_uri);
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

	file = nautilus_file_get (new_uri);

	/* User has explicitly chosen a viewer other than the default, so
	 * make it the default and then switch locations.
	 */
	/* FIXME bugzilla.eazel.com 1053: We might want an atomic operation
	 * for switching location and viewer together, so we don't have to
	 * rely on metadata for holding the default location.
	 */
	nautilus_mime_set_default_component_for_file (file, identifier->iid);

	nautilus_file_unref (file);

	fm_directory_view_switch_location
		(directory_view, new_uri,
		 nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW, FALSE));
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
			 get_containing_window (view),
			 fm_directory_view_chose_component_callback,
			 viewer_launch_parameters_new
			 	(NULL, uri, view));
	} else {
		nautilus_choose_application_for_file 
			(file,
			 get_containing_window (view),
			 fm_directory_view_chose_application_callback,
			 application_launch_parameters_new
			 	(NULL, uri, view));
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

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
other_application_callback (gpointer ignored, gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	open_with_other_program (FM_DIRECTORY_VIEW (callback_data), 
				 GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
other_viewer_callback (gpointer ignored, gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	open_with_other_program (FM_DIRECTORY_VIEW (callback_data), 
				 GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
trash_callback (gpointer *ignored, gpointer callback_data)
{
        FMDirectoryView *view;
        GList *selection;
        
        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
	        fm_directory_view_trash_or_delete_files (view, selection);
	}

        nautilus_file_list_free (selection);
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
duplicate_callback (gpointer *ignored, gpointer callback_data)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
	        fm_directory_view_duplicate_selection (view, selection);
	}

        nautilus_file_list_free (selection);
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
create_link_callback (gpointer ignored, gpointer callback_data)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

        view = FM_DIRECTORY_VIEW (callback_data);
	selection = fm_directory_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
	        fm_directory_view_create_links_for_files (view, selection);
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
bonobo_menu_empty_trash_callback (BonoboUIComponent *component, 
				  gpointer callback_data, 
				  const char *verb)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	nautilus_file_operations_empty_trash (GTK_WIDGET (FM_DIRECTORY_VIEW (callback_data)));
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
new_folder_callback (gpointer ignored, gpointer callback_data)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	fm_directory_view_new_folder (FM_DIRECTORY_VIEW (callback_data));
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
open_properties_window_callback (gpointer ignored, gpointer callback_data)
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

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 text_attribute_names_changed, (view));
}

/* FIXME bugzilla.eazel.com 1856: 
 * This #include and the call to nautilus_directory_async_state_changed
 * are a hack to get the embedded text to appear if the preference started
 * out off but gets turned on. This is obviously not the right API, but
 * I wanted to check in and Darin was at lunch...
 */
#include <libnautilus-extensions/nautilus-directory-private.h>
static void
embedded_text_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 embedded_text_policy_changed, (view));
	
	nautilus_directory_async_state_changed
		(fm_directory_view_get_model (view));

}

static void
image_display_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 image_display_policy_changed, (view));
}

static void
directory_view_font_family_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 font_family_changed, (view));
}

static void
click_policy_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 click_policy_changed, (view));
}

static void
smooth_graphics_mode_changed_callback (gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 smooth_graphics_mode_changed, (view));
}

static double fm_directory_view_preferred_zoom_levels[] = {
	(double) NAUTILUS_ICON_SIZE_SMALLEST	/ NAUTILUS_ICON_SIZE_STANDARD,
	(double) NAUTILUS_ICON_SIZE_SMALLER	/ NAUTILUS_ICON_SIZE_STANDARD,
	(double) NAUTILUS_ICON_SIZE_SMALL	/ NAUTILUS_ICON_SIZE_STANDARD,
	(double) NAUTILUS_ICON_SIZE_STANDARD	/ NAUTILUS_ICON_SIZE_STANDARD,
	(double) NAUTILUS_ICON_SIZE_LARGE	/ NAUTILUS_ICON_SIZE_STANDARD,
	(double) NAUTILUS_ICON_SIZE_LARGER	/ NAUTILUS_ICON_SIZE_STANDARD,
	(double) NAUTILUS_ICON_SIZE_LARGEST	/ NAUTILUS_ICON_SIZE_STANDARD,
};

static void
fm_directory_view_initialize (FMDirectoryView *directory_view)
{
	directory_view->details = g_new0 (FMDirectoryViewDetails, 1);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (directory_view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);

	directory_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (directory_view));

	directory_view->details->zoomable = nautilus_zoomable_new_from_bonobo_control
		(get_bonobo_control (directory_view),
		 .25,
		 4.0,
		 FALSE,
		 fm_directory_view_preferred_zoom_levels,
		 NAUTILUS_N_ELEMENTS (fm_directory_view_preferred_zoom_levels));		

	gtk_signal_connect (GTK_OBJECT (directory_view->details->nautilus_view), 
			    "stop_loading",
			    GTK_SIGNAL_FUNC (stop_loading_callback),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (load_location_callback), 
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->nautilus_view), 
			    "selection_changed",
			    GTK_SIGNAL_FUNC (selection_changed_callback), 
			    directory_view);

        gtk_signal_connect (GTK_OBJECT (get_bonobo_control (directory_view)),
                            "activate",
                            bonobo_control_activate_callback,
                            directory_view);

	gtk_signal_connect (GTK_OBJECT (directory_view->details->zoomable), 
			    "zoom_in",
			    zoomable_zoom_in_callback,
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->zoomable), 
			    "zoom_out", 
			    zoomable_zoom_out_callback,
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->zoomable), 
			    "set_zoom_level", 
			    zoomable_set_zoom_level_callback,
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->zoomable), 
			    "zoom_to_fit", 
			    zoomable_zoom_to_fit_callback,
			    directory_view);
	gtk_signal_connect_while_alive (GTK_OBJECT (nautilus_trash_monitor_get ()),
				        "trash_state_changed",
				        fm_directory_view_trash_state_changed_callback,
				        directory_view,
				        GTK_OBJECT (directory_view));

	gtk_widget_show (GTK_WIDGET (directory_view));

	/* Obtain the filtering preferences */
	directory_view->details->show_hidden_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES, FALSE);

	directory_view->details->show_backup_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES, FALSE);

	/* Keep track of changes in this pref to filter files accordingly. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					   filtering_changed_callback,
					   directory_view);
	
	/* Keep track of changes in this pref to filter files accordingly. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					   filtering_changed_callback,
					   directory_view);
	
	/* Keep track of changes in this pref to display menu names correctly. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
					   schedule_update_menus_callback,
					   directory_view);
	
	/* Keep track of changes in text attribute names */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_CAPTIONS,
					   text_attribute_names_changed_callback,
					   directory_view);

	/* Keep track of changes in embedded text policy */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
					   embedded_text_policy_changed_callback,
					   directory_view);

	/* Keep track of changes in image display policy */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					   image_display_policy_changed_callback,
					   directory_view);

	/* Keep track of changes in the font family */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
					   directory_view_font_family_changed_callback, 
					   directory_view);

	/* Keep track of changes in clicking policy */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					   click_policy_changed_callback,
					   directory_view);
	
	/* Keep track of changes in graphics trade offs */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
					   smooth_graphics_mode_changed_callback, 
					   directory_view);
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

	nautilus_file_list_free (view->details->pending_files_added);
	view->details->pending_files_added = NULL;
	nautilus_file_list_free (view->details->pending_files_changed);
	view->details->pending_files_changed = NULL;
	nautilus_g_list_free_deep (view->details->pending_uris_selected);
	view->details->pending_uris_selected = NULL;

	monitor_file_for_open_with (view, NULL);

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	disconnect_model_handlers (view);
	nautilus_directory_unref (view->details->model);
	nautilus_file_unref (view->details->directory_as_file);

	if (view->details->display_selection_idle_id != 0) {
		gtk_idle_remove (view->details->display_selection_idle_id);
	}

	if (view->details->update_menus_idle_id != 0) {
		gtk_idle_remove (view->details->update_menus_idle_id);
	}

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					      filtering_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					      filtering_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
					      schedule_update_menus_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_ICON_CAPTIONS,
					      text_attribute_names_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
					      embedded_text_policy_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					      image_display_policy_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
					      directory_view_font_family_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					      click_policy_changed_callback,
					      view);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
					      smooth_graphics_mode_changed_callback,
					      view);

	g_free (view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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

static void
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
}



static void
load_location_callback (NautilusView *nautilus_view,
			const char *location,
			FMDirectoryView *directory_view)
{
	NautilusDirectory *directory;
	gboolean force_reload;

	directory = nautilus_directory_get (location);
	/* An additional load of the same directory is how the
	 * framework tells us to reload.
	 */
	force_reload = directory == directory_view->details->model;
	load_directory (directory_view, directory, force_reload);
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
			    FMDirectoryView *directory_view)
{
	GList *selection;

	if (directory_view->details->loading) {
		nautilus_g_list_free_deep (directory_view->details->pending_uris_selected);
		directory_view->details->pending_uris_selected = NULL;
	}

	if (!directory_view->details->loading) {
		/* If we aren't still loading, set the selection right now. */
		selection = file_list_from_uri_list (selection_uris);		
		fm_directory_view_set_selection (directory_view, selection);
		nautilus_file_list_free (selection);
	} else {
		/* If we are still loading, add to the list of pending URIs instead. */
		directory_view->details->pending_uris_selected =
			g_list_concat (directory_view->details->pending_uris_selected,
				       nautilus_g_str_list_copy (selection_uris));
	}
}

static void
stop_loading_callback (NautilusView *nautilus_view,
		       FMDirectoryView *directory_view)
{
	fm_directory_view_stop (directory_view);
}


static void
check_for_directory_hard_limit (FMDirectoryView *view)
{
	NautilusDirectory *directory;
	GnomeDialog *dialog;

	directory = view->details->model;
	if (nautilus_directory_file_list_length_reached (directory)) {
		dialog = nautilus_warning_dialog (_("We're sorry, but the directory you're viewing has more files than "
						    "we're able to display.  As a result, we are only able to show you the "
						    "first 4000 files it contains. "
						    "\n"
						    "This is a temporary limitation in this Preview Release of Nautilus, "
						    "and will not be present in the final shipping version.\n"),
						  _("Too many Files"),
						  get_containing_window (view));
	}

}


static void
done_loading (FMDirectoryView *view)
{
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
	}

	view->details->loading = FALSE;
}

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
reset_background_callback (gpointer ignored, gpointer callback_data)
{
	g_assert (FM_IS_DIRECTORY_VIEW (callback_data));

	nautilus_background_reset 
		(fm_directory_view_get_background 
			(FM_DIRECTORY_VIEW (callback_data)));
}

/* handle the zoom in/out menu items */

static void
zoom_in_callback (GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoom_out_callback (GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);	
}

static void
zoom_default_callback (GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_restore_default_zoom_level (directory_view);
}


static void
zoomable_zoom_in_callback (NautilusZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoomable_zoom_out_callback (NautilusZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);
}

static NautilusZoomLevel
nautilus_zoom_level_from_double(double zoom_level)
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
zoomable_set_zoom_level_callback (NautilusZoomable *zoomable, double level, FMDirectoryView *view)
{
	fm_directory_view_zoom_to_level (view, nautilus_zoom_level_from_double(level));
}

static void
zoomable_zoom_to_fit_callback (NautilusZoomable *zoomable, FMDirectoryView *view)
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
}CopyMoveDoneData;

static void
copy_move_done_data_free (CopyMoveDoneData *data)
{
	if (data != NULL) {
		nautilus_g_list_free_deep_custom (data->added_files, (GFunc) gtk_object_unref, NULL);
		g_free (data);
	}
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

	/* We need to run after the default handler adds the folder we want to
	 * operate on. The ADD_FILE signal is registered as GTK_RUN_LAST, so we
	 * must use connect_after.
	 */
	gtk_signal_connect_full (GTK_OBJECT (directory_view),
				 "add_file",
				 pre_copy_move_add_file_callback,
				 NULL,
				 copy_move_done_data,
				 (GtkDestroyNotify) copy_move_done_data_free,
				 FALSE,
				 TRUE);
	return copy_move_done_data;
}

/* This function is used to pull out any debuting uris that were added
 * and (as a side effect) remove them from the debuting uri hash table.
 */
static gboolean
copy_move_done_partition_func (gpointer data, gpointer callback_data)
{
 	char* uri;
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
			fm_directory_view_set_selection (directory_view, debuting_uri_data->added_files);
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

static gboolean
display_pending_files (FMDirectoryView *view)
{
	GList *files_added, *files_changed, *uris_selected, *p;
	NautilusFile *file;
	GList *selection;

	selection = NULL;

	if (view->details->model != NULL
	    && nautilus_directory_are_all_files_seen (view->details->model)) {
		done_loading (view);
	}

	files_added = view->details->pending_files_added;
	files_changed = view->details->pending_files_changed;
	uris_selected = view->details->pending_uris_selected;

	if (files_added == NULL && files_changed == NULL && uris_selected == NULL) {
		return FALSE;
	}
	view->details->pending_files_added = NULL;
	view->details->pending_files_changed = NULL;

	gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_ADDING_FILES]);

	for (p = files_added; p != NULL; p = p->next) {
		file = NAUTILUS_FILE (p->data);
		
		if (nautilus_directory_contains_file (view->details->model, file)) {
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[ADD_FILE],
					 file);
		}
	}

	for (p = files_changed; p != NULL; p = p->next) {
		file = NAUTILUS_FILE (p->data);
		
		gtk_signal_emit (GTK_OBJECT (view),
				 signals[FILE_CHANGED],
				 file);
	}

	gtk_signal_emit (GTK_OBJECT (view), signals[DONE_ADDING_FILES]);

	nautilus_file_list_free (files_added);
	nautilus_file_list_free (files_changed);

	if (nautilus_directory_are_all_files_seen (view->details->model)
	    && uris_selected != NULL) {
		view->details->pending_uris_selected = NULL;
		
		selection = file_list_from_uri_list (uris_selected);
		nautilus_g_list_free_deep (uris_selected);

		fm_directory_view_set_selection (view, selection);
		fm_directory_view_reveal_selection (view);

		nautilus_file_list_free (selection);
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
	fm_directory_view_send_selection_change (view);

	return FALSE;
}

static gboolean
update_menus_idle_callback (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	fm_directory_view_update_menus (view);
	
	view->details->update_menus_idle_id = 0;

	return FALSE;
}

static gboolean
display_pending_idle_callback (gpointer data)
{
	/* Don't do another idle until we receive more files. */

	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);

	view->details->display_pending_idle_id = 0;

	display_pending_files (view);

	return FALSE;
}

static gboolean
display_pending_timeout_callback (gpointer data)
{
	/* Do another timeout if we displayed some files.
	 * Once we get all the files, we'll start using
	 * idle instead.
	 */

	FMDirectoryView *view;
	gboolean displayed_some;

	view = FM_DIRECTORY_VIEW (data);

	displayed_some = display_pending_files (view);
	if (displayed_some) {
		return TRUE;
	}

	view->details->display_pending_timeout_id = 0;
	return FALSE;
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
	GList *files_iterator;
	NautilusFile *file;
	char * name;
	gboolean include_file;

	/* Desktop always filters out hidden files */
	if (FM_IS_DESKTOP_ICON_VIEW (view)) {
		view->details->show_hidden_files = FALSE;
		view->details->show_backup_files = FALSE;
	}
	
	filtered_files = NULL;

	/* Filter out hidden files if needed */
	if (!view->details->show_hidden_files || !view->details->show_backup_files) {
		/* FIXME bugzilla.eazel.com 653: 
		 * Eventually this should become a generic filtering thingy. 
		 */
		for (files_iterator = files; 
		     files_iterator != NULL; 
		     files_iterator = files_iterator->next) {
			file = NAUTILUS_FILE (files_iterator->data);
			
			g_assert (file != NULL);
			
			name = nautilus_file_get_name (file);
			
			g_assert (name != NULL);

			if (!view->details->show_hidden_files && nautilus_str_has_prefix (name, ".")) {
				include_file = FALSE;
			} else if (!view->details->show_backup_files && nautilus_str_has_suffix (name, "~")) {
				include_file = FALSE;
			} else {
				include_file = TRUE;
			}

			if (include_file) {
				filtered_files = g_list_append (filtered_files, file);
			}
			
			g_free (name);
		}
		
		files = filtered_files;
	}

	/* Put the files on the pending list if there are any. */
	if (files != NULL) {
		nautilus_file_list_ref (files);
		*pending_list = g_list_concat (*pending_list, g_list_copy (files));
		
		/* If we haven't see all the files yet, then we'll wait for the
		 * timeout to fire. If we have seen all the files, then we'll use
		 * an idle instead.
		 */
		if (nautilus_directory_are_all_files_seen (view->details->model)) {
			schedule_idle_display_of_pending_files (view);
		} else {
			schedule_timeout_display_of_pending_files (view);
		}
	}

	g_list_free (filtered_files);
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

	NAUTILUS_CALL_VIRTUAL
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

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 zoom_to_level, (view, zoom_level));
}


void
fm_directory_view_set_zoom_level (FMDirectoryView *view, int zoom_level)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (!fm_directory_view_supports_zooming (view)) {
		return;
	}

	nautilus_zoomable_set_zoom_level
		(view->details->zoomable,
		 (double) nautilus_get_icon_size_for_zoom_level (zoom_level)
		 / NAUTILUS_ICON_SIZE_STANDARD);
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

	NAUTILUS_CALL_VIRTUAL
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

	return NAUTILUS_CALL_VIRTUAL
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

	return NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 can_zoom_out, (view));
}

GtkWidget *
fm_directory_view_get_background_widget (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return NAUTILUS_CALL_VIRTUAL
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

	return NAUTILUS_CALL_VIRTUAL
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
fm_directory_view_create_links_for_files (FMDirectoryView *view, GList *files)
{
	GList *uris;
	CopyMoveDoneData *copy_move_done_data;
	
        g_assert (FM_IS_DIRECTORY_VIEW (view));
        g_assert (files != NULL);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, append_uri_one, &uris);    

        g_assert (g_list_length (uris) == g_list_length (files));

        copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move (uris, NULL, NULL, GDK_ACTION_LINK, GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	nautilus_g_list_free_deep (uris);
}

static void
fm_directory_view_duplicate_selection (FMDirectoryView *view, GList *files)
{
	GList *uris;
	CopyMoveDoneData *copy_move_done_data;

        g_assert (FM_IS_DIRECTORY_VIEW (view));
        g_assert (files != NULL);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, append_uri_one, &uris);    

        g_assert (g_list_length (uris) == g_list_length (files));
        
        copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move (uris, NULL, NULL, GDK_ACTION_COPY, GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
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
fm_directory_link_type_in_selection (FMDirectoryView *view, NautilusLinkType link_type)
{
	gboolean saw_link;
	GList *selection, *node;
	NautilusFile *file;
	char *uri, *path;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	saw_link = FALSE;

	selection = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));
	for (node = selection; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (selection->data);

		if (!nautilus_file_is_nautilus_link (file)) {
			continue;
		}
		uri = nautilus_file_get_uri (file);
		path = gnome_vfs_get_local_path_from_uri (uri);

		switch (link_type) {
			case NAUTILUS_LINK_TRASH:
				/* It's probably OK that this ignores trash links that
                 	 	* are not local since the trash link we care about is
		 	 	* on the desktop.
		 	 	*/
				saw_link = path != NULL && nautilus_link_local_is_trash_link (path);
				if (saw_link) {
					break;
				}
				break;

			case NAUTILUS_LINK_MOUNT:
				saw_link = path != NULL && nautilus_link_local_is_volume_link (path);
				if (saw_link) {
					break;
				}
				break;
				
			case NAUTILUS_LINK_HOME:
				saw_link = path != NULL && nautilus_link_local_is_home_link (path);
				if (saw_link) {
					break;
				}
				break;

			default:
				break;

		}
		g_free (path);
		g_free (uri);
	}
	
	nautilus_file_list_free (selection);
	
	return saw_link;
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
	if (fm_directory_link_type_in_selection (view, NAUTILUS_LINK_TRASH)) {
		return TRUE;
	}

	if (fm_directory_link_type_in_selection (view, NAUTILUS_LINK_HOME)) {
		return TRUE;
	}

	if (fm_directory_link_type_in_selection (view, NAUTILUS_LINK_MOUNT)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
fm_directory_view_can_move_file_to_trash (FMDirectoryView *view, NautilusFile *file)
{
	/* Return TRUE if we can get a trash directory on the same volume as this file. */
	char *directory;
	GnomeVFSURI *directory_uri;
	GnomeVFSURI *trash_dir_uri;
	gboolean result;

	directory = nautilus_file_get_parent_uri (file);
	if (directory == NULL) {
		return FALSE;
	}
	directory_uri = gnome_vfs_uri_new (directory);

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
	g_free (directory);

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

		prompt = g_strdup_printf (_("\"%s\" cannot be moved to the trash. Do "
					    "you want to delete it immediately?"), file_name);
		g_free (file_name);
	} else {
		if (all) {
			prompt = g_strdup_printf (_("The %d selected items cannot be moved "
						    "to the trash. Do you want to delete them "
						    "immediately?"), uri_count);
		} else {
			prompt = g_strdup_printf (_("%d of the selected items cannot be moved "
						    "to the trash. Do you want to delete those "
						    "%d items immediately?"), uri_count, uri_count);
		}
	}

	dialog = nautilus_yes_no_dialog (
		prompt,
		_("Delete Immediately?"),
		_("Delete"),
		GNOME_STOCK_BUTTON_CANCEL,
		get_containing_window (view));

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
	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH, TRUE)) {
		return TRUE;
	}

	uri_count = g_list_length (uris);
	g_assert (uri_count > 0);

	if (uri_count == 1) {
		file_name = file_name_from_uri ((char *)uris->data);

		prompt = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\" "
					    "from the trash?"), file_name);
		g_free (file_name);
	} else {
		prompt = g_strdup_printf (_("Are you sure you want to permanently delete "
		  			    "the %d selected items from the trash?"), uri_count);
	}

	dialog = nautilus_yes_no_dialog (
		prompt,
		_("Delete From Trash?"),
		_("Delete"),
		GNOME_STOCK_BUTTON_CANCEL,
		get_containing_window (view));

	g_free (prompt);

	return gnome_dialog_run (dialog) == GNOME_OK;
}

static void
fm_directory_view_trash_or_delete_files (FMDirectoryView *view, GList *files)
{
	GList *file_node;
	NautilusFile *file;
	char *file_uri;
	GList *moveable_uris;
	GList *unmoveable_uris;
	GList *in_trash_uris;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (files != NULL);

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
	
	for (file_node = files; file_node != NULL; file_node = file_node->next) {
		file = NAUTILUS_FILE (file_node->data);
		file_uri = nautilus_file_get_uri (file);
		
		if (fm_directory_view_can_move_file_to_trash (view, file)) {
			moveable_uris = g_list_prepend (moveable_uris, file_uri);
		} else if (nautilus_file_is_in_trash (file)) {
			in_trash_uris = g_list_prepend (in_trash_uris, file_uri);
		} else {
			unmoveable_uris = g_list_prepend (unmoveable_uris, file_uri);
		}
	}

	if (moveable_uris != NULL) {
		nautilus_file_operations_move_to_trash (moveable_uris, GTK_WIDGET (view));
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
		NAUTILUS_CALL_VIRTUAL (FM_DIRECTORY_VIEW_CLASS, view, start_renaming_item, (view, target_uri));
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

	fm_directory_view_activate_file (FM_DIRECTORY_VIEW (callback_data), NAUTILUS_FILE (data), TRUE);
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

/**
 * Note that this is used both as a Bonobo menu callback and a signal callback.
 * The first parameter is different in these cases, but we just ignore it anyway.
 */
static void
remove_custom_icons_callback (gpointer ignored, gpointer view)
{
	GList *selection;

	selection = fm_directory_view_get_selection (FM_DIRECTORY_VIEW (view));
	g_list_foreach (selection, remove_custom_icon, NULL);
	nautilus_file_list_free (selection);

        /* Update menus because Remove Custom Icons item has changed state */
	schedule_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
finish_inserting_menu_item (GtkMenu *menu, 
			    GtkWidget *menu_item, 
			    int position, 
			    gboolean sensitive)
{
	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_menu_insert (menu, menu_item, position);
}

static void
finish_appending_menu_item (GtkMenu *menu, GtkWidget *menu_item, gboolean sensitive)
{
	finish_inserting_menu_item (menu, menu_item, -1, sensitive);
}

static void
compute_menu_item_info (FMDirectoryView *directory_view,
			const char *path, 
                        GList *selection,
                        gboolean include_accelerator_underbars,
                        char **return_name, 
                        gboolean *return_sensitivity)
{
	char *name, *stripped;
	int count;

	*return_sensitivity = TRUE;

        if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OPEN) == 0) {
                name = g_strdup (_("_Open"));
                *return_sensitivity = nautilus_g_list_exactly_one_item (selection);
        } else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH) == 0) {
		name = g_strdup (_("Open With"));
		*return_sensitivity = nautilus_g_list_exactly_one_item (selection);
        } else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION) == 0) {
		name = g_strdup (_("Other Application..."));
        } else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER) == 0) {
		name = g_strdup (_("Other Viewer..."));
        } else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW) == 0) {
		count = g_list_length (selection);
		if (count <= 1) {
			name = g_strdup (_("Open in _New Window"));
		} else {
			name = g_strdup_printf (_("Open in %d _New Windows"), count);
		}
		if (nautilus_g_list_exactly_one_item (selection)) {
			/* If the only selected item is launchable, dim out "Open in New Window"
			 * to avoid confusion about how it differs from "Open" in this case (it
			 * doesn't differ; they would do the same thing).
			 */
			*return_sensitivity = !file_is_launchable (NAUTILUS_FILE (selection->data));
		} else {
			*return_sensitivity = selection != NULL;
		}
        } else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER) == 0) {
		name = g_strdup (_("New Folder"));
		*return_sensitivity = fm_directory_view_supports_creating_files (directory_view);
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_TRASH) == 0) {
		if (fm_directory_all_selected_items_in_trash (directory_view)) {
			if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH, TRUE)) {
				name = g_strdup (_("Delete from _Trash..."));
			} else {
				name = g_strdup (_("Delete from _Trash"));
			}
		} else {
			name = g_strdup (_("Move to _Trash"));
		}
		*return_sensitivity = !fm_directory_view_is_read_only (directory_view) && selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE) == 0) {
		name = g_strdup (_("_Duplicate"));
		*return_sensitivity = fm_directory_view_supports_creating_files (directory_view) && selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_CREATE_LINK) == 0) {
		if (selection != NULL && !nautilus_g_list_exactly_one_item (selection)) {
			name = g_strdup (_("Create _Links"));
		} else {
			name = g_strdup (_("Create _Link"));
		}
		*return_sensitivity = fm_directory_view_supports_creating_files (directory_view) && selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES) == 0) {
		/* No ellipses here because this command does not require further
		 * information to be completed.
		 */
		name = g_strdup (_("Show _Properties"));
		*return_sensitivity = selection != NULL && fm_directory_view_supports_properties (directory_view);
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH) == 0) {
		if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_CONFIRM_TRASH, TRUE)) {
			name = g_strdup (_("_Empty Trash..."));
		} else {
			name = g_strdup (_("_Empty Trash"));
		}
		*return_sensitivity =  !nautilus_trash_monitor_is_empty ();
	} else if (strcmp (path, NAUTILUS_MENU_PATH_SELECT_ALL_ITEM) == 0) {
		name = g_strdup (_("_Select All Files"));
		*return_sensitivity = !fm_directory_view_is_empty (directory_view);
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS) == 0) {
                if (nautilus_g_list_more_than_one_item (selection)) {
                        name = g_strdup (_("R_emove Custom Images"));
                } else {
                        name = g_strdup (_("R_emove Custom Image"));
                }
        	*return_sensitivity = files_have_any_custom_images (selection);
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_RESET_BACKGROUND) == 0) {
                name = g_strdup (_("Reset _Background"));
        	*return_sensitivity = nautilus_file_background_is_set 
        		(fm_directory_view_get_background (directory_view));
        } else {
        	name = "";
                g_assert_not_reached ();
        }

	if (!include_accelerator_underbars) {
                stripped = nautilus_str_strip_chr (name, '_');
		g_free (name);
		name = stripped;
        }

	*return_name = name;
}

static void
set_menu_item_path (GtkMenuItem *item, const char *path)
{
	/* set_data_full is unhappy if you give it a destroy_func
	 * and a NULL, even though it would work fine in this case.
	 */
	if (path == NULL) {
		return;
	}

	gtk_object_set_data_full (GTK_OBJECT (item),
				  "path",
				  g_strdup (path),
				  g_free);
}

/* Append a new menu item to a GtkMenu, with the FMDirectoryView *
 * being the callback data.
 */
static void
append_gtk_menu_item (FMDirectoryView *view,
		      GtkMenu *menu,
		      GList *files,
		      const char *menu_path,
		      const char *verb_path,
		      GtkSignalFunc callback)
{
        GtkWidget *menu_item;
        char *label_string;
        gboolean sensitive;

        compute_menu_item_info (view, menu_path, files, FALSE, &label_string, &sensitive);
        menu_item = gtk_menu_item_new_with_label (label_string);
        g_free (label_string);

        set_menu_item_path (GTK_MENU_ITEM (menu_item), verb_path);

        gtk_signal_connect (GTK_OBJECT (menu_item),
                            "activate",
                            callback,
                            view);

        finish_appending_menu_item (menu, menu_item, sensitive);
}

static void
append_selection_menu_subtree (FMDirectoryView *view,
			       GtkMenu *parent_menu,
			       GtkMenu *child_menu,
			       GList *files,
			       const char *path,
			       const char *identifier)
{
        GtkWidget *menu_item;
        char *label_string;
        gboolean sensitive;

        compute_menu_item_info (view, path, files, FALSE, &label_string, &sensitive);
        menu_item = gtk_menu_item_new_with_label (label_string);
        g_free (label_string);

        finish_appending_menu_item (parent_menu, menu_item, sensitive);

	/* Store identifier in item, so we can find this item later */
	set_menu_item_path (GTK_MENU_ITEM (menu_item), identifier);

        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), 
        			   GTK_WIDGET (child_menu));
}

/* fm_directory_view_insert_context_menu_item:
 *
 * Insert a menu item into a context menu for a directory view.
 * 
 * @view: 	The FMDirectoryView in question.
 * @menu: 	The context menu in which to insert the new item.
 * @label: 	The user-visible text to appear in the menu item.
 * @identifier: A string that uniquely distinguishes this item from other
 * 		items in this menu. These can be published so that subclasses
 * 		can locate specific menu items for modification, positioning,
 * 		or removing. Pass NULL if you don't want subclasses to be able
 * 		to discover this item.
 * @position:	The index at which to insert the new item.
 * @callback: 	The function that's called when this item is selected. Note that
 * 		the second parameter (the "callback data") is always @view.
 * @sensitive:	Whether or not this item should be sensitive.
 */
GtkMenuItem *
fm_directory_view_insert_context_menu_item (FMDirectoryView *view, 
					    GtkMenu *menu, 
					    const char *label,
					    const char *identifier,
					    int position,
	       				    void (* callback) (GtkMenuItem *, FMDirectoryView *),
	       				    gboolean sensitive)
{
	GtkWidget *menu_item;
	guint accel_key;
	
	menu_item = gtk_menu_item_new_with_label (label);

	/* Add accelerator */
	accel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menu_item)->child), label);
	if (accel_key != GDK_VoidSymbol)
	{
		gtk_widget_add_accelerator (menu_item, "activate", gtk_accel_group_get_default (), 
					    accel_key, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	}
	
	/* Store command path in item, so we can find this item by command path later */
	set_menu_item_path (GTK_MENU_ITEM (menu_item), identifier);

	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (callback), view);
	finish_inserting_menu_item (menu, menu_item, position, sensitive);

	return GTK_MENU_ITEM (menu_item);
}

static void
fm_directory_view_append_context_menu_item (FMDirectoryView *view, 
					 GtkMenu *menu, 
					 const char *label,
					 const char *path,
	       				 void (* activate_handler) (GtkMenuItem *, FMDirectoryView *),
	       				 gboolean sensitive)
{
	fm_directory_view_insert_context_menu_item 
		(view, 
		 menu, 
		 label, 
		 path,
		 -1, 
		 activate_handler, 
		 sensitive);
}

static void
create_background_context_menu_zoom_items (FMDirectoryView *view, 
					   GtkMenu *menu)
{
	nautilus_gtk_menu_append_separator (menu);
		
	fm_directory_view_append_context_menu_item (view, menu, _("Zoom In"), NULL, zoom_in_callback,
		       fm_directory_view_can_zoom_in (view));
	fm_directory_view_append_context_menu_item (view, menu, _("Zoom Out"), NULL, zoom_out_callback,
		       fm_directory_view_can_zoom_out (view));
	fm_directory_view_append_context_menu_item (view, menu, _("Normal Size"), NULL, zoom_default_callback, TRUE);
}

static void
real_create_background_context_menu_items (FMDirectoryView *view, 
							     GtkMenu *menu)

{
	/* FIXME: This should share code by using compute_menu_item_info,
	 * but can't because of the "use underline for control key shortcut"
	 * hack here.
	 */
	fm_directory_view_append_context_menu_item 
		(view, menu, 
		 _("_New Folder"), 
		 FM_DIRECTORY_VIEW_COMMAND_NEW_FOLDER,
		 GTK_SIGNAL_FUNC (new_folder_callback), 
		 fm_directory_view_supports_creating_files (view));

	if (fm_directory_view_supports_zooming (view)) {
		create_background_context_menu_zoom_items (view, menu);
	}

	nautilus_gtk_menu_append_separator (menu);

	append_gtk_menu_item (view,
			      menu,
			      NULL,
			      FM_DIRECTORY_VIEW_MENU_PATH_RESET_BACKGROUND,
			      FM_DIRECTORY_VIEW_COMMAND_RESET_BACKGROUND,
			      reset_background_callback);
}

static void
launch_application_from_menu_item (GtkMenuItem *menu_item, gpointer callback_data)
{
	ApplicationLaunchParameters *launch_parameters;

	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (callback_data != NULL);

	launch_parameters = (ApplicationLaunchParameters *)callback_data;

	fm_directory_view_launch_application 
		(launch_parameters->application, 
		 launch_parameters->uri, 
		 launch_parameters->directory_view);
}

static void
view_uri_from_menu_item (GtkMenuItem *menu_item, gpointer callback_data)
{
	ViewerLaunchParameters *launch_parameters;

	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (callback_data != NULL);

	launch_parameters = (ViewerLaunchParameters *)callback_data;

	switch_location_and_view (launch_parameters->identifier,
				  launch_parameters->uri,
				  launch_parameters->directory_view);
}

static void
add_application_to_gtk_menu (FMDirectoryView *directory_view,
			     GtkMenu *menu, 
			     GnomeVFSMimeApplication *application, 
			     const char *uri)
{
	GtkWidget *menu_item;
	ApplicationLaunchParameters *launch_parameters;
	char *label_string;

	g_assert (GTK_IS_MENU (menu));

	label_string = g_strdup (application->name);
	menu_item = gtk_menu_item_new_with_label (label_string);
	g_free (label_string);

	launch_parameters = application_launch_parameters_new
		(application, uri, directory_view);

	nautilus_gtk_signal_connect_free_data_custom
		(GTK_OBJECT (menu_item),
		 "activate",
		 launch_application_from_menu_item,
		 launch_parameters,
		 (GtkDestroyNotify) application_launch_parameters_free);

	finish_appending_menu_item (menu, menu_item, TRUE);
}

static void
add_component_to_gtk_menu (FMDirectoryView *directory_view,
			   GtkMenu *menu, 
			   OAF_ServerInfo *component, 
			   const char *uri)
{
	GtkWidget *menu_item;
	NautilusViewIdentifier *identifier;
	ViewerLaunchParameters *launch_parameters;
	char *label;

	g_assert (GTK_IS_MENU (menu));

	identifier = nautilus_view_identifier_new_from_content_view (component);

	label = g_strdup_printf (_("%s Viewer"), identifier->name);
	menu_item = gtk_menu_item_new_with_label (label);
	g_free (label);

	launch_parameters = viewer_launch_parameters_new
		(identifier, uri, directory_view);
	nautilus_view_identifier_free (identifier);

	nautilus_gtk_signal_connect_free_data_custom
		(GTK_OBJECT (menu_item),
		 "activate",
		 view_uri_from_menu_item,
		 launch_parameters,
		 (GtkDestroyNotify) viewer_launch_parameters_free);

	finish_appending_menu_item (menu, menu_item, TRUE);
}

static GtkMenu *
create_open_with_gtk_menu (FMDirectoryView *view, GList *files)
{
 	GtkMenu *open_with_menu;
 	GList *applications, *components;
 	GList *node;
	NautilusFile *file;
 	char *uri;

	open_with_menu = GTK_MENU (gtk_menu_new ());
	gtk_widget_show (GTK_WIDGET (open_with_menu));

	/* This menu is only displayed when there's one selected item. */
	if (!nautilus_g_list_exactly_one_item (files)) {
		monitor_file_for_open_with (view, NULL);
	} else {
		file = NAUTILUS_FILE (files->data);
		
		monitor_file_for_open_with (view, file);

		uri = nautilus_file_get_uri (file);
		
		applications = nautilus_mime_get_short_list_applications_for_file (NAUTILUS_FILE (files->data));
		for (node = applications; node != NULL; node = node->next) {
			add_application_to_gtk_menu (view, open_with_menu, node->data, uri);
		}
		gnome_vfs_mime_application_list_free (applications); 

		append_gtk_menu_item (view,
				      open_with_menu,
			 	      files,
			 	      FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION,
			 	      FM_DIRECTORY_VIEW_COMMAND_OTHER_APPLICATION,
			 	      other_application_callback);

		nautilus_gtk_menu_append_separator (open_with_menu);

		components = nautilus_mime_get_short_list_components_for_file (NAUTILUS_FILE (files->data));
		for (node = components; node != NULL; node = node->next) {
			add_component_to_gtk_menu (view, open_with_menu, node->data, uri);
		}
		gnome_vfs_mime_component_list_free (components); 


		g_free (uri);

		append_gtk_menu_item (view,
				      open_with_menu,
				      files,
			 	      FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER,
			 	      FM_DIRECTORY_VIEW_COMMAND_OTHER_VIEWER,
			 	      other_viewer_callback);
	}

	return open_with_menu;
}

static void
real_create_selection_context_menu_items (FMDirectoryView *view,
							    GtkMenu *menu,
						       	    GList *files)
{
	gboolean link_in_selection;

	/* Check for special links */
	link_in_selection = special_link_in_selection (view);
	
	append_gtk_menu_item (view, menu, files,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN,
			      FM_DIRECTORY_VIEW_COMMAND_OPEN,
			      open_callback);
	append_gtk_menu_item (view, menu, files,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW,
			      FM_DIRECTORY_VIEW_COMMAND_OPEN_IN_NEW_WINDOW,
			      open_in_new_window_callback);
	append_selection_menu_subtree (view, menu, 
				       create_open_with_gtk_menu (view, files), files,
				       FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH,
				       FM_DIRECTORY_VIEW_COMMAND_OPEN_WITH);

	nautilus_gtk_menu_append_separator (menu);

	/* Don't add item if Trash link is in selection */
	if (!link_in_selection) {
		append_gtk_menu_item (view, menu, files,
				      FM_DIRECTORY_VIEW_MENU_PATH_TRASH,
				      FM_DIRECTORY_VIEW_COMMAND_TRASH,
				      trash_callback);
	}

	if (!link_in_selection) {
		append_gtk_menu_item (view, menu, files,
				      FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE,
				      FM_DIRECTORY_VIEW_COMMAND_DUPLICATE,
				      duplicate_callback);
		append_gtk_menu_item (view, menu, files,
				      FM_DIRECTORY_VIEW_MENU_PATH_CREATE_LINK,
				      FM_DIRECTORY_VIEW_COMMAND_CREATE_LINK,
				      create_link_callback);
	}
	append_gtk_menu_item (view, menu, files,
			      FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES,
			      FM_DIRECTORY_VIEW_COMMAND_SHOW_PROPERTIES,
			      open_properties_window_callback);
        append_gtk_menu_item (view, menu, files,
			      FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS,
			      FM_DIRECTORY_VIEW_COMMAND_REMOVE_CUSTOM_ICONS,
			      remove_custom_icons_callback);
}

static void
bonobo_launch_application_callback (BonoboUIComponent *component, gpointer callback_data, const char *path)
{
	ApplicationLaunchParameters *launch_parameters;
	
	launch_parameters = (ApplicationLaunchParameters *) callback_data;
	fm_directory_view_launch_application 
		(launch_parameters->application,
		 launch_parameters->uri,
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
add_open_with_program_menu_item (BonoboUIComponent *ui,
				 const char *parent_path,
				 const char *label,
				 const char *tip,
				 int index,
				 gpointer callback,
				 gpointer callback_data,
				 GDestroyNotify destroy_notify)
{
	char *escaped_label, *verb_name, *item_path;
	char *item_id;
	
	escaped_label = nautilus_str_double_underscores (label);

	item_id = g_strdup_printf ("program%d", index);
	nautilus_bonobo_add_menu_item (ui, item_id,
				       parent_path,
				       escaped_label, 
				       NULL);
	g_free (escaped_label);

	item_path = g_strconcat (parent_path, "/", item_id, NULL);
	g_free (item_id);

	nautilus_bonobo_set_tip (ui, item_path, tip);
	g_free (item_path);
	
	verb_name = nautilus_bonobo_get_menu_item_verb_name (label);
	bonobo_ui_component_add_verb_full (ui, verb_name, callback, callback_data, destroy_notify);	   
	g_free (verb_name);
}				 

static void
add_open_with_app_bonobo_menu_item (BonoboUIComponent *ui,
				    const char *label,
				    int index,
				    gpointer callback,
				    gpointer callback_data,
				    GDestroyNotify destroy_notify)
{
	char *tip;
	
	tip = g_strdup_printf (_("Use \"%s\" to open the selected item"), label);
	add_open_with_program_menu_item (ui, 
					 FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER,
					 label,
					 tip,
					 index,
					 callback,
					 callback_data,
					 destroy_notify);
	g_free (tip);					 
}

static void
add_open_with_viewer_bonobo_menu_item (BonoboUIComponent *ui,
				       const char *label,
				       int index,
				       gpointer callback,
				       gpointer callback_data,
				       GDestroyNotify destroy_notify)
{
	char *tip;
	
	tip = g_strdup_printf (_("Use \"%s\" to view the selected item"), label);
	add_open_with_program_menu_item (ui, 
					 FM_DIRECTORY_VIEW_MENU_PATH_VIEWERS_PLACEHOLDER,
					 label,
					 tip,
					 index,
					 callback,
					 callback_data,
					 destroy_notify);
	g_free (tip);
}

static void
add_application_to_bonobo_menu (FMDirectoryView *directory_view,
				GnomeVFSMimeApplication *application, 
				const char *uri,
				int index)
{
	ApplicationLaunchParameters *launch_parameters;

	launch_parameters = application_launch_parameters_new 
		(application, uri, directory_view);

	add_open_with_app_bonobo_menu_item (directory_view->details->ui, 
					    application->name,
					    index,
					    bonobo_launch_application_callback,
					    launch_parameters,
					    (GDestroyNotify) application_launch_parameters_free);
}

static void
add_component_to_bonobo_menu (FMDirectoryView *directory_view,
			      OAF_ServerInfo *content_view, 
			      const char *uri,
			      int index)
{
	NautilusViewIdentifier *identifier;
	ViewerLaunchParameters *launch_parameters;
	char *label;
	
	identifier = nautilus_view_identifier_new_from_content_view (content_view);
	launch_parameters = viewer_launch_parameters_new (identifier, uri, directory_view);
	nautilus_view_identifier_free (identifier);

	label = g_strdup_printf (_("%s Viewer"),
				 launch_parameters->identifier->name);

	add_open_with_viewer_bonobo_menu_item (directory_view->details->ui, 
					       label,
					       index, 
					       bonobo_open_location_with_viewer_callback, 
					       launch_parameters,
					       (GDestroyNotify) viewer_launch_parameters_free);
	g_free (label);
}


static void
update_one_menu_item (FMDirectoryView *view,
		      GList *selection,
		      const char *menu_path,
		      const char *verb_path)
{
	char *label_string;
	gboolean sensitive;

        compute_menu_item_info (view, menu_path, selection, TRUE, &label_string, &sensitive);

	nautilus_bonobo_set_sensitive (view->details->ui, verb_path, sensitive);
	nautilus_bonobo_set_label (view->details->ui, menu_path, label_string);

	g_free (label_string);
}

static void
reset_bonobo_trash_delete_menu (FMDirectoryView *view, GList *selection)
{
	if (fm_directory_all_selected_items_in_trash (view)) {
		nautilus_bonobo_set_description (view->details->ui, 
						 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
						 _("Delete all selected items permanently"));
		nautilus_bonobo_set_accelerator (view->details->ui, 
						 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
						 "");
	} else {
		nautilus_bonobo_set_description (view->details->ui, 
						 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
						 _("Move all selected items to the Trash"));
		nautilus_bonobo_set_accelerator (view->details->ui, 
						 FM_DIRECTORY_VIEW_MENU_PATH_TRASH, 
		/* NOTE to translators: DO NOT translate "Control*" part, it is a parsed string
		   Only change the "t" part to something that makes sense in your language.
		   This string defines Ctrl-T as "Move all selected items to the Trash" */
						 _("*Control*t"));
	}
	
	update_one_menu_item (view, selection, 
			      FM_DIRECTORY_VIEW_MENU_PATH_TRASH,
			      FM_DIRECTORY_VIEW_COMMAND_TRASH);
}

static void
reset_bonobo_open_with_menu (FMDirectoryView *view, GList *selection)
{
	GList *applications, *components, *node;
	NautilusFile *file;
	char *uri;
	int index;
	
	/* Clear any previous inserted items in the applications and viewers placeholders */
	nautilus_bonobo_remove_menu_items_and_verbs 
		(view->details->ui, FM_DIRECTORY_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_verbs 
		(view->details->ui, FM_DIRECTORY_VIEW_MENU_PATH_VIEWERS_PLACEHOLDER);
	
	/* This menu is only displayed when there's one selected item. */
	if (!nautilus_g_list_exactly_one_item (selection)) {
		monitor_file_for_open_with (view, NULL);
	} else {
		file = NAUTILUS_FILE (selection->data);
		
		monitor_file_for_open_with (view, file);

		uri = nautilus_file_get_uri (file);
		
		applications = nautilus_mime_get_short_list_applications_for_file (NAUTILUS_FILE (selection->data));
		for (node = applications, index = 0; node != NULL; node = node->next, index++) {
			add_application_to_bonobo_menu (view, node->data, uri, index);
		}
		gnome_vfs_mime_application_list_free (applications); 
		
		components = nautilus_mime_get_short_list_components_for_file (NAUTILUS_FILE (selection->data));
		for (node = components, index = 0; node != NULL; node = node->next, index++) {
			add_component_to_bonobo_menu (view, node->data, uri, index);
		}
		gnome_vfs_mime_component_list_free (components); 

		g_free (uri);
	}

	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH);
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
		BONOBO_UI_VERB ("New Folder", (BonoboUIVerbFn)new_folder_callback),
		BONOBO_UI_VERB ("Open", (BonoboUIVerbFn)open_callback),
		BONOBO_UI_VERB ("OpenNew", (BonoboUIVerbFn)open_in_new_window_callback),
		BONOBO_UI_VERB ("OtherApplication", (BonoboUIVerbFn)other_application_callback),
		BONOBO_UI_VERB ("OtherViewer", (BonoboUIVerbFn)other_viewer_callback),
		BONOBO_UI_VERB ("Show Properties", (BonoboUIVerbFn)open_properties_window_callback),
		BONOBO_UI_VERB ("Trash", (BonoboUIVerbFn)trash_callback),
		BONOBO_UI_VERB ("Duplicate", (BonoboUIVerbFn)duplicate_callback),
		BONOBO_UI_VERB ("Create Link", (BonoboUIVerbFn)create_link_callback),
		BONOBO_UI_VERB ("Empty Trash", bonobo_menu_empty_trash_callback),
		BONOBO_UI_VERB ("Select All", bonobo_menu_select_all_callback),
		BONOBO_UI_VERB ("Remove Custom Icons", (BonoboUIVerbFn)remove_custom_icons_callback),
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

}

static void
real_update_menus (FMDirectoryView *view)
{
	GList *selection;
	
	selection = fm_directory_view_get_selection (view);

	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER,
			      FM_DIRECTORY_VIEW_COMMAND_NEW_FOLDER);
	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN,
			      FM_DIRECTORY_VIEW_COMMAND_OPEN);
	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW,
			      FM_DIRECTORY_VIEW_COMMAND_OPEN_IN_NEW_WINDOW);

	reset_bonobo_open_with_menu (view, selection);
	reset_bonobo_trash_delete_menu (view, selection);

	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE,
			      FM_DIRECTORY_VIEW_COMMAND_DUPLICATE);
	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_CREATE_LINK,
			      FM_DIRECTORY_VIEW_COMMAND_CREATE_LINK);
	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES,
			      FM_DIRECTORY_VIEW_COMMAND_SHOW_PROPERTIES);
	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH,
			      FM_DIRECTORY_VIEW_COMMAND_EMPTY_TRASH);
	update_one_menu_item (view, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS,
			      FM_DIRECTORY_VIEW_COMMAND_REMOVE_CUSTOM_ICONS);

	update_one_menu_item (view, selection,
			      NAUTILUS_MENU_PATH_SELECT_ALL_ITEM,
			      NAUTILUS_COMMAND_SELECT_ALL);

	nautilus_file_list_free (selection);
}

static GtkMenu *
create_selection_context_menu (FMDirectoryView *view) 
{
	GtkMenu *menu;
	GList *selected_files;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	selected_files = fm_directory_view_get_selection (view);

	/* We've seen this happen in at least bugzilla.eazel.com 3322 */
	g_return_val_if_fail (selected_files != NULL, NULL);

	menu = GTK_MENU (gtk_menu_new ());

	/* Attach selection to menu, and free it when menu is freed.
	 * This lets menu item callbacks hold onto the files parameter.
	 */
	gtk_object_set_data_full (GTK_OBJECT (menu),
				  "selected_items",
				  selected_files,
				  (GtkDestroyNotify) nautilus_file_list_free);

	gtk_signal_emit (GTK_OBJECT (view),
			 signals[CREATE_SELECTION_CONTEXT_MENU_ITEMS], 
			 menu, selected_files);		
	
	return menu;
}

/**
 * fm_directory_view_create_background_context_menu_items:
 *
 * Add background menu items (i.e., those not dependent on a particular file)
 * to a context menu.
 * @view: An FMDirectoryView.
 * @menu: The menu being constructed. Could be a background menu or an item-specific
 * menu, because the background items are present in both.
 * 
 **/
static void
fm_directory_view_create_background_context_menu_items (FMDirectoryView *view,
							GtkMenu *menu)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[CREATE_BACKGROUND_CONTEXT_MENU_ITEMS], 
			 menu);
}


static GtkMenu *
create_background_context_menu (FMDirectoryView *view)
{
	GtkMenu *menu;

	menu = GTK_MENU (gtk_menu_new ());
	fm_directory_view_create_background_context_menu_items (view, menu);
	
	return menu;
}

/**
 * fm_directory_view_pop_up_selection_context_menu
 *
 * Pop up a context menu appropriate to the selected items at the last right click location.
 * @view: FMDirectoryView of interest.
 * @file: The model object for which a menu should be popped up.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/
void 
fm_directory_view_pop_up_selection_context_menu  (FMDirectoryView *view)
{
	GtkMenu *menu;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	menu = create_selection_context_menu (view);
	if (menu != NULL) {
		nautilus_pop_up_context_menu (menu,
					      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
					      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
					      0);
	}
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
fm_directory_view_pop_up_background_context_menu  (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* work in progress */
	if (TRUE) {
		nautilus_pop_up_context_menu (create_background_context_menu (view),
					      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
					      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
					      0);
	} else {
		nautilus_pop_up_context_menu (create_popup_menu 
						(view, FM_DIRECTORY_VIEW_POPUP_PATH_BACKGROUND),
					      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
					      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
					      0);
	}
}

static void
schedule_update_menus (FMDirectoryView *view) 
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	/* Make sure we haven't already destroyed it */
	g_assert (view->details->nautilus_view != NULL);

	view->details->menu_states_untrustworthy = TRUE;
	
	if (view->details->menus_merged
	    && view->details->update_menus_idle_id == 0) {
		view->details->update_menus_idle_id
			= gtk_idle_add (update_menus_idle_callback, view);
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

	/* Schedule a display of the new selection. */
	if (view->details->display_selection_idle_id == 0)
		view->details->display_selection_idle_id
			= gtk_idle_add (display_selection_info_idle_callback,
					view);

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
	return !nautilus_file_is_directory (file) 
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
					    "Do you want to put this link in the trash?"));
	} else {
		prompt = g_strdup_printf (_("This link can't be used, because its target \"%s\" doesn't exist. "
				 	    "Do you want to put this link in the trash?"),
					  target_path);
	}

	dialog = nautilus_yes_no_dialog (prompt,
					 _("Broken Link"),
					 _("Throw Away"),
					 GNOME_STOCK_BUTTON_CANCEL,
					 get_containing_window (view));

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
		fm_directory_view_trash_or_delete_files (view, &file_as_list);
	}

	g_free (target_path);
	g_free (prompt);
}

typedef struct {
	FMDirectoryView *view;
	NautilusFile *file;
	gboolean use_new_window;
} ActivateParameters;

static void
activate_callback (NautilusFile *file, gpointer callback_data)
{
	ActivateParameters *parameters;
	FMDirectoryView *view;
	char *uri, *command, *executable_path;
	GnomeVFSMimeActionType action_type;
	GnomeVFSMimeApplication *application;
	gboolean performed_special_handling;

	parameters = callback_data;

	view = FM_DIRECTORY_VIEW (parameters->view);

	uri = nautilus_file_get_activation_uri (file);

	performed_special_handling = FALSE;

	/* Note that we check for FILE_TYPE_SYMBOLIC_LINK only here,
	 * not specifically for broken-ness, because the file type
	 * will be the target's file type in the non-broken case.
	 */
	if (nautilus_file_is_broken_symbolic_link (file)) {
		report_broken_symbolic_link (view, file);
		performed_special_handling = TRUE;
	} else if (nautilus_istr_has_prefix (uri, "command:")) {
		/* don't allow command execution from remote uris to partially mitigate
		 * the security risk of executing arbitrary commands.  We want to
		 * further constrain this before 1.0, as expressed bug 2390 */
		
		if (!nautilus_file_is_local (file)) {
			nautilus_error_dialog (_("Sorry, but you can't execute commands from a remote site due to security considerations."), 
					       _("Can't execute remote links"), NULL);
			performed_special_handling = TRUE;
		} else {
			/* FIXME bugzilla.eazel.com 2390: Quite a security hole here. */
			command = g_strconcat (uri + 8, " &", NULL);
			system (command);
			g_free (command);
			performed_special_handling = TRUE;
		}
	} else if (file_is_launchable (file)) {
		/* FIXME bugzilla.eazel.com 2391: This should check if
		 * the activation URI points to something launchable,
		 * not the original file. Also, for symbolic links we
		 * need to check the X bit on the target file, not on
		 * the original.
		 */
		/* Launch executables to activate them. */
		/* FIXME bugzilla.eazel.com 1773: This is a lame way
		 * to run command-line tools, since there's no
		 * terminal for the output.
		 */
		executable_path = gnome_vfs_get_local_path_from_uri (uri);

		/* Non-local executables don't get launched. They fall through
		 * and act like non-executables.
		 */
		if (executable_path != NULL) {
			nautilus_launch_application_from_command (executable_path, NULL);
			g_free (executable_path);
			performed_special_handling = TRUE;
		}
	}

	if (!performed_special_handling) {
		action_type = nautilus_mime_get_default_action_type_for_file (file);
		application = nautilus_mime_get_default_application_for_file (file);

		/* We need to check for the case of having
		 * GNOME_VFS_MIME_ACTION_TYPE_APPLICATION as the
		 * action but having a NULL application returned.
		 */
		if (action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION && application == NULL) {			
			action_type = GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;
		}
		
		if (action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
			fm_directory_view_launch_application (application, uri, view);
		} else {
			/* If the action type is unspecified, treat it like
			 * the component case. This is most likely to happen
			 * (only happens?) when there are no registered
			 * viewers or apps, or there are errors in the
			 * mime.keys files.
			 */
			g_assert (action_type == GNOME_VFS_MIME_ACTION_TYPE_NONE
				  || action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
			
			fm_directory_view_switch_location
				(view, uri, parameters->use_new_window);
		}
		
		if (application != NULL) {
			gnome_vfs_mime_application_free (application);
		}
	}

	g_free (uri);
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
				 gboolean use_new_window)
{
	ActivateParameters *parameters;
	GList *attributes;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	/* Might have to read some of the file to activate it. */
	attributes = nautilus_mime_actions_get_full_file_attributes ();
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FILE_TYPE);
	parameters = g_new (ActivateParameters, 1);
	parameters->view = view;
	parameters->file = file;
	parameters->use_new_window = use_new_window;

	nautilus_file_call_when_ready
		(file, attributes, activate_callback, parameters);

	/* FIXME bugzilla.eazel.com 2392: Need a timed wait here too. */

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
			 ||  nautilus_preferences_get_boolean 
			 	(NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW, FALSE);

	if (!use_new_window || fm_directory_view_confirm_multiple_windows (view, file_count)) {
		for (node = files; node != NULL; node = node->next) {  	
			fm_directory_view_activate_file 
				(view, node->data, use_new_window);
		}
	}
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
		NautilusDirectory *directory,
		gboolean force_reload)
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

	view->details->force_reload = force_reload;

	/* FIXME: In theory, we also need to monitor here (as well as
         * doing a call when ready), in case external forces change
         * the directory's file metadata.
	 */
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
	nautilus_file_call_when_ready
		(view->details->directory_as_file,
		 attributes,
		 metadata_ready_callback, view);
	g_list_free (attributes);
}

static void
finish_loading_uri (FMDirectoryView *view)
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
					     view,
					     attributes,
					     view->details->force_reload);
	view->details->force_reload = FALSE;

	g_list_free (attributes);
}

static void
metadata_ready_callback (NautilusFile *file,
			 gpointer callback_data)
{
	FMDirectoryView *view;

	view = callback_data;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (view->details->directory_as_file == file);

	finish_loading_uri (view);
}

NautilusStringList *
fm_directory_view_get_emblem_names_to_exclude (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return NAUTILUS_CALL_VIRTUAL
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

	NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 merge_menus, (view));
}

static void
disconnect_handler (FMDirectoryView *view, int *id)
{
	if (*id != 0) {
		gtk_signal_disconnect (GTK_OBJECT (view->details->model), *id);
		*id = 0;
	}
}

static void
disconnect_model_handlers (FMDirectoryView *view)
{
	disconnect_handler (view, &view->details->files_added_handler_id);
	disconnect_handler (view, &view->details->files_changed_handler_id);
	if (view->details->model != NULL) {
		nautilus_directory_file_monitor_remove (view->details->model, view);
		nautilus_file_cancel_call_when_ready (view->details->directory_as_file,
						      metadata_ready_callback,
						      view);
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

	NAUTILUS_CALL_VIRTUAL
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

	NAUTILUS_CALL_VIRTUAL
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
 * fm_directory_view_reveal_selection:
 *
 * Scroll as necessary to reveal the selected items.
 **/
void
fm_directory_view_reveal_selection (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	NAUTILUS_CALL_VIRTUAL
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
	display_pending_files (view);
	if (view->details->model != NULL) {
		nautilus_directory_file_monitor_remove (view->details->model, view);
	}
	done_loading (view);
}

gboolean
fm_directory_view_is_read_only (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 is_read_only, (view));
}

gboolean
fm_directory_view_is_empty (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return NAUTILUS_CALL_VIRTUAL
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

	return NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 supports_creating_files, (view));
}

gboolean
fm_directory_view_accepts_dragged_files (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return NAUTILUS_CALL_VIRTUAL
		(FM_DIRECTORY_VIEW_CLASS, view,
		 accepts_dragged_files, (view));
}

static gboolean
showing_trash_directory (FMDirectoryView *view)
{
	return nautilus_file_is_in_trash (fm_directory_view_get_directory_as_file (view));
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

	return NAUTILUS_CALL_VIRTUAL
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

	return NAUTILUS_CALL_VIRTUAL
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

	NAUTILUS_CALL_VIRTUAL
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

	/* Hidden files are never shown on the desktop */
	if (FM_IS_DESKTOP_ICON_VIEW (directory_view)) {
		directory_view->details->show_hidden_files = FALSE;
		directory_view->details->show_backup_files = FALSE;
		return;
	}
	
	directory_view->details->show_hidden_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES, FALSE);
	
	directory_view->details->show_backup_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES, FALSE);
	
	/* Reload the current uri so that the filtering changes take place. */
	if (directory_view->details->model != NULL) {
		load_directory (directory_view,
				directory_view->details->model,
				FALSE);
	}
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
				   GdkPoint *relative_item_points,
				   const char *target_dir,
				   int copy_action,
				   int x,
				   int y,
				   FMDirectoryView *view)
{
	int index, count;
	CopyMoveDoneData *copy_move_done_data;
	
	if (relative_item_points) {
		/* add the drop location to the icon offsets */
		count = g_list_length ((GList *)item_uris);
		for (index = 0; index < count; index++ ){
			relative_item_points[index].x += x;
			relative_item_points[index].y += y;
		}
	}
	copy_move_done_data = pre_copy_move (view);
	nautilus_file_operations_copy_move
		(item_uris, relative_item_points, 
		 target_dir, copy_action, GTK_WIDGET (view),
		 copy_move_done_callback, copy_move_done_data);
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

static gboolean
menu_item_matches_path (GtkMenuItem *item, const char *path)
{
	return nautilus_strcmp ((const char *) gtk_object_get_data (GTK_OBJECT (item), "path"),
				path) == 0;
}

/**
 * fm_directory_view_get_context_menu_index:
 * 
 * Return index of specified menu item in the passed-in context menu.
 * Return -1 if item is not found. This is intended for subclasses to 
 * use to properly position new items in the context menu.
 * 
 * @menu: A GtkMenu, either the item-specific or background context menu
 * as passed to _create_selection_context_menu_items or
 * _create_background_context_menu_items.
 * @verb_path: command name (e.g. "/commands/Open") whose index in @menu should be returned.
 */
int
fm_directory_view_get_context_menu_index (GtkMenu *menu, const char *verb_path)
{
	GList *children, *node;
	GtkMenuItem *menu_item;
	int index;
	int result;

	g_return_val_if_fail (GTK_IS_MENU (menu), -1);
	g_return_val_if_fail (verb_path != NULL, -1);

	children = gtk_container_children (GTK_CONTAINER (menu));
	result = -1;
	
	for (node = children, index = 0; node != NULL; node = node->next, ++index) {
		menu_item = GTK_MENU_ITEM (node->data);
		if (menu_item_matches_path (menu_item, verb_path)) {
			result = index;
			break;
		}
	}

	g_list_free (children);

	return result;
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
	NautilusFile *old_file;
	GList *attributes;

	/* Quick out when not changing. */
	old_file = view->details->file_monitored_for_open_with;
	if (old_file == file) {
		return;
	}

	/* Point at the new file. */
	nautilus_file_ref (file);
	view->details->file_monitored_for_open_with = file;

	/* Stop monitoring the old file. */
	if (old_file != NULL) {
		nautilus_file_monitor_remove (old_file, view);
		nautilus_file_unref (old_file);
	}

	/* Start monitoring the new file. */
	if (file != NULL) {
		attributes = nautilus_mime_actions_get_full_file_attributes ();
		nautilus_file_monitor_add (file, view, attributes);
		g_list_free (attributes);
	}
}
