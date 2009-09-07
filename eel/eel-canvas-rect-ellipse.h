/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
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
 */
/*
  @NOTATION@
 */
/* Rectangle and ellipse item types for EelCanvas widget
 *
 * EelCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef EEL_CANVAS_RECT_ELLIPSE_H
#define EEL_CANVAS_RECT_ELLIPSE_H


#include <eel/eel-canvas.h>

G_BEGIN_DECLS


/* Base class for rectangle and ellipse item types.  These are defined by their top-left and
 * bottom-right corners.  Rectangles and ellipses share the following arguments:
 *
 * name			type		read/write	description
 * ------------------------------------------------------------------------------------------
 * x1			double		RW		Leftmost coordinate of rectangle or ellipse
 * y1			double		RW		Topmost coordinate of rectangle or ellipse
 * x2			double		RW		Rightmost coordinate of rectangle or ellipse
 * y2			double		RW		Bottommost coordinate of rectangle or ellipse
 * fill_color		string		W		X color specification for fill color,
 *							or NULL pointer for no color (transparent)
 * fill_color_gdk	GdkColor*	RW		Allocated GdkColor for fill
 * outline_color	string		W		X color specification for outline color,
 *							or NULL pointer for no color (transparent)
 * outline_color_gdk	GdkColor*	RW		Allocated GdkColor for outline
 * fill_stipple		GdkBitmap*	RW		Stipple pattern for fill
 * outline_stipple	GdkBitmap*	RW		Stipple pattern for outline
 * width_pixels		uint		RW		Width of the outline in pixels.  The outline will
 *							not be scaled when the canvas zoom factor is changed.
 * width_units		double		RW		Width of the outline in canvas units.  The outline
 *							will be scaled when the canvas zoom factor is changed.
 */


#define EEL_TYPE_CANVAS_RE            (eel_canvas_re_get_type ())
#define EEL_CANVAS_RE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_CANVAS_RE, EelCanvasRE))
#define EEL_CANVAS_RE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_CANVAS_RE, EelCanvasREClass))
#define EEL_IS_CANVAS_RE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_CANVAS_RE))
#define EEL_IS_CANVAS_RE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_CANVAS_RE))
#define EEL_CANVAS_RE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_CANVAS_RE, EelCanvasREClass))


typedef struct _EelCanvasRE      EelCanvasRE;
typedef struct _EelCanvasREClass EelCanvasREClass;

struct _EelCanvasRE {
	EelCanvasItem item;

	GdkBitmap *fill_stipple;	/* Stipple for fill */
	GdkBitmap *outline_stipple;	/* Stipple for outline */

	GdkGC *fill_gc;			/* GC for filling */
	GdkGC *outline_gc;		/* GC for outline */

	gulong fill_pixel;		/* Fill color */
	gulong outline_pixel;		/* Outline color */

	double x1, y1, x2, y2;		/* Corners of item */
	double width;			/* Outline width */

	guint fill_color;		/* Fill color, RGBA */
	guint outline_color;		/* Outline color, RGBA */

	/* Configuration flags */

	unsigned int fill_set : 1;	/* Is fill color set? */
	unsigned int outline_set : 1;	/* Is outline color set? */
	unsigned int width_pixels : 1;	/* Is outline width specified in pixels or units? */
};

struct _EelCanvasREClass {
	EelCanvasItemClass parent_class;
};


/* Standard Gtk function */
GType eel_canvas_re_get_type (void) G_GNUC_CONST;


/* Rectangle item.  No configurable or queryable arguments are available (use those in
 * EelCanvasRE).
 */


#define EEL_TYPE_CANVAS_RECT            (eel_canvas_rect_get_type ())
#define EEL_CANVAS_RECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_CANVAS_RECT, EelCanvasRect))
#define EEL_CANVAS_RECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_CANVAS_RECT, EelCanvasRectClass))
#define EEL_IS_CANVAS_RECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_CANVAS_RECT))
#define EEL_IS_CANVAS_RECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_CANVAS_RECT))
#define EEL_CANVAS_RECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_CANVAS_RECT, EelCanvasRectClass))


typedef struct _EelCanvasRect EelCanvasRect;
typedef struct _EelCanvasRectPrivate EelCanvasRectPrivate;
typedef struct _EelCanvasRectClass EelCanvasRectClass;

struct _EelCanvasRect {
	EelCanvasRE re;
	EelCanvasRectPrivate *priv;
};

struct _EelCanvasRectClass {
	EelCanvasREClass parent_class;
};


/* Standard Gtk function */
GType eel_canvas_rect_get_type (void) G_GNUC_CONST;


/* Ellipse item.  No configurable or queryable arguments are available (use those in
 * EelCanvasRE).
 */


#define EEL_TYPE_CANVAS_ELLIPSE            (eel_canvas_ellipse_get_type ())
#define EEL_CANVAS_ELLIPSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_CANVAS_ELLIPSE, EelCanvasEllipse))
#define EEL_CANVAS_ELLIPSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_CANVAS_ELLIPSE, EelCanvasEllipseClass))
#define EEL_IS_CANVAS_ELLIPSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_CANVAS_ELLIPSE))
#define EEL_IS_CANVAS_ELLIPSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_CANVAS_ELLIPSE))
#define EEL_CANVAS_ELLIPSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_CANVAS_ELLIPSE, EelCanvasEllipseClass))


typedef struct _EelCanvasEllipse EelCanvasEllipse;
typedef struct _EelCanvasEllipseClass EelCanvasEllipseClass;

struct _EelCanvasEllipse {
	EelCanvasRE re;
};

struct _EelCanvasEllipseClass {
	EelCanvasREClass parent_class;
};


/* Standard Gtk function */
GType eel_canvas_ellipse_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif
