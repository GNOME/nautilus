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
/* ntl-web-search.c: Rewrite KWebSearch using Gtk+ and Nautilus */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <limits.h>
#include <ctype.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>



typedef struct {
  char *name, *url_head, *url_tail;
} EngineInfo;

typedef struct {
  NautilusView *view;

  GtkCList *clist;

  GtkWidget *btn_search;
  GtkWidget *ent_params;
  GtkWidget *opt_engine;
  GtkWidget *menu_engine;

  EngineInfo *last_sel;

  BonoboUIHandler *uih;
} WebSearchView;

static int object_count = 0;

static void
do_search(GtkWidget *widget, WebSearchView *hview)
{
  char uri[PATH_MAX], real_query[PATH_MAX], *ctmp;
  int i, j;
  EngineInfo *ei;

  ctmp = gtk_entry_get_text(GTK_ENTRY(hview->ent_params));
  g_return_if_fail(ctmp);

  ei = hview->last_sel;
  for(i = j = 0; ctmp[i]; i++)
    {
      if(isalnum(ctmp[i]))
        real_query[j++] = ctmp[i];
      else
        {
          char dump[4];
          g_snprintf(dump, sizeof(dump), "%%%02x", (guint)ctmp[i]);
          real_query[j] = '\0';
          strcat(&real_query[j], dump);
          j += 3;
        }
    }
  real_query[j] = '\0';
  g_snprintf(uri, sizeof(uri), "%s%s%s", ei->url_head?ei->url_head:"", real_query, ei->url_tail?ei->url_tail:"");

  nautilus_view_open_location(hview->view, uri);
}

static void
do_destroy(GtkObject *obj)
{
  object_count--;
  if(object_count <= 0)
    gtk_main_quit();
}

static void
select_item(GtkObject *item, WebSearchView *hview)
{
  hview->last_sel = gtk_object_get_data(item, "EngineInfo");
}

static void
web_search_populate_engines(WebSearchView *hview)
{
  FILE *fh;
  char aline[LINE_MAX];

  fh = fopen(WEB_SEARCH_DATADIR "/standard.eng", "r");
  g_return_if_fail(fh);

  while(fgets(aline, sizeof(aline), fh))
    {
      EngineInfo *ei;
      char **pieces;
      GtkWidget *w;

      g_strstrip(aline);

      if(aline[0] == '#' || !aline[0])
        continue;

      pieces = g_strsplit(aline, "|", 3);

      if(!pieces || !pieces[0] || !pieces[1])
        {
          g_strfreev(pieces);
          continue;
        }

      ei = g_new0(EngineInfo, 1);
      ei->name = pieces[0];
      ei->url_head = pieces[1];
      ei->url_tail = pieces[2];
      g_free(pieces);

      w = gtk_menu_item_new_with_label(ei->name);
      gtk_signal_connect(GTK_OBJECT(w), "activate", select_item, hview);
      gtk_object_set_data(GTK_OBJECT(w), "EngineInfo", ei);

      gtk_container_add(GTK_CONTAINER(hview->menu_engine), w);
      gtk_widget_show(w);
      if(!hview->last_sel) {
        hview->last_sel = ei;
        gtk_option_menu_set_history(GTK_OPTION_MENU(hview->opt_engine), 0);
      }
    }
}

static BonoboObject *
make_obj(BonoboGenericFactory *Factory, const char *goad_id, gpointer closure)
{
  GtkWidget *vbox;
  WebSearchView *hview;

  g_return_val_if_fail(!strcmp(goad_id, "OAFIID:ntl_websearch_view:8216e1e4-6b01-4a28-82d9-5df30ed7d044"), NULL);

  hview = g_new0(WebSearchView, 1);


  vbox = gtk_vbox_new(FALSE, GNOME_PAD);

  hview->btn_search = gnome_pixmap_button(gnome_stock_pixmap_widget (vbox, GNOME_STOCK_PIXMAP_SEARCH), _("Search"));
  gtk_signal_connect(GTK_OBJECT(hview->btn_search), "clicked", do_search, hview);
  gtk_box_pack_start(GTK_BOX(vbox), hview->btn_search, FALSE, FALSE, GNOME_PAD);

  hview->opt_engine = gtk_option_menu_new();
  hview->menu_engine = gtk_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(hview->opt_engine), hview->menu_engine);
  gtk_container_add(GTK_CONTAINER(vbox), hview->opt_engine);

  web_search_populate_engines(hview);

  hview->ent_params = nautilus_entry_new();
  nautilus_undo_editable_set_undo_key ( GTK_EDITABLE (hview->ent_params), TRUE);
  
  gtk_signal_connect(GTK_OBJECT(hview->ent_params), "activate", do_search, hview);
  gtk_container_add(GTK_CONTAINER(vbox), hview->ent_params);

#if 0
  /* For now we just display the results as HTML */
  /* Results list */
  col_titles[0] = _("Results");
  clist = gtk_clist_new_with_titles(1, col_titles);
  gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
  gtk_clist_columns_autosize(GTK_CLIST(clist));
  wtmp = gtk_scrolled_window_new(gtk_clist_get_hadjustment(GTK_CLIST(clist)),
				 gtk_clist_get_vadjustment(GTK_CLIST(clist)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(vbox), wtmp);
  gtk_container_add(GTK_CONTAINER(wtmp), clist);
  gtk_signal_connect(GTK_OBJECT(clist), "select_row", web_search_select_row, hview);
#endif

  gtk_widget_show_all(vbox);

  hview->clist = NULL;

  
  /* create CORBA object */

  hview->view = nautilus_view_new (vbox);
  gtk_signal_connect(GTK_OBJECT (hview->view), "destroy", do_destroy, NULL);
  object_count++;

  /* handle selections */
  nautilus_clipboard_set_up_editable (GTK_EDITABLE (hview->ent_params),
                                       nautilus_view_get_bonobo_control (hview->view));
  
  return BONOBO_OBJECT (hview->view);
}

int main(int argc, char *argv[])
{
  BonoboGenericFactory *factory;
  CORBA_ORB orb;

  /* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);
#endif

  gnome_init_with_popt_table("ntl-web-search", VERSION, 
                             argc, argv,
                             oaf_popt_options, 0, NULL); 
  
  orb = oaf_init (argc, argv);

  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  factory = bonobo_generic_factory_new_multi("OAFIID:ntl_websearch_view_factory:c69658b2-732c-4a04-a493-4efe57051291", make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}
