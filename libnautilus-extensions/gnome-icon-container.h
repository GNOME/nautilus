/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container.h - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation

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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef _GNOME_ICON_CONTAINER_H
#define _GNOME_ICON_CONTAINER_H

#include <libgnomeui/libgnomeui.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

enum _GnomeIconContainerIconMode {
	GNOME_ICON_CONTAINER_NORMAL_ICONS,
	GNOME_ICON_CONTAINER_SMALL_ICONS
};
typedef enum _GnomeIconContainerIconMode GnomeIconContainerIconMode;

enum _GnomeIconContainerLayoutMode {
	GNOME_ICON_LAYOUT_MANUAL,
	GNOME_ICON_LAYOUT_AUTO
};
typedef enum _GnomeIconContainerLayoutMode GnomeIconContainerLayoutMode;

typedef struct _GnomeIconContainer GnomeIconContainer;
typedef struct _GnomeIconContainerClass GnomeIconContainerClass;
typedef struct _GnomeIconContainerPrivate GnomeIconContainerPrivate;

#include "gnome-icon-container-layout.h"


#define GNOME_ICON_CONTAINER(obj) \
	GTK_CHECK_CAST (obj, gnome_icon_container_get_type (), GnomeIconContainer)
#define GNOME_ICON_CONTAINER_CLASS(k) \
	GTK_CHECK_CLASS_CAST (k, gnome_icon_container_get_type (), GnomeIconListView)
#define GNOME_IS_ICON_CONTAINER(obj) \
	GTK_CHECK_TYPE (obj, gnome_icon_container_get_type ())


typedef gint (* GnomeIconContainerSortFunc) (const gchar *name_a,
					     gpointer data_a,
					     const gchar *name_b,
					     gpointer data_b,
					     gpointer user_data);

struct _GnomeIconContainer {
	GnomeCanvas canvas;
	GnomeIconContainerPrivate *priv;
};

struct _GnomeIconContainerClass {
	GnomeCanvasClass parent_class;

	void (* selection_changed) 	(GnomeIconContainer *container);
	gint (* button_press) 		(GnomeIconContainer *container,
					 GdkEventButton *event);
	void (* activate)		(GnomeIconContainer *container,
					 const gchar *name,
					 gpointer data);

	void (* context_click)		(GnomeIconContainer *container,
					 const gchar *name,
					 gpointer data);
};


guint		 gnome_icon_container_get_type	(void);

GtkWidget	*gnome_icon_container_new	(void);

void		 gnome_icon_container_clear	(GnomeIconContainer *view);

void		 gnome_icon_container_set_icon_mode
						(GnomeIconContainer *view,
						 GnomeIconContainerIconMode mode);

GnomeIconContainerIconMode
		 gnome_icon_container_get_icon_mode
						(GnomeIconContainer *view);

void		 gnome_icon_container_set_editable
						(GnomeIconContainer *view,
						  gboolean is_editable);
gboolean	 gnome_icon_container_get_editable
						(GnomeIconContainer *view);
 
void		 gnome_icon_container_add_pixbuf (GnomeIconContainer *view,
						  GdkPixbuf *image,
						  const gchar *text,
						  gint x, gint y,
						  gpointer data);

void		 gnome_icon_container_add_pixbuf_auto
						 (GnomeIconContainer *view,
						  GdkPixbuf *image,
						  const gchar *text,
						  gpointer data);
gboolean	 gnome_icon_container_add_pixbuf_with_layout
						 (GnomeIconContainer
						  *container,
						  GdkPixbuf *image,
						  const gchar *text,
						  gpointer data,
						  const GnomeIconContainerLayout
						  *layout);
 
gpointer	 gnome_icon_container_get_icon_data
						 (GnomeIconContainer *view,
						  const gchar *text);

void		 gnome_icon_container_relayout	 (GnomeIconContainer *view);
void		 gnome_icon_container_line_up	 (GnomeIconContainer *view);
GList 		*gnome_icon_container_get_selection
						 (GnomeIconContainer *view);

void		 gnome_icon_container_unselect_all
						 (GnomeIconContainer *view);
void		 gnome_icon_container_select_all (GnomeIconContainer *view);

void		 gnome_icon_container_enable_browser_mode
						 (GnomeIconContainer *view,
						  gboolean enable);

void		 gnome_icon_container_set_base_uri
						 (GnomeIconContainer *container,
						  const gchar *base_uri);

void		 gnome_icon_container_xlate_selected
						(GnomeIconContainer *container,
						 gint amount_x,
						 gint amount_y,
						 gboolean raise);

GnomeIconContainerLayout *
		 gnome_icon_container_get_layout
						(GnomeIconContainer *container);
#endif
