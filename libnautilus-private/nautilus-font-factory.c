/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-font-factory.c: Class for obtaining fonts.
 
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
  
   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-font-factory.h"

#include "nautilus-global-preferences.h"
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <pthread.h>
#include <unistd.h>

#include <libgnome/gnome-i18n.h>

#define NAUTILUS_TYPE_FONT_FACTORY \
	(nautilus_font_factory_get_type ())
#define NAUTILUS_FONT_FACTORY(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_FONT_FACTORY, NautilusFontFactory))
#define NAUTILUS_FONT_FACTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_FONT_FACTORY, NautilusFontFactoryClass))
#define NAUTILUS_IS_FONT_FACTORY(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_FONT_FACTORY))
#define NAUTILUS_IS_FONT_FACTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_FONT_FACTORY))

/* The font factory */
typedef struct {
	GtkObject object;

	GHashTable *fonts;
} NautilusFontFactory;

typedef struct {
	GtkObjectClass parent_class;
} NautilusFontFactoryClass;

/* FontHashNode */
typedef struct {
	char		*name;
	GdkFont		*font;
} FontHashNode;

static NautilusFontFactory *global_font_factory = NULL;

static GtkType nautilus_font_factory_get_type         (void);
static void    nautilus_font_factory_initialize_class (NautilusFontFactoryClass *class);
static void    nautilus_font_factory_initialize       (NautilusFontFactory      *factory);
static void    destroy                                (GtkObject                *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusFontFactory,
				   nautilus_font_factory,
				   GTK_TYPE_OBJECT)

static void
unref_global_font_factory (void)
{
	gtk_object_unref (GTK_OBJECT (global_font_factory));
}

/* Return a pointer to the single global font factory. */
static NautilusFontFactory *
nautilus_get_current_font_factory (void)
{
        if (global_font_factory == NULL) {
		global_font_factory = NAUTILUS_FONT_FACTORY (gtk_object_new (nautilus_font_factory_get_type (), NULL));
		gtk_object_ref (GTK_OBJECT (global_font_factory));
		gtk_object_sink (GTK_OBJECT (global_font_factory));
		g_atexit (unref_global_font_factory);
        }

        return global_font_factory;
}

GtkObject *
nautilus_font_factory_get (void)
{
	return GTK_OBJECT (nautilus_get_current_font_factory ());
}

static void
nautilus_font_factory_initialize (NautilusFontFactory *factory)
{
        factory->fonts = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
nautilus_font_factory_initialize_class (NautilusFontFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
}

static FontHashNode *
font_hash_node_alloc (const char *name)
{
	FontHashNode *node;
	
	g_assert (name != NULL);

	node = g_new0 (FontHashNode, 1);
	node->name = g_strdup (name);

	return node;
}

static void
font_hash_node_free (FontHashNode *node)
{
	g_assert (node != NULL);

	g_free (node->name);
	gdk_font_unref (node->font);

	g_free (node);
}

static void
free_one_hash_node (gpointer key, gpointer value, gpointer callback_data)
{
	FontHashNode *node;

	g_assert (key != NULL);
	g_assert (value != NULL);
	g_assert (callback_data == NULL);

	node = value;

	g_assert (node->name == key);

	font_hash_node_free (node);
}

static void
destroy (GtkObject *object)
{
	NautilusFontFactory *factory;

	factory = NAUTILUS_FONT_FACTORY (object);

	g_hash_table_foreach (factory->fonts, free_one_hash_node, NULL);
	g_hash_table_destroy (factory->fonts);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static FontHashNode *
font_hash_node_lookup (const char *name)
{
	NautilusFontFactory *factory;
	
	g_assert (name != NULL);

	factory = nautilus_get_current_font_factory ();
	return (FontHashNode *) g_hash_table_lookup (factory->fonts, name);
}

static FontHashNode *
font_hash_node_lookup_with_insertion (const char *name)
{
	NautilusFontFactory *factory;
	FontHashNode *node;
	GdkFont *font;

	g_assert (name != NULL);

	factory = nautilus_get_current_font_factory ();
	node = font_hash_node_lookup (name);

	if (node == NULL) {
		font = gdk_fontset_load (name);
		
		if (font != NULL) {
			node = font_hash_node_alloc (name);
			node->font = font;
			g_hash_table_insert (factory->fonts, node->name, node);
		}
	}
	
	return node;
}

/* Public functions */
GdkFont *
nautilus_font_factory_get_font_by_family (const char *family,
					  guint       size_in_pixels)
{
	NautilusFontFactory *factory;
	GdkFont *font;
	FontHashNode *node;
	char *font_name;
	char **fontset;
	char **iter;

	g_return_val_if_fail (family != NULL, NULL);
	g_return_val_if_fail (size_in_pixels > 0, NULL);

	/* FIXME bugzilla.gnome.org 47907: 
	 * The "GTK System Font" string is hard coded in many places.
	 */
	if (eel_str_is_equal (family, "GTK System Font")) {
		return eel_gtk_get_system_font ();
	}

	fontset = g_strsplit (family, ",", 5);
	iter = fontset;

	factory = nautilus_get_current_font_factory ();
	while (*iter) {
		/* FIXME bugzilla.gnome.org 47347: 
		 * Its a hack that we check for "-" prefixes in font names.
		 * We do this in order not to break transalted font families.
		 */
		if (!eel_str_has_prefix (*iter, "-")) {
			font_name = eel_gdk_font_xlfd_string_new ("*", 
								       *iter,
								       "medium",
								       "r",
								       "normal",
								       "*",
								       size_in_pixels);
		} else {
			font_name = g_strdup (*iter);
		}
	
		g_free (*iter);
		*iter = font_name;
		iter++;
	}

	font_name = g_strjoinv (",", fontset);
	g_strfreev (fontset);

	node = font_hash_node_lookup_with_insertion (font_name);

	if (node != NULL) {
		g_assert (node->font != NULL);
		font = node->font;
		gdk_font_ref (font);
	} else {
		font = eel_gdk_font_get_fixed ();
	}

	g_free (font_name);

	return font;
}

GdkFont *
nautilus_font_factory_get_font_from_preferences (guint size_in_pixels)
{
	static gboolean icon_view_font_auto_value_registered;
	static const char *icon_view_font_auto_value;

	/* Can't initialize this in initialize_class, because no font factory
	 * instance may yet exist when this is called.
	 */
	if (!icon_view_font_auto_value_registered) {
		eel_preferences_add_auto_string (NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
						 &icon_view_font_auto_value);
		icon_view_font_auto_value_registered = TRUE;
	}

	/* FIXME: We hardwire icon view font here, but some callers probably
	 * expect default font instead.
	 */
	return nautilus_font_factory_get_font_by_family (icon_view_font_auto_value, size_in_pixels);
}
