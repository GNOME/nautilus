 /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-icons-controller.c: Controller that puts NautilusFile together
   with the GnomeIconContainer.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "fm-icons-controller.h"

#include <libnautilus/nautilus-directory.h>
#include <libnautilus/nautilus-string.h>
#include <libnautilus/nautilus-gtk-macros.h>

static void                  fm_icons_controller_initialize_class  (FMIconsControllerClass   *klass);
static void                  fm_icons_controller_initialize        (FMIconsController        *controller);
static NautilusScalableIcon *fm_icons_controller_get_icon_image    (NautilusIconsController  *controller,
								    NautilusControllerIcon   *icon,
								    GList                   **emblem_icons);
static char *                fm_icons_controller_get_icon_property (NautilusIconsController  *controller,
								    NautilusControllerIcon   *icon,
								    const char               *property_name);
static char *                fm_icons_controller_get_icon_text     (NautilusIconsController  *controller,
								    NautilusControllerIcon   *icon);
static char *                fm_icons_controller_get_icon_uri      (NautilusIconsController  *controller,
								    NautilusControllerIcon   *icon);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMIconsController, fm_icons_controller, NAUTILUS_TYPE_ICONS_CONTROLLER)

static void
fm_icons_controller_initialize_class (FMIconsControllerClass *klass)
{
	NautilusIconsControllerClass *abstract_class;

	abstract_class = NAUTILUS_ICONS_CONTROLLER_CLASS (klass);

	abstract_class->get_icon_image = fm_icons_controller_get_icon_image;
	abstract_class->get_icon_property = fm_icons_controller_get_icon_property;
	abstract_class->get_icon_text = fm_icons_controller_get_icon_text;
	abstract_class->get_icon_uri = fm_icons_controller_get_icon_uri;
}

static void
fm_icons_controller_initialize (FMIconsController *controller)
{
}

FMIconsController *
fm_icons_controller_new (FMDirectoryViewIcons *icons)
{
	FMIconsController *controller;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (icons), NULL);

	controller = FM_ICONS_CONTROLLER (gtk_object_new (FM_TYPE_ICONS_CONTROLLER, NULL));
	controller->icons = icons;

	return controller;
}

static NautilusScalableIcon *
fm_icons_controller_get_icon_image (NautilusIconsController *controller,
				    NautilusControllerIcon *icon,
				    GList **emblems)
{
	/* Get the appropriate images for the file. */
	if (emblems != NULL) {
		*emblems = nautilus_icon_factory_get_emblem_icons_for_file (NAUTILUS_FILE (icon));
	}
	return nautilus_icon_factory_get_icon_for_file (NAUTILUS_FILE (icon));
}

/* return properties about the icon, keyed by the passed_in string.  If the property doesn't apply,
   return an empty string */
/* for now, the only property is "contents_as_text" */
/* We live dangerously, and consider files with NULL mime types to be text files; we can stop doing
   this when we have better mime-type routines */
   
static char *
fm_icons_controller_get_icon_property (NautilusIconsController *controller,
				       NautilusControllerIcon *icon,
				       const char *property_name)
{
	if (strcmp (property_name, "contents_as_text") == 0) {
		const char *mime_type;
		char *theme_name;
		gboolean use_text;

		/* FIXME: We need a better way to know when to use the mini-text than 
		 * the theme name starting with eazel.
		 */
		theme_name = nautilus_icon_factory_get_theme ();
		use_text = nautilus_str_has_prefix (theme_name, "eazel");
		g_free (theme_name);
		
		mime_type = nautilus_file_get_mime_type (NAUTILUS_FILE (icon));
		if (use_text && (mime_type == NULL || nautilus_str_has_prefix (mime_type, "text/"))) {
			return nautilus_file_get_uri (NAUTILUS_FILE (icon));
		}
	}
	
	/* nothing applied, so return an empty string */
	 
	return g_strdup ("");		
}

static char *
fm_icons_controller_get_icon_text (NautilusIconsController *controller,
				   NautilusControllerIcon *icon)
{
	char *attribute_names;
	char **text_array;
	char *result;
	int index;

	attribute_names = fm_directory_view_icons_get_icon_text_attribute_names
		(FM_ICONS_CONTROLLER (controller)->icons);
	text_array = g_strsplit (attribute_names, "|", 0);
	g_free (attribute_names);

	for (index = 0; text_array[index] != NULL; index++)
	{
		char *attribute_string;

		attribute_string = nautilus_file_get_string_attribute
			(NAUTILUS_FILE (icon), text_array[index]);
		
		/* Unknown attributes get turned into blank lines (also note that
		 * leaving a NULL in text_array would cause it to be incompletely
		 * freed).
		 */
		if (attribute_string == NULL) {
			attribute_string = g_strdup ("");
		}

		/* Replace each attribute name in the array with its string value */
		g_free (text_array[index]);
		text_array[index] = attribute_string;
	}

	result = g_strjoinv ("\n", text_array);

	g_strfreev (text_array);

	return result;
}

static char *
fm_icons_controller_get_icon_uri (NautilusIconsController *controller,
				  NautilusControllerIcon *icon)
{
	return nautilus_file_get_uri (NAUTILUS_FILE (icon));
}
