#include <libnautilus/libnautilus.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <string.h>

typedef struct {
  NautilusViewFrame *view_frame;

  GtkWidget *clist, *ent;

  GArray *items;

  gint8 notify_count;
} HyperbolaNavigationIndex;

typedef struct {
  char *text, *uri;
  guint8 indent;
  gboolean shown : 1;
} IndexItem;

static void
hyperbola_navigation_index_update_clist(HyperbolaNavigationIndex *hni)
{
  char *stxt, *tmp_stxt;
  char *words[100];
  int nwords;
  char *ctmp = NULL;
  int tmp_len;
  int i;

  stxt = gtk_entry_get_text(GTK_ENTRY(hni->ent));

  tmp_len = strlen(stxt)+1;
  tmp_stxt = alloca(tmp_len);
  memcpy(tmp_stxt, stxt, tmp_len);
  for(nwords = 0; (ctmp = strtok(tmp_stxt, ", \t")) && nwords < sizeof(words)/sizeof(words[0]); nwords++)
    words[nwords] = ctmp;

  gtk_clist_freeze(GTK_CLIST(hni->clist));
  gtk_clist_clear(GTK_CLIST(hni->clist));

  for(i = 0; i < hni->items->len; i++)
    {
      int j, rownum, uplevel;
      char rowtext[512];
      IndexItem *ii = &g_array_index(hni->items, IndexItem, i);

      for(j = 0; j < nwords; j++)
	{
	  if(strstr(ii->text, words[i]))
	    break;
	}

      ii->shown = !(nwords && j >= nwords);

      if(!ii->shown)
	continue;

      j = i;
      for(uplevel = ii->indent - 1; uplevel >= 0; uplevel--)
	{
	  IndexItem *previi;

	  for(; j >= 0; j--)
	    {
	      previi = &g_array_index(hni->items, IndexItem, j);
	      if(previi->indent == uplevel)
		break;
	    }

	  if(j < 0)
	    break;

	  if(!previi->shown)
	    {
	      /* Figure out the right place to insert the row */
	    }
	}

      g_snprintf(rowtext, sizeof(rowtext), "%*s%s", ii->indent * 2, "", ii->text); /* Lame way of indenting entries */
      rownum = gtk_clist_append(GTK_CLIST(hni->clist), (char **)&rowtext);
      gtk_clist_set_row_data(GTK_CLIST(hni->clist), rownum, ii);

      if(nwords) /* highlight this row as a match */
	{
	  GdkColor c;

	  c.red = c.green = 65535;
	  c.blue = 20000;
	  gdk_color_alloc(gdk_rgb_get_cmap(), &c);
	  gtk_clist_set_background(GTK_CLIST(hni->clist), rownum, &c);
	}
    }

  gtk_clist_thaw(GTK_CLIST(hni->clist));
}

static void
hyperbola_navigation_index_ent_changed(GtkWidget *ent, HyperbolaNavigationIndex *hni)
{
  hyperbola_navigation_index_update_clist(hni);
}

static void
hyperbola_navigation_index_ent_activate(GtkWidget *ent, HyperbolaNavigationIndex *hni)
{
}

static void
hyperbola_navigation_index_select_row(GtkWidget *clist, gint row, gint column, GdkEvent *event, HyperbolaNavigationIndex *hni)
{
  IndexItem *ii;
  Nautilus_NavigationRequestInfo loc;

  if(!event || event->type != GDK_2BUTTON_PRESS) /* we only care if the user has double-clicked on an item...? */
    return;

  ii = gtk_clist_get_row_data(GTK_CLIST(clist), row);
  memset(&loc, 0, sizeof(loc));
  loc.requested_uri = ii->uri;
  loc.new_window_default = loc.new_window_suggested = loc.new_window_enforced = Nautilus_V_UNKNOWN;
  nautilus_view_frame_request_location_change(NAUTILUS_VIEW_FRAME(hni->view_frame), &loc);
}

BonoboObject *hyperbola_navigation_index_new(void)
{
  HyperbolaNavigationIndex *hni;
  GtkWidget *wtmp;

  hni = g_new0(HyperbolaNavigationIndex, 1);
  hni->items = g_array_new(FALSE, FALSE, sizeof(IndexItem));

  hni->ent = gtk_entry_new();
  gtk_signal_connect(GTK_OBJECT(hni->ent), "changed", hyperbola_navigation_index_ent_changed, hni);
  gtk_signal_connect(GTK_OBJECT(hni->ent), "activate", hyperbola_navigation_index_ent_activate, hni);
  gtk_widget_show(hni->ent);

  hni->clist = gtk_clist_new(1);
  gtk_clist_freeze(GTK_CLIST(hni->clist));
  gtk_clist_set_selection_mode(GTK_CLIST(hni->clist), GTK_SELECTION_BROWSE);
  gtk_clist_set_column_widget(GTK_CLIST(hni->clist), 0, hni->ent);
  gtk_signal_connect(GTK_OBJECT(hni->clist), "select_row", hyperbola_navigation_index_select_row, hni);

  wtmp = gtk_scrolled_window_new(gtk_clist_get_hadjustment(GTK_CLIST(hni->clist)),
				 gtk_clist_get_vadjustment(GTK_CLIST(hni->clist)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(wtmp), hni->clist);
  hyperbola_navigation_index_update_clist(hni);
  gtk_clist_columns_autosize(GTK_CLIST(hni->clist));
  gtk_clist_thaw(GTK_CLIST(hni->clist));
  gtk_widget_show(hni->clist);
  gtk_widget_show(wtmp);

  hni->view_frame = NAUTILUS_VIEW_FRAME (nautilus_meta_view_frame_new (wtmp));
  nautilus_meta_view_frame_set_label(NAUTILUS_META_VIEW_FRAME(hni->view_frame), _("Help Index"));

  return BONOBO_OBJECT (hni->view_frame);
}
