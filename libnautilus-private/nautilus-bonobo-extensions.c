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

#include <liboaf/oaf-async.h>

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
nautilus_bonobo_add_menu_item (BonoboUIComponent *ui, const char *id, const char *path, const char *label)
{
	char *xml_string, *encoded_label, *verb_name;
	char *name, *full_id;

	/* Because we are constructing the XML ourselves, we need to
         * encode the label.
	 */
	encoded_label = bonobo_ui_util_encode_str (label);

	/* Labels may contain characters that are illegal in names. So
	 * we create the name by URI-encoding the label.
	 */
	full_id = g_strdup_printf ("%s/%s", path, id);
	name = gnome_vfs_escape_string (id);
	verb_name = gnome_vfs_escape_string (id);

	xml_string = g_strdup_printf ("<menuitem name=\"%s\" label=\"%s\" verb=\"verb:%s\"/>\n", 
				      name, encoded_label, verb_name);
	bonobo_ui_component_set (ui, path, xml_string, NULL);

	g_free (encoded_label);
	g_free (name);
	g_free (verb_name);
	g_free (full_id);
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
nautilus_bonobo_get_menu_item_verb_name (const char *path)
{
	char *verb_name, *escaped_path;
	
	escaped_path = gnome_vfs_escape_string (path);

	verb_name = g_strdup_printf ("verb:%s", escaped_path);

	g_free (escaped_path);
	
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

struct _NautilusBonoboActivate {
	NautilusBonoboActivateCallback activation_callback;
	gpointer callback_data;
	gboolean stop_activation;
};

static void
oaf_activation_callback (CORBA_Object object_reference, 
			 const char *error_reason, 
			 gpointer user_data)
{
	NautilusBonoboActivate *activate_struct;
	CORBA_Environment ev;
	
	activate_struct = (NautilusBonoboActivate *) user_data;
	CORBA_exception_init (&ev);
	
	if (CORBA_Object_is_nil (object_reference, &ev)) {
		/* error */
		activate_struct->activation_callback (NULL, 
						      activate_struct->callback_data);
		
	} else if (!activate_struct->stop_activation) {
		
		/* report activation to caller */
		activate_struct->activation_callback (object_reference, 
						      activate_struct->callback_data);
		
	} else if (activate_struct->stop_activation) {
		activate_struct->stop_activation = FALSE;
		
		Bonobo_Unknown_unref (object_reference, &ev);
		/* it is no use to check for exception here since we 
		   have no way of reporting it... */
	}
	CORBA_exception_free (&ev);
}


/**
 * nautilus_bonobo_activate_from_id:
 * @iid: iid of component to activate.
 * @callback: callback to call when activation finished.
 * @user_data: data to pass to callback when activation finished.
 *
 * This function will return NULL if something bad happened during 
 * activation. Alternatively, it will return a structure you are 
 * supposed to free yourself when you have received a call in your
 * callback.
 */
NautilusBonoboActivate *
nautilus_bonobo_activate_from_id (const char *iid, 
				  NautilusBonoboActivateCallback callback, 
				  gpointer user_data)
{
	NautilusBonoboActivate *activate_structure;
	CORBA_Environment ev;

	if (iid == NULL || callback == NULL) {
		return NULL;
	}

	activate_structure = g_new0 (NautilusBonoboActivate, 1);

	activate_structure->stop_activation = FALSE;
	activate_structure->activation_callback = callback;
	activate_structure->callback_data = user_data;

	CORBA_exception_init (&ev);
	oaf_activate_from_id_async ((const OAF_ActivationID) iid, 0, oaf_activation_callback, 
				    activate_structure , &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		return NULL;
	}

	CORBA_exception_free (&ev);

	return activate_structure;
}

/**
 * nautilus_bonobo_activate_from_id:
 * @iid: iid of component to activate.
 * @callback: callback to call when activation finished.
 * @user_data: data to pass to callback when activation finished.
 *
 * Stops activation of a component. Your callback will never be called.
 * you should free your %NautilusBonoboActivate strucutre through
 * nautilus_bonobo_activate_free after this call.
 */

void 
nautilus_bonobo_activate_stop (NautilusBonoboActivate *activate_structure)
{
	activate_structure->stop_activation = TRUE;
}

/**
 * nautilus_bonobo_activate_free: 
 * @activate_structure: structure to free.
 * 
 * Frees the corresponding structure.
 */
void
nautilus_bonobo_activate_free (NautilusBonoboActivate *activate_structure)
{
	g_free (activate_structure);
}
