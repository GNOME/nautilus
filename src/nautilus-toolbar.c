/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-toolbar.c - toolbar for Nautilus to overcome fixed spacing problem

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include <gtk/gtktoolbar.h>

#include <gnome.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

#include "nautilus-toolbar.h"

typedef struct _GtkToolbarChildSpace GtkToolbarChildSpace;
struct _GtkToolbarChildSpace
{
  GtkToolbarChild child;

  gint alloc_x, alloc_y;
};

static void	nautilus_toolbar_initialize_class	(NautilusToolbarClass *class);
static void	nautilus_toolbar_initialize		(NautilusToolbar      *bar);
static void	nautilus_toolbar_size_allocate		(GtkWidget     *widget,
			   				 GtkAllocation *allocation);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusToolbar, nautilus_toolbar, GTK_TYPE_TOOLBAR)

GtkWidget *
nautilus_toolbar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_TOOLBAR, NULL);
}


static void
nautilus_toolbar_initialize_class (NautilusToolbarClass *klass)
{
	
	GtkWidgetClass *widget_class;
 
  	widget_class = (GtkWidgetClass *) klass;	
	widget_class->size_allocate = nautilus_toolbar_size_allocate;
}

static void
nautilus_toolbar_initialize (NautilusToolbar *bar)
{
	bar->button_spacing = 48; /* default to reasonable amount */
}

void
nautilus_toolbar_set_button_spacing  (NautilusToolbar *toolbar, int spacing)
{
	toolbar->button_spacing = spacing;
}


static void
nautilus_toolbar_size_allocate (GtkWidget     *widget,
			   	GtkAllocation *allocation)
{
  GtkToolbar *toolbar;
  GtkToolbarChildSpace *child_space;
  NautilusToolbar *nautilus_toolbar;
  GList *children;
  GtkToolbarChild *child;
  GtkAllocation alloc;
  GtkRequisition child_requisition;
  gint border_width;
  gint spacing;
  gint item_width, item_height;
  gint width_to_use, height_to_use;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (allocation != NULL);

  toolbar = GTK_TOOLBAR (widget);
  nautilus_toolbar = NAUTILUS_TOOLBAR (widget);
  spacing = nautilus_toolbar->button_spacing;
  
  widget->allocation = *allocation;

  border_width = GTK_CONTAINER (toolbar)->border_width;

  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    alloc.x = allocation->x + border_width;
  else
    alloc.y = allocation->y + border_width;

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:
	  child_space = (GtkToolbarChildSpace *) child;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      child_space->alloc_x = alloc.x;
	      child_space->alloc_y = allocation->y + (allocation->height - toolbar->button_maxh) / 2;
	      alloc.x += toolbar->space_size;
	    }
	  else
	    {
	      child_space->alloc_x = allocation->x + (allocation->width - toolbar->button_maxw) / 2;
	      child_space->alloc_y = alloc.y;
	      alloc.y += toolbar->space_size;
	    }

	  break;
		
	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;
	 
	  item_width = child->widget->requisition.width;
	  item_height = child->widget->requisition.height;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL) {
	  	width_to_use = (item_width > spacing) ? item_width : spacing;
	  	height_to_use = toolbar->button_maxh;
	  } else {
	  	width_to_use = toolbar->button_maxw;
	  	height_to_use = (item_height > spacing) ? item_height : spacing;
	  }
	  
	  alloc.width = width_to_use;
	  alloc.height = height_to_use;


	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.y = allocation->y + (allocation->height - height_to_use) / 2;
	  else
	    alloc.x = allocation->x + (allocation->width - width_to_use) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);
	  
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += width_to_use;
	  else
	    alloc.y += height_to_use;

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  gtk_widget_get_child_requisition (child->widget, &child_requisition);
	  
	  alloc.width = child_requisition.width;
	  alloc.height = child_requisition.height;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.y = allocation->y + (allocation->height - child_requisition.height) / 2;
	  else
	    alloc.x = allocation->x + (allocation->width - child_requisition.width) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += child_requisition.width;
	  else
	    alloc.y += child_requisition.height;

	  break;

	default:
	  g_assert_not_reached ();
	}
    }
}
