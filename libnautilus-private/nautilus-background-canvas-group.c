/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-background.c: Object for the background of a widget.
 
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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-background-canvas-group.h"

#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>

#include "nautilus-background.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gtk-macros.h"

#include <stdio.h>

static void nautilus_background_canvas_group_initialize_class (gpointer	klass);
static void nautilus_background_canvas_group_initialize       (gpointer	object,
															   gpointer         klass);

static void nautilus_background_canvas_group_update (GnomeCanvasItem	*item,
						     double		 affine[6],
						     ArtSVP		*clip_path,
						     gint		 flags);
													 
static void nautilus_background_canvas_group_draw (GnomeCanvasItem	*item,
						   GdkDrawable		*drawable,
						   int			 x,
						   int			 y,
						   int			 width,
						   int			 height);
static void nautilus_background_canvas_group_render (GnomeCanvasItem	*item,
						     GnomeCanvasBuf	*buffer);


typedef GnomeCanvasGroup NautilusBackgroundCanvasGroup;
typedef GnomeCanvasGroupClass NautilusBackgroundCanvasGroupClass;

#define NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP \
	(nautilus_background_canvas_group_get_type ())
#define NAUTILUS_BACKGROUND_CANVAS_GROUP(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP, NautilusBackgroundCanvasGroup))
#define NAUTILUS_BACKGROUND_CANVAS_GROUP_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP, NautilusBackgroundCanvasGroupClass))
#define NAUTILUS_IS_BACKGROUND_CANVAS_GROUP(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP))
#define NAUTILUS_IS_BACKGROUND_CANVAS_GROUP_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP))

GtkType nautilus_background_canvas_group_get_type (void);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBackgroundCanvasGroup, nautilus_background_canvas_group, GNOME_TYPE_CANVAS_GROUP)

static void
nautilus_background_canvas_group_initialize_class (gpointer klass)
{
	GNOME_CANVAS_ITEM_CLASS (klass)->draw   = nautilus_background_canvas_group_draw;
	GNOME_CANVAS_ITEM_CLASS (klass)->render = nautilus_background_canvas_group_render;
	GNOME_CANVAS_ITEM_CLASS (klass)->update = nautilus_background_canvas_group_update;
}

/* This function is for initialization code that's needed both when we're allocating
 * a new NautilusBackgroundCanvasGroup object as well as when we're taking over an existing
 * GnomeCanvasGroup item (replacing its klass).
 */
static void
nautilus_background_canvas_group_initialize_common (NautilusBackgroundCanvasGroup *canvas_group)
{
#if 0
	/* gnome_canvas_set_dither is only available in gnome-libs > v 1.2.8
	 * We're not doing this now because:
	 *  - currently a version of gnome-libs > 1.2.8 is not widely available
	 *  - originally, this dithering was to avoid striations in background
	 *    gradients in 16 bit mode. This is no longer an issue because
	 *    background now dither themselves (in both aa and non-aa mode).
	 */
#ifdef HAVE_GNOME_CANVAS_SET_DITHER
	gnome_canvas_set_dither (GNOME_CANVAS_ITEM (canvas_group)->canvas, GDK_RGB_DITHER_MAX);
#endif
#endif
}

static void
nautilus_background_canvas_group_initialize (gpointer object, gpointer klass)
{
	/* The way we currently use nautilus_background_canvas_group, assigning
	 * it to the klass of a root canvas object, circumvents this initialze fn.
	 */
	nautilus_background_canvas_group_initialize_common (NAUTILUS_BACKGROUND_CANVAS_GROUP (object));
}


void
nautilus_background_canvas_group_supplant_root_class (GnomeCanvas *canvas)
{
	/* Attach ourselves to a canvas in a way that will work.
	   Changing the style is not sufficient.

	   Since there's no signal to override in GnomeCanvas to control
	   drawing the background, we change the class of the canvas root.
	   This gives us a chance to draw the background before any of the
	   objects draw themselves, and has no effect on the bounds or
	   anything related to scrolling.

	   We settled on this after less-than-thrilling results using a
	   canvas item as the background. The canvas item contributed to
	   the bounds of the canvas and had to constantly be resized.
	*/
	
	g_assert (GNOME_IS_CANVAS (canvas));
	
	if (GTK_OBJECT (canvas->root)->klass != gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP)) {
	
		g_assert (GTK_OBJECT (canvas->root)->klass == gtk_type_class (GNOME_TYPE_CANVAS_GROUP));
	
		GTK_OBJECT (canvas->root)->klass =
			gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP);

		nautilus_background_canvas_group_initialize_common (NAUTILUS_BACKGROUND_CANVAS_GROUP (canvas->root));
	}
}

static void
nautilus_background_canvas_group_update (GnomeCanvasItem *item,
					 double affine[6],
					 ArtSVP *clip_path,
					 gint flags)
{
	NautilusBackground *background = nautilus_get_widget_background (GTK_WIDGET (item->canvas));

	nautilus_background_pre_draw (background,
				      GTK_WIDGET (item->canvas)->allocation.width,
				      GTK_WIDGET (item->canvas)->allocation.height);

	NAUTILUS_CALL_PARENT (GNOME_CANVAS_ITEM_CLASS, update, (item, affine, clip_path, flags));				     
}

static void
nautilus_background_canvas_group_draw (GnomeCanvasItem *item,
				       GdkDrawable *drawable,
				       int x,
				       int y,
				       int width,
				       int height)
{
	NautilusBackground *background;
	GdkGC *gc;

	/* Draw the background. */
	background = nautilus_get_widget_background (GTK_WIDGET (item->canvas));

	/* If GtkStyle handled it, then we don't want to bother doing
	 * any additional work. It would be way slow to draw again.
	 */
	if (nautilus_background_is_too_complex_for_gtk_style (background)) {
		/* Create a new gc each time.
		 * If this is a speed problem, we can create one and keep it around,
		 * but it's a bit more complicated to ensure that it's always compatible
		 * with whatever drawable is passed in.
		 */
		gc = gdk_gc_new (drawable);

		/* FIXME bugzilla.eazel.com 5349:
		 * It shouldn't be necessary to call nautilus_background_pre_draw here.
		 * However, nautilus_background_canvas_group_update (who should be the one
		 * spot we call nautilus_background_pre_draw from) doesn't seemed to get
		 * called prior to all drawing.
		 */
		nautilus_background_pre_draw (background,
					      GTK_WIDGET (item->canvas)->allocation.width,
					      GTK_WIDGET (item->canvas)->allocation.height);

		nautilus_background_draw (background, drawable, gc, x, y, width, height);

		gdk_gc_unref (gc);
	}

	/* Call through to the GnomeCanvasGroup implementation, which
	 * will draw all the canvas items.
	 */
	NAUTILUS_CALL_PARENT (GNOME_CANVAS_ITEM_CLASS, draw, (item, drawable, x, y, width, height));				     
}


/* draw the background for the anti-aliased canvas case */
static void
nautilus_background_canvas_group_render (GnomeCanvasItem *item, GnomeCanvasBuf *buffer)
{
	NautilusBackground *background;
			
	background = nautilus_get_widget_background (GTK_WIDGET (item->canvas));
	if (background != NULL) {
		/* FIXME bugzilla.eazel.com 5349:
		 * It shouldn't be necessary to call nautilus_background_pre_draw here.
		 * However, nautilus_background_canvas_group_update (who should be the one
		 * spot we call nautilus_background_pre_draw from) doesn't seemed to get
		 * called prior to all drawing.
		 */
		nautilus_background_pre_draw (background,
					      GTK_WIDGET (item->canvas)->allocation.width,
					      GTK_WIDGET (item->canvas)->allocation.height);

		nautilus_background_draw_aa (background, buffer);
	}
	
	/* Call through to the GnomeCanvasGroup implementation, which will draw all
	 * the canvas items.
	 */
	NAUTILUS_CALL_PARENT (GNOME_CANVAS_ITEM_CLASS, render, (item, buffer));
}
