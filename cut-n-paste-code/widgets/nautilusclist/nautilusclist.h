/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * Copy-pasted from GtkCList to add some missing overridability.
 */

#ifndef NAUTILUS_CLIST_H__
#define NAUTILUS_CLIST_H__

#include <gdk/gdk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkenums.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clist flags */
enum {
  NAUTILUS_CLIST_IN_DRAG             = 1 <<  0,
  NAUTILUS_CLIST_ROW_HEIGHT_SET      = 1 <<  1,
  NAUTILUS_CLIST_SHOW_TITLES         = 1 <<  2,
  NAUTILUS_CLIST_CHILD_HAS_FOCUS     = 1 <<  3,
  NAUTILUS_CLIST_ADD_MODE            = 1 <<  4,
  NAUTILUS_CLIST_AUTO_SORT           = 1 <<  5,
  NAUTILUS_CLIST_AUTO_RESIZE_BLOCKED = 1 <<  6,
  NAUTILUS_CLIST_REORDERABLE         = 1 <<  7,
  NAUTILUS_CLIST_USE_DRAG_ICONS      = 1 <<  8,
  NAUTILUS_CLIST_DRAW_DRAG_LINE      = 1 <<  9,
  NAUTILUS_CLIST_DRAW_DRAG_RECT      = 1 << 10
}; 

/* cell types */
/* Superset of GtkCellType enum defined in gtk-clist.h */
typedef enum
{
  NAUTILUS_CELL_EMPTY,
  NAUTILUS_CELL_TEXT,
  NAUTILUS_CELL_PIXBUF,   	/* new for Nautilus */
  NAUTILUS_CELL_PIXTEXT,        /* now uses pixbuf */
  NAUTILUS_CELL_WIDGET,
  NAUTILUS_CELL_PIXBUF_LIST,   	/* new for Nautilus */
  NAUTILUS_CELL_LINK_TEXT	/* new for Nautilus */
} NautilusCellType;

typedef enum
{
  NAUTILUS_CLIST_DRAG_NONE,
  NAUTILUS_CLIST_DRAG_BEFORE,
  NAUTILUS_CLIST_DRAG_INTO,
  NAUTILUS_CLIST_DRAG_AFTER
} NautilusCListDragPos;

typedef enum
{
  NAUTILUS_BUTTON_IGNORED = 0,
  NAUTILUS_BUTTON_SELECTS = 1 << 0,
  NAUTILUS_BUTTON_DRAGS   = 1 << 1,
  NAUTILUS_BUTTON_EXPANDS = 1 << 2
} NautilusButtonAction;

#define NAUTILUS_TYPE_CLIST            (nautilus_clist_get_type ())
#define NAUTILUS_CLIST(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CLIST, NautilusCList))
#define NAUTILUS_CLIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CLIST, NautilusCListClass))
#define NAUTILUS_IS_CLIST(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CLIST))
#define NAUTILUS_IS_CLIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CLIST))

#define NAUTILUS_CLIST_FLAGS(clist)             (NAUTILUS_CLIST (clist)->flags)
#define NAUTILUS_CLIST_SET_FLAG(clist,flag)     (NAUTILUS_CLIST_FLAGS (clist) |= (NAUTILUS_ ## flag))
#define NAUTILUS_CLIST_UNSET_FLAG(clist,flag)   (NAUTILUS_CLIST_FLAGS (clist) &= ~(NAUTILUS_ ## flag))

#define NAUTILUS_CLIST_IN_DRAG(clist)           (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_IN_DRAG)
#define NAUTILUS_CLIST_ROW_HEIGHT_SET(clist)    (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_ROW_HEIGHT_SET)
#define NAUTILUS_CLIST_SHOW_TITLES(clist)       (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_SHOW_TITLES)
#define NAUTILUS_CLIST_CHILD_HAS_FOCUS(clist)   (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_CHILD_HAS_FOCUS)
#define NAUTILUS_CLIST_ADD_MODE(clist)          (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_ADD_MODE)
#define NAUTILUS_CLIST_AUTO_SORT(clist)         (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_AUTO_SORT)
#define NAUTILUS_CLIST_AUTO_RESIZE_BLOCKED(clist) (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_AUTO_RESIZE_BLOCKED)
#define NAUTILUS_CLIST_REORDERABLE(clist)       (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_REORDERABLE)
#define NAUTILUS_CLIST_USE_DRAG_ICONS(clist)    (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_USE_DRAG_ICONS)
#define NAUTILUS_CLIST_DRAW_DRAG_LINE(clist)    (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_DRAW_DRAG_LINE)
#define NAUTILUS_CLIST_DRAW_DRAG_RECT(clist)    (NAUTILUS_CLIST_FLAGS (clist) & NAUTILUS_CLIST_DRAW_DRAG_RECT)

#define NAUTILUS_CLIST_ROW(_glist_) ((NautilusCListRow *)((_glist_)->data))

/* pointer casting for cells */
#define NAUTILUS_CELL_TEXT(cell)     (((NautilusCellText *) &(cell)))
#define NAUTILUS_CELL_PIXBUF(cell)   (((NautilusCellPixbuf *) &(cell)))
#define NAUTILUS_CELL_PIXTEXT(cell)  (((NautilusCellPixText *) &(cell)))
#define NAUTILUS_CELL_WIDGET(cell)   (((NautilusCellWidget *) &(cell)))

typedef struct NautilusCList NautilusCList;
typedef struct NautilusCListClass NautilusCListClass;
typedef struct NautilusCListColumn NautilusCListColumn;
typedef struct NautilusCListRow NautilusCListRow;

typedef struct NautilusCell NautilusCell;
typedef struct NautilusCellText NautilusCellText;
typedef struct NautilusCellPixbuf NautilusCellPixbuf;
typedef struct NautilusCellPixText NautilusCellPixText;
typedef struct NautilusCellWidget NautilusCellWidget;

typedef gint (*NautilusCListCompareFunc) (NautilusCList     *clist,
				     gconstpointer ptr1,
				     gconstpointer ptr2);

typedef struct NautilusCListCellInfo NautilusCListCellInfo;
typedef struct NautilusCListDestInfo NautilusCListDestInfo;

struct NautilusCListCellInfo
{
  gint row;
  gint column;
};

struct NautilusCListDestInfo
{
  NautilusCListCellInfo cell;
  NautilusCListDragPos  insert_pos;
};

struct NautilusCList
{
  GtkContainer container;
  
  guint16 flags;
  
  /* mem chunks */
  GMemChunk *row_mem_chunk;
  GMemChunk *cell_mem_chunk;

  guint freeze_count;
  gboolean refresh_at_unfreeze_time;
  
  /* allocation rectangle after the conatiner_border_width
   * and the width of the shadow border */
  GdkRectangle internal_allocation;
  
  /* rows */
  gint rows;
  gint row_center_offset;
  gint row_height;
  GList *row_list;
  GList *row_list_end;
  
  /* columns */
  gint columns;
  GdkRectangle column_title_area;
  GdkWindow *title_window;
  
  /* dynamicly allocated array of column structures */
  NautilusCListColumn *column;
  
  /* the scrolling window and its height and width to
   * make things a little speedier */
  GdkWindow *clist_window;
  gint clist_window_width;
  gint clist_window_height;
  
  /* offsets for scrolling */
  gint hoffset;
  gint voffset;
  
  /* border shadow style */
  GtkShadowType shadow_type;
  
  /* the list's selection mode (gtkenums.h) */
  GtkSelectionMode selection_mode;
  
  /* list of selected rows */
  GList *selection;
  GList *selection_end;
  
  GList *undo_selection;
  GList *undo_unselection;
  gint undo_anchor;
  
  /* mouse buttons */
  guint8 button_actions[5];

  guint8 drag_button;

  /* dnd */
  NautilusCListCellInfo click_cell;

  /* scroll adjustments */
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  
  /* xor GC for the vertical drag line */
  GdkGC *xor_gc;
  
  /* gc for drawing unselected cells */
  GdkGC *fg_gc;
  GdkGC *bg_gc;
  
  /* cursor used to indicate dragging */
  GdkCursor *cursor_drag;
  
  /* the current x-pixel location of the xor-drag line */
  gint x_drag;
  
  /* focus handling */
  gint focus_row;
  
  /* dragging the selection */
  gint anchor;
  GtkStateType anchor_state;
  gint drag_pos;
  gint htimer;
  gint vtimer;
  
  GtkSortType sort_type;
  NautilusCListCompareFunc compare;
  gint sort_column;
};

struct NautilusCListClass
{
  GtkContainerClass parent_class;
  
  void  (*set_scroll_adjustments) (NautilusCList       *clist,
				   GtkAdjustment  *hadjustment,
				   GtkAdjustment  *vadjustment);
  void   (*refresh)             (NautilusCList       *clist);
  void   (*select_row)          (NautilusCList       *clist,
				 gint            row,
				 gint            column,
				 GdkEvent       *event);
  void   (*unselect_row)        (NautilusCList       *clist,
				 gint            row,
				 gint            column,
				 GdkEvent       *event);
  void   (*row_move)            (NautilusCList       *clist,
				 gint            source_row,
				 gint            dest_row);
  void   (*click_column)        (NautilusCList       *clist,
				 gint            column);
  void   (*resize_column)       (NautilusCList       *clist,
				 gint            column,
                                 gint            width);
  void   (*toggle_focus_row)    (NautilusCList       *clist);
  void   (*select_all)          (NautilusCList       *clist);
  void   (*unselect_all)        (NautilusCList       *clist);
  void   (*undo_selection)      (NautilusCList       *clist);
  void   (*start_selection)     (NautilusCList       *clist);
  void   (*end_selection)       (NautilusCList       *clist);
  void   (*extend_selection)    (NautilusCList       *clist,
				 GtkScrollType   scroll_type,
				 gfloat          position,
				 gboolean        auto_start_selection);
  void   (*scroll_horizontal)   (NautilusCList       *clist,
				 GtkScrollType   scroll_type,
				 gfloat          position);
  void   (*scroll_vertical)     (NautilusCList       *clist,
				 GtkScrollType   scroll_type,
				 gfloat          position);
  void   (*toggle_add_mode)     (NautilusCList       *clist);
  void   (*abort_column_resize) (NautilusCList       *clist);
  void   (*resync_selection)    (NautilusCList       *clist,
				 GdkEvent       *event);
  GList* (*selection_find)      (NautilusCList       *clist,
				 gint            row_number,
				 GList          *row_list_element);
  void   (*draw_rows)           (NautilusCList       *clist,
				 GdkRectangle   *area);
  void   (*draw_row)            (NautilusCList       *clist,
				 GdkRectangle   *area,
				 gint            row,
				 NautilusCListRow    *clist_row);
  void   (*draw_all)            (NautilusCList       *clist);
  void   (*draw_drag_highlight) (NautilusCList        *clist,
				 NautilusCListRow     *target_row,
				 gint             target_row_number,
				 NautilusCListDragPos  drag_pos);
  void   (*clear)               (NautilusCList       *clist);
  void   (*fake_unselect_all)   (NautilusCList       *clist,
				 gint            row);
  void   (*sort_list)           (NautilusCList       *clist);
  gint   (*insert_row)          (NautilusCList       *clist,
				 gint            row,
				 gchar          *text[]);
  void   (*remove_row)          (NautilusCList       *clist,
				 gint            row);
  gboolean (*set_cell_contents) (NautilusCList       *clist,
				 NautilusCListRow    *clist_row,
				 gint            column,
				 NautilusCellType     type,
				 const gchar    *text,
				 guint8          spacing,
				 GdkPixbuf      *pixbuf);
  void   (*cell_size_request)   (NautilusCList       *clist,
				 NautilusCListRow    *clist_row,
				 gint            column,
				 GtkRequisition *requisition);

};

struct NautilusCListColumn
{
  gchar *title;
  GdkRectangle area;
  
  GtkWidget *button;
  GdkWindow *window;
  
  gint width;
  gint min_width;
  gint max_width;
  GtkJustification justification;
  
  guint visible        : 1;  
  guint width_set      : 1;
  guint resizeable     : 1;
  guint auto_resize    : 1;
  guint button_passive : 1;
};

struct NautilusCListRow
{
  NautilusCell *cell;
  GtkStateType state;
  
  GdkColor foreground;
  GdkColor background;
  
  GtkStyle *style;

  gpointer data;
  GtkDestroyNotify destroy;
  
  guint fg_set     : 1;
  guint bg_set     : 1;
  guint selectable : 1;
};

/* Cell Structures */
struct NautilusCellText
{
  NautilusCellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  gchar *text;
};

struct NautilusCellPixbuf
{
  NautilusCellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  GdkPixbuf *pixbuf;
};

struct NautilusCellPixText
{
  NautilusCellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  gchar *text;
  guint8 spacing;
  GdkPixbuf *pixbuf;
};

struct NautilusCellWidget
{
  NautilusCellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  GtkWidget *widget;
};

struct NautilusCell
{
  NautilusCellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  union {
    gchar *text;
    
    struct {
      GdkPixbuf *pixbuf;
    } pb;
    
    struct {
      gchar *text;
      guint8 spacing;
      GdkPixbuf *pixbuf;
    } pt;
    
    GtkWidget *widget;
  } u;
};

GtkType nautilus_clist_get_type (void);

/* constructors useful for gtk-- wrappers */
void nautilus_clist_construct (NautilusCList *clist,
			  gint      columns,
			  gchar    *titles[]);

/* create a new NautilusCList */
GtkWidget* nautilus_clist_new             (gint   columns);
GtkWidget* nautilus_clist_new_with_titles (gint   columns,
				      gchar *titles[]);

/* set adjustments of clist */
void nautilus_clist_set_hadjustment (NautilusCList      *clist,
				GtkAdjustment *adjustment);
void nautilus_clist_set_vadjustment (NautilusCList      *clist,
				GtkAdjustment *adjustment);

/* get adjustments of clist */
GtkAdjustment* nautilus_clist_get_hadjustment (NautilusCList *clist);
GtkAdjustment* nautilus_clist_get_vadjustment (NautilusCList *clist);

/* set the border style of the clist */
void nautilus_clist_set_shadow_type (NautilusCList      *clist,
				GtkShadowType  type);

/* set the clist's selection mode */
void nautilus_clist_set_selection_mode (NautilusCList         *clist,
				   GtkSelectionMode  mode);

/* enable clists reorder ability */
void nautilus_clist_set_reorderable (NautilusCList *clist,
				gboolean  reorderable);
void nautilus_clist_set_use_drag_icons (NautilusCList *clist,
				   gboolean  use_icons);
void nautilus_clist_set_button_actions (NautilusCList *clist,
				   guint     button,
				   guint8    button_actions);

/* freeze all visual updates of the list, and then thaw the list after
 * you have made a number of changes and the updates wil occure in a
 * more efficent mannor than if you made them on a unfrozen list
 */
void nautilus_clist_freeze (NautilusCList *clist);
void nautilus_clist_thaw   (NautilusCList *clist);

/* show and hide the column title buttons */
void nautilus_clist_column_titles_show (NautilusCList *clist);
void nautilus_clist_column_titles_hide (NautilusCList *clist);

/* set the column title to be a active title (responds to button presses, 
 * prelights, and grabs keyboard focus), or passive where it acts as just
 * a title
 */
void nautilus_clist_column_title_active   (NautilusCList *clist,
				      gint      column);
void nautilus_clist_column_title_passive  (NautilusCList *clist,
				      gint      column);
void nautilus_clist_column_titles_active  (NautilusCList *clist);
void nautilus_clist_column_titles_passive (NautilusCList *clist);

/* set the title in the column title button */
void nautilus_clist_set_column_title (NautilusCList    *clist,
				 gint         column,
				 const gchar *title);

/* returns the title of column. Returns NULL if title is not set */
gchar * nautilus_clist_get_column_title (NautilusCList *clist,
				    gint      column);

/* set a widget instead of a title for the column title button */
void nautilus_clist_set_column_widget (NautilusCList  *clist,
				  gint       column,
				  GtkWidget *widget);

/* returns the column widget */
GtkWidget * nautilus_clist_get_column_widget (NautilusCList *clist,
					 gint      column);

/* set the justification on a column */
void nautilus_clist_set_column_justification (NautilusCList         *clist,
					 gint              column,
					 GtkJustification  justification);

/* set visibility of a column */
void nautilus_clist_set_column_visibility (NautilusCList *clist,
				      gint      column,
				      gboolean  visible);

/* enable/disable column resize operations by mouse */
void nautilus_clist_set_column_resizeable (NautilusCList *clist,
				      gint      column,
				      gboolean  resizeable);

/* resize column automatically to its optimal width */
void nautilus_clist_set_column_auto_resize (NautilusCList *clist,
				       gint      column,
				       gboolean  auto_resize);

gint nautilus_clist_columns_autosize (NautilusCList *clist);

/* return the optimal column width, i.e. maximum of all cell widths */
gint nautilus_clist_optimal_column_width (NautilusCList *clist,
				     gint      column);

/* set the pixel width of a column; this is a necessary step in
 * creating a CList because otherwise the column width is chozen from
 * the width of the column title, which will never be right
 */
void nautilus_clist_set_column_width (NautilusCList *clist,
				 gint      column,
				 gint      width);

/* set column minimum/maximum width. min/max_width < 0 => no restriction */
void nautilus_clist_set_column_min_width (NautilusCList *clist,
				     gint      column,
				     gint      min_width);
void nautilus_clist_set_column_max_width (NautilusCList *clist,
				     gint      column,
				     gint      max_width);

/* change the height of the rows, the default (height=0) is
 * the hight of the current font.
 */
void nautilus_clist_set_row_height (NautilusCList *clist,
			       guint     height);

/* scroll the viewing area of the list to the given column and row;
 * row_align and col_align are between 0-1 representing the location the
 * row should appear on the screnn, 0.0 being top or left, 1.0 being
 * bottom or right; if row or column is -1 then then there is no change
 */
void nautilus_clist_moveto (NautilusCList *clist,
		       gint      row,
		       gint      column,
		       gfloat    row_align,
		       gfloat    col_align);

/* returns whether the row is visible */
GtkVisibility nautilus_clist_row_is_visible (NautilusCList *clist,
					gint      row);

/* returns the cell type */
NautilusCellType nautilus_clist_get_cell_type (NautilusCList *clist,
				     gint      row,
				     gint      column);

/* sets a given cell's text, replacing its current contents */
void nautilus_clist_set_text (NautilusCList    *clist,
			 gint         row,
			 gint         column,
			 const gchar *text);

/* for the "get" functions, any of the return pointer can be
 * NULL if you are not interested
 */
gint nautilus_clist_get_text (NautilusCList  *clist,
			 gint       row,
			 gint       column,
			 gchar    **text);

/* sets a given cell's pixbuf, replacing its current contents */
void nautilus_clist_set_pixbuf (NautilusCList  *clist,
                          gint       row,
                          gint       column,
                          GdkPixbuf *pixbuf);

gint nautilus_clist_get_pixbuf (NautilusCList   *clist,
                          gint        row,
                          gint        column,
                          GdkPixbuf **pixbuf);

/* sets a given cell's pixbuf and text, replacing its current contents */
void nautilus_clist_set_pixtext (NautilusCList    *clist,
			    gint         row,
			    gint         column,
			    const gchar *text,
			    guint8       spacing,
			    GdkPixbuf   *pixbuf);

gint nautilus_clist_get_pixtext (NautilusCList   *clist,
			    gint        row,
			    gint        column,
			    gchar     **text,
			    guint8     *spacing,
			    GdkPixbuf **pixbuf);

/* sets the foreground color of a row, the color must already
 * be allocated
 */
void nautilus_clist_set_foreground (NautilusCList *clist,
			       gint      row,
			       GdkColor *color);

/* sets the background color of a row, the color must already
 * be allocated
 */
void nautilus_clist_set_background (NautilusCList *clist,
			       gint      row,
			       GdkColor *color);

/* set / get cell styles */
void nautilus_clist_set_cell_style (NautilusCList *clist,
			       gint      row,
			       gint      column,
			       GtkStyle *style);

GtkStyle *nautilus_clist_get_cell_style (NautilusCList *clist,
				    gint      row,
				    gint      column);

void nautilus_clist_set_row_style (NautilusCList *clist,
			      gint      row,
			      GtkStyle *style);

GtkStyle *nautilus_clist_get_row_style (NautilusCList *clist,
				   gint      row);

/* this sets a horizontal and vertical shift for drawing
 * the contents of a cell; it can be positive or negitive;
 * this is particulary useful for indenting items in a column
 */
void nautilus_clist_set_shift (NautilusCList *clist,
			  gint      row,
			  gint      column,
			  gint      vertical,
			  gint      horizontal);

/* set/get selectable flag of a single row */
void nautilus_clist_set_selectable (NautilusCList *clist,
			       gint      row,
			       gboolean  selectable);
gboolean nautilus_clist_get_selectable (NautilusCList *clist,
				   gint      row);

/* prepend/append returns the index of the row you just added,
 * making it easier to append and modify a row
 */
gint nautilus_clist_prepend (NautilusCList    *clist,
		        gchar       *text[]);
gint nautilus_clist_append  (NautilusCList    *clist,
			gchar       *text[]);

/* inserts a row at index row and returns the row where it was
 * actually inserted (may be different from "row" in auto_sort mode)
 */
gint nautilus_clist_insert (NautilusCList    *clist,
		       gint         row,
		       gchar       *text[]);

/* removes row at index row */
void nautilus_clist_remove (NautilusCList *clist,
		       gint      row);

/* sets a arbitrary data pointer for a given row */
void nautilus_clist_set_row_data (NautilusCList *clist,
			     gint      row,
			     gpointer  data);

/* sets a data pointer for a given row with destroy notification */
void nautilus_clist_set_row_data_full (NautilusCList         *clist,
			          gint              row,
			          gpointer          data,
				  GtkDestroyNotify  destroy);

/* returns the data set for a row */
gpointer nautilus_clist_get_row_data (NautilusCList *clist,
				 gint      row);

/* givin a data pointer, find the first (and hopefully only!)
 * row that points to that data, or -1 if none do
 */
gint nautilus_clist_find_row_from_data (NautilusCList *clist,
				   gpointer  data);

/* force selection of a row */
void nautilus_clist_select_row (NautilusCList *clist,
			   gint      row,
			   gint      column);

/* force unselection of a row */
void nautilus_clist_unselect_row (NautilusCList *clist,
			     gint      row,
			     gint      column);

/* undo the last select/unselect operation */
void nautilus_clist_undo_selection (NautilusCList *clist);

/* clear the entire list -- this is much faster than removing
 * each item with nautilus_clist_remove
 */
void nautilus_clist_clear (NautilusCList *clist);

/* return the row column corresponding to the x and y coordinates,
 * the returned values are only valid if the x and y coordinates
 * are respectively to a window == clist->clist_window
 */
gint nautilus_clist_get_selection_info (NautilusCList *clist,
			     	   gint      x,
			     	   gint      y,
			     	   gint     *row,
			     	   gint     *column);

/* in multiple or extended mode, select all rows */
void nautilus_clist_select_all (NautilusCList *clist);

/* in all modes except browse mode, deselect all rows */
void nautilus_clist_unselect_all (NautilusCList *clist);

/* swap the position of two rows */
void nautilus_clist_swap_rows (NautilusCList *clist,
			  gint      row1,
			  gint      row2);

/* move row from source_row position to dest_row position */
void nautilus_clist_row_move (NautilusCList *clist,
			 gint      source_row,
			 gint      dest_row);

/* sets a compare function different to the default */
void nautilus_clist_set_compare_func (NautilusCList            *clist,
				 NautilusCListCompareFunc  cmp_func);

/* the column to sort by */
void nautilus_clist_set_sort_column (NautilusCList *clist,
				gint      column);

/* how to sort : ascending or descending */
void nautilus_clist_set_sort_type (NautilusCList    *clist,
			      GtkSortType  sort_type);

/* sort the list with the current compare function */
void nautilus_clist_sort (NautilusCList *clist);

/* Automatically sort upon insertion */
void nautilus_clist_set_auto_sort (NautilusCList *clist,
			      gboolean  auto_sort);

gboolean nautilus_clist_check_unfrozen (NautilusCList *clist);


#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* NAUTILUS_CLIST_H__ */
