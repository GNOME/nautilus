/* FIXME bugzilla.eazel.com 5813:
 * As soon as gtk 1.2.9 is released, this hack needs to be exorcised.
 */


/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>

static void gdk_image_put_normal (GdkDrawable *drawable,
				  GdkGC       *gc,
				  GdkImage    *image,
				  gint         xsrc,
				  gint         ysrc,
				  gint         xdest,
				  gint         ydest,
				  gint         width,
				  gint         height);

GdkImage* NAUTILUS_BUG_5712_PR3_WORKAROUND__gdk_image_get (GdkWindow *window,
							   gint       x,
							   gint       y,
							   gint       width,
							   gint       height);

GdkImage*
NAUTILUS_BUG_5712_PR3_WORKAROUND__gdk_image_get (GdkWindow *window,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  GdkImage *image;
  GdkImagePrivate *private;
  GdkWindowPrivate *win_private;
  XImage *ximage;

  g_return_val_if_fail (window != NULL, NULL);

  win_private = (GdkWindowPrivate *) window;
  if (win_private->destroyed)
    return NULL;

  ximage = XGetImage (gdk_display,
		      win_private->xwindow,
		      x, y, width, height,
		      AllPlanes, ZPixmap);
  
  if (ximage == NULL)
    return NULL;
  
  private = g_new (GdkImagePrivate, 1);
  image = (GdkImage*) private;

  private->xdisplay = gdk_display;
  private->image_put = gdk_image_put_normal;
  private->ximage = ximage;
  image->type = GDK_IMAGE_NORMAL;
  image->visual = gdk_window_get_visual (window);
  image->width = width;
  image->height = height;
  image->depth = private->ximage->depth;

  image->mem = private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  image->bpp = private->ximage->bits_per_pixel;
  image->byte_order = private->ximage->byte_order;

  return image;
}

static void
gdk_image_put_normal (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkImage    *image,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height)
{
  GdkWindowPrivate *drawable_private;
  GdkImagePrivate *image_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  image_private = (GdkImagePrivate*) image;
  gc_private = (GdkGCPrivate*) gc;

  g_return_if_fail (image->type == GDK_IMAGE_NORMAL);

  XPutImage (drawable_private->xdisplay, drawable_private->xwindow,
	     gc_private->xgc, image_private->ximage,
	     xsrc, ysrc, xdest, ydest, width, height);
}
