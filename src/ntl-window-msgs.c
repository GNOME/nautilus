/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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

/* #define EXTREME_DEBUGGING */

#include "nautilus.h"
#include "ntl-window-private.h"
#include "ntl-index-panel.h"
#include "explorer-location-bar.h"
#include <libnautilus/nautilus-gtk-extensions.h>
#include <stdarg.h>

static void nautilus_window_notify_selection_change(NautilusWindow *window,
						    NautilusView *view,
						    Nautilus_SelectionInfo *loc,
						    NautilusView *requesting_view);
static void nautilus_window_refresh_title (NautilusWindow *window);
static void nautilus_window_load_content_view_menu (NautilusWindow *window, NautilusNavigationInfo *ni);

typedef enum { PROGRESS_INITIAL, PROGRESS_VIEWS, PROGRESS_DONE, PROGRESS_ERROR } ProgressType;

/* Indicate progress to user interface */
static void
nautilus_window_progress_indicate(NautilusWindow *window, ProgressType type, double percent,
                                  const char *msg)
{
  if(type == PROGRESS_ERROR)
    {
      char *the_uri;

      gtk_widget_show(gnome_error_dialog_parented(msg, GTK_WINDOW(window)));

      /* If it was an error loading a URI that had been dragged to the location bar, we might
         need to reset the URI */
      the_uri = window->ni?window->ni->requested_uri:"";
      explorer_location_bar_set_uri_string(EXPLORER_LOCATION_BAR(window->ent_uri),
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
  dest_si->selected_uris._buffer = CORBA_sequence_CORBA_string_allocbuf(n);
  for(i = 0; i < n; i++)
    dest_si->selected_uris._buffer[i] = CORBA_string_dup(src_si->selected_uris._buffer[i]);

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
  GSList *cur;
  Nautilus_SelectionInfo selinfo;

  selinfo.selected_uris = loc->selected_uris;
  selinfo.content_view = nautilus_view_get_objref(NAUTILUS_VIEW(window->content_view));

  CORBA_free(window->si);

  window->si = Nautilus_SelectionInfo__alloc();
  Nautilus_SelectionInfo__copy(window->si, &selinfo);

  nautilus_window_notify_selection_change(window, window->content_view, &selinfo, requesting_view);

  for(cur = window->meta_views; cur; cur = cur->next)
    nautilus_window_notify_selection_change(window, cur->data, &selinfo, requesting_view);
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
      && requesting_view != window->content_view)
    return; /* Only pay attention to progress information from the upcoming content view, for now */
  
  /* If the progress indicates we are done, record that, otherwise
     just record the fact that we have at least begun loading.
  */
  switch(loc->type)
    {
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

  nautilus_window_set_state_info(window, item, (NautilusWindowStateItem)0);
}

/* The bulk of this file - location changing */

static void
Nautilus_NavigationInfo__copy(Nautilus_NavigationInfo *dest_ni, Nautilus_NavigationInfo *src_ni)
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
nautilus_window_update_internals(NautilusWindow *window, NautilusNavigationInfo *loci)
{
  GnomeVFSURI *new_uri;

  if(loci) /* This is a location change */
    {
      Nautilus_NavigationInfo *newni;

      /* Maintain history lists. */
      if(!window->is_reload)
        {
	  nautilus_add_to_history_list (loci->navinfo.requested_uri);
        
          if (window->is_back)
            {
              /* Going back. Remove one item from the back list and 
               * add the current item to the forward list. 
               */
              g_assert(window->back_list);
              g_assert(!strcmp(nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (window->back_list->data)), loci->navinfo.requested_uri));
              g_assert(window->ni);

              window->forward_list = g_slist_prepend(window->forward_list, 
						     nautilus_bookmark_new (window->ni->requested_uri));
              gtk_object_unref(window->back_list->data);
              window->back_list = g_slist_remove_link(window->back_list, window->back_list);
            }
          else
            {
              /* Not going back. Could be an arbitrary new uri, or could be going forward in the forward list. 
               * Remove one item from the forward list if it's the same as the request.
               * Otherwise, clobber the entire forward list. FIXME: This is not quite correct behavior (doesn't
               * match web browsers) because it doesn't distinguish between using the Forward button or list
               * to move in the Forward chain and coincidentally visiting a site that happens to be in the
               * Forward chain.
               */
              if (window->forward_list)
                {
                  if (strcmp (loci->navinfo.requested_uri, 
                  	      nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (window->forward_list->data))) == 0)
                    {
                      gtk_object_unref(window->forward_list->data);
                      window->forward_list = g_slist_remove_link(window->forward_list, window->forward_list);
                    }
                  else
                    {
                      g_slist_foreach(window->forward_list, (GFunc)gtk_object_unref, NULL);
                      g_slist_free(window->forward_list); window->forward_list = NULL;
                    }
                }

              if (window->ni)
                {
                  /* Store bookmark for current location in back list, unless there is no current location */
                  window->back_list = g_slist_prepend(window->back_list, 
						      nautilus_bookmark_new (window->ni->requested_uri));
		}
            }
        }

      new_uri = gnome_vfs_uri_new (loci->navinfo.requested_uri);
      if(!new_uri)
        new_uri = gnome_vfs_uri_new (loci->navinfo.actual_uri);
      if(new_uri)
        {
          nautilus_window_allow_up(window, 
                                   gnome_vfs_uri_has_parent(new_uri));
          gnome_vfs_uri_unref(new_uri);
        }
      else
        nautilus_window_allow_up(window, FALSE);

      newni = Nautilus_NavigationInfo__alloc();
      Nautilus_NavigationInfo__copy(newni, &loci->navinfo);
      CORBA_free(window->ni);
      window->ni = newni;

      CORBA_free(window->si);
      window->si = NULL;

      nautilus_window_load_content_view_menu (window, loci);

      /* Notify the index panel of the location change. FIXME: Eventually,
         this will not be necessary when we restructure the index panel to
         be a NautilusView */
      nautilus_index_panel_set_uri(window->index_panel, loci->navinfo.requested_uri);
    }

  nautilus_window_allow_back(window, window->back_list != NULL);
  nautilus_window_allow_forward(window, window->forward_list != NULL);
  
  explorer_location_bar_set_uri_string(EXPLORER_LOCATION_BAR(window->ent_uri),
				       window->ni->requested_uri);
  nautilus_index_panel_set_uri (NAUTILUS_INDEX_PANEL (window->index_panel), window->ni->requested_uri);

  nautilus_window_refresh_title (window);
}

static void
nautilus_window_update_view(NautilusWindow *window,
                            NautilusView *view,
                            Nautilus_NavigationInfo *loci,
                            Nautilus_SelectionInfo *seli,
                            NautilusView *requesting_view,
			    NautilusView *content_view)
{
  g_return_if_fail(view);

  loci->self_originated = (view == requesting_view);

  nautilus_view_notify_location_change(NAUTILUS_VIEW(view), loci);

  if(seli)
    {
      seli->content_view = nautilus_view_get_client_objref(content_view);
      nautilus_window_notify_selection_change(window, view, seli, NULL);
    }
}

void
nautilus_window_view_destroyed(NautilusView *view, NautilusWindow *window)
{
  NautilusWindowStateItem item = VIEW_ERROR;
  nautilus_window_set_state_info(window, item, view, (NautilusWindowStateItem)0);
}

static void
nautilus_window_refresh_title (NautilusWindow *window)
{
  GnomeVFSURI *vfs_uri;
	
  g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	
  vfs_uri = gnome_vfs_uri_new (nautilus_window_get_requested_uri (window));
  if (vfs_uri == NULL)
    {
      gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));
    }
  else
    {
      gchar *short_name;
      gchar *new_title;

      short_name = gnome_vfs_uri_extract_short_name (vfs_uri);
      gnome_vfs_uri_unref (vfs_uri);

      g_assert (short_name != NULL);

      new_title = g_strdup_printf (_("Nautilus: %s"), short_name);
      gtk_window_set_title (GTK_WINDOW (window), new_title);
      g_free (new_title);
		
      g_free (short_name);
    }

}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
nautilus_window_has_really_changed(NautilusWindow *window)
{
  GSList *cur, *discard_views;
  GSList *new_meta_views;

  new_meta_views = window->new_meta_views;
  window->new_meta_views = NULL;

  if(window->new_content_view)
    {
      if(!GTK_WIDGET(window->new_content_view)->parent)
        {
          if(window->content_view)
            gtk_signal_disconnect_by_func(GTK_OBJECT(window->content_view), nautilus_window_view_destroyed, window);
          nautilus_window_set_content_view(window, window->new_content_view);
        }
      gtk_object_unref(GTK_OBJECT(window->new_content_view));
      window->new_content_view = NULL;
    }

  if(new_meta_views)
    {
      /* Do lots of shuffling to make sure we don't remove views that were already there, but add new views */
      for(cur = new_meta_views; cur; cur = cur->next)
        {
          if(!GTK_OBJECT_DESTROYED(cur->data) && !GTK_WIDGET(cur->data)->parent)
            nautilus_window_add_meta_view(window, cur->data);
          gtk_object_unref(cur->data);
        }

      for(discard_views = NULL, cur = window->meta_views; cur; cur = cur->next)
        if(!g_slist_find(new_meta_views, cur->data))
          discard_views = g_slist_prepend(discard_views, cur->data);
      g_slist_free(new_meta_views);
  
      for(cur = discard_views; cur; cur = cur->next)
        nautilus_window_remove_meta_view(window, cur->data);
      g_slist_free(discard_views);
    }

  if(window->pending_ni)
    {
      nautilus_window_update_internals(window, window->pending_ni);
      nautilus_navinfo_free(window->pending_ni); window->pending_ni = NULL;
    }
}

/* This is called when we are done loading to get rid of the load_info structure. */
static void
nautilus_window_free_load_info(NautilusWindow *window)
{
#if defined(EXTREME_DEBUGGING)
  g_message("-> FREE_LOAD_INFO <-");
#endif

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
  window->changes_pending =
    window->views_shown =
    window->view_bombed_out =
    window->view_activation_complete =
    window->cv_progress_initial =
    window->cv_progress_done =
    window->cv_progress_error = 
    window->sent_update_view =
    window->reset_to_idle = FALSE;
}

/* Meta view handling */
static NautilusView *
nautilus_window_load_meta_view(NautilusWindow *window,
                               const char *iid,
                               NautilusView *requesting_view)
{
  NautilusView *meta_view = NULL;
  GSList *curview;

  for(curview = window->meta_views; curview; curview = curview->next)
    {
      meta_view = NAUTILUS_VIEW(curview->data);
      if(!strcmp(nautilus_view_get_iid(meta_view), iid))
        break;
    }

  if(!curview)
    {
      meta_view = NAUTILUS_VIEW(gtk_widget_new(nautilus_meta_view_get_type(), "main_window", window, NULL));
      nautilus_window_connect_view(window, meta_view);
      if(!nautilus_view_load_client(meta_view, iid))
	{
	  gtk_widget_unref(GTK_WIDGET(meta_view));
	  meta_view = NULL;
	}
    }

  if(meta_view)
    {
      gtk_object_ref(GTK_OBJECT(meta_view));

      nautilus_view_set_active_errors(meta_view, TRUE);
    }

  return meta_view;
}

void
nautilus_window_request_location_change(NautilusWindow *window,
					Nautilus_NavigationRequestInfo *loc,
					NautilusView *requesting_view)
{  
  nautilus_window_change_location(window, loc, requesting_view, FALSE, FALSE);
}

static NautilusView *
nautilus_window_load_content_view(NautilusWindow *window,
                                  const char *iid,
                                  Nautilus_NavigationInfo *navinfo,
                                  NautilusView **requesting_view)
{
  NautilusView *content_view = window->content_view;
  NautilusView *new_view;

  g_return_val_if_fail(iid, NULL);
  g_return_val_if_fail(navinfo, NULL);

  if((!content_view || !NAUTILUS_IS_VIEW(content_view)) || strcmp(nautilus_view_get_iid(content_view), iid))
    {
      if(requesting_view != NULL && *requesting_view == window->content_view)
        {
          /* If we are going to be zapping the old view,
             we definitely don't want any of the new views
             thinking they made the request */
          *requesting_view = NULL;
        }

      new_view = NAUTILUS_VIEW(gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL));

      nautilus_window_connect_view(window, new_view);

      if(!nautilus_view_load_client(new_view, iid))
	{
	  gtk_widget_unref(GTK_WIDGET(new_view));
	  new_view = NULL;
	}
    }
  else
    new_view = window->content_view;

  if(new_view && NAUTILUS_IS_VIEW(new_view))
    {
      gtk_object_ref(GTK_OBJECT(new_view));

      navinfo->content_view = nautilus_view_get_client_objref(new_view);

      nautilus_view_set_active_errors(new_view, TRUE);
    }
  else
    new_view = NULL;

  return new_view;
}

static gboolean
nautilus_window_update_state(gpointer data)
{
  NautilusWindow *window = data;
  GSList *cur;
  gboolean retval = FALSE;

  if(window->making_changes)
    {
#ifdef EXTREME_DEBUGGING
      g_message("In the middle of making changes %d (action_tag %d) - RETURNING",
                window->making_changes, window->action_tag);
#endif
      return FALSE;
    }

  window->made_changes = 0;
  window->making_changes++;

#ifdef EXTREME_DEBUGGING
  g_message(">>> nautilus_window_update_state (action tag is %d):", window->action_tag);
  g_print("made_changes %d, making_changes %d\n", window->made_changes, window->making_changes);
  g_print("changes_pending %d, is_back %d, views_shown %d, view_bombed_out %d, view_activation_complete %d\n",
          window->changes_pending, window->is_back, window->views_shown,
          window->view_bombed_out, window->view_activation_complete);
  g_print("sent_update_view %d, cv_progress_initial %d, cv_progress_done %d, cv_progress_error %d, reset_to_idle %d\n",
          window->sent_update_view, window->cv_progress_initial, window->cv_progress_done, window->cv_progress_error,
          window->reset_to_idle);
#endif

  /* Now make any needed state changes based on available information */
  if(window->view_bombed_out && window->error_views)
    {
      for(cur = window->error_views; cur; cur = cur->next)
        {
          NautilusView *error_view = cur->data;

          if(NAUTILUS_IS_CONTENT_VIEW(error_view))
            {
              if(error_view == window->new_content_view)
                {
                  window->made_changes++;
                  window->reset_to_idle = TRUE;
                }

              if(error_view == window->content_view)
                {
                  if(GTK_WIDGET(window->content_view)->parent)
                    gtk_container_remove(GTK_CONTAINER(GTK_WIDGET(window->content_view)->parent),
                                         GTK_WIDGET(window->content_view));

                  window->content_view = NULL;
                  window->made_changes++;
                }
              window->cv_progress_error = TRUE;
            }
          else if(NAUTILUS_IS_META_VIEW(error_view))
            {
              if(g_slist_find(window->new_meta_views, error_view))
                {
                  window->new_meta_views = g_slist_remove(window->new_meta_views, error_view);
                  gtk_widget_unref(GTK_WIDGET(error_view));
                }
              if(g_slist_find(window->meta_views, error_view))
                {
                  nautilus_window_remove_meta_view(window, error_view);
                }
            }
          gtk_widget_unref(GTK_WIDGET(error_view));
        }
      g_slist_free(window->error_views);
      window->error_views = NULL;

      window->view_bombed_out = FALSE;
    }

  if(window->reset_to_idle)
    {
      GSList *cur;

#ifdef EXTREME_DEBUGGING
      g_message("Reset to idle!");
#endif

      window->changes_pending = FALSE;
      window->made_changes++;
      window->reset_to_idle = FALSE;

      if(window->cancel_tag)
        {
          gnome_vfs_async_cancel(window->cancel_tag);
          if(window->pending_ni)
            window->pending_ni->ah = NULL;
          window->cancel_tag = NULL;
        }

      if(window->pending_ni)
        {
          /* Tell previously-notified views to go back to the old page */
          for(cur = window->meta_views; cur; cur = cur->next)
            {
              if(g_slist_find(window->new_meta_views, cur->data))
                nautilus_window_update_view(window, cur->data, window->ni, window->si,
                                            NULL, window->content_view);
            }


          if(window->new_content_view
             && window->new_content_view == window->content_view)
            nautilus_window_update_view(window, window->content_view, window->ni, window->si,
                                        NULL, window->content_view);
        }

      if(window->new_content_view)
        gtk_widget_unref(GTK_WIDGET(window->new_content_view));

      for(cur = window->new_meta_views; cur; cur = cur->next)
        gtk_widget_unref(GTK_WIDGET(cur->data));
      g_slist_free(window->new_meta_views);
  
      nautilus_window_free_load_info(window);

      nautilus_window_allow_stop(window, FALSE);
    }

  if(window->changes_pending)
    {
      window->state = NW_LOADING_VIEWS;

#ifdef EXTREME_DEBUGGING
      g_message("Changes pending");
#endif

      if(window->pending_ni && !window->new_content_view && !window->cv_progress_error
         && !window->view_activation_complete)
        {
          window->new_content_view = nautilus_window_load_content_view(window, window->pending_ni->default_content_iid,
                                                                       &window->pending_ni->navinfo,
                                                                       &window->new_requesting_view);

          for(cur = window->pending_ni->meta_iids; cur; cur = cur->next)
            {
              NautilusView *meta_view = nautilus_window_load_meta_view(window, cur->data, window->new_requesting_view);

              if(meta_view)
                window->new_meta_views = g_slist_prepend(window->new_meta_views, meta_view);
            }

          window->view_activation_complete = TRUE;

          window->made_changes++;
        }

      if(window->view_activation_complete
         && !window->sent_update_view)
        {
          Nautilus_NavigationInfo *ni;
          Nautilus_SelectionInfo *si;

          if(window->pending_ni)
            {
              ni = &window->pending_ni->navinfo;
              si = NULL;
            }
          else
            {
              ni = window->ni;
              si = window->si;
            }

#ifdef EXTREME_DEBUGGING
          g_message("!!! Sending update_view");
#endif

          if(window->new_content_view)
            nautilus_window_update_view(window, window->new_content_view, ni, si,
                                        window->new_requesting_view, window->new_content_view);
          else
            window->cv_progress_error = TRUE;

          for(cur = window->new_meta_views; cur; cur = cur->next)
            nautilus_window_update_view(window, cur->data, ni, si,
                                        window->new_requesting_view, window->new_content_view);

          window->sent_update_view = TRUE;
          window->made_changes++;
        }

      if(!window->cv_progress_error
         && window->view_activation_complete
         && window->cv_progress_initial
         && !window->views_shown)
        {
          nautilus_window_has_really_changed(window);
          window->views_shown = TRUE;
          window->made_changes++;
        }

      if(window->cv_progress_error
         || window->cv_progress_done)
        {
          window->made_changes++;
          window->reset_to_idle = TRUE;
#ifdef EXTREME_DEBUGGING
          g_message("cv_progress_(error|done) kicking in");
#endif
        }
    }

  if(window->made_changes)
    {
      if(!window->action_tag)
        window->action_tag = g_idle_add_full(G_PRIORITY_LOW, nautilus_window_update_state, window, NULL);

      retval = TRUE;
      window->made_changes = 0;
    }
  else
    window->action_tag = 0;

  window->making_changes--;

#ifdef EXTREME_DEBUGGING
  g_message("update_state done (new action tag is %d, making_changes is %d) <<<", window->action_tag,
            window->making_changes);
#endif

  return retval;
}

void
nautilus_window_set_state_info(NautilusWindow *window, ...)
{
  va_list args;
  NautilusWindowStateItem item_type;
  NautilusView *new_view;
  gboolean do_sync = FALSE;

  if(window->made_changes) /* Ensure that changes happen in-order */
    {
      if(window->action_tag)
        {
          g_source_remove(window->action_tag);
          window->action_tag = 0;
        }

      nautilus_window_update_state(window);
    }

  va_start(args, window);

  while((item_type = va_arg(args, NautilusWindowStateItem)))
    {
      switch(item_type)
        {
        case NAVINFO_RECEIVED: /* The information needed for a location change to continue has been received */
#ifdef EXTREME_DEBUGGING
          g_message("NAVINFO_RECEIVED"); 
#endif
          window->pending_ni = va_arg(args, NautilusNavigationInfo*);
          window->cancel_tag = NULL;
          window->changes_pending = TRUE;
          break;
        case VIEW_ERROR:
          window->view_bombed_out = TRUE;
          new_view = va_arg(args, NautilusView*);
#ifdef EXTREME_DEBUGGING
          g_message("VIEW_ERROR on %p", new_view);
#endif
          gtk_object_ref(GTK_OBJECT(new_view)); /* Ya right */
          window->error_views = g_slist_prepend(window->error_views, new_view);
          break;
        case NEW_CONTENT_VIEW_ACTIVATED:
#ifdef EXTREME_DEBUGGING
          g_message("NEW_CONTENT_VIEW_ACTIVATED");
#endif
          g_return_if_fail(!window->new_content_view);
          new_view = va_arg(args, NautilusView*);
          gtk_object_ref(GTK_OBJECT(new_view));
          window->new_content_view = new_view;
          if(!window->pending_ni)
            window->view_activation_complete = TRUE;
          window->changes_pending = TRUE;
          window->views_shown = FALSE;
          break;
        case NEW_META_VIEW_ACTIVATED:
#ifdef EXTREME_DEBUGGING
          g_message("NEW_META_VIEW_ACTIVATED");
#endif
          new_view = va_arg(args, NautilusView*);
          gtk_object_ref(GTK_OBJECT(new_view));
          window->new_meta_views = g_slist_prepend(window->new_meta_views, new_view);
          window->changes_pending = TRUE;
          window->views_shown = FALSE;
          break;
        case CV_PROGRESS_INITIAL: /* We have received an "I am loading" indication from the content view */
#ifdef EXTREME_DEBUGGING
          g_message("CV_PROGRESS_INITIAL");
#endif
          window->cv_progress_initial = TRUE;
          window->cv_progress_done = window->cv_progress_error = FALSE;
          window->changes_pending = TRUE;
          break;
        case CV_PROGRESS_ERROR: /* We have received a load error from the content view */
          window->cv_progress_error = TRUE;
#ifdef EXTREME_DEBUGGING
          g_message("CV_PROGRESS_ERROR");
#endif
          break;
        case CV_PROGRESS_DONE: /* The content view is done loading */
          window->cv_progress_done = TRUE;
#ifdef EXTREME_DEBUGGING
          g_message("CV_PROGRESS_DONE");
#endif
          break;
        case RESET_TO_IDLE: /* Someone pressed the stop button or something */
          window->reset_to_idle = TRUE;
#ifdef EXTREME_DEBUGGING
          g_message("RESET_TO_IDLE");
#endif
          break;
        case SYNC_STATE:
          do_sync = TRUE;
#ifdef EXTREME_DEBUGGING
          g_message("SYNC_STATE");
#endif
          break;
        default:
          break;
        }
    }

  va_end(args);

  window->made_changes++;
  if(!window->making_changes)
    {
      if(do_sync)
        {
          if(window->action_tag)
            {
              g_message("Doing sync - action_tag was %d", window->action_tag);
              g_source_remove(window->action_tag);
              window->action_tag = 0;
            }
          if(nautilus_window_update_state(window))
            do_sync = FALSE;
        }

      if(!window->action_tag && !do_sync)
        {
          window->action_tag = g_idle_add_full(G_PRIORITY_LOW, nautilus_window_update_state, window, NULL);
#ifdef EXTREME_DEBUGGING
          g_message("Added callback to update_state - tag is %d", window->action_tag);
#endif
        }
    }
}

static void
nautilus_window_change_location_2(NautilusNavigationInfo *navi, gpointer data)
{
  NautilusWindow *window = data;
  char *errmsg;

  /* Do various error checking here */

  window->cancel_tag = NULL;

  if(!navi)
    {
      errmsg = _("The chosen file could not be retrieved.");
      goto errout;
    }

  if(!navi->default_content_iid)
    {
      errmsg = _("There is no known method of displaying the chosen file.");
      goto errout;
    }

  if (navi->use_new_window)
  {
  	/* Reset state of old window before creating new window. */
	nautilus_window_set_state_info(window, 
				       (NautilusWindowStateItem)RESET_TO_IDLE, 
				       (NautilusWindowStateItem)SYNC_STATE, 
				       (NautilusWindowStateItem)0);

  	window = nautilus_app_create_window ();
  	gtk_widget_show (GTK_WIDGET (window));
  }

  nautilus_window_set_state_info(window, (NautilusWindowStateItem)NAVINFO_RECEIVED, navi, (NautilusWindowStateItem)0);
  return;

 errout:
  nautilus_window_allow_stop(window, FALSE);
  if (navi != NULL)
    nautilus_navinfo_free(navi);
  window->is_back = FALSE;
  nautilus_window_progress_indicate(window, PROGRESS_ERROR, 0, errmsg);
}

void
nautilus_window_change_location(NautilusWindow *window,
				Nautilus_NavigationRequestInfo *loc,
				NautilusView *requesting_view,
				gboolean is_back,
                                gboolean is_reload)
{
  nautilus_window_set_state_info(window, (NautilusWindowStateItem)RESET_TO_IDLE, (NautilusWindowStateItem)SYNC_STATE, (NautilusWindowStateItem)0);

  while (gdk_events_pending())
    gtk_main_iteration();

  nautilus_window_progress_indicate(window, PROGRESS_INITIAL, 0, _("Gathering information"));
  window->is_back = is_back;
  window->is_reload = is_reload;
  window->new_requesting_view = requesting_view;

  nautilus_window_allow_stop(window, TRUE);

  window->cancel_tag =
    nautilus_navinfo_new(loc, window->ni, nautilus_window_change_location_2, window);
}


/******** content view switching **********/
static void
view_menu_switch_views_cb (GtkWidget *widget, gpointer data)
{
  NautilusWindow *window;
  NautilusView *view;
  char *iid;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_user_data(GTK_OBJECT(widget))));
  g_return_if_fail (data != NULL);

  window = NAUTILUS_WINDOW (gtk_object_get_user_data (GTK_OBJECT(widget)));
  g_assert (window->ni != NULL);

  iid = (char *)data;
  nautilus_window_allow_stop(window, TRUE);

  view = nautilus_window_load_content_view (window, iid, window->ni, NULL);
  nautilus_window_set_state_info(window, (NautilusWindowStateItem)NEW_CONTENT_VIEW_ACTIVATED, view, (NautilusWindowStateItem)0);
  window->is_back = FALSE;
  window->is_reload = TRUE;
}

/*
 * FIXME: Probably this should be moved to ntl-window.c with the rest of the UI.
 * I was waiting until we had the framework settled before doing that.
 */
static void
nautilus_window_load_content_view_menu (NautilusWindow *window, NautilusNavigationInfo *ni)
{
  GSList *iter_new;
  GtkWidget *new_menu;
  gint index, default_view_index;

  g_return_if_fail (NAUTILUS_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_OPTION_MENU (window->option_cvtype));
  g_return_if_fail (ni != NULL);

  new_menu = gtk_menu_new ();

  /* Add a menu item for each available content view type */
  iter_new = ni->content_identifiers;
  index = 0;
  default_view_index = -1;
  while (iter_new != NULL)
    {
      GtkWidget *menu_item;
      NautilusViewIdentifier *identifier;
      gchar *menu_label;

      identifier = (NautilusViewIdentifier *)iter_new->data;
      menu_label = g_strdup_printf (_("View as %s"), identifier->name);
      menu_item = gtk_menu_item_new_with_label (menu_label);
      g_free (menu_label);
    
      if (strcmp (identifier->iid, ni->default_content_iid) == 0)
        {
          default_view_index = index;
        }

      /* Free copy of iid string when signal disconnected. */
      nautilus_gtk_signal_connect_free_data (
                                             GTK_OBJECT (menu_item), 
                                             "activate", 
                                             GTK_SIGNAL_FUNC (view_menu_switch_views_cb), 
                                             g_strdup (identifier->iid));
      /* Store reference to window in item; no need to free this. */
      gtk_object_set_user_data (GTK_OBJECT (menu_item), window);
      gtk_menu_append (GTK_MENU (new_menu), menu_item);
      gtk_widget_show (menu_item);
      iter_new = g_slist_next (iter_new);
      ++index;
    }

  /*
   * We create and attach a new menu here because adding/removing
   * items from existing menu screws up the size of the option menu.
   */

  gtk_option_menu_set_menu (GTK_OPTION_MENU (window->option_cvtype), new_menu);

  g_assert (default_view_index >= 0);
  gtk_option_menu_set_history (GTK_OPTION_MENU (window->option_cvtype), 
  			       default_view_index);
}
