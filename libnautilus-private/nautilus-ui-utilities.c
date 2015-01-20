/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-ui-utilities.c - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

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
   see <http://www.gnu.org/licenses/>.

   Authors: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>

#include "nautilus-ui-utilities.h"
#include "nautilus-icon-info.h"
#include <eel/eel-graphic-effects.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>

static GMenuModel *
find_gmenu_model (GMenuModel  *model,
		  const gchar *model_id)
{
	gint i, n_items;
	GMenuModel *insertion_model = NULL;

	n_items = g_menu_model_get_n_items (model);

	for (i = 0; i < n_items && !insertion_model; i++) {
		gchar *id = NULL;
		if (g_menu_model_get_item_attribute (model, i, "id", "s", &id) &&
		    g_strcmp0 (id, model_id) == 0) {
			insertion_model = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
			if (!insertion_model)
				insertion_model = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
		} else {
			GMenuModel *submodel;
			GMenuModel *submenu;
			gint j, j_items;

			submodel = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);

			if (!submodel)
			        submodel = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);

			if (!submodel)
				continue;

			j_items = g_menu_model_get_n_items (submodel);
			for (j = 0; j < j_items; j++) {
				submenu = g_menu_model_get_item_link (submodel, j, G_MENU_LINK_SUBMENU);
				if (submenu) {
					insertion_model = find_gmenu_model (submenu, model_id);
					g_object_unref (submenu);
				}

				if (insertion_model)
					break;
			}

			g_object_unref (submodel);
		}

		g_free (id);
	}

	return insertion_model;
}

/*
 * The original GMenu is modified adding to the section @submodel_name
 * the items in @gmenu_to_merge.
 * @gmenu_to_merge should be a list of menu items.
 */
void
nautilus_gmenu_merge (GMenu       *original,
		      GMenu       *gmenu_to_merge,
		      const gchar *submodel_name,
		      gboolean     prepend)
{
	gint i, n_items;
	GMenuModel *submodel;
	GMenuItem *item;

	g_return_if_fail (G_IS_MENU (original));
	g_return_if_fail (G_IS_MENU (gmenu_to_merge));

	submodel = find_gmenu_model (G_MENU_MODEL (original), submodel_name);

	g_return_if_fail (submodel != NULL);

	n_items = g_menu_model_get_n_items (G_MENU_MODEL (gmenu_to_merge));

	for (i = 0; i < n_items; i++) {
		item = g_menu_item_new_from_model (G_MENU_MODEL (gmenu_to_merge), i);
		if (prepend)
			g_menu_prepend_item (G_MENU (submodel), item);
		else
			g_menu_append_item (G_MENU (submodel), item);
		g_object_unref (item);
	}

	g_object_unref (submodel);
}

/*
 * The GMenu @menu is modified adding to the section @submodel_name
 * the item @item.
 */
void
nautilus_gmenu_add_item_in_submodel (GMenu       *menu,
				     GMenuItem   *item,
				     const gchar *submodel_name,
				     gboolean     prepend)
{
	GMenuModel *submodel;

	g_return_if_fail (G_IS_MENU (menu));
	g_return_if_fail (G_IS_MENU_ITEM (item));

	submodel = find_gmenu_model (G_MENU_MODEL (menu), submodel_name);

	g_return_if_fail (submodel != NULL);
	if (prepend)
		g_menu_prepend_item (G_MENU (submodel), item);
	else
		g_menu_append_item (G_MENU (submodel), item);

	g_object_unref (submodel);
}

void
nautilus_pop_up_context_menu (GtkWidget      *parent,
			      GMenu          *menu,
			      GdkEventButton *event)
{
	GtkWidget *gtk_menu;

	int button;

	g_return_if_fail (G_IS_MENU (menu));
	g_return_if_fail (GTK_IS_WIDGET (parent));

	gtk_menu = gtk_menu_new_from_model (G_MENU_MODEL (menu));
	gtk_menu_attach_to_widget (GTK_MENU (gtk_menu), parent, NULL);

	/* The event button needs to be 0 if we're popping up this menu from
	 * a button release, else a 2nd click outside the menu with any button
	 * other than the one that invoked the menu will be ignored (instead
	 * of dismissing the menu). This is a subtle fragility of the GTK menu code.
	 */
	if (event) {
		button = event->type == GDK_BUTTON_RELEASE
			? 0
			: event->button;
	} else {
		button = 0;
	}

	gtk_menu_popup (GTK_MENU (gtk_menu),			/* menu */
			NULL,					/* parent_menu_shell */
			NULL,					/* parent_menu_item */
			NULL,					/* popup_position_func */
			NULL,					/* popup_position_data */
			button,					/* button */
			event ? event->time : gtk_get_current_event_time ()); /* activate_time */

	g_object_ref_sink (gtk_menu);
	g_object_unref (gtk_menu);
}

GdkPixbuf *
nautilus_ui_get_menu_icon (const char *icon_name,
			   GtkWidget  *parent_widget)
{
	NautilusIconInfo *info;
	GdkPixbuf *pixbuf;
	int size;
	int scale;

	size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	scale = gtk_widget_get_scale_factor (parent_widget);

	if (g_path_is_absolute (icon_name)) {
		info = nautilus_icon_info_lookup_from_path (icon_name, size, scale);
	} else {
		info = nautilus_icon_info_lookup_from_name (icon_name, size, scale);
	}
	pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (info, size);
	g_object_unref (info);

	return pixbuf;
}

char *
nautilus_escape_action_name (const char *action_name,
			     const char *prefix)
{
	GString *s;

	if (action_name == NULL) {
		return NULL;
	}

	s = g_string_new (prefix);

	while (*action_name != 0) {
		switch (*action_name) {
		case '\\':
			g_string_append (s, "\\\\");
			break;
		case '/':
			g_string_append (s, "\\s");
			break;
		case '&':
			g_string_append (s, "\\a");
			break;
		case '"':
			g_string_append (s, "\\q");
			break;
		case ' ':
			g_string_append (s, "+");
			break;
		case '(':
			g_string_append (s, "#");
			break;
		case ')':
			g_string_append (s, "^");
			break;
		case ':':
			g_string_append (s, "\\\\");
			break;
		default:
			g_string_append_c (s, *action_name);
		}

		action_name ++;
	}
	return g_string_free (s, FALSE);
}

static GdkPixbuf *
nautilus_get_thumbnail_frame (void)
{
	static GdkPixbuf *thumbnail_frame = NULL;

	if (thumbnail_frame == NULL) {
		thumbnail_frame = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/icons/thumbnail_frame.png", NULL);
	}

	return thumbnail_frame;
}

#define NAUTILUS_THUMBNAIL_FRAME_LEFT 3
#define NAUTILUS_THUMBNAIL_FRAME_TOP 3
#define NAUTILUS_THUMBNAIL_FRAME_RIGHT 3
#define NAUTILUS_THUMBNAIL_FRAME_BOTTOM 3

void
nautilus_ui_frame_image (GdkPixbuf **pixbuf)
{
	GdkPixbuf *pixbuf_with_frame, *frame;
	int left_offset, top_offset, right_offset, bottom_offset;

	frame = nautilus_get_thumbnail_frame ();
	if (frame == NULL) {
		return;
	}

	left_offset = NAUTILUS_THUMBNAIL_FRAME_LEFT;
	top_offset = NAUTILUS_THUMBNAIL_FRAME_TOP;
	right_offset = NAUTILUS_THUMBNAIL_FRAME_RIGHT;
	bottom_offset = NAUTILUS_THUMBNAIL_FRAME_BOTTOM;

	pixbuf_with_frame = eel_embed_image_in_frame
		(*pixbuf, frame,
		 left_offset, top_offset, right_offset, bottom_offset);
	g_object_unref (*pixbuf);

	*pixbuf = pixbuf_with_frame;
}

static GdkPixbuf *filmholes_left = NULL;
static GdkPixbuf *filmholes_right = NULL;

static gboolean
ensure_filmholes (void)
{
	if (filmholes_left == NULL) {
		filmholes_left = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/icons/filmholes.png", NULL);
	}
	if (filmholes_right == NULL &&
	    filmholes_left != NULL) {
		filmholes_right = gdk_pixbuf_flip (filmholes_left, TRUE);
	}

	return (filmholes_left && filmholes_right);
}

void
nautilus_ui_frame_video (GdkPixbuf **pixbuf)
{
	int width, height;
	int holes_width, holes_height;
	int i;

	if (!ensure_filmholes ())
		return;

	width = gdk_pixbuf_get_width (*pixbuf);
	height = gdk_pixbuf_get_height (*pixbuf);
	holes_width = gdk_pixbuf_get_width (filmholes_left);
	holes_height = gdk_pixbuf_get_height (filmholes_left);

	for (i = 0; i < height; i += holes_height) {
		gdk_pixbuf_composite (filmholes_left, *pixbuf, 0, i,
				      MIN (width, holes_width),
				      MIN (height - i, holes_height),
				      0, i, 1, 1, GDK_INTERP_NEAREST, 255);
	}

	for (i = 0; i < height; i += holes_height) {
		gdk_pixbuf_composite (filmholes_right, *pixbuf,
				      width - holes_width, i,
				      MIN (width, holes_width),
				      MIN (height - i, holes_height),
				      width - holes_width, i,
				      1, 1, GDK_INTERP_NEAREST, 255);
	}
}
