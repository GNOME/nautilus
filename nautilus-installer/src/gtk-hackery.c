/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * gtk is very low-level, so here are "helper" functions to make it a little
 * more usable.  it's kind of like your grandmother in her walker: she's
 * perfectly capable of going to the grocery store, you just have to "help"
 * her a little.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 *          Robey Pointer <robey@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "installer.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>


extern int installer_debug;

/* better than a macro, and uses our nice logging system */
void
log_debug (const gchar *format, ...)
{
	va_list args;

	if (installer_debug) {
		va_start (args, format);
		g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
		va_end (args);
	}
}

void
get_pixmap_width_height (char **xpmdata, int *width, int *height)
{
	char *ptr;

	ptr = strchr (xpmdata[0], ' ');
	if (ptr == NULL) {
		*width = *height = 0;
		return;
	}
	*width = atoi (xpmdata[0]);
	*height = atoi (ptr);
}

GdkPixbuf *
create_pixmap (GtkWidget *widget, char **xpmdata)
{
	GdkColormap *colormap;                                                        
	GdkPixmap *gdkpixmap;                                                         
	GdkBitmap *mask;	
	GdkPixbuf *pixbuf;
	int width, height;

	get_pixmap_width_height (xpmdata, &width, &height);
	colormap = gtk_widget_get_colormap (widget);
	gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask, NULL, (gchar **)xpmdata); 
	g_assert (gdkpixmap != NULL);
	pixbuf = gdk_pixbuf_get_from_drawable (NULL, gdkpixmap, colormap, 0, 0, 0, 0, width, height);
	gdk_pixmap_unref (gdkpixmap);   
	if (mask != NULL) {
		gdk_bitmap_unref (mask);
	}

	return pixbuf;     
}

GtkWidget *
create_gtk_pixmap (GtkWidget *widget, char **xpmdata)
{
        GdkColormap *colormap;
        GdkPixmap *gdkpixmap;
        GdkBitmap *mask;
        GtkWidget *pixmap;

        colormap = gtk_widget_get_colormap (widget);
        gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask, NULL, (gchar **)xpmdata);
        g_assert (gdkpixmap != NULL);
        pixmap = gtk_pixmap_new (gdkpixmap, mask);
        g_assert (pixmap != NULL);

        return pixmap;
}

GtkWidget *
gtk_label_new_with_font (const char *text, const char *fontname)
{
	GtkWidget *label;
	GtkStyle *style;
	GdkFont *font;

	/* oh how low we've sunk... */
	label = gtk_label_new (text);
	style = gtk_style_copy (label->style);
	font = gdk_fontset_load (fontname);
	if (font == NULL) {
		g_warning ("unable to load font '%s'!", fontname);
	} else {
		gdk_font_unref (style->font);
		style->font = font;
	}
	gtk_widget_set_style (label, style);
	gtk_style_unref (style);

	return label;
}

void
gtk_label_set_color (GtkWidget *label, guint32 rgb)
{
        GtkStyle *style;
        GdkColor *color;

        style = gtk_style_copy (label->style);
        color = &(style->fg[GTK_STATE_NORMAL]);
        color->red = ((rgb >> 8) & 0xff00) | (rgb >> 16);
        color->green = (rgb & 0xff00) | ((rgb >> 8) & 0xff);
        color->blue = ((rgb & 0xff) << 8) | (rgb & 0xff);
        gdk_colormap_alloc_color (gtk_widget_get_colormap (label), color, FALSE, TRUE);
        gtk_widget_set_style (label, style);
        gtk_style_unref (style);
}

void
gtk_box_add_padding (GtkWidget *box, int pad_x, int pad_y)
{
	GtkWidget *filler;

	filler = gtk_label_new ("");
	gtk_widget_set_usize (filler, pad_x ? pad_x : 1, pad_y ? pad_y : 1);
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (box), filler, FALSE, FALSE, 0);
}

/* sometimes you want a label to be in a vbox, but still be left-justified.
 * here's how that's done in the magical world of gtk.
 */
GtkWidget *
gtk_label_as_hbox (GtkWidget *label)
{
        GtkWidget *hbox;
        GtkWidget *crap;

        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        crap = gtk_label_new ("");
        gtk_widget_show (crap);
        gtk_box_pack_start (GTK_BOX (hbox), crap, TRUE, TRUE, 0);
        gtk_widget_show (hbox);
        return hbox;
}

GtkWidget *
gtk_box_nth (GtkWidget *box, int n)
{
	GList *list;
        GtkBoxChild *child;

	list = GTK_BOX (box)->children;
	if ((n < 0) || (n >= g_list_length (list))) {
		return NULL;
	}
        child = (GtkBoxChild *)(g_list_nth (list, n)->data);
        return GTK_WIDGET (child->widget);
}

/* do what gnome ought to do automatically */
void
gnome_reply_callback (int reply, gboolean *answer)
{
	*answer = (reply == 0);
}
