/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-layout.c

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

#include <glib.h>

#include "gnome-icon-container.h"
#include "gnome-icon-container-private.h"

#include "gnome-icon-container-layout.h"


struct _IconLayoutInfo {
	gchar *text;
	gint x, y;
};
typedef struct _IconLayoutInfo IconLayoutInfo;


struct _GnomeIconContainerLayout {
	GHashTable *name_to_layout;
};


GnomeIconContainerLayout *
gnome_icon_container_layout_new (void)
{
	GnomeIconContainerLayout *new;

	new = g_new (GnomeIconContainerLayout, 1);
	new->name_to_layout = g_hash_table_new (g_str_hash, g_str_equal);

	return new;
}

static void
hash_foreach_destroy (gpointer key,
		      gpointer value,
		      gpointer data)
{
	IconLayoutInfo *info;

	info = (IconLayoutInfo *) value;
	g_free (info->text);
	g_free (info);
}

void
gnome_icon_container_layout_free (GnomeIconContainerLayout *layout)
{
	g_return_if_fail (layout != NULL);

	g_hash_table_foreach (layout->name_to_layout,
			      hash_foreach_destroy, NULL);
	g_hash_table_destroy (layout->name_to_layout);

	g_free (layout);
}


void
gnome_icon_container_layout_add (GnomeIconContainerLayout *layout,
				 const gchar *icon_text,
				 gint x,
				 gint y)
{
	IconLayoutInfo *info;

	g_return_if_fail (layout != NULL);
	g_return_if_fail (icon_text != NULL);

	info = g_new (IconLayoutInfo, 1);
	info->text = g_strdup (icon_text);
	info->x = x;
	info->y = y;

	g_hash_table_remove (layout->name_to_layout, info->text);
	g_hash_table_insert (layout->name_to_layout, info->text, info);
}

gboolean
gnome_icon_container_layout_get_position (const GnomeIconContainerLayout *layout,
					  const gchar *icon_text,
					  gint *x_return,
					  gint *y_return)
{
	IconLayoutInfo *info;

	g_return_val_if_fail (layout != NULL, FALSE);
	g_return_val_if_fail (icon_text != NULL, FALSE);

	info = g_hash_table_lookup (layout->name_to_layout, icon_text);
	if (info == NULL)
		return FALSE;

	*x_return = info->x;
	*y_return = info->y;

	return TRUE;
}


struct _ForeachData {
	GnomeIconContainerLayout *layout;
	GnomeIconContainerLayoutForeachFunc callback;
	gpointer callback_data;
};
typedef struct _ForeachData ForeachData;

static void
foreach_helper (gpointer key,
		gpointer value,
		gpointer user_data)
{
	IconLayoutInfo *info;
	ForeachData *data;

	info = (IconLayoutInfo *) value;
	data = (ForeachData *) user_data;

	(* data->callback) (data->layout, info->text, info->x, info->y,
			    data->callback_data);
}

void
gnome_icon_container_layout_foreach (GnomeIconContainerLayout *layout,
				     GnomeIconContainerLayoutForeachFunc callback,
				     gpointer callback_data)
{
	ForeachData *data;

	g_return_if_fail (layout != NULL);
	g_return_if_fail (callback != NULL);

	data = g_new (ForeachData, 1);
	data->layout = layout;
	data->callback = callback;
	data->callback_data = callback_data;

	g_hash_table_foreach (layout->name_to_layout, foreach_helper, data);

	g_free (data);
}
