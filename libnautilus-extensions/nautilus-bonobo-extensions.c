/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bonobo-extensions.c - implementation of new functions that conceptually
                                  belong in bonobo. Perhaps some of these will be
                                  actually rolled into bonobo someday.

   Copyright (C) 2000 Eazel, Inc.

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

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-bonobo-extensions.h"

#include <bonobo/bonobo-ui-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>

void
nautilus_bonobo_set_accelerator (BonoboUIComponent *ui,
			   	 const char *path,
			   	 const char *accelerator)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	bonobo_ui_component_set_prop (ui, path,
				      "accel",
				      accelerator,
				      NULL);
}

void
nautilus_bonobo_set_description (BonoboUIComponent *ui,
			   	 const char *path,
			   	 const char *description)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	bonobo_ui_component_set_prop (ui, path,
				      "descr",
				      description,
				      NULL);
}

void
nautilus_bonobo_set_label (BonoboUIComponent *ui,
			   const char *path,
			   const char *label)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	bonobo_ui_component_set_prop (ui, path,
				      "label",
				      label,
				      NULL);
}

void
nautilus_bonobo_set_tip (BonoboUIComponent *ui,
			 const char *path,
			 const char *tip)
{
	g_return_if_fail (ui != NULL);
	bonobo_ui_component_set_prop (ui, path,
				      "tip",
				      tip,
				      NULL);
}

void
nautilus_bonobo_set_sensitive (BonoboUIComponent *ui,
			       const char *path,
			       gboolean sensitive)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	bonobo_ui_component_set_prop (ui, path,
				      "sensitive",
				      sensitive ? "1" : "0",
				      NULL);
}

void
nautilus_bonobo_set_toggle_state (BonoboUIComponent *ui,
			       	  const char *path,
			       	  gboolean state)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	bonobo_ui_component_set_prop (ui, path,
				      "state",
				      state ? "1" : "0",
				      NULL);
}

void
nautilus_bonobo_set_hidden (BonoboUIComponent *ui,
			    const char *path,
			    gboolean hidden)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	bonobo_ui_component_set_prop (ui, path,
				      "hidden",
				      hidden ? "1" : "0",
				      NULL);
}

char * 
nautilus_bonobo_get_label (BonoboUIComponent *ui,
		           const char *path)
{
	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), FALSE);

	return bonobo_ui_component_get_prop (ui, path, "label", NULL);
}

gboolean 
nautilus_bonobo_get_hidden (BonoboUIComponent *ui,
		            const char *path)
{
	char *value;
	gboolean hidden;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), FALSE);

	value = bonobo_ui_component_get_prop (ui, path, "hidden", NULL);

	if (value == NULL) {
		/* No hidden attribute means not hidden. */
		hidden = FALSE;
	} else {
		/* Anything other than "0" counts as TRUE */
		hidden = strcmp (value, "0") != 0;
	}

	g_free (value);

	return hidden;
}

void
nautilus_bonobo_add_menu_item (BonoboUIComponent *ui, const char *path, const char *label)
{
	char *xml_string, *encoded_label, *name;

	/* Because we are constructing the XML ourselves, we need to
         * encode the label.
	 */
	encoded_label = bonobo_ui_util_encode_str (label);

	/* Labels may contain characters that are illegal in names. So
	 * we create the name by URI-encoding the label.
	 */
	name = gnome_vfs_escape_string (label);
	
	xml_string = g_strdup_printf ("<menuitem name=\"%s\" label=\"%s\" verb=\"verb:%s\"/>\n", 
				      name, encoded_label, name);
	bonobo_ui_component_set (ui, path, xml_string, NULL);

	g_free (encoded_label);
	g_free (name);
	g_free (xml_string);
}

void
nautilus_bonobo_add_submenu (BonoboUIComponent *ui,
			     const char *path,
			     const char *label)
{
	char *xml_string, *encoded_label, *name;

	/* Because we are constructing the XML ourselves, we need to
         * encode the label.
	 */
	encoded_label = bonobo_ui_util_encode_str (label);

	/* Labels may contain characters that are illegal in names. So
	 * we create the name by URI-encoding the label.
	 */
	name = gnome_vfs_escape_string (label);
	
	xml_string = g_strdup_printf ("<submenu name=\"%s\" label=\"%s\"/>\n", 
				      name, encoded_label);
	bonobo_ui_component_set (ui, path, xml_string, NULL);

	g_free (encoded_label);
	g_free (name);
	g_free (xml_string);
}

void
nautilus_bonobo_add_menu_separator (BonoboUIComponent *ui, const char *path)
{
	bonobo_ui_component_set (ui, path, "<separator/>", NULL);
}

char *
nautilus_bonobo_get_menu_item_verb_name (const char *label)
{
	char *verb_name, *escaped_label;
	
	escaped_label = gnome_vfs_escape_string (label);

	verb_name = g_strdup_printf ("verb:%s", escaped_label);

	g_free (escaped_label);
	
	return verb_name;
}

/**
 * nautilus_bonobo_remove_menu_items
 * 
 * Removes all menu items contained in a menu or placeholder
 * 
 * @uih: The BonoboUIHandler for this menu item.
 * @path: The standard bonobo-style path specifier for this menu item.
 */
void
nautilus_bonobo_remove_menu_items (BonoboUIComponent *ui, const char *path)
{
	char *remove_wildcard;
	
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (path != NULL);

	remove_wildcard = g_strdup_printf ("%s/*", path);

	bonobo_ui_component_rm (ui, remove_wildcard, NULL);

	g_free (remove_wildcard);
}

void
nautilus_bonobo_set_icon (BonoboUIComponent *ui,
			  const char        *path,
			  const char        *icon_relative_path)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (path != NULL);
	g_return_if_fail (icon_relative_path != NULL);

	/* We don't do a get_prop here before setting since it just
	 * means more round-trip CORBA calls.
	 */
	bonobo_ui_component_set_prop (ui, path,
				      "pixname",
				      icon_relative_path, NULL);
	bonobo_ui_component_set_prop (ui, path,
				      "pixtype",
				      "filename", NULL);
}
