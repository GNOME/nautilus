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
#include "nautilus-zoom-control.h"
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <bonobo/bonobo-ui-util.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
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

/* FIXME bugzilla.eazel.com 1243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

/* This number controls a maximum character count for a Nautilus
 * window title. Without limiting the window title, most window
 * managers make the window wide enough to able to display the whole
 * title. When this happens, the Nautilus window in question becomes
 * unusable. This is a very common thing to happen, especially with
 * generated web content, such as bugzilla queries, which generate
 * very long urls. I found the number experimentally. To properly
 * compute it, we would need window manager support to access the
 * character metrics for the window title.
 */
#define MAX_TITLE_LENGTH 180

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

typedef enum {
        THIS_WINDOW,
        EXISTING_WINDOW,
        NEW_WINDOW
} OpenLocationWindow;

typedef struct {
	gboolean is_sidebar_panel;
	char *label;
} ViewFrameInfo;

static void connect_view           (NautilusWindow    *window, 
                                    NautilusViewFrame *view);
static void disconnect_view	   (NautilusWindow    *window,
                                    NautilusViewFrame *view);
static void disconnect_view_and_destroy        (NautilusWindow    *window,
                                                NautilusViewFrame *view);
static void disconnect_and_destroy_sidebar_panel (NautilusWindow    *window,
                                                  NautilusViewFrame *view);

static void cancel_location_change (NautilusWindow    *window);

static void
change_selection (NautilusWindow *window,
                  GList *selection)
{
        GList *sorted, *node;

        /* Sort list into canonical order and check if it's the same as
         * the selection we already have.
         */
        sorted = nautilus_g_str_list_alphabetize (nautilus_g_str_list_copy (selection));
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

/* window_set_title_with_time_stamp
 * 
 * Update the non-NautilusViewFrame objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 * 
 */
static void
window_set_title_with_time_stamp (NautilusWindow *window, const char *title)
{
        char *time_stamp;
	char *title_with_time_stamp;
	
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (title != NULL);

	time_stamp = nautilus_get_build_time_stamp ();
	
	if (time_stamp != NULL) {
		/* FIXME bugzilla.eazel.com 5037: The text Preview
		 * Release is hardcoded here. Are all builds with
		 * time stamps really best described as "preview
		 * release"?.
                 */
		title_with_time_stamp = g_strdup_printf (_("Nautilus (%s): %s"), time_stamp, title);
		gtk_window_set_title (GTK_WINDOW (window), title_with_time_stamp);
		g_free (title_with_time_stamp);
	} else {
		gtk_window_set_title (GTK_WINDOW (window), title);
	}

	g_free (time_stamp);
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
		window_set_title_with_time_stamp (window, _("Nautilus"));
        } else {
                window_title = nautilus_str_middle_truncate (title, MAX_TITLE_LENGTH);
		window_set_title_with_time_stamp (window, window_title);
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

/* set_displayed_location:
 * 
 * Update the non-NautilusViewFrame objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 */
static void
set_displayed_location (NautilusWindow *window, const char *uri)
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

/* The bulk of this file - location changing */

static void
check_bookmark_location_matches_uri (NautilusBookmark *bookmark, const char *uri)
{
	char *bookmark_uri;
	gboolean result;

	bookmark_uri = nautilus_bookmark_get_uri (bookmark);
	result = nautilus_uris_match (uri, bookmark_uri);
	if (!result) {
		g_warning ("bookmark uri is %s, but expected %s", bookmark_uri, uri);
	}
	g_free (bookmark_uri);
}

/* Debugging function used to verify that the last_location_bookmark
 * is in the state we expect when we're about to use it to update the
 * Back or Forward list.
 */
static void
check_last_bookmark_location_matches_window (NautilusWindow *window)
{
	check_bookmark_location_matches_uri (window->last_location_bookmark, window->location);
}

static void
handle_go_back (NautilusWindow *window, const char *location)
{
        guint i;
        GList *link;
        NautilusBookmark *bookmark;

        /* Going back. Move items from the back list to the forward list. */
        g_assert (g_list_length (window->back_list) > window->location_change_distance);
        check_bookmark_location_matches_uri (NAUTILUS_BOOKMARK (g_list_nth_data (window->back_list,
                                                                                window->location_change_distance)),
                                             location);
        g_assert (window->location != NULL);
        
        /* Move current location to Forward list */

        check_last_bookmark_location_matches_window (window);

        /* Use the first bookmark in the history list rather than creating a new one. */
        window->forward_list = g_list_prepend (window->forward_list, window->last_location_bookmark);
        gtk_object_ref (GTK_OBJECT (window->forward_list->data));
                                
        /* Move extra links from Back to Forward list */
        for (i = 0; i < window->location_change_distance; ++i) {
        	bookmark = NAUTILUS_BOOKMARK (window->back_list->data);
                window->back_list = g_list_remove (window->back_list, bookmark);
                window->forward_list = g_list_prepend (window->forward_list, bookmark);
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
        NautilusBookmark *bookmark;

        /* Going forward. Move items from the forward list to the back list. */
        g_assert (g_list_length (window->forward_list) > window->location_change_distance);
        check_bookmark_location_matches_uri (NAUTILUS_BOOKMARK (g_list_nth_data (window->forward_list,
                                                                                window->location_change_distance)),
                                             location);
        g_assert (window->location != NULL);
                                
        /* Move current location to Back list */

        check_last_bookmark_location_matches_window (window);
        
        /* Use the first bookmark in the history list rather than creating a new one. */
        window->back_list = g_list_prepend (window->back_list, window->last_location_bookmark);
        gtk_object_ref (GTK_OBJECT (window->back_list->data));
        
        /* Move extra links from Forward to Back list */
        for (i = 0; i < window->location_change_distance; ++i) {
        	bookmark = NAUTILUS_BOOKMARK (window->forward_list->data);
                window->forward_list = g_list_remove (window->forward_list, bookmark);
                window->back_list = g_list_prepend (window->back_list, bookmark);
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
                 * This also avoids a problem where set_displayed_location
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
viewed_file_changed_callback (NautilusFile *file,
                              NautilusWindow *window)
{
        char *new_location;

        g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (window->details->viewed_file == file);


	/* Close window if the file it's viewing has been deleted. */
	if (nautilus_file_is_gone (file)) {
                /* detecting a file is gone may happen in the middle
                 * of a pending location change, we need to cancel it
                 * before closing the window or things break.
                 */
                cancel_location_change (window);

                /* FIXME bugzilla.eazel.com 5038: Is closing the window really the right thing to do
                 * for all cases?
                 */
		nautilus_window_close (window);
	} else {
                new_location = nautilus_file_get_uri (file);

                /* if the file was renamed, update location 
                   and/or title */
                if (!nautilus_uris_match (new_location,
                                          window->location)) {
                        g_free (window->location);
                        window->location = new_location;

                        /* Check if we can go up. */
                        update_up_button (window);
                        /* Change the location bar to match the current location. */
                        nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (window->navigation_bar),
                                                              window->location);
                        update_title (window);
                } else {
                        g_free (new_location);
                }
        }
}

static void
cancel_viewed_file_changed_callback (NautilusWindow *window)
{
        if (window->details->viewed_file != NULL) {
                gtk_signal_disconnect_by_func (GTK_OBJECT (window->details->viewed_file),
                                               viewed_file_changed_callback,
                                               window);
        }
}

/* Handle the changes for the NautilusWindow itself. */
static void
update_for_new_location (NautilusWindow *window)
{
        char *new_location;
        NautilusFile *file;
        
        g_assert (window->pending_ni != NULL);

        new_location = nautilus_navigation_info_get_location (window->pending_ni);
        
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
        window->location = new_location;
        
        /* Create a NautilusFile for this location, so we can catch it
         * if it goes away.
         */
        cancel_viewed_file_changed_callback (window);
        file = nautilus_file_get (window->location);
        nautilus_window_set_viewed_file (window, file);
        gtk_signal_connect (GTK_OBJECT (file),
                            "changed",
                            viewed_file_changed_callback,
                            window);
        nautilus_file_unref (file);
        
        /* Clear the selection. */
        nautilus_g_list_free_deep (window->selection);
        window->selection = NULL;
        
        /* Check if we can go up. */
        update_up_button (window);
        
        /* Set up the content view menu for this new location. */
        nautilus_window_load_view_as_menu (window);
        
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

static gboolean
unref_callback (gpointer callback_data)
{
        gtk_object_unref (GTK_OBJECT (callback_data));
        return FALSE;
}

static void
ref_now_unref_at_idle_time (GtkObject *object)
{
        gtk_object_ref (object);
        g_idle_add (unref_callback, object);
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
location_has_really_changed (NautilusWindow *window)
{
        /* Switch to the new content view. */
        if (window->new_content_view != NULL) {
                if (GTK_WIDGET (window->new_content_view)->parent == NULL) {
                        /* If we don't unref the old view until idle
                         * time, we avoid certain kinds of problems in
                         * in-process components, since they won't
                         * lose their ViewFrame in the middle of some
                         * operation. This still doesn't necessarily
                         * help for out of process components.
                         */
                        if (window->content_view != NULL) {
                                ref_now_unref_at_idle_time (GTK_OBJECT (window->content_view));
                        }

                	disconnect_view (window, window->content_view);
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
                	nautilus_window_synch_view_as_menu (window);
                }
	}


        /* Tell the window we are finished. */
        if (window->pending_ni != NULL) {
                update_for_new_location (window);
                nautilus_navigation_info_free (window->pending_ni);
                if (window->pending_ni == window->cancel_tag) {
                        window->cancel_tag = NULL;
                }
                window->pending_ni = NULL;
        }

        update_title (window);
}

static gboolean
handle_unreadable_location (NautilusWindow *window, const char *location)
{
	NautilusFile *file;
	gboolean unreadable;
	char *file_name;
        char *message;

	/* An empty location doesn't jibe with our logic. It will
	 * work if there are any characters (even just one space).
         */
	if (location[0] == '\0') {
		return TRUE;
	}
	
	/* FIXME bugzilla.eazel.com 866: Can't expect to read the
	 * permissions instantly here. We might need to wait for
	 * a stat first.
	 */
	file = nautilus_file_get (location);

	/* If it's gone, it doesn't count as unreadable, and will be handled
	 * by the normal missing-uri mechanism.
	 */
	unreadable = !nautilus_file_is_gone (file) && !nautilus_file_can_read (file);

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
        NautilusWindow *target_window;
        gboolean create_new_window;
        
        if (handle_unreadable_location (window, location)) {
		return;
        }

        create_new_window = force_new_window;

	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window)
            && window->content_view != NULL) {
                create_new_window = TRUE;
        }

        if (create_new_window) {
                target_window = nautilus_application_create_window (window->application);
        } else {
                target_window = window;
        }

	nautilus_g_list_free_deep (target_window->pending_selection);
        target_window->pending_selection = nautilus_g_str_list_copy (new_selection);

        nautilus_window_begin_location_change
               (target_window, location,
                NAUTILUS_LOCATION_CHANGE_STANDARD, 0);
}

void
nautilus_window_open_location (NautilusWindow *window,
                               const char *location)
{
        open_location (window, location, FALSE, NULL);
}


static ViewFrameInfo *
view_frame_info_new (gboolean is_sidebar_panel, const char *label)
{
	ViewFrameInfo *new_info;

	g_return_val_if_fail (label != NULL, NULL);

	new_info = g_new0 (ViewFrameInfo, 1);
	new_info->is_sidebar_panel = is_sidebar_panel;
	new_info->label = g_strdup (label);

	return new_info;
}

static void
view_frame_info_free (ViewFrameInfo *info)
{
	if (info != NULL) {
		g_free (info->label);
		g_free (info);
	}
}

static void
set_view_frame_info (NautilusViewFrame *view_frame, 
		     gboolean is_sidebar_panel, 
		     const char *label)
{
	gtk_object_set_data_full (GTK_OBJECT (view_frame),
				  "info",
				  view_frame_info_new (is_sidebar_panel, label),
				  (GtkDestroyNotify) view_frame_info_free);
}

static gboolean
view_frame_is_sidebar_panel (NautilusViewFrame *view_frame)
{
	ViewFrameInfo *info;

	info = (ViewFrameInfo *)gtk_object_get_data 
		(GTK_OBJECT (view_frame), "info");
	return info->is_sidebar_panel;
}

static char *
view_frame_get_label (NautilusViewFrame *view_frame)
{
	ViewFrameInfo *info;

	info = (ViewFrameInfo *)gtk_object_get_data 
		(GTK_OBJECT (view_frame), "info");
	return g_strdup (info->label);
}

static void
report_content_view_failure_to_user_internal (NautilusWindow *window,
                                     	      NautilusViewFrame *view_frame,
                                     	      const char *message)
{
	char *label;

	label = view_frame_get_label (view_frame);
	message = g_strdup_printf (message, label);
	nautilus_error_dialog (message, _("View Failed"), GTK_WINDOW (window));
	g_free (label);
}

static void
report_current_content_view_failure_to_user (NautilusWindow *window,
                                     	     NautilusViewFrame *view_frame)
{
	report_content_view_failure_to_user_internal 
		(window,
		 view_frame,
		 _("The %s view encountered an error and can't continue. "
		   "You can choose another view or go to a different location."));
}

static void
report_nascent_content_view_failure_to_user (NautilusWindow *window,
                                     	     NautilusViewFrame *view_frame)
{
	report_content_view_failure_to_user_internal 
		(window,
		 view_frame,
		 _("The %s view encountered an error while starting up."));
}

static void
disconnect_destroy_unref_view (NautilusWindow *window, NautilusViewFrame *view_frame)
{
	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_VIEW_FRAME (view_frame));

	disconnect_view_and_destroy (window, view_frame);
	gtk_widget_unref (GTK_WIDGET (view_frame));
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
        
        bonobo_ui_component_freeze (window->details->shell_ui, NULL);
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_IN,
				       FALSE);
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_OUT,
				       FALSE);
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_NORMAL,
				       FALSE);
        
        bonobo_ui_component_thaw (window->details->shell_ui, NULL);

        content_view = window->content_view;
        if (content_view == NULL
            || strcmp (nautilus_view_frame_get_iid (content_view), iid) != 0) {

                new_view = nautilus_view_frame_new (window->details->ui_container,
                                                    window->application->undo_manager);
                gtk_object_ref (GTK_OBJECT (new_view));
                gtk_object_sink (GTK_OBJECT (new_view));
		set_view_frame_info (new_view, FALSE, id->name);
                connect_view (window, new_view);

                if (!nautilus_view_frame_load_client (new_view, iid)) {
                        /* FIXME bugzilla.eazel.com 5039: We need a way to report the specific
                           error that happens in this case - adapter
                           factory not found, component failed to
                           load, etc. */
                        report_nascent_content_view_failure_to_user (window, new_view);
                        disconnect_destroy_unref_view (window, new_view);

                        new_view = NULL;
                }
                
                /* Avoid being fooled by extra done notifications from the last view. 
                   This is a HACK because the state machine SUCKS. */
                window->cv_progress_done = FALSE;
                window->cv_progress_error = FALSE;
        } else {
        	gtk_object_ref (GTK_OBJECT (window->content_view));
                new_view = window->content_view;
        }
        
        if (new_view != NULL) {
		nautilus_view_identifier_free (window->content_view_id);
                window->content_view_id = nautilus_view_identifier_copy (id);
        }
        
        return new_view;
}



static void
report_sidebar_panel_failure_to_user (NautilusWindow *window, NautilusViewFrame *view_frame)
{
	char *message;
	char *label;

	label = view_frame_get_label (view_frame);

        if (label == NULL) {
                message = g_strdup
                        (_("One of the sidebar panels encountered an error and can't continue. "
                           "Unfortunately I couldn't tell which one."));
        } else {
                message = g_strdup_printf
                        (_("The %s sidebar panel encountered an error and can't continue. "
                           "If this keeps happening, you might want to turn this panel off."),
                         label);
        }

	nautilus_error_dialog (message, _("Sidebar Panel Failed"), GTK_WINDOW (window));

	g_free (label);
	g_free (message);
}

static void
handle_view_failure (NautilusWindow *window,
                     NautilusViewFrame *view)
{
	const char* current_iid;
	if (view_frame_is_sidebar_panel (view)) {
                report_sidebar_panel_failure_to_user (window, view);
		current_iid = nautilus_view_frame_get_iid (view);
		nautilus_sidebar_hide_active_panel_if_matches (window->sidebar, current_iid);
		disconnect_and_destroy_sidebar_panel (window, view);
	} else {
	        if (view == window->content_view) {
	                if (GTK_WIDGET (window->content_view)->parent) {
	                        gtk_container_remove (GTK_CONTAINER (GTK_WIDGET (window->content_view)->parent),
	                                              GTK_WIDGET (window->content_view));
	                }
	                report_current_content_view_failure_to_user (window, view);
	                window->content_view = NULL;
	                window->cv_progress_error = TRUE;
	        } else {
	                window->reset_to_idle = TRUE;
	                window->cv_progress_error = TRUE;
	                report_nascent_content_view_failure_to_user (window, view);
	        }
	}
}

static void
free_location_change (NautilusWindow *window)
{
        if (window->cancel_tag != NULL) {
                nautilus_navigation_info_cancel (window->cancel_tag);
                window->cancel_tag = NULL;
        }
        
        if (window->pending_ni != NULL) {
                nautilus_navigation_info_free (window->pending_ni);
                window->pending_ni = NULL;
        }
        
        if (window->new_content_view != NULL) {
        	if (window->new_content_view != window->content_view) {
	                disconnect_view_and_destroy (window, window->new_content_view);
        	}
        	gtk_object_unref (GTK_OBJECT (window->new_content_view));
                window->new_content_view = NULL;
        }
        
        nautilus_gtk_object_list_free (window->error_views);
        window->error_views = NULL;
}

static void
cancel_location_change (NautilusWindow *window)
{
        GList *node;

        if (window->pending_ni != NULL) {
                set_displayed_location
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
        
        nautilus_window_allow_stop (window, FALSE);

        free_location_change (window);

        window->views_shown = FALSE;
        window->view_bombed_out = FALSE;
        window->view_activation_complete = FALSE;
        window->cv_progress_initial = FALSE;
        window->cv_progress_done = FALSE;
        window->cv_progress_error = FALSE;
        window->sent_update_view = FALSE;
        window->reset_to_idle = FALSE;
}
        
static void
load_view_for_new_location (NautilusWindow *window)
{
        NautilusViewIdentifier *content_id;

        content_id = nautilus_navigation_info_get_initial_content_id (window->pending_ni);
        window->new_content_view = load_content_view (window, content_id);
        nautilus_view_identifier_free (content_id);
}

static void
set_view_location_and_selection (NautilusWindow *window)
{
        char *location;
        GList *selection, *node;
        
        if (window->pending_ni != NULL) {
                location = nautilus_navigation_info_get_location (window->pending_ni);
                selection = window->pending_selection;
        } else {
                g_assert (window->pending_selection == NULL);
                location = g_strdup (window->location);
                selection = window->selection;
        }
        
        set_displayed_location (window, location);
        
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

        g_free (location);
}

static gboolean
update_state (gpointer data)
{
        NautilusWindow *window;
        GList *p;
        gboolean made_changes;
	
        window = data;

        if (window->making_changes) {
                return FALSE;
        }
        
        made_changes = FALSE;
        window->making_changes++;
        
        /* Now make any needed state changes based on available information */

        if (window->view_bombed_out) {
                window->view_bombed_out = FALSE;

                for (p = window->error_views; p != NULL; p = p->next) {
                        handle_view_failure (window, NAUTILUS_VIEW_FRAME (p->data));
                        gtk_widget_unref (GTK_WIDGET (p->data));
                }

                g_list_free (window->error_views);
                window->error_views = NULL;

                made_changes = TRUE;
        }
        
        if (window->reset_to_idle) {
                window->reset_to_idle = FALSE;
                
                cancel_location_change (window);

                made_changes = TRUE;
        }
        
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
                
                location_has_really_changed (window);
                
                window->views_shown = TRUE;
                made_changes = TRUE;
        }
        
        if (window->cv_progress_error
            || window->cv_progress_done) {
                made_changes = TRUE;
                window->reset_to_idle = TRUE;
        }
        
        window->making_changes--;
        
        return made_changes;
}

typedef enum {
        INITIAL_VIEW_SELECTED,
        LOAD_DONE,
        LOAD_UNDERWAY,
        NEW_CONTENT_VIEW_READY,
        STOP,
        VIEW_FAILED,
} Stimulus;

static void
change_state (NautilusWindow *window,
              Stimulus stimulus,
              NautilusNavigationInfo *info,
              NautilusViewFrame *new_view)
{
        /* Ensure that changes happen in-order */
        while (update_state (window)) { }
        
        switch (stimulus) {
        case INITIAL_VIEW_SELECTED: /* The information needed for a location change to continue has been received */
                window->pending_ni = info;
                window->cancel_tag = NULL;
                break;
                
        case VIEW_FAILED:
                g_warning ("A view failed. The UI will handle this with a dialog but this should be debugged.");
                window->view_bombed_out = TRUE;
                gtk_object_ref (GTK_OBJECT (new_view));
                window->error_views = g_list_prepend (window->error_views, new_view);
                break;
                
        case NEW_CONTENT_VIEW_READY:
                g_return_if_fail (window->new_content_view == NULL);

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
                
        case LOAD_UNDERWAY: /* We have received an "I am loading" indication from the content view */
                nautilus_window_allow_stop (window, TRUE); /* for the case where we are "re-underway" */
                window->cv_progress_initial = TRUE;
                window->cv_progress_done = FALSE;
                break;
                
        case LOAD_DONE: /* The content view is done loading */
                window->cv_progress_initial = TRUE;
                window->cv_progress_done = TRUE;
                break;
                
        case STOP: /* Someone pressed the stop button or something */
                window->reset_to_idle = TRUE;
                break;
                
        default:
                g_assert_not_reached ();
                break;
        }
        
        while (update_state (window)) { }
}

static void
position_and_show_window_callback (NautilusFile *file,
                       		   gpointer callback_data)
{
	NautilusWindow *window;
	char *geometry_string;
        
	window = NAUTILUS_WINDOW (callback_data);

	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW)) {
		geometry_string = nautilus_file_get_metadata 
			(file, NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY, NULL);
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

        /* This object was ref'd when starting the callback. */
        nautilus_file_unref (file);
}                       			     

/* utility routine that returns true if there's one or fewer windows in the window list */
static gboolean
just_one_window (void)
{
	GSList *window_list;
	window_list = nautilus_application_windows ();
	return window_list == NULL || window_list->next == NULL;
}

static void
nautilus_window_end_location_change_callback (NautilusNavigationResult result_code,
                                              NautilusNavigationInfo *navigation_info,
                                              gboolean final,
                                              gpointer data)
{
        NautilusWindow *window;
        NautilusFile *file;
        char *location;
        char *full_uri_for_display;
        char *uri_for_display;
        char *error_message;
        char *scheme_string;
        char *type_string;
        char *dialog_title;
 	GnomeDialog *dialog;
        GList *attributes;
       
        if (!final) {
                return;
        }

        g_assert (navigation_info != NULL);
        
        window = NAUTILUS_WINDOW (data);
        window->location_change_end_reached = TRUE;

        if (result_code == NAUTILUS_NAVIGATION_RESULT_OK) {
                /* Navigation successful. */
                window->cancel_tag = navigation_info;

		/* If the window is not yet showing (as is the case for nascent
		 * windows), position and show it only after we've got the
		 * metadata (since position info is stored there).
		 */
                if (!GTK_WIDGET_VISIBLE (window)) {
                        location = nautilus_navigation_info_get_location (navigation_info);
	                file = nautilus_file_get (location);

                        attributes = g_list_append (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
			nautilus_file_call_when_ready (file,
                                                       attributes,
                                                       position_and_show_window_callback,
                                                       window);
                        g_list_free (attributes);

                        g_free (location);
                }
                
                change_state (window, INITIAL_VIEW_SELECTED, navigation_info, NULL);
                return;
        }
        
        /* Some sort of failure occurred. How 'bout we tell the user? */
        location = nautilus_navigation_info_get_location (navigation_info);
        full_uri_for_display = nautilus_format_uri_for_display (location);
	/* Truncate the URI so it doesn't get insanely wide. Note that even
	 * though the dialog uses wrapped text, if the URI doesn't contain
	 * white space then the text-wrapping code is too stupid to wrap it.
	 */
        uri_for_display = nautilus_str_middle_truncate
                (full_uri_for_display, MAX_URI_IN_DIALOG_LENGTH);
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
        	file = nautilus_file_get (location);
                type_string = nautilus_file_get_string_attribute (file, "type");
                nautilus_file_unref (file);
                if (type_string == NULL) {
	                error_message = g_strdup_printf
                                (_("Couldn't display \"%s\", because Nautilus cannot determine what type of file it is."),
                                 uri_for_display);
        	} else {
        		/* FIXME bugzilla.eazel.com 4932:
        		 * Should distinguish URIs with no handlers at all from remote URIs
        		 * with local-only handlers.
        		 */
	                error_message = g_strdup_printf
                                (_("Nautilus has no installed viewer capable of displaying \"%s\"."),
                                 uri_for_display);
			g_free (type_string);
        	}
                break;

        case NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME:
                /* Can't create a vfs_uri and get the method from that, because 
                 * gnome_vfs_uri_new might return NULL.
                 */
                scheme_string = nautilus_str_get_prefix (location, ":");
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

	case NAUTILUS_NAVIGATION_RESULT_HOST_NOT_FOUND:
                error_message = g_strdup_printf (_("Couldn't display \"%s\", because the host couldn't be found. "
                				   "Check that your proxy settings are correct."),
                                                 uri_for_display);
		break;

	case NAUTILUS_NAVIGATION_RESULT_HOST_HAS_NO_ADDRESS:
                error_message = g_strdup_printf (_("Couldn't display \"%s\", because the host name was empty. "
                				   "Check that your proxy settings are correct."),
                                                 uri_for_display);
		break;

	case NAUTILUS_NAVIGATION_RESULT_SERVICE_NOT_AVAILABLE:
		if (nautilus_is_search_uri (location)) {
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
        
        if (navigation_info != NULL) {
                if (window->cancel_tag != NULL) {
                        g_assert (window->cancel_tag == navigation_info);
                        window->cancel_tag = NULL;
                }
                nautilus_navigation_info_free (navigation_info);
        }
        
        if (dialog_title == NULL) {
		dialog_title = g_strdup (_("Can't Display Location"));
        }

        if (!GTK_WIDGET_VISIBLE (GTK_WIDGET (window))) {
                /* Destroy never-had-a-chance-to-be-seen window. This case
                 * happens when a new window cannot display its initial URI. 
                 */

                dialog = nautilus_error_dialog (error_message, dialog_title, NULL);
                
		/* if this is the only window, we don't want to quit, so we redirect it to home */
		if (just_one_window ()) {
			char *home_uri;
			
			/* the user could have typed in a home directory that doesn't exist,
			   in which case going home would cause an infinite loop, so we
			   better test for that */
			
			home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
			if (!nautilus_uris_match (home_uri, location)) {	
				nautilus_window_go_home (NAUTILUS_WINDOW (window));
			} else {
				/* the last fallback is to go to a known place that can't be deleted! */
				nautilus_window_goto_uri (NAUTILUS_WINDOW (window), "file:///");
			}
			g_free (home_uri);
		} else {
                /* Since this is a window, destroying it will also unref it. */
                gtk_object_destroy (GTK_OBJECT (window));
       		}
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
        g_free (location);
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
        NautilusNavigationInfo *navigation_info;

        g_assert (NAUTILUS_IS_WINDOW (window));
        g_assert (location != NULL);
        g_assert (type == NAUTILUS_LOCATION_CHANGE_BACK
                  || type == NAUTILUS_LOCATION_CHANGE_FORWARD
                  || distance == 0);

        change_state (window, STOP, NULL, NULL);
        
        window->location_change_type = type;
        window->location_change_distance = distance;
        
        nautilus_window_allow_stop (window, TRUE);

        if (type == NAUTILUS_LOCATION_CHANGE_RELOAD && window->details->viewed_file != NULL) {
                /* If we are reloading, invalidate all we know about the
                 * file so we learn about new mime types, contents, etc. 
                 */
                nautilus_file_invalidate_all_attributes (window->details->viewed_file);
        }        

        /* If we just set the cancel tag in the obvious way here we run into
         * a problem where the cancel tag is set to point to bogus data, because
         * the navigation info is freed by the callback before _new returns.
         * To reproduce this problem, just use any illegal URI.  */
        g_assert (window->cancel_tag == NULL);
        window->location_change_end_reached = FALSE;
        navigation_info = nautilus_navigation_info_new
                (location,
                 nautilus_window_end_location_change_callback,
                 window);
        if (!window->location_change_end_reached) {
                window->cancel_tag = navigation_info;
        }
}

static void
stop_loading (NautilusViewFrame *view)
{
        if (view != NULL) {
                nautilus_view_frame_stop_loading (view);
        }
}

static void
stop_loading_cover (gpointer data, gpointer callback_data)
{
        g_assert (callback_data == NULL);
        stop_loading (NAUTILUS_VIEW_FRAME (data));
}

void
nautilus_window_stop_loading (NautilusWindow *window)
{
        stop_loading (window->content_view);
        stop_loading (window->new_content_view);
        g_list_foreach (window->sidebar_panels, stop_loading_cover, NULL);
        change_state (window, STOP, NULL, NULL);
}

void
nautilus_window_set_content_view (NautilusWindow *window, NautilusViewIdentifier *id)
{
	NautilusFile *file;
        NautilusViewFrame *view;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (window->location != NULL);
	g_return_if_fail (id != NULL);

	file = nautilus_file_get (window->location);
        nautilus_mime_set_default_component_for_file
		(file, id->iid);
        nautilus_file_unref (file);
        
        nautilus_window_allow_stop (window, TRUE);
        
        view = load_content_view (window, id);
        change_state (window, NEW_CONTENT_VIEW_READY, NULL, view);
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
	const char *current_iid;
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
			current_iid = nautilus_view_frame_get_iid (sidebar_panel);
			nautilus_sidebar_hide_active_panel_if_matches (window->sidebar, current_iid);
			disconnect_and_destroy_sidebar_panel (window, sidebar_panel);
		} else {
                        identifier = (NautilusViewIdentifier *) found_node->data;

                        /* Right panel, make sure it has the right name. */
                        /* FIXME: Is this set_label necessary? Shouldn't it already
                         * have the right label here?
                         */
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
		sidebar_panel = nautilus_view_frame_new (window->details->ui_container,
                                                         window->application->undo_manager);
                gtk_object_ref (GTK_OBJECT (sidebar_panel));
                gtk_object_sink (GTK_OBJECT (sidebar_panel));
		nautilus_view_frame_set_label (sidebar_panel, identifier->name);
		set_view_frame_info (sidebar_panel, TRUE, identifier->name);
		connect_view (window, sidebar_panel);
		load_succeeded = nautilus_view_frame_load_client (sidebar_panel, identifier->iid);
		
		/* If the load failed, tell the user. */
		if (!load_succeeded) {
                        report_sidebar_panel_failure_to_user (window, sidebar_panel);
                        disconnect_destroy_unref_view (window, sidebar_panel);
			continue;
		}

                /* tell panel about the location and selection, if we have them */
                if (window->location != NULL) {
                        update_view (sidebar_panel, window->location, window->selection);
                }

		/* If the load succeeded, add the panel. */
		nautilus_window_add_sidebar_panel (window, sidebar_panel);
		gtk_object_unref (GTK_OBJECT (sidebar_panel));
	}

	g_list_free (identifier_list);
}

static void
zoom_level_changed_callback (NautilusViewFrame *view,
                             NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        /* This is called each time the component successfully completed
         * a zooming operation.
         */

	nautilus_zoom_control_set_zoom_level (NAUTILUS_ZOOM_CONTROL (window->zoom_control),
                                              nautilus_view_frame_get_zoom_level (view));

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_IN,
				       nautilus_zoom_control_can_zoom_in (NAUTILUS_ZOOM_CONTROL (window->zoom_control)));
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_OUT,
				       nautilus_zoom_control_can_zoom_out (NAUTILUS_ZOOM_CONTROL (window->zoom_control)));
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_NORMAL,
				       TRUE);
	/* FIXME bugzilla.eazel.com 3442: Desensitize "Zoom Normal"? */
}

static void
zoom_parameters_changed_callback (NautilusViewFrame *view,
                                  NautilusWindow *window)
{
        double zoom_level;

        g_assert (NAUTILUS_IS_WINDOW (window));

        /* This callback is invoked via the "zoom_parameters_changed"
         * signal of the BonoboZoomableFrame.
         * 
         * You can rely upon this callback being called in the following
         * situations:
         *
         * - a zoomable component has been set in the NautilusViewFrame;
         *   in this case nautilus_view_frame_set_to_component() emits the
         *   "zoom_parameters_changed" signal after creating the
         *   BonoboZoomableFrame and binding it to the Bonobo::Zoomable.
         *
         *   This means that we can use the following call to
         *   nautilus_zoom_control_set_parameters() to display the zoom
         *   control when a new zoomable component has been loaded.
         *
         * - a new file has been loaded by the zoomable component; this is
         *   not 100% guaranteed since it's up to the component to emit this
         *   signal, but I consider it "good behaviour" of a component to
         *   emit this signal after loading a new file.
         */

        nautilus_zoom_control_set_parameters
                (NAUTILUS_ZOOM_CONTROL (window->zoom_control),
                 nautilus_view_frame_get_min_zoom_level (view),
                 nautilus_view_frame_get_max_zoom_level (view),
                 nautilus_view_frame_get_has_min_zoom_level (view),
                 nautilus_view_frame_get_has_max_zoom_level (view),
                 nautilus_view_frame_get_preferred_zoom_levels (view));

        /* The initial zoom level of a component is allowed to be 0.0 if
         * there is no file loaded yet. In this case we need to set the
         * commands insensitive but display the zoom control nevertheless
         * (the component is just temporarily unable to zoom, but the
         *  zoom control will "do the right thing" here).
         */
        zoom_level = nautilus_view_frame_get_zoom_level (view);
        if (zoom_level == 0.0) {
                nautilus_bonobo_set_sensitive (window->details->shell_ui,
                                               NAUTILUS_COMMAND_ZOOM_IN,
                                               FALSE);
                nautilus_bonobo_set_sensitive (window->details->shell_ui,
                                               NAUTILUS_COMMAND_ZOOM_OUT,
                                               FALSE);
                nautilus_bonobo_set_sensitive (window->details->shell_ui,
                                               NAUTILUS_COMMAND_ZOOM_NORMAL,
                                               FALSE);

                /* Don't attempt to set 0.0 as zoom level. */
                return;
        }

        /* "zoom_parameters_changed" always implies "zoom_level_changed",
         * but you won't get both signals, so we need to pass it down.
         */
        zoom_level_changed_callback (view, window);
}


static Nautilus_History *
get_history_list_callback (NautilusViewFrame *view,
                           NautilusWindow *window)
{
	Nautilus_History *list;
	NautilusBookmark *bookmark;
	int length, i;
	GList *node;
	char *name, *location, *pixbuf_xml;
	GdkPixbuf *pixbuf;
	
	/* Get total number of history items */
	length = g_list_length (nautilus_get_history_list ());

	list = Nautilus_History__alloc ();

	list->_length = length;
	list->_maximum = length;
	list->_buffer = CORBA_sequence_Nautilus_HistoryItem_allocbuf (length);
	CORBA_sequence_set_release (list, CORBA_TRUE);
	
	/* Iterate through list and copy item data */
	for (i = 0, node = nautilus_get_history_list (); i < length; i++, node = node->next) {
		bookmark = node->data;

		name = nautilus_bookmark_get_name (bookmark);
		location = nautilus_bookmark_get_uri (bookmark);
		pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS);
		pixbuf_xml = bonobo_ui_util_pixbuf_to_xml (pixbuf);
		
		list->_buffer[i].title = CORBA_string_dup (name);
		list->_buffer[i].location = CORBA_string_dup (location);
		list->_buffer[i].icon = CORBA_string_dup (pixbuf_xml);

		g_free (name);
		g_free (location);
		g_free (pixbuf_xml);
		gdk_pixbuf_unref (pixbuf);		
	}

	return list;
}

static void
change_selection_callback (NautilusViewFrame *view,
                           GList *selection,
                           NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        change_selection (window, selection);
}

static void
change_status_callback (NautilusViewFrame *view,
                        const char *status,
                        NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        nautilus_window_set_status (window, status);
}

static void
failed_callback (NautilusViewFrame *view,
                 NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        change_state (window, VIEW_FAILED, NULL, view);
}

static void
load_underway_callback (NautilusViewFrame *view,
                        NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        /* FIXME bugzilla.eazel.com 2460: We intentionally ignore
         * progress from sidebar panels. Some sidebar panels may get
         * their own progress indicators later.
         */

        /* FIXME bugzilla.eazel.com 2461: Is progress from either old
         * or new really equally interesting?
         */

        if (view == window->new_content_view
            || view == window->content_view) {
                change_state (window, LOAD_UNDERWAY, NULL, NULL);
        }
}

static void
load_complete_callback (NautilusViewFrame *view,
                        NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        /* We intentionally ignore progress from sidebar panels. Some
           sidebar panels may get their own progress indicators
           later. */

        /* FIXME bugzilla.eazel.com 2461: Is progress from either old or new really equally interesting? */
        if (view == window->new_content_view
            || view == window->content_view) {
                change_state (window, LOAD_DONE, NULL, NULL);
        }
}

static void
open_location_in_this_window_callback (NautilusViewFrame *view,
                                       const char *location,
                                       NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        nautilus_window_open_location (window, location);
}

static void
open_location_prefer_existing_window_callback (NautilusViewFrame *view,
                                               const char *location,
                                               NautilusWindow *window)
{
        NautilusWindow *existing_window;
	GSList *node;

        g_assert (NAUTILUS_IS_WINDOW (window));

        /* First, handle the case where there's already a window for
         * this location.
         */
        for (node = nautilus_application_windows ();
             node != NULL; node = node->next) {
                existing_window = NAUTILUS_WINDOW (node->data);
                if (nautilus_uris_match (existing_window->location, location)) {
                        nautilus_gtk_window_present (GTK_WINDOW (existing_window));
                        return;
                }
        }

        /* Otherwise, open a new window. */
        open_location (window, location, TRUE, NULL);
}

static void
open_location_force_new_window_callback (NautilusViewFrame *view,
                                         const char *location,
                                         GList *selection,
                                         NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        open_location (window, location, TRUE, selection);
}

static void
title_changed_callback (NautilusViewFrame *view,
                        NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

        update_title (window);
}

#define FOR_EACH_NAUTILUS_WINDOW_SIGNAL(macro) \
	macro (change_selection)			\
	macro (change_status)				\
	macro (failed)					\
	macro (get_history_list)			\
	macro (load_complete)				\
	macro (load_underway)				\
	macro (open_location_force_new_window)		\
	macro (open_location_in_this_window)		\
	macro (open_location_prefer_existing_window)	\
	macro (title_changed)				\
	macro (zoom_level_changed)			\
        macro (zoom_parameters_changed)

static void
connect_view (NautilusWindow *window, NautilusViewFrame *view)
{
	GtkObject *view_object;
	
	view_object = GTK_OBJECT (view);

	#define CONNECT(signal) gtk_signal_connect \
        	(view_object, #signal, \
                 GTK_SIGNAL_FUNC (signal##_callback), window);
        FOR_EACH_NAUTILUS_WINDOW_SIGNAL (CONNECT)
	#undef CONNECT
}

static void
disconnect_view (NautilusWindow *window, NautilusViewFrame *view)
{
	GtkObject *view_object;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (view == NULL) {
		return;
	}

	g_assert (NAUTILUS_IS_VIEW_FRAME (view));

	view_object = GTK_OBJECT (view);

	#define DISCONNECT(signal) gtk_signal_disconnect_by_func \
        	(view_object, \
        	 GTK_SIGNAL_FUNC (signal##_callback), window);
        FOR_EACH_NAUTILUS_WINDOW_SIGNAL (DISCONNECT)
	#undef DISCONNECT
}

static void
disconnect_and_destroy_sidebar_panel (NautilusWindow *window, NautilusViewFrame *view)
{
        gtk_widget_ref (GTK_WIDGET (view));
	disconnect_view (window, view);
        nautilus_window_remove_sidebar_panel (window, view);
	gtk_widget_destroy (GTK_WIDGET (view));
        gtk_widget_unref (GTK_WIDGET (view));
}

static void
disconnect_view_and_destroy (NautilusWindow *window, NautilusViewFrame *view)
{
	disconnect_view (window, view);
	gtk_widget_destroy (GTK_WIDGET (view));
}

static void
disconnect_view_callback (gpointer list_item_data, gpointer callback_data)
{
	disconnect_view (NAUTILUS_WINDOW (callback_data),
                         NAUTILUS_VIEW_FRAME (list_item_data));
}


void
nautilus_window_manage_views_destroy (NautilusWindow *window)
{
        free_location_change (window);

	/* Disconnect view signals here so they don't trigger when
	 * views are destroyed.
	 */
	g_list_foreach (window->sidebar_panels, disconnect_view_callback, window);
        disconnect_view (window, window->content_view);

        /* Cancel callbacks. */
        cancel_viewed_file_changed_callback (window);
}
