#include "nautilus.h"
#include "ntl-window-private.h"
#include "explorer-location-bar.h"

static void nautilus_window_notify_selection_change(NautilusWindow *window,
						    NautilusView *view,
						    Nautilus_SelectionInfo *loc,
						    guint signum,
						    NautilusView *requesting_view);

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
                                      GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[2], loc, requesting_view);
}

void
nautilus_window_request_selection_change(NautilusWindow *window,
					 Nautilus_SelectionRequestInfo *loc,
					 GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[1], loc, requesting_view);
}

void
nautilus_window_request_location_change(NautilusWindow *window,
					Nautilus_NavigationRequestInfo *loc,
					GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[0], loc, requesting_view);
}

static void
nautilus_window_change_location_internal(NautilusWindow *window, NautilusNavigationInfo *loci, gboolean is_back)
{
  CORBA_free(window->si); window->si = NULL;

  if(is_back)
    {
      window->uris_next = g_slist_prepend(window->uris_next, window->uris_prev->data);
      window->uris_prev = g_slist_remove(window->uris_prev, window->uris_prev->data);
    }
  else
    {
      char *append_val;
      if(window->uris_next && !strcmp(loci->navinfo.requested_uri, window->uris_next->data))
	{
	  append_val = window->uris_next->data;
	  window->uris_next = g_slist_remove(window->uris_next, window->uris_next->data);
	}
      else
	{
	  append_val = g_strdup(loci->navinfo.requested_uri);
	  g_slist_foreach(window->uris_next, (GFunc)g_free, NULL);
	  g_slist_free(window->uris_next); window->uris_next = NULL;
	}
      if(append_val)
        window->uris_prev = g_slist_prepend(window->uris_prev, append_val);
    }

  gtk_widget_set_sensitive(window->btn_back, window->uris_prev?TRUE:FALSE);
  gtk_widget_set_sensitive(window->btn_fwd, window->uris_next?TRUE:FALSE);

  CORBA_free(window->ni);
  window->ni = Nautilus_NavigationInfo__alloc();
  Nautilus_NavigationInfo__copy(window->ni, &loci->navinfo);

  CORBA_free(window->si);
  window->si = NULL;

  explorer_location_bar_set_uri_string(EXPLORER_LOCATION_BAR(window->ent_url),
                                       loci->navinfo.requested_uri);
}

static void
nautilus_window_update_view(NautilusWindow *window,
                            NautilusView *view,
                            NautilusNavigationInfo *loci,
                            guint signum_location,
                            NautilusView *requesting_view)
{
  loci->navinfo.self_originated = (view == requesting_view);

  if(!signum_location)
    signum_location = gtk_signal_lookup("notify_location_change", nautilus_view_get_type());

  gtk_signal_emit(GTK_OBJECT(view), signum_location, loci);

  if(window->si)
    {
      window->si->content_view = nautilus_view_get_client_objref(NAUTILUS_VIEW(window->content_view));
      nautilus_window_notify_selection_change(window, view, window->si, 0, NULL);
    }
}

static void
nautilus_window_load_content_view(NautilusWindow *window,
                                  const char *iid,
                                  NautilusNavigationInfo *loci,
                                  guint signum_location,
                                  NautilusView **requesting_view)
{
  NautilusView *content_view = window->content_view;
  NautilusView *new_view;

  if(!content_view || strcmp(nautilus_view_get_iid(content_view), iid))
    {
      if(*requesting_view == window->content_view) /* If we are going to be zapping the old view,
                                                      we definitely don't want any of the new views
                                                      thinking they made the request */
        *requesting_view = NULL;

      new_view = NAUTILUS_VIEW(gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL));
      nautilus_view_load_client(new_view, iid);
      nautilus_window_set_content_view(window, new_view);
    }

  loci->navinfo.content_view = nautilus_view_get_client_objref(window->content_view);

  nautilus_window_update_view(window, window->content_view, loci, signum_location, *requesting_view);
}

static void
nautilus_window_load_meta_view(NautilusWindow *window,
                               const char *iid,
                               NautilusNavigationInfo *loci,
                               guint signum_location,
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
      meta_view = NAUTILUS_VIEW(gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL));
      nautilus_view_load_client(meta_view, iid);
      nautilus_window_add_meta_view(window, meta_view);
    }

  nautilus_window_update_view(window, meta_view, loci, 0, requesting_view);
}

/* This is the most complicated routine in Nautilus. Steps include:
   1. Get the information needed to load the URL.
   2. If a load is not possible, fail.
   3. Update the history, UI, and internal state for the proposed change.
   4. Load/notify content view.
   5. Load/notify meta views.
 
 */
void
nautilus_window_change_location(NautilusWindow *window,
				Nautilus_NavigationRequestInfo *loc,
				NautilusView *requesting_view,
				gboolean is_back)
{
  guint signum;
  NautilusNavigationInfo loci_spot, *loci;
  GSList *cur, *discard_views;

  /* Step 1 */
  loci = nautilus_navinfo_new(&loci_spot, loc, window->ni, requesting_view);

  /* Step 2 */
  if(!loci)
    {
      char cbuf[1024];
      g_snprintf(cbuf, sizeof(cbuf), _("Cannot load %s"), loc->requested_uri);
      nautilus_window_set_status(window, cbuf);
      return;
    }

  /* Step 3 */
  nautilus_window_change_location_internal(window, loci, is_back);

  /* Step 4 */
  signum = gtk_signal_lookup("notify_location_change", nautilus_view_get_type());

  nautilus_window_load_content_view(window, loci->content_iid, loci, signum, &requesting_view);

  /* Step 5 */

  /* Figure out which meta views are going to go away */
  discard_views = NULL;
  for(cur = window->meta_views; cur; cur = cur->next)
    {
      NautilusView *view = cur->data;

      if(!g_slist_find_custom(loci->meta_iids, view->iid, (GCompareFunc)strcmp))
	{
	  if(view == requesting_view)
	    requesting_view = NULL;

	  discard_views = g_slist_prepend(discard_views, view);
	}
    }

  for(cur = loci->meta_iids; cur; cur = cur->next)
    nautilus_window_load_meta_view(window, cur->data, loci, signum, requesting_view);

  for(cur = discard_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, cur->data);
  g_slist_free(discard_views);

  nautilus_navinfo_free(loci);
}
  
static void
nautilus_window_notify_selection_change(NautilusWindow *window,
					NautilusView *view,
					Nautilus_SelectionInfo *loc,
					guint signum,
					NautilusView *requesting_view)
{
  loc->self_originated = (view == requesting_view);

  if(!signum)
    signum = gtk_signal_lookup("notify_selection_change", nautilus_view_get_type());

  gtk_signal_emit(GTK_OBJECT(view), signum, loc);
}

void
nautilus_window_real_request_selection_change(NautilusWindow *window,
					      Nautilus_SelectionRequestInfo *loc,
					      NautilusView *requesting_view)
{
  GSList *cur;
  guint signum;
  Nautilus_SelectionInfo selinfo;

  signum = gtk_signal_lookup("notify_selection_change", nautilus_view_get_type());

  selinfo.selected_uris = loc->selected_uris;
  selinfo.content_view = NAUTILUS_VIEW(window->content_view)->view_client;

  CORBA_free(window->si);

  window->si = Nautilus_SelectionInfo__alloc();
  Nautilus_SelectionInfo__copy(window->si, &selinfo);

  signum = gtk_signal_lookup("notify_selection_change", nautilus_view_get_type());
  nautilus_window_notify_selection_change(window, window->content_view, &selinfo, signum, requesting_view);

  for(cur = window->meta_views; cur; cur = cur->next)
    nautilus_window_notify_selection_change(window, cur->data, &selinfo, signum, requesting_view);
}

void
nautilus_window_real_request_status_change(NautilusWindow *window,
					   Nautilus_StatusRequestInfo *loc,
					   NautilusView *requesting_view)
{
  nautilus_window_set_status(window, loc->status_string);
}

void
nautilus_window_real_request_location_change (NautilusWindow *window,
					      Nautilus_NavigationRequestInfo *loc,
					      NautilusView *requesting_view)
{
  nautilus_window_change_location(window, loc, requesting_view, FALSE);
}
