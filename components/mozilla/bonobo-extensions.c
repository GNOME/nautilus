/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bonobo-extensions.c - implementation of new functions that conceptually
                                  belong in bonobo. Perhaps some of these will be
                                  actually rolled into bonobo someday.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
            Darin Adler <darin@bentspoon.com>
*/

/* FIXME: This file copied intactly from libnautilus-private
 * to circuvent licensing issue with Mozilla component.  In
 * future, when nautilus-extensions is split into public libraries,
 * we can lose this file and use the extensions directly
 */

#include <config.h>
#include "bonobo-extensions.h"

#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <liboaf/oaf-async.h>

struct NautilusBonoboActivationHandle {
	NautilusBonoboActivationHandle **early_completion_hook;
	NautilusBonoboActivationCallback callback;
	gpointer callback_data;
	Bonobo_Unknown activated_object;
	gboolean cancel;
	guint idle_id;
};

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

static char *
get_numbered_menu_item_name (BonoboUIComponent *ui,
			      const char *container_path,
			      guint index)
{
	return g_strdup_printf ("%u", index);
}			      

char *
nautilus_bonobo_get_numbered_menu_item_path (BonoboUIComponent *ui,
					      const char *container_path, 
					      guint index)
{
	char *item_name;
	char *item_path;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), NULL); 
	g_return_val_if_fail (container_path != NULL, NULL);

	item_name = get_numbered_menu_item_name (ui, container_path, index);
	item_path = g_strconcat (container_path, "/", item_name, NULL);
	g_free (item_name);

	return item_path;
}

char *
nautilus_bonobo_get_numbered_menu_item_command (BonoboUIComponent *ui,
						 const char *container_path, 
						 guint index)
{
	char *command_name;
	char *path;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), NULL); 
	g_return_val_if_fail (container_path != NULL, NULL);

	path = nautilus_bonobo_get_numbered_menu_item_path (ui, container_path, index);
	command_name = gnome_vfs_escape_string (path);
	g_free (path);
	
	return command_name;
}

static void
add_numbered_menu_item_internal (BonoboUIComponent *ui,
			 	  const char *container_path,
			 	  guint index,
			 	  const char *label,
			 	  gboolean is_toggle,
			 	  GdkPixbuf *pixbuf)
{
	char *xml_item, *xml_command; 
	char *encoded_label, *command_name;
	char *item_name, *pixbuf_data;

	g_assert (BONOBO_IS_UI_COMPONENT (ui)); 
	g_assert (container_path != NULL);
	g_assert (label != NULL);
	g_assert (!is_toggle || pixbuf == NULL);

	/* Because we are constructing the XML ourselves, we need to
         * encode the label.
	 */
	encoded_label = bonobo_ui_util_encode_str (label);

	item_name = get_numbered_menu_item_name 
		(ui, container_path, index);
	command_name = nautilus_bonobo_get_numbered_menu_item_command 
		(ui, container_path, index);

	/* Note: we ignore the pixbuf for toggle items. This could be changed
	 * if we ever want a toggle item that also has a pixbuf.
	 */
	if (is_toggle) {
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" label=\"%s\" id=\"%s\" type=\"toggle\"/>\n", 
						item_name, encoded_label, command_name);
	} else if (pixbuf != NULL) {
		/* Encode pixbuf type and data into XML string */			
		pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
		
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" label=\"%s\" verb=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\"/>\n", 
						item_name, encoded_label, command_name, pixbuf_data);	
		g_free (pixbuf_data);
	} else {
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" label=\"%s\" verb=\"%s\"/>\n", 
						item_name, encoded_label, command_name);
	}
	g_free (encoded_label);
	g_free (item_name);
	
	bonobo_ui_component_set (ui, container_path, xml_item, NULL);
	g_free (xml_item);

	/* Make the command node here too, so callers can immediately set
	 * properties on it (otherwise it doesn't get created until some
	 * time later).
	 */
	xml_command = g_strdup_printf ("<cmd name=\"%s\"/>\n", command_name);
	bonobo_ui_component_set (ui, "/commands", xml_command, NULL);
	g_free (xml_command);

	g_free (command_name);
}			 

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of menu items. Each index
 * must be unique (normal use is to call this in a loop, and
 * increment the index for each item).
 */
void
nautilus_bonobo_add_numbered_menu_item (BonoboUIComponent *ui, 
					 const char *container_path, 
					 guint index,
			       		 const char *label, 
			       		 GdkPixbuf *pixbuf)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui)); 
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);

	add_numbered_menu_item_internal (ui, container_path, index, label, FALSE, pixbuf);
}

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of menu items. Each index
 * must be unique (normal use is to call this in a loop, and
 * increment the index for each item).
 */
void
nautilus_bonobo_add_numbered_toggle_menu_item (BonoboUIComponent *ui, 
					        const char *container_path, 
					        guint index,
			       		        const char *label)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui)); 
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);

	add_numbered_menu_item_internal (ui, container_path, index, label, TRUE, NULL);
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

static void
remove_commands (BonoboUIComponent *ui, const char *container_path)
{
	BonoboUINode *path_node;
	BonoboUINode *child_node;
	char *verb_name;
	char *id_name;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);

	bonobo_ui_component_freeze (ui, NULL);

	path_node = bonobo_ui_component_get_tree (ui, container_path, TRUE, NULL);

	if (path_node != NULL) {
		for (child_node = bonobo_ui_node_children (path_node);
		     child_node != NULL;
		     child_node = bonobo_ui_node_next (child_node)) {
			verb_name = bonobo_ui_node_get_attr (child_node, "verb");
			if (verb_name != NULL) {
				bonobo_ui_component_remove_verb (ui, verb_name);
				bonobo_ui_node_free_string (verb_name);
			} else {
				/* Only look for an id if there's no verb */
				id_name = bonobo_ui_node_get_attr (child_node, "id");
				if (id_name != NULL) {
					bonobo_ui_component_remove_listener (ui, id_name);
					bonobo_ui_node_free_string (id_name);
				}
			}

		}
	}

	bonobo_ui_node_free (path_node);
	bonobo_ui_component_thaw (ui, NULL);
}

/**
 * nautilus_bonobo_remove_menu_items_and_verbs
 * 
 * Removes all menu items contained in a menu or placeholder, and
 * their verbs.
 * 
 * @uih: The BonoboUIHandler for this menu item.
 * @container_path: The standard bonobo-style path specifier for this placeholder or submenu.
 */
void
nautilus_bonobo_remove_menu_items_and_commands (BonoboUIComponent *ui, 
					        const char *container_path)
{
	char *remove_wildcard;
	
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);

	remove_commands (ui, container_path);

	/* For speed, remove menu items themselves all in one fell swoop,
	 * though we removed the verbs one-by-one.
	 */
	remove_wildcard = g_strdup_printf ("%s/*", container_path);
	bonobo_ui_component_rm (ui, remove_wildcard, NULL);
	g_free (remove_wildcard);
}

static char *
eel_str_strip_chr (const char *source, char remove_this)
{
	char *result, *out;
	const char *in;
	
        if (source == NULL) {
		return NULL;
	}
	
	result = g_new (char, strlen (source) + 1);
	in = source;
	out = result;
	do {
		if (*in != remove_this) {
			*out++ = *in;
		}
	} while (*in++ != '\0');

        return result;
}

/* Call to set the user-visible label of a menu item to a string
 * containing an underscore accelerator. The underscore is stripped
 * off before setting the label of the command, because pop-up menu
 * and toolbar button labels shouldn't have the underscore.
 */
void	 
nautilus_bonobo_set_label_for_menu_item_and_command (BonoboUIComponent *ui,
						     const char	*menu_item_path,
						     const char	*command_path,
						     const char	*label_with_underscore)
{
	char *label_no_underscore;

	label_no_underscore = eel_str_strip_chr (label_with_underscore, '_');
	nautilus_bonobo_set_label (ui,
				   menu_item_path,
				   label_with_underscore);
	nautilus_bonobo_set_label (ui,
				   command_path,
				   label_no_underscore);
	
	g_free (label_no_underscore);
}

void
nautilus_bonobo_set_icon (BonoboUIComponent *ui,
			  const char *path,
			  const char *icon_relative_path)
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

static void
activation_handle_done (NautilusBonoboActivationHandle *handle)
{
	if (handle->early_completion_hook != NULL) {
		g_assert (*handle->early_completion_hook == handle);
		*handle->early_completion_hook = NULL;
	}
}

static gboolean
activation_idle_callback (gpointer callback_data)
{
	NautilusBonoboActivationHandle *handle;
	
	handle = (NautilusBonoboActivationHandle *) callback_data;

	(* handle->callback) (handle,
			      handle->activated_object,
			      handle->callback_data);

	activation_handle_done (handle);
	g_free (handle);

	return FALSE;
}

static void
activation_cancel (NautilusBonoboActivationHandle *handle)
{
	bonobo_object_release_unref (handle->activated_object, NULL);

	activation_handle_done (handle);
	g_free (handle);
}

static void
oaf_activation_callback (Bonobo_Unknown activated_object, 
			 const char *error_reason, 
			 gpointer callback_data)
{
	NautilusBonoboActivationHandle *handle;
	
	handle = (NautilusBonoboActivationHandle *) callback_data;

	handle->activated_object = activated_object;

	if (handle->cancel) {
		activation_cancel (handle);
	} else {
		handle->idle_id = gtk_idle_add (activation_idle_callback,
						handle);
	}
}

/**
 * nautilus_bonobo_activate_from_id:
 * @iid: iid of component to activate.
 * @callback: callback to call when activation finished.
 * @user_data: data to pass to callback when activation finished.
 *
 * This function will return NULL if something bad happened during 
 * activation.
 */
NautilusBonoboActivationHandle *
nautilus_bonobo_activate_from_id (const char *iid,
				  NautilusBonoboActivationCallback callback,
				  gpointer callback_data)
{
	NautilusBonoboActivationHandle *handle;

	g_return_val_if_fail (iid != NULL, NULL);
	g_return_val_if_fail (callback != NULL, NULL);

	handle = g_new0 (NautilusBonoboActivationHandle, 1);

	handle->early_completion_hook = &handle;
	handle->callback = callback;
	handle->callback_data = callback_data;

	oaf_activate_from_id_async ((char *) iid, 0,
				    oaf_activation_callback, 
				    handle, NULL);

	if (handle != NULL) {
		handle->early_completion_hook = NULL;
	}

	return handle;
}

/**
 * nautilus_bonobo_activate_stop:
 * @iid: iid of component to activate.
 * @callback: callback to call when activation finished.
 * @user_data: data to pass to callback when activation finished.
 *
 * Stops activation of a component. Your callback will not be called
 * after this call.
 */
void 
nautilus_bonobo_activate_cancel (NautilusBonoboActivationHandle *handle)
{
	if (handle == NULL) {
		return;
	}

	activation_handle_done (handle);

	if (handle->idle_id == 0) {
		/* no way to cancel the OAF part, so we just set a flag */
		handle->cancel = TRUE;
	} else {
		gtk_idle_remove (handle->idle_id);
		activation_cancel (handle);
	}
}
