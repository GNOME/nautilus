/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-smooth-text-layout-cache.h - A GtkObject subclass for efficiently rendering smooth text.

   Copyright (C) 2001 Eazel, Inc.

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

   Authors: John Harper <jsh@eazel.com>
*/

#ifndef NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE_H
#define NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE_H

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-smooth-text-layout.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT_CACHE		  (nautilus_smooth_text_layout_cache_get_type ())
#define NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE(obj)		  (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT_CACHE, NautilusSmoothTextLayoutCache))
#define NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE_CLASS(klass)	  (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT_CACHE, NautilusSmoothTextLayoutCacheClass))
#define NAUTILUS_IS_SMOOTH_TEXT_LAYOUT_CACHE(obj)	  (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT_CACHE))
#define NAUTILUS_IS_SMOOTH_TEXT_LAYOUT_CACHE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT_CACHE))

typedef struct NautilusSmoothTextLayoutCache        NautilusSmoothTextLayoutCache;
typedef struct NautilusSmoothTextLayoutCacheClass   NautilusSmoothTextLayoutCacheClass;
typedef struct NautilusSmoothTextLayoutCacheDetails NautilusSmoothTextLayoutCacheDetails;

struct NautilusSmoothTextLayoutCache
{
	/* Superclass */
	GtkObject object;

	/* Private things */
	NautilusSmoothTextLayoutCacheDetails *details;
};

struct NautilusSmoothTextLayoutCacheClass
{
	GtkObjectClass parent_class;
};

GtkType                           nautilus_smooth_text_layout_cache_get_type (void);
NautilusSmoothTextLayoutCache    *nautilus_smooth_text_layout_cache_new      (void);
NautilusSmoothTextLayout	 *nautilus_smooth_text_layout_cache_render   (NautilusSmoothTextLayoutCache *cache,
									      const char		    *text,
									      int			     text_length,
									      NautilusScalableFont	    *font,
									      int			     font_size,
									      gboolean			     wrap,
									      int			     line_spacing,
									      int			     max_text_width);

#if !defined (NAUTILUS_OMIT_SELF_CHECK)
void				  nautilus_self_check_smooth_text_layout_cache (void);
#endif /* NAUTILUS_OMIT_SELF_CHECK */

END_GNOME_DECLS

#endif /* NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE_H */
