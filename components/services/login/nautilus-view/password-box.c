/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
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
 * Authors: Robey Pointer <robey@eazel.com>
 */

#include <config.h>

#include "password-box.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#include <gnome.h>
#include <eel/eel-background.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-label.h>
#include <eel/eel-image.h>
#include <stdio.h>
#include <unistd.h>



/* gtk rulez */
static void
add_padding_to_box (GtkWidget *box, int pad_x, int pad_y)
{
	GtkWidget *filler;

	filler = gtk_label_new ("");
	gtk_widget_set_usize (filler, pad_x ? pad_x : 1, pad_y ? pad_y : 1);
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (box), filler, FALSE, FALSE, 0);
}

static void
add_filler_to_box (GtkWidget *box)
{
	GtkWidget *filler;

	filler = gtk_label_new ("");
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (box), filler, TRUE, TRUE, 0);
}

static gboolean
line_expose (GtkWidget *widget, GdkEventExpose *event)
{
	gdk_window_clear_area (widget->window, event->area.x, event->area.y, event->area.width, event->area.height);
	gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], &event->area);
	gdk_draw_line (widget->window, widget->style->fg_gc[widget->state],
		       widget->allocation.width/2, event->area.y,
		       widget->allocation.width/2, event->area.y + event->area.height);
	gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], NULL);

	return TRUE;
}

static GtkWidget *
vertical_line_new (int width)
{
	GtkWidget *line;
	EelBackground *background;

	line = gtk_drawing_area_new ();
	gtk_drawing_area_size (GTK_DRAWING_AREA (line), 1, 10);
	gtk_signal_connect (GTK_OBJECT (line), "expose_event", GTK_SIGNAL_FUNC (line_expose), NULL);
	gtk_widget_set_usize (line, width, -2);

	background = eel_get_widget_background (line);
	eel_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	return line;
}

static GtkWidget *
make_empty_viewport (void)
{
	GtkWidget *viewport;
	GtkWidget *hbox;

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (viewport), hbox);
	gtk_widget_show (viewport);
	return viewport;
}

static void
gtk_widget_set_colors (GtkWidget *widget, int foreground, int background)
{
	EelBackground *bground;
	GtkStyle *style;
	GdkColor *color;
	char *spec;

	bground = eel_get_widget_background (widget);
	spec = g_strdup_printf ("#%06X", background);
	eel_background_set_color (bground, spec);
	g_free (spec);

	/* foreground is much harder */
        style = gtk_style_copy (widget->style);
        color = &(style->fg[GTK_STATE_NORMAL]);
        color->red = ((foreground >> 8) & 0xff00) | (foreground >> 16);
        color->green = (foreground & 0xff00) | ((foreground >> 8) & 0xff);
        color->blue = ((foreground & 0xff) << 8) | (foreground & 0xff);
        gdk_colormap_alloc_color (gtk_widget_get_colormap (widget), color, FALSE, TRUE);
        gtk_widget_set_style (widget, style);
        gtk_style_unref (style);
}

/* okay, this is confusing and annoying, but it looks nice.
 * make a 3x3 grid: in the corners, little tiny images of rounded
 * corners.  on the top/bottom/left/right edges, empty viewports set
 * to a background color.  in the middle, a big viewport with a
 * background color where the entire contents of the password box fits.
 */
static GtkWidget *
make_bubble_shell (void)
{
        GtkWidget *table;
	GtkWidget *viewport;
	GtkWidget *image;

	table = gtk_table_new (3, 3, FALSE);

	image = eazel_services_image_new (BUBBLE_UL_FILENAME, NULL, 0);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 1, 0, 0, 0, 0);
	image = eazel_services_image_new (BUBBLE_UR_FILENAME, NULL, 0);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table), image, 2, 3, 0, 1, 0, 0, 0, 0);
	image = eazel_services_image_new (BUBBLE_LL_FILENAME, NULL, 0);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table), image, 0, 1, 2, 3, 0, 0, 0, 0);
	image = eazel_services_image_new (BUBBLE_LR_FILENAME, NULL, 0);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table), image, 2, 3, 2, 3, 0, 0, 0, 0);

	viewport = make_empty_viewport ();
	gtk_table_attach (GTK_TABLE (table), viewport, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL|GTK_SHRINK, 0, 0, 0);
	gtk_widget_set_usize (viewport, 0, 7);
	viewport = make_empty_viewport ();
	gtk_table_attach (GTK_TABLE (table), viewport, 1, 2, 2, 3, GTK_EXPAND|GTK_FILL|GTK_SHRINK, 0, 0, 0);
	gtk_widget_set_usize (viewport, 0, 7);
	viewport = make_empty_viewport ();
	gtk_table_attach (GTK_TABLE (table), viewport, 0, 1, 1, 2, 0, GTK_EXPAND|GTK_FILL|GTK_SHRINK, 0, 0);
	gtk_widget_set_usize (viewport, 7, 0);
	viewport = make_empty_viewport ();
	gtk_table_attach (GTK_TABLE (table), viewport, 2, 3, 1, 2, 0, GTK_EXPAND|GTK_FILL|GTK_SHRINK, 0, 0);
	gtk_widget_set_usize (viewport, 7, 0);

        return table;
}

static void
change_bubble_shell_colors (GtkWidget *table, int foreground, int background)
{
        GList *iter;
        GtkWidget *widget;

	for (iter = gtk_container_children (GTK_CONTAINER (table)); iter != NULL; iter = iter->next) {
		widget = (GtkWidget *)(iter->data);
		if (EEL_IS_IMAGE (widget)) {
			/* images are the little rounded corners arlo drew */
			eel_image_set_solid_background_color (EEL_IMAGE (widget), background);
		} else {
			gtk_widget_set_colors (widget, foreground, background);
		}
	}
}

void
password_box_set_colors (PasswordBox *box, int foreground, int background)
{
        char *text;

        /* if we don't hide/show during the color change, some widgets won't draw correctly [gtk rulez] */
        gtk_widget_hide (box->table);
	eel_label_set_solid_background_color (EEL_LABEL (box->label), background);
	eel_label_set_solid_background_color (EEL_LABEL (box->label_right), background);
	eel_label_set_text_color (EEL_LABEL (box->label), foreground);
	eel_label_set_text_color (EEL_LABEL (box->label_right), foreground);
	eel_image_set_solid_background_color (EEL_IMAGE (box->bong), background);

        /* you DON'T want to know.
         * ... doing this pokes the label enough that it'll wake up and notice its new colors.
         */
        text = g_strdup (eel_label_get_text (EEL_LABEL (box->label)));
        eel_label_set_text (EEL_LABEL (box->label), "-");
        eel_label_set_text (EEL_LABEL (box->label), text);
        g_free (text);

	gtk_widget_set_colors (box->line, foreground, background);
	gtk_widget_set_colors (box->viewport, foreground, background);
        change_bubble_shell_colors (box->table, foreground, background);
        gtk_widget_show (box->table);
}

GtkWidget *
password_box_get_entry (PasswordBox *box)
{
        return box->entry;
}

void
password_box_set_error_text (PasswordBox *box, const char *message)
{
	eel_label_set_text (EEL_LABEL (box->label_right), message);
}

void
password_box_show_error (PasswordBox *box, gboolean show_it)
{
        if (show_it) {
                password_box_set_colors (box, BUBBLE_FOREGROUND_ALERT_RGB, BUBBLE_BACKGROUND_ALERT_RGB);
                gtk_widget_show (box->hbox_right);
        } else {
                password_box_set_colors (box, BUBBLE_FOREGROUND_RGB, BUBBLE_BACKGROUND_RGB);
                gtk_widget_hide (box->hbox_right);
        }
}

PasswordBox *
password_box_new (const char *title)
{
	PasswordBox *box;

	box = g_new0 (PasswordBox, 1);

	/* title label for the entry box */
	box->label = eazel_services_label_new (title, 0, 0.0, 0.0, 0, 0,
					       BUBBLE_FOREGROUND_RGB,
					       BUBBLE_BACKGROUND_RGB,
					       NULL, 0, TRUE);
	eel_label_set_text (EEL_LABEL (box->label), title);
	gtk_widget_show (box->label);

	/* text entry, offset to the right a bit */
	box->hbox_left = gtk_hbox_new (FALSE, 0);
	add_padding_to_box (box->hbox_left, 20, 0);
	box->entry = gtk_entry_new_with_max_length (36);
	gtk_widget_show (box->entry);
	gtk_box_pack_start (GTK_BOX (box->hbox_left), box->entry, FALSE, FALSE, 0);
	add_filler_to_box (box->hbox_left);
	gtk_widget_show (box->hbox_left);

	/* put them together into a left-side vbox */
	box->vbox_left = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box->vbox_left), box->label, FALSE, FALSE, 0);
	add_padding_to_box (box->vbox_left, 0, 10);
	gtk_box_pack_start (GTK_BOX (box->vbox_left), box->hbox_left, FALSE, FALSE, 0);
	/* add_filler_to_box (box->vbox_left); */
	gtk_widget_show (box->vbox_left);

	/* right-side bong */
	box->bong = eazel_services_image_new (TINY_ALERT_ICON_FILENAME, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
	gtk_widget_show (box->bong);
	box->vbox_bong = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box->vbox_bong), box->bong, FALSE, FALSE, 0);
	add_filler_to_box (box->vbox_bong);
	gtk_widget_show (box->vbox_bong);

	/* vertical dividing line */
	box->line = vertical_line_new (9);
	gtk_widget_show (box->line);

	/* right-side label */
	box->label_right = eazel_services_label_new (title, 0, 0.0, 0.0, 0, 0,
						     BUBBLE_FOREGROUND_RGB,
						     BUBBLE_BACKGROUND_RGB,
						     NULL, -2, FALSE);
	eel_label_set_wrap (EEL_LABEL (box->label_right), TRUE);
	eel_label_set_justify (EEL_LABEL (box->label_right), GTK_JUSTIFY_LEFT);
	gtk_widget_show (box->label_right);

	/* hbox for all the right-side alert messages */
	box->hbox_right = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box->hbox_right), box->vbox_bong, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box->hbox_right), box->line, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box->hbox_right), box->label_right, FALSE, FALSE, 0);
	gtk_widget_show (box->hbox_right);

	/* floating in the middle of the right side */
	box->alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (box->alignment), box->hbox_right);
	gtk_widget_show (box->alignment);

	/* and finally, the overall hbox holds them all */
	box->hbox = gtk_hbox_new (FALSE, 0);
	add_padding_to_box (box->hbox, 5, 0);
	gtk_box_pack_start (GTK_BOX (box->hbox), box->vbox_left, FALSE, FALSE, 0);
	add_padding_to_box (box->hbox, 10, 0);
	gtk_box_pack_start (GTK_BOX (box->hbox), box->alignment, TRUE, TRUE, 0);
	gtk_widget_show (box->hbox);

	/* ...inside a viewport, so we can set the background color */
	box->viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (box->viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (box->viewport), box->hbox);
	gtk_widget_show (box->viewport);

        /* put it inside that rounded-edged box */
        box->table = make_bubble_shell ();
	gtk_table_attach (GTK_TABLE (box->table), box->viewport, 1, 2, 1, 2,
			  GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);

        password_box_show_error (box, FALSE);

	return box;
}
