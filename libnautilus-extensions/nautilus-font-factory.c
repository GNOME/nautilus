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
#include "nautilus-gtk-macros.h"
#include "nautilus-string.h"
#include "nautilus-global-preferences.h"

#include <unistd.h>
#include <pthread.h>

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

static GtkType              nautilus_font_factory_get_type         (void);
static void                 nautilus_font_factory_initialize_class (NautilusFontFactoryClass *class);
static void                 nautilus_font_factory_initialize       (NautilusFontFactory      *factory);
static NautilusFontFactory *nautilus_get_current_font_factory      (void);
static NautilusFontFactory *nautilus_font_factory_new              (void);
static char *               make_font_name_string                  (const char               *foundry,
								    const char               *familiy,
								    const char               *weight,
								    const char               *slant,
								    const char               *set_width,
								    const char               *add_style,
								    guint                     size_in_pixels);
static FontHashNode *       font_hash_node_alloc                   (const char               *name);
static FontHashNode *       font_hash_node_lookup                  (const char               *name);
static FontHashNode *       font_hash_node_lookup_with_insertion   (const char               *name);

#if 0
static void                     font_hash_node_free                     (FontHashNode         *node);
#endif

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusFontFactory, nautilus_font_factory, GTK_TYPE_OBJECT)

/* Return a pointer to the single global font factory. */
static NautilusFontFactory *
nautilus_get_current_font_factory (void)
{
        static NautilusFontFactory *global_font_factory = NULL;

        if (global_font_factory == NULL) {
                global_font_factory = nautilus_font_factory_new ();
        }

        return global_font_factory;
}

GtkObject *
nautilus_font_factory_get (void)
{
	return GTK_OBJECT (nautilus_get_current_font_factory ());
}

/* Create the font factory. */
static NautilusFontFactory *
nautilus_font_factory_new (void)
{
        NautilusFontFactory *factory;
        
        factory = (NautilusFontFactory *) gtk_object_new (nautilus_font_factory_get_type (), NULL);

        return factory;
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
}

static FontHashNode *
font_hash_node_alloc (const char *name)
{
	FontHashNode * node;
	
	g_assert (name != NULL);

	node = g_new (FontHashNode, 1);

	node->name = g_strdup (name);

	node->font = NULL;

	return node;
}

#if 0
static void
font_hash_node_free (FontHashNode *node)
{
	g_assert (node != NULL);

	g_free (node->name);

	g_free (node);
}
#endif

static FontHashNode *
font_hash_node_lookup (const char *name)
{
	static NautilusFontFactory *factory;

	gpointer hash_value;
	
	g_assert (name != NULL);

	factory = nautilus_get_current_font_factory ();
	g_assert (factory != NULL);

	hash_value = g_hash_table_lookup (factory->fonts, (gconstpointer) name);
	
	return (FontHashNode *) hash_value;
}

static FontHashNode *
font_hash_node_lookup_with_insertion (const char *name)
{
	static NautilusFontFactory *factory;

	FontHashNode *node = NULL;

	g_assert (name != NULL);

	factory = nautilus_get_current_font_factory ();
	g_assert (factory != NULL);


	node = font_hash_node_lookup (name);

	if (node == NULL) {
		GdkFont *font;
		
		font = gdk_font_load (name);
		
		if (font != NULL) {
			node = font_hash_node_alloc (name);
			node->font = font;
			
			gdk_font_ref (node->font);

			g_hash_table_insert (factory->fonts, node->name, node);
		}
	}
	
	return node;
}

static char *
make_font_name_string (const char *foundry,
		       const char *familiy,
		       const char *weight,
		       const char *slant,
		       const char *set_width,
		       const char *add_style,
		       guint size_in_pixels)
{
	char *font_name;

        const char *points = "*";
        const char *hor_res = "*";
        const char *ver_res = "*";
        const char *spacing = "*";
        const char *average_width = "*";
        const char *char_set_registry = "*";
        const char *char_set_encoding = "*";


	/*                             +---------------------------------------------------- foundry
	                               |  +------------------------------------------------- family
				       |  |  +---------------------------------------------- weight
				       |  |  |  +------------------------------------------- slant 
				       |  |  |  |  +---------------------------------------- sel_width
				       |  |  |  |  |  +------------------------------------- add-style
				       |  |  |  |  |  |  +---------------------------------- pixels   	
				       |  |  |  |  |  |  |  +------------------------------- points  
				       |  |  |  |  |  |  |  |  +---------------------------- hor_res        
				       |  |  |  |  |  |  |  |  |  +------------------------- ver_res        
				       |  |  |  |  |  |  |  |  |  |  +---------------------- spacing        
				       |  |  |  |  |  |  |  |  |  |  |  +------------------- average_width        
				       |  |  |  |  |  |  |  |  |  |  |  |  +---------------- char_set_registry
				       |  |  |  |  |  |  |  |  |  |  |  |  |  +------------- char_set_encoding */
	font_name = g_strdup_printf ("-%s-%s-%s-%s-%s-%s-%d-%s-%s-%s-%s-%s-%s-%s",
				     foundry,
				     familiy,
				     weight,
				     slant,
				     set_width,
				     add_style,
				     size_in_pixels,
				     points,
				     hor_res,
				     ver_res,
				     spacing,
				     average_width,
				     char_set_registry,
				     char_set_encoding);
	
	return font_name;
}

/* Public functions */
GdkFont *
nautilus_font_factory_get_font_by_family (const char *family,
					  guint       size_in_pixels)
{
	static NautilusFontFactory *factory;
	GdkFont *font = NULL;
	FontHashNode *node;
	char *font_name;

	g_return_val_if_fail (family != NULL, NULL);
	g_return_val_if_fail (size_in_pixels > 0, NULL);

	factory = nautilus_get_current_font_factory ();
	g_assert (factory != NULL);

	font_name = make_font_name_string ("*", 
					   family,
					   "medium",
					   "r",
					   "normal",
					   "*",
					   size_in_pixels);

	g_assert (font_name != NULL);

	node = font_hash_node_lookup_with_insertion (font_name);

	if (node != NULL) {
		g_assert (node->font);

		font = node->font;

		gdk_font_ref (font);
	}
	else {
		font = nautilus_font_factory_get_fallback_font ();
	}

	g_free (font_name);

	return font;
}

GdkFont *
nautilus_font_factory_get_font_from_preferences (guint size_in_pixels)
{
	char	*family;
	GdkFont *font;

	family = nautilus_preferences_get (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY, "helvetica");

	font = nautilus_font_factory_get_font_by_family (family, size_in_pixels);

	g_free (family);

	return font;
}

GdkFont *
nautilus_font_factory_get_fallback_font (void)
{
	static GdkFont *fixed_font;

	if (fixed_font == NULL) {
		fixed_font = gdk_font_load ("fixed");
		g_assert (fixed_font != NULL);
		gdk_font_ref (fixed_font);
	}

	return fixed_font;
}
