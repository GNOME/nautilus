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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

#include "nautilus.h"

void
nautilus_window_set_initial_state(NautilusWindow *window, const char *initial_url)
{
  if(initial_url)
    nautilus_window_goto_uri(window, initial_url);
  else
    {
      GString* path_name;

      path_name = g_string_new("file://");
      g_string_append(path_name, g_get_home_dir());
      nautilus_window_goto_uri(window, path_name->str);
      g_string_free(path_name, TRUE);
    }
}

void
nautilus_window_load_state(NautilusWindow *window, const char *config_path)
{
  char *vtype;
  GSList *cur;
  int i, n;
  char cbuf[1024];

  /* Remove old stuff */
  for(cur = window->meta_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, NAUTILUS_VIEW(cur->data));
  nautilus_window_set_content_view(window, NULL);

  /* Load new stuff */
  gnome_config_push_prefix(config_path);

  vtype = gnome_config_get_string("content_view_type=NONE");

  if(vtype && strcmp(vtype, "NONE")) /* Create new content view */
    {
      GtkWidget *w;

      w = gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL);
      nautilus_window_set_content_view(window, NAUTILUS_VIEW(w));
      nautilus_view_load_client(NAUTILUS_VIEW(w), vtype);

      g_snprintf(cbuf, sizeof(cbuf), "%s/Content_View/", config_path);
      gnome_config_push_prefix(cbuf);
      nautilus_view_load_state(NAUTILUS_VIEW(w), cbuf);
      gnome_config_pop_prefix();

      gtk_widget_show(w);
    }
  g_free(vtype);

  n = gnome_config_get_int("num_meta_views=0");
  for(i = 0; i < n; n++)
    {
      GtkWidget *nvw;

      g_snprintf(cbuf, sizeof(cbuf), "%s/meta_view_%d_type=0", config_path, i);
      vtype = gnome_config_get_string(cbuf);

      nvw = gtk_widget_new(nautilus_meta_view_get_type(), "main_window", window, NULL);
      nautilus_view_load_client(NAUTILUS_VIEW(nvw), vtype);

      g_snprintf(cbuf, sizeof(cbuf), "%s/Meta_View_%d/", config_path, i);

      gnome_config_push_prefix(cbuf);
      nautilus_view_load_state(NAUTILUS_VIEW(cur->data), cbuf);
      gnome_config_pop_prefix();

      nautilus_window_add_meta_view(window, NAUTILUS_VIEW(nvw));
    }

  gnome_config_pop_prefix();
}


void
nautilus_window_save_state(NautilusWindow *window, const char *config_path)
{
  GSList *cur;
  int n;
  char cbuf[1024];

  gnome_config_push_prefix(config_path);
  if(window->content_view)
    {
      gnome_config_set_string("content_view_type", NAUTILUS_VIEW(window->content_view)->iid);
      g_snprintf(cbuf, sizeof(cbuf), "%s/Content_View/", config_path);

      gnome_config_push_prefix(cbuf);

      nautilus_view_save_state(window->content_view, cbuf);

      gnome_config_pop_prefix();
    }
  else
    gnome_config_set_string("content_view_type", "NONE");


  n = g_slist_length(window->meta_views);

  for(n = 0, cur = window->meta_views; cur; cur = cur->next, n++)
    {
      if(!NAUTILUS_VIEW(cur->data)->iid)
	{
	  continue;
	  n--;
	}

      g_snprintf(cbuf, sizeof(cbuf), "meta_view_%d_type=0", n);

      gnome_config_set_string(cbuf, NAUTILUS_VIEW(cur->data)->iid);

      g_snprintf(cbuf, sizeof(cbuf), "%s/Meta_View_%d/", config_path, n);

      gnome_config_push_prefix(cbuf);

      nautilus_view_save_state(NAUTILUS_VIEW(cur->data), cbuf);

      gnome_config_pop_prefix();
    }
  gnome_config_set_int("num_meta_views", n);

  gnome_config_pop_prefix();
}
