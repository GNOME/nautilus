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
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-result.h>

#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-zoomable.h>

#include <libnautilus-extensions/nautilus-alloc.h>
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>

#include "fm-properties-window.h"
#include "dfos-xfer.h"

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

enum
{
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
	
	guint display_selection_idle_id;
	
	guint display_pending_timeout_id;
	guint display_pending_idle_id;
	
	guint files_added_handler_id;
	guint files_changed_handler_id;
	
	GList *pending_files_added;
	GList *pending_files_changed;
	GList *pending_uris_selected;

	gboolean loading;

	gboolean show_hidden_files;
};

/* forward declarations */

static int            display_selection_info_idle_callback                        (gpointer                  data);
static void           display_selection_info                                      (FMDirectoryView          *view);
static void           fm_directory_view_initialize_class                          (FMDirectoryViewClass     *klass);
static void           fm_directory_view_initialize                                (FMDirectoryView          *view);
static void           fm_directory_view_duplicate_selection                       (FMDirectoryView          *view,
										   GList                    *files);
static void           fm_directory_view_trash_or_delete_selection                 (FMDirectoryView          *view,
										   GList                    *files);
static void	      fm_directory_view_new_folder				  (FMDirectoryView          *view);
static void           fm_directory_view_destroy                                   (GtkObject                *object);
static void           fm_directory_view_activate_file_internal                    (FMDirectoryView          *view,
										   NautilusFile             *file,
										   gboolean                  use_new_window);
static void           fm_directory_view_create_background_context_menu_items      (FMDirectoryView          *view,
										   GtkMenu                  *menu);
static void           fm_directory_view_merge_menus                               (FMDirectoryView          *view);
static void           fm_directory_view_real_create_background_context_menu_items (FMDirectoryView          *view,
										   GtkMenu                  *menu);
static void           fm_directory_view_real_create_selection_context_menu_items  (FMDirectoryView          *view,
										   GtkMenu                  *menu,
										   GList                    *files);
static void           fm_directory_view_real_merge_menus                          (FMDirectoryView          *view);
static void           fm_directory_view_real_update_menus                         (FMDirectoryView          *view);
static GtkMenu *      create_selection_context_menu                               (FMDirectoryView          *view);
static GtkMenu *      create_background_context_menu                              (FMDirectoryView          *view);
static BonoboControl *get_bonobo_control                                          (FMDirectoryView          *view);
static void           stop_location_change_callback                               (NautilusView             *nautilus_view,
										   FMDirectoryView          *directory_view);
static void           notify_location_change_callback                             (NautilusView             *nautilus_view,
										   Nautilus_NavigationInfo  *nav_context,
										   FMDirectoryView          *directory_view);
static void           notify_selection_change_callback                            (NautilusView             *nautilus_view,
										   Nautilus_SelectionInfo   *sel_context,
										   FMDirectoryView          *directory_view);
static void           open_callback                                               (GtkMenuItem              *item,
										   GList                    *files);
static void           open_in_new_window_callback                                 (GtkMenuItem              *item,
										   GList                    *files);
static void           open_one_in_new_window                                      (gpointer                  data,
										   gpointer                  user_data);
static void           open_one_properties_window                                  (gpointer                  data,
										   gpointer                  user_data);
static void           zoomable_zoom_in_callback                                   (NautilusZoomable         *zoomable,
										   FMDirectoryView          *directory_view);
static void           zoomable_zoom_out_callback                                  (NautilusZoomable         *zoomable,
										   FMDirectoryView          *directory_view);
static void           schedule_idle_display_of_pending_files                      (FMDirectoryView          *view);
static void           unschedule_idle_display_of_pending_files                    (FMDirectoryView          *view);
static void           schedule_timeout_display_of_pending_files                   (FMDirectoryView          *view);
static void           unschedule_timeout_display_of_pending_files                 (FMDirectoryView          *view);
static void           unschedule_display_of_pending_files                         (FMDirectoryView          *view);
static void           disconnect_model_handlers                                   (FMDirectoryView          *view);
static void           show_hidden_files_changed_callback                          (gpointer                  user_data);
static void           get_required_metadata_keys                                  (FMDirectoryView          *view,
										   GList                   **directory_keys_result,
										   GList                   **file_keys_result);
static void           start_renaming_item                   	                  (FMDirectoryView          *view,
										   const char 		    *uri);
static void           metadata_ready_callback                                     (NautilusDirectory        *directory,
										   GList                    *files,
										   gpointer                  callback_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_file)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, bump_zoom_level)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_in)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_out)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, file_changed)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)
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
       				GTK_RUN_FIRST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, create_selection_context_menu_items),
		    		nautilus_gtk_marshal_NONE__BOXED_BOXED,
		    		GTK_TYPE_NONE, 2, GTK_TYPE_BOXED, GTK_TYPE_BOXED);
	signals[CREATE_BACKGROUND_CONTEXT_MENU_ITEMS] =
		gtk_signal_new ("create_background_context_menu_items",
       				GTK_RUN_FIRST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, create_background_context_menu_items),
		    		gtk_marshal_NONE__BOXED,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_BOXED);

	klass->create_selection_context_menu_items = fm_directory_view_real_create_selection_context_menu_items;
	klass->create_background_context_menu_items = fm_directory_view_real_create_background_context_menu_items;
        klass->merge_menus = fm_directory_view_real_merge_menus;
        klass->update_menus = fm_directory_view_real_update_menus;
	klass->get_required_metadata_keys = get_required_metadata_keys;
	klass->start_renaming_item = start_renaming_item;

	/* Function pointers that subclasses must override */

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, add_file);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, bump_zoom_level);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_in);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_out);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, clear);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, file_changed);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selection);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, select_all);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, set_selection);
}

static void
bonobo_menu_open_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless exactly
         * one item is selected.
         */
        g_assert (nautilus_g_list_exactly_one_item (selection));

	fm_directory_view_activate_file_internal (view, 
	                                  	  NAUTILUS_FILE (selection->data), 
	                                  	  FALSE);        

	nautilus_file_list_free (selection);
}

static void
bonobo_menu_open_in_new_window_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);
	g_list_foreach (selection, open_one_in_new_window, view);

	nautilus_file_list_free (selection);
}

static void
fm_directory_view_chose_application_callback (GnomeVFSMimeApplication *application, 
					      gpointer callback_data)
{
	NautilusFile *file;
	char *uri;

	g_return_if_fail (NAUTILUS_IS_FILE (callback_data));

	file = NAUTILUS_FILE (callback_data);

	if (application != NULL) {
		uri = nautilus_file_get_uri (file);
		nautilus_launch_application (application->command, uri);
		g_free (uri);
	}

	nautilus_file_unref (file);
}

static void
fm_directory_view_chose_component_callback (NautilusViewIdentifier *identifier, 
					    gpointer callback_data)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (callback_data));

	if (identifier != NULL) {
		/* FIXME: Need to implement switching location & url. */
	}

	nautilus_view_identifier_free (identifier);
}

static void
choose_application (FMDirectoryView *view, 
		    NautilusFile *file)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_file_ref (file);
	nautilus_choose_application_for_file 
		(file,
		 GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
		 fm_directory_view_chose_application_callback,
		 file);
}

static void
choose_component (FMDirectoryView *view, 
		  NautilusFile *file)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_choose_component_for_file 
		(file,
		 GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
		 fm_directory_view_chose_component_callback,
		 view);
}

static void
bonobo_menu_other_program_callback (BonoboUIHandler *ui_handler, 
				    gpointer user_data, 
				    const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless exactly
         * one item is selected.
         */
        g_assert (nautilus_g_list_exactly_one_item (selection));

	if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION) == 0) {
		choose_application (view, NAUTILUS_FILE (selection->data));
	} else {
		g_assert (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER) == 0);
		choose_component (view, NAUTILUS_FILE (selection->data));
	}

	nautilus_file_list_free (selection);
}

static void
bonobo_menu_move_to_trash_callback (BonoboUIHandler *ui_handler,
				    gpointer user_data,
				    const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);
        fm_directory_view_trash_or_delete_selection (view, selection);

        nautilus_file_list_free (selection);
}

static void
bonobo_menu_duplicate_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless at least
         * one item is selected.
         */
        g_assert (selection != NULL);

        fm_directory_view_duplicate_selection (view, selection);

        nautilus_file_list_free (selection);
}

static void
bonobo_menu_select_all_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
	g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	fm_directory_view_select_all (user_data);
}

static void
bonobo_menu_empty_trash_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	fs_empty_trash (GTK_WIDGET (FM_DIRECTORY_VIEW (user_data)));
}

static void
bonobo_menu_new_folder_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{                
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	fm_directory_view_new_folder (FM_DIRECTORY_VIEW (user_data));
}

static void
bonobo_menu_open_properties_window_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
        FMDirectoryView *view;
        GList *selection;
        
        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);
	selection = fm_directory_view_get_selection (view);

        /* UI should have prevented this from being called unless at least
         * one item is selected.
         */
        g_assert (selection != NULL);

	g_list_foreach (selection, open_one_properties_window, view);

        nautilus_file_list_free (selection);
}

static BonoboControl *
get_bonobo_control (FMDirectoryView *view)
{
        return BONOBO_CONTROL (nautilus_view_get_bonobo_control
			       (view->details->nautilus_view));
}

static void
bonobo_control_activate_callback (BonoboObject *control, gboolean state, gpointer user_data)
{
        FMDirectoryView *view;
        BonoboUIHandler *local_ui_handler;

        g_assert (FM_IS_DIRECTORY_VIEW (user_data));

        view = FM_DIRECTORY_VIEW (user_data);

        local_ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (control));

        if (state) {
		/* FIXME: Doesn't the remote UI handler leak here? */
                bonobo_ui_handler_set_container
			(local_ui_handler, 
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

	directory_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (directory_view));

	directory_view->details->zoomable = nautilus_zoomable_new_from_bonobo_control
		(get_bonobo_control (directory_view), .25, 4.0, FALSE);		

	gtk_signal_connect (GTK_OBJECT (directory_view->details->nautilus_view), 
			    "stop_location_change",
			    GTK_SIGNAL_FUNC (stop_location_change_callback),
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->nautilus_view), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (notify_location_change_callback), 
			    directory_view);
	gtk_signal_connect (GTK_OBJECT (directory_view->details->nautilus_view), 
			    "notify_selection_change",
			    GTK_SIGNAL_FUNC (notify_selection_change_callback), 
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

	gtk_widget_show (GTK_WIDGET (directory_view));

	/* Obtain the user level for filtering */
	directory_view->details->show_hidden_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES, FALSE);

	/* Keep track of changes in this pref to filter files accordingly. */
	nautilus_preferences_add_boolean_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
						   show_hidden_files_changed_callback,
						   directory_view);
}

static void
fm_directory_view_destroy (GtkObject *object)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (object);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					      show_hidden_files_changed_callback,
					      view);
	
	if (view->details->model != NULL) {
		disconnect_model_handlers (view);
		nautilus_directory_unref (view->details->model);
	}

	if (view->details->display_selection_idle_id != 0) {
		gtk_idle_remove (view->details->display_selection_idle_id);
	}

	unschedule_display_of_pending_files (view);

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
	guint non_folder_count, folder_count, folder_item_count;
	gboolean folder_item_count_known;
	guint item_count;
	GList *p;
	char *first_item_name;
	char *non_folder_str;
	char *folder_count_str;
	char *folder_item_count_str;
	Nautilus_StatusRequestInfo request;
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
			first_item_name = nautilus_link_get_display_name(nautilus_file_get_name (file));
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
		request.status_string = g_strdup_printf (_("%s%s"), 
							 folder_count_str, 
							 folder_item_count_str);
	} else {
		request.status_string = g_strdup_printf (_("%s%s, %s"), 
							 folder_count_str, 
							 folder_item_count_str,
							 non_folder_str);
	}

	g_free (first_item_name);
	g_free (folder_count_str);
	g_free (folder_item_count_str);
	g_free (non_folder_str);

	nautilus_view_request_status_change (view->details->nautilus_view,
					     &request);

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
	nautilus_view_request_selection_change (view->details->nautilus_view,
						&request);

	/* Free the URIs. */
	for (i = 0; i < request.selected_uris._length; i++) {
		g_free (request.selected_uris._buffer[i]);
	}
}



static void
notify_location_change_callback (NautilusView *nautilus_view,
				 Nautilus_NavigationInfo *navigation_context,
				 FMDirectoryView *directory_view)
{
	fm_directory_view_load_uri (directory_view, navigation_context->requested_uri);
}

static void
notify_selection_change_callback (NautilusView *nautilus_view,
				  Nautilus_SelectionInfo *selection_context,
				  FMDirectoryView *directory_view)
{
	int i;
	GList *selection;
	NautilusFile *file;

	selection = NULL;

	if (directory_view->details->loading) {
		nautilus_g_list_free_deep (directory_view->details->pending_uris_selected);
		directory_view->details->pending_uris_selected = NULL;
	}

	if (!selection_context->self_originated) {
		if (!directory_view->details->loading) {
			/* If we aren't still loading, set the selection right now. */
			for (i = 0; i < selection_context->selected_uris._length; i++) {
				file = nautilus_file_get (selection_context->selected_uris._buffer[i]);
				if (file != NULL) {
					selection = g_list_prepend (selection, file);
				}
			}
			
			fm_directory_view_set_selection (directory_view, selection);
			g_list_free (selection);
		} else {
			/* If we are still loading, add to the list of pending URIs instead. */
			for (i = 0; i < selection_context->selected_uris._length; i++) {
				directory_view->details->pending_uris_selected = 
					g_list_prepend (directory_view->details->pending_uris_selected, 
							g_strdup (selection_context->selected_uris._buffer[i]));
			}
		}
	} 
}

static void
stop_location_change_callback (NautilusView *nautilus_view,
			       FMDirectoryView *directory_view)
{
	fm_directory_view_stop (directory_view);
}



static void
done_loading (FMDirectoryView *view)
{
	Nautilus_ProgressRequestInfo progress;
	
	if (!view->details->loading) {
		return;
	}

	memset (&progress, 0, sizeof (progress));
	progress.amount = 100.0;
	progress.type = Nautilus_PROGRESS_DONE_OK;
	nautilus_view_request_progress_change (view->details->nautilus_view,
					       &progress);

	view->details->loading = FALSE;
}



static void
select_all_callback (GtkMenuItem *item, gpointer callback_data)
{
	g_assert (GTK_IS_MENU_ITEM (item));
	g_assert (callback_data == NULL);

	fm_directory_view_select_all (
		FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view")) );
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
zoomable_zoom_in_callback (NautilusZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoomable_zoom_out_callback (NautilusZoomable *zoomable, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);
}

static gboolean
display_pending_files (FMDirectoryView *view)
{
	GList *files_added, *files_changed, *uris_selected, *p;
	NautilusFile *file;
	GList *selection;
	char *uri;

	if (view->details->model != NULL
	    && nautilus_directory_are_all_files_seen (view->details->model)) {
		done_loading (view);
	}

	/* FIXME bugzilla.eazel.com 658: fix memory management here */

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
		file = p->data;
		
		if (nautilus_directory_contains_file (view->details->model, file)) {
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[ADD_FILE],
					 file);
		}
	}

	for (p = files_changed; p != NULL; p = p->next) {
		file = p->data;
		
		gtk_signal_emit (GTK_OBJECT (view),
				 signals[FILE_CHANGED],
				 file);
	}

	gtk_signal_emit (GTK_OBJECT (view), signals[DONE_ADDING_FILES]);

	if (nautilus_directory_are_all_files_seen (view->details->model)
	    && view->details->pending_uris_selected != NULL) {
		selection = NULL;
		view->details->pending_uris_selected = NULL;
		
		for (p = uris_selected; p != NULL; p = p->next) {
			uri = p->data;
			file = nautilus_file_get (uri);
			if (file != NULL) {
				selection = g_list_prepend (selection, file);
			}
		}
		nautilus_g_list_free_deep (uris_selected);

		fm_directory_view_set_selection (view, selection);

		g_list_free (selection);
	}

	nautilus_file_list_free (files_added);
	nautilus_file_list_free (files_changed);

	return TRUE;
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	view->details->display_selection_idle_id = 0;

	display_selection_info (view);
	fm_directory_view_send_selection_change (view);

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
	GList *filtered_files = NULL;
	GList *files_iterator;
	NautilusFile *file;
	char * name;

	/* Filter out hidden files if needed */
	if (!view->details->show_hidden_files) {
		/* FIXME bugzilla.eazel.com 653: 
		 * Eventually this should become a generic filtering thingy. 
		 */
		for (files_iterator = files; 
		     files_iterator != NULL; 
		     files_iterator = files_iterator->next) {
			file = NAUTILUS_FILE (files_iterator->data);
			
			g_assert (file != NULL);
			
			name = nautilus_link_get_display_name(nautilus_file_get_name (file));
			
			g_assert (name != NULL);
			
			if (!nautilus_str_has_prefix (name, ".")) {
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
append_uri_one (gpointer data, gpointer user_data)
{
	NautilusFile *file;
	GList **result;
	
	g_assert (NAUTILUS_IS_FILE (data));

	result = (GList **) user_data;
	file = (NautilusFile *) data;
	*result = g_list_append (*result, nautilus_file_get_uri (file));
}

static void
fm_directory_view_duplicate_selection (FMDirectoryView *view, GList *files)
{
	GList *uris;

        g_assert (FM_IS_DIRECTORY_VIEW (view));
        g_assert (files != NULL);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, append_uri_one, &uris);    

        g_assert (g_list_length (uris) == g_list_length (files));
        
	fs_xfer (uris, NULL, NULL, GDK_ACTION_COPY, GTK_WIDGET (view));
	nautilus_g_list_free_deep (uris);
}

static gboolean
fm_directory_is_trash (FMDirectoryView *view)
{
	/* Return TRUE if directory view is Trash or inside Trash */
	char *directory;
	GnomeVFSURI *directory_uri;
	GnomeVFSURI *trash_dir_uri;
	gboolean result;

	trash_dir_uri = NULL;
	directory = fm_directory_view_get_uri (view);
	directory_uri = gnome_vfs_uri_new (directory);

	result = gnome_vfs_find_directory (directory_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, TRUE, 0777) == GNOME_VFS_OK;

	result &= (gnome_vfs_uri_equal (trash_dir_uri, directory_uri)
		|| gnome_vfs_uri_is_parent (trash_dir_uri, directory_uri, TRUE));

	gnome_vfs_uri_unref (trash_dir_uri);
	gnome_vfs_uri_unref (directory_uri);
	g_free (directory);

	return result;
}

static gboolean
fm_directory_can_move_to_trash (FMDirectoryView *view)
{
	/* Return TRUE if we can get a trash directory on the same volume */
	char *directory;
	GnomeVFSURI *directory_uri;
	GnomeVFSURI *trash_dir_uri;
	gboolean result;

	trash_dir_uri = NULL;
	directory = fm_directory_view_get_uri (view);
	directory_uri = gnome_vfs_uri_new (directory);

	result = gnome_vfs_find_directory (directory_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_dir_uri, TRUE, 0777) == GNOME_VFS_OK;

	if (result && (gnome_vfs_uri_equal (trash_dir_uri, directory_uri)
		|| gnome_vfs_uri_is_parent (trash_dir_uri, directory_uri, TRUE))) {
		/* don't allow trashing items already in the Trash */
		result = FALSE;
	}
	gnome_vfs_uri_unref (trash_dir_uri);
	gnome_vfs_uri_unref (directory_uri);
	g_free (directory);

	return result;
}

static gboolean
fm_directory_view_confirm_deletion (FMDirectoryView *view, GList *files)
{
	char *prompt;
	int file_count;
	GnomeVFSURI *uri;
	char *text_uri;
	char *short_name;
	gboolean result;

	g_assert (FM_IS_DIRECTORY_VIEW (view));

	file_count = g_list_length (files);
	g_assert (file_count > 0);
	
	if (file_count == 1) {

		text_uri = nautilus_file_get_name (NAUTILUS_FILE (files->data));
		uri = gnome_vfs_uri_new (text_uri);
		g_free (text_uri);

		short_name = gnome_vfs_uri_extract_short_name (uri);
		prompt = g_strdup_printf (_("Are you sure you want to permanently "
			"remove item \"%s\"?"), short_name);
		g_free (short_name);
		gnome_vfs_uri_unref (uri);
	} else {
		prompt = g_strdup_printf (_("Are you sure you want to permanently "
			"remove the %d selected items?"), file_count);
	}

	result = nautilus_simple_dialog (GTK_WIDGET (view), prompt, _("Deleting items"),
		_("Delete"), _("Cancel"), NULL) == 0;

	g_free (prompt);
	return result;
}

static void
fm_directory_view_trash_or_delete_selection (FMDirectoryView *view, GList *files)
{
	GList *uris;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (files != NULL);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, append_uri_one, &uris);    

	g_assert (g_list_length (uris) == g_list_length (files));

	if (fm_directory_can_move_to_trash (view)) {
		fs_move_to_trash (uris, GTK_WIDGET (view));
	} else if (fm_directory_is_trash (view)
		|| fm_directory_view_confirm_deletion (view, files)) {
		fs_delete (uris, GTK_WIDGET (view));
	}
	
	nautilus_g_list_free_deep (uris);
}

static void
duplicate_callback (GtkMenuItem *item, GList *files)
{
	fm_directory_view_duplicate_selection
		(FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view")), 
		 files);
}

static void
trash_callback (GtkMenuItem *item, GList *files)
{
        fm_directory_view_trash_or_delete_selection
		(FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view")), 
		 files);
}

static void
start_renaming_item (FMDirectoryView *view, const char *uri)
{
	GList *selection;
	NautilusFile *file;

	selection = NULL;
	file = nautilus_file_get (uri);
	if (file ==  NULL) {
		return;
	}

	selection = g_list_prepend (NULL, file);
	fm_directory_view_set_selection (view, selection);
}

typedef struct {
	FMDirectoryView *view;
	char *uri;
} RenameLaterParameters;

static gboolean
new_folder_rename_later (void *data)
{
	RenameLaterParameters *parameters = (RenameLaterParameters *)data;

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (parameters->view)->klass)->start_renaming_item) 
		(parameters->view, parameters->uri);

	g_free (parameters->uri);
	g_free (parameters);

	return FALSE;
}

static void
new_folder_done (const char *new_folder_uri, gpointer data)
{
	FMDirectoryView *directory_view;
	RenameLaterParameters *parameters;
	
	directory_view = (FMDirectoryView *)data;
	parameters = g_new0 (RenameLaterParameters, 1);

	parameters->uri = g_strdup (new_folder_uri);
	parameters->view = directory_view;

	/* FIXME:
	 * runing the start_renaming_item with a delay because at this point
	 * it's not in the icon container's icon list. 
	 * There are two problems with this besides clunkiness - by the time the 
	 * timeout expires, the directory view could be dead and the delay value 
	 * is arbitrary, if the machine is loaded up, it may not be enough.
	 * Need to add a mechanism here that ensures synchronously that the item
	 * is added to the icon container instead.
	 */
	gtk_timeout_add (100, new_folder_rename_later, parameters);
}

static void
fm_directory_view_new_folder (FMDirectoryView *directory_view)
{
	char *parent_uri;

	parent_uri = fm_directory_view_get_uri (directory_view);
	fs_new_folder (GTK_WIDGET(directory_view), parent_uri, new_folder_done, directory_view);

	g_free (parent_uri);
}

/* handle the open command */

static void
open_callback (GtkMenuItem *item, GList *files)
{
	FMDirectoryView *directory_view;

	g_assert (nautilus_g_list_exactly_one_item (files));
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
open_in_new_window_callback (GtkMenuItem *item, GList *files)
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
open_properties_window_callback (GtkMenuItem *item, GList *files)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view"));

	g_list_foreach (files, open_one_properties_window, directory_view);
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
	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
finish_adding_menu_item (GtkMenu *menu, GtkWidget *menu_item, gboolean sensitive)
{
	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}

static void
compute_menu_item_info (const char *path, 
                        GList *selection,
                        gboolean include_accelerator_underbars,
                        char **return_name, 
                        gboolean *return_sensitivity)
{
	char *name, *stripped;
	int count;

	*return_sensitivity = TRUE;

        if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_OPEN) == 0) {
                name = g_strdup_printf (_("_Open"));
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
		*return_sensitivity = selection != NULL;
        } else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER) == 0) {
		name = g_strdup (_("New Folder"));
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_DELETE) == 0) {
		name = g_strdup (_("_Delete..."));
		*return_sensitivity = selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_TRASH) == 0) {
		name = g_strdup (_("_Move to Trash"));
		*return_sensitivity = selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE) == 0) {
		name = g_strdup (_("_Duplicate"));
		*return_sensitivity = selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES) == 0) {
		name = g_strdup (_("Show _Properties"));
		*return_sensitivity = selection != NULL;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH) == 0) {
		name = g_strdup (_("_Empty Trash"));
		/* FIXME bugzilla.eazel.com 656: Should only be sensitive if trash is not empty */
		*return_sensitivity = TRUE;
	} else if (strcmp (path, NAUTILUS_MENU_PATH_SELECT_ALL_ITEM) == 0) {
		name = g_strdup (_("_Select All"));
		*return_sensitivity = TRUE;
	} else if (strcmp (path, FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS) == 0) {
                if (nautilus_g_list_more_than_one_item (selection)) {
                        name = g_strdup (_("R_emove Custom Images"));
                } else {
                        name = g_strdup (_("R_emove Custom Image"));
                }
        	*return_sensitivity = files_have_any_custom_images (selection);
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
append_gtk_menu_item_with_view (FMDirectoryView *view,
			        GtkMenu *menu,
			        GList *files,
			        const char *path,
			        GtkSignalFunc callback)
{
        GtkWidget *menu_item;
        char *label_string;
        gboolean sensitive;

        compute_menu_item_info (path, files, FALSE, &label_string, &sensitive);
        menu_item = gtk_menu_item_new_with_label (label_string);
        g_free (label_string);

        if (sensitive) {
        	if (view != NULL) {
			/* Attach directory view to item. */
			gtk_object_ref (GTK_OBJECT (view));
			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "directory_view",
						  view,
						  (GtkDestroyNotify) gtk_object_unref);
        	}

		/* Attack callback function to item. */
                gtk_signal_connect (GTK_OBJECT (menu_item),
                                    "activate",
                                    callback,
                                    files);
        }

        finish_adding_menu_item (menu, menu_item, sensitive);
}

static void
append_selection_menu_subtree (FMDirectoryView *view,
			       GtkMenu *parent_menu,
			       GtkMenu *child_menu,
			       GList *files,
			       const char *path)
{
        GtkWidget *menu_item;
        char *label_string;
        gboolean sensitive;

        compute_menu_item_info (path, files, FALSE, &label_string, &sensitive);
        menu_item = gtk_menu_item_new_with_label (label_string);
        g_free (label_string);

        finish_adding_menu_item (parent_menu, menu_item, sensitive);

        gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), 
        			   GTK_WIDGET (child_menu));
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
fm_directory_view_real_create_background_context_menu_items (FMDirectoryView *view, 
							     GtkMenu *menu)
{
	append_gtk_menu_item_with_view (view, 
	                                menu, 
	                                NULL, 
	                                NAUTILUS_MENU_PATH_SELECT_ALL_ITEM, 
			      		select_all_callback);
	/* FIXME: Need to think clearly about what items to include here.
	 * We want the list to be pretty short, but not degenerately short.
	 * Zoom In and Out don't really seem to belong. Maybe "Show Properties"
	 * (for the current location, not selection -- but would have to not
	 * include this item when there's a selection)? Add Bookmark? (same issue).
	 */
	add_menu_item (view, menu, _("Zoom In"), zoom_in_callback,
		       fm_directory_view_can_zoom_in (view));
	add_menu_item (view, menu, _("Zoom Out"), zoom_out_callback,
		       fm_directory_view_can_zoom_out (view));
}

static void
other_application_callback (GtkMenuItem *menu_item, GList *files)
{
	FMDirectoryView *view;

	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (nautilus_g_list_exactly_one_item (files));
	g_assert (NAUTILUS_IS_FILE (files->data));

	/* We've cleverly stashed away the directory view in the menu item. */
	view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (menu_item), 
						       "directory_view"));

        choose_application (view, NAUTILUS_FILE (files->data));
}

static void
other_viewer_callback (GtkMenuItem *menu_item, GList *files)
{
	FMDirectoryView *view;

	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (nautilus_g_list_exactly_one_item (files));
	g_assert (NAUTILUS_IS_FILE (files->data));

	/* We've cleverly stashed away the directory view in the menu item. */
	view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (menu_item), 
						       "directory_view"));

        choose_component (view, NAUTILUS_FILE (files->data));
}

static void
add_open_with_gtk_menu_item (GtkMenu *menu, const char *label)
{
	GtkWidget *menu_item;

	if (label != NULL) {
		menu_item = gtk_menu_item_new_with_label (label);
	} else {
		/* No label means this is a separator. */
		menu_item = gtk_menu_item_new ();
	}

	finish_adding_menu_item (menu, menu_item, TRUE);
}

static void
launch_application_from_menu_item (GtkMenuItem *menu_item, gpointer user_data)
{
	GnomeVFSMimeApplication *application;
	char *uri;

	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (user_data == NULL);

	application = (GnomeVFSMimeApplication *)gtk_object_get_data (GTK_OBJECT (menu_item), 
								      "application");
	g_assert (application != NULL);

	uri = (char *)gtk_object_get_data (GTK_OBJECT (menu_item), "uri");
	g_assert (uri != NULL);

	nautilus_launch_application (application->command, uri);
}

static void
view_uri_from_menu_item (GtkMenuItem *menu_item, gpointer user_data)
{
	NautilusViewIdentifier *identifier;
	char *uri;

	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (user_data == NULL);

	identifier = (NautilusViewIdentifier *)gtk_object_get_data 
		(GTK_OBJECT (menu_item), "identifier");
	g_assert (identifier != NULL);

	uri = (char *)gtk_object_get_data (GTK_OBJECT (menu_item), "uri");
	g_assert (uri != NULL);

	/* FIXME: Need to switch to new viewer, opening new window if user's
	 * preference is set that way, with the given uri.
	 */
	g_message ("POOF! (switch to \"%s\" with viewer \"%s\" here)", uri, identifier->iid);
}

static void
add_application_to_gtk_menu (GtkMenu *menu, 
			     GnomeVFSMimeApplication *application, 
			     const char *uri)
{
	GtkWidget *menu_item;
	char *label_string;

	g_assert (GTK_IS_MENU (menu));

	label_string = g_strdup (application->name);
	menu_item = gtk_menu_item_new_with_label (label_string);
	g_free (label_string);

	gtk_object_set_data_full (GTK_OBJECT (menu_item),
				  "application",
				  gnome_vfs_mime_application_copy (application),
				  (GtkDestroyNotify)gnome_vfs_mime_application_free);

	gtk_object_set_data_full (GTK_OBJECT (menu_item),
				  "uri",
				  g_strdup (uri),
				  g_free);

	gtk_signal_connect
		(GTK_OBJECT (menu_item),
		 "activate",
		 launch_application_from_menu_item,
		 NULL);

	finish_adding_menu_item (menu, menu_item, TRUE);
}

static void
add_component_to_gtk_menu (GtkMenu *menu, 
			   OAF_ServerInfo *component, 
			   const char *uri)
{
	GtkWidget *menu_item;
	NautilusViewIdentifier *identifier;

	g_assert (GTK_IS_MENU (menu));

	identifier = nautilus_view_identifier_new_from_content_view (component);

	menu_item = gtk_menu_item_new_with_label (identifier->name);

	gtk_object_set_data_full (GTK_OBJECT (menu_item),
				  "identifier",
				  identifier,
				  (GtkDestroyNotify)nautilus_view_identifier_free);

	gtk_object_set_data_full (GTK_OBJECT (menu_item),
				  "uri",
				  g_strdup (uri),
				  g_free);

	gtk_signal_connect
		(GTK_OBJECT (menu_item),
		 "activate",
		 view_uri_from_menu_item,
		 NULL);

	finish_adding_menu_item (menu, menu_item, TRUE);
}

static GtkMenu *
create_open_with_gtk_menu (FMDirectoryView *view, GList *files)
{
 	GtkMenu *open_with_menu;
 	GList *applications, *components;
 	GList *node;
 	char *uri;

	open_with_menu = GTK_MENU (gtk_menu_new ());
	gtk_widget_show (GTK_WIDGET (open_with_menu));

	/* This menu is only displayed when there's one selected item. */
	if (nautilus_g_list_exactly_one_item (files)) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (files->data));

		applications = 
			gnome_vfs_mime_get_short_list_applications_for_uri (uri);

		for (node = applications; node != NULL; node = node->next) {
			add_application_to_gtk_menu (open_with_menu, node->data, uri);
		}

		gnome_vfs_mime_application_list_free (applications); 

		append_gtk_menu_item_with_view (view,
						open_with_menu,
			 			files,
			 			FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION,
			 			other_application_callback);

		add_open_with_gtk_menu_item (open_with_menu, NULL);

		components = 
			gnome_vfs_mime_get_short_list_components_for_uri (uri);

		for (node = components; node != NULL; node = node->next) {
			add_component_to_gtk_menu (open_with_menu, node->data, uri);
		}

		gnome_vfs_mime_component_list_free (components); 

		g_free (uri);

		append_gtk_menu_item_with_view (view,
						open_with_menu,
						files,
			 			FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER,
			 			other_viewer_callback);
	}

	return open_with_menu;
}

static void
fm_directory_view_real_create_selection_context_menu_items (FMDirectoryView *view,
							    GtkMenu *menu,
						       	    GList *files)
{
	append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_OPEN,
				    	open_callback);
	append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW,
				    	open_in_new_window_callback);
	append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER,
				    	bonobo_menu_new_folder_callback);
	append_selection_menu_subtree (view, menu, 
				       create_open_with_gtk_menu (view, files), files,
				       FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH);
	append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_TRASH,
				    	trash_callback);
	append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE,
				    	duplicate_callback);
	append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES,
				    	open_properties_window_callback);
        append_gtk_menu_item_with_view (view, menu, files,
				    	FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS,
				   	remove_custom_icons_callback);
}

static void
insert_bonobo_menu_item (BonoboUIHandler *ui_handler,
                         GList *selection,
                         const char *path,
                         const char *hint,
                         int position,
			 guint accelerator_key, 
			 GdkModifierType ac_mods,
                         BonoboUIHandlerCallbackFunc callback,
                         gpointer callback_data)
{
        char *label;
        gboolean sensitive;

        compute_menu_item_info (path, selection, TRUE, &label, &sensitive);
        bonobo_ui_handler_menu_new_item
		(ui_handler, path, label, hint, 
		 position,                      /* Position, -1 means at end */
		 BONOBO_UI_HANDLER_PIXMAP_NONE, /* Pixmap type */
		 NULL,                          /* Pixmap data */
		 accelerator_key,               /* Accelerator key */
		 ac_mods,                       /* Modifiers for accelerator */
		 callback, callback_data);
        g_free (label);
        bonobo_ui_handler_menu_set_sensitivity (ui_handler, path, sensitive);
}

static void
insert_bonobo_menu_subtree (BonoboUIHandler *ui_handler,
                            GList *selection,
                            const char *path,
                            const char *hint,
                            int position,
			    guint accelerator_key, 
			    GdkModifierType ac_mods)
{
        char *label;
        gboolean sensitive;

        compute_menu_item_info (path, selection, TRUE, &label, &sensitive);
        bonobo_ui_handler_menu_new_subtree
		(ui_handler, path, label, hint, 
		 position,                      /* Position, -1 means at end */
		 BONOBO_UI_HANDLER_PIXMAP_NONE, /* Pixmap type */
		 NULL,                          /* Pixmap data */
		 accelerator_key,               /* Accelerator key */
		 ac_mods);                      /* Modifiers for accelerator */
        g_free (label);
        bonobo_ui_handler_menu_set_sensitivity (ui_handler, path, sensitive);
}

static void
add_open_with_bonobo_menu_item (BonoboUIHandler *ui_handler,
				const char *label)
{
	char *path;
	
	path = bonobo_ui_handler_build_path 
		(FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH,
		 label,
		 NULL);
	bonobo_ui_handler_menu_new_item
		(ui_handler, path, label, NULL,
		 -1, BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
		 0, 0,
		 NULL, NULL);
	g_free (path);
}				

static void
add_application_to_bonobo_menu (BonoboUIHandler *ui_handler, 
				GnomeVFSMimeApplication *application, 
				const char *uri)
{
	/* FIXME: Need to pass application and uri somehow to callback */
	add_open_with_bonobo_menu_item (ui_handler, application->name);
}

static void
add_component_to_bonobo_menu (BonoboUIHandler *ui_handler, 
			      NautilusViewIdentifier *identifier, 
			      const char *uri)
{
	/* FIXME: Need to pass identifier and uri somehow to callback */
	add_open_with_bonobo_menu_item (ui_handler, identifier->name);
}


static void
reset_bonobo_trash_delete_menu (FMDirectoryView *view, BonoboUIHandler *ui_handler, GList *selection)
{
	bonobo_ui_handler_menu_remove (ui_handler, 
		 FM_DIRECTORY_VIEW_MENU_PATH_TRASH);
	bonobo_ui_handler_menu_remove (ui_handler, 
		 FM_DIRECTORY_VIEW_MENU_PATH_DELETE);

	if (fm_directory_can_move_to_trash (view)) {
		insert_bonobo_menu_item 
			(ui_handler, selection,
			 FM_DIRECTORY_VIEW_MENU_PATH_TRASH,
			 _("Move all selected items to the Trash"),
			 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_CLOSE_ITEM) + 3,
			  'T', GDK_CONTROL_MASK,
			 bonobo_menu_move_to_trash_callback, view);
	} else {
		insert_bonobo_menu_item 
			(ui_handler, selection,
			 FM_DIRECTORY_VIEW_MENU_PATH_DELETE,
			 _("Delete all selected items"),
			 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_CLOSE_ITEM) + 3,
			 0, 0,
			 bonobo_menu_move_to_trash_callback, view);
	}
}

static void
reset_bonobo_open_with_menu (FMDirectoryView *view, BonoboUIHandler *ui_handler, GList *selection)
{
	GList *applications, *components;
	GList *node;
	char *uri;

	/* Remove old copy of this menu (if any) */
	bonobo_ui_handler_menu_remove 
		(ui_handler, 
		 FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH);

	insert_bonobo_menu_subtree 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH,
		 _("Choose a program with which to open the selected item"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM) + 4,
		 0, 0);

	/* This menu is only displayed when there's one selected item. */
	if (nautilus_g_list_exactly_one_item (selection)) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));

		applications = gnome_vfs_mime_get_short_list_applications_for_uri (uri);

		for (node = applications; node != NULL; node = node->next) {
			add_application_to_bonobo_menu (ui_handler, node->data, uri);
		}

		/* FIXME: Need to free list, but API not yet existent:
		 * gnome_vfs_mime_application_list_free (applications); 
		 */

		insert_bonobo_menu_item 
			(ui_handler, selection,
			 FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION,
			 _("Choose another application with which to open the selected item"),
			 -1,
			 0, 0,
			 bonobo_menu_other_program_callback, view);

		bonobo_ui_handler_menu_new_separator 
			(ui_handler, 
			 FM_DIRECTORY_VIEW_MENU_PATH_SEPARATOR_BEFORE_VIEWERS, 
			 -1);

		components = gnome_vfs_mime_get_short_list_components_for_uri (uri);

		for (node = components; node != NULL; node = node->next) {
			add_component_to_bonobo_menu (ui_handler, node->data, uri);
		}

		/* FIXME: Need to free list, but API not yet existent:
		 * gnome_vfs_mime_component_list_free (components); 
		 */

		insert_bonobo_menu_item 
			(ui_handler, selection,
			 FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER,
			 _("Choose another viewer with which to view the selected item"),
			 -1,
			 0, 0,
			 bonobo_menu_other_program_callback, view);

		g_free (uri);
	}
}

static void
fm_directory_view_real_merge_menus (FMDirectoryView *view)
{
        GList *selection;
        BonoboUIHandler *ui_handler;

        selection = fm_directory_view_get_selection (view);
        ui_handler = fm_directory_view_get_bonobo_ui_handler (view);

	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER,
		 _("Create a new folder in this window"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM) + 1,
		  0, 0,	/* Accelerator will be inherited */
		 (BonoboUIHandlerCallbackFunc) bonobo_menu_new_folder_callback, view);
	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_OPEN,
		 _("Open the selected item in this window"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM) + 2,
		 'O', GDK_CONTROL_MASK,
		 bonobo_menu_open_callback, view);
	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW,
		 _("Open each selected item in a new window"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM) + 3,
		 0, 0,
		 bonobo_menu_open_in_new_window_callback, view);
	reset_bonobo_open_with_menu (view, ui_handler, selection);

        bonobo_ui_handler_menu_new_separator
		(ui_handler,
		 FM_DIRECTORY_VIEW_MENU_PATH_SEPARATOR_AFTER_CLOSE,
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_CLOSE_ITEM) + 1);

	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES,
		 _("View or modify the properties of the selected items"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_CLOSE_ITEM) + 2,
		 0, 0,
		 bonobo_menu_open_properties_window_callback, view);

	reset_bonobo_trash_delete_menu (view, ui_handler, selection);

	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE,
		 _("Duplicate all selected items"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_CLOSE_ITEM) + 4,
		  'D', GDK_CONTROL_MASK,
		 bonobo_menu_duplicate_callback, view);
	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH,
		 _("Delete all items in the trash"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_CLOSE_ITEM) + 5,
		  0, 0,
		 bonobo_menu_empty_trash_callback, view);
	insert_bonobo_menu_item 
		(ui_handler, selection,
		 NAUTILUS_MENU_PATH_SELECT_ALL_ITEM,
		 _("Select all items in this window"),
		 bonobo_ui_handler_menu_get_pos (ui_handler, NAUTILUS_MENU_PATH_SELECT_ALL_ITEM),
		  0, 0,	/* Accelerator will be inherited */
		 bonobo_menu_select_all_callback, view);

        bonobo_ui_handler_menu_new_separator
		(ui_handler,
		 FM_DIRECTORY_VIEW_MENU_PATH_SEPARATOR_BEFORE_ICONS,
		 -1);

	insert_bonobo_menu_item 
		(ui_handler, selection,
		 FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS,
		 _("Remove the custom image from each selected icon"),
		 -1,
		  0, 0,	/* Accelerator will be inherited */
		 (BonoboUIHandlerCallbackFunc) remove_custom_icons_callback, view);

        nautilus_file_list_free (selection);
}

static void
update_one_menu_item (BonoboUIHandler *ui_handler,
		      GList *selection,
		      const char *menu_path)
{
	char *label_string;
	gboolean sensitive;

        compute_menu_item_info (menu_path, selection, TRUE, &label_string, &sensitive);
        bonobo_ui_handler_menu_set_sensitivity (ui_handler, menu_path, sensitive);
        bonobo_ui_handler_menu_set_label (ui_handler, menu_path, label_string);
        g_free (label_string);
}

static void
fm_directory_view_real_update_menus (FMDirectoryView *view)
{
	BonoboUIHandler *handler;
	GList *selection;

	handler = fm_directory_view_get_bonobo_ui_handler (view);
	selection = fm_directory_view_get_selection (view);

	update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN);
	update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW);

	reset_bonobo_open_with_menu (view, handler, selection);
	reset_bonobo_trash_delete_menu (view, handler, selection);

	update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_TRASH);
	update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE);
	update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES);
	update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH);
        update_one_menu_item (handler, selection,
			      FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS);


	nautilus_file_list_free (selection);
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
	if (selected_files != NULL) {
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
	fm_directory_view_create_background_context_menu_items (view, menu);

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
			= gtk_idle_add (display_selection_info_idle_callback,
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

	request.requested_uri = nautilus_file_get_mapped_uri (file);
	request.new_window_requested = use_new_window;
	nautilus_view_request_location_change (view->details->nautilus_view,
					       &request);

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
	gboolean use_new_window_preference;

	use_new_window_preference = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
								      FALSE);

	fm_directory_view_activate_file_internal (view,
						  file,
						  request_new_window || use_new_window_preference);
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
	NautilusDirectory *old_model;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (uri != NULL);

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	disconnect_model_handlers (view);

	old_model = view->details->model;
	view->details->model = nautilus_directory_get (uri);
	nautilus_directory_unref (old_model);

	nautilus_directory_call_when_ready
		(view->details->model,
		 NULL, TRUE,
		 metadata_ready_callback, view);
}

static void
finish_loading_uri (FMDirectoryView *view)
{
	Nautilus_ProgressRequestInfo progress;
	GList *attributes;

	memset (&progress, 0, sizeof (progress));
	progress.type = Nautilus_PROGRESS_UNDERWAY;
	nautilus_view_request_progress_change (view->details->nautilus_view,
					       &progress);

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_LOADING]);

	schedule_timeout_display_of_pending_files (view);
	view->details->loading = TRUE;

	/* Start loading. */
	attributes = g_list_prepend (NULL,
				     NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT);
	nautilus_directory_file_monitor_add (view->details->model, view,
					     attributes, FALSE, TRUE,
					     files_added_callback, view);
	g_list_free (attributes);

	/* Attach a handler to get any further files that show up as we
	 * load and sychronize. We won't miss any files because this
	 * signal is emitted from an idle routine and so we will be
	 * connected before the next time it is emitted.
	 */
    	view->details->files_added_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model),
		 "files_added",
		 GTK_SIGNAL_FUNC (files_added_callback),
		 view);
	view->details->files_changed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model), 
		 "files_changed",
		 GTK_SIGNAL_FUNC (files_changed_callback),
		 view);
}

static void
metadata_ready_callback (NautilusDirectory *directory,
			 GList *files,
			 gpointer callback_data)
{
	FMDirectoryView *view;

	view = callback_data;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (files == NULL);
	g_assert (view->details->model == directory);

	finish_loading_uri (view);
}

static void
get_required_metadata_keys (FMDirectoryView *view,
			    GList **directory_keys_result,
			    GList **file_keys_result)
{
	GList *directory_keys;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (directory_keys_result != NULL);
	g_assert (file_keys_result != NULL);

	directory_keys = NULL;

	/* This needs to be a list of all the metadata needed.
	 * For now, it's kinda hard-coded. Later we might want
	 * to gather this info from various sources.
	 */
	directory_keys = g_list_prepend (directory_keys,
					 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR);
	directory_keys = g_list_prepend (directory_keys,
					 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE);

	*directory_keys_result = directory_keys;
	*file_keys_result = NULL;
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
	disconnect_handler (view, &view->details->files_changed_handler_id);
	if (view->details->model != NULL) {
		nautilus_directory_file_monitor_remove (view->details->model, view);
	}
	nautilus_directory_cancel_callback (view->details->model,
					    metadata_ready_callback,
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

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->select_all) (view);
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

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->set_selection) (view, selection);
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
show_hidden_files_changed_callback (gpointer		user_data)
{
	FMDirectoryView	*directory_view;
	char		*same_uri;

	g_assert (FM_IS_DIRECTORY_VIEW (user_data));

	directory_view = FM_DIRECTORY_VIEW (user_data);

	directory_view->details->show_hidden_files = 
		nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES, FALSE);

	/* Reload the current uri so that the filtering changes take place. */
	if (directory_view->details->model != NULL) {
		same_uri = nautilus_directory_get_uri (directory_view->details->model);
		g_assert (same_uri != NULL);
		fm_directory_view_load_uri (directory_view, same_uri);
		g_free (same_uri);
	}
}

char *
fm_directory_view_get_uri (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	return nautilus_directory_get_uri (view->details->model);
}

void
fm_directory_view_move_copy_items (const GList *item_uris,
				   const GdkPoint *relative_item_points,
				   const char *target_dir,
				   int copy_action,
				   int x,
				   int y,
				   FMDirectoryView *view)
{
	fs_xfer (item_uris, relative_item_points, target_dir, copy_action, GTK_WIDGET (view));
}

gboolean
fm_directory_view_can_accept_item (NautilusFile *target_item,
				   const char *item_uri,
				   FMDirectoryView *view)
{
	g_assert (NAUTILUS_IS_FILE (target_item));
	g_assert (FM_IS_DIRECTORY_VIEW (view));

	return nautilus_drag_can_accept_item (target_item, item_uri);
}

/**
 * fm_directory_view_get_context_menu_index:
 * 
 * Return index of menu item in context menu.  This function only
 * works if it is referring to the context menu that is displayed
 * when it is brought up in response to a click on an icon item.
 * Return -1 if item is not found
 * 
 * @menu_name: Item index to be returned.
 * 
 * 
 */
gint
fm_directory_view_get_context_menu_index(const char *menu_name)
{
	if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_OPEN, menu_name) == 0) {
		return 0;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW, menu_name) == 0) {
		return 1;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH, menu_name) == 0) {
		return 2;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_DELETE, menu_name) == 0) {
		return 3;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_TRASH, menu_name) == 0) {
		return 3;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE, menu_name) == 0) {
		return 4;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES, menu_name) == 0) {
		return 5;
	} else if (g_strcasecmp(FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS, menu_name) == 0) {
		return 6;
	/* Separator at position 7 */
	} else if (g_strcasecmp(NAUTILUS_MENU_PATH_SELECT_ALL_ITEM, menu_name) == 0) {
		return 8;
	} else {
		/* No match found */
		return -1;
	}
}
