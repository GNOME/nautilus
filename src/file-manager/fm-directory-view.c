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
 */

#include <config.h>
#include "fm-directory-view.h"

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkcheckmenuitem.h>

#include <bonobo/bonobo-control.h>
#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-directory-list.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus/nautilus-alloc.h>
#include <libnautilus/nautilus-global-preferences.h>
#include <libnautilus/nautilus-gtk-extensions.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-icon-factory.h>
#include <libnautilus/nautilus-string.h>
#include <libnautilus/nautilus-zoomable.h>

#include "fm-properties-window.h"

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

/* Paths to use when creating & referring to bonobo menu items */
#define MENU_PATH_OPEN                      "/File/Open"
#define MENU_PATH_OPEN_IN_NEW_WINDOW        "/File/OpenNew"
#define MENU_PATH_DELETE                    "/File/Delete"
#define MENU_PATH_SELECT_ALL                "/Edit/Select All"
#define MENU_PATH_SET_PROPERTIES            "/Edit/Set Properties"

enum
{
	ADD_FILE,
	APPEND_BACKGROUND_CONTEXT_MENU_ITEMS,
	APPEND_SELECTION_CONTEXT_MENU_ITEMS,
	BEGIN_ADDING_FILES,
	BEGIN_LOADING,
	CLEAR,
	DONE_ADDING_FILES,
	FILE_CHANGED,
	REMOVE_FILE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _FMDirectoryViewDetails
{
	NautilusContentViewFrame *view_frame;
	NautilusZoomable         *zoomable;

	NautilusDirectory *model;
	
	guint display_selection_idle_id;
	
	guint display_pending_timeout_id;
	guint display_pending_idle_id;
	
	guint files_added_handler_id;
	guint files_removed_handler_id;
	guint files_changed_handler_id;
	
	GList *pending_files_added;
	GList *pending_files_removed;
	GList *pending_files_changed;

	gboolean loading;

	gint user_level;
	gboolean use_new_window;
};

/* forward declarations */
static void           delete_cb                                                   (GtkMenuItem             *item,
										   GList                   *files);
static int            display_selection_info_idle_cb                              (gpointer                 data);
static void           display_selection_info                                      (FMDirectoryView         *view);
static void           fm_directory_view_initialize_class                          (FMDirectoryViewClass    *klass);
static void           fm_directory_view_initialize                                (FMDirectoryView         *view);
static void           fm_directory_view_delete_with_confirm                       (FMDirectoryView         *view,
										   GList                   *files);
static void           fm_directory_view_destroy                                   (GtkObject               *object);
static void	      fm_directory_view_activate_file_internal 			  (FMDirectoryView 	   *view, 
				 	  					   NautilusFile 	   *file,
				 	  					   gboolean 	            use_new_window);
static void           fm_directory_view_append_background_context_menu_items      (FMDirectoryView         *view,
										   GtkMenu                 *menu);
static void           fm_directory_view_merge_menus                               (FMDirectoryView         *view);
static void           fm_directory_view_real_append_background_context_menu_items (FMDirectoryView         *view,
										   GtkMenu                 *menu);
static void           fm_directory_view_real_append_selection_context_menu_items  (FMDirectoryView         *view,
										   GtkMenu                 *menu,
										   GList                   *files);
static void           fm_directory_view_real_merge_menus                          (FMDirectoryView         *view);
static void           fm_directory_view_real_update_menus                         (FMDirectoryView         *view);
static GtkMenu *      create_selection_context_menu                               (FMDirectoryView         *view);
static GtkMenu *      create_background_context_menu                              (FMDirectoryView         *view);
static BonoboControl *get_bonobo_control                                          (FMDirectoryView         *view);
static void           stop_location_change_cb                                     (NautilusViewFrame       *view_frame,
										   FMDirectoryView         *directory_view);
static void           notify_location_change_cb                                   (NautilusViewFrame       *view_frame,
										   Nautilus_NavigationInfo *nav_context,
										   FMDirectoryView         *directory_view);
static void           open_cb                                                     (GtkMenuItem             *item,
										   GList                   *files);
static void           open_in_new_window_cb                                       (GtkMenuItem             *item,
										   GList                   *files);
static void           open_one_in_new_window                                      (gpointer                 data,
										   gpointer                 user_data);
static void           select_all_cb                                               (GtkMenuItem             *item,
										   FMDirectoryView         *directory_view);
static void           zoom_in_cb                                                  (GtkMenuItem             *item,
										   FMDirectoryView         *directory_view);
static void           zoom_out_cb                                                 (GtkMenuItem             *item,
										   FMDirectoryView         *directory_view);
static void           zoomable_zoom_in_cb                                         (NautilusZoomable        *zoomable,
										   FMDirectoryView         *directory_view);
static void           zoomable_zoom_out_cb                                        (NautilusZoomable        *zoomable,
										   FMDirectoryView         *directory_view);
static void           schedule_idle_display_of_pending_files                      (FMDirectoryView         *view);
static void           unschedule_idle_display_of_pending_files                    (FMDirectoryView         *view);
static void           schedule_timeout_display_of_pending_files                   (FMDirectoryView         *view);
static void           unschedule_timeout_display_of_pending_files                 (FMDirectoryView         *view);
static void           unschedule_display_of_pending_files                         (FMDirectoryView         *view);
static void           disconnect_model_handlers                                   (FMDirectoryView         *view);
static void           user_level_changed_callback                                 (NautilusPreferences     *preferences,
										   const char              *name,
										   NautilusPreferencesType  type,
										   gconstpointer            value,
										   gpointer                 user_data);
static void           use_new_window_changed_callback                             (NautilusPreferences     *preferences,
										   const char              *name,
										   NautilusPreferencesType  type,
										   gconstpointer            value,
										   gpointer                 user_data);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_file)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, bump_zoom_level)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_in)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_out)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, file_changed)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, remove_file)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, select_all)

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
	signals[REMOVE_FILE] =
		gtk_signal_new ("remove_file",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, remove_file),
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
	signals[APPEND_SELECTION_CONTEXT_MENU_ITEMS] =
		gtk_signal_new ("append_selection_context_menu_items",
       				GTK_RUN_FIRST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, append_selection_context_menu_items),
		    		nautilus_gtk_marshal_NONE__BOXED_BOXED,
		    		GTK_TYPE_NONE, 2, GTK_TYPE_BOXED, GTK_TYPE_BOXED);
	signals[APPEND_BACKGROUND_CONTEXT_MENU_ITEMS] =
		gtk_signal_new ("append_background_context_menu_items",
       				GTK_RUN_FIRST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, append_background_context_menu_items),
		    		gtk_marshal_NONE__BOXED,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);

	klass->append_selection_context_menu_items = fm_directory_view_real_append_selection_context_menu_items;
	klass->append_background_context_menu_items = fm_directory_view_real_append_background_context_menu_items;
        klass->merge_menus = fm_directory_view_real_merge_menus;
        klass->update_menus = fm_directory_view_real_update_menus;

	/* Function pointers that subclasses must override */

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, add_file);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, bump_zoom_level);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_in);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_out);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, clear);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, file_changed);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selection);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, remove_file);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, select_all);
}

static void
bonobo_menu_open_cb (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless exactly
         * one item is selected.
         */
        g_assert (g_list_length(selection) == 1);

	fm_directory_view_activate_file_internal (view, 
	                                  	  NAUTILUS_FILE (selection->data), 
	                                  	  FALSE);        

	nautilus_file_list_free (selection);
}

static void
bonobo_menu_open_in_new_window_cb (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless at least
         * one item is selected.
         */
        g_assert (g_list_length(selection) > 0);

	g_list_foreach (selection, open_one_in_new_window, view);

	nautilus_file_list_free (selection);
}

static void
bonobo_menu_delete_cb (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless at least
         * one item is selected.
         */
        g_assert (g_list_length(selection) > 0);

        fm_directory_view_delete_with_confirm (view, selection);

        nautilus_file_list_free (selection);
}

static BonoboControl *
get_bonobo_control (FMDirectoryView *view)
{
        return BONOBO_CONTROL (nautilus_view_frame_get_bonobo_control
			       (NAUTILUS_VIEW_FRAME (view->details->view_frame)));
}

static void
bonobo_control_activate_cb (BonoboObject *control, gboolean state, gpointer user_data)
{
        FMDirectoryView *view;
        BonoboUIHandler *local_ui_handler;

        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);

        local_ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (control));

        if (state) {
                bonobo_ui_handler_set_container (local_ui_handler, 
                                                 bonobo_control_get_remote_ui_handler (BONOBO_CONTROL (control)));
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

static void
fm_directory_view_initialize (FMDirectoryView *directory_view)
{
	directory_view->details = g_new0 (FMDirectoryViewDetails, 1);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (directory_view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);

	directory_view->details->view_frame = NAUTILUS_CONTENT_VIEW_FRAME
		(nautilus_content_view_frame_new (GTK_WIDGET (directory_view)));

	directory_view->details->zoomable = 
		nautilus_zoomable_new_from_bonobo_control (BONOBO_OBJECT 
							   (directory_view->details->view_frame),
							   .25, 4.0, FALSE);		

	gtk_signal_connect (GTK_OBJECT (directory_view->details->view_frame), 
			    "stop_location_change",
			    GTK_SIGNAL_FUNC (stop_location_change_cb),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (notify_location_change_cb), 
			    directory_view);

        gtk_signal_connect (GTK_OBJECT (get_bonobo_control (directory_view)),
                            "activate",
                            bonobo_control_activate_cb,
                            directory_view);

	gtk_signal_connect (GTK_OBJECT (directory_view->details->zoomable), 
			    "zoom_in",
			    zoomable_zoom_in_cb,
			    directory_view);

	gtk_signal_connect (GTK_OBJECT (directory_view->details->zoomable), 
			    "zoom_out", 
			    zoomable_zoom_out_cb,
			    directory_view);

	gtk_widget_show (GTK_WIDGET (directory_view));

	/* Obtain the user level for filtering */
	directory_view->details->user_level = 
		nautilus_preferences_get_enum (nautilus_preferences_get_global_preferences (),
					       NAUTILUS_PREFERENCES_USER_LEVEL);

	/* Keep track of subsequent user level changes so that we dont have to query
	 * preferences continually */
	nautilus_preferences_add_callback (nautilus_preferences_get_global_preferences (),
					   NAUTILUS_PREFERENCES_USER_LEVEL,
					   user_level_changed_callback,
					   directory_view);

	directory_view->details->use_new_window =
		nautilus_preferences_get_boolean (nautilus_preferences_get_global_preferences (),
						  NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW);

	nautilus_preferences_add_callback (nautilus_preferences_get_global_preferences (),
					   NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
					   use_new_window_changed_callback,
					   directory_view);	
}

static void
fm_directory_view_destroy (GtkObject *object)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (object);

	nautilus_preferences_remove_callback (nautilus_preferences_get_global_preferences (),
					      NAUTILUS_PREFERENCES_USER_LEVEL,
					      user_level_changed_callback,
					      view);
	nautilus_preferences_remove_callback (nautilus_preferences_get_global_preferences (),
					      NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
					      use_new_window_changed_callback,
					      view);
	
	if (view->details->model != NULL) {
		disconnect_model_handlers (view);
		gtk_object_unref (GTK_OBJECT (view->details->model));
	}

	if (view->details->display_selection_idle_id != 0) {
		gtk_idle_remove (view->details->display_selection_idle_id);
	}

	unschedule_display_of_pending_files (view);

	bonobo_object_unref (BONOBO_OBJECT (view->details->view_frame));

	g_free (view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



/**
 * display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: FMDirectoryView for which to display selection info.
 * 
 **/
static void
display_selection_info (FMDirectoryView *view)
{
	GList *selection;
	GnomeVFSFileSize non_folder_size;
	guint non_folder_count;
	guint folder_count;
	guint folder_item_count;
	GList *p;
	char *first_item_name;
	char *non_folder_str;
	char *folder_count_str;
	char *folder_item_count_str;
	Nautilus_StatusRequestInfo request;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	selection = fm_directory_view_get_selection (view);
	
	folder_count = 0;
	folder_item_count = 0;
	non_folder_count = 0;
	non_folder_size = 0;
	first_item_name = NULL;
	folder_count_str = NULL;
	non_folder_str = NULL;
	folder_item_count_str = NULL;
	
	for (p = selection; p != NULL; p = p->next) {
		NautilusFile *file;

		file = p->data;
		if (nautilus_file_is_directory (file)) {
			folder_count++;
			folder_item_count += nautilus_file_get_directory_item_count (file, FALSE);
		} else {
			non_folder_count++;
			non_folder_size += nautilus_file_get_size (file);
		}

		if (first_item_name == NULL) {
			first_item_name = nautilus_file_get_name (file);
		}
	}
	
	nautilus_file_list_free (selection);
	
	memset (&request, 0, sizeof (request));

	/* Break out cases for localization's sake. But note that there are still pieces
	 * being assembled in a particular order, which may be a problem for some localizers.
	 */

	if (folder_count != 0) {
		if (folder_count == 1) {
			if (non_folder_count == 0) {
				folder_count_str = g_strdup_printf (_("\"%s\" selected"), first_item_name);
			} else {
				folder_count_str = g_strdup (_("1 directory selected"));
			}
		} else {
			folder_count_str = g_strdup_printf (_("%d directories selected"), folder_count);
		}

		if (folder_item_count == 0) {
			folder_item_count_str = g_strdup (_("(containing 0 items)"));
		}
		else if (folder_item_count == 1) {
			folder_item_count_str = g_strdup (_("(containing 1 item)"));
		} else {
			folder_item_count_str = g_strdup_printf (_("(containing %d items)"), folder_item_count);
		}
	}

	if (non_folder_count != 0) {
		char *size_string;

		size_string = gnome_vfs_file_size_to_string (non_folder_size);

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
		request.status_string = g_strdup ("");
	} else if (folder_count == 0) {
		request.status_string = g_strdup (non_folder_str);
	} else if (non_folder_count == 0) {
		request.status_string = g_strdup_printf (_("%s %s"), 
							 folder_count_str, 
							 folder_item_count_str);
	} else {
		request.status_string = g_strdup_printf (_("%s %s, %s"), 
							 folder_count_str, 
							 folder_item_count_str,
							 non_folder_str);
	}

	g_free (first_item_name);
	g_free (folder_count_str);
	g_free (folder_item_count_str);
	g_free (non_folder_str);

	nautilus_view_frame_request_status_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &request);

	g_free (request.status_string);
}

static void
fm_directory_view_send_selection_change (FMDirectoryView *view)
{
	Nautilus_SelectionRequestInfo request;
	GList *selection;
	GList *p;
	int i;

	memset (&request, 0, sizeof (request));

	/* Collect a list of URIs. */
	selection = fm_directory_view_get_selection (view);
	request.selected_uris._buffer = g_alloca (g_list_length (selection) * sizeof (char *));
	for (p = selection; p != NULL; p = p->next) {
		request.selected_uris._buffer[request.selected_uris._length++]
			= nautilus_file_get_uri (p->data);
	}
	nautilus_file_list_free (selection);

	/* Send the selection change. */
	nautilus_view_frame_request_selection_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &request);

	/* Free the URIs. */
	for (i = 0; i < request.selected_uris._length; i++) {
		g_free (request.selected_uris._buffer[i]);
	}
}



static void
notify_location_change_cb (NautilusViewFrame *view_frame,
			   Nautilus_NavigationInfo *navigation_context,
			   FMDirectoryView *directory_view)
{
	fm_directory_view_load_uri (directory_view, navigation_context->requested_uri);
}

static void
stop_location_change_cb (NautilusViewFrame *view_frame,
			 FMDirectoryView *directory_view)
{
	fm_directory_view_stop (directory_view);
}



static void
stop_load (FMDirectoryView *view, gboolean error)
{
	Nautilus_ProgressRequestInfo progress;
	
	if (!view->details->loading) {
		g_assert (!error);
		return;
	}

	nautilus_directory_monitor_files_unref (view->details->model);
	
	memset (&progress, 0, sizeof (progress));
	progress.amount = 100.0;
	progress.type = error ? Nautilus_PROGRESS_DONE_ERROR : Nautilus_PROGRESS_DONE_OK;
	nautilus_view_frame_request_progress_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &progress);

	view->details->loading = FALSE;
}



/* handle the "select all" menu command */

static void
select_all_cb (GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_select_all (directory_view);
}

/* handle the zoom in/out menu items */

static void
zoom_in_cb (GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoom_out_cb (GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);
}


static void
zoomable_zoom_in_cb (NautilusZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoomable_zoom_out_cb (NautilusZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);
}

static gboolean
display_pending_files (FMDirectoryView *view)
{
	GList *files_added, *files_removed, *files_changed, *p;
	NautilusFile *file;

	if (view->details->model != NULL
	    && nautilus_directory_are_all_files_seen (view->details->model)) {
		stop_load (view, FALSE);
	}

	files_added = view->details->pending_files_added;
	files_removed = view->details->pending_files_removed;
	files_changed = view->details->pending_files_changed;
	if (files_added == NULL && files_removed == NULL && files_changed == NULL) {
		return FALSE;
	}
	view->details->pending_files_added = NULL;
	view->details->pending_files_removed = NULL;
	view->details->pending_files_changed = NULL;

	gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_ADDING_FILES]);

	for (p = files_added; p != NULL; p = p->next) {
		file = p->data;
		
		if (!nautilus_file_is_gone (file)) {
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[ADD_FILE],
					 file);
		}
	}

	for (p = files_removed; p != NULL; p = p->next) {
		file = p->data;
		
		g_assert (nautilus_file_is_gone (file));
		gtk_signal_emit (GTK_OBJECT (view),
				 signals[REMOVE_FILE],
				 file);
	}

	for (p = files_changed; p != NULL; p = p->next) {
		file = p->data;
		
		if (!nautilus_file_is_gone (file)) {
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[FILE_CHANGED],
					 file);
		}
	}

	gtk_signal_emit (GTK_OBJECT (view), signals[DONE_ADDING_FILES]);

	nautilus_file_list_free (files_added);
	nautilus_file_list_free (files_removed);
	nautilus_file_list_free (files_changed);

	return TRUE;
}

static gboolean
display_selection_info_idle_cb (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	view->details->display_selection_idle_id = 0;

	display_selection_info (view);
	fm_directory_view_send_selection_change (view);

	return FALSE;
}

static gboolean
display_pending_idle_cb (gpointer data)
{
	/* Don't do another idle until we receive more files. */

	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);

	view->details->display_pending_idle_id = 0;

	display_pending_files (view);

	return FALSE;
}

static gboolean
display_pending_timeout_cb (gpointer data)
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
		gtk_idle_add (display_pending_idle_cb, view);
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
				 display_pending_timeout_cb, view);
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
	GList *filtered_files = NULL;
	GList *files_iterator;

	/* Filter out files according to the user level */
	switch (view->details->user_level) {
	case NAUTILUS_USER_LEVEL_NOVICE:
	case NAUTILUS_USER_LEVEL_INTERMEDIATE: 

		/* FIXME: Eventually this should become a generic filtering thingy. */
		for (files_iterator = files; 
		     files_iterator != NULL; 
		     files_iterator = files_iterator->next) {
			NautilusFile *file;
			char * name;
			
			file = NAUTILUS_FILE (files_iterator->data);
			
			g_assert (file != NULL);
			
			name = nautilus_file_get_name (file);
			
			g_assert (name != NULL);
			
			if (!nautilus_str_has_prefix (name, ".")) {
				filtered_files = g_list_append (filtered_files, file);
			}
			
			g_free (name);
		}
		
		files = filtered_files;

		break;
		
	case NAUTILUS_USER_LEVEL_HACKER:
	default:
		break;
	}
	
	/* Put the files on the pending list if there are any. */
	if (files) {
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
}

static void
files_added_cb (NautilusDirectory *directory,
		GList *files,
		gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_added);
}

static void
files_removed_cb (NautilusDirectory *directory,
		  GList *files,
		  gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_removed);
}

static void
files_changed_cb (NautilusDirectory *directory,
		  GList *files,
		  gpointer callback_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_changed);
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

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->bump_zoom_level) (view, zoom_increment);
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

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->can_zoom_in) (view);
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

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->can_zoom_out) (view);
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

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->get_selection) (view);
}

/**
 * fm_directory_view_get_bonobo_ui_handler:
 *
 * Get the BonoboUIHandler for this FMDirectoryView.
 * This is normally called only by subclasses in order to
 * install and modify bonobo menus and such.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: BonoboUIHandler for this view.
 * 
 **/
BonoboUIHandler *
fm_directory_view_get_bonobo_ui_handler (FMDirectoryView *view)
{
        g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);
        
        return bonobo_control_get_ui_handler (get_bonobo_control (view));
}

/**
 * fm_directory_view_get_view_frame:
 *
 * Get the NautilusContentViewFrame for this FMDirectoryView.
 * This is normally called only by the embedding framework.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusContentViewFrame for this view.
 * 
 **/
NautilusContentViewFrame *
fm_directory_view_get_view_frame (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return view->details->view_frame;
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

/* handle the delete command */
/* FIXME: need to handle errors better, and provide feedback for long deletes */
static void
delete_one (gpointer data, gpointer user_data)
{
	NautilusFile *file;
	GtkWidget *error_box;
	char *text_uri, *message;
	
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (FM_IS_DIRECTORY_VIEW (user_data));
	
	file = NAUTILUS_FILE (data);
		
	nautilus_file_delete (file);	
	
	/* report errors if necessary */
	if (!nautilus_file_is_gone (file)) {
		text_uri = nautilus_file_get_uri (file);
		message = g_strdup_printf (_("Sorry, but %s could not be deleted"),
					   text_uri);
		g_free (text_uri);
		error_box = gnome_message_box_new (message, GNOME_MESSAGE_BOX_WARNING,
						   GNOME_STOCK_BUTTON_OK, NULL);
		g_free (message);
	}
}

static gboolean
confirm_delete (FMDirectoryView *directory_view, GList *files)
{
        GtkWidget *dialog;
        GtkWidget *prompt_widget;
        GtkWidget *top_widget;
        char *prompt;
        int file_count;
        int reply;

        g_assert (FM_IS_DIRECTORY_VIEW (directory_view));

        file_count = g_list_length (files);
        g_assert (file_count > 0);

        /* Don't use GNOME_STOCK_BUTTON_CANCEL because the
         * red X is confusing in this context.
         */
        dialog = gnome_dialog_new (_("Delete Selection"),
                                   _("Delete"),
                                   _("Cancel"),
                                   NULL);
        gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
        gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);

        top_widget = gtk_widget_get_toplevel (GTK_WIDGET (directory_view));
        g_assert (GTK_IS_WINDOW (top_widget));
        gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (top_widget));
        
        if (file_count == 1) {
                GnomeVFSURI *vfs_uri;
                char *text_uri;
                char *short_name;

                text_uri = nautilus_file_get_name (NAUTILUS_FILE (files->data));
                vfs_uri = gnome_vfs_uri_new (text_uri);
                g_free (text_uri);

                short_name = gnome_vfs_uri_extract_short_name (vfs_uri);
                prompt = g_strdup_printf (_("Are you sure you want to delete \"%s\"?"), short_name);
                g_free (short_name);
        } else {
                prompt = g_strdup_printf (_("Are you sure you want to delete the %d selected items?"), file_count);
        }

        prompt_widget = gtk_label_new (prompt);
        g_free (prompt);
        
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
                            prompt_widget,
                            TRUE, TRUE, GNOME_PAD);

        gtk_widget_show_all (dialog);

        reply = gnome_dialog_run (GNOME_DIALOG (dialog));

        return reply == 0;
}

static void
fm_directory_view_delete_with_confirm (FMDirectoryView *view, GList *files)
{
        g_assert (FM_IS_DIRECTORY_VIEW (view));
        
	if (confirm_delete (view, files)) {
	        g_list_foreach (files, delete_one, view);    
	}
}

static void
delete_cb (GtkMenuItem *item, GList *files)
{
        fm_directory_view_delete_with_confirm
		(FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view")), 
		 files);
}

/* handle the open command */

static void
open_cb (GtkMenuItem *item, GList *files)
{
	FMDirectoryView *directory_view;

	g_assert (g_list_length (files) == 1);
	g_assert (NAUTILUS_IS_FILE (files->data));

	directory_view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view"));

	fm_directory_view_activate_file_internal (directory_view, files->data, FALSE);
}

static void
open_one_in_new_window (gpointer data, gpointer user_data)
{
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	fm_directory_view_activate_file_internal (FM_DIRECTORY_VIEW (user_data), NAUTILUS_FILE (data), TRUE);
}

static void
open_in_new_window_cb (GtkMenuItem *item, GList *files)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view"));

	g_list_foreach (files, open_one_in_new_window, directory_view);
}

static void
open_one_properties_window (gpointer data, gpointer user_data)
{
	g_assert (NAUTILUS_IS_FILE (data));
	g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	nautilus_gtk_window_present (fm_properties_window_get_or_create (data));
}

static void
open_properties_window_cb (GtkMenuItem *item, GList *files)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view"));

	g_list_foreach (files, open_one_properties_window, directory_view);
}

static void
finish_adding_menu_item (GtkMenu *menu, GtkWidget *menu_item, gboolean sensitive)
{
	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}

static void
add_menu_item (FMDirectoryView *view, GtkMenu *menu, const char *label,
	       void (* activate_handler) (GtkMenuItem *, FMDirectoryView *),
	       gboolean sensitive)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label (label);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (activate_handler), view);
	finish_adding_menu_item (menu, menu_item, sensitive);
}

static void
fm_directory_view_real_append_background_context_menu_items (FMDirectoryView *view, 
							     GtkMenu *menu)
{
	add_menu_item (view, menu, _("Select All"), select_all_cb, TRUE);
	add_menu_item (view, menu, _("Zoom In"), zoom_in_cb,
		       fm_directory_view_can_zoom_in (view));
	add_menu_item (view, menu, _("Zoom Out"), zoom_out_cb,
		       fm_directory_view_can_zoom_out (view));
}

static void
compute_menu_item_info (const char *path, 
                        int selection_length, 
                        gboolean include_accelerator_underbars,
                        char **return_name, 
                        gboolean *return_sensitivity)
{
	char *name, *stripped;

        if (strcmp (path, MENU_PATH_OPEN) == 0) {
                name = g_strdup_printf (_("_Open"));
                *return_sensitivity = selection_length == 1;
        } else if (strcmp (path, MENU_PATH_OPEN_IN_NEW_WINDOW) == 0) {
		if (selection_length <= 1) {
			name = g_strdup (_("Open in _New Window"));
		} else {
			name = g_strdup_printf (_("Open in %d _New Windows"), selection_length);
		}
		*return_sensitivity = selection_length > 0;
	} else if (strcmp (path, MENU_PATH_DELETE) == 0) {
		name = g_strdup (_("_Delete..."));
		*return_sensitivity = selection_length > 0;
	} else if (strcmp (path, MENU_PATH_SET_PROPERTIES) == 0) {
		name = g_strdup (_("Set _Properties..."));
		*return_sensitivity = selection_length > 0;
        } else {
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
append_selection_menu_item (FMDirectoryView *view,
			    GtkMenu *menu,
			    GList *files,
			    const char *path,
			    GtkSignalFunc callback)
{
        GtkWidget *menu_item;
        char *label_string;
        gboolean sensitive;

        compute_menu_item_info (path, g_list_length (files), FALSE, &label_string, &sensitive);
        menu_item = gtk_menu_item_new_with_label (label_string);
        g_free (label_string);

        if (sensitive)
        {
		/* Attach directory view to item. */
		gtk_object_ref (GTK_OBJECT (view));
		gtk_object_set_data_full (GTK_OBJECT (menu_item),
					  "directory_view",
					  view,
					  (GtkDestroyNotify) gtk_object_unref);

		/* Attack callback function to item. */
                gtk_signal_connect (GTK_OBJECT (menu_item),
                                    "activate",
                                    callback,
                                    files);
        }

        finish_adding_menu_item (menu, menu_item, sensitive);
}

static void
fm_directory_view_real_append_selection_context_menu_items (FMDirectoryView *view,
							    GtkMenu *menu,
						       	    GList *files)
{
	append_selection_menu_item (view, menu, files,
				    MENU_PATH_OPEN, open_cb);
	append_selection_menu_item (view, menu, files,
				    MENU_PATH_OPEN_IN_NEW_WINDOW, open_in_new_window_cb);
	append_selection_menu_item (view, menu, files,
				    MENU_PATH_DELETE, delete_cb);
	append_selection_menu_item (view, menu, files,
				    MENU_PATH_SET_PROPERTIES, open_properties_window_cb);
}

static void
fm_directory_view_real_merge_menus (FMDirectoryView *view)
{
        BonoboUIHandler *ui_handler;

        ui_handler = fm_directory_view_get_bonobo_ui_handler (view);

        /* FIXME: The first few items here have magic number indexes. Need to
         * invent and use a scheme whereby Nautilus publishes some or all of
         * its menu item paths so that components can merge items into the
         * right places without special knowledge like this.
         */
        bonobo_ui_handler_menu_new_item (ui_handler,
                                         MENU_PATH_OPEN,
                                         _("_Open"),
                                         _("Open the selected item in this window"),
                                         1,
                                         BONOBO_UI_HANDLER_PIXMAP_NONE,
                                         NULL,
                                         'O',
                                         GDK_CONTROL_MASK,
                                         bonobo_menu_open_cb,
                                         view);                
        bonobo_ui_handler_menu_new_item (ui_handler,
                                         MENU_PATH_OPEN_IN_NEW_WINDOW,
                                         _("Open in New Window"),
                                         _("Open each selected item in a new window"),
                                         2,
                                         BONOBO_UI_HANDLER_PIXMAP_NONE,
                                         NULL,
                                         0,
                                         0,
                                         bonobo_menu_open_in_new_window_cb,
                                         view);                
        bonobo_ui_handler_menu_new_item (ui_handler,
                                         MENU_PATH_DELETE,
                                         _("Delete..."),
                                         _("Delete all selected items"),
                                         3,	                                        
                                         BONOBO_UI_HANDLER_PIXMAP_NONE,
                                         NULL,
                                         0,
                                         0,
                                         bonobo_menu_delete_cb,
                                         view);                
        bonobo_ui_handler_menu_new_item (ui_handler,
                                         MENU_PATH_SELECT_ALL,
                                         _("Select All"),
                                         _("Select all items in this window"),
                                         bonobo_ui_handler_menu_get_pos (ui_handler, MENU_PATH_SELECT_ALL),
                                         BONOBO_UI_HANDLER_PIXMAP_NONE,
                                         NULL,
                                         0,
                                         0,
                                         (BonoboUIHandlerCallbackFunc) select_all_cb,
                                         view);
}

static void
update_one_menu_item (BonoboUIHandler *local_ui_handler, const char* menu_path, int item_count)
{
	char *label_string;
	gboolean sensitive;

        compute_menu_item_info (menu_path, item_count, TRUE, &label_string, &sensitive);
        bonobo_ui_handler_menu_set_sensitivity (local_ui_handler, menu_path, sensitive);
        bonobo_ui_handler_menu_set_label (local_ui_handler, menu_path, label_string);
        g_free (label_string);
}

static void
fm_directory_view_real_update_menus (FMDirectoryView *view)
{
        BonoboUIHandler *handler;
	GList *selection;
	int count;

        handler = fm_directory_view_get_bonobo_ui_handler (view);
	selection = fm_directory_view_get_selection (view);
	count = g_list_length (selection);
	nautilus_file_list_free (selection);

        update_one_menu_item (handler, MENU_PATH_OPEN, count);
        update_one_menu_item (handler, MENU_PATH_OPEN_IN_NEW_WINDOW, count);
        update_one_menu_item (handler, MENU_PATH_DELETE, count);
}

static GtkMenu *
create_selection_context_menu (FMDirectoryView *view) 
{
	GtkMenu *menu;
	GtkWidget *menu_item;
	GList *selected_files;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	menu = GTK_MENU (gtk_menu_new ());
	selected_files = fm_directory_view_get_selection (view);
	if (selected_files != NULL)
	{
		/* Attach selection to menu, and free it when menu is freed.
		 * This lets menu item callbacks hold onto the files parameter.
		 */
		gtk_object_set_data_full (GTK_OBJECT (menu),
					  "selected_items",
					  selected_files,
					  (GtkDestroyNotify) nautilus_file_list_free);

		gtk_signal_emit (GTK_OBJECT (view),
				 signals[APPEND_SELECTION_CONTEXT_MENU_ITEMS], 
				 menu, selected_files);
		
		/* Add separator between selection-specific 
		 * and view-general menu items.
		 */		
		menu_item = gtk_menu_item_new ();
		gtk_widget_show (menu_item);
		gtk_menu_append (menu, menu_item);

	}

	/* Show commands not specific to this item also, since it might
	 * be hard (especially in list view) to find a place to click
	 * that's not on an item.
	 */
	fm_directory_view_append_background_context_menu_items (view, menu);

	return menu;
}

/**
 * fm_directory_view_append_background_context_menu_items:
 *
 * Add background menu items (i.e., those not dependent on a particular file)
 * to a context menu.
 * @view: An FMDirectoryView.
 * @menu: The menu being constructed. Could be a background menu or an item-specific
 * menu, because the background items are present in both.
 * 
 **/
static void
fm_directory_view_append_background_context_menu_items (FMDirectoryView *view,
							GtkMenu *menu)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 signals[APPEND_BACKGROUND_CONTEXT_MENU_ITEMS], 
			 menu);
}


static GtkMenu *
create_background_context_menu (FMDirectoryView *view)
{
	GtkMenu *menu;

	menu = GTK_MENU (gtk_menu_new ());
	fm_directory_view_append_background_context_menu_items (view, menu);
	
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
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	nautilus_pop_up_context_menu (create_selection_context_menu (view),
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT);
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
	
	nautilus_pop_up_context_menu (create_background_context_menu (view),
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT);
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

	/* Update menu item states to match selection */
	fm_directory_view_update_menus (view);

	/* Schedule a display of the new selection. */
	if (view->details->display_selection_idle_id == 0)
		view->details->display_selection_idle_id
			= gtk_idle_add (display_selection_info_idle_cb,
					view);
}

/**
 * fm_directory_view_activate_file_internal:
 * 
 * Activate an file in this view. This might involve switching the displayed
 * location for the current window, or launching an application. The only
 * difference between this private call and the public fm_directory_view_activate_file
 * is that the public call combines the caller's new-window request with the
 * user's preference to create the value for this call.
 * @view: FMDirectoryView in question.
 * @file: A NautilusFile representing the file in this view to activate.
 * @use_new_window: Should this item be opened in a new window?
 * 
 **/
static void
fm_directory_view_activate_file_internal (FMDirectoryView *view, 
				 	  NautilusFile *file,
				 	  gboolean use_new_window)
{
	Nautilus_NavigationRequestInfo request;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	request.requested_uri = nautilus_file_get_uri (file);
	request.new_window_requested = use_new_window;
	nautilus_view_frame_request_location_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &request);

	g_free (request.requested_uri);
}


/**
 * fm_directory_view_activate_file:
 * 
 * Activate an file in this view. This might involve switching the displayed
 * location for the current window, or launching an application. This is normally
 * called only by subclasses.
 * @view: FMDirectoryView in question.
 * @file: A NautilusFile representing the file in this view to activate.
 * @request_new_window: Should this item be opened in a new window?
 * 
 **/
void
fm_directory_view_activate_file (FMDirectoryView *view, 
				 NautilusFile *file,
				 gboolean request_new_window)
{
	fm_directory_view_activate_file_internal (view,
						  file,
						  request_new_window || view->details->use_new_window);
}

/**
 * fm_directory_view_load_uri:
 * 
 * Switch the displayed location to a new uri. If the uri is not valid,
 * the location will not be switched; user feedback will be provided instead.
 * @view: FMDirectoryView whose location will be changed.
 * @uri: A string representing the uri to switch to.
 * 
 **/
void
fm_directory_view_load_uri (FMDirectoryView *view,
			    const char *uri)
{
	Nautilus_ProgressRequestInfo progress;
	NautilusDirectory *old_model;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (uri != NULL);

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	disconnect_model_handlers (view);

	old_model = view->details->model;
	view->details->model = nautilus_directory_get (uri);
	if (old_model != NULL) {
		gtk_object_unref (GTK_OBJECT (old_model));
	}

	memset (&progress, 0, sizeof (progress));
	progress.type = Nautilus_PROGRESS_UNDERWAY;
	nautilus_view_frame_request_progress_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &progress);

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_LOADING]);

	schedule_timeout_display_of_pending_files (view);
	view->details->loading = TRUE;
	nautilus_directory_monitor_files_ref (view->details->model,
					      files_added_cb,
					      view);

	/* Attach a handler to get any further files that show up as we
	 * load and sychronize. We won't miss any files because this
	 * signal is emitted from an idle routine and so we will be
	 * connected before the next time it is emitted.
	 */
    	view->details->files_added_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model), 
		 "files_added",
		 GTK_SIGNAL_FUNC (files_added_cb),
		 view);
	view->details->files_removed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model), 
		 "files_removed",
		 GTK_SIGNAL_FUNC (files_removed_cb),
		 view);
	view->details->files_changed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model), 
		 "files_changed",
		 GTK_SIGNAL_FUNC (files_changed_cb),
		 view);
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

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->merge_menus) (view);
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
	disconnect_handler (view, &view->details->files_removed_handler_id);
	disconnect_handler (view, &view->details->files_changed_handler_id);

	if (view->details->loading) {
		g_assert (view->details->model != NULL);
		nautilus_directory_monitor_files_unref (view->details->model);
		view->details->loading = FALSE;
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

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->select_all) (view);
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
	stop_load (view, FALSE);
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

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->update_menus) (view);
}

static void
use_new_window_changed_callback (NautilusPreferences *preferences,
			         const char *name,
			         NautilusPreferencesType type,
			         gconstpointer value,
			         gpointer user_data)
{
	g_assert (NAUTILUS_IS_PREFERENCES (preferences));
	g_assert (strcmp (name, NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW) == 0);
	g_assert (type == NAUTILUS_PREFERENCE_BOOLEAN);
	g_assert (GPOINTER_TO_INT (value) == FALSE || GPOINTER_TO_INT (value) == TRUE);
	g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	FM_DIRECTORY_VIEW (user_data)->details->use_new_window = GPOINTER_TO_INT (value);
}

static void
user_level_changed_callback (NautilusPreferences *preferences,
			     const char *name,
			     NautilusPreferencesType type,
			     gconstpointer value,
			     gpointer user_data)
{
	FMDirectoryView *directory_view;
	char *same_uri;

	g_assert (NAUTILUS_IS_PREFERENCES (preferences));
	g_assert (strcmp (name, NAUTILUS_PREFERENCES_USER_LEVEL) == 0);
	g_assert (type == NAUTILUS_PREFERENCE_ENUM);
	g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	directory_view = FM_DIRECTORY_VIEW (user_data);

	directory_view->details->user_level = GPOINTER_TO_INT (value);

	/* Reload the current uri so that the filtering changes take place. */
	if (directory_view->details->model != NULL) {
		same_uri = nautilus_directory_get_uri (directory_view->details->model);
		g_assert (same_uri != NULL);
		fm_directory_view_load_uri (directory_view, same_uri);
		g_free (same_uri);
	}
}
