/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-emblem-utils.h: Utilities for handling emblems
 
   Copyright (C) 2002 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtkwindow.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

GList *    nautilus_emblem_list_available             (void);
void       nautilus_emblem_refresh_list               (void);
gboolean   nautilus_emblem_should_show_in_list        (const char *emblem);
gboolean   nautilus_emblem_verify_keyword             (GtkWindow *parent_window,
						       const char *keyword,
						       const char *display_name);
void       nautilus_emblem_install_custom_emblem      (GdkPixbuf  *pixbuf,
						       const char *keyword,
						       const char *display_name,
						       GtkWindow  *parent_window);

gboolean   nautilus_emblem_remove_emblem              (const char *keyword);
gboolean   nautilus_emblem_rename_emblem              (const char *keyword,
						       const char *display_name);

GdkPixbuf *nautilus_emblem_load_pixbuf_for_emblem     (GFile      *emblem);
char *     nautilus_emblem_get_keyword_from_icon_name (const char *emblem);
char *     nautilus_emblem_get_icon_name_from_keyword (const char *keyword);

gboolean   nautilus_emblem_can_remove_emblem          (const char *keyword);
gboolean   nautilus_emblem_can_rename_emblem          (const char *keyword);

char *     nautilus_emblem_create_unique_keyword      (const char *base);

