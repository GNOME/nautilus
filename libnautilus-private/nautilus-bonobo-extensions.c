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

#include <config.h>
#include "nautilus-bonobo-extensions.h"

#include "nautilus-icon-factory.h"

#include <eel/eel-string.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-debug.h>
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-utils.h>

struct NautilusBonoboActivationHandle {
	NautilusBonoboActivationHandle **early_completion_hook;
	NautilusBonoboActivationCallback callback;
	gpointer callback_data;
	Bonobo_Unknown activated_object;
	gboolean cancel;
	guint idle_id;
};

typedef enum {
	NUMBERED_MENU_ITEM_PLAIN,
	NUMBERED_MENU_ITEM_TOGGLE,
	NUMBERED_MENU_ITEM_RADIO
} NumberedMenuItemType;

void
nautilus_bonobo_set_accelerator (BonoboUIComponent *ui,
			   	 const char *path,
			   	 const char *accelerator)
{
	bonobo_ui_component_set_prop (ui, path, "accel", accelerator, NULL);
}

void
nautilus_bonobo_set_label (BonoboUIComponent *ui,
			   const char *path,
			   const char *label)
{
	bonobo_ui_component_set_prop (ui, path, "label", label, NULL);
}

void
nautilus_bonobo_set_tip (BonoboUIComponent *ui,
			 const char *path,
			 const char *tip)
{
	bonobo_ui_component_set_prop (ui, path, "tip", tip, NULL);
}

void
nautilus_bonobo_set_sensitive (BonoboUIComponent *ui,
			       const char *path,
			       gboolean sensitive)
{
	bonobo_ui_component_set_prop (ui, path, "sensitive", sensitive ? "1" : "0", NULL);
}

void
nautilus_bonobo_set_toggle_state (BonoboUIComponent *ui,
			       	  const char *path,
			       	  gboolean state)
{
	bonobo_ui_component_set_prop (ui, path, "state", state ? "1" : "0", NULL);
}

void
nautilus_bonobo_set_hidden (BonoboUIComponent *ui,
			    const char *path,
			    gboolean hidden)
{
	bonobo_ui_component_set_prop (ui, path, "hidden", hidden ? "1" : "0", NULL);
}

char * 
nautilus_bonobo_get_label (BonoboUIComponent *ui,
		           const char *path)
{
	return bonobo_ui_component_get_prop (ui, path, "label", NULL);
}

gboolean 
nautilus_bonobo_get_hidden (BonoboUIComponent *ui,
		            const char *path)
{
	char *value;
	gboolean hidden;
	CORBA_Environment ev;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	CORBA_exception_init (&ev);
	value = bonobo_ui_component_get_prop (ui, path, "hidden", &ev);
	CORBA_exception_free (&ev);

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
get_numbered_menu_item_name (guint index)
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

	item_name = get_numbered_menu_item_name (index);
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

guint
nautilus_bonobo_get_numbered_menu_item_index_from_command (const char *command)
{
	char *path;
	char *index_string;
	int index;
	gboolean got_index;

	path = gnome_vfs_unescape_string (command, NULL);
	index_string = strrchr (path, '/');

	if (index_string == NULL) {
		got_index = FALSE;
	} else {
		got_index = eel_str_to_int (index_string + 1, &index);
	}
	g_free (path);

	g_return_val_if_fail (got_index, 0);

	return index;
}

char *
nautilus_bonobo_get_numbered_menu_item_container_path_from_command (const char *command)
{
	char *path;
	char *index_string;
	char *container_path;

	path = gnome_vfs_unescape_string (command, NULL);
	index_string = strrchr (path, '/');

	container_path = index_string == NULL
		? NULL
		: g_strndup (path, index_string - path);
	g_free (path);

	return container_path;
}

static void
add_numbered_menu_item_internal (BonoboUIComponent *ui,
				 const char *container_path,
				 guint index,
				 const char *label,
				 NumberedMenuItemType type,
				 GdkPixbuf *pixbuf,
			 	 const char *radio_group_name)
{
	char *xml_item, *xml_command; 
	char *command_name;
	char *item_name, *pixbuf_data;
	char *path;

	g_assert (BONOBO_IS_UI_COMPONENT (ui)); 
	g_assert (container_path != NULL);
	g_assert (label != NULL);
	g_assert (type == NUMBERED_MENU_ITEM_PLAIN || pixbuf == NULL);
	g_assert (type == NUMBERED_MENU_ITEM_RADIO || radio_group_name == NULL);
	g_assert (type != NUMBERED_MENU_ITEM_RADIO || radio_group_name != NULL);

	item_name = get_numbered_menu_item_name (index);
	command_name = nautilus_bonobo_get_numbered_menu_item_command 
		(ui, container_path, index);

	switch (type) {
	case NUMBERED_MENU_ITEM_TOGGLE:
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" id=\"%s\" type=\"toggle\"/>\n", 
					    item_name, command_name);
		break;
	case NUMBERED_MENU_ITEM_RADIO:
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" id=\"%s\" type=\"radio\" group=\"%s\"/>\n", 
					    item_name, command_name, radio_group_name);
		break;
	case NUMBERED_MENU_ITEM_PLAIN:
		if (pixbuf != NULL) {
			pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);			
			xml_item = g_strdup_printf ("<menuitem name=\"%s\" verb=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\"/>\n", 
						    item_name, command_name, pixbuf_data);
			g_free (pixbuf_data);
		} else {
			xml_item = g_strdup_printf ("<menuitem name=\"%s\" verb=\"%s\"/>\n", 
						    item_name, command_name);
		}
		break;
	default:
		g_assert_not_reached ();
		xml_item = NULL;	/* keep compiler happy */
	}

	g_free (item_name);
	
	bonobo_ui_component_set (ui, container_path, xml_item, NULL);

	g_free (xml_item);

	path = nautilus_bonobo_get_numbered_menu_item_path (ui, container_path, index);
	nautilus_bonobo_set_label (ui, path, label);
	g_free (path);

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

	add_numbered_menu_item_internal (ui, container_path, index, label,
					 NUMBERED_MENU_ITEM_PLAIN, pixbuf, NULL);
}

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of toggle menu items. Each index
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

	add_numbered_menu_item_internal (ui, container_path, index, label,
					 NUMBERED_MENU_ITEM_TOGGLE, NULL, NULL);
}

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of radio menu items. Each index
 * must be unique (normal use is to call this in a loop, and
 * increment the index for each item).
 */
void
nautilus_bonobo_add_numbered_radio_menu_item (BonoboUIComponent *ui, 
					      const char *container_path, 
					      guint index,
			       		      const char *label,
			       		      const char *radio_group_name)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui)); 
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);

	add_numbered_menu_item_internal (ui, container_path, index, label,
					 NUMBERED_MENU_ITEM_RADIO, NULL, radio_group_name);
}

void
nautilus_bonobo_add_submenu (BonoboUIComponent *ui,
			     const char *path,
			     const char *label,
			     GdkPixbuf *pixbuf)
{
	char *xml_string, *name, *pixbuf_data, *submenu_path;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (path != NULL);
	g_return_if_fail (label != NULL);
	g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

	/* Labels may contain characters that are illegal in names. So
	 * we create the name by URI-encoding the label.
	 */
	name = gnome_vfs_escape_string (label);

	if (pixbuf != NULL) {
		pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);			
		xml_string = g_strdup_printf ("<submenu name=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\"/>\n", 
					      name, pixbuf_data);
		g_free (pixbuf_data);
	} else {
		xml_string = g_strdup_printf ("<submenu name=\"%s\"/>\n", name);
	}

	bonobo_ui_component_set (ui, path, xml_string, NULL);

	g_free (xml_string);

	submenu_path = g_strconcat (path, "/", name, NULL);
	nautilus_bonobo_set_label (ui, submenu_path, label);
	g_free (submenu_path);

	g_free (name);
}

void
nautilus_bonobo_add_menu_separator (BonoboUIComponent *ui, const char *path)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (path != NULL);

	bonobo_ui_component_set (ui, path, "<separator/>", NULL);
}

static void
remove_commands (BonoboUIComponent *ui, const char *container_path)
{
	BonoboUINode *path_node;
	BonoboUINode *child_node;
	char *verb_name;
	char *id_name;

	path_node = bonobo_ui_component_get_tree (ui, container_path, TRUE, NULL);
	if (path_node == NULL) {
		return;
	}

	bonobo_ui_component_freeze (ui, NULL);

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

	bonobo_ui_component_thaw (ui, NULL);

	bonobo_ui_node_free (path_node);
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

/* Call to set the user-visible label of a command to a string
 * containing an underscore accelerator. The underscore is stripped
 * off before setting the label of the toolitem, because toolbar
 * button labels shouldn't have the underscore.
 */
void	 
nautilus_bonobo_set_label_for_toolitem_and_command (BonoboUIComponent *ui,
						    const char	*toolitem_path,
						    const char	*command_path,
						    const char	*label_with_underscore)
{
	char *label_no_underscore;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (toolitem_path != NULL);
	g_return_if_fail (command_path != NULL);
	g_return_if_fail (label_with_underscore != NULL);

	label_no_underscore = eel_str_strip_chr (label_with_underscore, '_');
	nautilus_bonobo_set_label (ui,
				   command_path,
				   label_with_underscore);
	nautilus_bonobo_set_label (ui,
				   toolitem_path,
				   label_no_underscore);
	
	g_free (label_no_underscore);
}

static char *
get_extension_menu_item_xml (NautilusMenuItem *item)
{
	GString *ui_xml;
	char *pixbuf_data;
	GdkPixbuf *pixbuf;
	char *name;
	char *icon;
	
	ui_xml = g_string_new ("");

	g_object_get (G_OBJECT (item), "name", &name, "icon", &icon, NULL);

	g_string_append_printf (ui_xml,
				"<menuitem name=\"%s\" verb=\"%s\"",
				name, name);
	
	if (icon) {
		pixbuf = nautilus_icon_factory_get_pixbuf_from_name 
			(icon,
			 NULL,
			 NAUTILUS_ICON_SIZE_FOR_MENUS,
			 NULL);
		if (pixbuf) {
			pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
			g_string_append_printf (ui_xml, " pixtype=\"pixbuf\" pixname=\"%s\"", pixbuf_data);
			g_free (pixbuf_data);
			g_object_unref (pixbuf);
		}
	}
	g_string_append (ui_xml, "/>");

	g_free (name);
	g_free (icon);

	return g_string_free (ui_xml, FALSE);
}

static void
extension_action_callback (BonoboUIComponent *component,
			   gpointer callback_data, const char *path)
{
	nautilus_menu_item_activate (NAUTILUS_MENU_ITEM (callback_data));
}

char *
nautilus_bonobo_get_extension_item_command_xml (NautilusMenuItem *item)
{
	char *name;
	char *label;
	char *tip;
	gboolean sensitive;
	char *xml;

	g_object_get (G_OBJECT (item), 
		      "name", &name, "label", &label, 
		      "tip", &tip, "sensitive", &sensitive,
		      NULL);

	xml = g_strdup_printf ("<cmd name=\"%s\" label=\"%s\" tip=\"%s\" sensitive=\"%s\"/>", 
			       name, label, tip, sensitive ? "1" : "0");

	g_free (name);
	g_free (label);
	g_free (tip);

	return xml;
}

void
nautilus_bonobo_add_extension_item_command (BonoboUIComponent *ui,
					    NautilusMenuItem *item)
{
	char *xml;
	char *name;
	GClosure *closure;

	xml = nautilus_bonobo_get_extension_item_command_xml (item);

	bonobo_ui_component_set (ui, "/commands", xml, NULL);

	g_free (xml);

	g_object_get (G_OBJECT (item), "name", &name, NULL);

	closure = g_cclosure_new
		(G_CALLBACK (extension_action_callback),
		 g_object_ref (item),
		 (GClosureNotify)g_object_unref);
	
	bonobo_ui_component_add_verb_full (ui, name, closure);

	g_free (name);
}

void 
nautilus_bonobo_add_extension_item (BonoboUIComponent *ui,
				    const char *path,
				    NautilusMenuItem *item)
{
	char *item_xml;

	item_xml = get_extension_menu_item_xml (item);
	
	bonobo_ui_component_set (ui, path, item_xml, NULL);

	g_free (item_xml);
}

static char *
get_extension_toolbar_item_xml (NautilusMenuItem *item)
{
	GString *ui_xml;
	char *pixbuf_data;
	GdkPixbuf *pixbuf;
	char *name;
	char *icon;
	gboolean priority;
	
	ui_xml = g_string_new ("");


	g_object_get (item, 
		      "name", &name, "priority", &priority, 
		      "icon", &icon,
		      NULL);
	g_string_append_printf (ui_xml,
				"<toolitem name=\"%s\" verb=\"%s\"",
				name, name);

	if (priority) {
		g_string_append (ui_xml, " priority=\"1\"");
	}

	if (icon) {
		pixbuf = nautilus_icon_factory_get_pixbuf_from_name 
			(icon,
			 NULL,
			 NAUTILUS_ICON_SIZE_FOR_MENUS,
			 NULL);
		if (pixbuf) {
			pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
			g_string_append_printf (ui_xml, " pixtype=\"pixbuf\" pixname=\"%s\"", pixbuf_data);
			g_free (pixbuf_data);
			g_object_unref (pixbuf);
		}
	}
	g_string_append (ui_xml, "/>");

	g_free (name);
	g_free (icon);

	return g_string_free (ui_xml, FALSE);
}

void 
nautilus_bonobo_add_extension_toolbar_item (BonoboUIComponent *ui,
					    const char *path,
					    NautilusMenuItem *item)
{
	char *item_xml;

	item_xml = get_extension_toolbar_item_xml (item);
	
	bonobo_ui_component_set (ui, path, item_xml, NULL);

	g_free (item_xml);
}

static void
activation_handle_done (NautilusBonoboActivationHandle *handle)
{
	if (handle->early_completion_hook != NULL) {
		g_assert (*handle->early_completion_hook == handle);
		*handle->early_completion_hook = NULL;
	}
}

static void
activation_handle_free (NautilusBonoboActivationHandle *handle)
{
	activation_handle_done (handle);
	
	if (handle->activated_object != NULL) {
		bonobo_object_release_unref (handle->activated_object, NULL);
	}
	
	g_free (handle);
}

static GHashTable *nautilus_activation_shortcuts = NULL;

struct CreateObjectData {
	NautilusBonoboCreateObject create_object;
	gpointer callback_data;
};

void
nautilus_bonobo_register_activation_shortcut (const char                       *iid,
					      NautilusBonoboCreateObject        create_object_callback,
					      gpointer                          callback_data)
{
	struct CreateObjectData *data;
	
	if (nautilus_activation_shortcuts == NULL) {
		nautilus_activation_shortcuts = g_hash_table_new_full
			(g_str_hash, g_str_equal, g_free, g_free);
		eel_debug_call_at_shutdown_with_data ((GFreeFunc)g_hash_table_destroy,
						      nautilus_activation_shortcuts);
	}

	data = g_new (struct CreateObjectData, 1);
	data->create_object = create_object_callback;
	data->callback_data = callback_data;
	g_hash_table_insert (nautilus_activation_shortcuts,
			     g_strdup (iid), data);
}

void
nautilus_bonobo_unregister_activation_shortcut (const char *iid)
{
	if (nautilus_activation_shortcuts == NULL) {
		g_assert_not_reached ();
		return;
	}
	g_hash_table_remove (nautilus_activation_shortcuts, iid);
}

static gboolean
activation_idle_callback (gpointer callback_data)
{
	NautilusBonoboActivationHandle *handle;
	
	handle = (NautilusBonoboActivationHandle *) callback_data;

	(* handle->callback) (handle,
			      handle->activated_object,
			      handle->callback_data);

	activation_handle_free (handle);

	return FALSE;
}

static void
activation_cancel (NautilusBonoboActivationHandle *handle)
{
	activation_handle_free (handle);
}

static void
bonobo_activation_activation_callback (Bonobo_Unknown activated_object, 
				       const char *error_reason, 
				       gpointer callback_data)
{
	NautilusBonoboActivationHandle *handle;
	
	handle = (NautilusBonoboActivationHandle *) callback_data;

	if (activated_object == NULL) {
		g_warning ("activation failed: %s", error_reason);
	}

	handle->activated_object = activated_object;

	if (handle->cancel) {
		activation_cancel (handle);
	} else {
		handle->idle_id = g_idle_add (activation_idle_callback,
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
	struct CreateObjectData *data;
	CORBA_Object activated_object;
	
	g_return_val_if_fail (iid != NULL, NULL);
	g_return_val_if_fail (callback != NULL, NULL);

	handle = g_new0 (NautilusBonoboActivationHandle, 1);

	handle->early_completion_hook = &handle;
	handle->callback = callback;
	handle->callback_data = callback_data;

	handle->activated_object = CORBA_OBJECT_NIL;
	
	if (nautilus_activation_shortcuts != NULL) {
		data = g_hash_table_lookup (nautilus_activation_shortcuts, iid);
		if (data != NULL) {
			activated_object = (*data->create_object) (iid, data->callback_data);
			if (activated_object != CORBA_OBJECT_NIL) {
				handle->activated_object = activated_object;
				handle->early_completion_hook = NULL;
				handle->idle_id = g_idle_add (activation_idle_callback,
							      handle);
				return handle;
			}
		}
	}


	bonobo_activation_activate_from_id_async ((char *) iid, 0,
				    bonobo_activation_activation_callback, 
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
		/* no way to cancel the underlying bonobo-activation call, so we just set a flag */
		handle->cancel = TRUE;
	} else {
		g_source_remove (handle->idle_id);
		activation_cancel (handle);
	}
}

Bonobo_RegistrationResult
nautilus_bonobo_activation_register_for_display (const char    *iid,
						 Bonobo_Unknown ref)
{
	const char *display_name;
	GSList *reg_env ;
	Bonobo_RegistrationResult result;
	
	display_name = gdk_display_get_name (gdk_display_get_default());
	reg_env = bonobo_activation_registration_env_set (NULL,
							  "DISPLAY", display_name);
	result = bonobo_activation_register_active_server (iid, ref, reg_env);
	bonobo_activation_registration_env_free (reg_env);
	return result;
}
