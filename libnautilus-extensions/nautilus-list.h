/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-list.h: Enhanced version of GtkCList for Nautilus.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.

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

#include <libgnome/gnome-defs.h>
#include <cut-n-paste-code/widgets/nautilusclist/nautilusclist.h>

/* This class was originally derived from the GtkFList class in gmc.
 */

/* It is sad that we have to do this. GtkCList's behavior is so broken that we
 * have to override all the event handlers and implement our own selection
 * behavior. Sigh. -Federico
 */

/* pointer casting for cells */
#define NAUTILUS_CELL_PIXBUF_LIST(cell)	((NautilusCellPixbufList *) &(cell))
/* no #define for NAUTILUS_CELL_LINK_TEXT, use GTK_CELL_TEXT instead */

/* returns the GList item for the nth row */
#define	ROW_ELEMENT(clist, row)	(((row) == (clist)->rows - 1) ? \
				 (clist)->row_list_end : \
				 g_list_nth ((clist)->row_list, (row)))

typedef struct NautilusCellPixbufList NautilusCellPixbufList;
/* no struct for NautilusCellLinkText, use GtkCellText instead */

/* Since the info in each cell must fit in the GtkCell struct that CList defines,
 * we disguise ours in the GtkCellWidget format, with our pixbufs list where
 * the widget would be.
 */
struct NautilusCellPixbufList
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
	NautilusCList clist;
	NautilusListDetails *details;
};

struct NautilusListClass {
	NautilusCListClass parent_class;

	/* Signal: invoke the popup menu for selected items */
	void (* context_click_selection) (NautilusList *list, 
					  GdkEventButton *event);

	/* Signal: invoke the popup menu for empty areas */
	void (* context_click_background) (NautilusList *list,
					   GdkEventButton *event);

	/* Signal: announce that one or more items have been activated. */
	void (* activate) (NautilusList *list, GList *row_data_list);

	/* Signal: selection has changed */
	void (* selection_changed) (NautilusList *list);

	/* column resize tracking calls */
	void      (* column_resize_track_start) (GtkWidget *widget, int column);
	void      (* column_resize_track)       (GtkWidget *widget, int column);
	void      (* column_resize_track_end)   (GtkWidget *widget, int column);
	void      (* select_matching_name)      (GtkWidget *widget, const char *);
	void      (* select_previous_name)      (GtkWidget *widget);
	void      (* select_next_name)          (GtkWidget *widget);
	GdkPixbuf (* get_drag_pixbuf)           (NautilusList *list, int row_index);
	int       (* get_sort_column_index)     (NautilusList *list);

	/* dnd handling. defer the semantics of dnd to the application side, not nautilus-list */
	gboolean  (* handle_dragged_items)      (GtkWidget     *widget,
						 int            action,
						 GList         *drop_data,
						 int            x,
						 int            y,
						 guint          info);
	void      (* handle_dropped_items)      (GtkWidget     *widget,
						 int            action,
						 GList         *drop_data,
						 int            x,
						 int            y,
						 guint          info);
	void      (* get_default_action)        (GtkWidget     *widget,
						 int           *default_action,
						 int           *non_default_action,
						 GdkDragContext *context,
						 GList         *drop_data,
						 int            x,
						 int            y,
						 guint          info);

};

typedef gboolean (* NautilusEachRowFunction) (NautilusCListRow *, int, gpointer);

GtkType      nautilus_list_get_type                  (void);
GtkWidget *  nautilus_list_new_with_titles           (int                      columns,
						      const char * const      *titles);
void         nautilus_list_initialize_dnd            (NautilusList            *list);
GList *      nautilus_list_get_selection             (NautilusList            *list);
void         nautilus_list_set_selection             (NautilusList            *list,
						      GList                   *selection);
void	     nautilus_list_reveal_row		     (NautilusList	      *list,
						      int		       row);
gboolean     nautilus_list_is_row_selected           (NautilusList            *list,
						      int                      row);
void	     nautilus_list_get_cell_rectangle	     (NautilusList 	      *clist,
						      int		       row_index,
						      int 		       column_index,
						      GdkRectangle 	      *result);
void         nautilus_list_set_pixbuf_list           (NautilusList            *list,
						      gint                     row,
						      gint                     column,
						      GList                   *pixbufs);
void	     nautilus_list_set_pixbuf		     (NautilusList	      *list,
						      int		       row_index,
						      int		       column_index,
						      GdkPixbuf		      *pixbuf);
GdkPixbuf   *nautilus_list_get_pixbuf		     (NautilusList	      *list,
						      int		       row_index,
						      int		       column_index);
void         nautilus_list_mark_cell_as_link         (NautilusList            *list,
						      gint                     row,
						      gint                     column);
void         nautilus_list_set_single_click_mode     (NautilusList            *list,
						      gboolean                 single_click_mode);
void         nautilus_list_select_row                (NautilusList            *list,
						      int                      row);
NautilusCListRow *nautilus_list_row_at               (NautilusList            *list,
						      int                      y);
int	     nautilus_list_get_first_selected_row    (NautilusList	      *list);
int	     nautilus_list_get_last_selected_row     (NautilusList	      *list);
void         nautilus_list_each_selected_row         (NautilusList            *list,
						      NautilusEachRowFunction  function,
						      gpointer                 data);
gboolean     nautilus_list_rejects_dropped_icons     (NautilusList	      *list);
void	     nautilus_list_set_rejects_dropped_icons (NautilusList	      *list,
						      gboolean		       new_value);
void 	     nautilus_list_set_drag_prelight_row     (NautilusList	      *list,
						      int		       y);
void 	     nautilus_list_get_initial_drag_offset   (NautilusList	      *list,
						      int		      *x,
						      int		      *y);

void	     nautilus_list_set_anti_aliased_mode     (NautilusList	      *list,
						      gboolean		       anti_aliased_mode);
gboolean     nautilus_list_is_anti_aliased	     (NautilusList	      *list);

int 	     nautilus_list_draw_cell_pixbuf	     (NautilusList	      *list,
						      GdkWindow		      *window,
						      GdkRectangle	      *clip_rectangle,
						      GdkGC		      *fg_gc,
						      guint32		       bg_rgb,
						      GdkPixbuf		      *pixbuf,
						      int		       x,
						      int		       y);
void	     nautilus_list_get_cell_style	     (NautilusList	      *list,
						      NautilusCListRow	      *row,
						      int		       state,
						      int		       row_index,
						      int		       column_index,
						      GtkStyle		     **style,
						      GdkGC		     **fg_gc,
						      GdkGC		     **bg_gc,
						      guint32		      *bg_rgb);
void	     nautilus_list_set_alternate_row_colors  (NautilusList	      *list,
						      gboolean		       state);
void	     nautilus_list_set_background_color_offsets (NautilusList         *list,
							 long		       background_offset,
							 long		       selection_offset);

#endif /* NAUTILUS_LIST_H */





