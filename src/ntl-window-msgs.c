#include "nautilus.h"
#include "ntl-window-private.h"
#include "explorer-location-bar.h"

struct _NautilusWindowLoadInfo {
  enum {
    GETTING_URI_INFO,
    WAITING_FOR_VIEWS,
    PROCESSING,
    DONE
  } state;

  union {
    struct {
      guint cancel_tag;
    } getting_uri_info;
  } u;

  /* Valid in the WAITING_FOR_VIEWS or PROCESSING states */
  NautilusNavigationInfo *ni;

  GSList *new_meta_views;
  NautilusView *new_content_view;

  gboolean got_content_progress;

  gboolean is_back;
};

static void nautilus_window_notify_selection_change(NautilusWindow *window,
						    NautilusView *view,
						    Nautilus_SelectionInfo *loc,
						    NautilusView *requesting_view);

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


void
nautilus_window_request_status_change(NautilusWindow *window,
                                      Nautilus_StatusRequestInfo *loc,
                                      NautilusView *requesting_view)
{
  nautilus_window_set_status(window, loc->status_string);
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
nautilus_window_request_location_change(NautilusWindow *window,
					Nautilus_NavigationRequestInfo *loc,
					NautilusView *requesting_view)
{  
  nautilus_window_change_location(window, loc, requesting_view, FALSE);
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
nautilus_window_request_progress_change(NautilusWindow *window,
					Nautilus_ProgressRequestInfo *loc,
					NautilusView *requesting_view)
{
  if (window->load_info)
    {
      if(requesting_view != window->load_info->new_content_view)
	return; /* Only pay attention to progress information from the main view, for now */

      /* We have gotten something from the content view, which means it is safe to do the "switchover" to the new page */
      window->load_info->got_content_progress = TRUE;
      nautilus_window_end_location_change(window);
      nautilus_window_allow_stop (window, !(loc->type == Nautilus_PROGRESS_DONE_OK
					    || loc->type == Nautilus_PROGRESS_DONE_ERROR));

      g_message("Progress is %f", loc->amount);
    }
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
nautilus_window_change_location_internal(NautilusWindow *window, Nautilus_NavigationInfo *loci, gboolean is_back)
{
  GnomeVFSURI *new_uri;

  CORBA_free(window->si); window->si = NULL;

  /* Maintain history lists. */
  if (is_back)
    {
      /* Going back. Remove one item from the prev list and add the current item to the next list. */

      g_assert(window->uris_prev);
      g_assert(!strcmp((const gchar*)window->uris_prev->data, loci->requested_uri));
      g_assert(window->ni);

      window->uris_next = g_slist_prepend(window->uris_next, g_strdup(window->ni->requested_uri));
      g_free(window->uris_prev->data);
      window->uris_prev = g_slist_remove_link(window->uris_prev, window->uris_prev);
    }
  else
    {
      /* Going forward. Remove one item from the next if it's the same as the the request.
       * Otherwise, clobber the entire next list.
       */

      if (window->uris_next && !strcmp(loci->requested_uri, (const gchar*)window->uris_next->data))
	{
	  g_free(window->uris_next->data);
	  window->uris_next = g_slist_remove_link(window->uris_next, window->uris_next);
	}
      else
	{
	  g_slist_foreach(window->uris_next, (GFunc)g_free, NULL);
	  g_slist_free(window->uris_next); window->uris_next = NULL;
	}
      if (window->ni)
	window->uris_prev = g_slist_prepend(window->uris_prev, g_strdup(window->ni->requested_uri));
    }

  nautilus_window_allow_back(window, window->uris_prev?TRUE:FALSE);
  nautilus_window_allow_forward(window, window->uris_next?TRUE:FALSE);
  
  new_uri = gnome_vfs_uri_new (loci->requested_uri);
  nautilus_window_allow_up(window, 
			   gnome_vfs_uri_has_parent(new_uri));
  gnome_vfs_uri_destroy(new_uri);

  if(window->ni != loci)
    {
      Nautilus_NavigationInfo *newni;

      newni = Nautilus_NavigationInfo__alloc();
      Nautilus_NavigationInfo__copy(newni, loci);
      CORBA_free(window->ni);
      window->ni = newni;

      CORBA_free(window->si);
      window->si = NULL;
    }

  explorer_location_bar_set_uri_string(EXPLORER_LOCATION_BAR(window->ent_uri),
                                       loci->requested_uri);
}

static void
nautilus_window_update_view(NautilusWindow *window,
                            NautilusView *view,
                            Nautilus_NavigationInfo *loci,
                            NautilusView *requesting_view,
			    NautilusView *content_view)
{
  g_return_if_fail(view);

  loci->self_originated = (view == requesting_view);

  nautilus_view_notify_location_change(NAUTILUS_VIEW(view), loci);

  if(window->si)
    {
      window->si->content_view = nautilus_view_get_client_objref(content_view);
      nautilus_window_notify_selection_change(window, view, window->si, NULL);
    }
}

void
nautilus_window_view_destroyed(NautilusView *view, NautilusWindow *window)
{
  if(NAUTILUS_IS_CONTENT_VIEW(view))
    {
      if(window->load_info)
	{
	  if(view == window->load_info->new_content_view)
	    {
	      nautilus_window_display_error
		(window,
		 _("A software component, needed to display the requested page, has turned fatalistic."));
	      nautilus_window_end_location_change(window);
	    }
	}

      if(view == window->content_view)
	window->content_view = NULL;
    }
  else if(NAUTILUS_IS_META_VIEW(view))
    {
      if(window->load_info)
	window->load_info->new_meta_views = g_slist_remove(window->load_info->new_meta_views, view);

      nautilus_window_remove_meta_view(window, view);
    }
}

static void
nautilus_window_load_content_view(NautilusWindow *window,
                                  const char *iid,
                                  NautilusNavigationInfo *loci,
                                  NautilusView **requesting_view)
{
  NautilusView *content_view = window->content_view;
  NautilusView *new_view;

  g_return_if_fail(iid);
  g_return_if_fail(loci);
  g_return_if_fail(requesting_view);

  if((!content_view || !NAUTILUS_IS_VIEW(content_view)) || strcmp(nautilus_view_get_iid(content_view), iid))
    {
      if(*requesting_view == window->content_view) /* If we are going to be zapping the old view,
                                                      we definitely don't want any of the new views
                                                      thinking they made the request */
        *requesting_view = NULL;

      new_view = NAUTILUS_VIEW(gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL));

      nautilus_window_connect_view(window, new_view);

      if(!nautilus_view_load_client(new_view, iid))
	{
	  gtk_widget_destroy(GTK_WIDGET(new_view));
	  new_view = NULL;
	}
    }
  else
    new_view = window->content_view;

  if(new_view && NAUTILUS_IS_VIEW(new_view))
    {
      gtk_object_ref(GTK_OBJECT(new_view));

      loci->navinfo.content_view = nautilus_view_get_client_objref(new_view);
      window->load_info->new_content_view = new_view;

      nautilus_view_set_active_errors(new_view, TRUE);

      nautilus_window_update_view(window, new_view, &(loci->navinfo), *requesting_view, window->load_info->new_content_view);
    }
}

static void
nautilus_window_load_meta_view(NautilusWindow *window,
                               const char *iid,
                               NautilusNavigationInfo *loci,
                               NautilusView *requesting_view)
{
  NautilusView *meta_view;
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
	  gtk_widget_destroy(GTK_WIDGET(meta_view));
	  meta_view = NULL;
	}
    }

  if(meta_view)
    {
      gtk_object_ref(GTK_OBJECT(meta_view));

      nautilus_view_set_active_errors(meta_view, TRUE);

      window->load_info->new_meta_views = g_slist_prepend(window->load_info->new_meta_views, meta_view);

      nautilus_window_update_view(window, meta_view, &(loci->navinfo), requesting_view, window->load_info->new_content_view);
    }
}

void
nautilus_window_end_location_change(NautilusWindow *window)
{
  GSList *cur, *discard_views;

  if (!window->load_info)
    {
      if(window->content_view)
	nautilus_view_stop_location_change (window->content_view);

      g_slist_foreach(window->meta_views, (GFunc) nautilus_view_stop_location_change, NULL);

      nautilus_window_allow_stop(window, FALSE);

      return;
    }

  switch (window->load_info->state)
    {
    case DONE: /* The normal case - the page has finished loading successfully */
      /* Do lots of shuffling to make sure we don't remove views that were already there, but add new views */
      for(cur = window->load_info->new_meta_views; cur; cur = cur->next)
	{
	  if(!GTK_WIDGET(cur->data)->parent)
	    nautilus_window_add_meta_view(window, cur->data);
	  gtk_object_unref(cur->data);
	  /* nautilus_view_set_active_errors(cur->data, FALSE); */
	}
      for(cur = window->meta_views; cur; cur = cur->next)
	if(!g_slist_find(window->load_info->new_meta_views, cur->data))
	  discard_views = g_slist_prepend(discard_views, cur->data);

      for(cur = discard_views; cur; cur = cur->next)
	nautilus_window_remove_meta_view(window, cur->data);
      g_slist_free(discard_views);

      if(!GTK_WIDGET(window->load_info->new_content_view)->parent)
	nautilus_window_set_content_view(window, window->load_info->new_content_view);
      gtk_object_unref(GTK_OBJECT(window->load_info->new_content_view));
      /* nautilus_view_set_active_errors(window->load_info->new_content_view, FALSE); */

      nautilus_window_change_location_internal(window, &window->load_info->ni->navinfo, window->load_info->is_back);
      break;

      /* all of the following are error cases - we try to recover as best we can */
    case WAITING_FOR_VIEWS:
      for(cur = window->load_info->new_meta_views; cur; cur = cur->next)
	{
	  gtk_widget_unref(cur->data);
	  /* nautilus_view_set_active_errors(cur->data, FALSE); */
	}
      if(window->load_info->new_content_view)
	{
	  gtk_widget_unref(GTK_WIDGET(window->load_info->new_content_view));
	  /* nautilus_view_set_active_errors(window->load_info->new_content_view, FALSE); */
	}

      /* Tell previously-notified views to go back to the old page */
      for(cur = window->meta_views; cur; cur = cur->next)
	{
	  if(g_slist_find(window->load_info->new_meta_views, cur->data))
	    nautilus_window_update_view(window, cur->data, window->ni, NULL, window->content_view);
	}
      if(window->load_info->new_content_view == window->content_view)
	nautilus_window_update_view(window, window->content_view, window->ni, NULL, window->content_view);
      explorer_location_bar_set_uri_string(EXPLORER_LOCATION_BAR(window->ent_uri),
					   window->ni->requested_uri);

    case GETTING_URI_INFO:
    default:
      break;

    case PROCESSING:
      return; /* We don't want to do anything in the processing state - someone else is already busy manipulating things */
    }

  nautilus_navinfo_free(window->load_info->ni);

  g_slist_free(window->load_info->new_meta_views);
  g_free(window->load_info); window->load_info = NULL;

  nautilus_window_allow_stop(window, FALSE);
}

/* This is the most complicated routine in Nautilus. Steps include:
   1. Get the information needed to load the URL.
   2. If a load is not possible, fail.
   3. Update the history, UI, and internal state for the proposed change.
   4. Load/notify content view.
   5. Load/notify meta views.
 */

/* Step 2 */
static void
nautilus_window_change_location_2(NautilusNavigationInfo *ni, gpointer data)
{
  GSList *cur, *discard_views;

  NautilusWindow *window = data;

  window->load_info->state = PROCESSING;
  window->load_info->ni = ni;

  if(!ni)
    {
      nautilus_window_display_error
	(window,
	 _("The chosen hyperlink is invalid, or points to an inaccessable page."));

      nautilus_window_end_location_change(window);

      return;
    }

  if(!ni->content_iid)
    {
      nautilus_window_display_error
	(window,
	 _("There is not any known method of displaying the selected page."));

      nautilus_window_end_location_change(window);

      return;
    }

  nautilus_window_load_content_view(window, ni->content_iid, ni, (NautilusView **)&ni->requesting_view);
  if(!window->load_info->new_content_view)
    {
      nautilus_window_display_error
	(window,
	 _("The software needed to display this page could not be loaded."));

      nautilus_window_end_location_change(window);

      return;
    }

  /* Step 5 */

  /* Figure out which meta views are going to go away */
  discard_views = NULL;
  for(cur = window->meta_views; cur; cur = cur->next)
    {
      NautilusView *view = cur->data;

      if(!g_slist_find_custom(ni->meta_iids, view->iid, (GCompareFunc)g_str_equal))
	{
	  if(cur->data == ni->requesting_view)
	    ni->requesting_view = NULL;

	  discard_views = g_slist_prepend(discard_views, view);
	}
    }

  for(cur = ni->meta_iids; cur; cur = cur->next)
    nautilus_window_load_meta_view(window, cur->data, ni, ni->requesting_view);

  for(cur = discard_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, cur->data);
  g_slist_free(discard_views);

  if(window->load_info->got_content_progress)
    {
      window->load_info->state = DONE;
      nautilus_window_end_location_change(window);
    }
  else
    window->load_info->state = WAITING_FOR_VIEWS;
}

/* Step 1 */
void
nautilus_window_change_location(NautilusWindow *window,
				Nautilus_NavigationRequestInfo *loc,
				NautilusView *requesting_view,
				gboolean is_back)
{
  guint cancel_tag;

  nautilus_window_end_location_change(window); /* End any previous location change that was pending. */

  while(gdk_events_pending())
    gtk_main_iteration();

  nautilus_window_allow_stop(window, TRUE);

  /* Init load_info */
  window->load_info = g_new0(NautilusWindowLoadInfo, 1);
  window->load_info->state = GETTING_URI_INFO;
  window->load_info->is_back = is_back;

  cancel_tag =
    nautilus_navinfo_new(loc, window->ni, requesting_view, nautilus_window_change_location_2, window);

  if(window->load_info)
    window->load_info->u.getting_uri_info.cancel_tag = cancel_tag;
}
