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

/* Main operations needed:
 *   Initiate location change
 *   Initiate content view change
 *   Cancel action
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

void
nautilus_window_report_selection_change (NautilusWindow *window,
                                         GList *selection,
                                         NautilusViewFrame *view)
{
        GList *sorted, *p;

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
        nautilus_view_frame_selection_changed (window->content_view, selection);
        for (p = window->sidebar_panels; p != NULL; p = p->next) {
                nautilus_view_frame_selection_changed (p->data, selection);
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
nautilus_window_report_load_progress (NautilusWindow *window,
                                      double fraction_done,
                                      NautilusViewFrame *view)
{
        /* For now, we ignore the fraction_done parameter. */
        nautilus_window_report_load_underway (window, view);
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
        GnomeVFSURI *vfs_uri;
        char *short_name, *colon_pos;
	
        if (text_uri != NULL) {
                vfs_uri = gnome_vfs_uri_new (text_uri);
                if (vfs_uri != NULL) {
                        short_name = gnome_vfs_uri_extract_short_name (vfs_uri);
                        gnome_vfs_uri_unref (vfs_uri);
                        if (short_name == NULL) {
                                short_name = g_strdup (_("(untitled)"));
                        }
                        return short_name;
                } else {
                	colon_pos = strchr (text_uri, ':');
                	if (colon_pos != NULL) {
                                if (colon_pos[1] != '\0') {
                                        return g_strdup (colon_pos + 1);
                                } else {
                                        return g_strndup (text_uri,
                                                          colon_pos - text_uri);
                                }
                        }
                }
        }

        return g_strdup ("");
}

/* nautilus_window_get_current_location_title:
 * 
 * Get a newly allocated copy of the user-displayable title for the current
 * location. Note that the window title is related to this but might not
 * be exactly this.
 * @window: The NautilusWindow in question.
 * 
 * Return value: A newly allocated string. Use g_free when done with it.
 */
static char *
nautilus_window_get_current_location_title (NautilusWindow *window)
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

/* nautilus_window_update_title:
 * 
 * Update the non-NautilusViewFrame objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 * 
 */
static void
nautilus_window_update_title (NautilusWindow *window)
{
        char *title;
        char *window_title;
        char *truncated_title;
        GList *temp;

        title = nautilus_window_get_current_location_title (window);

        if (title[0] == '\0') {
                gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));
        } else {
                truncated_title = nautilus_str_middle_truncate (title, MAX_TITLE_LENGTH);
                window_title = g_strdup_printf (_("Nautilus: %s"), truncated_title);
                g_free (truncated_title);

                gtk_window_set_title (GTK_WINDOW (window), window_title);

                g_free (window_title);
        }
        
        nautilus_sidebar_set_title (window->sidebar, title);
        nautilus_bookmark_set_name (window->current_location_bookmark, title);

        /* Name of item in history list may have changed, tell listeners. */
        nautilus_send_history_list_changed ();

        /* warn all views and sidebar panels of the potential title change */
        if (window->content_view != NULL) {
                nautilus_view_frame_title_changed (window->content_view);
        }
        if (window->new_content_view != NULL) {
                nautilus_view_frame_title_changed (window->new_content_view);
        }
        
        for (temp = window->sidebar_panels; temp != NULL; temp = temp->next) {
                if (temp->data != NULL) {
                        nautilus_view_frame_title_changed (NAUTILUS_VIEW_FRAME (temp->data));
                }
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
        
        nautilus_window_update_title (window);
}

void
nautilus_window_title_changed (NautilusWindow *window,
                               NautilusViewFrame *view)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));

        /* Only the content view can change the window title. */
        if (view == window->content_view || view == window->new_content_view) {
                nautilus_window_update_title (window);
        }
}

/* The bulk of this file - location changing */

static gboolean
check_last_bookmark_location_matches_window (NautilusWindow *window)
{
	char *uri;
	gboolean result;

	uri = nautilus_bookmark_get_uri (window->last_location_bookmark);
	result = nautilus_uris_match (uri, window->location);
	if (!result) {
        	/* FIXME bugzilla.eazel.com 2707: This is always a bug, and there might be multiple bugs here.
                 * Right now one of them is so common that I'm changing this from an
                 * assert to a warning to stop blocking other work. When known bugs here are fixed,
                 * we should change this back to g_error.
                 */
		 g_warning ("last_location_bookmark is %s, but should match %s", uri, window->location);
	}
	g_free (uri);

	return result;
}

static void
handle_go_back (NautilusWindow *window, const char *location)
{
        guint i;

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
                NautilusBookmark *bookmark;
                
                bookmark = window->back_list->data;
                window->back_list = g_list_remove_link (window->back_list, window->back_list);
                window->forward_list = g_list_prepend (window->forward_list, bookmark);
        }
        
        /* One bookmark falls out of back/forward lists and becomes viewed location */
        gtk_object_unref (window->back_list->data);
        window->back_list = g_list_remove_link (window->back_list, window->back_list);
}

static void
handle_go_forward (NautilusWindow *window, const char *location)
{
        guint i;

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
                NautilusBookmark *bookmark;
                
                bookmark = window->forward_list->data;
                window->forward_list = g_list_remove_link (window->forward_list, window->forward_list);
                window->back_list = g_list_prepend (window->back_list, bookmark);
        }
        
        /* One bookmark falls out of back/forward lists and becomes viewed location */
        gtk_object_unref (window->forward_list->data);
        window->forward_list = g_list_remove_link (window->forward_list, window->forward_list);
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

/* Handle the changes for the NautilusWindow itself. */
static void
nautilus_window_update_internals (NautilusWindow *window)
{
        const char *new_location;
        char *current_title;
        
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
        current_title = nautilus_window_get_current_location_title (window);
        nautilus_sidebar_set_uri (window->sidebar, window->location, current_title);
        g_free (current_title);
}

static void
nautilus_window_update_view (NautilusWindow *window,
                             NautilusViewFrame *view,
                             const char *new_location,
                             GList *new_selection,
                             NautilusViewFrame *requesting_view,
                             NautilusViewFrame *content_view)
{
        if (view != requesting_view) {
                nautilus_view_frame_load_location (view, new_location);
        }
        
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
        GList *discard_views;
        GList *p;
        GList *new_sidebar_panels;
        NautilusViewFrame *view;
        
        new_sidebar_panels = window->new_sidebar_panels;
        window->new_sidebar_panels = NULL;

        /* Switch to the new content view. */
        if (window->new_content_view != NULL) {
                if (GTK_WIDGET (window->new_content_view)->parent == NULL) {
                	nautilus_window_disconnect_view (window, window->content_view);
                        nautilus_window_set_content_view (window, window->new_content_view);
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
        
                /* Remove sidebar views that aren't going to be kept. */
                discard_views = NULL;
                for (p = window->sidebar_panels; p != NULL; p = p->next) {
                        view = NAUTILUS_VIEW_FRAME (p->data);
                        
                        if (g_list_find (new_sidebar_panels, view) == NULL) {
                                discard_views = g_list_prepend (discard_views, view);
                        }
                }
                for (p = discard_views; p != NULL; p = p->next) {
                        view = NAUTILUS_VIEW_FRAME (p->data);
                        
                        nautilus_window_disconnect_view (window, view);
                        nautilus_window_remove_sidebar_panel (window, view);
                }
                g_list_free (discard_views);
                
                /* Add any new views */
                for (p = new_sidebar_panels; p != NULL; p = p->next) {
                        view = NAUTILUS_VIEW_FRAME (p->data);
                        
                        if (!GTK_OBJECT_DESTROYED (GTK_OBJECT (view))
                            && GTK_WIDGET (view)->parent == NULL) {
                                nautilus_window_add_sidebar_panel (window, view);
                        }
                }
        }

        nautilus_gtk_object_list_free (new_sidebar_panels);

        /* Tell the window we are finished. */
        if (window->pending_ni != NULL) {
                nautilus_window_update_internals (window);
                nautilus_navigation_info_free (window->pending_ni);
                if (window->pending_ni == window->cancel_tag) {
                        window->cancel_tag = NULL;
                }
                window->pending_ni = NULL;
        }

        nautilus_window_update_title (window);
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
        window->new_sidebar_panels = NULL;
        window->new_content_view = NULL;
        window->cancel_tag = NULL;
        if (window->action_tag != 0) {
		g_source_remove (window->action_tag);
	        window->action_tag = 0;
        }
        window->made_changes = 0;
        window->state = NW_IDLE;
        window->changes_pending = FALSE;
        window->views_shown = FALSE;
        window->view_bombed_out = FALSE;
        window->view_activation_complete = FALSE;
        window->cv_progress_initial = FALSE;
        window->cv_progress_done = FALSE;
        window->cv_progress_error =  FALSE;
        window->sent_update_view = FALSE;
        window->reset_to_idle = FALSE;
}

/* Meta view handling */
static NautilusViewFrame *
nautilus_window_load_sidebar_panel (NautilusWindow *window,
                                    const char *iid,
                                    NautilusViewFrame *requesting_view)
{
        NautilusViewFrame *sidebar_panel;
        GList *p;
        
        /* Find an existing sidebar panel. */
        sidebar_panel = NULL;
        for (p = window->sidebar_panels; p != NULL; p = p->next) {
                sidebar_panel = NAUTILUS_VIEW_FRAME (p->data);
                if (strcmp (nautilus_view_frame_get_iid (sidebar_panel), iid) == 0)
                        break;
        }
        
        /* Create a new sidebar panel. */
        if (p == NULL) {
                sidebar_panel = nautilus_view_frame_new (window->ui_handler,
                                                         window->application->undo_manager);
                nautilus_window_connect_view (window, sidebar_panel);
                if (!nautilus_view_frame_load_client (sidebar_panel, iid)) {
                        gtk_widget_unref (GTK_WIDGET (sidebar_panel));
                        sidebar_panel = NULL;
                }
        }
        
        if (sidebar_panel != NULL) {
                /* FIXME bugzilla.eazel.com 2463: Do we really want to ref even in the case
                 * where we just made the panel?
                 */
                gtk_object_ref (GTK_OBJECT (sidebar_panel));
                nautilus_view_frame_set_active_errors (sidebar_panel, TRUE);
        }
        
        return sidebar_panel;
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
                nautilus_error_dialog (message, _("Nautilus: Inadequate permissions"), GTK_WINDOW (window));
                g_free (message);
	}

	nautilus_file_unref (file);

	return unreadable;
}

static void
open_location (NautilusWindow *window,
               const char *location,
               gboolean force_new_window,
               GList *new_selection,
               NautilusViewFrame *view_if_already_loading)
{
        NautilusWindow *traverse_window;
        gboolean create_new_window;
	GSList *element;

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
                g_assert (view_if_already_loading == NULL);

                /* Determine if a window with this uri is already open.  If so, activate it */
		/* FIXME bugzilla.eazel.com 2464: 
		 * This may be the desired bahavior, but the prefs UI still says open
		 * new window.  How can we resolve this inconsistancy?
		 */                 
		for (element = nautilus_application_windows (); element != NULL; element = element->next) {
			traverse_window = element->data;
			if (traverse_window->location != NULL && nautilus_uris_match (traverse_window->location, location)) {
				gtk_widget_show_now (GTK_WIDGET (traverse_window));
				nautilus_gdk_window_bring_to_front (GTK_WIDGET (traverse_window)->window);								
				return;
			}
		}

		/* No open window found.  Create a new one. */
                open_location (
                	nautilus_application_create_window (window->application), 
                	location, FALSE, new_selection, NULL);
                return;
        }

	nautilus_g_list_free_deep (window->pending_selection);
        window->pending_selection = nautilus_g_str_list_copy (new_selection);

        nautilus_window_begin_location_change
               (window, location,
                view_if_already_loading,
                NAUTILUS_LOCATION_CHANGE_STANDARD, 0);
}

void
nautilus_window_open_location (NautilusWindow *window,
                               const char *location,
                               NautilusViewFrame *view)
{
        /* Don't pass along the view because everyone should be told to load. */
        open_location (window, location, FALSE, NULL, NULL);
}

void
nautilus_window_open_location_in_new_window (NautilusWindow *window,
                                             const char *location,
                                             NautilusViewFrame *view)
{
        /* Don't pass along the view because everyone should be told to load. */
        open_location (window, location, TRUE, NULL, NULL);
}

void
nautilus_window_open_in_new_window_and_select (NautilusWindow *window,
                                               const char *location,
                                               GList *selection,
                                               NautilusViewFrame *view)
{
        /* Don't pass along the view because everyone should be told to load. */
        open_location (window, location, TRUE, selection, NULL);
}

void
nautilus_window_report_location_change (NautilusWindow *window,
                                        const char *location,
                                        NautilusViewFrame *view)
{
        open_location (window, location, FALSE, NULL, view);
}

NautilusViewFrame *
nautilus_window_load_content_view (NautilusWindow *window,
                                   NautilusViewIdentifier *id,
                                   NautilusViewFrame **requesting_view)
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

                if (requesting_view != NULL && *requesting_view == window->content_view) {
                        /* If we are going to be zapping the old view,
                         * we definitely don't want any of the new views
                         * thinking they made the request
                        */
                        *requesting_view = NULL;
                }
                
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
                
                nautilus_view_frame_set_active_errors (new_view, TRUE);
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
	nautilus_error_dialog (message, _("Nautilus: View failed"), GTK_WINDOW (window));
	g_free (message);
}

static void
report_sidebar_panel_failure_to_user (NautilusWindow *window, NautilusViewFrame *panel)
{
	char *name;
	char *message;

	name = nautilus_view_frame_get_label (panel);
        if (name == NULL) {
                message = g_strdup ("One of the sidebar panels encountered an error and can't continue. Unfortunately I couldn't tell which one. ");
        } else {
	message = g_strdup_printf ("The %s sidebar panel encountered an error and can't continue. "
			           "If this keeps happening, you might want to turn this panel off.",
			           name);
        }

	g_free (name);

	nautilus_error_dialog (message, _("Nautilus: Sidebar panel failed"), GTK_WINDOW (window));

	g_free (message);
}

static gboolean
nautilus_window_update_state (gpointer data)
{
        NautilusWindow *window;
        GList *p;
        gboolean result;
        GList *sidebar_panel_identifiers;
	
        window = data;

        if (window->making_changes) {
                x_message (("In the middle of making changes %d (action_tag %d) - RETURNING",
                            window->making_changes, window->action_tag));
                return FALSE;
        }
        
        window->made_changes = 0;
        window->making_changes++;
        
#ifdef EXTREME_DEBUGGING
        g_message(">>> nautilus_window_update_state (action tag is %d):", window->action_tag);
        g_print("made_changes %d, making_changes %d\n", window->made_changes, window->making_changes);
        g_print("changes_pending %d, location_change_type %d, views_shown %d, view_bombed_out %d, view_activation_complete %d\n",
                window->changes_pending, window->location_change_type, window->views_shown,
                window->view_bombed_out, window->view_activation_complete);
        g_print("sent_update_view %d, cv_progress_initial %d, cv_progress_done %d, cv_progress_error %d, reset_to_idle %d\n",
                window->sent_update_view, window->cv_progress_initial, window->cv_progress_done, window->cv_progress_error,
                window->reset_to_idle);
#endif
        
        /* Now make any needed state changes based on available information */
        if (window->view_bombed_out && window->error_views != NULL) {
                for (p = window->error_views; p != NULL; p = p->next) {
                        NautilusViewFrame *error_view = p->data;
                        
                        if (error_view == window->new_content_view) {
                                window->made_changes++;
                                window->reset_to_idle = TRUE;
                                window->cv_progress_error = TRUE;
                        }
                        
                        if (error_view == window->content_view) {
                                if (GTK_WIDGET (window->content_view)->parent) {
                                        gtk_container_remove (GTK_CONTAINER (GTK_WIDGET (window->content_view)->parent),
                                                              GTK_WIDGET (window->content_view));
                                }
                                report_content_view_failure_to_user (window);
                                window->content_view = NULL;
                                window->made_changes++;
                                window->cv_progress_error = TRUE;
                        }

                        report_sidebar_panel_failure_to_user (window, error_view);

                        if (g_list_find (window->new_sidebar_panels, error_view) != NULL) {
                                window->new_sidebar_panels = g_list_remove (window->new_sidebar_panels, error_view);
                                gtk_widget_unref (GTK_WIDGET (error_view));
                        }

                        nautilus_window_remove_sidebar_panel (window, error_view);

                        gtk_widget_unref (GTK_WIDGET (error_view));
                }
                g_list_free (window->error_views);
                window->error_views = NULL;
                
                window->view_bombed_out = FALSE;
        }
        
        if (window->reset_to_idle) {
                x_message (("Reset to idle!"));
                
                window->changes_pending = FALSE;
                window->made_changes++;
                window->reset_to_idle = FALSE;
                
                if (window->cancel_tag != NULL) {
                        nautilus_navigation_info_cancel (window->cancel_tag);
                        window->cancel_tag = NULL;
                }
                
                if (window->pending_ni != NULL) {
                        nautilus_window_set_displayed_location
                                (window, window->location == NULL ? "" : window->location);
                        
                        /* Tell previously-notified views to go back to the old page */
                        for (p = window->sidebar_panels; p != NULL; p = p->next) {
                                if (g_list_find (window->new_sidebar_panels, p->data) != NULL) {
                                        nautilus_window_update_view (window, p->data,
                                                                     window->location, window->selection,
                                                                     NULL, window->content_view);
                                }
                        }
                        
                        if (window->new_content_view
                            && window->new_content_view == window->content_view) {
                                nautilus_window_update_view (window, window->content_view,
                                                             window->location, window->selection,
                                                             NULL, window->content_view);
                        }
                }
                
                if (window->new_content_view != NULL) {
                        gtk_widget_unref (GTK_WIDGET (window->new_content_view));
                }
                for (p = window->new_sidebar_panels; p != NULL; p = p->next) {
                        gtk_widget_unref (GTK_WIDGET (p->data));
                }
                g_list_free (window->new_sidebar_panels);
                
                nautilus_window_free_load_info (window);
                
                nautilus_window_allow_stop (window, FALSE);
        }
        
        if (window->changes_pending) {
                window->state = NW_LOADING_VIEWS;
                
                x_message (("Changes pending"));
                
                if (window->pending_ni
                    && !window->new_content_view
                    && !window->cv_progress_error
                    && !window->view_activation_complete) {

                        window->new_content_view = nautilus_window_load_content_view
                                (window, window->pending_ni->initial_content_id,
                                 &window->new_requesting_view);

			sidebar_panel_identifiers = 
				nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers ();

                        for (p = sidebar_panel_identifiers; p != NULL; p = p->next) {
                                NautilusViewFrame *sidebar_panel;
                                NautilusViewIdentifier *identifier;
                                
                                identifier = (NautilusViewIdentifier *) p->data;
                                
                                sidebar_panel = nautilus_window_load_sidebar_panel
                                        (window, identifier->iid, window->new_requesting_view);
                                if (sidebar_panel != NULL) {
                                        nautilus_view_frame_set_label (sidebar_panel, identifier->name);
                                        window->new_sidebar_panels = g_list_prepend (window->new_sidebar_panels, sidebar_panel);
                                }
                        }
                        
                        nautilus_view_identifier_list_free (sidebar_panel_identifiers);
                        
                        window->view_activation_complete = TRUE;
                        window->made_changes++;
                }
                
                if (window->view_activation_complete
                    && !window->sent_update_view) {
                        const char *location;
                        GList *selection;
                        
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
                        
                        if (window->new_content_view) {
                                nautilus_window_update_view (window, window->new_content_view,
                                                             location, selection,
                                                             window->new_requesting_view, window->new_content_view);
                        } else {
                                /* FIXME bugzilla.eazel.com 2457: Silent error here! */
                                window->cv_progress_error = TRUE;
                        }
                        
                        for (p = window->new_sidebar_panels; p != NULL; p = p->next) {
                                nautilus_window_update_view (window, p->data,
                                                             location, selection,
                                                             window->new_requesting_view,
                                                             window->new_content_view);
                        }

                        nautilus_g_list_free_deep (window->pending_selection);
                        window->pending_selection = NULL;
                        
                        window->sent_update_view = TRUE;
                        window->made_changes++;
                }
                
                if (!window->cv_progress_error
                    && window->view_activation_complete
                    && window->cv_progress_initial
                    && !window->views_shown) {
                        
                        nautilus_window_has_really_changed (window);
                        window->views_shown = TRUE;
                        window->made_changes++;
                }
                
                if (window->cv_progress_error
                    || window->cv_progress_done) {

                        x_message (("cv_progress_(error|done) kicking in"));

                        window->made_changes++;
                        window->reset_to_idle = TRUE;
                }
        }
        
        if (window->made_changes) {
                if (!window->action_tag) {
                        window->action_tag = g_idle_add_full (G_PRIORITY_LOW,
                                                              nautilus_window_update_state,
                                                              window, NULL);
                }
                
                result = TRUE;
                window->made_changes = 0;
        } else {
                result = FALSE;
                window->action_tag = 0;
        }
        
        window->making_changes--;
        
        x_message(("update_state done (new action tag is %d, making_changes is %d) <<<",
                   window->action_tag, window->making_changes));
        
        return result;
}

void
nautilus_window_set_state_info (NautilusWindow *window, ...)
{
        va_list args;
        NautilusWindowStateItem item_type;
        NautilusViewFrame *new_view;
        gboolean do_sync;

        /* Ensure that changes happen in-order */
        if (window->made_changes != 0) {
                if (window->action_tag != 0) {
                        g_source_remove(window->action_tag);
                        window->action_tag = 0;
                }
                nautilus_window_update_state(window);
        }
        
        do_sync = FALSE;
        
        va_start (args, window);
        
        while ((item_type = va_arg (args, NautilusWindowStateItem)) != 0) {
                switch (item_type) {
                        
                case NAVINFO_RECEIVED: /* The information needed for a location change to continue has been received */
                        x_message (("NAVINFO_RECEIVED"));
                        window->pending_ni = va_arg(args, NautilusNavigationInfo*);
                        window->cancel_tag = NULL;
                        window->changes_pending = TRUE;
                        break;

                case VIEW_ERROR:
                        new_view = va_arg (args, NautilusViewFrame*);
                        x_message (("VIEW_ERROR on %p", new_view));
                        window->view_bombed_out = TRUE;
                        gtk_object_ref (GTK_OBJECT (new_view));
                        window->error_views = g_list_prepend (window->error_views, new_view);
                        break;

                case NEW_CONTENT_VIEW_ACTIVATED:
                        x_message (("NEW_CONTENT_VIEW_ACTIVATED"));
                        g_return_if_fail (window->new_content_view == NULL);
                        g_return_if_fail (window->new_sidebar_panels == NULL);
                        new_view = va_arg (args, NautilusViewFrame*);
                        /* Don't ref here, reference is held by widget hierarchy. */
                        window->new_content_view = new_view;
                        /* We only come here in cases where the location does not change,
                         * so the sidebar panels don't change either.
                         */
                        window->new_sidebar_panels = nautilus_gtk_object_list_copy (window->sidebar_panels);
                        if (window->pending_ni == NULL) {
                                window->view_activation_complete = TRUE;
                        }
                        window->changes_pending = TRUE;
                        window->views_shown = FALSE;
                        break;

                case CV_PROGRESS_INITIAL: /* We have received an "I am loading" indication from the content view */
                        x_message (("CV_PROGRESS_INITIAL"));
                        window->cv_progress_initial = TRUE;
                        window->changes_pending = TRUE;
                        break;

                case CV_PROGRESS_ERROR: /* We have received a load error from the content view */
                        x_message (("CV_PROGRESS_ERROR"));
                        window->cv_progress_error = TRUE;
                        break;

                case CV_PROGRESS_DONE: /* The content view is done loading */
                        x_message (("CV_PROGRESS_DONE"));
                        if (!window->cv_progress_initial) {
                                window->cv_progress_initial = TRUE;
                                window->changes_pending = TRUE;
                        }
                        window->cv_progress_done = TRUE;
                        break;

                case RESET_TO_IDLE: /* Someone pressed the stop button or something */
                        x_message (("RESET_TO_IDLE"));
                        window->reset_to_idle = TRUE;
                        do_sync = TRUE;
                        break;

                default:
                        break;
                }
        }
        
        va_end (args);
        
        window->made_changes++;
        if (!window->making_changes) {
                if (do_sync) {
                        if (window->action_tag != 0) {
                                x_message (("Doing sync - action_tag was %d",
                                            window->action_tag));
                                g_source_remove (window->action_tag);
                                window->action_tag = 0;
                        }
                        if (nautilus_window_update_state (window)) {
                                do_sync = FALSE;
                        }
                }
                
                if (window->action_tag == 0 && !do_sync) {
                        window->action_tag = g_idle_add_full (G_PRIORITY_LOW,
                                                              nautilus_window_update_state,
                                                              window, NULL);
                        x_message (("Added callback to update_state - tag is %d",
                                    window->action_tag));
                }
        }
}

static void
nautilus_window_end_location_change_callback (NautilusNavigationResult result_code,
                                              NautilusNavigationInfo *navi,
                                              gpointer data)
{
        NautilusWindow *window;
        NautilusFile *file;
        char *requested_uri;
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
                /* Navigation successful. Show the window to handle the
                 * new-window case. (Doesn't hurt if window is already showing.)
                 * Maybe this should go sometime later so the blank window isn't
                 * on screen so long.
                 */
                window->cancel_tag = navi;
                gtk_widget_show (GTK_WIDGET (window));
                nautilus_window_set_state_info
                        (window, 
                         (NautilusWindowStateItem) NAVINFO_RECEIVED, navi, 
                         (NautilusWindowStateItem) 0);
                return;
        }
        
        /* Some sort of failure occurred. How 'bout we tell the user? */
        requested_uri = navi->location;
	uri_for_display = nautilus_format_uri_for_display (requested_uri);

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
                                (_("Couldn't display \"%s\", because Nautilus cannot handle items of this type."),
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

	case NAUTILUS_NAVIGATION_RESULT_SERVICE_NOT_AVAILABLE:
		if (nautilus_is_search_uri (requested_uri)) {
			/* FIXME bugzilla.eazel.com 2458: Need to give the user some advice about what to do here. */
			error_message = g_strdup_printf (_("Sorry, searching can't be used now. In the future this message will be more helpful."));
			dialog_title = g_strdup (_("Nautilus: Searching unavailable"));
			break;
		} /* else fall through */
        default:
                error_message = g_strdup_printf (_("Nautilus cannot display \"%s\"."), uri_for_display);
        }
        
        if (navi != NULL) {
                if (window->cancel_tag != NULL) {
                        g_assert (window->cancel_tag == navi);
                        window->cancel_tag = NULL;
                }
                nautilus_navigation_info_free (navi);
        }
        
        if (dialog_title == NULL) {
		dialog_title = g_strdup (_("Nautilus: Can't display location"));
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

                /* FIXME bugzilla.eazel.com 2459: Is a destroy really sufficient here? Who does the unref? */
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
 * @requesting_view: The view from which this location change originated, can be NULL.
 * @type: Which type of location change is this? Standard, back, forward, or reload?
 * @distance: If type is back or forward, the index into the back or forward chain. If
 * type is standard or reload, this is ignored, and must be 0.
 */
void
nautilus_window_begin_location_change (NautilusWindow *window,
                                       const char *location,
                                       NautilusViewFrame *requesting_view,
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
        window->new_requesting_view = requesting_view;
        
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
