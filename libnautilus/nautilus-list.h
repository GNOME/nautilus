/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-list.h: Enhanced version of GtkCList for Nautilus.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Federico Mena <federico@nuclecu.unam.mx>,
            Ettore Perazzoli <ettore@gnu.org>,
            John Sullivan <sullivan@eazel.com>,
	    Pavel Cisler <pavel@eazel.com>
 */

#ifndef NAUTILUS_LIST_H
#define NAUTILUS_LIST_H

#include <gtk/gtkclist.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* This class was originally derived from the GtkFList class in gmc.
 */

/* It is sad that we have to do this. GtkCList's behavior is so broken that we
 * have to override all the event handlers and implement our own selection
 * behavior. Sigh. -Federico
 */

/* Superset of GtkCellType enum defined in gtk-clist.h */
typedef enum
{
  NAUTILUS_CELL_EMPTY,	 	/* GTK_CELL_EMPTY */
  NAUTILUS_CELL_TEXT,	 	/* GTK_CELL_TEXT */
  NAUTILUS_CELL_PIXMAP,	 	/* GTK_CELL_PIXMAP */
  NAUTILUS_CELL_PIXTEXT, 	/* GTK_CELL_PIXTEXT */
  NAUTILUS_CELL_WIDGET,	 	/* GTK_CELL_WIDGET */
  NAUTILUS_CELL_PIXBUF_LIST   	/* new for Nautilus */
} NautilusCellType;

/* pointer casting for cells */
#define NAUTILUS_CELL_PIXBUF_LIST(cell)	(((NautilusCellPixbufList *) &(cell)))

typedef struct _NautilusCellPixbufList NautilusCellPixbufList;

/*
 * Since the info in each cell must fit in the GtkCell struct that CList defines,
 * we disguise ours in the GtkCellWidget format, with our pixbufs pointer where
 * the widget would be.
 */
struct _NautilusCellPixbufList
{
  NautilusCellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  GList *pixbufs; /* list of GdkPixbuf * */
};

#define NAUTILUS_TYPE_LIST            (nautilus_list_get_type ())
#define NAUTILUS_LIST(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LIST, NautilusList))
#define NAUTILUS_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LIST, NautilusListClass))
#define NAUTILUS_IS_LIST(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LIST))
#define NAUTILUS_IS_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LIST))

typedef struct NautilusList NautilusList;
typedef struct NautilusListClass NautilusListClass;
typedef struct NautilusListDetails NautilusListDetails;

struct NautilusList {
	GtkCList clist;
	NautilusListDetails *details;
};

struct NautilusListClass {
	GtkCListClass parent_class;

	/* Signal: invoke the popup menu for selected items */
	void (* context_click_selection) (NautilusList *list, int row);

	/* Signal: invoke the popup menu for empty areas */
	void (* context_click_background) (NautilusList *list);

	/* Signal: open the file in the selected row */
	void (* activate) (NautilusList *list, gpointer data);

	/* Signal: initiate a drag and drop operation */
	void (* start_drag) (NautilusList *list, int button, GdkEvent *event);

	/* Signal: selection has changed */
	void (* selection_changed) (NautilusList *list);

	/* column resize tracking calls */
	void (* column_resize_track_start) (GtkWidget *widget, int column);
	void (* column_resize_track) (GtkWidget *widget, int column);
	void (* column_resize_track_end) (GtkWidget *widget, int column);
};

GtkType    nautilus_list_get_type        (void);
GtkWidget *nautilus_list_new_with_titles (int                 columns,
					  const char * const *titles);
GList *    nautilus_list_get_selection   (NautilusList       *list);
gboolean   nautilus_list_is_row_selected (NautilusList	     *list,
					  int		      row);

void 	   nautilus_list_set_pixbuf_list (NautilusList       *list,
				      	  gint	   	      row,
				      	  gint		      column,
				      	  GList	     	     *pixbufs);
#endif /* NAUTILUS_LIST_H */
