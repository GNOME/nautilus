/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "nautilus-window-manage-views.h"

#include "nautilus-applicable-views.h"
#include "nautilus-application.h"
#include "nautilus-main.h"
#include "nautilus-location-bar.h"
#include "nautilus-window-private.h"
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-mime-actions.h>
#include <libnautilus-extensions/nautilus-search-uri.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <stdarg.h>

/* FIXME bugzilla.eazel.com 1243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

/*#define EXTREME_DEBUGGING*/

#ifdef EXTREME_DEBUGGING
#define x_message(parameters) g_message parameters
#else
#define x_message(parameters)
#endif

/* This number controls a maximum character count for a Nautilus window title
 * (not counting the prefix 'Nautilus:')
 * 
 * Without limiting the window title, the window manager makes the window wide
 * enough to able to display the whole title.  When this happens, the Nautilus
 * window in question becomes unusable.
 *
 * This is a very common thing to happen, especially with generated web content,
 * such as bugzilla queries, which generate very long urls.
 *
 * I found the number experimentally.  To properly compute it, we would need 
 * window manager support to access the character metrics for the window title.
 */
#define MAX_TITLE_LENGTH 180

/* This number controls a maximum character count for a URL that is displayed
 * as part of a dialog. It's fairly arbitrary -- big enough to allow most
 * "normal" URIs to display in full, but small enough to prevent the dialog
 * from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

static void nautilus_window_set_state_info (NautilusWindow *window, ...);

void
nautilus_window_report_selection_change (NautilusWindow *window,
                                         GList *selection,
                                         NautilusViewFrame *view)
{
        GList *sorted, *node;

        /* Sort list into canonical order and check if it's the same as
         * the selection we already have.
         */
        sorted = nautilus_g_str_list_sort (nautilus_g_str_list_copy (selection));
        if (nautilus_g_str_list_equal (sorted, window->selection)) {
                nautilus_g_list_free_deep (sorted);
                return;
        }

        /* Store the new selection. */
        nautilus_g_list_free_deep (window->selection);
        window->selection = sorted;

        /* Tell all the view frames about it. */
        nautilus_view_frame_selection_changed (window->content_view, sorted);
        for (node = window->sidebar_panels; node != NULL; node = node->next) {
                nautilus_view_frame_selection_changed
                        (NAUTILUS_VIEW_FRAME (node->data), sorted);
        }
}

void
nautilus_window_report_status (NautilusWindow *window,
                               const char *status,
                               NautilusViewFrame *view)
{
        nautilus_window_set_status (window, status);
}

/* Since these can be called while the window is still getting set up,
 * they don't do any work directly.
 */
void
nautilus_window_report_load_underway (NautilusWindow *window,
                                      NautilusViewFrame *view)
{
        /* FIXME bugzilla.eazel.com 2460: OK to ignore progress from sidebar views? */
        /* FIXME bugzilla.eazel.com 2461: Is progress from either old or new really equally interesting? */
        if (view == window->new_content_view
            || view == window->content_view) {
               nautilus_window_set_state_info
                       (window,
                        (NautilusWindowStateItem) CV_PROGRESS_INITIAL,
                        (NautilusWindowStateItem) 0);
        }
}

void
nautilus_window_report_load_complete (NautilusWindow *window,
                                      NautilusViewFrame *view)
{
        /* FIXME bugzilla.eazel.com 2460: OK to ignore progress from sidebar views? */
        /* FIXME bugzilla.eazel.com 2461: Is progress from either old or new really equally interesting? */
        if (view == window->new_content_view
            || view == window->content_view) {
               nautilus_window_set_state_info
                       (window,
                        (NautilusWindowStateItem) CV_PROGRESS_DONE,
                        (NautilusWindowStateItem) 0);
        }
}

void
nautilus_window_report_load_failed (NautilusWindow *window,
                                    NautilusViewFrame *view)
{
        /* FIXME bugzilla.eazel.com 2460: OK to ignore progress from sidebar views? */
        /* FIXME bugzilla.eazel.com 2461: Is progress from either old or new really equally interesting? */
        if (view == window->new_content_view
            || view == window->content_view) {
               nautilus_window_set_state_info
                       (window,
                        (NautilusWindowStateItem) CV_PROGRESS_ERROR,
                        (NautilusWindowStateItem) 0);
        }
}

static char *
compute_default_title (const char *text_uri)
{
	NautilusFile *file;
	char *title;

	if (text_uri == NULL) {
		title = g_strdup ("");
	} else {
		file = nautilus_file_get (text_uri);
		title = nautilus_file_get_name (file);
		nautilus_file_unref (file);
	}

	return title;
}

/* compute_title:
 * 
 * Get a newly allocated copy of the user-displayable title for the current
 * location. Note that the window title is related to this but might not
 * be exactly this.
 * @window: The NautilusWindow in question.
 * 
 * Return value: A newly allocated string. Use g_free when done with it.
 */
static char *
compute_title (NautilusWindow *window)
{
        char *title;

	title = NULL;
        if (window->new_content_view != NULL) {
                title = nautilus_view_frame_get_title (window->new_content_view);
        } else if (window->content_view != NULL) {
                title = nautilus_view_frame_get_title (window->content_view);
        }
        if (title == NULL) {
                title = compute_default_title (window->location);
        }
        return title;
}

/* update_title:
 * 
 * Update the non-NautilusViewFrame objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 * 
 */
static void
update_title (NautilusWindow *window)
{
        char *title;
        char *window_title;
        GList *node;

        title = compute_title (window);

        /* Remember the title and check if it's the same as last time. */
        if (window->details->title != NULL
            && strcmp (title, window->details->title) == 0) {
                g_free (title);
                return;
        }
        g_free (window->details->title);
        window->details->title = g_strdup (title);

        if (title[0] == '\0') {
                gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));
        } else {
                window_title = nautilus_str_middle_truncate (title, MAX_TITLE_LENGTH);
                gtk_window_set_title (GTK_WINDOW (window), window_title);
                g_free (window_title);
        }

        nautilus_sidebar_set_title (window->sidebar, title);
        nautilus_bookmark_set_name (window->current_location_bookmark, title);

        g_free (title);
        
        /* Name of item in history list may have changed, tell listeners. */
        nautilus_send_history_list_changed ();

        /* warn all views and sidebar panels of the potential title change */
        if (window->content_view != NULL) {
                nautilus_view_frame_title_changed (window->content_view, title);
        }
        if (window->new_content_view != NULL) {
                nautilus_view_frame_title_changed (window->new_content_view, title);
        }
        for (node = window->sidebar_panels; node != NULL; node = node->next) {
                nautilus_view_frame_title_changed (NAUTILUS_VIEW_FRAME (node->data), title);
        }
}

/* nautilus_window_set_displayed_location:
 * 
 * Update the non-NautilusViewFrame objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 */
static void
nautilus_window_set_displayed_location (NautilusWindow *window, const char *uri)
{
        char *bookmark_uri;
        gboolean recreate;

        if (window->current_location_bookmark == NULL) {
                recreate = TRUE;
        } else {
                bookmark_uri = nautilus_bookmark_get_uri (window->current_location_bookmark);
                recreate = !nautilus_uris_match (bookmark_uri, uri);
                g_free (bookmark_uri);
        }
        
        if (recreate) {
                /* We've changed locations, must recreate bookmark for current location. */
                if (window->last_location_bookmark != NULL)  {
                        gtk_object_unref (GTK_OBJECT (window->last_location_bookmark));
                }
                window->last_location_bookmark = window->current_location_bookmark;
                window->current_location_bookmark = nautilus_bookmark_new (uri, uri);
        }
        
        update_title (window);
}

void
nautilus_window_title_changed (NautilusWindow *window,
                               NautilusViewFrame *view)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

        /* Only the content view can change the window title. */
        if (view == window->content_view || view == window->new_content_view) {
                update_title (window);
        }
}

/* The bulk of this file - location changing */


/* Debugging function used to verify that the last_location_bookmark
 * is in the state we expect when we're about to use it to update the
 * Back or Forward list.
 */
static void
check_last_bookmark_location_matches_window (NautilusWindow *window)
{
	char *uri;
	gboolean result;

	uri = nautilus_bookmark_get_uri (window->last_location_bookmark);
	result = nautilus_uris_match (uri, window->location);
	if (!result) {
		 g_error ("last_location_bookmark is %s, but expected %s", uri, window->location);
	}
	g_free (uri);
}

static void
handle_go_back (NautilusWindow *window, const char *location)
{
        guint i;
        GList *link;        

        /* Going back. Move items from the back list to the forward list. */
        g_assert (g_list_length (window->back_list) > window->location_change_distance);
        nautilus_assert_computed_str
                (nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (g_list_nth_data (window->back_list,
                                                                                window->location_change_distance))),
                 location);
        g_assert (window->location != NULL);
        
        /* Move current location to Forward list */

        check_last_bookmark_location_matches_window (window);

        /* Use the first bookmark in the history list rather than creating a new one. */
        window->forward_list = g_list_prepend (window->forward_list, window->last_location_bookmark);
        gtk_object_ref (GTK_OBJECT (window->forward_list->data));
                                
        /* Move extra links from Back to Forward list */
        for (i = 0; i < window->location_change_distance; ++i) {
                link = window->back_list;
                window->back_list = g_list_remove_link (window->back_list, link);
                g_list_free_1 (link);
                window->forward_list = g_list_prepend (window->forward_list, link->data);
        }
        
        /* One bookmark falls out of back/forward lists and becomes viewed location */
        link = window->back_list;
        window->back_list = g_list_remove_link (window->back_list, link);
        gtk_object_unref (GTK_OBJECT (link->data));
        g_list_free_1 (link);
}

static void
handle_go_forward (NautilusWindow *window, const char *location)
{
        guint i;
        GList *link;

        /* Going forward. Move items from the forward list to the back list. */
        g_assert (g_list_length (window->forward_list) > window->location_change_distance);
        nautilus_assert_computed_str
                (nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (g_list_nth_data (window->forward_list,
                                                                                window->location_change_distance))),
                 location);
        g_assert (window->location != NULL);
                                
        /* Move current location to Back list */

        check_last_bookmark_location_matches_window (window);
        
        /* Use the first bookmark in the history list rather than creating a new one. */
        window->back_list = g_list_prepend (window->back_list, window->last_location_bookmark);
        gtk_object_ref (GTK_OBJECT (window->back_list->data));
        
        /* Move extra links from Forward to Back list */
        for (i = 0; i < window->location_change_distance; ++i) {
                link = window->forward_list;
                window->forward_list = g_list_remove_link (window->forward_list, link);
                window->back_list = g_list_prepend (window->back_list, link->data);
                g_list_free_1 (link);
        }
        
        /* One bookmark falls out of back/forward lists and becomes viewed location */
        link = window->forward_list;
        window->forward_list = g_list_remove_link (window->forward_list, link);
        gtk_object_unref (GTK_OBJECT (link->data));
        g_list_free_1 (link);
}

static void
handle_go_elsewhere (NautilusWindow *window, const char *location)
{
       /* Clobber the entire forward list, and move displayed location to back list */
        nautilus_window_clear_forward_list (window);
                                
        if (window->location != NULL) {
                /* If we're returning to the same uri somehow, don't put this uri on back list. 
                 * This also avoids a problem where nautilus_window_set_displayed_location
                 * didn't update last_location_bookmark since the uri didn't change.
                 */
                if (!nautilus_uris_match (window->location, location)) {
                        /* Store bookmark for current location in back list, unless there is no current location */
	                check_last_bookmark_location_matches_window (window);
                        
                        /* Use the first bookmark in the history list rather than creating a new one. */
                        window->back_list = g_list_prepend (window->back_list, window->last_location_bookmark);
                        gtk_object_ref (GTK_OBJECT (window->back_list->data));
                }
        }
}

static void
update_up_button (NautilusWindow *window)
{
        gboolean allowed;
        GnomeVFSURI *new_uri;

        allowed = FALSE;
        if (window->location != NULL) {
                new_uri = gnome_vfs_uri_new (window->location);
                if (new_uri != NULL) {
                        allowed = gnome_vfs_uri_has_parent (new_uri);
                        gnome_vfs_uri_unref (new_uri);
                }
        }
        nautilus_window_allow_up (window, allowed);
}

static void
viewed_file_changed_callback (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Close window if the file it's viewing has been deleted. */
	if (nautilus_file_is_gone (window->details->viewed_file)) {
		nautilus_window_close (window);
	}
}

/* Handle the changes for the NautilusWindow itself. */
static void
nautilus_window_update_internals (NautilusWindow *window)
{
        const char *new_location;
        
        if (window->pending_ni != NULL) {
                new_location = window->pending_ni->location;
                
                /* Maintain history lists. */
                if (window->location_change_type != NAUTILUS_LOCATION_CHANGE_RELOAD) {
                        /* Always add new location to history list. */
                        nautilus_add_to_history_list (window->current_location_bookmark);
                        
                        /* Update back and forward list. */
                        if (window->location_change_type == NAUTILUS_LOCATION_CHANGE_BACK) {
                                handle_go_back (window, new_location);
                        } else if (window->location_change_type == NAUTILUS_LOCATION_CHANGE_FORWARD) {
                                handle_go_forward (window, new_location);
                        } else {
                                g_assert (window->location_change_type == NAUTILUS_LOCATION_CHANGE_STANDARD);
                                handle_go_elsewhere (window, new_location);
                        }
                }
                
                /* Set the new location. */
                g_free (window->location);
                window->location = g_strdup (new_location);

                /* Create a NautilusFile for this location, so we can
                 * check if it goes away.
                 */
                nautilus_file_unref (window->details->viewed_file);
                window->details->viewed_file = nautilus_file_get (window->location);
                gtk_signal_connect_object_while_alive (GTK_OBJECT (window->details->viewed_file),
                		    		       "changed", 
                		    		       viewed_file_changed_callback, 
                		    		       GTK_OBJECT (window));
                
                /* Clear the selection. */
                nautilus_g_list_free_deep (window->selection);
                window->selection = NULL;
                
                /* Check if we can go up. */
                update_up_button (window);
                
                /* Set up the content view menu for this new location. */
                nautilus_window_load_content_view_menu (window);
        }
        
        /* Check if the back and forward buttons need enabling or disabling. */
        nautilus_window_allow_back (window, window->back_list != NULL);
        nautilus_window_allow_forward (window, window->forward_list != NULL);
        
        /* Change the location bar to match the current location. */
        nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (window->navigation_bar),
                                              window->location);
        
        /* Notify the sidebar of the location change. */
        /* FIXME bugzilla.eazel.com 211:
         * Eventually, this will not be necessary when we restructure the 
         * sidebar itself to be a NautilusViewFrame.
         */
        nautilus_sidebar_set_uri (window->sidebar,
                                  window->location,
                                  window->details->title);
}

static void
update_view (NautilusViewFrame *view,
             const char *new_location,
             GList *new_selection)
{
        nautilus_view_frame_load_location (view, new_location);
        nautilus_view_frame_selection_changed (view, new_selection);
}

void
nautilus_window_view_failed (NautilusWindow *window, NautilusViewFrame *view)
{
        nautilus_window_set_state_info
                (window,
                 (NautilusWindowStateItem) VIEW_ERROR, view,
                 (NautilusWindowStateItem) 0);
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
nautilus_window_has_really_changed (NautilusWindow *window)
{
        /* Switch to the new content view. */
        if (window->new_content_view != NULL) {
                if (GTK_WIDGET (window->new_content_view)->parent == NULL) {
                	nautilus_window_disconnect_view (window, window->content_view);
                        nautilus_window_set_content_view_widget (window, window->new_content_view);
                }
                gtk_object_unref (GTK_OBJECT (window->new_content_view));
                window->new_content_view = NULL;
                
		/* Update displayed view in menu. Only do this if we're not switching
		 * locations though, because if we are switching locations we'll
		 * install a whole new set of views in the menu later (the current
		 * views in the menu are for the old location).
		 */
                if (window->pending_ni == NULL) {
                	nautilus_window_synch_content_view_menu (window);
                }
	}


        /* Tell the window we are finished. */
        if (window->pending_ni != NULL) {
                nautilus_window_update_internals (window);
                nautilus_navigation_info_free (window->pending_ni);
                if (window->pending_ni == window->cancel_tag) {
                        window->cancel_tag = NULL;
                }
                window->pending_ni = NULL;
        }

        update_title (window);
}

/* This is called when we are done loading to get rid of the load_info structure. */
static void
nautilus_window_free_load_info (NautilusWindow *window)
{
        x_message (("-> FREE_LOAD_INFO <-"));
        
        if (window->pending_ni != NULL) {
                nautilus_navigation_info_free (window->pending_ni);
                window->pending_ni = NULL;
        }

        window->error_views = NULL;
        window->new_content_view = NULL;
        window->cancel_tag = NULL;
        window->views_shown = FALSE;
        window->view_bombed_out = FALSE;
        window->view_activation_complete = FALSE;
        window->cv_progress_initial = FALSE;
        window->cv_progress_done = FALSE;
        window->cv_progress_error =  FALSE;
        window->sent_update_view = FALSE;
        window->reset_to_idle = FALSE;
}

static gboolean
handle_unreadable_location (NautilusWindow *window, const char *uri)
{
	NautilusFile *file;
	gboolean unreadable;
	char *file_name;
        char *message;

	/* FIXME bugzilla.eazel.com 866: Can't expect to read the
	 * permissions instantly here. We might need to wait for
	 * a stat first.
	 */
	file = nautilus_file_get (uri);

	/* Can't make file object; can't check permissions; can't determine
	 * whether file is readable so return FALSE.
	 */
	if (file == NULL) {
		return FALSE;
	}

	unreadable = !nautilus_file_can_read (file);

	if (unreadable) {
		file_name = nautilus_file_get_name (file);
        	message = g_strdup_printf (_("You do not have the permissions necessary to view \"%s\"."), file_name);
                g_free (file_name);
                nautilus_error_dialog (message, _("Inadequate Permissions"), GTK_WINDOW (window));
                g_free (message);
	}

	nautilus_file_unref (file);

	return unreadable;
}

static void
open_location (NautilusWindow *window,
               const char *location,
               gboolean force_new_window,
               GList *new_selection)
{
        NautilusWindow *existing_window;
        gboolean create_new_window;
	GSList *node;

	/* empty location doesn't jive with our logic, if there are any characters,
	 * it will work ok, even if there is space */
	if (location[0] == '\0') {
		return;
	}
	
        if (handle_unreadable_location (window, location)) {
		return;
        }
        
        create_new_window = force_new_window;
	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window) && window->content_view != NULL) {
                create_new_window = TRUE;
        }

        if (create_new_window) {
                /* Determine if a window with this uri is already open.  If so, activate it */
		/* FIXME bugzilla.eazel.com 2464: 
		 * This may be the desired bahavior, but the prefs UI still says open
		 * new window.  How can we resolve this inconsistancy?
		 */                 
		for (node = nautilus_application_windows (); node != NULL; node = node->next) {
			existing_window = NAUTILUS_WINDOW (node->data);
			if (existing_window->location != NULL
                            && nautilus_uris_match (existing_window->location, location)) {
				gtk_widget_show_now (GTK_WIDGET (existing_window));
				nautilus_gdk_window_bring_to_front (GTK_WIDGET (existing_window)->window);								
				return;
			}
		}

		/* No open window found.  Create a new one. */
                open_location (nautilus_application_create_window (window->application), 
                               location, FALSE, new_selection);
                return;
        }

	nautilus_g_list_free_deep (window->pending_selection);
        window->pending_selection = nautilus_g_str_list_copy (new_selection);

        nautilus_window_begin_location_change
               (window, location,
                NAUTILUS_LOCATION_CHANGE_STANDARD, 0);
}

void
nautilus_window_open_location (NautilusWindow *window,
                               const char *location)
{
        open_location (window, location, FALSE, NULL);
}

void
nautilus_window_open_location_in_new_window (NautilusWindow *window,
                                             const char *location,
                                             GList *selection)
{
        open_location (window, location, TRUE, selection);
}

static NautilusViewFrame *
load_content_view (NautilusWindow *window,
                   NautilusViewIdentifier *id)
{
        const char *iid;
        NautilusViewFrame *content_view;
        NautilusViewFrame *new_view;

 	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
        	/* We force the desktop to use a desktop_icon_view. It's simpler
        	 * to fix it here than trying to make it pick the right view in
        	 * the first place.
        	 */
		iid = NAUTILUS_DESKTOP_ICON_VIEW_IID;
	} else {
        	g_return_val_if_fail (id, NULL);
        	iid = id->iid;
        }
        
        /* Assume new content is not zoomable. When/if it sends a zoom_level_changed
         * the zoom_control will get shown.
         */
	gtk_widget_hide (window->zoom_control);
	bonobo_ui_handler_menu_set_sensitivity (window->ui_handler,
						NAUTILUS_MENU_PATH_ZOOM_IN_ITEM,
						FALSE);
	bonobo_ui_handler_menu_set_sensitivity (window->ui_handler,
						NAUTILUS_MENU_PATH_ZOOM_OUT_ITEM,
						FALSE);
	bonobo_ui_handler_menu_set_sensitivity (window->ui_handler,
						NAUTILUS_MENU_PATH_ZOOM_NORMAL_ITEM,
						FALSE);
        
        content_view = window->content_view;
        if (!NAUTILUS_IS_VIEW_FRAME (content_view)
            || strcmp (nautilus_view_frame_get_iid (content_view), iid) != 0) {

                new_view = nautilus_view_frame_new (window->ui_handler,
                                                    window->application->undo_manager);
                nautilus_window_connect_view (window, new_view);
                
                if (!nautilus_view_frame_load_client (new_view, iid)) {
                        gtk_widget_unref (GTK_WIDGET(new_view));
                        new_view = NULL;
                }
                
                /* Avoid being fooled by extra done notifications from the last view. 
                   This is a HACK because the state machine SUCKS. */
                window->cv_progress_done = FALSE;
                window->cv_progress_error = FALSE;
        } else {
                new_view = window->content_view;
        }
        
        if (!NAUTILUS_IS_VIEW_FRAME (new_view)) {
                new_view = NULL;
        } else {
                gtk_object_ref (GTK_OBJECT (new_view));
                
		nautilus_view_identifier_free (window->content_view_id);
                window->content_view_id = nautilus_view_identifier_copy (id);
        }
        
        return new_view;
}

static void
report_content_view_failure_to_user (NautilusWindow *window)
{
	char *message;

	message = g_strdup_printf ("The %s view encountered an error and can't continue. "
				   "You can choose another view or go to a different location.", 
				   window->content_view_id->name);
	nautilus_error_dialog (message, _("View Failed"), GTK_WINDOW (window));
	g_free (message);
}

static void
report_sidebar_panel_failure_to_user (NautilusWindow *window)
{
	char *message;

        if (window->details->dead_view_name == NULL) {
                message = g_strdup
                        (_("One of the sidebar panels encountered an error and can't continue. "
                           "Unfortunately I couldn't tell which one."));
        } else {
                message = g_strdup_printf
                        (_("The %s sidebar panel encountered an error and can't continue. "
                           "If this keeps happening, you might want to turn this panel off."),
                         window->details->dead_view_name);
        }

	nautilus_error_dialog (message, _("Sidebar Panel Failed"), GTK_WINDOW (window));

	g_free (message);
}

static void
handle_view_failure (NautilusWindow *window,
                     NautilusViewFrame *view)
{
        if (view == window->new_content_view) {
                window->reset_to_idle = TRUE;
                window->cv_progress_error = TRUE;
        } else if (view == window->content_view) {
                if (GTK_WIDGET (window->content_view)->parent) {
                        gtk_container_remove (GTK_CONTAINER (GTK_WIDGET (window->content_view)->parent),
                                              GTK_WIDGET (window->content_view));
                }
                report_content_view_failure_to_user (window);
                window->content_view = NULL;
                window->cv_progress_error = TRUE;
        } else {
                report_sidebar_panel_failure_to_user (window);
        }
        
        nautilus_window_remove_sidebar_panel (window, view);
}

static void
cancel_location_change (NautilusWindow *window)
{
        GList *node;

        if (window->cancel_tag != NULL) {
                nautilus_navigation_info_cancel (window->cancel_tag);
                window->cancel_tag = NULL;
        }
        
        if (window->pending_ni != NULL) {
                nautilus_window_set_displayed_location
                        (window, window->location == NULL ? "" : window->location);
                
                /* Tell previously-notified views to go back to the old page */
                for (node = window->sidebar_panels; node != NULL; node = node->next) {
                        update_view (node->data, window->location, window->selection);
                }
                
                if (window->new_content_view != NULL
                    && window->new_content_view == window->content_view) {
                        update_view (window->content_view, window->location, window->selection);
                }
        }
        
        if (window->new_content_view != NULL) {
                gtk_widget_unref (GTK_WIDGET (window->new_content_view));
        }
        
        nautilus_window_free_load_info (window);
        
        nautilus_window_allow_stop (window, FALSE);
}

static void
load_view_for_new_location (NautilusWindow *window)
{
        window->new_content_view = load_content_view
                (window, window->pending_ni->initial_content_id);
}

static void
set_view_location_and_selection (NautilusWindow *window)
{
        const char *location;
        GList *selection, *node;
        
        if (window->pending_ni != NULL) {
                location = window->pending_ni->location;
                selection = window->pending_selection;
        } else {
                g_assert (window->pending_selection == NULL);
                location = window->location;
                selection = window->selection;
        }
        
        nautilus_window_set_displayed_location (window, location);
        
        x_message (("!!! Sending update_view"));
        
        if (window->new_content_view != NULL) {
                update_view (window->new_content_view, location, selection);
        } else {
                /* FIXME bugzilla.eazel.com 2457: Silent error here! */
                window->cv_progress_error = TRUE;
        }
        
        for (node = window->sidebar_panels; node != NULL; node = node->next) {
                update_view (node->data, location, selection);
        }
        
        nautilus_g_list_free_deep (window->pending_selection);
        window->pending_selection = NULL;
}

static gboolean
nautilus_window_update_state (gpointer data)
{
        NautilusWindow *window;
        GList *p;
        gboolean made_changes;
	
        window = data;

        if (window->making_changes) {
                x_message (("In the middle of making changes %d - RETURNING",
                            window->making_changes));
                return FALSE;
        }
        
        made_changes = FALSE;
        window->making_changes++;
        
#ifdef EXTREME_DEBUGGING
        g_message (">>> nautilus_window_update_state:");
        g_print ("making_changes %d\n", window->making_changes);
        g_print ("location_change_type %d, views_shown %d, view_bombed_out %d, view_activation_complete %d\n",
                 window->location_change_type, window->views_shown,
                 window->view_bombed_out, window->view_activation_complete);
        g_print ("sent_update_view %d, cv_progress_initial %d, cv_progress_done %d, cv_progress_error %d, reset_to_idle %d\n",
                 window->sent_update_view, window->cv_progress_initial, window->cv_progress_done, window->cv_progress_error,
                 window->reset_to_idle);
#endif
        
        /* Now make any needed state changes based on available information */

        if (window->view_bombed_out) {
                window->view_bombed_out = FALSE;

                for (p = window->error_views; p != NULL; p = p->next) {
                        handle_view_failure (window, NAUTILUS_VIEW_FRAME (p->data));
        
                        /* The dead_view_name refers only to the first error_view, so
                         * clear it out here after handling the first one. Subsequent
                         * times through this loop, if that ever actually happens, nothing
                         * will happen here.
                         */
                        g_free (window->details->dead_view_name);
                        window->details->dead_view_name = NULL;
        
                        gtk_widget_unref (GTK_WIDGET (p->data));
                }

                g_list_free (window->error_views);
                window->error_views = NULL;

                made_changes = TRUE;
        }
        
        if (window->reset_to_idle) {
                x_message (("Reset to idle!"));

                window->reset_to_idle = FALSE;
                
                cancel_location_change (window);

                made_changes = TRUE;
        }
        
        x_message (("Changes pending"));
        
        if (window->pending_ni != NULL
            && window->new_content_view == NULL
            && !window->cv_progress_error
            && !window->view_activation_complete) {
                
                load_view_for_new_location (window);
                
                window->view_activation_complete = TRUE;
                made_changes = TRUE;
        }
        
        if (window->view_activation_complete
            && !window->sent_update_view) {
                
                set_view_location_and_selection (window);
                
                window->sent_update_view = TRUE;
                made_changes = TRUE;
        }
        
        if (!window->cv_progress_error
            && window->view_activation_complete
            && window->cv_progress_initial
            && !window->views_shown) {
                
                nautilus_window_has_really_changed (window);
                
                window->views_shown = TRUE;
                made_changes = TRUE;
        }
        
        if (window->cv_progress_error
            || window->cv_progress_done) {
                made_changes = TRUE;
                window->reset_to_idle = TRUE;
        }
        
        window->making_changes--;
        
        x_message(("update_state done (making_changes is %d) <<<",
                   window->making_changes));
        
        return made_changes;
}

static void
nautilus_window_set_state_info (NautilusWindow *window, ...)
{
        va_list args;
        NautilusWindowStateItem item_type;
        NautilusViewFrame *new_view;

        /* Ensure that changes happen in-order */
        while (nautilus_window_update_state (window)) {
        }
        
        va_start (args, window);
        
        while ((item_type = va_arg (args, NautilusWindowStateItem)) != 0) {
                switch (item_type) {
                        
                case NAVINFO_RECEIVED: /* The information needed for a location change to continue has been received */
                        x_message (("NAVINFO_RECEIVED"));
                        window->pending_ni = va_arg(args, NautilusNavigationInfo*);
                        window->cancel_tag = NULL;
                        break;

                case VIEW_ERROR:
                        new_view = va_arg (args, NautilusViewFrame*);
                        x_message (("VIEW_ERROR on %p", new_view));
                        g_warning ("A view failed. The UI will handle this with a dialog but this should be debugged.");
                        window->view_bombed_out = TRUE;
                        /* Get label now, since view frame may be destroyed later. */
                        /* FIXME: We're only saving the name of the first error_view
			 * here. The rest of this code is structured to handle multiple
			 * error_views. I didn't go to the extra effort of saving a 
			 * name with teach error_view since (A) we only see one at a
			 * time in practice, and (B) all this code is likely to be
			 * rewritten soon.
			 */
                        if (window->details->dead_view_name == NULL) {
	                        window->details->dead_view_name = nautilus_view_frame_get_label (new_view);
                        }
                        gtk_object_ref (GTK_OBJECT (new_view));
                        window->error_views = g_list_prepend (window->error_views, new_view);
                        break;

                case NEW_CONTENT_VIEW_ACTIVATED:
                        x_message (("NEW_CONTENT_VIEW_ACTIVATED"));
                        g_return_if_fail (window->new_content_view == NULL);
                        new_view = va_arg (args, NautilusViewFrame*);
                        /* Don't ref here, reference is held by widget hierarchy. */
                        window->new_content_view = new_view;
                        /* We only come here in cases where the location does not change,
                         * so the sidebar panels don't change either.
                         */
                        if (window->pending_ni == NULL) {
                                window->view_activation_complete = TRUE;
                        }
                        window->views_shown = FALSE;
                        break;

                case CV_PROGRESS_INITIAL: /* We have received an "I am loading" indication from the content view */
                        x_message (("CV_PROGRESS_INITIAL"));
                        window->cv_progress_initial = TRUE;
                        break;

                case CV_PROGRESS_ERROR: /* We have received a load error from the content view */
                        x_message (("CV_PROGRESS_ERROR"));
                        window->cv_progress_error = TRUE;
                        break;

                case CV_PROGRESS_DONE: /* The content view is done loading */
                        x_message (("CV_PROGRESS_DONE"));
                        if (!window->cv_progress_initial) {
                                window->cv_progress_initial = TRUE;
                        }
                        window->cv_progress_done = TRUE;
                        break;

                case RESET_TO_IDLE: /* Someone pressed the stop button or something */
                        x_message (("RESET_TO_IDLE"));
                        window->reset_to_idle = TRUE;
                        break;

                default:
                        break;
                }
        }
        
        va_end (args);
        
        while (nautilus_window_update_state (window)) {
        }
}

static void
position_and_show_window_callback (NautilusDirectory *directory,
                       		   GList *files,
                       		   gpointer callback_data)
{
	NautilusWindow *window;
	char *geometry_string;

	window = NAUTILUS_WINDOW (callback_data);

	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
					      FALSE)) {
		geometry_string = nautilus_directory_get_metadata 
			(directory, NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY, NULL);
		if (geometry_string != NULL) {
			nautilus_gtk_window_set_initial_geometry_from_string 
				(GTK_WINDOW (window), 
				 geometry_string,
				 NAUTILUS_WINDOW_MIN_WIDTH, 
				 NAUTILUS_WINDOW_MIN_HEIGHT);
		}
		g_free (geometry_string);
	}

	gtk_widget_show (GTK_WIDGET (window));
	
	/* This directory object was reffed for this callback */
	nautilus_directory_unref (directory);
}                       			     

static void
nautilus_window_end_location_change_callback (NautilusNavigationResult result_code,
                                              NautilusNavigationInfo *navi,
                                              gpointer data)
{
        NautilusWindow *window;
        NautilusFile *file;
        NautilusDirectory *directory;
        const char *requested_uri;
        char *full_uri_for_display;
        char *uri_for_display;
        char *error_message;
        char *scheme_string;
        char *type_string;
        char *dialog_title;
 	GnomeDialog *dialog;
       
        g_assert (navi != NULL);
        
        window = NAUTILUS_WINDOW (data);
        window->location_change_end_reached = TRUE;
        
        if (result_code == NAUTILUS_NAVIGATION_RESULT_OK) {
                /* Navigation successful. */
                window->cancel_tag = navi;

		/* If the window is not yet showing (as is the case for nascent
		 * windows), position and show it only after we've got the
		 * metadata (since position info is stored there).
		 */
                if (!GTK_WIDGET_VISIBLE (window)) {
	                directory = nautilus_directory_get (navi->location);
			nautilus_directory_call_when_ready (directory,
							    NULL,
							    TRUE,
							    position_and_show_window_callback,
							    window);
                }
						
                nautilus_window_set_state_info
                        (window, 
                         (NautilusWindowStateItem) NAVINFO_RECEIVED, navi, 
                         (NautilusWindowStateItem) 0);
                return;
        }
        
        /* Some sort of failure occurred. How 'bout we tell the user? */
        requested_uri = navi->location;
        full_uri_for_display = nautilus_format_uri_for_display (requested_uri);
	/* Truncate the URI so it doesn't get insanely wide. Note that even
	 * though the dialog uses wrapped text, if the URI doesn't contain
	 * white space then the text-wrapping code is too stupid to wrap it.
	 */
        uri_for_display = nautilus_str_middle_truncate (full_uri_for_display, MAX_URI_IN_DIALOG_LENGTH);
	g_free (full_uri_for_display);

	dialog_title = NULL;
        
        switch (result_code) {

        case NAUTILUS_NAVIGATION_RESULT_NOT_FOUND:
                error_message = g_strdup_printf
                        (_("Couldn't find \"%s\". Please check the spelling and try again."),
                         uri_for_display);
                break;

        case NAUTILUS_NAVIGATION_RESULT_INVALID_URI:
                error_message = g_strdup_printf
                        (_("\"%s\" is not a valid location. Please check the spelling and try again."),
                         uri_for_display);
                break;

        case NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE:
                /* FIXME bugzilla.eazel.com 866: Can't expect to read the
                 * permissions instantly here. We might need to wait for
                 * a stat first.
                 */
        	file = nautilus_file_get (requested_uri);
        	if (file == NULL) {
                        type_string = NULL;
                } else {
        		type_string = nautilus_file_get_string_attribute (file, "type");
			nautilus_file_unref (file);
                }
                if (type_string == NULL) {
	                error_message = g_strdup_printf
                                (_("Couldn't display \"%s\", because Nautilus cannot handle items of this unknown type."),
                                 uri_for_display);
        	} else {
	                error_message = g_strdup_printf
                                (_("Couldn't display \"%s\", because Nautilus cannot handle items of type \"%s\"."),
                                 uri_for_display, type_string);
			g_free (type_string);
        	}
                break;

        case NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME:
                /* Can't create a vfs_uri and get the method from that, because 
                 * gnome_vfs_uri_new might return NULL.
                 */
                scheme_string = nautilus_str_get_prefix (requested_uri, ":");
                g_assert (scheme_string != NULL);  /* Shouldn't have gotten this error unless there's a : separator. */
                error_message = g_strdup_printf (_("Couldn't display \"%s\", because Nautilus cannot handle %s: locations."),
                                                 uri_for_display, scheme_string);
                g_free (scheme_string);
                break;

	case NAUTILUS_NAVIGATION_RESULT_LOGIN_FAILED:
                error_message = g_strdup_printf (_("Couldn't display \"%s\", because the attempt to log in failed."),
                                                 uri_for_display);		
		break;

	case NAUTILUS_NAVIGATION_RESULT_ACCESS_DENIED:
                error_message = g_strdup_printf (_("Couldn't display \"%s\", because access was denied."),
                                                 uri_for_display);
		break;

	case NAUTILUS_NAVIGATION_RESULT_SERVICE_NOT_AVAILABLE:
		if (nautilus_is_search_uri (requested_uri)) {
			/* FIXME bugzilla.eazel.com 2458: Need to give
                         * the user better advice about what to do
                         * here.
                         */
			error_message = g_strdup_printf
                                (_("Searching is unavailable right now, because you either have no index, "
                                   "or the search service isn't running. "
                                   "Be sure that you have started the Medusa search service, and if you "
                                   "don't have an index, that the Medusa indexer is running."));
			dialog_title = g_strdup (_("Searching Unavailable"));
			break;
		} /* else fall through */
        default:
                error_message = g_strdup_printf (_("Nautilus cannot display \"%s\"."),
                                                 uri_for_display);
        }
        
        if (navi != NULL) {
                if (window->cancel_tag != NULL) {
                        g_assert (window->cancel_tag == navi);
                        window->cancel_tag = NULL;
                }
                nautilus_navigation_info_free (navi);
        }
        
        if (dialog_title == NULL) {
		dialog_title = g_strdup (_("Can't Display Location"));
        }

        if (!GTK_WIDGET_VISIBLE (GTK_WIDGET (window))) {
                /* Destroy never-had-a-chance-to-be-seen window. This case
                 * happens when a new window cannot display its initial URI. 
                 */

                dialog = nautilus_error_dialog (error_message, dialog_title, NULL);
                
                /* If window is the sole window open, destroying it will
                 * kill the main event loop and the dialog will go away
                 * before the user has a chance to see it. Prevent this
                 * by registering the dialog before calling destroy.
                 */
                if (nautilus_main_is_event_loop_mainstay (GTK_OBJECT (window))) {
                	nautilus_main_event_loop_register (GTK_OBJECT (dialog));
		}

                /* Since this is a window, destroying it will also unref it. */
                gtk_object_destroy (GTK_OBJECT (window));
        } else {
                /* Clean up state of already-showing window */
                nautilus_window_allow_stop (window, FALSE);
                nautilus_error_dialog (error_message, dialog_title, GTK_WINDOW (window));

                /* Leave the location bar showing the bad location that the user
                 * typed (or maybe achieved by dragging or something). Many times
                 * the mistake will just be an easily-correctable typo. The user
                 * can choose "Refresh" to get the original URI back in the location bar.
                 */
        }

	g_free (dialog_title);
	g_free (uri_for_display);
        g_free (error_message);
}

/*
 * nautilus_window_begin_location_change
 * 
 * Change a window's location.
 * @window: The NautilusWindow whose location should be changed.
 * @loc: A Nautilus_NavigationRequestInfo specifying info about this transition.
 * @type: Which type of location change is this? Standard, back, forward, or reload?
 * @distance: If type is back or forward, the index into the back or forward chain. If
 * type is standard or reload, this is ignored, and must be 0.
 */
void
nautilus_window_begin_location_change (NautilusWindow *window,
                                       const char *location,
                                       NautilusLocationChangeType type,
                                       guint distance)
{
        const char *current_iid;
        NautilusNavigationInfo *navigation_info;
        
        g_assert (NAUTILUS_IS_WINDOW (window));
        g_assert (location != NULL);
        g_assert (type == NAUTILUS_LOCATION_CHANGE_BACK
                  || type == NAUTILUS_LOCATION_CHANGE_FORWARD
                  || distance == 0);
        
        nautilus_window_set_state_info
                (window,
                 (NautilusWindowStateItem) RESET_TO_IDLE,
                 (NautilusWindowStateItem) 0);
        
        while (gdk_events_pending()) {
                gtk_main_iteration();
        }
        
        window->location_change_type = type;
        window->location_change_distance = distance;
        
        nautilus_window_allow_stop (window, TRUE);
        
        current_iid = NULL;
        if (window->content_view != NULL) {
                current_iid = nautilus_view_frame_get_iid (window->content_view);
        }
        
        /* If we just set the cancel tag in the obvious way here we run into
         * a problem where the cancel tag is set to point to bogus data, because
         * the navigation info is freed by the callback before _new returns.
         * To reproduce this problem, just use any illegal URI.
         */
        g_assert (window->cancel_tag == NULL);
        window->location_change_end_reached = FALSE;
        navigation_info = nautilus_navigation_info_new
                (location,
                 nautilus_window_end_location_change_callback,
                 window,
                 current_iid);
        if (!window->location_change_end_reached) {
                window->cancel_tag = navigation_info;
        }
}

void
nautilus_window_stop_loading (NautilusWindow *window)
{
        nautilus_window_set_state_info (window, RESET_TO_IDLE, 0);
}

void
nautilus_window_set_content_view (NautilusWindow *window, NautilusViewIdentifier *id)
{
        NautilusDirectory *directory;
	NautilusFile *file;
        NautilusViewFrame *view;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (window->location != NULL);
	g_return_if_fail (id != NULL);

        if (window->content_view != NULL) {
        }

        directory = nautilus_directory_get (window->location);
	file = nautilus_file_get (window->location);
	g_assert (directory != NULL);
        nautilus_mime_set_default_component_for_uri
		(directory, file, id->iid);
        nautilus_directory_unref (directory);
        nautilus_file_unref (file);
        
        nautilus_window_allow_stop (window, TRUE);
        
        view = load_content_view (window, id);
        nautilus_window_set_state_info
		(window,
		 (NautilusWindowStateItem) NEW_CONTENT_VIEW_ACTIVATED, view,
		 (NautilusWindowStateItem) 0);
}

static int
compare_view_identifier_with_iid (gconstpointer passed_view_identifier,
                                  gconstpointer passed_iid)
{
        return strcmp (((NautilusViewIdentifier *) passed_view_identifier)->iid,
                       (char *) passed_iid);
}

void
nautilus_window_set_sidebar_panels (NautilusWindow *window,
                                    GList *passed_identifier_list)
{
	GList *identifier_list;
	GList *node, *next, *found_node;
	NautilusViewFrame *sidebar_panel;
	NautilusViewIdentifier *identifier;
	gboolean load_succeeded;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* Make a copy of the list so we can remove items from it. */
	identifier_list = g_list_copy (passed_identifier_list);
	
	/* Remove panels from the window that don't appear in the list. */
	for (node = window->sidebar_panels; node != NULL; node = next) {
		next = node->next;

		sidebar_panel = NAUTILUS_VIEW_FRAME (node->data);
		
		found_node = g_list_find_custom (identifier_list,
						 (char *) nautilus_view_frame_get_iid (sidebar_panel),
						 compare_view_identifier_with_iid);
		if (found_node == NULL) {
			nautilus_window_disconnect_view	(window, sidebar_panel);
			nautilus_window_remove_sidebar_panel (window, sidebar_panel);
		} else {
                        identifier = (NautilusViewIdentifier *) found_node->data;

                        /* Right panel, make sure it has the right name. */
                        nautilus_view_frame_set_label (sidebar_panel, identifier->name);

                        /* Since this was found, there's no need to add it in the loop below. */
			identifier_list = g_list_remove_link (identifier_list, found_node);
			g_list_free_1 (found_node);
		}
        }

	/* Add panels to the window that were in the list, but not the window. */
	for (node = identifier_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		
		identifier = (NautilusViewIdentifier *) node->data;

                /* Create and load the panel. */
		sidebar_panel = nautilus_view_frame_new (window->ui_handler,
							 window->application->undo_manager);
		nautilus_view_frame_set_label (sidebar_panel, identifier->name);
		nautilus_window_connect_view (window, sidebar_panel);
		load_succeeded = nautilus_view_frame_load_client (sidebar_panel, identifier->iid);
		
		/* If the load failed, tell the user. */
		if (!load_succeeded) {
			/* FIXME: This needs to report the error to the user. */
			g_warning ("sidebar_panels_changed_callback: Failed to load_client for '%s' meta view.", 
				   identifier->iid);
			gtk_object_sink (GTK_OBJECT (sidebar_panel));
			continue;
		}

		/* If the load succeeded, add the panel. */
		nautilus_window_add_sidebar_panel (window, sidebar_panel);
	}

	g_list_free (identifier_list);
}
