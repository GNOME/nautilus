/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-view.c: Implementation of the object representing a data view,
   and its associated CORBA object for proxying requests into this
   object. */
#include "config.h"

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnorba/gnorba.h>

typedef struct {
  NautilusViewFrame *view;

  GtkCList *clist;

  gint notify_count;

  BonoboUIHandler *uih;
} HistoryView;

#define HISTORY_VIEW_COLUMN_ICON	0
#define HISTORY_VIEW_COLUMN_NAME	1
#define HISTORY_VIEW_COLUMN_COUNT	2

static const NautilusBookmark *
get_bookmark_from_row (GtkCList *clist, int row)
{
  g_assert (NAUTILUS_IS_BOOKMARK (gtk_clist_get_row_data (clist, row)));
  return NAUTILUS_BOOKMARK (gtk_clist_get_row_data (clist, row));  
}

static const char *
get_uri_from_row (GtkCList *clist, int row)
{
  return nautilus_bookmark_get_uri (get_bookmark_from_row (clist, row));
}


static void
install_icon (GtkCList *clist, gint row)
{
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	const NautilusBookmark *bookmark;

	bookmark = get_bookmark_from_row (clist, row);
	if (!nautilus_bookmark_get_pixmap_and_mask (bookmark,
		  				    NAUTILUS_ICON_SIZE_SMALLER,
						    &pixmap,
						    &bitmap))
	{
		return;
	}

	gtk_clist_set_pixmap (clist,	
			      row,
			      HISTORY_VIEW_COLUMN_ICON,
			      pixmap,
			      bitmap);
}

static void
history_view_update_icons (GtkCList *clist)
{
  int row;

  for (row = 0; row < clist->rows; ++row) 
    {
      install_icon (clist, row);
    }
}

static void
hyperbola_navigation_history_notify_location_change (NautilusViewFrame *view,
						     Nautilus_NavigationInfo *loci,
						     HistoryView *hview)
{
  char *cols[HISTORY_VIEW_COLUMN_COUNT];
  int new_rownum;
  GtkCList *clist;
  NautilusBookmark *bookmark;
  int i;

  hview->notify_count++;

  clist = hview->clist;
  gtk_clist_freeze(clist);

  bookmark = nautilus_bookmark_new (loci->requested_uri);

  /* If a bookmark for this location was already in list, remove it
   * (no duplicates in list, new one goes at top)
   */
  for (i = 0; i < clist->rows; ++i)
    {
      if (nautilus_bookmark_compare_with (get_bookmark_from_row (clist, i), 
      					  bookmark)
      		== 0)
        {
          gtk_clist_remove (clist, i);
          /* Since we check with each insertion, no need to check further */
          break;
        }
    }

  cols[HISTORY_VIEW_COLUMN_ICON] = NULL;
  /* Ugh. Gotta cast away the const */
  cols[HISTORY_VIEW_COLUMN_NAME] = (char *)nautilus_bookmark_get_name (bookmark);
  new_rownum = gtk_clist_prepend(clist, cols);
  gtk_clist_set_row_data_full (clist,
  			       new_rownum,
  			       bookmark,
  			       (GtkDestroyNotify)gtk_object_unref);
  install_icon (clist, new_rownum);

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

  /* First row is always current location, by definition, so don't activate */
  if (row == 0)
      return;

  /* FIXME: There are bugs here if you drag up & down */

  gtk_clist_freeze(clist);

  if(gtk_clist_row_is_visible(clist, row) != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(clist, row, -1, 0.5, 0.0);

  /* FIXME: gotta cast away const because requested_uri isn't defined correctly */
  reqi.requested_uri = (char *)get_uri_from_row (clist, row);
  reqi.new_window_requested = FALSE;

  nautilus_view_frame_request_location_change(hview->view, &reqi);

  gtk_clist_thaw(clist);
}

static int object_count = 0;

static void
do_destroy(GtkObject *obj, HistoryView *hview)
{
  object_count--;
  if(object_count <= 0)
    gtk_main_quit();
}

static BonoboObject *
make_obj(BonoboGenericFactory *Factory, const char *goad_id, gpointer closure)
{
  GtkWidget *wtmp;
  GtkCList *clist;
  HistoryView *hview;

  g_return_val_if_fail(!strcmp(goad_id, "ntl_history_view"), NULL);

  hview = g_new0(HistoryView, 1);

  /* create interface */
  clist = GTK_CLIST (gtk_clist_new (HISTORY_VIEW_COLUMN_COUNT));
  gtk_clist_column_titles_hide (clist);
  gtk_clist_set_row_height (clist, NAUTILUS_ICON_SIZE_SMALLER);
  gtk_clist_set_selection_mode(clist, GTK_SELECTION_BROWSE);
  gtk_clist_columns_autosize(clist);
  wtmp = gtk_scrolled_window_new(gtk_clist_get_hadjustment(clist),
				 gtk_clist_get_vadjustment(clist));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(wtmp), GTK_WIDGET (clist));

  gtk_widget_show_all(wtmp);

  /* create object */
  hview->view = NAUTILUS_VIEW_FRAME (nautilus_meta_view_frame_new (wtmp));
  gtk_signal_connect (GTK_OBJECT (hview->view), "destroy", do_destroy, hview);
  object_count++;

  hview->clist = (GtkCList *)clist;

  /* set description */
  nautilus_meta_view_frame_set_label(NAUTILUS_META_VIEW_FRAME(hview->view),
                                     _("History"));

  /* handle events */
  gtk_signal_connect(GTK_OBJECT(hview->view), "notify_location_change", hyperbola_navigation_history_notify_location_change, hview);
  gtk_signal_connect(GTK_OBJECT(clist), "select_row", hyperbola_navigation_history_select_row, hview);

  gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					 "icons_changed",
					 history_view_update_icons,
					 GTK_OBJECT (hview->clist));


  return BONOBO_OBJECT (hview->view);
}

int main(int argc, char *argv[])
{
  BonoboGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table("ntl-history-view", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
  gnome_vfs_init ();

  factory = bonobo_generic_factory_new_multi("ntl_history_view_factory", make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}
