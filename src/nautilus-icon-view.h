/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-view.h - interface for icon view of directory.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#ifndef NAUTILUS_ICON_VIEW_H
#define NAUTILUS_ICON_VIEW_H

#include "nautilus-view.h"

typedef struct NautilusIconView NautilusIconView;
typedef struct NautilusIconViewClass NautilusIconViewClass;

#define NAUTILUS_TYPE_ICON_VIEW nautilus_icon_view_get_type()
#define NAUTILUS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_VIEW, NautilusIconView))
#define NAUTILUS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_VIEW, NautilusIconViewClass))
#define NAUTILUS_IS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_VIEW))
#define NAUTILUS_IS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_VIEW))
#define NAUTILUS_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_VIEW, NautilusIconViewClass))

#define NAUTILUS_ICON_VIEW_ID "OAFIID:Nautilus_File_Manager_Icon_View"
#define FM_COMPACT_VIEW_ID "OAFIID:Nautilus_File_Manager_Compact_View"

typedef struct NautilusIconViewDetails NautilusIconViewDetails;

struct NautilusIconView {
	NautilusView parent;
	NautilusIconViewDetails *details;
};

struct NautilusIconViewClass {
	NautilusViewClass parent_class;

	/* Methods that can be overriden for settings you don't want to come from metadata.
	 */
	 
	/* Note: get_directory_sort_by must return a string that can/will be g_freed.
	 */
	char *	 (* get_directory_sort_by)       (NautilusIconView *icon_view, 
						  NautilusFile *file);
	void     (* set_directory_sort_by)       (NautilusIconView *icon_view, 
						  NautilusFile *file, 
						  const char* sort_by);

	gboolean (* get_directory_sort_reversed) (NautilusIconView *icon_view, 
						  NautilusFile *file);
	void     (* set_directory_sort_reversed) (NautilusIconView *icon_view, 
						  NautilusFile *file, 
						  gboolean sort_reversed);

	gboolean (* get_directory_auto_layout)   (NautilusIconView *icon_view, 
						  NautilusFile *file);
	void     (* set_directory_auto_layout)   (NautilusIconView *icon_view, 
						  NautilusFile *file, 
						  gboolean auto_layout);
	
	gboolean (* get_directory_tighter_layout) (NautilusIconView *icon_view, 
						   NautilusFile *file);
	void     (* set_directory_tighter_layout)   (NautilusIconView *icon_view, 
						     NautilusFile *file, 
						     gboolean tighter_layout);

	/* Override "clean_up" if your subclass has its own notion of where icons should be positioned */
	void	 (* clean_up)			 (NautilusIconView *icon_view);

	/* supports_auto_layout is a function pointer that subclasses may
	 * override to control whether or not the automatic layout options
	 * should be enabled. The default implementation returns TRUE.
	 */
	gboolean (* supports_auto_layout)	 (NautilusIconView *view);

	/* supports_manual_layout is a function pointer that subclasses may
	 * override to control whether or not the manual layout options
	 * should be enabled. The default implementation returns TRUE iff
	 * not in compact mode.
	 */
	gboolean (* supports_manual_layout)	 (NautilusIconView *view);

	/* supports_scaling is a function pointer that subclasses may
	 * override to control whether or not the manual layout supports
	 * scaling. The default implementation returns FALSE
	 */
	gboolean (* supports_scaling)	 (NautilusIconView *view);

	/* supports_auto_layout is a function pointer that subclasses may
	 * override to control whether snap-to-grid mode
	 * should be enabled. The default implementation returns FALSE.
	 */
	gboolean (* supports_keep_aligned)	 (NautilusIconView *view);

	/* supports_auto_layout is a function pointer that subclasses may
	 * override to control whether snap-to-grid mode
	 * should be enabled. The default implementation returns FALSE.
	 */
	gboolean (* supports_labels_beside_icons)	 (NautilusIconView *view);
};

/* GObject support */
GType   nautilus_icon_view_get_type      (void);
int     nautilus_icon_view_compare_files (NautilusIconView   *icon_view,
					  NautilusFile *a,
					  NautilusFile *b);
void    nautilus_icon_view_filter_by_screen (NautilusIconView *icon_view,
					     gboolean filter);
gboolean nautilus_icon_view_is_compact   (NautilusIconView *icon_view);

void    nautilus_icon_view_register         (void);
void    nautilus_icon_view_compact_register (void);

NautilusIconContainer * nautilus_icon_view_get_icon_container (NautilusIconView *view);

#endif /* NAUTILUS_ICON_VIEW_H */
