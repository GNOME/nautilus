/*
 *  fm-bonobo-provider.h - Bonobo API support
 * 
 *  Copyright (C) 2002 James Willcox
 *                2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: James Willcox <james@gnome.org>
 *           Dave Camp <dave@ximian.com>
 *
 */

/* This object exports the bonobo context menus and property pages
 * using the new extension interface. */ 

#include <config.h>
#include "fm-bonobo-provider.h"

#include <string.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-zoomable.h>
#include <eel/eel-glib-extensions.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-file-info.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-view-identifier.h>

typedef struct {
	char *id;
	char *verb;
	CORBA_sequence_CORBA_string *uri_list;
} BonoboMimeActionData;


typedef struct {
	GList *files;
	GtkWidget *vbox;
	char *view_name;
} ActivationData;

static void fm_bonobo_provider_instance_init            (FMBonoboProvider               *provider);
static void fm_bonobo_provider_class_init               (FMBonoboProviderClass          *class);

static GObjectClass *parent_class;

static BonoboMimeActionData *
bonobo_mime_action_data_new (const char *id, const char *verb, GList *files)
{
	BonoboMimeActionData *data;
	CORBA_sequence_CORBA_string *uri_list;
	int i;

	data = g_new (BonoboMimeActionData, 1);
	data->id = g_strdup (id);
	data->verb = g_strdup (verb);

	/* convert the GList of files into a CORBA sequence */

	uri_list = CORBA_sequence_CORBA_string__alloc ();
	uri_list->_maximum = g_list_length (files);
	uri_list->_length = uri_list->_maximum;
	uri_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (uri_list->_length);

	for (i=0; files; files = files->next, i++)
	{
		NautilusFile *file;
		char *uri;

		file = files->data;
		uri = nautilus_file_get_uri (file);

		uri_list->_buffer[i] = CORBA_string_dup ((char*)uri);

		g_free (uri);
	}

	CORBA_sequence_set_release (uri_list, CORBA_TRUE);
	data->uri_list = uri_list;


	return data;
}

static void
bonobo_mime_action_data_free (BonoboMimeActionData *data)
{
	g_free (data->id);
	g_free (data->verb);
	g_free (data);
}

static void
bonobo_mime_action_activate_callback (CORBA_Object obj,
				      const char *error_reason,
				      gpointer user_data)
{
	Bonobo_Listener listener;
	CORBA_Environment ev;
	BonoboMimeActionData *data;
	CORBA_any any;

	data = user_data;

	if (obj == CORBA_OBJECT_NIL) {
		GtkWidget *dialog;

		/* FIXME: make an error message that is not so lame */
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not complete specified action:  %s"), error_reason);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
		return;
	}

	CORBA_exception_init (&ev);

	listener = Bonobo_Unknown_queryInterface (obj,
						  "IDL:Bonobo/Listener:1.0",
						  &ev);

	if (!BONOBO_EX (&ev)) {
		any._type = TC_CORBA_sequence_CORBA_string;
		any._value = data->uri_list;
		Bonobo_Listener_event (listener, data->verb, &any, &ev);
		bonobo_object_release_unref (listener, &ev);
	} else {
		GtkWidget *dialog;

		/* FIXME: make an error message that is not so lame */
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("Could not complete specified action."));
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
	}

}

static void
bonobo_mime_action_callback (NautilusMenuItem *item,
			     gpointer callback_data)
{
	BonoboMimeActionData *data;

	data = callback_data;
	
	bonobo_activation_activate_from_id_async (data->id, 0,
				bonobo_mime_action_activate_callback,
				data, NULL);
	
}

static void
bonobo_mime_action_menu_data_destroy_callback (gpointer data, GClosure *closure)
{
	bonobo_mime_action_data_free ((BonoboMimeActionData *)data);
}

static gboolean
no_locale_at_end (const char *str)
{
	int len;

	len = strlen (str);
	if (len > 3 &&
	    str[len-3] == '-' &&
	    g_ascii_isalpha (str[len-2]) &&
	    g_ascii_isalpha (str[len-1])) {
		return FALSE;
	}
	if (len > 6 &&
	    str[len-6] == '-' &&
	    g_ascii_isalpha (str[len-5]) &&
	    g_ascii_isalpha (str[len-4]) &&
	    str[len-3] == '_' &&
	    g_ascii_isalpha (str[len-2]) &&
	    g_ascii_isalpha (str[len-1])) {
		return FALSE;
	}
	return TRUE;
}

static GList *
get_bonobo_menu_verb_names (Bonobo_ServerInfo *info)
{
	GList *l;
	unsigned int i;
	int offset;

	offset = strlen ("nautilusverb:");

	l = NULL;
	for (i = 0; i < info->props._length; i++) {

		/* look for properties that start with "nautilusverb:".  The
		 * part following the colon is the verb name
		 */
		if (strstr (info->props._buffer[i].name, "nautilusverb:") &&
		    no_locale_at_end (info->props._buffer[i].name)) {
			l = g_list_prepend (l,
			      g_strdup (&info->props._buffer[i].name[offset]));	
		}
	}

	return l;
}

static gboolean
can_handle_multiple_files (Bonobo_ServerInfo *info)
{
	Bonobo_ActivationProperty *prop;

	prop = bonobo_server_info_prop_find (info, "nautilus:can_handle_multiple_files");

	if (prop == NULL || prop->v._d != Bonobo_ACTIVATION_P_BOOLEAN) {
		return FALSE;
	} else {
		return prop->v._u.value_boolean;
	}
}

static GList *
get_menu_items_for_server (Bonobo_ServerInfo *info,
			   GList *verb_names,
			   GList *files)
{
	GList *items;
	GList *l;
	const GList *langs;
	GSList *langs_cpy;
	
	langs = gnome_i18n_get_language_list ("LANG");
	langs_cpy = NULL;
	/* copy it to a singly linked list since bonobo wants that...sigh */
	for (; langs; langs = langs->next) {
		langs_cpy = g_slist_append (langs_cpy, langs->data);
	}

	items = NULL;	
	
	/* build the commands */
	for (l = verb_names; l; l = l->next) {
		NautilusMenuItem *item;
		BonoboMimeActionData *data;
		char *verb;
		char *prop_name;
		const char *label;
		const char *icon_name;
		GClosure *closure;		
		
		verb = l->data;

		prop_name = g_strdup_printf ("nautilusverb:%s", verb);
		label = bonobo_server_info_prop_lookup (info, prop_name,
							langs_cpy);
		g_free (prop_name);

		prop_name = g_strdup_printf ("nautilusverbicon:%s", verb);
		icon_name = bonobo_server_info_prop_lookup (info, prop_name,
							    langs_cpy);
		g_free (prop_name);
		if (!icon_name) {
                        icon_name = bonobo_server_info_prop_lookup (info, "nautilus:icon",
                                                                    langs_cpy);
                }
		
		data = bonobo_mime_action_data_new (info->iid,
						    verb, files);
		closure = g_cclosure_new
			(G_CALLBACK (bonobo_mime_action_callback),
			 data,
			 bonobo_mime_action_menu_data_destroy_callback);
		
		item = nautilus_menu_item_new (verb,
					       label,
					       "", /* no tip for bonobo items */
					       icon_name);
		g_signal_connect_data (item, "activate",
				       G_CALLBACK (bonobo_mime_action_callback),
				       data,
				       bonobo_mime_action_menu_data_destroy_callback,
				       0);
		items = g_list_prepend (items, item);
	}
	items = g_list_reverse (items);

	g_slist_free (langs_cpy);

	/* if it doesn't handle multiple files, disable the menu items */
	if ((g_list_length (files) > 1) &&
	    (can_handle_multiple_files (info) == FALSE)) {
		GList *l;

		for (l = items; l != NULL; l = l->next) {
			g_object_set (l->data, "sensitive", FALSE, NULL);
		}
	}

	return items;
}

static GList *
fm_bonobo_provider_get_file_items (NautilusMenuProvider *provider,
				   GtkWidget *window,
				   GList *selection)
{
	GList *components;
	GList *items;
	GList *l;
	
	components = nautilus_mime_get_popup_components_for_files (selection);

	items = NULL;
	for (l = components; l; l = l->next) {
		Bonobo_ServerInfo *info;
		GList *verb_names;

		info = l->data;
		verb_names = get_bonobo_menu_verb_names (info);
		items = g_list_concat (items,
				       get_menu_items_for_server (info, verb_names, selection));
		eel_g_list_free_deep (verb_names);
		
	}

	if (components != NULL) {
		gnome_vfs_mime_component_list_free (components);
	}

	return items;
}

static void 
fm_bonobo_provider_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
	iface->get_file_items = fm_bonobo_provider_get_file_items;
}

static GtkWidget *
bonobo_page_error_message (const char *view_name,
			   const char *msg)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *image;

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR,
					  GTK_ICON_SIZE_DIALOG);

	msg = g_strdup_printf ("There was an error while trying to create the view named `%s':  %s", view_name, msg);
	label = gtk_label_new (msg);

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);

	return hbox;
}

static CORBA_sequence_CORBA_string *
get_uri_list (GList *file_list)
{
	CORBA_sequence_CORBA_string *uri_list;
	GList *l;
	int i;
	
	uri_list = CORBA_sequence_CORBA_string__alloc ();
	uri_list->_maximum = g_list_length (file_list);
	uri_list->_length = uri_list->_maximum;
	uri_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (uri_list->_length);
	for (i = 0, l = file_list; l != NULL; i++, l = l->next) {
		char *uri;
		
		uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));
		uri_list->_buffer[i] = CORBA_string_dup (uri);
		g_free (uri);
	}	

	return uri_list;
}

static void
bonobo_page_activate_callback (CORBA_Object obj,
			       const char *error_reason,
			       gpointer user_data)
{
	ActivationData *data;
	GtkWidget *widget;
	CORBA_Environment ev;

	data = (ActivationData *)user_data;

	CORBA_exception_init (&ev);
	widget = NULL;

	if (obj != CORBA_OBJECT_NIL) {
		Bonobo_Control control;
		Bonobo_PropertyBag pb;
		GList *keys;

		control = Bonobo_Unknown_queryInterface
				(obj, "IDL:Bonobo/Control:1.0", &ev);

		pb = Bonobo_Control_getProperties (control, &ev);

		if (!BONOBO_EX (&ev)) {
			gboolean new_property;

			keys = bonobo_pbclient_get_keys (pb, NULL);
			new_property = FALSE;
			if (g_list_find_custom (keys, "uris", (GCompareFunc)strcmp)) {
				new_property = TRUE;
			}
			bonobo_pbclient_free_keys (keys);

			if (new_property) {
				/* Set the 'uris' property. */
				CORBA_sequence_CORBA_string *uri_list;
				BonoboArg *arg;
				
				uri_list = get_uri_list (data->files);
				arg = bonobo_arg_new (TC_CORBA_sequence_CORBA_string);
				arg->_value = uri_list;
				bonobo_pbclient_set_value_async (pb, "uris", arg, &ev);
				bonobo_arg_release (arg);
			} else {
				/* Set the 'URI' property. */
				BonoboArg *arg;
				char *uri;

				if (data->files->next != NULL) {
					g_warning ("Multifile property page does not support the 'uris' property");
					bonobo_object_release_unref (pb, NULL);
					bonobo_object_release_unref (control, NULL);
					return;
				}

				uri = nautilus_file_info_get_uri (NAUTILUS_FILE_INFO (data->files->data));

				arg = bonobo_arg_new (BONOBO_ARG_STRING);
				BONOBO_ARG_SET_STRING (arg, uri);
				bonobo_pbclient_set_value_async (pb, "URI", arg, &ev);
				bonobo_arg_release (arg);
				g_free (uri);
			}

			bonobo_object_release_unref (pb, NULL);

			if (!BONOBO_EX (&ev)) {
				widget = bonobo_widget_new_control_from_objref
					(control, CORBA_OBJECT_NIL);
				bonobo_object_release_unref (control, &ev);
			}
		}
	}

	if (widget == NULL) {
		widget = bonobo_page_error_message (data->view_name,
						    error_reason);
	}

	gtk_container_add (GTK_CONTAINER (data->vbox), widget);
	gtk_widget_show (widget);

	g_free (data->view_name);
	g_free (data);
}

static GList *
fm_bonobo_provider_get_pages (NautilusPropertyPageProvider *provider,
			      GList *files)
{
	GList *all_components, *l;
	GList *components;
	CORBA_Environment ev;
	GList *pages;

	/* find all the property pages for this file */
	all_components = nautilus_mime_get_property_components_for_files 
		(files);
	
	/* filter out property pages that don't support multiple files */
	if (files->next) {
		components = NULL;
		for (l = all_components; l != NULL; l = l->next) {
			if (can_handle_multiple_files (l->data)) {
				components = g_list_prepend (components, 
							     l->data);
			}
		}
		components = g_list_reverse (components);
		g_list_free (all_components);
	} else {
		components = all_components;
	}

	CORBA_exception_init (&ev);

	pages = NULL;
	
	l = components;
	while (l != NULL) {
		NautilusViewIdentifier *view_id;
		Bonobo_ServerInfo *server;
		ActivationData *data;
		GtkWidget *vbox;
		NautilusPropertyPage *page;

		server = l->data;
		l = l->next;

		view_id = nautilus_view_identifier_new_from_property_page (server);
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 
						GNOME_PAD);
		gtk_widget_show (vbox);

		page = nautilus_property_page_new (view_id->iid,
						   gtk_label_new (view_id->name),
						   vbox);
		
		pages = g_list_prepend (pages, page);

		data = g_new (ActivationData, 1);
		data->files = nautilus_file_info_list_copy (files);
		data->vbox = vbox;
		data->view_name = g_strdup (view_id->name);

		bonobo_activation_activate_from_id_async
			(view_id->iid,
			 0, bonobo_page_activate_callback,
			 data, &ev);
	}

	return g_list_reverse (pages);
}

static void 
fm_bonobo_provider_property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface)
{
	iface->get_pages = fm_bonobo_provider_get_pages;
}

static void 
fm_bonobo_provider_instance_init (FMBonoboProvider *provider)
{
}

static void
fm_bonobo_provider_class_init (FMBonoboProviderClass *class)
{
	parent_class = g_type_class_peek_parent (class);
}

GType
fm_bonobo_provider_get_type (void) 
{
	static GType provider_type = 0;

	if (!provider_type) {
		static const GTypeInfo type_info = {
			sizeof (FMBonoboProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) fm_bonobo_provider_class_init,
			NULL, 
			NULL,
			sizeof (FMBonoboProvider),
			0,
			(GInstanceInitFunc) fm_bonobo_provider_instance_init,
		};

		static const GInterfaceInfo menu_provider_iface_info = {
			(GInterfaceInitFunc) fm_bonobo_provider_menu_provider_iface_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo property_page_provider_iface_info = {
			(GInterfaceInitFunc) fm_bonobo_provider_property_page_provider_iface_init,
			NULL,
			NULL
		};
		
		provider_type = g_type_register_static (G_TYPE_OBJECT,
							"FMBonoboProvider",
							&type_info, 0);

		g_type_add_interface_static (provider_type,
					     NAUTILUS_TYPE_MENU_PROVIDER,
					     &menu_provider_iface_info);
		g_type_add_interface_static (provider_type,
					     NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
					     &property_page_provider_iface_info);
	}

	return provider_type;
}

