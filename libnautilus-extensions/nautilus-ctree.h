/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>
 *
 * NautilusCTree widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

#ifndef NAUTILUS_CTREE_H
#define NAUTILUS_CTREE_H

#include <gtk/gtkclist.h>

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

#define NAUTILUS_TYPE_CTREE            		(nautilus_ctree_get_type ())
#define NAUTILUS_CTREE(obj)            		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CTREE, NautilusCTree))
#define NAUTILUS_CTREE_CLASS(klass)    		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CTREE, NautilusCTreeClass))
#define NAUTILUS_IS_CTREE(obj)         		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CTREE))
#define NAUTILUS_IS_CTREE_CLASS(klass) 		(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CTREE))

#define NAUTILUS_CTREE_ROW(_node_) 		((NautilusCTreeRow *)(((GList *)(_node_))->data))
#define NAUTILUS_CTREE_NODE(_node_) 		((NautilusCTreeNode *)((_node_)))
#define NAUTILUS_CTREE_NODE_NEXT(_nnode_) 	((NautilusCTreeNode *)(((GList *)(_nnode_))->next))
#define NAUTILUS_CTREE_NODE_PREV(_pnode_) 	((NautilusCTreeNode *)(((GList *)(_pnode_))->prev))
#define NAUTILUS_CTREE_FUNC(_func_) 		((NautilusCTreeFunc)(_func_))

typedef enum
{
	NAUTILUS_CTREE_POS_BEFORE,
	NAUTILUS_CTREE_POS_AS_CHILD,
	NAUTILUS_CTREE_POS_AFTER
} NautilusCTreePos;

typedef enum
{
	NAUTILUS_CTREE_LINES_NONE,
	NAUTILUS_CTREE_LINES_SOLID,
	NAUTILUS_CTREE_LINES_DOTTED,
	NAUTILUS_CTREE_LINES_TABBED
} NautilusCTreeLineStyle;

typedef enum
{
	NAUTILUS_CTREE_EXPANDER_TRIANGLE
} NautilusCTreeExpanderStyle;

typedef enum
{
	NAUTILUS_CTREE_EXPANSION_EXPAND,
	NAUTILUS_CTREE_EXPANSION_EXPAND_RECURSIVE,
	NAUTILUS_CTREE_EXPANSION_COLLAPSE,
	NAUTILUS_CTREE_EXPANSION_COLLAPSE_RECURSIVE,
	NAUTILUS_CTREE_EXPANSION_TOGGLE,
	NAUTILUS_CTREE_EXPANSION_TOGGLE_RECURSIVE
} NautilusCTreeExpansionType;

typedef struct _NautilusCTree      NautilusCTree;
typedef struct _NautilusCTreeClass NautilusCTreeClass;
typedef struct _NautilusCTreeRow   NautilusCTreeRow;
typedef struct _NautilusCTreeNode  NautilusCTreeNode;

typedef void (*NautilusCTreeFunc) 	(NautilusCTree     *ctree,
			      		 NautilusCTreeNode *node,
			      		 gpointer      data);

typedef gboolean (*NautilusCTreeGNodeFunc) (NautilusCTree     *ctree,
					    guint         depth,
					    GNode        *gnode,
					    NautilusCTreeNode *cnode,
					    gpointer      data);

typedef gboolean (*NautilusCTreeCompareDragFunc) (NautilusCTree     *ctree,
						  NautilusCTreeNode *source_node,
						  NautilusCTreeNode *new_parent,
						  NautilusCTreeNode *new_sibling);

struct _NautilusCTree
{
	GtkCList clist;
	
	GdkGC *lines_gc;
	
	gint tree_indent;
	gint tree_spacing;
	gint tree_column;
	
	guint line_style     : 2;
	guint show_stub      : 1;
	
	NautilusCTreeNode *prelight_node;

	NautilusCTreeRow *dnd_prelighted_row;

	NautilusCTreeCompareDragFunc drag_compare;
};

struct _NautilusCTreeClass
{
  GtkCListClass parent_class;
  
  void (*tree_select_row)   (NautilusCTree     *ctree,
			     NautilusCTreeNode *row,
			     gint          column);
  void (*tree_unselect_row) (NautilusCTree     *ctree,
			     NautilusCTreeNode *row,
			     gint          column);
  void (*tree_expand)       (NautilusCTree     *ctree,
			     NautilusCTreeNode *node);
  void (*tree_collapse)     (NautilusCTree     *ctree,
			     NautilusCTreeNode *node);
  void (*tree_move)         (NautilusCTree     *ctree,
			     NautilusCTreeNode *node,
			     NautilusCTreeNode *new_parent,
			     NautilusCTreeNode *new_sibling);
  void (*change_focus_row_expansion) (NautilusCTree *ctree,
				      NautilusCTreeExpansionType action);
  void (*tree_activate_row) (NautilusCTree     *ctree,
			     NautilusCTreeNode *row,
			     gint		column);
};

struct _NautilusCTreeRow
{
	GtkCListRow row;
  
	NautilusCTreeNode *parent;
	NautilusCTreeNode *sibling;
	NautilusCTreeNode *children;
  
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
  
	guint16 level;
  
	guint is_leaf  : 1;
	guint expanded : 1;

	gboolean mouse_down;
	gboolean in_hotspot;

};

struct _NautilusCTreeNode {
  GList list;
};


/***********************************************************
 *           Creation, insertion, deletion                 *
 ***********************************************************/

GtkType nautilus_ctree_get_type                       (void);
void nautilus_ctree_construct                         (NautilusCTree          *ctree,
						       gint                    columns, 
						       gint                    tree_column,
						       gchar                  *titles[]);
GtkWidget * nautilus_ctree_new_with_titles            (gint                    columns, 
						       gint                    tree_column,
						       gchar                  *titles[]);
GtkWidget * nautilus_ctree_new                        (gint                    columns, 
						       gint                    tree_column);
NautilusCTreeNode * nautilus_ctree_insert_node        (NautilusCTree          *ctree,
						       NautilusCTreeNode      *parent, 
						       NautilusCTreeNode      *sibling,
						       gchar                  *text[],
						       guint8                  spacing,
						       GdkPixmap              *pixmap_closed,
						       GdkBitmap              *mask_closed,
						       GdkPixmap              *pixmap_opened,
						       GdkBitmap              *mask_opened,
						       gboolean                is_leaf,
						       gboolean                expanded);
void nautilus_ctree_remove_node                       (NautilusCTree          *ctree, 
						       NautilusCTreeNode      *node);
NautilusCTreeNode * nautilus_ctree_insert_gnode       (NautilusCTree          *ctree,
						       NautilusCTreeNode      *parent,
						       NautilusCTreeNode      *sibling,
						       GNode                  *gnode,
						       NautilusCTreeGNodeFunc  func,
						       gpointer                data);
GNode * nautilus_ctree_export_to_gnode                (NautilusCTree          *ctree,
						       GNode                  *parent,
						       GNode                  *sibling,
						       NautilusCTreeNode      *node,
						       NautilusCTreeGNodeFunc  func,
						       gpointer                data);

/***********************************************************
 *  Generic recursive functions, querying / finding tree   *
 *  information                                            *
 ***********************************************************/

void nautilus_ctree_post_recursive                    (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node,
						       NautilusCTreeFunc  func,
						       gpointer      data);
void nautilus_ctree_post_recursive_to_depth           (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node,
						       gint          depth,
						       NautilusCTreeFunc  func,
						       gpointer      data);
void nautilus_ctree_pre_recursive                     (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node,
						       NautilusCTreeFunc  func,
						       gpointer      data);
void nautilus_ctree_pre_recursive_to_depth            (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node,
						       gint          depth,
						       NautilusCTreeFunc  func,
						       gpointer      data);
gboolean nautilus_ctree_is_viewable                   (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);
NautilusCTreeNode * nautilus_ctree_last                    (NautilusCTree     *ctree,
							    NautilusCTreeNode *node);
NautilusCTreeNode * nautilus_ctree_find_node_ptr           (NautilusCTree     *ctree,
							    NautilusCTreeRow  *ctree_row);
NautilusCTreeNode * nautilus_ctree_node_nth                (NautilusCTree     *ctree,
							    int         row);
gboolean nautilus_ctree_find                          (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       NautilusCTreeNode *child);
gboolean nautilus_ctree_is_ancestor                   (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       NautilusCTreeNode *child);
NautilusCTreeNode * nautilus_ctree_find_by_row_data        (NautilusCTree     *ctree,
							    NautilusCTreeNode *node,
							    gpointer      data);
/* returns a GList of all NautilusCTreeNodes with row->data == data. */
GList * nautilus_ctree_find_all_by_row_data           (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gpointer      data);
NautilusCTreeNode * nautilus_ctree_find_by_row_data_custom (NautilusCTree     *ctree,
							    NautilusCTreeNode *node,
							    gpointer      data,
							    GCompareFunc  func);
/* returns a GList of all NautilusCTreeNodes with row->data == data. */
GList * nautilus_ctree_find_all_by_row_data_custom    (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gpointer      data,
						       GCompareFunc  func);
gboolean nautilus_ctree_is_hot_spot                   (NautilusCTree     *ctree,
						       gint          x,
						       gint          y);

/***********************************************************
 *   Tree signals : move, expand, collapse, (un)select     *
 ***********************************************************/

void nautilus_ctree_move                              (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       NautilusCTreeNode *new_parent, 
						       NautilusCTreeNode *new_sibling);
void nautilus_ctree_expand                            (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_expand_recursive                  (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_expand_to_depth                   (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          depth);
void nautilus_ctree_collapse                          (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_collapse_recursive                (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_collapse_to_depth                 (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          depth);
void nautilus_ctree_toggle_expansion                  (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_toggle_expansion_recursive        (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_select                            (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);
void nautilus_ctree_select_recursive                  (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);
void nautilus_ctree_unselect                          (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);
void nautilus_ctree_unselect_recursive                (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);
void nautilus_ctree_real_select_recursive             (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node, 
						       gint          state);
void nautilus_ctree_draw_node 			 (NautilusCTree 	*ctree, 
						  NautilusCTreeNode 	*node);

/***********************************************************
 *           Analogons of GtkCList functions               *
 ***********************************************************/

void nautilus_ctree_node_set_text                     (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       const gchar  *text);
void nautilus_ctree_node_set_pixmap                   (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       GdkPixmap    *pixmap,
						       GdkBitmap    *mask);
void nautilus_ctree_node_set_pixtext                  (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       const gchar  *text,
						       guint8        spacing,
						       GdkPixmap    *pixmap,
						       GdkBitmap    *mask);
void nautilus_ctree_set_node_info                     (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       const gchar  *text,
						       guint8        spacing,
						       GdkPixmap    *pixmap_closed,
						       GdkBitmap    *mask_closed,
						       GdkPixmap    *pixmap_opened,
						       GdkBitmap    *mask_opened,
						       gboolean      is_leaf,
						       gboolean      expanded);
void nautilus_ctree_node_set_shift                    (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       gint          vertical,
						       gint          horizontal);
void nautilus_ctree_node_set_selectable               (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gboolean      selectable);
gboolean nautilus_ctree_node_get_selectable           (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
GtkCellType nautilus_ctree_node_get_cell_type         (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column);
gint nautilus_ctree_node_get_text                     (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       gchar       **text);
gint nautilus_ctree_node_get_pixmap                   (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       GdkPixmap   **pixmap,
						       GdkBitmap   **mask);
gint nautilus_ctree_node_get_pixtext                  (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       gchar       **text,
						       guint8       *spacing,
						       GdkPixmap   **pixmap,
						       GdkBitmap   **mask);
gint nautilus_ctree_get_node_info                     (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gchar       **text,
						       guint8       *spacing,
						       GdkPixmap   **pixmap_closed,
						       GdkBitmap   **mask_closed,
						       GdkPixmap   **pixmap_opened,
						       GdkBitmap   **mask_opened,
						       gboolean     *is_leaf,
						       gboolean     *expanded);

void nautilus_ctree_set_prelight                 (NautilusCTree *ctree,
						  int            y);

void nautilus_ctree_node_set_row_style           (NautilusCTree     *ctree,
						  NautilusCTreeNode *node,
						  GtkStyle     *style);
GtkStyle * nautilus_ctree_node_get_row_style          (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_node_set_cell_style               (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       GtkStyle     *style);
GtkStyle * nautilus_ctree_node_get_cell_style         (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column);
void nautilus_ctree_node_set_foreground               (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       GdkColor     *color);
void nautilus_ctree_node_set_background               (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       GdkColor     *color);
void nautilus_ctree_node_set_row_data                 (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gpointer      data);
void nautilus_ctree_node_set_row_data_full            (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gpointer      data,
						       GtkDestroyNotify destroy);
gpointer nautilus_ctree_node_get_row_data             (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_node_moveto                       (NautilusCTree     *ctree,
						       NautilusCTreeNode *node,
						       gint          column,
						       gfloat        row_align,
						       gfloat        col_align);
GtkVisibility nautilus_ctree_node_is_visible          (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);

/***********************************************************
 *             NautilusCTree specific functions            *
 ***********************************************************/

void nautilus_ctree_set_indent            	(NautilusCTree        		*ctree, 
					   	 gint                     	indent);
void nautilus_ctree_set_spacing           	(NautilusCTree                	*ctree, 
				      		 gint                     	spacing);
void nautilus_ctree_set_show_stub         	(NautilusCTree                	*ctree, 
				      		 gboolean                 	show_stub);
void nautilus_ctree_set_line_style        	(NautilusCTree                	*ctree, 
				      		 NautilusCTreeLineStyle        	line_style);
void nautilus_ctree_set_drag_compare_func 	(NautilusCTree     	      	*ctree,
				      		 NautilusCTreeCompareDragFunc  	cmp_func);

/***********************************************************
 *             Tree sorting functions                      *
 ***********************************************************/

void nautilus_ctree_sort_node                         (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);
void nautilus_ctree_sort_single_node		      (NautilusCTree     *ctree,
						       NautilusCTreeNode *node);
void nautilus_ctree_sort_recursive                    (NautilusCTree     *ctree, 
						       NautilusCTreeNode *node);



#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* NAUTILUS_CTREE_H */
