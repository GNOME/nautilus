/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald, 
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


#include <config.h>

#include "nautilus-ctree.h"

#include <gtk/gtkbindings.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkdnd.h>
#include <gdk/gdkx.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <gtk/gtkclist.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "libnautilus-extensions/nautilus-graphic-effects.h"
#include "libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h"

#define PM_SIZE                    8
#define TAB_SIZE                   (PM_SIZE + 6)
#define CELL_SPACING               1
#define CLIST_OPTIMUM_SIZE         64
#define COLUMN_INSET               3
#define DRAG_WIDTH                 6

#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
#define ROW_FROM_YPIXEL(clist, y)  (((y) - (clist)->voffset) / \
                                    ((clist)->row_height + CELL_SPACING))
#define COLUMN_LEFT_XPIXEL(clist, col)  ((clist)->column[(col)].area.x \
                                    + (clist)->hoffset)
#define COLUMN_LEFT(clist, column) ((clist)->column[(column)].area.x)

static inline gint
COLUMN_FROM_XPIXEL (GtkCList * clist,
		    gint x)
{
  gint i, cx;

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].visible)
      {
	cx = clist->column[i].area.x + clist->hoffset;

	if (x >= (cx - (COLUMN_INSET + CELL_SPACING)) &&
	    x <= (cx + clist->column[i].area.width + COLUMN_INSET))
	  return i;
      }

  /* no match */
  return -1;
}

#define GTK_CLIST_CLASS_FW(_widget_) GTK_CLIST_CLASS (((GtkObject*) (_widget_))->klass)
#define CLIST_UNFROZEN(clist)     (((GtkCList*) (clist))->freeze_count == 0)
#define CLIST_REFRESH(clist)    G_STMT_START { \
  if (CLIST_UNFROZEN (clist)) \
    GTK_CLIST_CLASS_FW (clist)->refresh ((GtkCList*) (clist)); \
} G_STMT_END


enum {
	ARG_0,
	ARG_N_COLUMNS,
	ARG_TREE_COLUMN,
	ARG_INDENT,
	ARG_SPACING,
	ARG_SHOW_STUB,
	ARG_LINE_STYLE
};


static void nautilus_ctree_class_init        (NautilusCTreeClass  *klass);
static void nautilus_ctree_init              (NautilusCTree       *ctree);
static void nautilus_ctree_set_arg		(GtkObject      *object,
					 GtkArg         *arg,
					 guint           arg_id);
static void nautilus_ctree_get_arg      	(GtkObject      *object,
					 GtkArg         *arg,
					 guint           arg_id);
static void nautilus_ctree_realize           (GtkWidget      *widget);
static void nautilus_ctree_unrealize         (GtkWidget      *widget);
static gint nautilus_ctree_button_press      (GtkWidget      *widget,
					 GdkEventButton *event);
static gboolean nautilus_ctree_event   	(GtkWidget      *widget,
					 GdkEvent 	*event,
					 gpointer 	user_data);
static void ctree_attach_styles         (NautilusCTree       *ctree,
					 NautilusCTreeNode   *node,
					 gpointer        data);
static void ctree_detach_styles         (NautilusCTree       *ctree,
					 NautilusCTreeNode   *node, 
					 gpointer        data);
static gint draw_cell_pixmap            (GdkWindow      *window,
					 GdkRectangle   *clip_rectangle,
					 GdkGC          *fg_gc,
					 GdkPixmap      *pixmap,
					 GdkBitmap      *mask,
					 gint            x,
					 gint            y,
					 gint            width,
					 gint            height);
static void get_cell_style              (GtkCList       *clist,
					 GtkCListRow    *clist_row,
					 gint            state,
					 gint            column,
					 GtkStyle      **style,
					 GdkGC         **fg_gc,
					 GdkGC         **bg_gc);
static gint nautilus_ctree_draw_expander     (NautilusCTree       *ctree,
					      NautilusCTreeRow    *ctree_row,
					      GtkStyle       *style,
					      GdkRectangle   *clip_rectangle,
					      gint            x);
static gint nautilus_ctree_draw_lines        (NautilusCTree       *ctree,
					 NautilusCTreeRow    *ctree_row,
					 gint            row,
					 gint            column,
					 gint            state,
					 GdkRectangle   *clip_rectangle,
					 GdkRectangle   *cell_rectangle,
					 GdkRectangle   *crect,
					 GdkRectangle   *area,
					 GtkStyle       *style);
static void draw_row                    (GtkCList       *clist,
					 GdkRectangle   *area,
					 gint            row,
					 GtkCListRow    *clist_row);
static void draw_drag_highlight         (GtkCList        *clist,
					 GtkCListRow     *dest_row,
					 gint             dest_row_number,
					 GtkCListDragPos  drag_pos);
static void tree_draw_node              (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node);
static void set_cell_contents           (GtkCList      *clist,
					 GtkCListRow   *clist_row,
					 gint           column,
					 GtkCellType    type,
					 const gchar   *text,
					 guint8         spacing,
					 GdkPixmap     *pixmap,
					 GdkBitmap     *mask);
static void set_node_info               (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 const gchar   *text,
					 guint8         spacing,
					 GdkPixmap     *pixmap_closed,
					 GdkBitmap     *mask_closed,
					 GdkPixmap     *pixmap_opened,
					 GdkBitmap     *mask_opened,
					 gboolean       is_leaf,
					 gboolean       expanded);
static NautilusCTreeRow *row_new             (NautilusCTree      *ctree);
static void row_delete                  (NautilusCTree      *ctree,
				 	 NautilusCTreeRow   *ctree_row);
static void tree_delete                 (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
					 gpointer       data);
static void tree_delete_row             (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
					 gpointer       data);
static void real_clear                  (GtkCList      *clist);
static void tree_update_level           (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
					 gpointer       data);
static void tree_select                 (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
					 gpointer       data);
static void tree_unselect               (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
				         gpointer       data);
static void real_select_all             (GtkCList      *clist);
static void real_unselect_all           (GtkCList      *clist);
static void tree_expand                 (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node,
					 gpointer       data);
static void tree_collapse               (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node,
					 gpointer       data);
static void tree_collapse_to_depth      (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
					 gint           depth);
static void tree_toggle_expansion       (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 gpointer       data);
static void change_focus_row_expansion  (NautilusCTree      *ctree,
				         NautilusCTreeExpansionType expansion);
static void real_select_row             (GtkCList      *clist,
					 gint           row,
					 gint           column,
					 GdkEvent      *event);
static void real_unselect_row           (GtkCList      *clist,
					 gint           row,
					 gint           column,
					 GdkEvent      *event);
static void real_tree_select            (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 gint           column);
static void real_tree_unselect          (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 gint           column);
static void real_tree_expand            (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node);
static void real_tree_collapse          (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node);
static void real_tree_move              (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 NautilusCTreeNode  *new_parent, 
					 NautilusCTreeNode  *new_sibling);
static void real_row_move               (GtkCList      *clist,
					 gint           source_row,
					 gint           dest_row);
static void nautilus_ctree_link              (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 NautilusCTreeNode  *parent,
					 NautilusCTreeNode  *sibling,
					 gboolean       update_focus_row);
static void nautilus_ctree_unlink            (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node,
					 gboolean       update_focus_row);
static NautilusCTreeNode * nautilus_ctree_last_visible (NautilusCTree     *ctree,
					      NautilusCTreeNode *node);
static gboolean ctree_is_hot_spot       (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node,
					 gint           row, 
					 gint           x, 
					 gint           y);
static void tree_sort                   (NautilusCTree      *ctree,
					 NautilusCTreeNode  *node,
					 gpointer       data);
static void fake_unselect_all           (GtkCList      *clist,
					 gint           row);
static GList * selection_find           (GtkCList      *clist,
					 gint           row_number,
					 GList         *row_list_element);
static void resync_selection            (GtkCList      *clist,
					 GdkEvent      *event);
static void real_undo_selection         (GtkCList      *clist);
static void select_row_recursive        (NautilusCTree      *ctree, 
					 NautilusCTreeNode  *node, 
					 gpointer       data);
static gint real_insert_row             (GtkCList      *clist,
					 gint           row,
					 gchar         *text[]);
static void real_remove_row             (GtkCList      *clist,
					 gint           row);
static void real_sort_list              (GtkCList      *clist);
static void cell_size_request           (GtkCList       *clist,
					 GtkCListRow    *clist_row,
					 gint            column,
					 GtkRequisition *requisition);
static void column_auto_resize          (GtkCList       *clist,
					 GtkCListRow    *clist_row,
					 gint            column,
					 gint            old_width);
static void auto_resize_columns         (GtkCList       *clist);


static gboolean check_drag               (NautilusCTree         *ctree,
			                  NautilusCTreeNode     *drag_source,
					  NautilusCTreeNode     *drag_target,
					  GtkCListDragPos   insert_pos);
static void nautilus_ctree_drag_begin         (GtkWidget        *widget,
					  GdkDragContext   *context);
static gint nautilus_ctree_drag_motion        (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  guint             time);
static void nautilus_ctree_drag_data_received (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *selection_data,
					  guint             info,
					  guint32           time);
static void remove_grab                  (GtkCList         *clist);
static void drag_dest_cell               (GtkCList         *clist,
					  gint              x,
					  gint              y,
					  GtkCListDestInfo *dest_info);


enum
{
	TREE_SELECT_ROW,
	TREE_UNSELECT_ROW,
	TREE_EXPAND,
	TREE_COLLAPSE,
	TREE_MOVE,
	CHANGE_FOCUS_ROW_EXPANSION,
	LAST_SIGNAL
};

static GtkCListClass *parent_class = NULL;
static GtkContainerClass *container_class = NULL;
static guint ctree_signals[LAST_SIGNAL] = {0};


GtkType
nautilus_ctree_get_type (void)
{
  static GtkType ctree_type = 0;

  if (!ctree_type)
    {
      static const GtkTypeInfo ctree_info =
      {
	"NautilusCTree",
	sizeof (NautilusCTree),
	sizeof (NautilusCTreeClass),
	(GtkClassInitFunc) nautilus_ctree_class_init,
	(GtkObjectInitFunc) nautilus_ctree_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      ctree_type = gtk_type_unique (GTK_TYPE_CLIST, &ctree_info);
    }

  return ctree_type;
}

static void
nautilus_ctree_class_init (NautilusCTreeClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCListClass *clist_class;
	GtkBindingSet *binding_set;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	container_class = (GtkContainerClass *) klass;
	clist_class = (GtkCListClass *) klass;

	parent_class = gtk_type_class (GTK_TYPE_CLIST);
	container_class = gtk_type_class (GTK_TYPE_CONTAINER);

	gtk_object_add_arg_type ("NautilusCTree::n_columns",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_N_COLUMNS);
	gtk_object_add_arg_type ("NautilusCTree::tree_column",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_TREE_COLUMN);
	gtk_object_add_arg_type ("NautilusCTree::indent",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_INDENT);
	gtk_object_add_arg_type ("NautilusCTree::spacing",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SPACING);
	gtk_object_add_arg_type ("NautilusCTree::show_stub",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SHOW_STUB);
	object_class->set_arg = nautilus_ctree_set_arg;
	object_class->get_arg = nautilus_ctree_get_arg;

	ctree_signals[TREE_SELECT_ROW] =
    gtk_signal_new ("tree_select_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (NautilusCTreeClass, tree_select_row),
		    gtk_marshal_NONE__POINTER_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_CTREE_NODE, GTK_TYPE_INT);
  ctree_signals[TREE_UNSELECT_ROW] =
    gtk_signal_new ("tree_unselect_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (NautilusCTreeClass, tree_unselect_row),
		    gtk_marshal_NONE__POINTER_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_CTREE_NODE, GTK_TYPE_INT);
  ctree_signals[TREE_EXPAND] =
    gtk_signal_new ("tree_expand",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (NautilusCTreeClass, tree_expand),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1, GTK_TYPE_CTREE_NODE);
  ctree_signals[TREE_COLLAPSE] =
    gtk_signal_new ("tree_collapse",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (NautilusCTreeClass, tree_collapse),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1, GTK_TYPE_CTREE_NODE);
  ctree_signals[TREE_MOVE] =
    gtk_signal_new ("tree_move",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (NautilusCTreeClass, tree_move),
		    gtk_marshal_NONE__POINTER_POINTER_POINTER,
		    GTK_TYPE_NONE, 3, GTK_TYPE_CTREE_NODE,
		    GTK_TYPE_CTREE_NODE, GTK_TYPE_CTREE_NODE);
  ctree_signals[CHANGE_FOCUS_ROW_EXPANSION] =
    gtk_signal_new ("change_focus_row_expansion",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (NautilusCTreeClass,
				       change_focus_row_expansion),
		    gtk_marshal_NONE__ENUM,
		    GTK_TYPE_NONE, 1, GTK_TYPE_CTREE_EXPANSION_TYPE);
  gtk_object_class_add_signals (object_class, ctree_signals, LAST_SIGNAL);

  widget_class->realize = nautilus_ctree_realize;
  widget_class->unrealize = nautilus_ctree_unrealize;
  widget_class->button_press_event = nautilus_ctree_button_press;
  
  widget_class->drag_begin = nautilus_ctree_drag_begin;
  widget_class->drag_motion = nautilus_ctree_drag_motion;
  widget_class->drag_data_received = nautilus_ctree_drag_data_received;

  clist_class->select_row = real_select_row;
  clist_class->unselect_row = real_unselect_row;
  clist_class->row_move = real_row_move;
  clist_class->undo_selection = real_undo_selection;
  clist_class->resync_selection = resync_selection;
  clist_class->selection_find = selection_find;
  clist_class->click_column = NULL;
  clist_class->draw_row = draw_row;
  clist_class->draw_drag_highlight = draw_drag_highlight;
  clist_class->clear = real_clear;
  clist_class->select_all = real_select_all;
  clist_class->unselect_all = real_unselect_all;
  clist_class->fake_unselect_all = fake_unselect_all;
  clist_class->insert_row = real_insert_row;
  clist_class->remove_row = real_remove_row;
  clist_class->sort_list = real_sort_list;
  clist_class->set_cell_contents = set_cell_contents;
  clist_class->cell_size_request = cell_size_request;

  klass->tree_select_row = real_tree_select;
  klass->tree_unselect_row = real_tree_unselect;
  klass->tree_expand = real_tree_expand;
  klass->tree_collapse = real_tree_collapse;
  klass->tree_move = real_tree_move;
  klass->change_focus_row_expansion = change_focus_row_expansion;

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
				'+', GDK_SHIFT_MASK,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM, NAUTILUS_CTREE_EXPANSION_EXPAND);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Add, 0,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM, NAUTILUS_CTREE_EXPANSION_EXPAND);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Add, GDK_CONTROL_MASK,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM,
				NAUTILUS_CTREE_EXPANSION_EXPAND_RECURSIVE);
  gtk_binding_entry_add_signal (binding_set,
				'-', 0,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM, NAUTILUS_CTREE_EXPANSION_COLLAPSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Subtract, 0,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM, NAUTILUS_CTREE_EXPANSION_COLLAPSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Subtract, GDK_CONTROL_MASK,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM,
				NAUTILUS_CTREE_EXPANSION_COLLAPSE_RECURSIVE);
  gtk_binding_entry_add_signal (binding_set,
				'=', 0,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM, NAUTILUS_CTREE_EXPANSION_TOGGLE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Multiply, 0,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM, NAUTILUS_CTREE_EXPANSION_TOGGLE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Multiply, GDK_CONTROL_MASK,
				"change_focus_row_expansion", 1,
				GTK_TYPE_ENUM,
				NAUTILUS_CTREE_EXPANSION_TOGGLE_RECURSIVE);	
}

static void
nautilus_ctree_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  NautilusCTree *ctree;

  ctree = NAUTILUS_CTREE (object);

  switch (arg_id)
    {
    case ARG_N_COLUMNS: /* construct-only arg, only set when !GTK_CONSTRUCTED */
      if (ctree->tree_column)
	nautilus_ctree_construct (ctree,
			     MAX (1, GTK_VALUE_UINT (*arg)),
			     ctree->tree_column, NULL);
      else
	GTK_CLIST (ctree)->columns = MAX (1, GTK_VALUE_UINT (*arg));
      break;
    case ARG_TREE_COLUMN: /* construct-only arg, only set when !GTK_CONSTRUCTED */
      if (GTK_CLIST (ctree)->columns)
	nautilus_ctree_construct (ctree,
			     GTK_CLIST (ctree)->columns,
			     MAX (1, GTK_VALUE_UINT (*arg)),
			     NULL);
      else
	ctree->tree_column = MAX (1, GTK_VALUE_UINT (*arg));
      break;
    case ARG_INDENT:
      nautilus_ctree_set_indent (ctree, GTK_VALUE_UINT (*arg));
      break;
    case ARG_SPACING:
      nautilus_ctree_set_spacing (ctree, GTK_VALUE_UINT (*arg));
      break;
    case ARG_SHOW_STUB:
      nautilus_ctree_set_show_stub (ctree, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_LINE_STYLE:
      nautilus_ctree_set_line_style (ctree, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
nautilus_ctree_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  NautilusCTree *ctree;

  ctree = NAUTILUS_CTREE (object);

  switch (arg_id)
    {
    case ARG_N_COLUMNS:
      GTK_VALUE_UINT (*arg) = GTK_CLIST (ctree)->columns;
      break;
    case ARG_TREE_COLUMN:
      GTK_VALUE_UINT (*arg) = ctree->tree_column;
      break;
    case ARG_INDENT:
      GTK_VALUE_UINT (*arg) = ctree->tree_indent;
      break;
    case ARG_SPACING:
      GTK_VALUE_UINT (*arg) = ctree->tree_spacing;
      break;
    case ARG_SHOW_STUB:
      GTK_VALUE_BOOL (*arg) = ctree->show_stub;
      break;
    case ARG_LINE_STYLE:
      GTK_VALUE_ENUM (*arg) = ctree->line_style;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
nautilus_ctree_init (NautilusCTree *ctree)
{
	GtkCList *clist;

	GTK_CLIST_SET_FLAG (ctree, CLIST_DRAW_DRAG_RECT);
	GTK_CLIST_SET_FLAG (ctree, CLIST_DRAW_DRAG_LINE);

	clist = GTK_CLIST (ctree);

	ctree->tree_indent    	= 20;
	ctree->tree_spacing   	= 5;
	ctree->tree_column    	= 0;
	ctree->line_style     	= NAUTILUS_CTREE_LINES_NONE;
	ctree->drag_compare   	= NULL;
	ctree->show_stub      	= TRUE;
	ctree->prelight_node	= NULL;
	
	clist->button_actions[0] |= GTK_BUTTON_EXPANDS;

	gtk_signal_connect (GTK_OBJECT (ctree), "event",
			    GTK_SIGNAL_FUNC (nautilus_ctree_event), ctree);
}

static void
ctree_attach_styles (NautilusCTree     *ctree,
		     NautilusCTreeNode *node,
		     gpointer      data)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  if (NAUTILUS_CTREE_ROW (node)->row.style)
    NAUTILUS_CTREE_ROW (node)->row.style =
      gtk_style_attach (NAUTILUS_CTREE_ROW (node)->row.style, clist->clist_window);

  if (NAUTILUS_CTREE_ROW (node)->row.fg_set || NAUTILUS_CTREE_ROW (node)->row.bg_set)
    {
      GdkColormap *colormap;

      colormap = gtk_widget_get_colormap (GTK_WIDGET (ctree));
      if (NAUTILUS_CTREE_ROW (node)->row.fg_set)
	gdk_color_alloc (colormap, &(NAUTILUS_CTREE_ROW (node)->row.foreground));
      if (NAUTILUS_CTREE_ROW (node)->row.bg_set)
	gdk_color_alloc (colormap, &(NAUTILUS_CTREE_ROW (node)->row.background));
    }

  for (i = 0; i < clist->columns; i++)
    if  (NAUTILUS_CTREE_ROW (node)->row.cell[i].style)
      NAUTILUS_CTREE_ROW (node)->row.cell[i].style =
	gtk_style_attach (NAUTILUS_CTREE_ROW (node)->row.cell[i].style,
			  clist->clist_window);
}

static void
ctree_detach_styles (NautilusCTree     *ctree,
		     NautilusCTreeNode *node,
		     gpointer      data)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  if (NAUTILUS_CTREE_ROW (node)->row.style)
    gtk_style_detach (NAUTILUS_CTREE_ROW (node)->row.style);
  for (i = 0; i < clist->columns; i++)
    if  (NAUTILUS_CTREE_ROW (node)->row.cell[i].style)
      gtk_style_detach (NAUTILUS_CTREE_ROW (node)->row.cell[i].style);
}

static void
nautilus_ctree_realize (GtkWidget *widget)
{
  NautilusCTree *ctree;
  GtkCList *clist;
  GdkGCValues values;
  NautilusCTreeNode *node;
  NautilusCTreeNode *child;
  gint i;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (widget));

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  ctree = NAUTILUS_CTREE (widget);
  clist = GTK_CLIST (widget);

  node = NAUTILUS_CTREE_NODE (clist->row_list);
  for (i = 0; i < clist->rows; i++)
    {
      if (!NAUTILUS_CTREE_ROW (node)->is_leaf && !NAUTILUS_CTREE_ROW (node)->expanded)
	for (child = NAUTILUS_CTREE_ROW (node)->children; 
	     child != NULL;
	     child = NAUTILUS_CTREE_ROW (child)->sibling)
	  nautilus_ctree_pre_recursive (ctree, child, ctree_attach_styles, NULL);
      node = NAUTILUS_CTREE_NODE_NEXT (node);
    }

  values.foreground = widget->style->fg[GTK_STATE_NORMAL];
  values.background = widget->style->base[GTK_STATE_NORMAL];
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  values.line_style = GDK_LINE_SOLID;
  ctree->lines_gc = gdk_gc_new_with_values (GTK_CLIST(widget)->clist_window, 
					    &values,
					    GDK_GC_FOREGROUND |
					    GDK_GC_BACKGROUND |
					    GDK_GC_SUBWINDOW |
					    GDK_GC_LINE_STYLE);

  if (ctree->line_style == NAUTILUS_CTREE_LINES_DOTTED)
    {
      gdk_gc_set_line_attributes (ctree->lines_gc, 1, 
				  GDK_LINE_ON_OFF_DASH, None, None);
      gdk_gc_set_dashes (ctree->lines_gc, 0, "\1\1", 2);
    }
}

#define	ROW_ELEMENT(clist, row)	(((row) == (clist)->rows - 1) ? \
				 (clist)->row_list_end : \
				 g_list_nth ((clist)->row_list, (row)))

static gint 
nautilus_ctree_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GdkEventMotion *motion;
	int press_row, press_column, row;
	NautilusCTree *tree;
	NautilusCTreeNode *node, *old_node;
	NautilusCTreeRow *ctree_row;
	GtkCList *clist;
	gint x, y;
	GdkModifierType button;

	tree = NAUTILUS_CTREE (widget);
	clist = GTK_CLIST (widget);

	/* Do prelighting */ 
	if (event->type == GDK_MOTION_NOTIFY) {
		motion = (GdkEventMotion *) event;

		/* Get node that we are over */
		row = gtk_clist_get_selection_info (clist, motion->x, motion->y, &press_row, &press_column);
		if (row <= 0) {
			return FALSE;
		}
		
		ctree_row = ROW_ELEMENT (clist, press_row)->data;
		if (ctree_row == NULL) {
			return FALSE;
		}
		
		node = nautilus_ctree_find_node_ptr (tree, ctree_row);
		if (node == NULL) {
			return FALSE;
		}

		/* Cancel prelighting if we have a button pressed */
		gdk_window_get_pointer (widget->window, &x, &y, &button);
		if ((button & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK)) != 0) {
			if (nautilus_ctree_is_hot_spot (tree, motion->x, motion->y)) {
				/* Handle moving in and out of hotspot while mouse is down */
				if (!ctree_row->in_hotspot) {
					ctree_row->in_hotspot = TRUE;
					nautilus_ctree_draw_node (tree, node);
				}
			} else {
				if (ctree_row->in_hotspot) {
					ctree_row->in_hotspot = FALSE;
					nautilus_ctree_draw_node (tree, node);
				}
			}

			/* Remove prelighting */
			if (tree->prelight_node != NULL) {
				old_node = tree->prelight_node;
				tree->prelight_node = NULL;
				nautilus_ctree_draw_node (tree, old_node);
			}
			return FALSE;
		}
						
		if (nautilus_ctree_is_hot_spot (tree, motion->x, motion->y)) {
			if (node != tree->prelight_node) {
				/* Redraw old prelit node and save and draw new highlight */
				old_node = tree->prelight_node;
				tree->prelight_node = node;
				nautilus_ctree_draw_node (tree, old_node);
				nautilus_ctree_draw_node (tree, tree->prelight_node);
			}				
		} else if (tree->prelight_node != NULL) {
			/* End prelighting of last expander */
			old_node = tree->prelight_node;
			tree->prelight_node = NULL;
			nautilus_ctree_draw_node (tree, old_node);
		}
	}
	
	return FALSE;
}

static void
nautilus_ctree_unrealize (GtkWidget *widget)
{
  NautilusCTree *ctree;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (widget));

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

  ctree = NAUTILUS_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      NautilusCTreeNode *node;
      NautilusCTreeNode *child;
      gint i;

      node = NAUTILUS_CTREE_NODE (clist->row_list);
      for (i = 0; i < clist->rows; i++)
	{
	  if (!NAUTILUS_CTREE_ROW (node)->is_leaf &&
	      !NAUTILUS_CTREE_ROW (node)->expanded)
	    for (child = NAUTILUS_CTREE_ROW (node)->children; 
		 child != NULL;
		 child = NAUTILUS_CTREE_ROW (child)->sibling)
	      nautilus_ctree_pre_recursive(ctree, child, ctree_detach_styles, NULL);
	  node = NAUTILUS_CTREE_NODE_NEXT (node);
	}
    }

  gdk_gc_destroy (ctree->lines_gc);
}

static gint
nautilus_ctree_button_press (GtkWidget *widget, GdkEventButton *event)
{
	NautilusCTree *ctree;
	GtkCList *clist;
	gint button_actions;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CTREE (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	ctree = NAUTILUS_CTREE (widget);
	clist = GTK_CLIST (widget);

	button_actions = clist->button_actions[event->button - 1];

	if (button_actions == GTK_BUTTON_IGNORED) {
    		return FALSE;
    	}

	if (event->window == clist->clist_window)
	{
		NautilusCTreeNode *work;
		gint x;
		gint y;
		gint row;
		gint column;

		x = event->x;
		y = event->y;

		if (!gtk_clist_get_selection_info (clist, x, y, &row, &column)) {
			return FALSE;
		}

      		work = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, row));
	  
      		if (button_actions & GTK_BUTTON_EXPANDS && 
      		    (!NAUTILUS_CTREE_ROW (work)->is_leaf  &&
		     (event->type == GDK_2BUTTON_PRESS ||
		      ctree_is_hot_spot (ctree, work, row, x, y))))
		{
	  		if (NAUTILUS_CTREE_ROW (work)->expanded) {
	    			nautilus_ctree_collapse (ctree, work);
	    		} else {
	    			nautilus_ctree_expand (ctree, work);
	    		}
	  		return FALSE;
		}
    	}
    	
	return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
}

static void
draw_drag_highlight (GtkCList        *clist,
		     GtkCListRow     *dest_row,
		     gint             dest_row_number,
		     GtkCListDragPos  drag_pos)
{
  NautilusCTree *ctree;
  GdkPoint points[4];
  gint level;
  gint i;
  gint y = 0;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  ctree = NAUTILUS_CTREE (clist);

  level = ((NautilusCTreeRow *)(dest_row))->level;

  y = ROW_TOP_YPIXEL (clist, dest_row_number) - 1;

  switch (drag_pos)
    {
    case GTK_CLIST_DRAG_NONE:
      break;
    case GTK_CLIST_DRAG_AFTER:
      y += clist->row_height + 1;
    case GTK_CLIST_DRAG_BEFORE:
      
      if (clist->column[ctree->tree_column].visible)
	switch (clist->column[ctree->tree_column].justification)
	  {
	  case GTK_JUSTIFY_CENTER:
	  case GTK_JUSTIFY_FILL:
	  case GTK_JUSTIFY_LEFT:
	    if (ctree->tree_column > 0)
	      gdk_draw_line (clist->clist_window, clist->xor_gc, 
			     COLUMN_LEFT_XPIXEL(clist, 0), y,
			     COLUMN_LEFT_XPIXEL(clist, ctree->tree_column - 1)+
			     clist->column[ctree->tree_column - 1].area.width,
			     y);

	    gdk_draw_line (clist->clist_window, clist->xor_gc, 
			   COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) + 
			   ctree->tree_indent * level -
			   (ctree->tree_indent - PM_SIZE) / 2, y,
			   GTK_WIDGET (ctree)->allocation.width, y);
	    break;
	  case GTK_JUSTIFY_RIGHT:
	    if (ctree->tree_column < clist->columns - 1)
	      gdk_draw_line (clist->clist_window, clist->xor_gc, 
			     COLUMN_LEFT_XPIXEL(clist, ctree->tree_column + 1),
			     y,
			     COLUMN_LEFT_XPIXEL(clist, clist->columns - 1) +
			     clist->column[clist->columns - 1].area.width, y);
      
	    gdk_draw_line (clist->clist_window, clist->xor_gc, 
			   0, y, COLUMN_LEFT_XPIXEL(clist, ctree->tree_column)
			   + clist->column[ctree->tree_column].area.width -
			   ctree->tree_indent * level +
			   (ctree->tree_indent - PM_SIZE) / 2, y);
	    break;
	  }
      else
	gdk_draw_line (clist->clist_window, clist->xor_gc, 
		       0, y, clist->clist_window_width, y);
      break;
    case GTK_CLIST_DRAG_INTO:
      y = ROW_TOP_YPIXEL (clist, dest_row_number) + clist->row_height;

      if (clist->column[ctree->tree_column].visible)
	switch (clist->column[ctree->tree_column].justification)
	  {
	  case GTK_JUSTIFY_CENTER:
	  case GTK_JUSTIFY_FILL:
	  case GTK_JUSTIFY_LEFT:
	    points[0].x =  COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) + 
	      ctree->tree_indent * level - (ctree->tree_indent - PM_SIZE) / 2;
	    points[0].y = y;
	    points[3].x = points[0].x;
	    points[3].y = y - clist->row_height - 1;
	    points[1].x = clist->clist_window_width - 1;
	    points[1].y = points[0].y;
	    points[2].x = points[1].x;
	    points[2].y = points[3].y;

	    for (i = 0; i < 3; i++)
	      gdk_draw_line (clist->clist_window, clist->xor_gc,
			     points[i].x, points[i].y,
			     points[i+1].x, points[i+1].y);

	    if (ctree->tree_column > 0)
	      {
		points[0].x = COLUMN_LEFT_XPIXEL(clist,
						 ctree->tree_column - 1) +
		  clist->column[ctree->tree_column - 1].area.width ;
		points[0].y = y;
		points[3].x = points[0].x;
		points[3].y = y - clist->row_height - 1;
		points[1].x = 0;
		points[1].y = points[0].y;
		points[2].x = 0;
		points[2].y = points[3].y;

		for (i = 0; i < 3; i++)
		  gdk_draw_line (clist->clist_window, clist->xor_gc,
				 points[i].x, points[i].y, points[i+1].x, 
				 points[i+1].y);
	      }
	    break;
	  case GTK_JUSTIFY_RIGHT:
	    points[0].x =  COLUMN_LEFT_XPIXEL(clist, ctree->tree_column) - 
	      ctree->tree_indent * level + (ctree->tree_indent - PM_SIZE) / 2 +
	      clist->column[ctree->tree_column].area.width;
	    points[0].y = y;
	    points[3].x = points[0].x;
	    points[3].y = y - clist->row_height - 1;
	    points[1].x = 0;
	    points[1].y = points[0].y;
	    points[2].x = 0;
	    points[2].y = points[3].y;

	    for (i = 0; i < 3; i++)
	      gdk_draw_line (clist->clist_window, clist->xor_gc,
			     points[i].x, points[i].y,
			     points[i+1].x, points[i+1].y);

	    if (ctree->tree_column < clist->columns - 1)
	      {
		points[0].x = COLUMN_LEFT_XPIXEL(clist, ctree->tree_column +1);
		points[0].y = y;
		points[3].x = points[0].x;
		points[3].y = y - clist->row_height - 1;
		points[1].x = clist->clist_window_width - 1;
		points[1].y = points[0].y;
		points[2].x = points[1].x;
		points[2].y = points[3].y;

		for (i = 0; i < 3; i++)
		  gdk_draw_line (clist->clist_window, clist->xor_gc,
				 points[i].x, points[i].y,
				 points[i+1].x, points[i+1].y);
	      }
	    break;
	  }
      else
	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			    0, y - clist->row_height,
			    clist->clist_window_width - 1, clist->row_height);
      break;
    }
}

static NautilusCTreeRow *
nautilus_ctree_row_at (NautilusCTree *ctree, int y)
{
	int row_index, column_index;

	y -= (GTK_CONTAINER (ctree)->border_width +
		GTK_WIDGET (ctree)->style->klass->ythickness +
		GTK_CLIST (ctree)->column_title_area.height);
	
	if (!gtk_clist_get_selection_info (GTK_CLIST (ctree), 10, y, &row_index, &column_index)) {
		return NULL;
	}
	
	return g_list_nth (GTK_CLIST (ctree)->row_list, row_index)->data;
}


static void
get_cell_rectangle (GtkCList *clist, int row_index, int column_index, GdkRectangle *result)
{
	result->x = clist->column[column_index].area.x + clist->hoffset;
	result->y = ROW_TOP_YPIXEL (clist, row_index);
	result->width = clist->column[column_index].area.width;
	result->height = clist->row_height;
}


void 
nautilus_ctree_set_prelight (NautilusCTree      *ctree,
			     int                 y)
{
	GtkCList *clist;
	NautilusCTreeRow *row, *last_row;

	g_return_if_fail (ctree != NULL);
	g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

	clist = GTK_CLIST (ctree);

	row = NULL;

	if (y >= 0) { 
		row = nautilus_ctree_row_at (ctree, y);
	}
	
	if (row != ctree->dnd_prelighted_row) {
		last_row = ctree->dnd_prelighted_row;
		ctree->dnd_prelighted_row = row;

		{
			GdkRectangle rect;
			int row_index;
			/* Redraw old cell */
			if (last_row != NULL) {
				row_index = g_list_index (clist->row_list, last_row);
				get_cell_rectangle (clist, row_index, 0, &rect);
				gtk_widget_draw (GTK_WIDGET (clist), &rect);			
			}

			/* Draw new cell */
			if (ctree->dnd_prelighted_row != NULL) {
				row_index = g_list_index (clist->row_list, ctree->dnd_prelighted_row);
				get_cell_rectangle (clist, row_index, 0, &rect);
				gtk_widget_draw (GTK_WIDGET (clist), &rect);
			}
		}
	}
}

static gint
draw_cell_pixmap (GdkWindow    *window,
		  GdkRectangle *clip_rectangle,
		  GdkGC        *fg_gc,
		  GdkPixmap    *pixmap,
		  GdkBitmap    *mask,
		  gint          x,
		  gint          y,
		  gint          width,
		  gint          height)
{
  gint xsrc = 0;
  gint ysrc = 0;

  if (mask)
    {
      gdk_gc_set_clip_mask (fg_gc, mask);
      gdk_gc_set_clip_origin (fg_gc, x, y);
    }
  if (x < clip_rectangle->x)
    {
      xsrc = clip_rectangle->x - x;
      width -= xsrc;
      x = clip_rectangle->x;
    }
  if (x + width > clip_rectangle->x + clip_rectangle->width)
    width = clip_rectangle->x + clip_rectangle->width - x;

  if (y < clip_rectangle->y)
    {
      ysrc = clip_rectangle->y - y;
      height -= ysrc;
      y = clip_rectangle->y;
    }
  if (y + height > clip_rectangle->y + clip_rectangle->height)
    height = clip_rectangle->y + clip_rectangle->height - y;

  if (width > 0 && height > 0)
    gdk_draw_pixmap (window, fg_gc, pixmap, xsrc, ysrc, x, y, width, height);

  if (mask)
    {
      gdk_gc_set_clip_rectangle (fg_gc, NULL);
      gdk_gc_set_clip_origin (fg_gc, 0, 0);
    }

  return x + MAX (width, 0);
}

static void
get_cell_style (GtkCList     *clist,
		GtkCListRow  *clist_row,
		gint          state,
		gint          column,
		GtkStyle    **style,
		GdkGC       **fg_gc,
		GdkGC       **bg_gc)
{
  gint fg_state;

  if ((state == GTK_STATE_NORMAL) &&
      (GTK_WIDGET (clist)->state == GTK_STATE_INSENSITIVE))
    fg_state = GTK_STATE_INSENSITIVE;
  else
    fg_state = state;

  if (clist_row->cell[column].style)
    {
      if (style)
	*style = clist_row->cell[column].style;
      if (fg_gc)
	*fg_gc = clist_row->cell[column].style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = clist_row->cell[column].style->bg_gc[state];
	else
	  *bg_gc = clist_row->cell[column].style->base_gc[state];
      }
    }
  else if (clist_row->style)
    {
      if (style)
	*style = clist_row->style;
      if (fg_gc)
	*fg_gc = clist_row->style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = clist_row->style->bg_gc[state];
	else
	  *bg_gc = clist_row->style->base_gc[state];
      }
    }
  else
    {
      if (style)
	*style = GTK_WIDGET (clist)->style;
      if (fg_gc)
	*fg_gc = GTK_WIDGET (clist)->style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = GTK_WIDGET (clist)->style->bg_gc[state];
	else
	  *bg_gc = GTK_WIDGET (clist)->style->base_gc[state];
      }

      if (state != GTK_STATE_SELECTED)
	{
	  if (fg_gc && clist_row->fg_set)
	    *fg_gc = clist->fg_gc;
	  if (bg_gc && clist_row->bg_set)
	    *bg_gc = clist->bg_gc;
	}
    }
}

static gint
nautilus_ctree_draw_expander (NautilusCTree *ctree, NautilusCTreeRow *ctree_row, GtkStyle *style,
			      GdkRectangle *clip_rectangle, gint x)
{
	GtkCList *clist;
	GdkPoint points[3];
	gint justification_factor;
	gint y;
	NautilusCTreeNode *node;

	clist = GTK_CLIST (ctree);
	if (clist->column[ctree->tree_column].justification == GTK_JUSTIFY_RIGHT)
		justification_factor = -1;
	else
		justification_factor = 1;

	y = (clip_rectangle->y + (clip_rectangle->height - PM_SIZE) / 2 - (clip_rectangle->height + 1) % 2);

  	if (ctree_row->is_leaf) {
		return x + justification_factor * (PM_SIZE + 3);	  
	}

	gdk_gc_set_clip_rectangle (style->fg_gc[GTK_STATE_NORMAL], clip_rectangle);
	gdk_gc_set_clip_rectangle (style->base_gc[GTK_STATE_NORMAL], clip_rectangle);

	if (ctree_row->expanded)
	{
  		points[0].x = x;
  		points[0].y = y + (PM_SIZE + 2) / 6;
  		points[1].x = points[0].x + justification_factor * (PM_SIZE + 2);
  		points[1].y = points[0].y;
  		points[2].x = (points[0].x + justification_factor * (PM_SIZE + 2) / 2);
  		points[2].y = y + 2 * (PM_SIZE + 2) / 3;
	} else {
  		points[0].x = x + justification_factor * ((PM_SIZE + 2) / 6 + 2);
  		points[0].y = y - 1;
  		points[1].x = points[0].x;
  		points[1].y = points[0].y + (PM_SIZE + 2);
  		points[2].x = (points[0].x + justification_factor * (2 * (PM_SIZE + 2) / 3 - 1));
  		points[2].y = points[0].y + (PM_SIZE + 2) / 2;
	}

	gdk_draw_polygon (clist->clist_window, style->base_gc[GTK_STATE_NORMAL], TRUE, points, 3);
	if (ctree_row->mouse_down) {
			gdk_draw_polygon (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL], !ctree_row->in_hotspot, points, 3);
	} else {
		node = nautilus_ctree_find_node_ptr (ctree, ctree_row);
		if (node != NULL) {
			if (node == ctree->prelight_node) {
				/* Draw prelight state */
				gdk_draw_polygon (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL], FALSE, points, 3);
			} else {
				gdk_draw_polygon (clist->clist_window, style->fg_gc[GTK_STATE_NORMAL], TRUE, points, 3);
			}
		}
	}
	
	x += justification_factor * (PM_SIZE + 3);

	gdk_gc_set_clip_rectangle (style->fg_gc[GTK_STATE_NORMAL], NULL);
	gdk_gc_set_clip_rectangle (style->base_gc[GTK_STATE_NORMAL], NULL);

	return x;
}

static gint
nautilus_ctree_draw_lines (NautilusCTree     *ctree,
		      NautilusCTreeRow  *ctree_row,
		      gint          row,
		      gint          column,
		      gint          state,
		      GdkRectangle *clip_rectangle,
		      GdkRectangle *cell_rectangle,
		      GdkRectangle *crect,
		      GdkRectangle *area,
		      GtkStyle     *style)
{
  GtkCList *clist;
  NautilusCTreeNode *node;
  NautilusCTreeNode *parent;
  GdkRectangle tree_rectangle;
  GdkRectangle tc_rectangle;
  GdkGC *bg_gc;
  gint offset;
  gint offset_x;
  gint offset_y;
  gint xcenter;
  gint ycenter;
  gint next_level;
  gint column_right;
  gint column_left;
  gint justify_right;
  gint justification_factor;
  
  clist = GTK_CLIST (ctree);
  ycenter = clip_rectangle->y + (clip_rectangle->height / 2);
  justify_right = (clist->column[column].justification == GTK_JUSTIFY_RIGHT);

  if (justify_right)
    {
      offset = (clip_rectangle->x + clip_rectangle->width - 1 -
		ctree->tree_indent * (ctree_row->level - 1));
      justification_factor = -1;
    }
  else
    {
      offset = clip_rectangle->x + ctree->tree_indent * (ctree_row->level - 1);
      justification_factor = 1;
    }

  switch (ctree->line_style)
    {
    case NAUTILUS_CTREE_LINES_NONE:
      break;
    case NAUTILUS_CTREE_LINES_TABBED:
      xcenter = offset + justification_factor * TAB_SIZE;

      column_right = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) +
		      clist->column[ctree->tree_column].area.width +
		      COLUMN_INSET);
      column_left = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) -
		     COLUMN_INSET - CELL_SPACING);

      if (area)
	{
	  tree_rectangle.y = crect->y;
	  tree_rectangle.height = crect->height;

	  if (justify_right)
	    {
	      tree_rectangle.x = xcenter;
	      tree_rectangle.width = column_right - xcenter;
	    }
	  else
	    {
	      tree_rectangle.x = column_left;
	      tree_rectangle.width = xcenter - column_left;
	    }

	  if (!gdk_rectangle_intersect (area, &tree_rectangle, &tc_rectangle))
	    {
	      offset += justification_factor * 3;
	      break;
	    }
	}

      gdk_gc_set_clip_rectangle (ctree->lines_gc, crect);

      next_level = ctree_row->level;

      if (!ctree_row->sibling || (ctree_row->children && ctree_row->expanded))
	{
	  node = nautilus_ctree_find_node_ptr (ctree, ctree_row);
	  if (NAUTILUS_CTREE_NODE_NEXT (node))
	    next_level = NAUTILUS_CTREE_ROW (NAUTILUS_CTREE_NODE_NEXT (node))->level;
	  else
	    next_level = 0;
	}

      if (ctree->tree_indent > 0)
	{
	  node = ctree_row->parent;
	  while (node)
	    {
	      xcenter -= (justification_factor * ctree->tree_indent);

	      if ((justify_right && xcenter < column_left) ||
		  (!justify_right && xcenter > column_right))
		{
		  node = NAUTILUS_CTREE_ROW (node)->parent;
		  continue;
		}

	      tree_rectangle.y = cell_rectangle->y;
	      tree_rectangle.height = cell_rectangle->height;
	      if (justify_right)
		{
		  tree_rectangle.x = MAX (xcenter - ctree->tree_indent + 1,
					  column_left);
		  tree_rectangle.width = MIN (xcenter - column_left,
					      ctree->tree_indent);
		}
	      else
		{
		  tree_rectangle.x = xcenter;
		  tree_rectangle.width = MIN (column_right - xcenter,
					      ctree->tree_indent);
		}

	      if (!area || gdk_rectangle_intersect (area, &tree_rectangle,
						    &tc_rectangle))
		{
		  get_cell_style (clist, &NAUTILUS_CTREE_ROW (node)->row,
				  state, column, NULL, NULL, &bg_gc);

		  if (bg_gc == clist->bg_gc)
		    gdk_gc_set_foreground
		      (clist->bg_gc, &NAUTILUS_CTREE_ROW (node)->row.background);

		  if (!area)
		    gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
					tree_rectangle.x,
					tree_rectangle.y,
					tree_rectangle.width,
					tree_rectangle.height);
		  else 
		    gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
					tc_rectangle.x,
					tc_rectangle.y,
					tc_rectangle.width,
					tc_rectangle.height);
		}
	      if (next_level > NAUTILUS_CTREE_ROW (node)->level)
		gdk_draw_line (clist->clist_window, ctree->lines_gc,
			       xcenter, crect->y,
			       xcenter, crect->y + crect->height);
	      else
		{
		  gint width;

		  offset_x = MIN (ctree->tree_indent, 2 * TAB_SIZE);
		  width = offset_x / 2 + offset_x % 2;

		  parent = NAUTILUS_CTREE_ROW (node)->parent;

		  tree_rectangle.y = ycenter;
		  tree_rectangle.height = (cell_rectangle->y - ycenter +
					   cell_rectangle->height);

		  if (justify_right)
		    {
		      tree_rectangle.x = MAX(xcenter + 1 - width, column_left);
		      tree_rectangle.width = MIN (xcenter + 1 - column_left,
						  width);
		    }
		  else
		    {
		      tree_rectangle.x = xcenter;
		      tree_rectangle.width = MIN (column_right - xcenter,
						  width);
		    }

		  if (!area ||
		      gdk_rectangle_intersect (area, &tree_rectangle,
					       &tc_rectangle))
		    {
		      if (parent)
			{
			  get_cell_style (clist, &NAUTILUS_CTREE_ROW (parent)->row,
					  state, column, NULL, NULL, &bg_gc);
			  if (bg_gc == clist->bg_gc)
			    gdk_gc_set_foreground
			      (clist->bg_gc,
			       &NAUTILUS_CTREE_ROW (parent)->row.background);
			}
		      else if (state == GTK_STATE_SELECTED)
			bg_gc = style->base_gc[state];
		      else
			bg_gc = GTK_WIDGET (clist)->style->base_gc[state];

		      if (!area)
			gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
					    tree_rectangle.x,
					    tree_rectangle.y,
					    tree_rectangle.width,
					    tree_rectangle.height);
		      else
			gdk_draw_rectangle (clist->clist_window,
					    bg_gc, TRUE,
					    tc_rectangle.x,
					    tc_rectangle.y,
					    tc_rectangle.width,
					    tc_rectangle.height);
		    }

		  get_cell_style (clist, &NAUTILUS_CTREE_ROW (node)->row,
				  state, column, NULL, NULL, &bg_gc);
		  if (bg_gc == clist->bg_gc)
		    gdk_gc_set_foreground
		      (clist->bg_gc, &NAUTILUS_CTREE_ROW (node)->row.background);

		  gdk_gc_set_clip_rectangle (bg_gc, crect);
		  gdk_draw_arc (clist->clist_window, bg_gc, TRUE,
				xcenter - (justify_right * offset_x),
				cell_rectangle->y,
				offset_x, clist->row_height,
				(180 + (justify_right * 90)) * 64, 90 * 64);
		  gdk_gc_set_clip_rectangle (bg_gc, NULL);

		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 xcenter, cell_rectangle->y, xcenter, ycenter);

		  if (justify_right)
		    gdk_draw_arc (clist->clist_window, ctree->lines_gc, FALSE,
				  xcenter - offset_x, cell_rectangle->y,
				  offset_x, clist->row_height,
				  270 * 64, 90 * 64);
		  else
		    gdk_draw_arc (clist->clist_window, ctree->lines_gc, FALSE,
				  xcenter, cell_rectangle->y,
				  offset_x, clist->row_height,
				  180 * 64, 90 * 64);
		}
	      node = NAUTILUS_CTREE_ROW (node)->parent;
	    }
	}

      if (state != GTK_STATE_SELECTED)
	{
	  tree_rectangle.y = clip_rectangle->y;
	  tree_rectangle.height = clip_rectangle->height;
	  tree_rectangle.width = COLUMN_INSET + CELL_SPACING +
	    MIN (clist->column[ctree->tree_column].area.width + COLUMN_INSET,
		 TAB_SIZE);

	  if (justify_right)
	    tree_rectangle.x = MAX (xcenter + 1, column_left);
	  else
	    tree_rectangle.x = column_left;

	  if (!area)
	    gdk_draw_rectangle (clist->clist_window,
				GTK_WIDGET
				(ctree)->style->base_gc[GTK_STATE_NORMAL],
				TRUE,
				tree_rectangle.x,
				tree_rectangle.y,
				tree_rectangle.width,
				tree_rectangle.height);
	  else if (gdk_rectangle_intersect (area, &tree_rectangle,
					    &tc_rectangle))
	    gdk_draw_rectangle (clist->clist_window,
				GTK_WIDGET
				(ctree)->style->base_gc[GTK_STATE_NORMAL],
				TRUE,
				tc_rectangle.x,
				tc_rectangle.y,
				tc_rectangle.width,
				tc_rectangle.height);
	}

      xcenter = offset + (justification_factor * ctree->tree_indent / 2);

      get_cell_style (clist, &ctree_row->row, state, column, NULL, NULL,
		      &bg_gc);
      if (bg_gc == clist->bg_gc)
	gdk_gc_set_foreground (clist->bg_gc, &ctree_row->row.background);

      gdk_gc_set_clip_rectangle (bg_gc, crect);
      if (ctree_row->is_leaf)
	{
	  GdkPoint points[6];

	  points[0].x = offset + justification_factor * TAB_SIZE;
	  points[0].y = cell_rectangle->y;

	  points[1].x = points[0].x - justification_factor * 4;
	  points[1].y = points[0].y;

	  points[2].x = points[1].x - justification_factor * 2;
	  points[2].y = points[1].y + 3;

	  points[3].x = points[2].x;
	  points[3].y = points[2].y + clist->row_height - 5;

	  points[4].x = points[3].x + justification_factor * 2;
	  points[4].y = points[3].y + 3;

	  points[5].x = points[4].x + justification_factor * 4;
	  points[5].y = points[4].y;

	  gdk_draw_polygon (clist->clist_window, bg_gc, TRUE, points, 6);
	  gdk_draw_lines (clist->clist_window, ctree->lines_gc, points, 6);
	}
      else 
	{
	  gdk_draw_arc (clist->clist_window, bg_gc, TRUE,
			offset - (justify_right * 2 * TAB_SIZE),
			cell_rectangle->y,
			2 * TAB_SIZE, clist->row_height,
			(90 + (180 * justify_right)) * 64, 180 * 64);
	  gdk_draw_arc (clist->clist_window, ctree->lines_gc, FALSE,
			offset - (justify_right * 2 * TAB_SIZE),
			cell_rectangle->y,
			2 * TAB_SIZE, clist->row_height,
			(90 + (180 * justify_right)) * 64, 180 * 64);
	}
      gdk_gc_set_clip_rectangle (bg_gc, NULL);
      gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);

      offset += justification_factor * 3;
      break;
    default:
      xcenter = offset + justification_factor * PM_SIZE / 2;

      if (area)
	{
	  tree_rectangle.y = crect->y;
	  tree_rectangle.height = crect->height;

	  if (justify_right)
	    {
	      tree_rectangle.x = xcenter - PM_SIZE / 2 - 2;
	      tree_rectangle.width = (clip_rectangle->x +
				      clip_rectangle->width -tree_rectangle.x);
	    }
	  else
	    {
	      tree_rectangle.x = clip_rectangle->x + PM_SIZE / 2;
	      tree_rectangle.width = (xcenter + PM_SIZE / 2 + 2 -
				      clip_rectangle->x);
	    }

	  if (!gdk_rectangle_intersect (area, &tree_rectangle, &tc_rectangle))
	    break;
	}

      offset_x = 1;
      offset_y = 0;
      if (ctree->line_style == NAUTILUS_CTREE_LINES_DOTTED)
	{
	  offset_x += abs((clip_rectangle->x + clist->hoffset) % 2);
	  offset_y  = abs((cell_rectangle->y + clist->voffset) % 2);
	}

      clip_rectangle->y--;
      clip_rectangle->height++;
      gdk_gc_set_clip_rectangle (ctree->lines_gc, clip_rectangle);
      gdk_draw_line (clist->clist_window, ctree->lines_gc,
		     xcenter,
		     (ctree->show_stub || clist->row_list->data != ctree_row) ?
		     cell_rectangle->y + offset_y : ycenter,
		     xcenter,
		     (ctree_row->sibling) ? crect->y +crect->height : ycenter);

      gdk_draw_line (clist->clist_window, ctree->lines_gc,
		     xcenter + (justification_factor * offset_x), ycenter,
		     xcenter + (justification_factor * (PM_SIZE / 2 + 2)),
		     ycenter);

      node = ctree_row->parent;
      while (node)
	{
	  xcenter -= (justification_factor * ctree->tree_indent);

	  if (NAUTILUS_CTREE_ROW (node)->sibling)
	    gdk_draw_line (clist->clist_window, ctree->lines_gc, 
			   xcenter, cell_rectangle->y + offset_y,
			   xcenter, crect->y + crect->height);
	  node = NAUTILUS_CTREE_ROW (node)->parent;
	}
      gdk_gc_set_clip_rectangle (ctree->lines_gc, NULL);
      clip_rectangle->y++;
      clip_rectangle->height--;
      break;
    }
  return offset;
}

static void
draw_row (GtkCList     *clist,
	  GdkRectangle *area,
	  gint          row,
	  GtkCListRow  *clist_row)
{
  GtkWidget *widget;
  NautilusCTree  *ctree;
  GdkRectangle *rect;
  GdkRectangle *crect;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle; 
  GdkRectangle clip_rectangle;
  GdkRectangle intersect_rectangle;
  gint last_column;
  gint column_left = 0;
  gint column_right = 0;
  gint offset = 0;
  gint state;
  gint i;

  g_return_if_fail (clist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (clist) || row < 0 || row >= clist->rows)
    return;

  widget = GTK_WIDGET (clist);
  ctree  = NAUTILUS_CTREE  (clist);

  /* if the function is passed the pointer to the row instead of null,
   * it avoids this expensive lookup */
  if (!clist_row)
    clist_row = (g_list_nth (clist->row_list, row))->data;

  /* rectangle of the entire row */
  row_rectangle.x = 0;
  row_rectangle.y = ROW_TOP_YPIXEL (clist, row);
  row_rectangle.width = clist->clist_window_width;
  row_rectangle.height = clist->row_height;

  /* rectangle of the cell spacing above the row */
  cell_rectangle.x = 0;
  cell_rectangle.y = row_rectangle.y - CELL_SPACING;
  cell_rectangle.width = row_rectangle.width;
  cell_rectangle.height = CELL_SPACING;

  /* rectangle used to clip drawing operations, its y and height
   * positions only need to be set once, so we set them once here. 
   * the x and width are set withing the drawing loop below once per
   * column */
  clip_rectangle.y = row_rectangle.y;
  clip_rectangle.height = row_rectangle.height;

  if (clist_row->state == GTK_STATE_NORMAL)
    {
      if (clist_row->fg_set)
	gdk_gc_set_foreground (clist->fg_gc, &clist_row->foreground);
      if (clist_row->bg_set)
	gdk_gc_set_foreground (clist->bg_gc, &clist_row->background);
    }
  
  state = clist_row->state;

  gdk_gc_set_foreground (ctree->lines_gc,
			 &widget->style->fg[clist_row->state]);

  /* draw the cell borders */
  if (area)
    {
      rect = &intersect_rectangle;
      crect = &intersect_rectangle;

      if (gdk_rectangle_intersect (area, &cell_rectangle, crect))
	gdk_draw_rectangle (clist->clist_window,
			    widget->style->base_gc[GTK_STATE_ACTIVE], TRUE,
			    crect->x, crect->y, crect->width, crect->height);
    }
  else
    {
      rect = &clip_rectangle;
      crect = &cell_rectangle;

      gdk_draw_rectangle (clist->clist_window,
			  widget->style->base_gc[GTK_STATE_ACTIVE], TRUE,
			  crect->x, crect->y, crect->width, crect->height);
    }

  /* horizontal black lines */
  if (ctree->line_style == NAUTILUS_CTREE_LINES_TABBED)
    { 

      column_right = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) +
		      clist->column[ctree->tree_column].area.width +
		      COLUMN_INSET);
      column_left = (COLUMN_LEFT_XPIXEL (clist, ctree->tree_column) -
		     COLUMN_INSET - (ctree->tree_column != 0) * CELL_SPACING);

      switch (clist->column[ctree->tree_column].justification)
	{
	case GTK_JUSTIFY_CENTER:
	case GTK_JUSTIFY_FILL:
	case GTK_JUSTIFY_LEFT:
	  offset = (column_left + ctree->tree_indent *
		    (((NautilusCTreeRow *)clist_row)->level - 1));

	  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
			 MIN (offset + TAB_SIZE, column_right),
			 cell_rectangle.y,
			 clist->clist_window_width, cell_rectangle.y);
	  break;
	case GTK_JUSTIFY_RIGHT:
	  offset = (column_right - 1 - ctree->tree_indent *
		    (((NautilusCTreeRow *)clist_row)->level - 1));

	  gdk_draw_line (clist->clist_window, ctree->lines_gc,
			 -1, cell_rectangle.y,
			 MAX (offset - TAB_SIZE, column_left),
			 cell_rectangle.y);
	  break;
	}
    }

  /* the last row has to clear its bottom cell spacing too */
  if (clist_row == clist->row_list_end->data)
    {
      cell_rectangle.y += clist->row_height + CELL_SPACING;

      if (!area || gdk_rectangle_intersect (area, &cell_rectangle, crect))
	{
	  gdk_draw_rectangle (clist->clist_window,
			      widget->style->base_gc[GTK_STATE_ACTIVE], TRUE,
			      crect->x, crect->y, crect->width, crect->height);

	  /* horizontal black lines */
	  if (ctree->line_style == NAUTILUS_CTREE_LINES_TABBED)
	    { 
	      switch (clist->column[ctree->tree_column].justification)
		{
		case GTK_JUSTIFY_CENTER:
		case GTK_JUSTIFY_FILL:
		case GTK_JUSTIFY_LEFT:
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 MIN (column_left + TAB_SIZE + COLUMN_INSET +
				      (((NautilusCTreeRow *)clist_row)->level > 1) *
				      MIN (ctree->tree_indent / 2, TAB_SIZE),
				      column_right),
				 cell_rectangle.y,
				 clist->clist_window_width, cell_rectangle.y);
		  break;
		case GTK_JUSTIFY_RIGHT:
		  gdk_draw_line (clist->clist_window, ctree->lines_gc, 
				 -1, cell_rectangle.y,
				 MAX (column_right - TAB_SIZE - 1 -
				      COLUMN_INSET -
				      (((NautilusCTreeRow *)clist_row)->level > 1) *
				      MIN (ctree->tree_indent / 2, TAB_SIZE),
				      column_left - 1), cell_rectangle.y);
		  break;
		}
	    }
	}
    }	  

  for (last_column = clist->columns - 1;
       last_column >= 0 && !clist->column[last_column].visible; last_column--)
    ;

  /* iterate and draw all the columns (row cells) and draw their contents */
  for (i = 0; i < clist->columns; i++)
    {
      GtkStyle *style;
      GdkGC *fg_gc; 
      GdkGC *bg_gc;

      gint width;
      gint height;
      gint pixmap_width;
      gint string_width;
      gint old_offset;
      gint row_center_offset;

      if (!clist->column[i].visible)
	continue;

      get_cell_style (clist, clist_row, state, i, &style, &fg_gc, &bg_gc);

      /* calculate clipping region */
      clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
      clip_rectangle.width = clist->column[i].area.width;

      cell_rectangle.x = clip_rectangle.x - COLUMN_INSET - CELL_SPACING;
      cell_rectangle.width = (clip_rectangle.width + 2 * COLUMN_INSET +
			      (1 + (i == last_column)) * CELL_SPACING);
      cell_rectangle.y = clip_rectangle.y;
      cell_rectangle.height = clip_rectangle.height;

      string_width = 0;
      pixmap_width = 0;

      if (area && !gdk_rectangle_intersect (area, &cell_rectangle,
					    &intersect_rectangle))
	{
	  if (i != ctree->tree_column)
	    continue;
	}
      else
	{
	  gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
			      crect->x, crect->y, crect->width, crect->height);

	  /* calculate real width for column justification */
	  switch (clist_row->cell[i].type)
	    {
	    case GTK_CELL_TEXT:
	      width = gdk_string_width
		(style->font, GTK_CELL_TEXT (clist_row->cell[i])->text);
	      break;
	    case GTK_CELL_PIXMAP:
	      gdk_window_get_size
		(GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap, &pixmap_width,
		 &height);
	      width = pixmap_width;
	      break;
	    case GTK_CELL_PIXTEXT:
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		gdk_window_get_size
		  (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
		   &pixmap_width, &height);

	      width = pixmap_width;

	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->text)
		{
		  string_width = gdk_string_width
		    (style->font, GTK_CELL_PIXTEXT (clist_row->cell[i])->text);
		  width += string_width;
		}

	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->text &&
		  GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		width +=  GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;

	      if (i == ctree->tree_column)
		width += (ctree->tree_indent *
			  ((NautilusCTreeRow *)clist_row)->level);
	      break;
	    default:
	      continue;
	      break;
	    }

	  switch (clist->column[i].justification)
	    {
	    case GTK_JUSTIFY_LEFT:
	      offset = clip_rectangle.x + clist_row->cell[i].horizontal;
	      break;
	    case GTK_JUSTIFY_RIGHT:
	      offset = (clip_rectangle.x + clist_row->cell[i].horizontal +
			clip_rectangle.width - width);
	      break;
	    case GTK_JUSTIFY_CENTER:
	    case GTK_JUSTIFY_FILL:
	      offset = (clip_rectangle.x + clist_row->cell[i].horizontal +
			(clip_rectangle.width / 2) - (width / 2));
	      break;
	    };

	  if (i != ctree->tree_column)
	    {
	      offset += clist_row->cell[i].horizontal;
	      switch (clist_row->cell[i].type)
		{
		case GTK_CELL_PIXMAP:
			offset = draw_cell_pixmap (clist->clist_window, &cell_rectangle, fg_gc,
						   GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
						   GTK_CELL_PIXMAP (clist_row->cell[i])->mask,
						   offset,
						   clip_rectangle.y + clist_row->cell[i].vertical +
						   (clip_rectangle.height - height) / 2,
						   pixmap_width, height);
		  break;
		case GTK_CELL_PIXTEXT:
			offset = draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
						   GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
						   GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
						   offset,
						   clip_rectangle.y + clist_row->cell[i].vertical +
						   (clip_rectangle.height - height) / 2,
						   pixmap_width, height);

		  offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
		case GTK_CELL_TEXT:
		  row_center_offset = ((clist->row_height -
					(style->font->ascent
					 + style->font->descent)) / 2
					 + style->font->ascent);

		  gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
		  gdk_draw_string
		    (clist->clist_window, style->font, fg_gc,
		     offset,
		     row_rectangle.y + row_center_offset +
		     clist_row->cell[i].vertical,
		     (clist_row->cell[i].type == GTK_CELL_PIXTEXT) ?
		     GTK_CELL_PIXTEXT (clist_row->cell[i])->text :
		     GTK_CELL_TEXT (clist_row->cell[i])->text);
		  gdk_gc_set_clip_rectangle (fg_gc, NULL);
		  break;
		default:
		  break;
		}
	      continue;
	    }
	}

      if (bg_gc == clist->bg_gc)
	gdk_gc_set_background (ctree->lines_gc, &clist_row->background);

      /* draw ctree->tree_column */
      cell_rectangle.y -= CELL_SPACING;
      cell_rectangle.height += CELL_SPACING;

      if (area && !gdk_rectangle_intersect (area, &cell_rectangle,
					    &intersect_rectangle))
	continue;

      /* draw lines */
      offset = nautilus_ctree_draw_lines (ctree, (NautilusCTreeRow *)clist_row, row, i,
					  state, &clip_rectangle, &cell_rectangle,
					  crect, area, style);

      /* draw expander */
      offset = nautilus_ctree_draw_expander (ctree, (NautilusCTreeRow *)clist_row,
					     style, &clip_rectangle, offset);

      if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
	offset -= ctree->tree_spacing;
      else
	offset += ctree->tree_spacing;

      if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
	offset -= (pixmap_width + clist_row->cell[i].horizontal);
      else
	offset += clist_row->cell[i].horizontal;

      old_offset = offset;
      {
	      int dark_width, dark_height;
	      GdkPixbuf *src_pixbuf, *dark_pixbuf;
	      GdkPixmap *dark_pixmap;
	      GdkBitmap *dark_mask;

	      if (((GtkCListRow *)ctree->dnd_prelighted_row) == clist_row) {
		      
		      gdk_window_get_geometry (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
					       NULL, NULL, &dark_width, &dark_height, NULL);
		      
		      src_pixbuf = gdk_pixbuf_get_from_drawable 
			      (NULL,
			       GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			       gdk_rgb_get_cmap (),
			       0, 0, 0, 0, dark_width, dark_height);
		      
		      if (src_pixbuf != NULL) {
			      /* Create darkened pixmap */			
			      dark_pixbuf = nautilus_create_darkened_pixbuf (src_pixbuf,
									     0.8 * 255,
									     0.8 * 255);
			      if (dark_pixbuf != NULL) {
				      gdk_pixbuf_render_pixmap_and_mask (dark_pixbuf,
									 &dark_pixmap, &dark_mask,
									 NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
				      
				      offset = draw_cell_pixmap (clist->clist_window, &cell_rectangle, fg_gc,
							dark_pixmap, GTK_CELL_PIXTEXT (clist_row->cell[i])->mask, offset,
							clip_rectangle.y + clist_row->cell[i].vertical +
							(clip_rectangle.height - height) / 2,
							pixmap_width, height);
				      
							gdk_pixbuf_unref (dark_pixbuf);
			      }
			      gdk_pixbuf_unref (src_pixbuf);
		      }					
	      } else {		
		      offset = draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
						 GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
						 GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
						 offset,
						 clip_rectangle.y + clist_row->cell[i].vertical +
						 (clip_rectangle.height - height) / 2,
						 pixmap_width, height);


	      }
      }

      if (string_width)
	{ 
	  if (clist->column[i].justification == GTK_JUSTIFY_RIGHT)
	    {
	      offset = (old_offset - string_width);
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		offset -= GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	    }
	  else
	    {
	      if (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap)
		offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	    }

	  row_center_offset = ((clist->row_height -
				(style->font->ascent
				 + style->font->descent)) / 2
			       + style->font->ascent);

	  gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
	  gdk_draw_string (clist->clist_window, style->font, fg_gc, offset,
			   row_rectangle.y + row_center_offset +
			   clist_row->cell[i].vertical,
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->text);
	}
      gdk_gc_set_clip_rectangle (fg_gc, NULL);
    }

  /* draw focus rectangle */
  if (clist->focus_row == row &&
      GTK_WIDGET_CAN_FOCUS (widget) && GTK_WIDGET_HAS_FOCUS (widget))
    {
      if (!area)
	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			    row_rectangle.x, row_rectangle.y,
			    row_rectangle.width - 1, row_rectangle.height - 1);
      else if (gdk_rectangle_intersect (area, &row_rectangle,
					&intersect_rectangle))
	{
	  gdk_gc_set_clip_rectangle (clist->xor_gc, &intersect_rectangle);
	  gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			      row_rectangle.x, row_rectangle.y,
			      row_rectangle.width - 1,
			      row_rectangle.height - 1);
	  gdk_gc_set_clip_rectangle (clist->xor_gc, NULL);
	}
    }
}

void
nautilus_ctree_draw_node (NautilusCTree *ctree, NautilusCTreeNode *node)
{
	if (ctree == NULL || node == NULL) {
		return;
	}
	
	tree_draw_node (ctree, node);
}

static void
tree_draw_node (NautilusCTree     *ctree, 
	        NautilusCTreeNode *node)
{
	GtkCList *clist;
  
	clist = GTK_CLIST (ctree);

	if (CLIST_UNFROZEN (clist) && nautilus_ctree_is_viewable (ctree, node))
	{
		NautilusCTreeNode *work;
		gint num = 0;
      
		work = NAUTILUS_CTREE_NODE (clist->row_list);
		while (work && work != node)
		{
			work = NAUTILUS_CTREE_NODE_NEXT (work);
			num++;
		}
		
		if (work && gtk_clist_row_is_visible (clist, num) != GTK_VISIBILITY_NONE) {
			GTK_CLIST_CLASS_FW (clist)->draw_row			
	  			(clist, NULL, num, GTK_CLIST_ROW ((GList *) node));
		}
	}
}

static NautilusCTreeNode *
nautilus_ctree_last_visible (NautilusCTree     *ctree,
			NautilusCTreeNode *node)
{
  NautilusCTreeNode *work;
  
  if (!node)
    return NULL;

  work = NAUTILUS_CTREE_ROW (node)->children;

  if (!work || !NAUTILUS_CTREE_ROW (node)->expanded)
    return node;

  while (NAUTILUS_CTREE_ROW (work)->sibling)
    work = NAUTILUS_CTREE_ROW (work)->sibling;

  return nautilus_ctree_last_visible (ctree, work);
}

static void
nautilus_ctree_link (NautilusCTree     *ctree,
		NautilusCTreeNode *node,
		NautilusCTreeNode *parent,
		NautilusCTreeNode *sibling,
		gboolean      update_focus_row)
{
  GtkCList *clist;
  GList *list_end;
  GList *list;
  GList *work;
  gboolean visible = FALSE;
  gint rows = 0;
  
  if (sibling)
    g_return_if_fail (NAUTILUS_CTREE_ROW (sibling)->parent == parent);
  g_return_if_fail (node != NULL);
  g_return_if_fail (node != sibling);
  g_return_if_fail (node != parent);

  clist = GTK_CLIST (ctree);

  if (update_focus_row && clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  for (rows = 1, list_end = (GList *)node; list_end->next;
       list_end = list_end->next)
    rows++;

  NAUTILUS_CTREE_ROW (node)->parent = parent;
  NAUTILUS_CTREE_ROW (node)->sibling = sibling;

  if (!parent || (parent && (nautilus_ctree_is_viewable (ctree, parent) &&
			     NAUTILUS_CTREE_ROW (parent)->expanded)))
    {
      visible = TRUE;
      clist->rows += rows;
    }

  if (parent)
    work = (GList *)(NAUTILUS_CTREE_ROW (parent)->children);
  else
    work = clist->row_list;

  if (sibling)
    {
      if (work != (GList *)sibling)
	{
	  while (NAUTILUS_CTREE_ROW (work)->sibling != sibling)
	    work = (GList *)(NAUTILUS_CTREE_ROW (work)->sibling);
	  NAUTILUS_CTREE_ROW (work)->sibling = node;
	}

      if (sibling == NAUTILUS_CTREE_NODE (clist->row_list))
	clist->row_list = (GList *) node;
      if (NAUTILUS_CTREE_NODE_PREV (sibling) &&
	  NAUTILUS_CTREE_NODE_NEXT (NAUTILUS_CTREE_NODE_PREV (sibling)) == sibling)
	{
	  list = (GList *)NAUTILUS_CTREE_NODE_PREV (sibling);
	  list->next = (GList *)node;
	}
      
      list = (GList *)node;
      list->prev = (GList *)NAUTILUS_CTREE_NODE_PREV (sibling);
      list_end->next = (GList *)sibling;
      list = (GList *)sibling;
      list->prev = list_end;
      if (parent && NAUTILUS_CTREE_ROW (parent)->children == sibling)
	NAUTILUS_CTREE_ROW (parent)->children = node;
    }
  else
    {
      if (work)
	{
	  /* find sibling */
	  while (NAUTILUS_CTREE_ROW (work)->sibling)
	    work = (GList *)(NAUTILUS_CTREE_ROW (work)->sibling);
	  NAUTILUS_CTREE_ROW (work)->sibling = node;
	  
	  /* find last visible child of sibling */
	  work = (GList *) nautilus_ctree_last_visible (ctree,
						   NAUTILUS_CTREE_NODE (work));
	  
	  list_end->next = work->next;
	  if (work->next)
	    list = work->next->prev = list_end;
	  work->next = (GList *)node;
	  list = (GList *)node;
	  list->prev = work;
	}
      else
	{
	  if (parent)
	    {
	      NAUTILUS_CTREE_ROW (parent)->children = node;
	      list = (GList *)node;
	      list->prev = (GList *)parent;
	      if (NAUTILUS_CTREE_ROW (parent)->expanded)
		{
		  list_end->next = (GList *)NAUTILUS_CTREE_NODE_NEXT (parent);
		  if (NAUTILUS_CTREE_NODE_NEXT(parent))
		    {
		      list = (GList *)NAUTILUS_CTREE_NODE_NEXT (parent);
		      list->prev = list_end;
		    }
		  list = (GList *)parent;
		  list->next = (GList *)node;
		}
	      else
		list_end->next = NULL;
	    }
	  else
	    {
	      clist->row_list = (GList *)node;
	      list = (GList *)node;
	      list->prev = NULL;
	      list_end->next = NULL;
	    }
	}
    }

  nautilus_ctree_pre_recursive (ctree, node, tree_update_level, NULL); 

  if (clist->row_list_end == NULL ||
      clist->row_list_end->next == (GList *)node)
    clist->row_list_end = list_end;

  if (visible && update_focus_row)
    {
      gint pos;
	  
      pos = g_list_position (clist->row_list, (GList *)node);
  
      if (pos <= clist->focus_row)
	{
	  clist->focus_row += rows;
	  clist->undo_anchor = clist->focus_row;
	}
    }
}

static void
nautilus_ctree_unlink (NautilusCTree     *ctree, 
		  NautilusCTreeNode *node,
                  gboolean      update_focus_row)
{
  GtkCList *clist;
  gint rows;
  gint level;
  gint visible;
  NautilusCTreeNode *work;
  NautilusCTreeNode *parent;
  GList *list;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);
  
  if (update_focus_row && clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  visible = nautilus_ctree_is_viewable (ctree, node);

  /* clist->row_list_end unlinked ? */
  if (visible &&
      (NAUTILUS_CTREE_NODE_NEXT (node) == NULL ||
       (NAUTILUS_CTREE_ROW (node)->children &&
	nautilus_ctree_is_ancestor (ctree, node,
				    NAUTILUS_CTREE_NODE (clist->row_list_end)))))
	  clist->row_list_end = (GList *) (NAUTILUS_CTREE_NODE_PREV (node));

  /* update list */
  rows = 0;
  level = NAUTILUS_CTREE_ROW (node)->level;
  work = NAUTILUS_CTREE_NODE_NEXT (node);
  while (work && NAUTILUS_CTREE_ROW (work)->level > level)
    {
      work = NAUTILUS_CTREE_NODE_NEXT (work);
      rows++;
    }

  if (visible)
    {
      clist->rows -= (rows + 1);

      if (update_focus_row)
	{
	  gint pos;
	  
	  pos = g_list_position (clist->row_list, (GList *)node);
	  if (pos + rows < clist->focus_row)
	    clist->focus_row -= (rows + 1);
	  else if (pos <= clist->focus_row)
	    {
	      if (!NAUTILUS_CTREE_ROW (node)->sibling)
		clist->focus_row = MAX (pos - 1, 0);
	      else
		clist->focus_row = pos;
	      
	      clist->focus_row = MIN (clist->focus_row, clist->rows - 1);
	    }
	  clist->undo_anchor = clist->focus_row;
	}
    }

  if (work)
    {
      list = (GList *)NAUTILUS_CTREE_NODE_PREV (work);
      list->next = NULL;
      list = (GList *)work;
      list->prev = (GList *)NAUTILUS_CTREE_NODE_PREV (node);
    }

  if (NAUTILUS_CTREE_NODE_PREV (node) &&
      NAUTILUS_CTREE_NODE_NEXT (NAUTILUS_CTREE_NODE_PREV (node)) == node)
    {
      list = (GList *)NAUTILUS_CTREE_NODE_PREV (node);
      list->next = (GList *)work;
    }

  /* update tree */
  parent = NAUTILUS_CTREE_ROW (node)->parent;
  if (parent)
    {
      if (NAUTILUS_CTREE_ROW (parent)->children == node)
	{
	  NAUTILUS_CTREE_ROW (parent)->children = NAUTILUS_CTREE_ROW (node)->sibling;
	  if (NAUTILUS_CTREE_ROW (parent)->is_leaf)
	    nautilus_ctree_collapse (ctree, parent);
	}
      else
	{
	  NautilusCTreeNode *sibling;

	  sibling = NAUTILUS_CTREE_ROW (parent)->children;
	  while (NAUTILUS_CTREE_ROW (sibling)->sibling != node)
	    sibling = NAUTILUS_CTREE_ROW (sibling)->sibling;
	  NAUTILUS_CTREE_ROW (sibling)->sibling = NAUTILUS_CTREE_ROW (node)->sibling;
	}
    }
  else
    {
      if (clist->row_list == (GList *)node)
	clist->row_list = (GList *) (NAUTILUS_CTREE_ROW (node)->sibling);
      else
	{
	  NautilusCTreeNode *sibling;

	  sibling = NAUTILUS_CTREE_NODE (clist->row_list);
	  while (NAUTILUS_CTREE_ROW (sibling)->sibling != node)
	    sibling = NAUTILUS_CTREE_ROW (sibling)->sibling;
	  NAUTILUS_CTREE_ROW (sibling)->sibling = NAUTILUS_CTREE_ROW (node)->sibling;
	}
    }
}

static void
real_row_move (GtkCList *clist,
	       gint      source_row,
	       gint      dest_row)
{
  NautilusCTree *ctree;
  NautilusCTreeNode *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  if (GTK_CLIST_AUTO_SORT (clist))
    return;

  if (source_row < 0 || source_row >= clist->rows ||
      dest_row   < 0 || dest_row   >= clist->rows ||
      source_row == dest_row)
    return;

  ctree = NAUTILUS_CTREE (clist);
  node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, source_row));

  if (source_row < dest_row)
    {
      NautilusCTreeNode *work; 

      dest_row++;
      work = NAUTILUS_CTREE_ROW (node)->children;

      while (work && NAUTILUS_CTREE_ROW (work)->level > NAUTILUS_CTREE_ROW (node)->level)
	{
	  work = NAUTILUS_CTREE_NODE_NEXT (work);
	  dest_row++;
	}

      if (dest_row > clist->rows)
	dest_row = clist->rows;
    }

  if (dest_row < clist->rows)
    {
      NautilusCTreeNode *sibling;

      sibling = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, dest_row));
      nautilus_ctree_move (ctree, node, NAUTILUS_CTREE_ROW (sibling)->parent, sibling);
    }
  else
    nautilus_ctree_move (ctree, node, NULL, NULL);
}

static void
real_tree_move (NautilusCTree     *ctree,
		NautilusCTreeNode *node,
		NautilusCTreeNode *new_parent, 
		NautilusCTreeNode *new_sibling)
{
  GtkCList *clist;
  NautilusCTreeNode *work;
  gboolean visible = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (!new_sibling || 
		    NAUTILUS_CTREE_ROW (new_sibling)->parent == new_parent);

  if (new_parent && NAUTILUS_CTREE_ROW (new_parent)->is_leaf)
    return;

  /* new_parent != child of child */
  for (work = new_parent; work; work = NAUTILUS_CTREE_ROW (work)->parent)
    if (work == node)
      return;

  clist = GTK_CLIST (ctree);

  visible = nautilus_ctree_is_viewable (ctree, node);

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (new_parent == NAUTILUS_CTREE_ROW (node)->parent)
	return;
      
      if (new_parent)
	new_sibling = NAUTILUS_CTREE_ROW (new_parent)->children;
      else
	new_sibling = NAUTILUS_CTREE_NODE (clist->row_list);

      while (new_sibling && clist->compare
	     (clist, NAUTILUS_CTREE_ROW (node), NAUTILUS_CTREE_ROW (new_sibling)) > 0)
	new_sibling = NAUTILUS_CTREE_ROW (new_sibling)->sibling;
    }

  if (new_parent == NAUTILUS_CTREE_ROW (node)->parent && 
      new_sibling == NAUTILUS_CTREE_ROW (node)->sibling)
    return;

  gtk_clist_freeze (clist);

  work = NULL;
  if (nautilus_ctree_is_viewable (ctree, node) ||
      nautilus_ctree_is_viewable (ctree, new_sibling))
    work = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));
      
  nautilus_ctree_unlink (ctree, node, FALSE);
  nautilus_ctree_link (ctree, node, new_parent, new_sibling, FALSE);
  
  if (work)
    {
      while (work &&  !nautilus_ctree_is_viewable (ctree, work))
	work = NAUTILUS_CTREE_ROW (work)->parent;
      clist->focus_row = g_list_position (clist->row_list, (GList *)work);
      clist->undo_anchor = clist->focus_row;
    }

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist) &&
      (visible || nautilus_ctree_is_viewable (ctree, node)))
    gtk_clist_set_column_width
      (clist, ctree->tree_column,
       gtk_clist_optimal_column_width (clist, ctree->tree_column));

  gtk_clist_thaw (clist);
}

static void
change_focus_row_expansion (NautilusCTree          *ctree,
			    NautilusCTreeExpansionType action)
{
  GtkCList *clist;
  NautilusCTreeNode *node;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (ctree))
    return;
  
  if (!(node =
	NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row))) ||
      NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  switch (action)
    {
    case NAUTILUS_CTREE_EXPANSION_EXPAND:
      nautilus_ctree_expand (ctree, node);
      break;
    case NAUTILUS_CTREE_EXPANSION_EXPAND_RECURSIVE:
      nautilus_ctree_expand_recursive (ctree, node);
      break;
    case NAUTILUS_CTREE_EXPANSION_COLLAPSE:
      nautilus_ctree_collapse (ctree, node);
      break;
    case NAUTILUS_CTREE_EXPANSION_COLLAPSE_RECURSIVE:
      nautilus_ctree_collapse_recursive (ctree, node);
      break;
    case NAUTILUS_CTREE_EXPANSION_TOGGLE:
      nautilus_ctree_toggle_expansion (ctree, node);
      break;
    case NAUTILUS_CTREE_EXPANSION_TOGGLE_RECURSIVE:
      nautilus_ctree_toggle_expansion_recursive (ctree, node);
      break;
    }
}

static void 
real_tree_expand (NautilusCTree     *ctree,
		  NautilusCTreeNode *node)
{
  GtkCList *clist;
  NautilusCTreeNode *work;
  GtkRequisition requisition;
  gboolean visible;
  gint level;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (!node || NAUTILUS_CTREE_ROW (node)->expanded || NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);
  
  GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);

  NAUTILUS_CTREE_ROW (node)->expanded = TRUE;
  level = NAUTILUS_CTREE_ROW (node)->level;

  visible = nautilus_ctree_is_viewable (ctree, node);
  /* get cell width if tree_column is auto resized */
  if (visible && clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    GTK_CLIST_CLASS_FW (clist)->cell_size_request
      (clist, &NAUTILUS_CTREE_ROW (node)->row, ctree->tree_column, &requisition);

  /* unref/unset closed pixmap */
  if (GTK_CELL_PIXTEXT 
      (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      gdk_pixmap_unref
	(GTK_CELL_PIXTEXT
	 (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT 
	     (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  /* set/ref opened pixmap */
  if (NAUTILUS_CTREE_ROW (node)->pixmap_opened)
    {
      GTK_CELL_PIXTEXT 
	(NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	gdk_pixmap_ref (NAUTILUS_CTREE_ROW (node)->pixmap_opened);

      if (NAUTILUS_CTREE_ROW (node)->mask_opened) 
	GTK_CELL_PIXTEXT 
	  (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  gdk_pixmap_ref (NAUTILUS_CTREE_ROW (node)->mask_opened);
    }


  work = NAUTILUS_CTREE_ROW (node)->children;
  if (work)
    {
      GList *list = (GList *)work;
      gint *cell_width = NULL;
      gint tmp = 0;
      gint row;
      gint i;
      
      if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
	{
	  cell_width = g_new0 (gint, clist->columns);
	  if (clist->column[ctree->tree_column].auto_resize)
	      cell_width[ctree->tree_column] = requisition.width;

	  while (work)
	    {
	      /* search maximum cell widths of auto_resize columns */
	      for (i = 0; i < clist->columns; i++)
		if (clist->column[i].auto_resize)
		  {
		    GTK_CLIST_CLASS_FW (clist)->cell_size_request
		      (clist, &NAUTILUS_CTREE_ROW (work)->row, i, &requisition);
		    cell_width[i] = MAX (requisition.width, cell_width[i]);
		  }

	      list = (GList *)work;
	      work = NAUTILUS_CTREE_NODE_NEXT (work);
	      tmp++;
	    }
	}
      else
	while (work)
	  {
	    list = (GList *)work;
	    work = NAUTILUS_CTREE_NODE_NEXT (work);
	    tmp++;
	  }

      list->next = (GList *)NAUTILUS_CTREE_NODE_NEXT (node);

      if (NAUTILUS_CTREE_NODE_NEXT (node))
	{
	  GList *tmp_list;

	  tmp_list = (GList *)NAUTILUS_CTREE_NODE_NEXT (node);
	  tmp_list->prev = list;
	}
      else
	clist->row_list_end = list;

      list = (GList *)node;
      list->next = (GList *)(NAUTILUS_CTREE_ROW (node)->children);

      if (visible)
	{
	  /* resize auto_resize columns if needed */
	  for (i = 0; i < clist->columns; i++)
	    if (clist->column[i].auto_resize &&
		cell_width[i] > clist->column[i].width)
	      gtk_clist_set_column_width (clist, i, cell_width[i]);
	  g_free (cell_width);

	  /* update focus_row position */
	  row = g_list_position (clist->row_list, (GList *)node);
	  if (row < clist->focus_row)
	    clist->focus_row += tmp;

	  clist->rows += tmp;
	  CLIST_REFRESH (clist);
	}
    }
  else if (visible && clist->column[ctree->tree_column].auto_resize)
    /* resize tree_column if needed */
    column_auto_resize (clist, &NAUTILUS_CTREE_ROW (node)->row, ctree->tree_column,
			requisition.width);

  tree_draw_node (ctree, node);
}

static void 
real_tree_collapse (NautilusCTree     *ctree,
		    NautilusCTreeNode *node)
{
  GtkCList *clist;
  NautilusCTreeNode *work;
  GtkRequisition requisition;
  gboolean visible;
  gint level;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (!node || !NAUTILUS_CTREE_ROW (node)->expanded ||
      NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
  
  NAUTILUS_CTREE_ROW (node)->expanded = FALSE;
  level = NAUTILUS_CTREE_ROW (node)->level;

  visible = nautilus_ctree_is_viewable (ctree, node);
  /* get cell width if tree_column is auto resized */
  if (visible && clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    GTK_CLIST_CLASS_FW (clist)->cell_size_request
      (clist, &NAUTILUS_CTREE_ROW (node)->row, ctree->tree_column, &requisition);

  /* unref/unset opened pixmap */
  if (GTK_CELL_PIXTEXT 
      (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      gdk_pixmap_unref
	(GTK_CELL_PIXTEXT
	 (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT 
	     (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  /* set/ref closed pixmap */
  if (NAUTILUS_CTREE_ROW (node)->pixmap_closed)
    {
      GTK_CELL_PIXTEXT 
	(NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	gdk_pixmap_ref (NAUTILUS_CTREE_ROW (node)->pixmap_closed);

      if (NAUTILUS_CTREE_ROW (node)->mask_closed) 
	GTK_CELL_PIXTEXT 
	  (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  gdk_pixmap_ref (NAUTILUS_CTREE_ROW (node)->mask_closed);
    }

  work = NAUTILUS_CTREE_ROW (node)->children;
  if (work)
    {
      gint tmp = 0;
      gint row;
      GList *list;

      while (work && NAUTILUS_CTREE_ROW (work)->level > level)
	{
	  work = NAUTILUS_CTREE_NODE_NEXT (work);
	  tmp++;
	}

      if (work)
	{
	  list = (GList *)node;
	  list->next = (GList *)work;
	  list = (GList *)NAUTILUS_CTREE_NODE_PREV (work);
	  list->next = NULL;
	  list = (GList *)work;
	  list->prev = (GList *)node;
	}
      else
	{
	  list = (GList *)node;
	  list->next = NULL;
	  clist->row_list_end = (GList *)node;
	}

      if (visible)
	{
	  /* resize auto_resize columns if needed */
	  auto_resize_columns (clist);

	  row = g_list_position (clist->row_list, (GList *)node);
	  if (row < clist->focus_row)
	    clist->focus_row -= tmp;
	  clist->rows -= tmp;
	  CLIST_REFRESH (clist);
	}
    }
  else if (visible && clist->column[ctree->tree_column].auto_resize &&
	   !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    /* resize tree_column if needed */
    column_auto_resize (clist, &NAUTILUS_CTREE_ROW (node)->row, ctree->tree_column,
			requisition.width);
    
  tree_draw_node (ctree, node);
}

static void
column_auto_resize (GtkCList    *clist,
		    GtkCListRow *clist_row,
		    gint         column,
		    gint         old_width)
{
  /* resize column if needed for auto_resize */
  GtkRequisition requisition;

  if (!clist->column[column].auto_resize ||
      GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    return;

  if (clist_row)
    GTK_CLIST_CLASS_FW (clist)->cell_size_request (clist, clist_row,
						   column, &requisition);
  else
    requisition.width = 0;

  if (requisition.width > clist->column[column].width)
    gtk_clist_set_column_width (clist, column, requisition.width);
  else if (requisition.width < old_width &&
	   old_width == clist->column[column].width)
    {
      GList *list;
      gint new_width;

      /* run a "gtk_clist_optimal_column_width" but break, if
       * the column doesn't shrink */
      if (GTK_CLIST_SHOW_TITLES (clist) && clist->column[column].button)
	new_width = (clist->column[column].button->requisition.width -
		     (CELL_SPACING + (2 * COLUMN_INSET)));
      else
	new_width = 0;

      for (list = clist->row_list; list; list = list->next)
	{
	  GTK_CLIST_CLASS_FW (clist)->cell_size_request
	    (clist, GTK_CLIST_ROW (list), column, &requisition);
	  new_width = MAX (new_width, requisition.width);
	  if (new_width == clist->column[column].width)
	    break;
	}
      if (new_width < clist->column[column].width)
	gtk_clist_set_column_width (clist, column, new_width);
    }
}

static void
auto_resize_columns (GtkCList *clist)
{
  gint i;

  if (GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    return;

  for (i = 0; i < clist->columns; i++)
    column_auto_resize (clist, NULL, i, clist->column[i].width);
}

static void
cell_size_request (GtkCList       *clist,
		   GtkCListRow    *clist_row,
		   gint            column,
		   GtkRequisition *requisition)
{
	NautilusCTree *ctree;
	GtkStyle *style;
	gint width;
	gint height;

	g_return_if_fail (clist != NULL);
	g_return_if_fail (NAUTILUS_IS_CTREE (clist));
	g_return_if_fail (requisition != NULL);

	ctree = NAUTILUS_CTREE (clist);

	get_cell_style (clist, clist_row, GTK_STATE_NORMAL, column, &style, NULL, NULL);

	switch (clist_row->cell[column].type)
	{
		case GTK_CELL_TEXT:
			requisition->width =
				gdk_string_width (style->font, GTK_CELL_TEXT (clist_row->cell[column])->text);
      			requisition->height = style->font->ascent + style->font->descent;
      			break;
      			
		case GTK_CELL_PIXTEXT:
			if (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap) {
				gdk_window_get_size (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap,
						     &width, &height);
				width += GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing;
			} else {
				width = height = 0;
			}
				  
			requisition->width = width + gdk_string_width (style->font,
								       GTK_CELL_TEXT (clist_row->cell[column])->text);
      			requisition->height = MAX (style->font->ascent + style->font->descent, height);

			if (column == ctree->tree_column) {
				requisition->width += (ctree->tree_spacing + ctree->tree_indent *
				 		      (((NautilusCTreeRow *) clist_row)->level - 1));
				requisition->width += PM_SIZE + 3;
			}

			if (ctree->line_style == NAUTILUS_CTREE_LINES_TABBED) {
				requisition->width += 3;
			}		
      			break;
      			
		case GTK_CELL_PIXMAP:
			gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap, &width, &height);
			requisition->width = width;
			requisition->height = height;
			break;
			
		default:
			requisition->width  = 0;
			requisition->height = 0;
			break;
	}

	requisition->width  += clist_row->cell[column].horizontal;
	requisition->height += clist_row->cell[column].vertical;
}

static void
set_cell_contents (GtkCList    *clist,
		   GtkCListRow *clist_row,
		   gint         column,
		   GtkCellType  type,
		   const gchar *text,
		   guint8       spacing,
		   GdkPixmap   *pixmap,
		   GdkBitmap   *mask)
{
  gboolean visible = FALSE;
  NautilusCTree *ctree;
  GtkRequisition requisition;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));
  g_return_if_fail (clist_row != NULL);

  ctree = NAUTILUS_CTREE (clist);

  if (clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      NautilusCTreeNode *parent;

      parent = ((NautilusCTreeRow *)clist_row)->parent;
      if (!parent || (parent && NAUTILUS_CTREE_ROW (parent)->expanded &&
		      nautilus_ctree_is_viewable (ctree, parent)))
	{
	  visible = TRUE;
	  GTK_CLIST_CLASS_FW (clist)->cell_size_request (clist, clist_row,
							 column, &requisition);
	}
    }

  switch (clist_row->cell[column].type)
    {
    case GTK_CELL_EMPTY:
      break;
      
    case GTK_CELL_TEXT:
      g_free (GTK_CELL_TEXT (clist_row->cell[column])->text);
      break;
    case GTK_CELL_PIXMAP:
      gdk_pixmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap);
      if (GTK_CELL_PIXMAP (clist_row->cell[column])->mask)
	gdk_bitmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->mask);
      break;
    case GTK_CELL_PIXTEXT:
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->text)
	g_free (GTK_CELL_PIXTEXT (clist_row->cell[column])->text);
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap);
	  if (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask)
	    gdk_bitmap_unref
	      (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask);
	}
      break;
    case GTK_CELL_WIDGET:
      /* unimplimented */
      break;
      
    default:
      break;
    }

  clist_row->cell[column].type = GTK_CELL_EMPTY;
  if (column == ctree->tree_column && type != GTK_CELL_EMPTY)
    type = GTK_CELL_PIXTEXT;

  switch (type)
    {
    case GTK_CELL_TEXT:
      if (text)
	{
	  clist_row->cell[column].type = GTK_CELL_TEXT;
	  GTK_CELL_TEXT (clist_row->cell[column])->text = g_strdup (text);
	}
      break;
    case GTK_CELL_PIXMAP:
      if (pixmap)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXMAP;
	  GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap = pixmap;
	  /* We set the mask even if it is NULL */
	  GTK_CELL_PIXMAP (clist_row->cell[column])->mask = mask;
	}
      break;
    case GTK_CELL_PIXTEXT:
      if (column == ctree->tree_column)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXTEXT;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
	  if (text)
	    GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
	  else
	    GTK_CELL_PIXTEXT (clist_row->cell[column])->text = NULL;
	  if (pixmap)
	    {
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
	    }
	  else
	    {
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = NULL;
	      GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = NULL;
	    }
	}
      else if (text && pixmap)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXTEXT;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
	}
      break;
    default:
      break;
    }
  
  if (visible && clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    column_auto_resize (clist, clist_row, column, requisition.width);
}

static void 
set_node_info (NautilusCTree     *ctree,
	       NautilusCTreeNode *node,
	       const gchar  *text,
	       guint8        spacing,
	       GdkPixmap    *pixmap_closed,
	       GdkBitmap    *mask_closed,
	       GdkPixmap    *pixmap_opened,
	       GdkBitmap    *mask_opened,
	       gboolean      is_leaf,
	       gboolean      expanded)
{
  if (NAUTILUS_CTREE_ROW (node)->pixmap_opened)
    {
      gdk_pixmap_unref (NAUTILUS_CTREE_ROW (node)->pixmap_opened);
      if (NAUTILUS_CTREE_ROW (node)->mask_opened) 
	gdk_bitmap_unref (NAUTILUS_CTREE_ROW (node)->mask_opened);
    }
  if (NAUTILUS_CTREE_ROW (node)->pixmap_closed)
    {
      gdk_pixmap_unref (NAUTILUS_CTREE_ROW (node)->pixmap_closed);
      if (NAUTILUS_CTREE_ROW (node)->mask_closed) 
	gdk_bitmap_unref (NAUTILUS_CTREE_ROW (node)->mask_closed);
    }

  NAUTILUS_CTREE_ROW (node)->pixmap_opened = NULL;
  NAUTILUS_CTREE_ROW (node)->mask_opened   = NULL;
  NAUTILUS_CTREE_ROW (node)->pixmap_closed = NULL;
  NAUTILUS_CTREE_ROW (node)->mask_closed   = NULL;

  if (pixmap_closed)
    {
      NAUTILUS_CTREE_ROW (node)->pixmap_closed = gdk_pixmap_ref (pixmap_closed);
      if (mask_closed) 
	NAUTILUS_CTREE_ROW (node)->mask_closed = gdk_bitmap_ref (mask_closed);
    }
  if (pixmap_opened)
    {
      NAUTILUS_CTREE_ROW (node)->pixmap_opened = gdk_pixmap_ref (pixmap_opened);
      if (mask_opened) 
	NAUTILUS_CTREE_ROW (node)->mask_opened = gdk_bitmap_ref (mask_opened);
    }

  NAUTILUS_CTREE_ROW (node)->is_leaf  = is_leaf;
  NAUTILUS_CTREE_ROW (node)->expanded = (is_leaf) ? FALSE : expanded;

  if (NAUTILUS_CTREE_ROW (node)->expanded)
    nautilus_ctree_node_set_pixtext (ctree, node, ctree->tree_column,
				text, spacing, pixmap_opened, mask_opened);
  else 
    nautilus_ctree_node_set_pixtext (ctree, node, ctree->tree_column,
				text, spacing, pixmap_closed, mask_closed);

  if (GTK_CLIST_AUTO_SORT (GTK_CLIST (ctree))
      && NAUTILUS_CTREE_ROW (node)->parent != NULL)
    {
      nautilus_ctree_sort_node (ctree, NAUTILUS_CTREE_ROW (node)->parent);
    }
}

static void
tree_delete (NautilusCTree     *ctree, 
	     NautilusCTreeNode *node, 
	     gpointer      data)
{
  tree_unselect (ctree,  node, NULL);
  row_delete (ctree, NAUTILUS_CTREE_ROW (node));
  g_list_free_1 ((GList *)node);
}

static void
tree_delete_row (NautilusCTree     *ctree, 
		 NautilusCTreeNode *node, 
		 gpointer      data)
{
  row_delete (ctree, NAUTILUS_CTREE_ROW (node));
  g_list_free_1 ((GList *)node);
}

static void
tree_update_level (NautilusCTree     *ctree, 
		   NautilusCTreeNode *node, 
		   gpointer      data)
{
  if (!node)
    return;

  if (NAUTILUS_CTREE_ROW (node)->parent)
      NAUTILUS_CTREE_ROW (node)->level = 
	NAUTILUS_CTREE_ROW (NAUTILUS_CTREE_ROW (node)->parent)->level + 1;
  else
      NAUTILUS_CTREE_ROW (node)->level = 1;
}

static void
tree_select (NautilusCTree     *ctree, 
	     NautilusCTreeNode *node, 
	     gpointer      data)
{
  if (node && NAUTILUS_CTREE_ROW (node)->row.state != GTK_STATE_SELECTED &&
      NAUTILUS_CTREE_ROW (node)->row.selectable)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW],
		     node, -1);
}

static void
tree_unselect (NautilusCTree     *ctree, 
	       NautilusCTreeNode *node, 
	       gpointer      data)
{
  if (node && NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW], 
		     node, -1);
}

static void
tree_expand (NautilusCTree     *ctree, 
	     NautilusCTreeNode *node, 
	     gpointer      data)
{
  if (node && !NAUTILUS_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
}

static void
tree_collapse (NautilusCTree     *ctree, 
	       NautilusCTreeNode *node, 
	       gpointer      data)
{
  if (node && NAUTILUS_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
}

static void
tree_collapse_to_depth (NautilusCTree     *ctree, 
			NautilusCTreeNode *node, 
			gint          depth)
{
  if (node && NAUTILUS_CTREE_ROW (node)->level == depth)
    nautilus_ctree_collapse_recursive (ctree, node);
}

static void
tree_toggle_expansion (NautilusCTree     *ctree,
		       NautilusCTreeNode *node,
		       gpointer      data)
{
  if (!node)
    return;

  if (NAUTILUS_CTREE_ROW (node)->expanded)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
  else
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
}

static NautilusCTreeRow *
row_new (NautilusCTree *ctree)
{
	GtkCList *clist;
	NautilusCTreeRow *ctree_row;
	int i;

	clist = GTK_CLIST (ctree);
	ctree_row = g_chunk_new (NautilusCTreeRow, clist->row_mem_chunk);
	ctree_row->row.cell = g_chunk_new (GtkCell, clist->cell_mem_chunk);

	for (i = 0; i < clist->columns; i++) {
		ctree_row->row.cell[i].type = GTK_CELL_EMPTY;
		ctree_row->row.cell[i].vertical = 0;
		ctree_row->row.cell[i].horizontal = 0;
		ctree_row->row.cell[i].style = NULL;
	}

	GTK_CELL_PIXTEXT (ctree_row->row.cell[ctree->tree_column])->text = NULL;

	ctree_row->row.fg_set     = FALSE;
	ctree_row->row.bg_set     = FALSE;
	ctree_row->row.style      = NULL;
	ctree_row->row.selectable = TRUE;
	ctree_row->row.state      = GTK_STATE_NORMAL;
	ctree_row->row.data       = NULL;
	ctree_row->row.destroy    = NULL;

	ctree_row->level         = 0;
	ctree_row->expanded      = FALSE;
	ctree_row->parent        = NULL;
	ctree_row->sibling       = NULL;
	ctree_row->children      = NULL;
	ctree_row->pixmap_closed = NULL;
	ctree_row->mask_closed   = NULL;
	ctree_row->pixmap_opened = NULL;
	ctree_row->mask_opened   = NULL;
	ctree_row->mouse_down    = FALSE;
	ctree_row->in_hotspot    = FALSE;
	
	return ctree_row;
}

static void
row_delete (NautilusCTree    *ctree,
	    NautilusCTreeRow *ctree_row)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  for (i = 0; i < clist->columns; i++)
    {
      GTK_CLIST_CLASS_FW (clist)->set_cell_contents
	(clist, &(ctree_row->row), i, GTK_CELL_EMPTY, NULL, 0, NULL, NULL);
      if (ctree_row->row.cell[i].style)
	{
	  if (GTK_WIDGET_REALIZED (ctree))
	    gtk_style_detach (ctree_row->row.cell[i].style);
	  gtk_style_unref (ctree_row->row.cell[i].style);
	}
    }

  if (ctree_row->row.style)
    {
      if (GTK_WIDGET_REALIZED (ctree))
	gtk_style_detach (ctree_row->row.style);
      gtk_style_unref (ctree_row->row.style);
    }

  if (ctree_row->pixmap_closed)
    {
      gdk_pixmap_unref (ctree_row->pixmap_closed);
      if (ctree_row->mask_closed)
	gdk_bitmap_unref (ctree_row->mask_closed);
    }

  if (ctree_row->pixmap_opened)
    {
      gdk_pixmap_unref (ctree_row->pixmap_opened);
      if (ctree_row->mask_opened)
	gdk_bitmap_unref (ctree_row->mask_opened);
    }

  if (ctree_row->row.destroy)
    {
      GtkDestroyNotify dnotify = ctree_row->row.destroy;
      gpointer ddata = ctree_row->row.data;

      ctree_row->row.destroy = NULL;
      ctree_row->row.data = NULL;

      dnotify (ddata);
    }

  g_mem_chunk_free (clist->cell_mem_chunk, ctree_row->row.cell);
  g_mem_chunk_free (clist->row_mem_chunk, ctree_row);
}

static void
real_select_row (GtkCList *clist,
		 gint      row,
		 gint      column,
		 GdkEvent *event)
{
  GList *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));
  
  if ((node = g_list_nth (clist->row_list, row)) &&
      NAUTILUS_CTREE_ROW (node)->row.selectable)
    gtk_signal_emit (GTK_OBJECT (clist), ctree_signals[TREE_SELECT_ROW],
		     node, column);
}

static void
real_unselect_row (GtkCList *clist,
		   gint      row,
		   gint      column,
		   GdkEvent *event)
{
  GList *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  if ((node = g_list_nth (clist->row_list, row)))
    gtk_signal_emit (GTK_OBJECT (clist), ctree_signals[TREE_UNSELECT_ROW],
		     node, column);
}

static void
real_tree_select (NautilusCTree     *ctree,
		  NautilusCTreeNode *node,
		  gint          column)
{
  GtkCList *clist;
  GList *list;
  NautilusCTreeNode *sel_row;
  gboolean node_selected;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (!node || NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED ||
      !NAUTILUS_CTREE_ROW (node)->row.selectable)
    return;

  clist = GTK_CLIST (ctree);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:

      node_selected = FALSE;
      list = clist->selection;

      while (list)
	{
	  sel_row = list->data;
	  list = list->next;
	  
	  if (node == sel_row)
	    node_selected = TRUE;
	  else
	    gtk_signal_emit (GTK_OBJECT (ctree),
			     ctree_signals[TREE_UNSELECT_ROW], sel_row, column);
	}

      if (node_selected)
	return;

    default:
      break;
    }

  NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;

  if (!clist->selection)
    {
      clist->selection = g_list_append (clist->selection, node);
      clist->selection_end = clist->selection;
    }
  else
    clist->selection_end = g_list_append (clist->selection_end, node)->next;

  tree_draw_node (ctree, node);
}

static void
real_tree_unselect (NautilusCTree     *ctree,
		    NautilusCTreeNode *node,
		    gint          column)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (!node || NAUTILUS_CTREE_ROW (node)->row.state != GTK_STATE_SELECTED)
    return;

  clist = GTK_CLIST (ctree);

  if (clist->selection_end && clist->selection_end->data == node)
    clist->selection_end = clist->selection_end->prev;

  clist->selection = g_list_remove (clist->selection, node);
  
  NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;

  tree_draw_node (ctree, node);
}

static void
select_row_recursive (NautilusCTree     *ctree, 
		      NautilusCTreeNode *node, 
		      gpointer      data)
{
  if (!node || NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED ||
      !NAUTILUS_CTREE_ROW (node)->row.selectable)
    return;

  GTK_CLIST (ctree)->undo_unselection = 
    g_list_prepend (GTK_CLIST (ctree)->undo_unselection, node);
  nautilus_ctree_select (ctree, node);
}

static void
real_select_all (GtkCList *clist)
{
  NautilusCTree *ctree;
  NautilusCTreeNode *node;
  
  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  ctree = NAUTILUS_CTREE (clist);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      return;

    case GTK_SELECTION_EXTENDED:

      gtk_clist_freeze (clist);

      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
	  
      clist->anchor_state = GTK_STATE_SELECTED;
      clist->anchor = -1;
      clist->drag_pos = -1;
      clist->undo_anchor = clist->focus_row;

      for (node = NAUTILUS_CTREE_NODE (clist->row_list); node;
	   node = NAUTILUS_CTREE_NODE_NEXT (node))
	nautilus_ctree_pre_recursive (ctree, node, select_row_recursive, NULL);

      gtk_clist_thaw (clist);
      break;

    case GTK_SELECTION_MULTIPLE:
      nautilus_ctree_select_recursive (ctree, NULL);
      break;

    default:
      /* do nothing */
      break;
    }
}

static void
real_unselect_all (GtkCList *clist)
{
  NautilusCTree *ctree;
  NautilusCTreeNode *node;
  GList *list;
 
  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));
  
  ctree = NAUTILUS_CTREE (clist);

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_BROWSE:
      if (clist->focus_row >= 0)
	{
	  nautilus_ctree_select
	    (ctree,
	     NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row)));
	  return;
	}
      break;

    case GTK_SELECTION_EXTENDED:
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;

      clist->anchor = -1;
      clist->drag_pos = -1;
      clist->undo_anchor = clist->focus_row;
      break;

    default:
      break;
    }

  list = clist->selection;

  while (list)
    {
      node = list->data;
      list = list->next;
      nautilus_ctree_unselect (ctree, node);
    }
}

static gboolean
ctree_is_hot_spot (NautilusCTree     *ctree, 
		   NautilusCTreeNode *node,
		   gint          row, 
		   gint          x, 
		   gint          y)
{
	NautilusCTreeRow *tree_row;
	GtkCList *clist;
	GtkCellPixText *cell;
	gint xl;
	gint yu;
  
	g_return_val_if_fail (ctree != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);

	clist = GTK_CLIST (ctree);

	if (!clist->column[ctree->tree_column].visible) {
		return FALSE;
	}

	tree_row = NAUTILUS_CTREE_ROW (node);

	cell = GTK_CELL_PIXTEXT(tree_row->row.cell[ctree->tree_column]);

	yu = (ROW_TOP_YPIXEL (clist, row) + (clist->row_height - PM_SIZE) / 2 - (clist->row_height - 1) % 2);

	if (clist->column[ctree->tree_column].justification == GTK_JUSTIFY_RIGHT) {
		xl = (clist->column[ctree->tree_column].area.x + 
	  	      clist->column[ctree->tree_column].area.width - 1 + clist->hoffset -
	  	     (tree_row->level - 1) * ctree->tree_indent - PM_SIZE -
	  	     (ctree->line_style == NAUTILUS_CTREE_LINES_TABBED) * 3);
	} else {
    		xl = (clist->column[ctree->tree_column].area.x + clist->hoffset +
	  	     (tree_row->level - 1) * ctree->tree_indent +
	  	     (ctree->line_style == NAUTILUS_CTREE_LINES_TABBED) * 3);
	}

	return (x >= xl - 3 && x <= xl + 3 + PM_SIZE && y >= yu - 3 && y <= yu + PM_SIZE + 3);
}

/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/


/***********************************************************
 *           Creation, insertion, deletion                 *
 ***********************************************************/

void
nautilus_ctree_construct (NautilusCTree    *ctree,
		     gint         columns, 
		     gint         tree_column,
		     gchar       *titles[])
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (GTK_OBJECT_CONSTRUCTED (ctree) == FALSE);

  clist = GTK_CLIST (ctree);

  clist->row_mem_chunk = g_mem_chunk_new ("ctree row mem chunk",
					  sizeof (NautilusCTreeRow),
					  sizeof (NautilusCTreeRow)
					  * CLIST_OPTIMUM_SIZE, 
					  G_ALLOC_AND_FREE);

  clist->cell_mem_chunk = g_mem_chunk_new ("ctree cell mem chunk",
					   sizeof (GtkCell) * columns,
					   sizeof (GtkCell) * columns
					   * CLIST_OPTIMUM_SIZE, 
					   G_ALLOC_AND_FREE);

  ctree->tree_column = tree_column;

  gtk_clist_construct (clist, columns, titles);
}

GtkWidget *
nautilus_ctree_new_with_titles (gint         columns, 
			   gint         tree_column,
			   gchar       *titles[])
{
	GtkWidget *widget;

	g_return_val_if_fail (columns > 0, NULL);
	g_return_val_if_fail (tree_column >= 0 && tree_column < columns, NULL);

	widget = GTK_WIDGET (gtk_type_new (NAUTILUS_TYPE_CTREE));
	nautilus_ctree_construct (NAUTILUS_CTREE (widget), columns, tree_column, titles);

	return widget;
}

GtkWidget *
nautilus_ctree_new (gint columns, 
	       gint tree_column)
{
  return nautilus_ctree_new_with_titles (columns, tree_column, NULL);
}

static gint
real_insert_row (GtkCList *clist,
		 gint      row,
		 gchar    *text[])
{
  NautilusCTreeNode *parent = NULL;
  NautilusCTreeNode *sibling;
  NautilusCTreeNode *node;

  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (clist), -1);

  sibling = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, row));
  if (sibling)
    parent = NAUTILUS_CTREE_ROW (sibling)->parent;

  node = nautilus_ctree_insert_node (NAUTILUS_CTREE (clist), parent, sibling, text, 5,
				     NULL, NULL, NULL, NULL, TRUE, FALSE);

  if (GTK_CLIST_AUTO_SORT (clist) || !sibling)
    return g_list_position (clist->row_list, (GList *) node);
  
  return row;
}

NautilusCTreeNode * 
nautilus_ctree_insert_node (NautilusCTree     *ctree,
		       NautilusCTreeNode *parent, 
		       NautilusCTreeNode *sibling,
		       gchar        *text[],
		       guint8        spacing,
		       GdkPixmap    *pixmap_closed,
		       GdkBitmap    *mask_closed,
		       GdkPixmap    *pixmap_opened,
		       GdkBitmap    *mask_opened,
		       gboolean      is_leaf,
		       gboolean      expanded)
{
	GtkCList *clist;
	NautilusCTreeRow *new_row;
	NautilusCTreeNode *node;
	GList *list;
	gint i;

	g_return_val_if_fail (ctree != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);
	if (sibling) {
    		g_return_val_if_fail (NAUTILUS_CTREE_ROW (sibling)->parent == parent, NULL);
    	}

	if (parent && NAUTILUS_CTREE_ROW (parent)->is_leaf) {
    		return NULL;
    	}

	clist = GTK_CLIST (ctree);

	/* create the row */
	new_row = row_new (ctree);
	list = g_list_alloc ();
	list->data = new_row;
	node = NAUTILUS_CTREE_NODE (list);

  if (text)
    for (i = 0; i < clist->columns; i++)
      if (text[i] && i != ctree->tree_column)
	GTK_CLIST_CLASS_FW (clist)->set_cell_contents
	  (clist, &(new_row->row), i, GTK_CELL_TEXT, text[i], 0, NULL, NULL);

  set_node_info (ctree, node, text ?
		 text[ctree->tree_column] : NULL, spacing, pixmap_closed,
		 mask_closed, pixmap_opened, mask_opened, is_leaf, expanded);

  /* sorted insertion */
  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (parent)
	sibling = NAUTILUS_CTREE_ROW (parent)->children;
      else
	sibling = NAUTILUS_CTREE_NODE (clist->row_list);

      while (sibling && clist->compare
	     (clist, NAUTILUS_CTREE_ROW (node), NAUTILUS_CTREE_ROW (sibling)) > 0)
	sibling = NAUTILUS_CTREE_ROW (sibling)->sibling;
    }

  nautilus_ctree_link (ctree, node, parent, sibling, TRUE);

  if (text && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist) &&
      nautilus_ctree_is_viewable (ctree, node))
    {
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  column_auto_resize (clist, &(new_row->row), i, 0);
    }

  if (clist->rows == 1)
    {
      clist->focus_row = 0;
      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	nautilus_ctree_select (ctree, node);
    }


  CLIST_REFRESH (clist);

  return node;
}

NautilusCTreeNode *
nautilus_ctree_insert_gnode (NautilusCTree          *ctree,
			NautilusCTreeNode      *parent,
			NautilusCTreeNode      *sibling,
			GNode             *gnode,
			NautilusCTreeGNodeFunc  func,
			gpointer           data)
{
  GtkCList *clist;
  NautilusCTreeNode *cnode = NULL;
  NautilusCTreeNode *child = NULL;
  NautilusCTreeNode *new_child;
  GList *list;
  GNode *work;
  guint depth = 1;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (gnode != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  if (sibling)
    g_return_val_if_fail (NAUTILUS_CTREE_ROW (sibling)->parent == parent, NULL);
  
  clist = GTK_CLIST (ctree);

  if (parent)
    depth = NAUTILUS_CTREE_ROW (parent)->level + 1;

  list = g_list_alloc ();
  list->data = row_new (ctree);
  cnode = NAUTILUS_CTREE_NODE (list);

  gtk_clist_freeze (clist);

  set_node_info (ctree, cnode, "", 0, NULL, NULL, NULL, NULL, TRUE, FALSE);

  if (!func (ctree, depth, gnode, cnode, data))
    {
      tree_delete_row (ctree, cnode, NULL);
      return NULL;
    }

  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (parent)
	sibling = NAUTILUS_CTREE_ROW (parent)->children;
      else
	sibling = NAUTILUS_CTREE_NODE (clist->row_list);

      while (sibling && clist->compare
	     (clist, NAUTILUS_CTREE_ROW (cnode), NAUTILUS_CTREE_ROW (sibling)) > 0)
	sibling = NAUTILUS_CTREE_ROW (sibling)->sibling;
    }

  nautilus_ctree_link (ctree, cnode, parent, sibling, TRUE);

  for (work = g_node_last_child (gnode); work; work = work->prev)
    {
      new_child = nautilus_ctree_insert_gnode (ctree, cnode, child,
					  work, func, data);
      if (new_child)
	child = new_child;
    }	
  
  gtk_clist_thaw (clist);

  return cnode;
}

GNode *
nautilus_ctree_export_to_gnode (NautilusCTree          *ctree,
			   GNode             *parent,
			   GNode             *sibling,
			   NautilusCTreeNode      *node,
			   NautilusCTreeGNodeFunc  func,
			   gpointer           data)
{
  NautilusCTreeNode *work;
  GNode *gnode;
  gint depth;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  if (sibling)
    {
      g_return_val_if_fail (parent != NULL, NULL);
      g_return_val_if_fail (sibling->parent == parent, NULL);
    }

  gnode = g_node_new (NULL);
  depth = g_node_depth (parent) + 1;
  
  if (!func (ctree, depth, gnode, node, data))
    {
      g_node_destroy (gnode);
      return NULL;
    }

  if (parent)
    g_node_insert_before (parent, sibling, gnode);

  if (!NAUTILUS_CTREE_ROW (node)->is_leaf)
    {
      GNode *new_sibling = NULL;

      for (work = NAUTILUS_CTREE_ROW (node)->children; work;
	   work = NAUTILUS_CTREE_ROW (work)->sibling)
	new_sibling = nautilus_ctree_export_to_gnode (ctree, gnode, new_sibling,
						 work, func, data);

      g_node_reverse_children (gnode);
    }

  return gnode;
}
  
static void
real_remove_row (GtkCList *clist,
		 gint      row)
{
  NautilusCTreeNode *node;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, row));

  if (node)
    nautilus_ctree_remove_node (NAUTILUS_CTREE (clist), node);
}

void
nautilus_ctree_remove_node (NautilusCTree     *ctree, 
		       NautilusCTreeNode *node)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  if (node)
    {
      gboolean visible;

      visible = nautilus_ctree_is_viewable (ctree, node);
      nautilus_ctree_unlink (ctree, node, TRUE);
      nautilus_ctree_post_recursive (ctree, node, NAUTILUS_CTREE_FUNC (tree_delete),
				NULL);
      if (clist->selection_mode == GTK_SELECTION_BROWSE && !clist->selection &&
	  clist->focus_row >= 0)
	gtk_clist_select_row (clist, clist->focus_row, -1);

      auto_resize_columns (clist);
    }
  else
    gtk_clist_clear (clist);

  gtk_clist_thaw (clist);
}

static void
real_clear (GtkCList *clist)
{
  NautilusCTree *ctree;
  NautilusCTreeNode *work;
  NautilusCTreeNode *ptr;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  ctree = NAUTILUS_CTREE (clist);

  /* remove all rows */
  work = NAUTILUS_CTREE_NODE (clist->row_list);
  clist->row_list = NULL;
  clist->row_list_end = NULL;

  GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  while (work)
    {
      ptr = work;
      work = NAUTILUS_CTREE_ROW (work)->sibling;
      nautilus_ctree_post_recursive (ctree, ptr, NAUTILUS_CTREE_FUNC (tree_delete_row), 
				NULL);
    }
  GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);

  parent_class->clear (clist);
}


/***********************************************************
 *  Generic recursive functions, querying / finding tree   *
 *  information                                            *
 ***********************************************************/


void
nautilus_ctree_post_recursive (NautilusCTree     *ctree, 
			  NautilusCTreeNode *node,
			  NautilusCTreeFunc  func,
			  gpointer      data)
{
  NautilusCTreeNode *work;
  NautilusCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (node)
    work = NAUTILUS_CTREE_ROW (node)->children;
  else
    work = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (work)
    {
      tmp = NAUTILUS_CTREE_ROW (work)->sibling;
      nautilus_ctree_post_recursive (ctree, work, func, data);
      work = tmp;
    }

  if (node)
    func (ctree, node, data);
}

void
nautilus_ctree_post_recursive_to_depth (NautilusCTree     *ctree, 
				   NautilusCTreeNode *node,
				   gint          depth,
				   NautilusCTreeFunc  func,
				   gpointer      data)
{
  NautilusCTreeNode *work;
  NautilusCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (depth < 0)
    {
      nautilus_ctree_post_recursive (ctree, node, func, data);
      return;
    }

  if (node)
    work = NAUTILUS_CTREE_ROW (node)->children;
  else
    work = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  if (work && NAUTILUS_CTREE_ROW (work)->level <= depth)
    {
      while (work)
	{
	  tmp = NAUTILUS_CTREE_ROW (work)->sibling;
	  nautilus_ctree_post_recursive_to_depth (ctree, work, depth, func, data);
	  work = tmp;
	}
    }

  if (node && NAUTILUS_CTREE_ROW (node)->level <= depth)
    func (ctree, node, data);
}

void
nautilus_ctree_pre_recursive (NautilusCTree     *ctree, 
			 NautilusCTreeNode *node,
			 NautilusCTreeFunc  func,
			 gpointer      data)
{
  NautilusCTreeNode *work;
  NautilusCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (node)
    {
      work = NAUTILUS_CTREE_ROW (node)->children;
      func (ctree, node, data);
    }
  else
    work = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (work)
    {
      tmp = NAUTILUS_CTREE_ROW (work)->sibling;
      nautilus_ctree_pre_recursive (ctree, work, func, data);
      work = tmp;
    }
}

void
nautilus_ctree_pre_recursive_to_depth (NautilusCTree     *ctree, 
				  NautilusCTreeNode *node,
				  gint          depth, 
				  NautilusCTreeFunc  func,
				  gpointer      data)
{
  NautilusCTreeNode *work;
  NautilusCTreeNode *tmp;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (func != NULL);

  if (depth < 0)
    {
      nautilus_ctree_pre_recursive (ctree, node, func, data);
      return;
    }

  if (node)
    {
      work = NAUTILUS_CTREE_ROW (node)->children;
      if (NAUTILUS_CTREE_ROW (node)->level <= depth)
	func (ctree, node, data);
    }
  else
    work = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  if (work && NAUTILUS_CTREE_ROW (work)->level <= depth)
    {
      while (work)
	{
	  tmp = NAUTILUS_CTREE_ROW (work)->sibling;
	  nautilus_ctree_pre_recursive_to_depth (ctree, work, depth, func, data);
	  work = tmp;
	}
    }
}

gboolean
nautilus_ctree_is_viewable (NautilusCTree     *ctree, 
		       NautilusCTreeNode *node)
{ 
  NautilusCTreeRow *work;

  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  work = NAUTILUS_CTREE_ROW (node);

  while (work->parent && NAUTILUS_CTREE_ROW (work->parent)->expanded)
    work = NAUTILUS_CTREE_ROW (work->parent);

  if (!work->parent)
    return TRUE;

  return FALSE;
}

NautilusCTreeNode * 
nautilus_ctree_last (NautilusCTree     *ctree,
		     NautilusCTreeNode *node)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);

  if (!node) 
    return NULL;

  while (NAUTILUS_CTREE_ROW (node)->sibling)
    node = NAUTILUS_CTREE_ROW (node)->sibling;
  
  if (NAUTILUS_CTREE_ROW (node)->children)
    return nautilus_ctree_last (ctree, NAUTILUS_CTREE_ROW (node)->children);
  
  return node;
}

NautilusCTreeNode *
nautilus_ctree_find_node_ptr (NautilusCTree    *ctree,
			 NautilusCTreeRow *ctree_row)
{
  NautilusCTreeNode *node;
  
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (ctree_row != NULL, FALSE);
  
  if (ctree_row->parent)
    node = NAUTILUS_CTREE_ROW (ctree_row->parent)->children;
  else
    node = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (NAUTILUS_CTREE_ROW (node) != ctree_row)
    node = NAUTILUS_CTREE_ROW (node)->sibling;
  
  return node;
}

NautilusCTreeNode *
nautilus_ctree_node_nth (NautilusCTree *ctree,
			 int     row)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);

  if ((row < 0) || (row >= GTK_CLIST(ctree)->rows))
    return NULL;
 
  return NAUTILUS_CTREE_NODE (g_list_nth (GTK_CLIST (ctree)->row_list, row));
}

gboolean
nautilus_ctree_find (NautilusCTree     *ctree,
		     NautilusCTreeNode *node,
		     NautilusCTreeNode *child)
{
  if (!child)
    return FALSE;

  if (!node)
    node = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (node == child) 
	return TRUE;
      if (NAUTILUS_CTREE_ROW (node)->children)
	{
	  if (nautilus_ctree_find (ctree, NAUTILUS_CTREE_ROW (node)->children, child))
	    return TRUE;
	}
      node = NAUTILUS_CTREE_ROW (node)->sibling;
    }
  return FALSE;
}

gboolean
nautilus_ctree_is_ancestor (NautilusCTree     *ctree,
		       NautilusCTreeNode *node,
		       NautilusCTreeNode *child)
{
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  if (NAUTILUS_CTREE_ROW (node)->children)
    return nautilus_ctree_find (ctree, NAUTILUS_CTREE_ROW (node)->children, child);

  return FALSE;
}

NautilusCTreeNode *
nautilus_ctree_find_by_row_data (NautilusCTree     *ctree,
			    NautilusCTreeNode *node,
			    gpointer      data)
{
  NautilusCTreeNode *work;
  
  if (!node)
    node = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);
  
  while (node)
    {
      if (NAUTILUS_CTREE_ROW (node)->row.data == data) 
	return node;
      if (NAUTILUS_CTREE_ROW (node)->children &&
	  (work = nautilus_ctree_find_by_row_data 
	   (ctree, NAUTILUS_CTREE_ROW (node)->children, data)))
	return work;
      node = NAUTILUS_CTREE_ROW (node)->sibling;
    }
  return NULL;
}

GList *
nautilus_ctree_find_all_by_row_data (NautilusCTree     *ctree,
				NautilusCTreeNode *node,
				gpointer      data)
{
  GList *list = NULL;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);

  /* if node == NULL then look in the whole tree */
  if (!node)
    node = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (NAUTILUS_CTREE_ROW (node)->row.data == data)
        list = g_list_append (list, node);

      if (NAUTILUS_CTREE_ROW (node)->children)
        {
	  GList *sub_list;

          sub_list = nautilus_ctree_find_all_by_row_data (ctree,
							  NAUTILUS_CTREE_ROW
							  (node)->children,
							  data);
          list = g_list_concat (list, sub_list);
        }
      node = NAUTILUS_CTREE_ROW (node)->sibling;
    }
  return list;
}

NautilusCTreeNode *
nautilus_ctree_find_by_row_data_custom (NautilusCTree     *ctree,
				   NautilusCTreeNode *node,
				   gpointer      data,
				   GCompareFunc  func)
{
  NautilusCTreeNode *work;

  g_return_val_if_fail (func != NULL, NULL);

  if (!node)
    node = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (!func (NAUTILUS_CTREE_ROW (node)->row.data, data))
	return node;
      if (NAUTILUS_CTREE_ROW (node)->children &&
	  (work = nautilus_ctree_find_by_row_data_custom
	   (ctree, NAUTILUS_CTREE_ROW (node)->children, data, func)))
	return work;
      node = NAUTILUS_CTREE_ROW (node)->sibling;
    }
  return NULL;
}

GList *
nautilus_ctree_find_all_by_row_data_custom (NautilusCTree     *ctree,
				       NautilusCTreeNode *node,
				       gpointer      data,
				       GCompareFunc  func)
{
  GList *list = NULL;

  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (func != NULL, NULL);

  /* if node == NULL then look in the whole tree */
  if (!node)
    node = NAUTILUS_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  while (node)
    {
      if (!func (NAUTILUS_CTREE_ROW (node)->row.data, data))
        list = g_list_append (list, node);

      if (NAUTILUS_CTREE_ROW (node)->children)
        {
	  GList *sub_list;

          sub_list = nautilus_ctree_find_all_by_row_data_custom (ctree,
							    NAUTILUS_CTREE_ROW
							    (node)->children,
							    data,
							    func);
          list = g_list_concat (list, sub_list);
        }
      node = NAUTILUS_CTREE_ROW (node)->sibling;
    }
  return list;
}

gboolean
nautilus_ctree_is_hot_spot (NautilusCTree *ctree, 
		       	    gint      x, 
		       	    gint      y)
{
	NautilusCTreeNode *node;
	gint column;
	gint row;
  
	g_return_val_if_fail (ctree != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), FALSE);

	if (gtk_clist_get_selection_info (GTK_CLIST (ctree), x, y, &row, &column)) {
		if ((node = NAUTILUS_CTREE_NODE(g_list_nth (GTK_CLIST (ctree)->row_list, row)))) {
			return ctree_is_hot_spot (ctree, node, row, x, y);
		}
	}
	
	return FALSE;
}


/***********************************************************
 *   Tree signals : move, expand, collapse, (un)select     *
 ***********************************************************/


void
nautilus_ctree_move (NautilusCTree     *ctree,
		NautilusCTreeNode *node,
		NautilusCTreeNode *new_parent, 
		NautilusCTreeNode *new_sibling)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_MOVE], node,
		   new_parent, new_sibling);
}

void
nautilus_ctree_expand (NautilusCTree     *ctree,
		  NautilusCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  if (NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_EXPAND], node);
}

void 
nautilus_ctree_expand_recursive (NautilusCTree     *ctree,
			    NautilusCTreeNode *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (node && NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  if (CLIST_UNFROZEN (clist) && (!node || nautilus_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  nautilus_ctree_post_recursive (ctree, node, NAUTILUS_CTREE_FUNC (tree_expand), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void 
nautilus_ctree_expand_to_depth (NautilusCTree     *ctree,
			   NautilusCTreeNode *node,
			   gint          depth)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if (node && NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  if (CLIST_UNFROZEN (clist) && (!node || nautilus_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  nautilus_ctree_post_recursive_to_depth (ctree, node, depth,
				     NAUTILUS_CTREE_FUNC (tree_expand), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void
nautilus_ctree_collapse (NautilusCTree     *ctree,
		    NautilusCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  if (NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_COLLAPSE], node);
}

void 
nautilus_ctree_collapse_recursive (NautilusCTree     *ctree,
			      NautilusCTreeNode *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;
  gint i;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (node && NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && (!node || nautilus_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  nautilus_ctree_post_recursive (ctree, node, NAUTILUS_CTREE_FUNC (tree_collapse), NULL);
  GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].auto_resize)
      gtk_clist_set_column_width (clist, i,
				  gtk_clist_optimal_column_width (clist, i));

  if (thaw)
    gtk_clist_thaw (clist);
}

void 
nautilus_ctree_collapse_to_depth (NautilusCTree     *ctree,
			     NautilusCTreeNode *node,
			     gint          depth)
{
  GtkCList *clist;
  gboolean thaw = FALSE;
  gint i;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (node && NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && (!node || nautilus_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  nautilus_ctree_post_recursive_to_depth (ctree, node, depth,
				     NAUTILUS_CTREE_FUNC (tree_collapse_to_depth),
				     GINT_TO_POINTER (depth));
  GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_RESIZE_BLOCKED);
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].auto_resize)
      gtk_clist_set_column_width (clist, i,
				  gtk_clist_optimal_column_width (clist, i));

  if (thaw)
    gtk_clist_thaw (clist);
}

void
nautilus_ctree_toggle_expansion (NautilusCTree     *ctree,
			    NautilusCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  
  if (NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  tree_toggle_expansion (ctree, node, NULL);
}

void 
nautilus_ctree_toggle_expansion_recursive (NautilusCTree     *ctree,
				      NautilusCTreeNode *node)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  
  if (node && NAUTILUS_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && (!node || nautilus_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }
  
  nautilus_ctree_post_recursive (ctree, node,
			    NAUTILUS_CTREE_FUNC (tree_toggle_expansion), NULL);

  if (thaw)
    gtk_clist_thaw (clist);
}

void
nautilus_ctree_select (NautilusCTree     *ctree, 
		  NautilusCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (NAUTILUS_CTREE_ROW (node)->row.selectable)
    gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_SELECT_ROW],
		     node, -1);
}

void
nautilus_ctree_unselect (NautilusCTree     *ctree, 
		    NautilusCTreeNode *node)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  gtk_signal_emit (GTK_OBJECT (ctree), ctree_signals[TREE_UNSELECT_ROW],
		   node, -1);
}

void
nautilus_ctree_select_recursive (NautilusCTree     *ctree, 
			    NautilusCTreeNode *node)
{
  nautilus_ctree_real_select_recursive (ctree, node, TRUE);
}

void
nautilus_ctree_unselect_recursive (NautilusCTree     *ctree, 
			      NautilusCTreeNode *node)
{
  nautilus_ctree_real_select_recursive (ctree, node, FALSE);
}

void
nautilus_ctree_real_select_recursive (NautilusCTree     *ctree, 
				 NautilusCTreeNode *node, 
				 gint          state)
{
  GtkCList *clist;
  gboolean thaw = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  if ((state && 
       (clist->selection_mode ==  GTK_SELECTION_BROWSE ||
	clist->selection_mode == GTK_SELECTION_SINGLE)) ||
      (!state && clist->selection_mode ==  GTK_SELECTION_BROWSE))
    return;

  if (CLIST_UNFROZEN (clist) && (!node || nautilus_ctree_is_viewable (ctree, node)))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (state)
    nautilus_ctree_post_recursive (ctree, node,
			      NAUTILUS_CTREE_FUNC (tree_select), NULL);
  else 
    nautilus_ctree_post_recursive (ctree, node,
			      NAUTILUS_CTREE_FUNC (tree_unselect), NULL);
  
  if (thaw)
    gtk_clist_thaw (clist);
}


/***********************************************************
 *           Analogons of GtkCList functions               *
 ***********************************************************/


void 
nautilus_ctree_node_set_text (NautilusCTree     *ctree,
			 NautilusCTreeNode *node,
			 gint          column,
			 const gchar  *text)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;
  
  clist = GTK_CLIST (ctree);

  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, &(NAUTILUS_CTREE_ROW(node)->row), column, GTK_CELL_TEXT,
     text, 0, NULL, NULL);

  tree_draw_node (ctree, node);
}

void 
nautilus_ctree_node_set_pixmap (NautilusCTree     *ctree,
			   NautilusCTreeNode *node,
			   gint          column,
			   GdkPixmap    *pixmap,
			   GdkBitmap    *mask)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  g_return_if_fail (pixmap != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  gdk_pixmap_ref (pixmap);
  if (mask) 
    gdk_pixmap_ref (mask);

  clist = GTK_CLIST (ctree);

  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, &(NAUTILUS_CTREE_ROW (node)->row), column, GTK_CELL_PIXMAP,
     NULL, 0, pixmap, mask);

  tree_draw_node (ctree, node);
}

void 
nautilus_ctree_node_set_pixtext (NautilusCTree     *ctree,
			    NautilusCTreeNode *node,
			    gint          column,
			    const gchar  *text,
			    guint8        spacing,
			    GdkPixmap    *pixmap,
			    GdkBitmap    *mask)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);
  if (column != ctree->tree_column)
    g_return_if_fail (pixmap != NULL);
  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  clist = GTK_CLIST (ctree);

  if (pixmap)
    {
      gdk_pixmap_ref (pixmap);
      if (mask) 
	gdk_pixmap_ref (mask);
    }

  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, &(NAUTILUS_CTREE_ROW (node)->row), column, GTK_CELL_PIXTEXT,
     text, spacing, pixmap, mask);

  tree_draw_node (ctree, node);
}

void 
nautilus_ctree_set_node_info (NautilusCTree     *ctree,
			 NautilusCTreeNode *node,
			 const gchar  *text,
			 guint8        spacing,
			 GdkPixmap    *pixmap_closed,
			 GdkBitmap    *mask_closed,
			 GdkPixmap    *pixmap_opened,
			 GdkBitmap    *mask_opened,
			 gboolean      is_leaf,
			 gboolean      expanded)
{
  gboolean old_leaf;
  gboolean old_expanded;
 
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  old_leaf = NAUTILUS_CTREE_ROW (node)->is_leaf;
  old_expanded = NAUTILUS_CTREE_ROW (node)->expanded;

  if (is_leaf && NAUTILUS_CTREE_ROW (node)->children)
    {
      NautilusCTreeNode *work;
      NautilusCTreeNode *ptr;
      
      work = NAUTILUS_CTREE_ROW (node)->children;
      while (work)
	{
	  ptr = work;
	  work = NAUTILUS_CTREE_ROW(work)->sibling;
	  nautilus_ctree_remove_node (ctree, ptr);
	}
    }

  set_node_info (ctree, node, text, spacing, pixmap_closed, mask_closed,
		 pixmap_opened, mask_opened, is_leaf, expanded);

  if (!is_leaf && !old_leaf)
    {
      NAUTILUS_CTREE_ROW (node)->expanded = old_expanded;
      if (expanded && !old_expanded)
	nautilus_ctree_expand (ctree, node);
      else if (!expanded && old_expanded)
	nautilus_ctree_collapse (ctree, node);
    }

  NAUTILUS_CTREE_ROW (node)->expanded = (is_leaf) ? FALSE : expanded;
  
  tree_draw_node (ctree, node);
}

void
nautilus_ctree_node_set_shift (NautilusCTree     *ctree,
			  NautilusCTreeNode *node,
			  gint          column,
			  gint          vertical,
			  gint          horizontal)
{
  GtkCList *clist;
  GtkRequisition requisition;
  gboolean visible = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return;

  clist = GTK_CLIST (ctree);

  if (clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      visible = nautilus_ctree_is_viewable (ctree, node);
      if (visible)
	GTK_CLIST_CLASS_FW (clist)->cell_size_request
	  (clist, &NAUTILUS_CTREE_ROW (node)->row, column, &requisition);
    }

  NAUTILUS_CTREE_ROW (node)->row.cell[column].vertical   = vertical;
  NAUTILUS_CTREE_ROW (node)->row.cell[column].horizontal = horizontal;

  if (visible)
    column_auto_resize (clist, &NAUTILUS_CTREE_ROW (node)->row,
			column, requisition.width);

  tree_draw_node (ctree, node);
}

static void
remove_grab (GtkCList *clist)
{
  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    {
      gtk_grab_remove (GTK_WIDGET (clist));
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
    }

  if (clist->htimer)
    {
      gtk_timeout_remove (clist->htimer);
      clist->htimer = 0;
    }

  if (clist->vtimer)
    {
      gtk_timeout_remove (clist->vtimer);
      clist->vtimer = 0;
    }
}

void
nautilus_ctree_node_set_selectable (NautilusCTree     *ctree,
			       NautilusCTreeNode *node,
			       gboolean      selectable)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (selectable == NAUTILUS_CTREE_ROW (node)->row.selectable)
    return;

  NAUTILUS_CTREE_ROW (node)->row.selectable = selectable;

  if (!selectable && NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
    {
      GtkCList *clist;

      clist = GTK_CLIST (ctree);

      if (clist->anchor >= 0 &&
	  clist->selection_mode == GTK_SELECTION_EXTENDED)
	{
	  clist->drag_button = 0;
	  remove_grab (clist);

	  GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
	}
      nautilus_ctree_unselect (ctree, node);
    }      
}

gboolean
nautilus_ctree_node_get_selectable (NautilusCTree     *ctree,
			       NautilusCTreeNode *node)
{
  g_return_val_if_fail (node != NULL, FALSE);

  return NAUTILUS_CTREE_ROW (node)->row.selectable;
}

GtkCellType 
nautilus_ctree_node_get_cell_type (NautilusCTree     *ctree,
			      NautilusCTreeNode *node,
			      gint          column)
{
  g_return_val_if_fail (ctree != NULL, -1);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), -1);
  g_return_val_if_fail (node != NULL, -1);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return -1;

  return NAUTILUS_CTREE_ROW (node)->row.cell[column].type;
}

gint
nautilus_ctree_node_get_text (NautilusCTree      *ctree,
			 NautilusCTreeNode  *node,
			 gint           column,
			 gchar        **text)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return 0;

  if (NAUTILUS_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_TEXT)
    return 0;

  if (text)
    *text = GTK_CELL_TEXT (NAUTILUS_CTREE_ROW (node)->row.cell[column])->text;

  return 1;
}

gint
nautilus_ctree_node_get_pixmap (NautilusCTree     *ctree,
			   NautilusCTreeNode *node,
			   gint          column,
			   GdkPixmap   **pixmap,
			   GdkBitmap   **mask)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return 0;

  if (NAUTILUS_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_PIXMAP)
    return 0;

  if (pixmap)
    *pixmap = GTK_CELL_PIXMAP (NAUTILUS_CTREE_ROW(node)->row.cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXMAP (NAUTILUS_CTREE_ROW (node)->row.cell[column])->mask;

  return 1;
}

gint
nautilus_ctree_node_get_pixtext (NautilusCTree      *ctree,
			    NautilusCTreeNode  *node,
			    gint           column,
			    gchar        **text,
			    guint8        *spacing,
			    GdkPixmap    **pixmap,
			    GdkBitmap    **mask)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);
  
  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return 0;
  
  if (NAUTILUS_CTREE_ROW (node)->row.cell[column].type != GTK_CELL_PIXTEXT)
    return 0;
  
  if (text)
    *text = GTK_CELL_PIXTEXT (NAUTILUS_CTREE_ROW (node)->row.cell[column])->text;
  if (spacing)
    *spacing = GTK_CELL_PIXTEXT (NAUTILUS_CTREE_ROW 
				 (node)->row.cell[column])->spacing;
  if (pixmap)
    *pixmap = GTK_CELL_PIXTEXT (NAUTILUS_CTREE_ROW 
				(node)->row.cell[column])->pixmap;
  if (mask)
    *mask = GTK_CELL_PIXTEXT (NAUTILUS_CTREE_ROW (node)->row.cell[column])->mask;
  
  return 1;
}

gint
nautilus_ctree_get_node_info (NautilusCTree      *ctree,
			 NautilusCTreeNode  *node,
			 gchar        **text,
			 guint8        *spacing,
			 GdkPixmap    **pixmap_closed,
			 GdkBitmap    **mask_closed,
			 GdkPixmap    **pixmap_opened,
			 GdkBitmap    **mask_opened,
			 gboolean      *is_leaf,
			 gboolean      *expanded)
{
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), 0);
  g_return_val_if_fail (node != NULL, 0);
  
  if (text)
    *text = GTK_CELL_PIXTEXT 
      (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->text;
  if (spacing)
    *spacing = GTK_CELL_PIXTEXT 
      (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->spacing;
  if (pixmap_closed)
    *pixmap_closed = NAUTILUS_CTREE_ROW (node)->pixmap_closed;
  if (mask_closed)
    *mask_closed = NAUTILUS_CTREE_ROW (node)->mask_closed;
  if (pixmap_opened)
    *pixmap_opened = NAUTILUS_CTREE_ROW (node)->pixmap_opened;
  if (mask_opened)
    *mask_opened = NAUTILUS_CTREE_ROW (node)->mask_opened;
  if (is_leaf)
    *is_leaf = NAUTILUS_CTREE_ROW (node)->is_leaf;
  if (expanded)
    *expanded = NAUTILUS_CTREE_ROW (node)->expanded;
  
  return 1;
}

void
nautilus_ctree_node_set_cell_style (NautilusCTree     *ctree,
			       NautilusCTreeNode *node,
			       gint          column,
			       GtkStyle     *style)
{
  GtkCList *clist;
  GtkRequisition requisition;
  gboolean visible = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);

  if (column < 0 || column >= clist->columns)
    return;

  if (NAUTILUS_CTREE_ROW (node)->row.cell[column].style == style)
    return;

  if (clist->column[column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      visible = nautilus_ctree_is_viewable (ctree, node);
      if (visible)
	GTK_CLIST_CLASS_FW (clist)->cell_size_request
	  (clist, &NAUTILUS_CTREE_ROW (node)->row, column, &requisition);
    }

  if (NAUTILUS_CTREE_ROW (node)->row.cell[column].style)
    {
      if (GTK_WIDGET_REALIZED (ctree))
        gtk_style_detach (NAUTILUS_CTREE_ROW (node)->row.cell[column].style);
      gtk_style_unref (NAUTILUS_CTREE_ROW (node)->row.cell[column].style);
    }

  NAUTILUS_CTREE_ROW (node)->row.cell[column].style = style;

  if (NAUTILUS_CTREE_ROW (node)->row.cell[column].style)
    {
      gtk_style_ref (NAUTILUS_CTREE_ROW (node)->row.cell[column].style);
      
      if (GTK_WIDGET_REALIZED (ctree))
        NAUTILUS_CTREE_ROW (node)->row.cell[column].style =
	  gtk_style_attach (NAUTILUS_CTREE_ROW (node)->row.cell[column].style,
			    clist->clist_window);
    }

  if (visible)
    column_auto_resize (clist, &NAUTILUS_CTREE_ROW (node)->row, column,
			requisition.width);

  tree_draw_node (ctree, node);
}

GtkStyle *
nautilus_ctree_node_get_cell_style (NautilusCTree     *ctree,
			       NautilusCTreeNode *node,
			       gint          column)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  if (column < 0 || column >= GTK_CLIST (ctree)->columns)
    return NULL;

  return NAUTILUS_CTREE_ROW (node)->row.cell[column].style;
}

void
nautilus_ctree_node_set_row_style (NautilusCTree     *ctree,
			      NautilusCTreeNode *node,
			      GtkStyle     *style)
{
  GtkCList *clist;
  GtkRequisition requisition;
  gboolean visible;
  gint *old_width = NULL;
  gint i;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  clist = GTK_CLIST (ctree);

  if (NAUTILUS_CTREE_ROW (node)->row.style == style)
    return;
  
  visible = nautilus_ctree_is_viewable (ctree, node);
  if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      old_width = g_new (gint, clist->columns);
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  {
	    GTK_CLIST_CLASS_FW (clist)->cell_size_request
	      (clist, &NAUTILUS_CTREE_ROW (node)->row, i, &requisition);
	    old_width[i] = requisition.width;
	  }
    }

  if (NAUTILUS_CTREE_ROW (node)->row.style)
    {
      if (GTK_WIDGET_REALIZED (ctree))
        gtk_style_detach (NAUTILUS_CTREE_ROW (node)->row.style);
      gtk_style_unref (NAUTILUS_CTREE_ROW (node)->row.style);
    }

  NAUTILUS_CTREE_ROW (node)->row.style = style;

  if (NAUTILUS_CTREE_ROW (node)->row.style)
    {
      gtk_style_ref (NAUTILUS_CTREE_ROW (node)->row.style);
      
      if (GTK_WIDGET_REALIZED (ctree))
        NAUTILUS_CTREE_ROW (node)->row.style =
	  gtk_style_attach (NAUTILUS_CTREE_ROW (node)->row.style,
			    clist->clist_window);
    }

  if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  column_auto_resize (clist, &NAUTILUS_CTREE_ROW (node)->row, i,
			      old_width[i]);
      g_free (old_width);
    }
  tree_draw_node (ctree, node);
}

GtkStyle *
nautilus_ctree_node_get_row_style (NautilusCTree     *ctree,
			      NautilusCTreeNode *node)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  return NAUTILUS_CTREE_ROW (node)->row.style;
}

void
nautilus_ctree_node_set_foreground (NautilusCTree     *ctree,
			       NautilusCTreeNode *node,
			       GdkColor     *color)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (color)
    {
      NAUTILUS_CTREE_ROW (node)->row.foreground = *color;
      NAUTILUS_CTREE_ROW (node)->row.fg_set = TRUE;
      if (GTK_WIDGET_REALIZED (ctree))
	gdk_color_alloc (gtk_widget_get_colormap (GTK_WIDGET (ctree)),
			 &NAUTILUS_CTREE_ROW (node)->row.foreground);
    }
  else
    NAUTILUS_CTREE_ROW (node)->row.fg_set = FALSE;

  tree_draw_node (ctree, node);
}

void
nautilus_ctree_node_set_background (NautilusCTree     *ctree,
			       NautilusCTreeNode *node,
			       GdkColor     *color)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  if (color)
    {
      NAUTILUS_CTREE_ROW (node)->row.background = *color;
      NAUTILUS_CTREE_ROW (node)->row.bg_set = TRUE;
      if (GTK_WIDGET_REALIZED (ctree))
	gdk_color_alloc (gtk_widget_get_colormap (GTK_WIDGET (ctree)),
			 &NAUTILUS_CTREE_ROW (node)->row.background);
    }
  else
    NAUTILUS_CTREE_ROW (node)->row.bg_set = FALSE;

  tree_draw_node (ctree, node);
}

void
nautilus_ctree_node_set_row_data (NautilusCTree     *ctree,
			     NautilusCTreeNode *node,
			     gpointer      data)
{
  nautilus_ctree_node_set_row_data_full (ctree, node, data, NULL);
}

void
nautilus_ctree_node_set_row_data_full (NautilusCTree         *ctree,
				  NautilusCTreeNode     *node,
				  gpointer          data,
				  GtkDestroyNotify  destroy)
{
  GtkDestroyNotify dnotify;
  gpointer ddata;
  
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (node != NULL);

  dnotify = NAUTILUS_CTREE_ROW (node)->row.destroy;
  ddata = NAUTILUS_CTREE_ROW (node)->row.data;
  
  NAUTILUS_CTREE_ROW (node)->row.data = data;
  NAUTILUS_CTREE_ROW (node)->row.destroy = destroy;

  if (dnotify)
    dnotify (ddata);
}

gpointer
nautilus_ctree_node_get_row_data (NautilusCTree     *ctree,
			     NautilusCTreeNode *node)
{
  g_return_val_if_fail (ctree != NULL, NULL);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), NULL);

  return node ? NAUTILUS_CTREE_ROW (node)->row.data : NULL;
}

void
nautilus_ctree_node_moveto (NautilusCTree     *ctree,
		       NautilusCTreeNode *node,
		       gint          column,
		       gfloat        row_align,
		       gfloat        col_align)
{
  gint row = -1;
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  while (node && !nautilus_ctree_is_viewable (ctree, node))
    node = NAUTILUS_CTREE_ROW (node)->parent;

  if (node)
    row = g_list_position (clist->row_list, (GList *)node);
  
  gtk_clist_moveto (clist, row, column, row_align, col_align);
}

GtkVisibility nautilus_ctree_node_is_visible (NautilusCTree     *ctree,
                                         NautilusCTreeNode *node)
{
  gint row;
  
  g_return_val_if_fail (ctree != NULL, 0);
  g_return_val_if_fail (node != NULL, 0);
  
  row = g_list_position (GTK_CLIST (ctree)->row_list, (GList*) node);
  return gtk_clist_row_is_visible (GTK_CLIST (ctree), row);
}


/***********************************************************
 *             NautilusCTree specific functions                 *
 ***********************************************************/

void
nautilus_ctree_set_indent (NautilusCTree *ctree, 
                      gint      indent)
{
  GtkCList *clist;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (indent >= 0);

  if (indent == ctree->tree_indent)
    return;

  clist = GTK_CLIST (ctree);
  ctree->tree_indent = indent;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    gtk_clist_set_column_width
      (clist, ctree->tree_column,
       gtk_clist_optimal_column_width (clist, ctree->tree_column));
  else
    CLIST_REFRESH (ctree);
}

void
nautilus_ctree_set_spacing (NautilusCTree *ctree, 
		       gint      spacing)
{
  GtkCList *clist;
  gint old_spacing;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));
  g_return_if_fail (spacing >= 0);

  if (spacing == ctree->tree_spacing)
    return;

  clist = GTK_CLIST (ctree);

  old_spacing = ctree->tree_spacing;
  ctree->tree_spacing = spacing;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    gtk_clist_set_column_width (clist, ctree->tree_column,
				clist->column[ctree->tree_column].width +
				spacing - old_spacing);
  else
    CLIST_REFRESH (ctree);
}

void
nautilus_ctree_set_show_stub (NautilusCTree *ctree, 
			 gboolean  show_stub)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  show_stub = show_stub != FALSE;

  if (show_stub != ctree->show_stub)
    {
      GtkCList *clist;

      clist = GTK_CLIST (ctree);
      ctree->show_stub = show_stub;

      if (CLIST_UNFROZEN (clist) && clist->rows &&
	  gtk_clist_row_is_visible (clist, 0) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row
	  (clist, NULL, 0, GTK_CLIST_ROW (clist->row_list));
    }
}

void 
nautilus_ctree_set_line_style (NautilusCTree          *ctree, 
			  NautilusCTreeLineStyle  line_style)
{
  GtkCList *clist;
  NautilusCTreeLineStyle old_style;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  if (line_style == ctree->line_style)
    return;

  clist = GTK_CLIST (ctree);

  old_style = ctree->line_style;
  ctree->line_style = line_style;

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    {
      if (old_style == NAUTILUS_CTREE_LINES_TABBED)
	gtk_clist_set_column_width
	  (clist, ctree->tree_column,
	   clist->column[ctree->tree_column].width - 3);
      else if (line_style == NAUTILUS_CTREE_LINES_TABBED)
	gtk_clist_set_column_width
	  (clist, ctree->tree_column,
	   clist->column[ctree->tree_column].width + 3);
    }

  if (GTK_WIDGET_REALIZED (ctree))
    {
      switch (line_style)
	{
	case NAUTILUS_CTREE_LINES_SOLID:
	  if (GTK_WIDGET_REALIZED (ctree))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, GDK_LINE_SOLID, 
					None, None);
	  break;
	case NAUTILUS_CTREE_LINES_DOTTED:
	  if (GTK_WIDGET_REALIZED (ctree))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, 
					GDK_LINE_ON_OFF_DASH, None, None);
	  gdk_gc_set_dashes (ctree->lines_gc, 0, "\1\1", 2);
	  break;
	case NAUTILUS_CTREE_LINES_TABBED:
	  if (GTK_WIDGET_REALIZED (ctree))
	    gdk_gc_set_line_attributes (ctree->lines_gc, 1, GDK_LINE_SOLID, 
					None, None);
	  break;
	case NAUTILUS_CTREE_LINES_NONE:
	  break;
	default:
	  return;
	}
      CLIST_REFRESH (ctree);
    }
}

/***********************************************************
 *             Tree sorting functions                      *
 ***********************************************************/


static void
tree_sort (NautilusCTree     *ctree,
	   NautilusCTreeNode *node,
	   gpointer      data)
{
  NautilusCTreeNode *list_start;
  NautilusCTreeNode *cmp;
  NautilusCTreeNode *work;
  GtkCList *clist;

  clist = GTK_CLIST (ctree);

  if (node)
    list_start = NAUTILUS_CTREE_ROW (node)->children;
  else
    list_start = NAUTILUS_CTREE_NODE (clist->row_list);

  while (list_start)
    {
      cmp = list_start;
      work = NAUTILUS_CTREE_ROW (cmp)->sibling;
      while (work)
	{
	  if (clist->sort_type == GTK_SORT_ASCENDING)
	    {
	      if (clist->compare 
		  (clist, NAUTILUS_CTREE_ROW (work), NAUTILUS_CTREE_ROW (cmp)) < 0)
		cmp = work;
	    }
	  else
	    {
	      if (clist->compare 
		  (clist, NAUTILUS_CTREE_ROW (work), NAUTILUS_CTREE_ROW (cmp)) > 0)
		cmp = work;
	    }
	  work = NAUTILUS_CTREE_ROW (work)->sibling;
	}
      if (cmp == list_start)
	list_start = NAUTILUS_CTREE_ROW (cmp)->sibling;
      else
	{
	  nautilus_ctree_unlink (ctree, cmp, FALSE);
	  nautilus_ctree_link (ctree, cmp, node, list_start, FALSE);
	}
    }
}

void
nautilus_ctree_sort_recursive (NautilusCTree     *ctree, 
			  NautilusCTreeNode *node)
{
  GtkCList *clist;
  NautilusCTreeNode *focus_node = NULL;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (!node || (node && nautilus_ctree_is_viewable (ctree, node)))
    focus_node =
      NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));
      
  nautilus_ctree_post_recursive (ctree, node, NAUTILUS_CTREE_FUNC (tree_sort), NULL);

  if (!node)
    tree_sort (ctree, NULL, NULL);

  if (focus_node)
    {
      clist->focus_row = g_list_position (clist->row_list,(GList *)focus_node);
      clist->undo_anchor = clist->focus_row;
    }

  gtk_clist_thaw (clist);
}

static void
real_sort_list (GtkCList *clist)
{
  nautilus_ctree_sort_recursive (NAUTILUS_CTREE (clist), NULL);
}

void
nautilus_ctree_sort_node (NautilusCTree     *ctree, 
		     NautilusCTreeNode *node)
{
  GtkCList *clist;
  NautilusCTreeNode *focus_node = NULL;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  if (clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (!node || (node && nautilus_ctree_is_viewable (ctree, node)))
    focus_node = NAUTILUS_CTREE_NODE
      (g_list_nth (clist->row_list, clist->focus_row));

  tree_sort (ctree, node, NULL);

  if (focus_node)
    {
      clist->focus_row = g_list_position (clist->row_list,(GList *)focus_node);
      clist->undo_anchor = clist->focus_row;
    }

  gtk_clist_thaw (clist);
}

/************************************************************************/

static void
fake_unselect_all (GtkCList *clist,
		   gint      row)
{
  GList *list;
  GList *focus_node = NULL;

  if (row >= 0 && (focus_node = g_list_nth (clist->row_list, row)))
    {
      if (NAUTILUS_CTREE_ROW (focus_node)->row.state == GTK_STATE_NORMAL &&
	  NAUTILUS_CTREE_ROW (focus_node)->row.selectable)
	{
	  NAUTILUS_CTREE_ROW (focus_node)->row.state = GTK_STATE_SELECTED;
	  
	  if (CLIST_UNFROZEN (clist) &&
	      gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row,
						  GTK_CLIST_ROW (focus_node));
	}  
    }

  clist->undo_selection = clist->selection;
  clist->selection = NULL;
  clist->selection_end = NULL;
  
  for (list = clist->undo_selection; list; list = list->next)
    {
      if (list->data == focus_node)
	continue;

      NAUTILUS_CTREE_ROW ((GList *)(list->data))->row.state = GTK_STATE_NORMAL;
      tree_draw_node (NAUTILUS_CTREE (clist), NAUTILUS_CTREE_NODE (list->data));
    }
}

static GList *
selection_find (GtkCList *clist,
		gint      row_number,
		GList    *row_list_element)
{
  return g_list_find (clist->selection, row_list_element);
}

static void
resync_selection (GtkCList *clist, GdkEvent *event)
{
  NautilusCTree *ctree;
  GList *list;
  NautilusCTreeNode *node;
  gint i;
  gint e;
  gint row;
  gboolean unselect;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  if (clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (clist->anchor < 0 || clist->drag_pos < 0)
    return;

  ctree = NAUTILUS_CTREE (clist);
  
  clist->freeze_count++;

  i = MIN (clist->anchor, clist->drag_pos);
  e = MAX (clist->anchor, clist->drag_pos);

  if (clist->undo_selection)
    {
      list = clist->selection;
      clist->selection = clist->undo_selection;
      clist->selection_end = g_list_last (clist->selection);
      clist->undo_selection = list;
      list = clist->selection;

      while (list)
	{
	  node = list->data;
	  list = list->next;
	  
	  unselect = TRUE;

	  if (nautilus_ctree_is_viewable (ctree, node))
	    {
	      row = g_list_position (clist->row_list, (GList *)node);
	      if (row >= i && row <= e)
		unselect = FALSE;
	    }
	  if (unselect && NAUTILUS_CTREE_ROW (node)->row.selectable)
	    {
	      NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
	      nautilus_ctree_unselect (ctree, node);
	      clist->undo_selection = g_list_prepend (clist->undo_selection,
						      node);
	    }
	}
    }    

  if (clist->anchor < clist->drag_pos)
    {
      for (node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, i)); i <= e;
	   i++, node = NAUTILUS_CTREE_NODE_NEXT (node))
	if (NAUTILUS_CTREE_ROW (node)->row.selectable)
	  {
	    if (g_list_find (clist->selection, node))
	      {
		if (NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_NORMAL)
		  {
		    NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
		    nautilus_ctree_unselect (ctree, node);
		    clist->undo_selection =
		      g_list_prepend (clist->undo_selection, node);
		  }
	      }
	    else if (NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
	      {
		NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;
		clist->undo_unselection =
		  g_list_prepend (clist->undo_unselection, node);
	      }
	  }
    }
  else
    {
      for (node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list, e)); i <= e;
	   e--, node = NAUTILUS_CTREE_NODE_PREV (node))
	if (NAUTILUS_CTREE_ROW (node)->row.selectable)
	  {
	    if (g_list_find (clist->selection, node))
	      {
		if (NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_NORMAL)
		  {
		    NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_SELECTED;
		    nautilus_ctree_unselect (ctree, node);
		    clist->undo_selection =
		      g_list_prepend (clist->undo_selection, node);
		  }
	      }
	    else if (NAUTILUS_CTREE_ROW (node)->row.state == GTK_STATE_SELECTED)
	      {
		NAUTILUS_CTREE_ROW (node)->row.state = GTK_STATE_NORMAL;
		clist->undo_unselection =
		  g_list_prepend (clist->undo_unselection, node);
	      }
	  }
    }

  clist->undo_unselection = g_list_reverse (clist->undo_unselection);
  for (list = clist->undo_unselection; list; list = list->next)
    nautilus_ctree_select (ctree, list->data);

  clist->anchor = -1;
  clist->drag_pos = -1;

  if (!CLIST_UNFROZEN (clist))
    clist->freeze_count--;
}

static void
real_undo_selection (GtkCList *clist)
{
  NautilusCTree *ctree;
  GList *work;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (clist));

  if (clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (!(clist->undo_selection || clist->undo_unselection))
    {
      gtk_clist_unselect_all (clist);
      return;
    }

  ctree = NAUTILUS_CTREE (clist);

  for (work = clist->undo_selection; work; work = work->next)
    if (NAUTILUS_CTREE_ROW (work->data)->row.selectable)
      nautilus_ctree_select (ctree, NAUTILUS_CTREE_NODE (work->data));

  for (work = clist->undo_unselection; work; work = work->next)
    if (NAUTILUS_CTREE_ROW (work->data)->row.selectable)
      nautilus_ctree_unselect (ctree, NAUTILUS_CTREE_NODE (work->data));

  if (GTK_WIDGET_HAS_FOCUS (clist) && clist->focus_row != clist->undo_anchor)
    {
      gtk_widget_draw_focus (GTK_WIDGET (clist));
      clist->focus_row = clist->undo_anchor;
      gtk_widget_draw_focus (GTK_WIDGET (clist));
    }
  else
    clist->focus_row = clist->undo_anchor;
  
  clist->undo_anchor = -1;
 
  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  if (ROW_TOP_YPIXEL (clist, clist->focus_row) + clist->row_height >
      clist->clist_window_height)
    gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
  else if (ROW_TOP_YPIXEL (clist, clist->focus_row) < 0)
    gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);

}

void
nautilus_ctree_set_drag_compare_func (NautilusCTree                *ctree,
				 NautilusCTreeCompareDragFunc  cmp_func)
{
  g_return_if_fail (ctree != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (ctree));

  ctree->drag_compare = cmp_func;
}

static gboolean
check_drag (NautilusCTree        *ctree,
	    NautilusCTreeNode    *drag_source,
	    NautilusCTreeNode    *drag_target,
	    GtkCListDragPos  insert_pos)
{
  g_return_val_if_fail (ctree != NULL, FALSE);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (ctree), FALSE);

  if (drag_source && drag_source != drag_target &&
      (!NAUTILUS_CTREE_ROW (drag_source)->children ||
       !nautilus_ctree_is_ancestor (ctree, drag_source, drag_target)))
    {
      switch (insert_pos)
	{
	case GTK_CLIST_DRAG_NONE:
	  return FALSE;
	case GTK_CLIST_DRAG_AFTER:
	  if (NAUTILUS_CTREE_ROW (drag_target)->sibling != drag_source)
	    return (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 drag_source,
					 NAUTILUS_CTREE_ROW (drag_target)->parent,
					 NAUTILUS_CTREE_ROW(drag_target)->sibling));
	  break;
	case GTK_CLIST_DRAG_BEFORE:
	  if (NAUTILUS_CTREE_ROW (drag_source)->sibling != drag_target)
	    return (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 drag_source,
					 NAUTILUS_CTREE_ROW (drag_target)->parent,
					 drag_target));
	  break;
	case GTK_CLIST_DRAG_INTO:
	  if (!NAUTILUS_CTREE_ROW (drag_target)->is_leaf &&
	      NAUTILUS_CTREE_ROW (drag_target)->children != drag_source)
	    return (!ctree->drag_compare ||
		    ctree->drag_compare (ctree,
					 drag_source,
					 drag_target,
					 NAUTILUS_CTREE_ROW (drag_target)->children));
	  break;
	}
    }
  return FALSE;
}



/************************************/
static void
drag_dest_info_destroy (gpointer data)
{
  GtkCListDestInfo *info = data;

  g_free (info);
}

static void
drag_dest_cell (GtkCList         *clist,
		gint              x,
		gint              y,
		GtkCListDestInfo *dest_info)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (clist);

  dest_info->insert_pos = GTK_CLIST_DRAG_NONE;

  y -= (GTK_CONTAINER (widget)->border_width +
	widget->style->klass->ythickness + clist->column_title_area.height);
  dest_info->cell.row = ROW_FROM_YPIXEL (clist, y);

  if (dest_info->cell.row >= clist->rows)
    {
      dest_info->cell.row = clist->rows - 1;
      y = ROW_TOP_YPIXEL (clist, dest_info->cell.row) + clist->row_height;
    }
  if (dest_info->cell.row < -1)
    dest_info->cell.row = -1;

  x -= GTK_CONTAINER (widget)->border_width + widget->style->klass->xthickness;
  dest_info->cell.column = COLUMN_FROM_XPIXEL (clist, x);

  if (dest_info->cell.row >= 0)
    {
      gint y_delta;
      gint h = 0;

      y_delta = y - ROW_TOP_YPIXEL (clist, dest_info->cell.row);
      
      if (GTK_CLIST_DRAW_DRAG_RECT(clist) &&
	  !NAUTILUS_CTREE_ROW (g_list_nth (clist->row_list,
				      dest_info->cell.row))->is_leaf)
	{
	  dest_info->insert_pos = GTK_CLIST_DRAG_INTO;
	  h = clist->row_height / 4;
	}
      else if (GTK_CLIST_DRAW_DRAG_LINE(clist))
	{
	  dest_info->insert_pos = GTK_CLIST_DRAG_BEFORE;
	  h = clist->row_height / 2;
	}

      if (GTK_CLIST_DRAW_DRAG_LINE(clist))
	{
	  if (y_delta < h)
	    dest_info->insert_pos = GTK_CLIST_DRAG_BEFORE;
	  else if (clist->row_height - y_delta < h)
	    dest_info->insert_pos = GTK_CLIST_DRAG_AFTER;
	}
    }
}

static void
nautilus_ctree_drag_begin (GtkWidget	     *widget,
		      GdkDragContext *context)
{
  GtkCList *clist;
  NautilusCTree *ctree;
  gboolean use_icons;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (widget));
  g_return_if_fail (context != NULL);

  clist = GTK_CLIST (widget);
  ctree = NAUTILUS_CTREE (widget);

  use_icons = GTK_CLIST_USE_DRAG_ICONS (clist);
  GTK_CLIST_UNSET_FLAG (clist, CLIST_USE_DRAG_ICONS);
  GTK_WIDGET_CLASS (parent_class)->drag_begin (widget, context);

  if (use_icons)
    {
      NautilusCTreeNode *node;

      GTK_CLIST_SET_FLAG (clist, CLIST_USE_DRAG_ICONS);
      node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list,
					 clist->click_cell.row));
      if (node)
	{
	  if (GTK_CELL_PIXTEXT
	      (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
	    {
	      gtk_drag_set_icon_pixmap
		(context,
		 gtk_widget_get_colormap (widget),
		 GTK_CELL_PIXTEXT
		 (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap,
		 GTK_CELL_PIXTEXT
		 (NAUTILUS_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask,
		 -2, -2);
	      return;
	    }
	}
      gtk_drag_set_icon_default (context);
    }
}

static gint
nautilus_ctree_drag_motion (GtkWidget      *widget,
		       GdkDragContext *context,
		       gint            x,
		       gint            y,
		       guint           time)
{
  GtkCList *clist;
  NautilusCTree *ctree;
  GtkCListDestInfo new_info;
  GtkCListDestInfo *dest_info;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (NAUTILUS_IS_CTREE (widget), FALSE);

  clist = GTK_CLIST (widget);
  ctree = NAUTILUS_CTREE (widget);

  dest_info = g_dataset_get_data (context, "gtk-clist-drag-dest");

  if (!dest_info)
    {
      dest_info = g_new (GtkCListDestInfo, 1);
	  
      dest_info->cell.row    = -1;
      dest_info->cell.column = -1;
      dest_info->insert_pos  = GTK_CLIST_DRAG_NONE;

      g_dataset_set_data_full (context, "gtk-clist-drag-dest", dest_info,
			       drag_dest_info_destroy);
    }

  drag_dest_cell (clist, x, y, &new_info);

  if (GTK_CLIST_REORDERABLE (clist))
    {
      GList *list;
      GdkAtom atom = gdk_atom_intern ("gtk-clist-drag-reorder", FALSE);

      list = context->targets;
      while (list)
	{
	  if (atom == GPOINTER_TO_UINT (list->data))
	    break;
	  list = list->next;
	}

      if (list)
	{
	  NautilusCTreeNode *drag_source;
	  NautilusCTreeNode *drag_target;

	  drag_source = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list,
						    clist->click_cell.row));
	  drag_target = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list,
						    new_info.cell.row));

	  if (gtk_drag_get_source_widget (context) != widget ||
	      !check_drag (ctree, drag_source, drag_target,
			   new_info.insert_pos))
	    {
	      if (dest_info->cell.row < 0)
		{
		  gdk_drag_status (context, GDK_ACTION_DEFAULT, time);
		  return FALSE;
		}
	      return TRUE;
	    }

	  if (new_info.cell.row != dest_info->cell.row ||
	      (new_info.cell.row == dest_info->cell.row &&
	       dest_info->insert_pos != new_info.insert_pos))
	    {
	      if (dest_info->cell.row >= 0)
		GTK_CLIST_CLASS_FW (clist)->draw_drag_highlight
		  (clist,
		   g_list_nth (clist->row_list, dest_info->cell.row)->data,
		   dest_info->cell.row, dest_info->insert_pos);

	      dest_info->insert_pos  = new_info.insert_pos;
	      dest_info->cell.row    = new_info.cell.row;
	      dest_info->cell.column = new_info.cell.column;

	      GTK_CLIST_CLASS_FW (clist)->draw_drag_highlight
		(clist,
		 g_list_nth (clist->row_list, dest_info->cell.row)->data,
		 dest_info->cell.row, dest_info->insert_pos);

	      gdk_drag_status (context, context->suggested_action, time);
	    }
	  return TRUE;
	}
    }

  dest_info->insert_pos  = new_info.insert_pos;
  dest_info->cell.row    = new_info.cell.row;
  dest_info->cell.column = new_info.cell.column;
  return TRUE;
}

static void
nautilus_ctree_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      gint              x,
			      gint              y,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint32           time)
{
  NautilusCTree *ctree;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (NAUTILUS_IS_CTREE (widget));
  g_return_if_fail (context != NULL);
  g_return_if_fail (selection_data != NULL);

  ctree = NAUTILUS_CTREE (widget);
  clist = GTK_CLIST (widget);

  if (GTK_CLIST_REORDERABLE (clist) &&
      gtk_drag_get_source_widget (context) == widget &&
      selection_data->target ==
      gdk_atom_intern ("gtk-clist-drag-reorder", FALSE) &&
      selection_data->format == GTK_TYPE_POINTER &&
      selection_data->length == sizeof (GtkCListCellInfo))
    {
      GtkCListCellInfo *source_info;

      source_info = (GtkCListCellInfo *)(selection_data->data);
      if (source_info)
	{
	  GtkCListDestInfo dest_info;
	  NautilusCTreeNode *source_node;
	  NautilusCTreeNode *dest_node;

	  drag_dest_cell (clist, x, y, &dest_info);
	  
	  source_node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list,
						    source_info->row));
	  dest_node = NAUTILUS_CTREE_NODE (g_list_nth (clist->row_list,
						  dest_info.cell.row));

	  if (!source_node || !dest_node)
	    return;

	  switch (dest_info.insert_pos)
	    {
	    case GTK_CLIST_DRAG_NONE:
	      break;
	    case GTK_CLIST_DRAG_INTO:
	      if (check_drag (ctree, source_node, dest_node,
			      dest_info.insert_pos))
		nautilus_ctree_move (ctree, source_node, dest_node,
				NAUTILUS_CTREE_ROW (dest_node)->children);
	      g_dataset_remove_data (context, "gtk-clist-drag-dest");
	      break;
	    case GTK_CLIST_DRAG_BEFORE:
	      if (check_drag (ctree, source_node, dest_node,
			      dest_info.insert_pos))
		nautilus_ctree_move (ctree, source_node,
				NAUTILUS_CTREE_ROW (dest_node)->parent, dest_node);
	      g_dataset_remove_data (context, "gtk-clist-drag-dest");
	      break;
	    case GTK_CLIST_DRAG_AFTER:
	      if (check_drag (ctree, source_node, dest_node,
			      dest_info.insert_pos))
		nautilus_ctree_move (ctree, source_node,
				NAUTILUS_CTREE_ROW (dest_node)->parent, 
				NAUTILUS_CTREE_ROW (dest_node)->sibling);
	      g_dataset_remove_data (context, "gtk-clist-drag-dest");
	      break;
	    }
	}
    }
}
