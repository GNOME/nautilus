/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
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
#include "ntl-window-msgs.h"

#include <stdarg.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libnautilus/nautilus-string.h>
#include <libnautilus/nautilus-gtk-extensions.h>
#include <libnautilus/nautilus-metadata.h>
#include "ntl-app.h"
#include "ntl-meta-view.h"
#include "ntl-uri-map.h"
#include "ntl-window-private.h"
#include "ntl-window-state.h"
#include "nautilus-location-bar.h"

/* #define EXTREME_DEBUGGING */

#ifdef EXTREME_DEBUGGING
#define x_message(parameters) g_message parameters
#else
#define x_message(parameters)
#endif

static void nautilus_window_notify_selection_change (NautilusWindow         *window,
                                                     NautilusView           *view,
                                                     Nautilus_SelectionInfo *loc,
                                                     NautilusView           *requesting_view);
static void nautilus_window_load_content_view_menu  (NautilusWindow         *window,
                                                     NautilusNavigationInfo *ni);

typedef enum { PROGRESS_INITIAL, PROGRESS_VIEWS, PROGRESS_DONE, PROGRESS_ERROR } ProgressType;

/* Indicate progress to user interface */
static void
nautilus_window_progress_indicate (NautilusWindow *window, ProgressType type, double percent,
                                   const char *msg)
{
        char *the_uri;
                
        if (type == PROGRESS_ERROR) {
                gtk_widget_show (gnome_error_dialog_parented (msg, GTK_WINDOW (window)));
                
                /* If it was an error loading a URI that had been dragged to the location bar, we might
                   need to reset the URI */
                the_uri = window->ni == NULL ? "" : window->ni->requested_uri;
                nautilus_location_bar_set_location (NAUTILUS_LOCATION_BAR (window->ent_uri),
                                                    the_uri);
        }
}

/* Stays alive */
static void
Nautilus_SelectionInfo__copy(Nautilus_SelectionInfo *dest_si, Nautilus_SelectionInfo *src_si)
{
        int i, n;
        
        dest_si->selected_uris = src_si->selected_uris;
        
        n = dest_si->selected_uris._length;
        dest_si->selected_uris._buffer = CORBA_sequence_CORBA_string_allocbuf (n);
        for(i = 0; i < n; i++) {
                dest_si->selected_uris._buffer[i] = CORBA_string_dup (src_si->selected_uris._buffer[i]);
        }
        
        dest_si->content_view = CORBA_OBJECT_NIL;
        dest_si->self_originated = CORBA_FALSE;
}

static void
nautilus_window_notify_selection_change(NautilusWindow *window,
					NautilusView *view,
					Nautilus_SelectionInfo *loc,
					NautilusView *requesting_view)
{
        loc->self_originated = (view == requesting_view);
        nautilus_view_notify_selection_change(view, loc);
}

void
nautilus_window_request_selection_change(NautilusWindow *window,
					 Nautilus_SelectionRequestInfo *loc,
					 NautilusView *requesting_view)
{
        GSList *p;
        Nautilus_SelectionInfo selinfo;
        CORBA_Environment environment;
        
        CORBA_exception_init (&environment);
        selinfo.selected_uris = loc->selected_uris;
        selinfo.content_view = CORBA_Object_duplicate
                (nautilus_view_get_objref (NAUTILUS_VIEW (window->content_view)),
                 &environment);
        CORBA_exception_free (&environment);
        
        CORBA_free (window->si);
        
        window->si = Nautilus_SelectionInfo__alloc();
        Nautilus_SelectionInfo__copy(window->si, &selinfo);
        
        nautilus_window_notify_selection_change(window, window->content_view, &selinfo, requesting_view);
        
        for (p = window->meta_views; p != NULL; p = p->next) {
                nautilus_window_notify_selection_change(window, p->data, &selinfo, requesting_view);
        }
}

void
nautilus_window_request_status_change(NautilusWindow *window,
                                      Nautilus_StatusRequestInfo *loc,
                                      NautilusView *requesting_view)
{
        nautilus_window_set_status(window, loc->status_string);
}

/*
 * Since this is can be called while the window is still getting set up, it
 * doesn't do any work directly.
 */
void
nautilus_window_request_progress_change(NautilusWindow *window,
					Nautilus_ProgressRequestInfo *loc,
					NautilusView *requesting_view)
{
        NautilusWindowStateItem item;
        
        if (requesting_view != window->new_content_view
            && requesting_view != window->content_view) {
                return; /* Only pay attention to progress information from the upcoming content view, for now */
        }
  
        /* If the progress indicates we are done, record that, otherwise
           just record the fact that we have at least begun loading.
        */
        switch(loc->type) {
        case Nautilus_PROGRESS_DONE_OK:
                item = CV_PROGRESS_DONE;
                break;
        case Nautilus_PROGRESS_DONE_ERROR:
                item = CV_PROGRESS_ERROR;
                break;
        case Nautilus_PROGRESS_UNDERWAY:
                item = CV_PROGRESS_INITIAL;
                break;
        default:
                g_assert_not_reached();
                item = -1;
                break;
        }
        
        nautilus_window_set_state_info(window, item, (NautilusWindowStateItem) 0);
}

static char *
compute_default_title (const char *text_uri)
{
        GnomeVFSURI *vfs_uri;
        char *short_name;
	
        if (text_uri != NULL) {
                vfs_uri = gnome_vfs_uri_new (text_uri);
                if (vfs_uri != NULL) {
                        short_name = gnome_vfs_uri_extract_short_name (vfs_uri);
                        gnome_vfs_uri_unref (vfs_uri);
                        
                        g_assert (short_name != NULL);
                        return short_name;
                }
        }

        return g_strdup(_("Nautilus"));
}

/*
 * nautilus_window_get_current_location_title:
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
        return window->requested_title != NULL
                ? g_strdup (window->requested_title)
                : g_strdup (window->default_title);
}

/*
 * nautilus_window_update_title_internal:
 * 
 * Update the non-NautilusView objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 * 
 */
static void
nautilus_window_update_title_internal (NautilusWindow *window, const char *title)
{
        char *window_title;
        
        if (strcmp (title, _("Nautilus")) == 0) {
                gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));
        } else {
                window_title = g_strdup_printf (_("Nautilus: %s"), title);
                gtk_window_set_title (GTK_WINDOW (window), window_title);
                g_free (window_title);
        }
        
        nautilus_index_panel_set_title (window->index_panel, title);
        nautilus_bookmark_set_name (window->current_location_bookmark, title);

        /* Name of item in history list may have changed, tell listeners. */
        nautilus_send_history_list_changed ();
}

/*
 * nautilus_window_reset_title_internal:
 * 
 * Update the non-NautilusView objects that use the location's user-displayable
 * title in some way. Called when the location or title has changed.
 * @window: The NautilusWindow in question.
 * @title: The new user-displayable title.
 * 
 */
static void
nautilus_window_reset_title_internal (NautilusWindow *window, const char *uri)
{
        g_free (window->requested_title);
        window->requested_title = NULL;
        g_free (window->default_title);
        window->default_title = compute_default_title (uri);
        
        if (window->current_location_bookmark == NULL || 
            strcmp (uri, nautilus_bookmark_get_uri (window->current_location_bookmark)) != 0) {
                /* We've changed locations, must recreate bookmark for current location. */
                if (window->last_location_bookmark != NULL)  {
                        gtk_object_unref (GTK_OBJECT (window->last_location_bookmark));
                }
                window->last_location_bookmark = window->current_location_bookmark;
                window->current_location_bookmark = nautilus_bookmark_new (uri);
        }
        
        nautilus_window_update_title_internal (window, window->default_title);
}

void
nautilus_window_request_title_change(NautilusWindow *window,
                                     const char *new_title,
                                     NautilusContentView *requesting_view)
{
        g_return_if_fail (new_title != NULL);
        
        g_free (window->requested_title);
        window->requested_title = g_strdup (new_title);
        
        nautilus_window_update_title_internal (window, new_title);
}

/* The bulk of this file - location changing */

static void
Nautilus_NavigationInfo__copy (Nautilus_NavigationInfo *dest_ni, Nautilus_NavigationInfo *src_ni)
{
        dest_ni->requested_uri = CORBA_string_dup(src_ni->requested_uri);
        dest_ni->actual_uri = CORBA_string_dup(src_ni->actual_uri);
        dest_ni->content_type = CORBA_string_dup(src_ni->content_type);
        dest_ni->referring_uri = CORBA_string_dup(src_ni->referring_uri);
        dest_ni->actual_referring_uri = CORBA_string_dup(src_ni->actual_referring_uri);
        dest_ni->referring_content_type = CORBA_string_dup(src_ni->referring_content_type);
        dest_ni->content_view = CORBA_OBJECT_NIL;
        dest_ni->self_originated = CORBA_FALSE;
}

/* Handle the changes for the NautilusWindow itself. */
static void
nautilus_window_update_internals (NautilusWindow *window, NautilusNavigationInfo *navi)
{
        GnomeVFSURI *new_uri;
        char *current_title;
        
        if(navi) {
                Nautilus_NavigationInfo *newni;
                
                /* Maintain history lists. */
                if (window->location_change_type != NAUTILUS_LOCATION_CHANGE_RELOAD) {        
                        /* Always add new location to history list. */
                        nautilus_add_to_history_list (window->current_location_bookmark);
                        
                        if (window->location_change_type == NAUTILUS_LOCATION_CHANGE_BACK) {
                                guint index;
                                
                                /* Going back. Move items from the back list to the forward list. */
                                g_assert (g_slist_length (window->back_list) > window->location_change_distance);
                                g_assert (!strcmp(nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (g_slist_nth_data (window->back_list, window->location_change_distance))), navi->navinfo.requested_uri));
                                g_assert (window->ni);
                                
                                /* Move current location to Forward list */
                                g_assert (strcmp (nautilus_bookmark_get_uri (window->last_location_bookmark), window->ni->requested_uri) == 0);
                                /* Use the first bookmark in the history list rather than creating a new one. */
                                window->forward_list = g_slist_prepend (window->forward_list, window->last_location_bookmark);
                                gtk_object_ref (GTK_OBJECT (window->forward_list->data));
                                
                                /* Move extra links from Back to Forward list */
                                for (index = 0; index < window->location_change_distance; ++index) {
                                        NautilusBookmark *bookmark;
                                        
                                        bookmark = window->back_list->data;
                                        window->back_list = g_slist_remove_link (window->back_list, window->back_list);
                                        window->forward_list = g_slist_prepend (window->forward_list, bookmark);
                                }
                                
                                /* One bookmark falls out of back/forward lists and becomes viewed location */
                                gtk_object_unref (window->back_list->data);
                                window->back_list = g_slist_remove_link (window->back_list, window->back_list);

                        } else if (window->location_change_type == NAUTILUS_LOCATION_CHANGE_FORWARD) {
                                guint index;
                                
                                /* Going back. Move items from the forward list to the back list. */
                                g_assert(g_slist_length (window->forward_list) > window->location_change_distance);
                                g_assert(!strcmp(nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (g_slist_nth_data (window->forward_list, window->location_change_distance))), navi->navinfo.requested_uri));
                                g_assert(window->ni);
                                
                                /* Move current location to Back list */
                                g_assert (strcmp (nautilus_bookmark_get_uri (window->last_location_bookmark), window->ni->requested_uri) == 0);
                                /* Use the first bookmark in the history list rather than creating a new one. */
                                window->back_list = g_slist_prepend (window->back_list, window->last_location_bookmark);
                                gtk_object_ref (GTK_OBJECT (window->back_list->data));
                                
                                /* Move extra links from Forward to Back list */
                                for (index = 0; index < window->location_change_distance; ++index) {
                                        NautilusBookmark *bookmark;
                                        
                                        bookmark = window->forward_list->data;
                                        window->forward_list = g_slist_remove_link (window->forward_list, window->forward_list);
                                        window->back_list = g_slist_prepend (window->back_list, bookmark);
                                }
                                
                                /* One bookmark falls out of back/forward lists and becomes viewed location */
                                gtk_object_unref (window->forward_list->data);
                                window->forward_list = g_slist_remove_link (window->forward_list, window->forward_list);
                        } else {
                                g_assert (window->location_change_type == NAUTILUS_LOCATION_CHANGE_STANDARD);
                                /* Clobber the entire forward list, and move displayed location to back list */
                                if (window->forward_list) {
                                        g_slist_foreach(window->forward_list, (GFunc)gtk_object_unref, NULL);
                                        g_slist_free(window->forward_list); window->forward_list = NULL;
                                }
                                
                                if (window->ni) {
                                	/*
                                	 * If we're returning to the same uri somehow, don't put this uri on back list. 
                                	 * This also avoids a problem where nautilus_window_reset_title_internal
                                	 * didn't update last_location_bookmark since the uri didn't change.
                                	 */
                                	if (strcmp (window->ni->requested_uri, navi->navinfo.requested_uri) != 0) {
	                                        /* Store bookmark for current location in back list, unless there is no current location */
	                                        g_assert (strcmp (nautilus_bookmark_get_uri (window->last_location_bookmark), 
	                                        		  window->ni->requested_uri) == 0);
	                                        /* Use the first bookmark in the history list rather than creating a new one. */
	                                        window->back_list = g_slist_prepend (window->back_list, window->last_location_bookmark);
	                                        gtk_object_ref (GTK_OBJECT (window->back_list->data));
                                	}
                                }
                        }
                }
                
                new_uri = gnome_vfs_uri_new (navi->navinfo.requested_uri);
                if(!new_uri && navi->navinfo.actual_uri)
                        new_uri = gnome_vfs_uri_new (navi->navinfo.actual_uri);
                if (new_uri) {
                        nautilus_window_allow_up(window, 
                                                 gnome_vfs_uri_has_parent(new_uri));
                        gnome_vfs_uri_unref(new_uri);
                } else {
                        nautilus_window_allow_up(window, FALSE);
                }
                
                newni = Nautilus_NavigationInfo__alloc();
                Nautilus_NavigationInfo__copy(newni, &navi->navinfo);
                
                CORBA_free(window->ni);
                
                window->ni = newni;
                
                CORBA_free(window->si);
                window->si = NULL;
                
                nautilus_window_load_content_view_menu (window, navi);
        }
        
        nautilus_window_allow_back(window, window->back_list != NULL);
        nautilus_window_allow_forward(window, window->forward_list != NULL);
        
        nautilus_location_bar_set_location(NAUTILUS_LOCATION_BAR(window->ent_uri),
                                           window->ni->requested_uri);
        
        /*
         * Notify the index panel of the location change. FIXME: Eventually,
         * this will not be necessary when we restructure the index panel to
         * be a NautilusView.
         */
        current_title = nautilus_window_get_current_location_title (window);
        nautilus_index_panel_set_uri (window->index_panel, window->ni->requested_uri, current_title);
        g_free (current_title);
}

static void
nautilus_window_update_view (NautilusWindow *window,
                             NautilusView *view,
                             Nautilus_NavigationInfo *navi,
                             Nautilus_SelectionInfo *seli,
                             NautilusView *requesting_view,
                             NautilusView *content_view)
{
        CORBA_Environment environment;
        
        g_return_if_fail(view);
        
        navi->self_originated = (view == requesting_view);
        
        nautilus_view_notify_location_change (view, navi);
        
        if(seli) {
                CORBA_exception_init(&environment);
                CORBA_Object_release(seli->content_view, &environment);
                seli->content_view = CORBA_Object_duplicate
                        (nautilus_view_get_client_objref(content_view),
                         &environment);
                CORBA_exception_free(&environment);
                
                nautilus_window_notify_selection_change(window, view, seli, NULL);
        }
}

void
nautilus_window_view_destroyed(NautilusView *view, NautilusWindow *window)
{
        NautilusWindowStateItem item = VIEW_ERROR;
        nautilus_window_set_state_info(window, item, view, (NautilusWindowStateItem) 0);
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
nautilus_window_has_really_changed(NautilusWindow *window)
{
        GSList *p, *discard_views;
        GSList *new_meta_views;
        
        new_meta_views = window->new_meta_views;
        window->new_meta_views = NULL;
        
        if (window->new_content_view) {
                if (!GTK_WIDGET (window->new_content_view)->parent) {
                        if(window->content_view)
                                gtk_signal_disconnect_by_func(GTK_OBJECT(window->content_view), nautilus_window_view_destroyed, window);
                        nautilus_window_set_content_view(window, window->new_content_view);
                }
                gtk_object_unref(GTK_OBJECT(window->new_content_view));
                window->new_content_view = NULL;
        }
        
        if (new_meta_views) {
                /* Do lots of shuffling to make sure we don't remove views that were already there, but add new views */
                for (p = new_meta_views; p != NULL; p = p->next) {
                        if (!GTK_OBJECT_DESTROYED (p->data) && !GTK_WIDGET (p->data)->parent)
                                nautilus_window_add_meta_view (window, p->data);
                        gtk_object_unref (p->data);
                }
                
                discard_views = NULL;
                for (p = window->meta_views; p != NULL; p = p->next) {
                        if (!g_slist_find(new_meta_views, p->data)) {
                                discard_views = g_slist_prepend(discard_views, p->data);
                        }
                }
                g_slist_free (new_meta_views);
                
                for (p = discard_views; p != NULL; p = p->next) {
                        nautilus_window_remove_meta_view(window, p->data);
                }
                g_slist_free (discard_views);
        }
        
        if (window->pending_ni != NULL) {
                nautilus_window_update_internals (window, window->pending_ni);
                nautilus_navinfo_free (window->pending_ni);
                window->pending_ni = NULL;
        }
}

/* This is called when we are done loading to get rid of the load_info structure. */
static void
nautilus_window_free_load_info (NautilusWindow *window)
{
        x_message (("-> FREE_LOAD_INFO <-"));
        
        if (window->pending_ni)
                nautilus_navinfo_free(window->pending_ni);
        window->pending_ni = NULL;
        window->error_views = NULL;
        window->new_meta_views = NULL;
        window->new_content_view = NULL;
        window->cancel_tag = NULL;
        window->action_tag = 0;
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
static NautilusView *
nautilus_window_load_meta_view(NautilusWindow *window,
                               const char *iid,
                               NautilusView *requesting_view)
{
        NautilusView *meta_view;
        GSList *p;
        
        meta_view = NULL;
        for (p = window->meta_views; p != NULL; p = p->next) {
                meta_view = NAUTILUS_VIEW (p->data);
                if (!strcmp (nautilus_view_get_iid (meta_view), iid))
                        break;
        }
        
        if (p == NULL) {
                meta_view = NAUTILUS_VIEW (gtk_widget_new (nautilus_meta_view_get_type(),
                                                           "main_window", window, NULL));
                nautilus_window_connect_view (window, meta_view);
                if (!nautilus_view_load_client (meta_view, iid)) {
                        gtk_widget_unref (GTK_WIDGET (meta_view));
                        meta_view = NULL;
                }
        }
        
        if (meta_view != NULL) {
                gtk_object_ref (GTK_OBJECT (meta_view));
                nautilus_view_set_active_errors (meta_view, TRUE);
        }
        
        return meta_view;
}

void
nautilus_window_request_location_change (NautilusWindow *window,
                                         Nautilus_NavigationRequestInfo *loc,
                                         NautilusView *requesting_view)
{  
        NautilusWindow *new_window;
        
        if (loc->new_window_requested) {
                new_window = nautilus_app_create_window (NAUTILUS_APP(window->app));
                nautilus_window_set_initial_state (new_window, loc->requested_uri);
        } else {
                nautilus_window_begin_location_change (window, loc,
                                                       requesting_view,
                                                       NAUTILUS_LOCATION_CHANGE_STANDARD, 0);
        }
}

static NautilusView *
nautilus_window_load_content_view(NautilusWindow *window,
                                  const char *iid,
                                  Nautilus_NavigationInfo *navinfo,
                                  NautilusView **requesting_view)
{
        NautilusView *content_view = window->content_view;
        NautilusView *new_view;
        CORBA_Environment environment;
        
        g_return_val_if_fail(iid, NULL);
        g_return_val_if_fail(navinfo, NULL);
        
        if (!NAUTILUS_IS_VIEW (content_view)
            || strcmp (nautilus_view_get_iid (content_view), iid) != 0) {

                if (requesting_view != NULL && *requesting_view == window->content_view) {
                        /* If we are going to be zapping the old view,
                         * we definitely don't want any of the new views
                         * thinking they made the request
                        */
                        *requesting_view = NULL;
                }
                
                new_view = NAUTILUS_VIEW (gtk_widget_new (nautilus_content_view_get_type(),
                                                          "main_window", window, NULL));
                
                nautilus_window_connect_content_view (window, NAUTILUS_CONTENT_VIEW (new_view));
                
                if (!nautilus_view_load_client (new_view, iid)) {
                        gtk_widget_unref(GTK_WIDGET(new_view));
                        new_view = NULL;
                }
                
                /* Avoid being fooled by extra done notifications from the last view. 
                   This is a HACK because the state machine SUCKS. */
                window->cv_progress_done = FALSE;
                window->cv_progress_error = FALSE;
        } else {
                new_view = window->content_view;
        }
        
        if (!NAUTILUS_IS_VIEW (new_view)) {
                new_view = NULL;
        } else {
                gtk_object_ref (GTK_OBJECT (new_view));
                
                CORBA_exception_init(&environment);
                CORBA_Object_release(navinfo->content_view, &environment);
                navinfo->content_view = CORBA_Object_duplicate
                        (nautilus_view_get_client_objref (new_view),
                         &environment);
                CORBA_exception_free(&environment);
                
                nautilus_view_set_active_errors (new_view, TRUE);
        }
        
        return new_view;
}

static gboolean
nautilus_window_update_state(gpointer data)
{
        NautilusWindow *window;
        GSList *p;
        gboolean result;

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
                        NautilusView *error_view = p->data;
                        
                        if (NAUTILUS_IS_CONTENT_VIEW(error_view)) {
                                if (error_view == window->new_content_view) {
                                        window->made_changes++;
                                        window->reset_to_idle = TRUE;
                                }
                                
                                if (error_view == window->content_view) {
                                        if (GTK_WIDGET (window->content_view)->parent) {
                                                gtk_container_remove (GTK_CONTAINER (GTK_WIDGET (window->content_view)->parent),
                                                                      GTK_WIDGET (window->content_view));
                                        }
                                        window->content_view = NULL;
                                        window->made_changes++;
                                }
                                window->cv_progress_error = TRUE;
                        }
                        else if (NAUTILUS_IS_META_VIEW(error_view))
                        {
                                if (g_slist_find (window->new_meta_views, error_view) != NULL) {
                                        window->new_meta_views = g_slist_remove (window->new_meta_views, error_view);
                                        gtk_widget_unref (GTK_WIDGET (error_view));
                                }
                                if (g_slist_find (window->meta_views, error_view) != NULL) {
                                        nautilus_window_remove_meta_view (window, error_view);
                                }
                        }
                        gtk_widget_unref (GTK_WIDGET (error_view));
                }
                g_slist_free (window->error_views);
                window->error_views = NULL;
                
                window->view_bombed_out = FALSE;
        }
        
        if (window->reset_to_idle) {
                x_message (("Reset to idle!"));
                
                window->changes_pending = FALSE;
                window->made_changes++;
                window->reset_to_idle = FALSE;
                
                if (window->cancel_tag) {
                        gnome_vfs_async_cancel (window->cancel_tag);
                        if (window->pending_ni != NULL) {
                                window->pending_ni->ah = NULL;
                        }
                        window->cancel_tag = NULL;
                }
                
                if (window->pending_ni) {
                        nautilus_window_reset_title_internal (window, window->ni->requested_uri);
                        
                        /* Tell previously-notified views to go back to the old page */
                        for (p = window->meta_views; p != NULL; p = p->next) {
                                if (g_slist_find (window->new_meta_views, p->data) != NULL) {
                                        nautilus_window_update_view (window, p->data, window->ni, window->si,
                                                                     NULL, window->content_view);
                                }
                        }
                        
                        if (window->new_content_view
                            && window->new_content_view == window->content_view) {
                                nautilus_window_update_view (window, window->content_view,
                                                             window->ni, window->si,
                                                             NULL, window->content_view);
                        }
                }
                
                if (window->new_content_view) {
                        gtk_widget_unref (GTK_WIDGET (window->new_content_view));
                }
                
                for (p = window->new_meta_views; p != NULL; p = p->next) {
                        gtk_widget_unref (GTK_WIDGET (p->data));
                }
                g_slist_free (window->new_meta_views);
                
                nautilus_window_free_load_info (window);
                
                nautilus_window_allow_stop (window, FALSE);
        }
        
        if (window->changes_pending) {
                window->state = NW_LOADING_VIEWS;
                
                x_message (("Changes pending"));
                
                if (window->pending_ni && !window->new_content_view && !window->cv_progress_error
                    && !window->view_activation_complete) {
                        window->new_content_view = nautilus_window_load_content_view
                                (window, window->pending_ni->initial_content_iid,
                                 &window->pending_ni->navinfo,
                                 &window->new_requesting_view);
                        
                        for (p = window->pending_ni->meta_iids; p != NULL; p = p->next) {
                                NautilusView *meta_view;

                                meta_view = nautilus_window_load_meta_view
                                        (window, p->data, window->new_requesting_view);
                                if (meta_view != NULL) {
                                        window->new_meta_views = g_slist_prepend (window->new_meta_views, meta_view);
                                }
                        }
                        
                        window->view_activation_complete = TRUE;
                        window->made_changes++;
                }
                
                if (window->view_activation_complete
                    && !window->sent_update_view) {
                        
                        Nautilus_NavigationInfo *ni;
                        Nautilus_SelectionInfo *si;
                        
                        if (window->pending_ni) {
                                ni = &window->pending_ni->navinfo;
                                si = NULL;
                        } else {
                                ni = window->ni;
                                si = window->si;
                        }
                        
                        nautilus_window_reset_title_internal (window, ni->requested_uri);
                        
                        x_message (("!!! Sending update_view"));
                        
                        if (window->new_content_view) {
                                nautilus_window_update_view (window, window->new_content_view, ni, si,
                                                             window->new_requesting_view, window->new_content_view);
                        } else {
                                window->cv_progress_error = TRUE;
                        }
                        
                        for (p = window->new_meta_views; p != NULL; p = p->next) {
                                nautilus_window_update_view (window, p->data, ni, si,
                                                             window->new_requesting_view,
                                                             window->new_content_view);
                        }
                        
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
        NautilusView *new_view;
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
                        new_view = va_arg (args, NautilusView*);
                        x_message (("VIEW_ERROR on %p", new_view));
                        window->view_bombed_out = TRUE;
                        gtk_object_ref (GTK_OBJECT (new_view));
                        window->error_views = g_slist_prepend (window->error_views, new_view);
                        break;

                case NEW_CONTENT_VIEW_ACTIVATED:
                        x_message (("NEW_CONTENT_VIEW_ACTIVATED"));
                        g_return_if_fail (window->new_content_view == NULL);
                        new_view = va_arg (args, NautilusView*);
                        /* Don't ref here, reference is held by widget hierarchy. */
                        window->new_content_view = new_view;
                        if (window->pending_ni == NULL) {
                                window->view_activation_complete = TRUE;
                        }
                        window->changes_pending = TRUE;
                        window->views_shown = FALSE;
                        break;

                case NEW_META_VIEW_ACTIVATED:
                        x_message (("NEW_META_VIEW_ACTIVATED"));
                        new_view = va_arg (args, NautilusView*);
                        /* Don't ref here, reference is held by widget hierarchy. */
                        window->new_meta_views = g_slist_prepend (window->new_meta_views, new_view);
                        window->changes_pending = TRUE;
                        window->views_shown = FALSE;
                        break;

                case CV_PROGRESS_INITIAL: /* We have received an "I am loading" indication from the content view */
                        x_message (("CV_PROGRESS_INITIAL"));
                        window->cv_progress_initial = TRUE;
                        window->cv_progress_done = FALSE;
                        window->cv_progress_error = FALSE;
                        window->changes_pending = TRUE;
                        break;

                case CV_PROGRESS_ERROR: /* We have received a load error from the content view */
                        x_message (("CV_PROGRESS_ERROR"));
                        window->cv_progress_error = TRUE;
                        break;

                case CV_PROGRESS_DONE: /* The content view is done loading */
                        x_message (("CV_PROGRESS_DONE"));
                        window->cv_progress_done = TRUE;
                        break;

                case RESET_TO_IDLE: /* Someone pressed the stop button or something */
                        x_message (("RESET_TO_IDLE"));
                        window->reset_to_idle = TRUE;
                        break;

                case SYNC_STATE:
                        x_message (("SYNC_STATE"));
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
nautilus_window_end_location_change_callback (NautilusNavigationInfo *navi, gpointer data)
{
        NautilusWindow *window = data;
        char *requested_uri;
        char *error_message;
        char * scheme_string;
        
        g_assert (navi != NULL);
        
        window->cancel_tag = NULL;
        
        if (navi->result_code == NAUTILUS_NAVIGATION_RESULT_OK)
        {
                /* Navigation successful. Show the window to handle the
                 * new-window case. (Doesn't hurt if window is already showing.)
                 * Maybe this should go sometime later so the blank window isn't
                 * on screen so long.
                 */
                gtk_widget_show (GTK_WIDGET (window));
                nautilus_window_set_state_info (window, 
                                                (NautilusWindowStateItem)NAVINFO_RECEIVED, 
                                                navi, 
                                                (NautilusWindowStateItem)0);
                return;
        }
        
        /* Some sort of failure occurred. How 'bout we tell the user? */
        requested_uri = navi->navinfo.requested_uri;
        
        switch (navi->result_code) {

        case NAUTILUS_NAVIGATION_RESULT_NOT_FOUND:
                error_message = g_strdup_printf (_("Couldn't find \"%s\".\nPlease check the spelling and try again."), requested_uri);
                break;

        case NAUTILUS_NAVIGATION_RESULT_INVALID_URI:
                error_message = g_strdup_printf (_("\"%s\" is not a valid location.\nPlease check the spelling and try again."), requested_uri);
                break;

        case NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE:
                error_message = g_strdup_printf ("Couldn't display \"%s\",\nbecause Nautilus cannot handle items of this type.", requested_uri);
                break;

        case NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME:
                /* Can't create a vfs_uri and get the method from that, because 
                 * gnome_vfs_uri_new might return NULL.
                 */
                scheme_string = nautilus_str_get_prefix (requested_uri, ":");
                g_assert (scheme_string != NULL);  /* Shouldn't have gotten this error unless there's a : separator. */
                error_message = g_strdup_printf ("Couldn't display \"%s\",\nbecause Nautilus cannot handle %s: locations.",
                                                 requested_uri, scheme_string);
                g_free (scheme_string);
                break;

        default:
                /* It is so sad that we can't say anything more useful than this.
                 * When this comes up, we should figure out what's really happening
                 * and add another specific case.
                 */
                error_message = g_strdup_printf ("Nautilus cannot display \"%s\".", requested_uri);
        }
        
        if (navi != NULL) {
                nautilus_navinfo_free (navi);
        }
        
        if (!GTK_WIDGET_VISIBLE (GTK_WIDGET (window))) {
                /* Destroy never-had-a-chance-to-be-seen window. This case
                 * happens when a new window cannot display its initial URI. 
                 */
                gtk_object_destroy (GTK_OBJECT (window));
                gtk_widget_show (gnome_error_dialog (error_message));
        } else {
                /* Clean up state of already-showing window */
                nautilus_window_allow_stop (window, FALSE);
                nautilus_window_progress_indicate (window, PROGRESS_ERROR, 0, error_message);
        }
        
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
                                       Nautilus_NavigationRequestInfo *loc,
                                       NautilusView *requesting_view,
                                       NautilusLocationChangeType type,
                                       guint distance)
{
        const char *current_iid;
        
        g_assert (NAUTILUS_IS_WINDOW (window));
        g_assert (loc != NULL);
        g_assert (type == NAUTILUS_LOCATION_CHANGE_BACK || 
                  type == NAUTILUS_LOCATION_CHANGE_FORWARD || 
                  distance == 0);
        
        nautilus_window_set_state_info (window,
                                        (NautilusWindowStateItem)RESET_TO_IDLE,
                                        (NautilusWindowStateItem)SYNC_STATE,
                                        (NautilusWindowStateItem)0);
        
        while (gdk_events_pending()) {
                gtk_main_iteration();
        }
        
        nautilus_window_progress_indicate (window, PROGRESS_INITIAL, 0, _("Gathering information"));
        
        window->location_change_type = type;
        window->location_change_distance = distance;
        window->new_requesting_view = requesting_view;
        
        nautilus_window_allow_stop (window, TRUE);
        
        current_iid = NULL;
        if (window->content_view != NULL) {
                current_iid = nautilus_view_get_iid (window->content_view);
        }
        
        window->cancel_tag = nautilus_navinfo_new
                (loc, window->ni,
                 nautilus_window_end_location_change_callback,
                 window, current_iid);
}


/******** content view switching **********/
static void
view_menu_switch_views_cb (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        NautilusView *view;
        NautilusDirectory *directory;
        char *iid;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_user_data (GTK_OBJECT (widget))));
        g_return_if_fail (data != NULL);
        
        window = NAUTILUS_WINDOW (gtk_object_get_user_data (GTK_OBJECT (widget)));
        g_assert (window->ni != NULL);
        
        iid = (char *) data;
        
        directory = nautilus_directory_get (window->ni->requested_uri);
        g_assert (directory != NULL);
        nautilus_directory_set_metadata (directory,
                                         NAUTILUS_METADATA_KEY_INITIAL_VIEW,
                                         NULL,
                                         iid);
        nautilus_directory_unref (directory);
        
        nautilus_window_allow_stop (window, TRUE);
        
        view = nautilus_window_load_content_view (window, iid, window->ni, NULL);
        nautilus_window_set_state_info (window,
                                        (NautilusWindowStateItem)NEW_CONTENT_VIEW_ACTIVATED, view,
                                        (NautilusWindowStateItem)0);
}

/*
 * FIXME: Probably this should be moved to ntl-window.c with the rest of the UI.
 * I was waiting until we had the framework settled before doing that.
 */
static void
nautilus_window_load_content_view_menu (NautilusWindow *window,
                                        NautilusNavigationInfo *ni)
{
        GSList *p;
        GtkWidget *new_menu;
        int index, default_view_index;
        GtkWidget *menu_item;
        NautilusViewIdentifier *identifier;
        char *menu_label;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (GTK_IS_OPTION_MENU (window->option_cvtype));
        g_return_if_fail (ni != NULL);
        
        new_menu = gtk_menu_new ();
        
        /* Add a menu item for each available content view type */
        index = 0;
        default_view_index = -1;
        for (p = ni->content_identifiers; p != NULL; p = p->next) {
                identifier = (NautilusViewIdentifier *) p->data;
                menu_label = g_strdup_printf (_("View as %s"), identifier->name);
                menu_item = gtk_menu_item_new_with_label (menu_label);
                g_free (menu_label);
                
                if (strcmp (identifier->iid, ni->initial_content_iid) == 0) {
                        default_view_index = index;
                }
                
                /* Free copy of iid string when signal disconnected. */
                nautilus_gtk_signal_connect_free_data
                        (GTK_OBJECT (menu_item),
                         "activate",
                         GTK_SIGNAL_FUNC (view_menu_switch_views_cb), 
                         g_strdup (identifier->iid));

                /* Store reference to window in item; no need to free this. */
                gtk_object_set_user_data (GTK_OBJECT (menu_item), window);
                gtk_menu_append (GTK_MENU (new_menu), menu_item);
                gtk_widget_show (menu_item);

                ++index;
        }
        
        /*
         * We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        
        gtk_option_menu_set_menu (GTK_OPTION_MENU (window->option_cvtype),
                                  new_menu);
        
        g_assert (default_view_index >= 0);
        gtk_option_menu_set_history (GTK_OPTION_MENU (window->option_cvtype), 
                                     default_view_index);
}
