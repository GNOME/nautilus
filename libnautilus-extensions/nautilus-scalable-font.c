/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-scalable-font.c - A GtkObject subclass for access to scalable fonts.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-scalable-font-private.h"

#include "nautilus-background.h"
#include "nautilus-font-manager.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-glyph.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-string.h"
#include <librsvg/rsvg-ft.h>
#include <libgnome/gnome-util.h>

/* Detail member struct */
struct NautilusScalableFontDetails
{
	RsvgFTFontHandle font_handle;
	char *font_file_name;
};

/* Global things */
static RsvgFTCtx		*global_rsvg_ft_context = NULL;
static GHashTable		*global_font_handle_table = NULL;

/* GtkObjectClass methods */
static void nautilus_scalable_font_initialize_class (NautilusScalableFontClass *font_class);
static void nautilus_scalable_font_initialize       (NautilusScalableFont      *font);
static void nautilus_scalable_font_destroy          (GtkObject                 *object);
static void initialize_global_stuff_if_needed       (void);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusScalableFont, nautilus_scalable_font, GTK_TYPE_OBJECT)

/* Class init methods */
static void
nautilus_scalable_font_initialize_class (NautilusScalableFontClass *font_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (font_class);

	initialize_global_stuff_if_needed ();

	/* GtkObjectClass */
	object_class->destroy = nautilus_scalable_font_destroy;
}

void
nautilus_scalable_font_initialize (NautilusScalableFont *font)
{
	font->details = g_new0 (NautilusScalableFontDetails, 1);

	font->details->font_handle = NAUTILUS_SCALABLE_FONT_UNDEFINED_HANDLE;
}

/* GtkObjectClass methods */
static void
nautilus_scalable_font_destroy (GtkObject *object)
{
 	NautilusScalableFont *font;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (object));

	font = NAUTILUS_SCALABLE_FONT (object);

	g_free (font->details->font_file_name);
	g_free (font->details);
	
	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* Public NautilusScalableFont methods */

/**
 * nautilus_scalable_font_new:
 * @file_name: Postscript or TrueType font file name.
 *
 * Returns a font for the given font file name.
 *
 */
NautilusScalableFont *
nautilus_scalable_font_new (const char *file_name)
{
	NautilusScalableFont *font;
	RsvgFTFontHandle font_handle = -1;

	g_return_val_if_fail (nautilus_strlen (file_name) > 0, NULL);
	g_return_val_if_fail (nautilus_font_manager_file_is_scalable_font (file_name), NULL);

	initialize_global_stuff_if_needed ();

	font = NAUTILUS_SCALABLE_FONT (gtk_object_new (nautilus_scalable_font_get_type (), NULL));
	gtk_object_ref (GTK_OBJECT (font));
	gtk_object_sink (GTK_OBJECT (font));
	
	font->details->font_file_name = g_strdup (file_name);

	if (!g_hash_table_lookup_extended (global_font_handle_table,
					   font->details->font_file_name,
					   NULL,
					   (gpointer *) &font_handle)) {
		font_handle = rsvg_ft_intern (global_rsvg_ft_context, font->details->font_file_name);
		g_hash_table_insert (global_font_handle_table,
				     font->details->font_file_name,
				     GINT_TO_POINTER (font_handle));
	}
	g_assert (font_handle >= 0);

	font->details->font_handle = font_handle;

	return font;
}

NautilusScalableFont *
nautilus_scalable_font_make_bold (NautilusScalableFont *font)
{
	char *bold_font_file_name;
	NautilusScalableFont *bold_font;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);

	bold_font_file_name = nautilus_font_manager_get_bold (font->details->font_file_name);
	bold_font = nautilus_scalable_font_new (bold_font_file_name);
	g_free (bold_font_file_name);

	return bold_font;
}

NautilusDimensions
nautilus_scalable_font_measure_text (const NautilusScalableFont *font,
				     int font_size,
				     const char *text,
				     guint text_length)
{
	NautilusDimensions dimensions;
	NautilusGlyph *glyph;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NAUTILUS_DIMENSIONS_EMPTY);
	g_return_val_if_fail (font_size > 0, NAUTILUS_DIMENSIONS_EMPTY);

	if (text == NULL || text[0] == '\0' || text_length == 0) {
		return NAUTILUS_DIMENSIONS_EMPTY;
	}

	g_return_val_if_fail (text_length <= strlen (text), NAUTILUS_DIMENSIONS_EMPTY);

	glyph = nautilus_glyph_new (font, font_size, text, text_length);
	dimensions = nautilus_glyph_get_dimensions (glyph);
	nautilus_glyph_free (glyph);

	return dimensions;
}

int
nautilus_scalable_font_text_width (const NautilusScalableFont *font,
				   int font_size,
				   const char *text,
				   guint text_length)
{
	NautilusDimensions dimensions;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), 0);
	g_return_val_if_fail (font_size > 0, 0);

	if (text == NULL || text[0] == '\0' || text_length == 0) {
		return 0;
	}

	g_return_val_if_fail (text_length <= strlen (text), 0);

	dimensions = nautilus_scalable_font_measure_text (font, font_size, text, text_length);

	return dimensions.width;
}

void
nautilus_scalable_font_draw_text (const NautilusScalableFont *font,
				  GdkPixbuf *destination_pixbuf,
				  int x,
				  int y,
				  const ArtIRect *clip_area,
				  int font_size,
				  const char *text,
				  guint text_length,
				  guint32 color,
				  int opacity)
{
	NautilusGlyph *glyph;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (font_size > 0);
	g_return_if_fail (opacity >= NAUTILUS_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= NAUTILUS_OPACITY_FULLY_OPAQUE);

	if (text == NULL || text[0] == '\0' || text_length == 0) {
		return;
	}

	g_return_if_fail (text_length <= strlen (text));

	glyph = nautilus_glyph_new (font, font_size, text, text_length);

	nautilus_glyph_draw_to_pixbuf (glyph,
				       destination_pixbuf,
				       x,
				       y,
				       clip_area,
				       color,
				       opacity);

	nautilus_glyph_free (glyph);
}

/**
 * nautilus_scalable_font_largest_fitting_font_size
 * @font: A NautilusScalableFont
 * @text: Text to use for measurement.
 * @available_width: How much space is available in pixels.
 * @minimum_acceptable_font_size: The minimum acceptable font size in pixels.
 * @maximum_acceptable_font_size: The maximum acceptable font size in pixels.
 *
 * Returns: A font size than when used to render &text, will fit it all in 
 *          &available_width.  The minimum and maximum acceptable dimensions
 *          control the limits on the size of the font.  The font size is
 *          guranteed to be within this range.
 */
int
nautilus_scalable_font_largest_fitting_font_size (const NautilusScalableFont *font,
						  const char *text,
						  int available_width,
						  int minimum_acceptable_font_size,
						  int maximum_acceptable_font_size)
{
	NautilusStringList *tokenized_string;
	int i;
	char *longest_string;
	int longest_string_length;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), 0);
	g_return_val_if_fail (text != NULL, 0);
	g_return_val_if_fail (text[0] != '\0', 0);
	g_return_val_if_fail (available_width > 0, 0);
	g_return_val_if_fail (minimum_acceptable_font_size > 0, 0);
	g_return_val_if_fail (maximum_acceptable_font_size > 0, 0);
	g_return_val_if_fail (maximum_acceptable_font_size > minimum_acceptable_font_size, 0);

	tokenized_string = nautilus_string_list_new_from_tokens (text, "\n", FALSE);
	longest_string = nautilus_string_list_get_longest_string (tokenized_string);
	g_assert (longest_string != NULL);
	nautilus_string_list_free (tokenized_string);
	longest_string_length = strlen (longest_string);
	
	for (i = maximum_acceptable_font_size; i >= minimum_acceptable_font_size; i--) {
		int width;

		width = nautilus_scalable_font_text_width (font,
							   i,
							   longest_string,
							   longest_string_length);
		
		if (width <= available_width) {
			g_free (longest_string);
			return i;
		}
	}

	g_free (longest_string);

	return minimum_acceptable_font_size;
}

NautilusScalableFont *
nautilus_scalable_font_get_default_font (void)
{
	char *default_font_file_name;
	NautilusScalableFont *default_font;

	/* FIXME bugzilla.eazel.com 7344:
	 * Its evil that we have to peek preferences here to
	 * find the default smooth font, but so it goes.
	 */
	default_font_file_name = nautilus_preferences_get (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_SMOOTH_FONT);
	if (!g_file_exists (default_font_file_name)) {
		g_free (default_font_file_name);
		default_font_file_name = nautilus_font_manager_get_default_font ();
	}

	g_assert (default_font_file_name != NULL);
	default_font = nautilus_scalable_font_new (default_font_file_name);
	g_free (default_font_file_name);
	g_assert (NAUTILUS_IS_SCALABLE_FONT (default_font));
	return default_font;
}

NautilusScalableFont *
nautilus_scalable_font_get_default_bold_font (void)
{
	char *default_bold_font_file_name;
	NautilusScalableFont *default_bold_font;

	/* FIXME bugzilla.eazel.com 7344:
	 * Its evil that we have to peek preferences here to
	 * find the default smooth font, but so it goes.
	 */
	default_bold_font_file_name = nautilus_preferences_get ("directory-view/smooth_font");
	if (!g_file_exists (default_bold_font_file_name)) {
		g_free (default_bold_font_file_name);
		default_bold_font_file_name = nautilus_font_manager_get_default_bold_font ();
	}

	g_assert (default_bold_font_file_name != NULL);
	default_bold_font = nautilus_scalable_font_new (default_bold_font_file_name);
	g_free (default_bold_font_file_name);
	g_assert (NAUTILUS_IS_SCALABLE_FONT (default_bold_font));
	return default_bold_font;
}

/* 'atexit' destructor for rsvg context */
static void
destroy_global_rsvg_ft_context (void)
{
	rsvg_ft_ctx_done (global_rsvg_ft_context);
}

static void
free_global_font_handle_table (void)
{
	if (global_font_handle_table != NULL) {
		g_hash_table_destroy (global_font_handle_table);
	}

	global_font_handle_table = NULL;
}

static void
initialize_global_stuff_if_needed (void)
{
	/* Initialize the rsvg font context shared by all fonts */
	if (global_rsvg_ft_context == NULL) {
		global_rsvg_ft_context = rsvg_ft_ctx_new ();
		g_atexit (destroy_global_rsvg_ft_context);
	}

	if (global_font_handle_table == NULL) {
		global_font_handle_table = g_hash_table_new (g_str_hash, g_str_equal);
		g_atexit (free_global_font_handle_table);
	}
}

/* Private NautilusScalableFont things */
int
nautilus_scalable_font_get_rsvg_handle (const NautilusScalableFont *font)
{
	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NAUTILUS_SCALABLE_FONT_UNDEFINED_HANDLE);

	return font->details->font_handle;
}

struct _RsvgFTCtx *
nautilus_scalable_font_get_rsvg_context (const NautilusScalableFont *font)
{
	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);

	initialize_global_stuff_if_needed ();

	return global_rsvg_ft_context;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_scalable_font (void)
{
	/* const char *fonts_place = NAUTILUS_DATADIR "/fonts/urw"; */
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
