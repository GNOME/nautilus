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

void
nautilus_bonobo_set_accelerator (BonoboUIComponent *ui,
			   	 const char *path,
			   	 const char *accelerator)
{
	g_assert (ui != NULL);
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
	g_assert (ui != NULL);
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
	g_assert (ui != NULL);
	bonobo_ui_component_set_prop (ui, path,
				      "label",
				      label,
				      NULL);
}

void
nautilus_bonobo_set_sensitive (BonoboUIComponent *ui,
			       const char *path,
			       gboolean sensitive)
{
	g_assert (ui != NULL);
	bonobo_ui_component_set_prop (ui, path,
				      "sensitive",
				      sensitive ? "1" : "0",
				      NULL);
}

void
nautilus_bonobo_set_hidden (BonoboUIComponent *ui,
			    const char *path,
			    gboolean hidden)
{
	g_assert (ui != NULL);
	bonobo_ui_component_set_prop (ui, path,
				      "hidden",
				      hidden ? "1" : "0",
				      NULL);
}

gboolean 
nautilus_bonobo_get_hidden (BonoboUIComponent *ui,
		            const char        *path)
{
	char *value;

	g_assert (ui != NULL);

	value = bonobo_ui_component_get_prop (ui, path,
					      "hidden",
					      NULL);
	if (value == NULL) {
		return TRUE;
	}

	if (strcmp (value, "1") == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

void
nautilus_bonobo_add_menu_item (BonoboUIComponent *ui, const char *path, const char *label)
{
	CORBA_Environment  ev;
	char *xml_string;
		
	CORBA_exception_init (&ev);

	xml_string = g_strdup_printf ("<menuitem name=\"name:%s\" label=\"%s\" verb=\"verb:%s\"/>\n", label, label, label);
	bonobo_ui_component_set (ui, path, xml_string, &ev);

	g_free (xml_string);
	CORBA_exception_free (&ev);
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
	
	remove_wildcard = g_strdup_printf ("%s/*", path);

	bonobo_ui_component_rm (ui, remove_wildcard, NULL);

	g_free (remove_wildcard);
}

void
nautilus_bonobo_set_icon (BonoboUIComponent *ui,
			  const char        *path,
			  const char        *icon_relative_path)
{
	char *current_icon;
	char *pixtype;

	g_assert (ui != NULL);
	g_return_if_fail (icon_relative_path != NULL);
	g_return_if_fail (path != NULL);

	current_icon = bonobo_ui_component_get_prop (ui, path,
						     "pixname", NULL);
	if (current_icon == NULL
	    || strcmp (current_icon, icon_relative_path) != 0) {
		/*g_print ("setting %s to %s\n", path, icon_relative_path);*/
		bonobo_ui_component_set_prop (ui, path,
					      "pixname",
					      icon_relative_path, NULL);
	}

	g_free (current_icon);


	pixtype = bonobo_ui_component_get_prop (ui, path,
						"pixtype", NULL);
	if (pixtype == NULL
	    || strcmp (pixtype, "filename") != 0) {
		bonobo_ui_component_set_prop (ui, path,
					      "pixtype",
					      "filename", NULL);
	}

	g_free (pixtype);

}

#ifdef UIH

/**
 * nautilus_bonobo_ui_handler_menu_toggle_appearance
 * 
 * Changes a toggleable bonobo menu item's apparent state
 * without invoking its callback.
 * 
 * @uih: The BonoboUIHandler for this menu item.
 * @path: The standard bonobo-style path specifier for this menu item.
 * @new_value: TRUE if item should appear checked (on), FALSE otherwise.
 */
void 
nautilus_bonobo_ui_handler_menu_set_toggle_appearance (BonoboUIHandler *uih,
				      	   		    const char *path,
				      	   		    gboolean new_value)
{
	BonoboUIHandlerCallback saved_callback;
	gpointer saved_callback_data;
	GDestroyNotify saved_destroy_notify;

	/* Temporarily clear out callback and data so when we
	 * set the toggle state the callback isn't called. 
	 */
	bonobo_ui_handler_menu_get_callback (uih, path, &saved_callback,
					     &saved_callback_data, &saved_destroy_notify);
	bonobo_ui_handler_menu_remove_callback_no_notify (uih, path);
        bonobo_ui_handler_menu_set_toggle_state (uih, path, new_value);
	bonobo_ui_handler_menu_set_callback (uih, path, saved_callback,
					     saved_callback_data, saved_destroy_notify);		
}

#endif /* UIH */
