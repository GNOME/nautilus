#include "config.h"

#include <libnautilus/libnautilus.h>

typedef struct {
  NautilusViewClient *view;

  GtkCList *clist;

  gint notify_count, last_row;
} HistoryView;

static void
hyperbola_navigation_history_notify_location_change (NautilusViewClient *view,
						     Nautilus_NavigationInfo *loci,
						     HistoryView *hview)
{
  char *cols[2];
  int new_rownum;
  GtkCList *clist;

  hview->notify_count++;

  clist = hview->clist;

  if(hview->last_row >= 0)
    {
      char *uri;
      int i, j;

      /* If we are moving 'forward' in history, must either just
	 select a new row that is farther ahead in history, or delete
	 all the history ahead of this point */

      for(i = -1; i <= 1; i++)
	{
	  if((hview->last_row + i) < 0)
	    continue;

	  gtk_clist_get_text(clist, hview->last_row + i, 0, &uri);
	  if(!strcmp(uri, loci->requested_uri))
	    {
	      hview->last_row = new_rownum = hview->last_row + i;
	      goto skip_prepend;
	    }
	}

      for(j = 0; j < hview->last_row; j++)
	gtk_clist_remove(clist, 0);
    }

  gtk_clist_freeze(clist);
  cols[0] = (char *)loci->requested_uri;
  hview->last_row = new_rownum = gtk_clist_prepend(clist, cols);

 skip_prepend:
  gtk_clist_columns_autosize(clist);

  if(gtk_clist_row_is_visible(clist, new_rownum) != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(clist, new_rownum, -1, 0.5, 0.0);

  gtk_clist_select_row(clist, new_rownum, 0);

  gtk_clist_thaw(clist);

  hview->notify_count--;
}

static void
hyperbola_navigation_history_select_row(GtkCList *clist, gint row, gint column, GdkEvent *event,
					HistoryView *hview)
{
  Nautilus_NavigationRequestInfo reqi;

  if(hview->notify_count > 0)
    return;

  gtk_clist_freeze(clist);

  if(hview->last_row == row)
    return;

  hview->last_row = row;

  if(gtk_clist_row_is_visible(clist, row) != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(clist, row, -1, 0.5, 0.0);

  gtk_clist_get_text(clist, row, 0, &reqi.requested_uri);

  reqi.new_window_default = reqi.new_window_suggested = Nautilus_V_FALSE;
  reqi.new_window_enforced = Nautilus_V_FALSE;

  nautilus_view_client_request_location_change(hview->view, &reqi);

  gtk_clist_thaw(clist);
}

static int object_count = 0;

static void
do_destroy(GtkObject *obj)
{
  object_count--;
  if(object_count <= 0)
    gtk_main_quit();
}

static GnomeObject * make_obj(GnomeGenericFactory *Factory, const char *goad_id, gpointer closure)
{
  GtkWidget *client, *clist, *wtmp;
  GnomeObject *ctl;
  char *col_titles[1];
  HistoryView *hview;

  g_return_val_if_fail(!strcmp(goad_id, "ntl_history_view"), NULL);

  hview = g_new0(HistoryView, 1);
  hview->last_row = -1;
  client = gtk_widget_new(nautilus_meta_view_client_get_type(), NULL);
  gtk_signal_connect(GTK_OBJECT(client), "destroy", do_destroy, NULL);
  object_count++;

  ctl = nautilus_view_client_get_gnome_object(NAUTILUS_VIEW_CLIENT(client));

  /* create interface */
  col_titles[0] = _("Path");
  clist = gtk_clist_new_with_titles(1, col_titles);
  gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
  gtk_clist_columns_autosize(GTK_CLIST(clist));
  wtmp = gtk_scrolled_window_new(gtk_clist_get_hadjustment(GTK_CLIST(clist)),
				 gtk_clist_get_vadjustment(GTK_CLIST(clist)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(client), wtmp);
  gtk_container_add(GTK_CONTAINER(wtmp), clist);

  gtk_widget_show_all(client);
  
  /* handle events */
  gtk_signal_connect(GTK_OBJECT(client), "notify_location_change", hyperbola_navigation_history_notify_location_change, hview);
  gtk_signal_connect(GTK_OBJECT(clist), "select_row", hyperbola_navigation_history_select_row, hview);

  /* set description */
  nautilus_meta_view_client_set_label(NAUTILUS_META_VIEW_CLIENT(client),
				      _("History"));

  hview->view = (NautilusViewClient *)client;
  hview->clist = (GtkCList *)clist;

  return ctl;
}

int main(int argc, char *argv[])
{
  GnomeGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table("ntl-history-view", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  factory = gnome_generic_factory_new_multi("ntl_history_view_factory", make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}
