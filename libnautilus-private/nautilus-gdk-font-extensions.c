/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gdk-extensions.c: GdkFont extensions.

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

   Authors: Darin Adler <darin@eazel.com>, 
            Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "nautilus-gdk-font-extensions.h"

#include "nautilus-font-factory.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string-list.h"
#include "nautilus-string.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>

/* Magic constant copied from GTK+ */
#define MAX_FONTS 32767

/* These are arbitrary constants to catch insane values */
#define MIN_NUM_STEPS 1
#define MAX_NUM_STEPS 40

/* Indeces of some xlfd string entries.  The first entry is 1 */
#define XLFD_WEIGHT_INDEX			3
#define XLFD_SLANT_INDEX			4
#define XLFD_SIZE_IN_PIXELS_INDEX		7
#define XLFD_SIZE_IN_POINTS_INDEX		8
#define XLFD_HORIZONTAL_RESOLUTION_INDEX	9
#define XLFD_VERTICAL_RESOLUTION_INDEX		10
#define XLFD_MAX_INDEX				14

/* Indicates an invalid xlfd string */
#define XLFD_INVALID_VALUE			-1

/* Indicates a xlfd wildcard: * */
#define XLFD_WILDCARD_VALUE			-2

#define ELLIPSIS "..."

/* Font functions that could be public */
static NautilusStringList *      font_list_fonts                          (const char               *pattern);
static const NautilusStringList *font_list_fonts_cached                   (const char               *pattern,
									   GCompareFunc              compare_function);
static char *                    font_get_name                            (const GdkFont            *font);
static guint                     font_get_size_in_pixels                  (const GdkFont            *font);
static GdkFont *                 font_get_bold                            (GdkFont                  *font);

/* XLFD string operations */
static char *                    xlfd_string_get_nth                      (const char               *xlfd_string,
									   guint                     n);
static char *                    xlfd_string_replace_nth                  (const char               *xlfd_string,
									   guint                     n,
									   const char               *replace_string);
static int                       xlfd_string_get_nth_as_int               (const char               *xlfd_string,
									   guint                     n);
static gboolean                  xlfd_string_is_scalable_non_bitmap       (const char               *xlfd_string);
static gboolean                  xlfd_string_could_be_scalable_non_bitmap (const char               *xlfd_string);

/* Test functions for searching font lists */
static gboolean                  font_entry_has_bold_weight_test          (const NautilusStringList *string_list,
									   const char               *string,
									   gpointer                  callback_data);
static gboolean                  font_entry_has_italic_slant_test         (const NautilusStringList *string_list,
									   const char               *string,
									   gpointer                  callback_data);
static gboolean                  font_entry_is_scalable_non_bitmap_test   (const NautilusStringList *string_list,
									   const char               *string,
									   gpointer                  callback_data);
/* Comparison functions for sorting fonts lists */
static int                       compare_xlfd_by_size_in_points           (gconstpointer             string_a,
									   gconstpointer             string_b);
static int                       compare_xlfd_by_size_in_pixels           (gconstpointer             string_a,
									   gconstpointer             string_b);

/**
 * nautilus_gdk_font_get_italic
 * @plain_font: A font.
 * Returns: An italic variant of @plain_font or NULL.
 *
 * Tries to find an italic flavor of a given font.  Return the same font
 * if no italic font is found.
 */
GdkFont *
nautilus_gdk_font_get_italic (GdkFont *font)
{
	char *name;
	char *slant_pattern;
	GdkFont *result = NULL;
	char *pattern_match;
	const NautilusStringList *font_list;

	/* FIXME bugzilla.eazel.com 7350:
	 * The issue is that the the italic font matching code here 
	 * assumes that all fonts are for the same encoding.
	 * So we might find an italic version of a font, but but it will
	 * be the in a wrong encoding and thus not appropiate for the current
	 * locale.  This only happens in multi byte locales, because of the 
	 * limited number of fonts found in these.
	 */
	if (nautilus_dumb_down_for_multi_byte_locale_hack ()) {
		gdk_font_ref (font);
		return font;
	}

	name = font_get_name (font);

	/* Replace the slant with a wildard */
	slant_pattern = xlfd_string_replace_nth (name, XLFD_SLANT_INDEX, "*");

	if (slant_pattern == NULL) {
		g_free (name);
		gdk_font_ref (font);
		return font;
	}

	font_list = font_list_fonts_cached (slant_pattern, NULL);
	
 	/* Find one with an italic slant */
 	pattern_match = nautilus_string_list_find_by_function (font_list,
 							       font_entry_has_italic_slant_test,
 							       NULL);

	if (pattern_match != NULL) {
		char *slant_name;
		char *italic_name;

		/* Find out the italic slant */
		slant_name = xlfd_string_get_nth (pattern_match, XLFD_SLANT_INDEX);
		
		/* Set the italic slant on the original name */
		italic_name = xlfd_string_replace_nth (name, XLFD_SLANT_INDEX, slant_name);
		
		result = gdk_fontset_load (italic_name);
		if (result == NULL) {
			gdk_font_ref (font);
			result = font;
		}

		g_free (italic_name);
		g_free (slant_name);
	} else {
		/* If no font was found, return the source font */
		gdk_font_ref (font);
		result = font;
	}

	g_free (pattern_match);
	g_free (slant_pattern);
	g_free (name);

	return result;
}

static GHashTable *bold_font_table = NULL;

typedef struct {
	char *name;
	GdkFont *bold_font;
} BoldFontEntry;

static void
bold_font_table_free_one_node (gpointer key,
			       gpointer value,
			       gpointer callback_data)
{
	BoldFontEntry *entry;

	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);

	entry = value;
	g_free (entry->name);
	gdk_font_unref (entry->bold_font);
	g_free (entry);
}

static void
bold_font_table_free (void)
{
	if (bold_font_table != NULL) {
		g_hash_table_foreach (bold_font_table, bold_font_table_free_one_node, NULL);
		g_hash_table_destroy (bold_font_table);
	}

	bold_font_table = NULL;
}

/**
 * nautilus_gdk_font_get_bold
 * @plain_font: A font.
 * Returns: A bold variant of @plain_font or NULL.
 *
 * Tries to find a bold flavor of a given font.  Return the same font
 * if no bold font is found.
 */
GdkFont *
nautilus_gdk_font_get_bold (GdkFont *font)
{
	char *name;
	BoldFontEntry *entry;

	g_return_val_if_fail (font != NULL, NULL);

	/* FIXME bugzilla.eazel.com 7350:
	 * The issue is that the the bold font matching code here 
	 * assumes that all fonts are for the same encoding.
	 * So we might find a bold version of a font, but but it will
	 * be the in a wrong encoding and thus not appropiate for the current
	 * locale.  This only happens in multi byte locales, because of the 
	 * limited number of fonts found in these.
	 */
	if (nautilus_dumb_down_for_multi_byte_locale_hack ()) {
		gdk_font_ref (font);
		return font;
	}

	if (bold_font_table == NULL) {
		bold_font_table = g_hash_table_new (g_str_hash, g_str_equal);
		g_atexit (bold_font_table_free);
	}

	g_assert (bold_font_table != NULL);

	name = font_get_name (font);
	entry = g_hash_table_lookup (bold_font_table, name);

	if (entry != NULL) {
		g_assert (entry->bold_font != NULL);
		gdk_font_ref (entry->bold_font);
		g_free (name);
		return entry->bold_font;
	}

	entry = g_new0 (BoldFontEntry, 1);
	entry->name = g_strdup (name);
	entry->bold_font = font_get_bold (font);

	if (entry->bold_font == NULL) {
		g_free (entry->name);
		g_free (entry);
		gdk_font_ref (font);
		return font;
	}
	g_free (name);
	g_hash_table_insert (bold_font_table, entry->name, entry);

	g_assert (g_hash_table_lookup (bold_font_table, entry->name) == entry);

	gdk_font_ref (entry->bold_font);
	return entry->bold_font;
}

/* Return a scalable font matching the given target size */
static GdkFont *
font_scalable_get_by_size (const char *xlfd_string,
			   guint target_size,
			   guint index)
{
	GdkFont *larger_font = NULL;
	char *larger_size_string;
	char *larger_font_name;
	
	g_return_val_if_fail (xlfd_string != NULL, NULL);
	g_return_val_if_fail (target_size > 0, NULL);
	g_return_val_if_fail (index == XLFD_SIZE_IN_POINTS_INDEX || index == XLFD_SIZE_IN_PIXELS_INDEX, NULL);

	larger_size_string = g_strdup_printf ("%d", target_size);
	
	larger_font_name = xlfd_string_replace_nth (xlfd_string,
						    index,
						    larger_size_string);
	
	if (larger_font_name != NULL) {
		larger_font = gdk_fontset_load (larger_font_name);
	}
	
	g_free (larger_size_string);
	g_free (larger_font_name);
	
	return larger_font;
}

/* Return a bitmap font matching the given target size as much as possible */
static GdkFont *
font_bitmap_get_by_size (const char *xlfd_string,
			 guint target_size,
			 guint index,
			 GCompareFunc compare_function)
{
	GdkFont *larger_font = NULL;
 	char *last_entry;
	guint max_size;
 	char *larger_pattern_xlfd = NULL;
	const NautilusStringList *font_list;
	
	g_return_val_if_fail (xlfd_string != NULL, NULL);
	g_return_val_if_fail (target_size > 0, NULL);
	g_return_val_if_fail (index == XLFD_SIZE_IN_POINTS_INDEX || index == XLFD_SIZE_IN_PIXELS_INDEX, NULL);
	g_return_val_if_fail (compare_function != NULL, NULL);
	
 	larger_pattern_xlfd = xlfd_string_replace_nth (xlfd_string, index, "*");

	if (larger_pattern_xlfd == NULL) {
		return NULL;
	}

	/* Make a query */
	font_list = font_list_fonts_cached (larger_pattern_xlfd, compare_function);

	last_entry = nautilus_string_list_nth (font_list, nautilus_string_list_get_length (font_list) - 1);
	max_size = xlfd_string_get_nth_as_int (last_entry, index);
	g_free (last_entry);

	if (target_size >= max_size) {
		/* If target font is large, then return the maximum available size */
		char *larger_font_name;
		char *size_string;
		
		size_string = g_strdup_printf ("%d", max_size);
		larger_font_name = xlfd_string_replace_nth (xlfd_string, index, size_string);
		g_free (size_string);
		larger_font = gdk_fontset_load (larger_font_name);
		g_free (larger_font_name);
	} else {
		/* Look for the closest matching font */
		guint i = 0;
		int found_size = 0;
		char *larger_font_name;
		char *size_string;

		while ((i < nautilus_string_list_get_length (font_list)) && (found_size == 0)) {
			char *entry;
			guint entry_value;

			entry = nautilus_string_list_nth (font_list, i);
			entry_value = xlfd_string_get_nth_as_int (entry, index);

			if (entry_value >= target_size) {
				found_size = entry_value;
			}
			
			g_free (entry);

			i++;
		}

		if (found_size > 0) {
			size_string = g_strdup_printf ("%d", found_size);
			larger_font_name = xlfd_string_replace_nth (xlfd_string, index, size_string);
			g_free (size_string);
			larger_font = gdk_fontset_load (larger_font_name);
			g_free (larger_font_name);
		}
	}
	
 	g_free (larger_pattern_xlfd);
	
	return larger_font;
}

/**
 * nautilus_gdk_font_get_larger
 * @font: A GdkFont.
 * Returns: A GdkFont that is num_steps larger than the given one.
 *          For scalable fonts, the resulting font is always the
 *          requested size.  For bitmap fonts, the result will be
 *          the first font equal or larger than the requested size.
 */
GdkFont *
nautilus_gdk_font_get_larger (GdkFont *font,
			      int num_steps)
{
	GdkFont *result = NULL;
	char *name;
	int size_in_points;
	int size_in_pixels;
	int target_index = -1;

	g_return_val_if_fail (font != NULL, NULL);
	g_return_val_if_fail (ABS (num_steps) >= MIN_NUM_STEPS, NULL);
	g_return_val_if_fail (ABS (num_steps) <= MAX_NUM_STEPS, NULL);

	/* FIXME bugzilla.eazel.com 7350:
	 * The issue is that the the larger font matching code here 
	 * assumes that all fonts are for the same encoding.
	 * So we might find a larger version of a font, but but it will
	 * be the in a wrong encoding and thus not appropiate for the current
	 * locale.  This only happens in multi byte locales, because of the 
	 * limited number of fonts found in these.
	 */
	if (nautilus_dumb_down_for_multi_byte_locale_hack ()) {
		gdk_font_ref (font);
		return font;
	}

	name = font_get_name (font);

	size_in_points = xlfd_string_get_nth_as_int (name, XLFD_SIZE_IN_POINTS_INDEX);
	size_in_pixels = xlfd_string_get_nth_as_int (name, XLFD_SIZE_IN_PIXELS_INDEX);

	if (size_in_points <= 0 && size_in_pixels <= 0) {
		g_free (name);
		return NULL;
	}

	/* If for some crazy reason both the "size_in_points" and
	 * the "size_in_pixels" wildcards are on, then we pick
	 * only one. */

	/* FIXME bugzilla.eazel.com xxxx:
	 * Choosing pixels over points here is an arbitrary 
	 * decision.  Is there a right one ?
	 */
	if (size_in_points == XLFD_WILDCARD_VALUE && size_in_pixels == XLFD_WILDCARD_VALUE) {
		size_in_pixels = 1;
		size_in_points = 0;
	}

	/* Figure out the index of the entry we need to bump */
	if (size_in_points > 0) {
		target_index = XLFD_SIZE_IN_POINTS_INDEX;
	} else {
		target_index = XLFD_SIZE_IN_PIXELS_INDEX;
	}
	
	g_assert (target_index == XLFD_SIZE_IN_POINTS_INDEX
		  || target_index == XLFD_SIZE_IN_PIXELS_INDEX);

	if (xlfd_string_could_be_scalable_non_bitmap (name)) {
		/* If the font is scalable then we simply can add or subtract
		 * points or pixels to get the target font.
		 */
		if (target_index == XLFD_SIZE_IN_PIXELS_INDEX) {
			guint target_size_in_pixels;

			target_size_in_pixels = size_in_pixels + num_steps;
			
			result = font_scalable_get_by_size (name, target_size_in_pixels, XLFD_SIZE_IN_PIXELS_INDEX);
		} else {
			guint target_size_in_points;

			target_size_in_points = size_in_points + (10 * num_steps);

			result = font_scalable_get_by_size (name, target_size_in_points, XLFD_SIZE_IN_POINTS_INDEX);
		}
	} else {
		/* If the font is not scalable, then we have to make a query
		 * and find the next available larger font.
		 */
		if (target_index == XLFD_SIZE_IN_PIXELS_INDEX) {
			guint target_size_in_pixels;

			target_size_in_pixels = size_in_pixels + num_steps;

			result = font_bitmap_get_by_size (name,
							  target_size_in_pixels,
							  XLFD_SIZE_IN_PIXELS_INDEX,
							  compare_xlfd_by_size_in_pixels);
		} else {
			guint target_size_in_points;

			target_size_in_points = size_in_points + (num_steps * 10);
			
			result = font_bitmap_get_by_size (name,
							  target_size_in_points,
							  XLFD_SIZE_IN_POINTS_INDEX,
							  compare_xlfd_by_size_in_points);
		}
	}

	g_free (name);

	/* If no font was found, return the source font */
	if (result == NULL) {
		gdk_font_ref (font);
		result = font;
	}

	g_assert (result != NULL);

	return result;
}

/**
 * nautilus_gdk_font_get_smaller
 * @font: A GdkFont.
 * Returns: A GdkFont that is num_steps smaller than the given one.
 *          For scalable fonts, the resulting font is always the
 *          requested size.  For bitmap fonts, the result will be
 *          the first font equal or smaller than the requested size.
 */
GdkFont *
nautilus_gdk_font_get_smaller (GdkFont *font, int num_steps)
{
	g_return_val_if_fail (font != NULL, NULL);
	g_return_val_if_fail (ABS (num_steps) >= MIN_NUM_STEPS, NULL);
	g_return_val_if_fail (ABS (num_steps) <= MAX_NUM_STEPS, NULL);

	return nautilus_gdk_font_get_larger (font, -ABS (num_steps));
}

/**
 * nautilus_gdk_font_equal
 * @font_a_null_allowed: A font or NULL.
 * @font_b_null_allowed: A font or NULL.
 *
 * Calls gdk_font_equal, unless one of the fonts is NULL.
 */
gboolean
nautilus_gdk_font_equal (GdkFont *font_a_null_allowed,
			 GdkFont *font_b_null_allowed)
{
	if (font_a_null_allowed == NULL) {
		return font_b_null_allowed == NULL;
	}
	if (font_b_null_allowed == NULL) {
		return FALSE;
	}
	return gdk_font_equal (font_a_null_allowed, font_b_null_allowed);
}

/**
 * nautilus_gdk_font_get_largest_fitting
 * @font: A GdkFont.
 * @text: Text to use for measurement.
 * @available_width: How much space is available in pixels.
 * @minimum_acceptable_font_size: The minimum acceptable font size in pixels.
 * @maximum_acceptable_font_size: The maximum acceptable font size in pixels.
 *
 * Returns: A GdkFont that when used to render &text, will fit it all in 
 *          &available_width.  The minimum and maximum acceptable dimensions
 *          control the limits on the size of the font.  The font size is
 *          guranteed to be within this range.
 *          The resulting font needs to be freed with gdk_font_unref()
 */
GdkFont *
nautilus_gdk_font_get_largest_fitting (GdkFont *font,
				       const char *text,
				       int available_width,
				       int minimum_acceptable_font_size,
				       int maximum_acceptable_font_size)
{
	GdkFont *largest_fitting_font = NULL;
	NautilusStringList *tokenized_string;
	char *longest_string;
	guint longest_string_length;

	g_return_val_if_fail (font != NULL, NULL);
	g_return_val_if_fail (text != NULL, 0);
	g_return_val_if_fail (text[0] != '\0', 0);
	g_return_val_if_fail (available_width > 0, NULL);
	g_return_val_if_fail (minimum_acceptable_font_size > 0, NULL);
	g_return_val_if_fail (maximum_acceptable_font_size > 0, NULL);
	g_return_val_if_fail (maximum_acceptable_font_size > minimum_acceptable_font_size, NULL);

	/* FIXME bugzilla.eazel.com 7350:
	 * The issue is that the the larger font matching code here 
	 * assumes that all fonts are for the same encoding.
	 * So we might find a larger version of a font, but but it will
	 * be the in a wrong encoding and thus not appropiate for the current
	 * locale.  This only happens in multi byte locales, because of the 
	 * limited number of fonts found in these.
	 */
	if (nautilus_dumb_down_for_multi_byte_locale_hack ()) {
		gdk_font_ref (font);
		return font;
	}
	
	tokenized_string = nautilus_string_list_new_from_tokens (text, "\n", FALSE);
	longest_string = nautilus_string_list_get_longest_string (tokenized_string);
	g_assert (longest_string != NULL);
	nautilus_string_list_free (tokenized_string);
	longest_string_length = strlen (longest_string);

	/* Make sure the font was specified in pixels */
	if (font_get_size_in_pixels (font) > 0) {
		int candidate_size;
		char *font_name;
		gboolean done;
		
		font_name = font_get_name (font);
		
		/* Iterate through the fonts until we find one that works */
		candidate_size = maximum_acceptable_font_size;
		done = FALSE;
		while (!done) {
 			GdkFont *candidate_font;
 			int candidate_width;

			candidate_font = font_bitmap_get_by_size (font_name,
								  candidate_size,
								  XLFD_SIZE_IN_PIXELS_INDEX,
								  compare_xlfd_by_size_in_pixels);
			if (candidate_font != NULL) {
				candidate_width = gdk_string_width (candidate_font, longest_string);
				
				if ((candidate_width <= available_width) 
				    || (candidate_size <= minimum_acceptable_font_size)) {
					done = TRUE;
					largest_fitting_font = candidate_font;
				} else {
					gdk_font_unref (candidate_font);
				}
			}
			
			candidate_size--;
		}

		g_free (font_name);
	}

	g_free (longest_string);

	return largest_fitting_font;
}

/**
 * nautilus_string_ellipsize_start:
 * 
 * @string: A a string to be ellipsized.
 * @font: A a font used to measure the resulting string width.
 * @width: Desired maximum width in points.
 * Returns: A truncated string at most @width points long.
 * 
 * Truncates a string, removing characters from the start and 
 * replacing them with "..." 
 * 
 */
char *
nautilus_string_ellipsize_start (const char *string, GdkFont *font, int width)
{
	int truncate_offset;
	int resulting_width;

	resulting_width = gdk_string_width (font, string);
	if (resulting_width <= width) {
		/* String is already short enough. */
		return g_strdup (string);
	}
	
	/* Account for the width of the ellipsis. */
	width -= gdk_string_width (font, ELLIPSIS);
	

	if (width < 0) {
		/* No room even for a an ellipsis. */
		return g_strdup ("");
	}
	
	g_assert (strlen (string) > 0);

	/* We rely on the fact that GdkFont does not use kerning and that
	 * gdk_string_width ("foo") + gdk_string_width ("bar") ==
	 * gdk_string_width ("foobar")
	 */
        for (truncate_offset = 1; ; truncate_offset++) {
        	if (string[truncate_offset] == '\0') {
			break;
        	}

        	resulting_width -= gdk_char_width (font, string[truncate_offset - 1]);

        	if (resulting_width <= width) {
			break;
        	}
        }

	return g_strconcat (ELLIPSIS, string + truncate_offset, NULL);
}

/**
 * nautilus_string_ellipsize_end:
 * 
 * @string: A a string to be ellipsized.
 * @font: A a font used to measure the resulting string width.
 * @width: Desired maximum width in points.
 * Returns: A truncated string at most @width points long.
 * 
 * Truncates a string, removing characters from the end and 
 * replacing them with "..." 
 * 
 */
char *
nautilus_string_ellipsize_end (const char *string, GdkFont *font, int width)
{
	int truncated_length;
	char *result;
	int resulting_width;

	resulting_width = gdk_string_width (font, string);
	if (resulting_width <= width) {
		/* String is already short enough. */
		return g_strdup (string);
	}
	
	/* Account for the width of the ellipsis. */
	width -= gdk_string_width (font, ELLIPSIS);
	

	if (width < 0) {
		/* No room even for a an ellipsis. */
		return g_strdup ("");
	}
	
        for (truncated_length = strlen (string) - 1; truncated_length > 0; truncated_length--) {
        	resulting_width -= gdk_char_width (font, string[truncated_length]);
        	if (resulting_width <= width) {
			break;
        	}
        }
	
	result = g_malloc (truncated_length + strlen (ELLIPSIS) + 1);
	memcpy (result, string, truncated_length);
	strcpy (result + truncated_length, ELLIPSIS);

	return result;
}

/**
 * nautilus_string_ellipsize_middle:
 * 
 * @string: A a string to be ellipsized.
 * @font: A a font used to measure the resulting string width.
 * @width: Desired maximum width in points.
 * Returns: A truncated string at most @width points long.
 * 
 * Truncates a string, removing characters from the middle and 
 * replacing them with "..." 
 * 
 */
char *
nautilus_string_ellipsize_middle (const char *string, GdkFont *font, int width)
{
	int original_length;
	int starting_fragment_length;
	int ending_fragment_offset;
	int resulting_width;
	char *result;

	resulting_width = gdk_string_width (font, string);
	if (resulting_width <= width) {
		/* String is already short enough. */
		return g_strdup (string);
	}
	
	/* Account for the width of the ellipsis. */
	width -= gdk_string_width (font, ELLIPSIS);
	

	if (width < 0) {
		/* No room even for a an ellipsis. */
		return g_strdup ("");
	}

	/* Split the original string into two halves */
	original_length = strlen (string);
	
	g_assert (original_length > 0);
	
	starting_fragment_length = original_length / 2;
	ending_fragment_offset = starting_fragment_length + 1;
	
	/* Shave off a character at a time from the first and the second half
	 * until we can fit
	 */
	resulting_width -= gdk_char_width (font, string[ending_fragment_offset - 1]);
	
	/* depending on whether the original string length is odd or even, start by
	 * shaving off the characters from the starting or ending fragment
	 */
	switch (original_length % 2) {
	        while (TRUE) {
	case 0:
			if (resulting_width <= width) {
				break;
			}
			g_assert (starting_fragment_length > 0 || ending_fragment_offset < original_length);
			if (starting_fragment_length > 0) {
				starting_fragment_length--;
			}
			resulting_width -= gdk_char_width (font, string[starting_fragment_length]);
	case 1:
			if (resulting_width <= width) {
				break;
			}
			g_assert (starting_fragment_length > 0 || ending_fragment_offset < original_length);
			if (ending_fragment_offset < original_length) {
				ending_fragment_offset++;
			}
			resulting_width -= gdk_char_width (font, string[ending_fragment_offset - 1]);
	        }
	}
	
	/* patch the two fragments together with an ellipsis */
	result = g_malloc (starting_fragment_length + (original_length - ending_fragment_offset)
		+ strlen (ELLIPSIS) + 1);
	memcpy (result, string, starting_fragment_length);
	strcpy (result + starting_fragment_length, ELLIPSIS);
	strcpy (result + starting_fragment_length + strlen (ELLIPSIS), string + ending_fragment_offset);

	return result;
}

/* Private font things */

/* Find a bold flavor of a font */
static GdkFont *
font_get_bold (GdkFont *font)
{
	char *name;
	char *weight_pattern;
	GdkFont *result = NULL;
	char *pattern_match;
	const NautilusStringList *font_list;

	g_return_val_if_fail (font != NULL, NULL);

	name = font_get_name (font);

	/* Replace the weight with a wildard */
	weight_pattern = xlfd_string_replace_nth (name, XLFD_WEIGHT_INDEX, "*");

	if (weight_pattern == NULL) {
		g_free (name);
		gdk_font_ref (font);
		return font;
	}

	font_list = font_list_fonts_cached (weight_pattern, NULL);
	
	/* Find one with a bold weight */
	pattern_match = nautilus_string_list_find_by_function (font_list,
							       font_entry_has_bold_weight_test,
							       NULL);

	if (pattern_match != NULL) {
		char *weight_name;
		char *bold_name;

		/* Find out the bold weight */
		weight_name = xlfd_string_get_nth (pattern_match, XLFD_WEIGHT_INDEX);
		
		/* Set the bold weight on the original name */
		bold_name = xlfd_string_replace_nth (name, XLFD_WEIGHT_INDEX, weight_name);
		
		result = gdk_fontset_load (bold_name);
		if (result == NULL) {
			gdk_font_ref (font);
			result = font;
		}

		g_free (bold_name);
		g_free (weight_name);
	} else {
		/* If no font was found, return the source font */
		gdk_font_ref (font);
		result = font;
	}

	g_free (pattern_match);
	g_free (weight_pattern);
	g_free (name);

	return result;
}

/* A wrapper for XListFonts() */
static NautilusStringList *
font_list_fonts (const char *pattern)
{
	NautilusStringList *list;
	char **font_names;
	int actual_count;
	int i;

	font_names = XListFonts (GDK_DISPLAY (), pattern, MAX_FONTS, &actual_count);

	if (font_names == NULL || actual_count <= 0) {
		return NULL;
	}

	list = nautilus_string_list_new (FALSE);

	for (i = 0; i < actual_count; i++) {
		nautilus_string_list_insert (list, font_names[i]);
	}

	XFreeFontNames (font_names);

	return list;
}

/* A version of list_fonts that returns a cached value.  In general
 * I dont hesitate to copy string lists around to avoid the caller
 * having to decide to free or not.  However, list_fonts is a very 
 * special case.  It is an expensive operation, and one that can
 * be liberally called from Nautilus.  For example, the sidebar title
 * results in many such queries being made.  Further, we have profiler
 * evidence to indicate that the specific operation of listing fonts
 * is responsible for a not insignificant amount of starvation of the 
 * gnome vfs working thread.  So there.
 */
static GHashTable *font_list_table = NULL;

typedef struct
{
	char *pattern;
	NautilusStringList *font_list;
} FontListEntry;

static void
font_list_table_free_one_node (gpointer key,
			       gpointer value,
			       gpointer callback_data)
{
	FontListEntry *entry;

	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);

	entry = value;
	g_free (entry->pattern);
	nautilus_string_list_free (entry->font_list);
	g_free (entry);
}

static void
font_list_table_free (void)
{
	if (font_list_table != NULL) {
		g_hash_table_foreach (font_list_table, font_list_table_free_one_node, NULL);
		g_hash_table_destroy (font_list_table);
	}

	font_list_table = NULL;
}

/* Return a cached list of fonts.  If a compare_function is given, the
 * list will be sorted accordingly.  As such, the sorting state of the 
 * cached font list is undefined if the compare_function is NULL.
 */
static const NautilusStringList *
font_list_fonts_cached (const char *pattern,
			GCompareFunc compare_function)
{
	FontListEntry *entry;

	g_return_val_if_fail (pattern != NULL, NULL);

	if (font_list_table == NULL) {
		font_list_table = g_hash_table_new (g_str_hash, g_str_equal);
		g_atexit (font_list_table_free);
	}

	g_assert (font_list_table != NULL);
	entry = g_hash_table_lookup (font_list_table, pattern);

	if (entry != NULL) {
		g_assert (entry->font_list != NULL);
		if (compare_function != NULL) {
			nautilus_string_list_sort_by_function (entry->font_list, compare_function);
		}
		return entry->font_list;
	}

	entry = g_new0 (FontListEntry, 1);
	entry->pattern = g_strdup (pattern);
	entry->font_list = font_list_fonts (pattern);

	if (entry->font_list == NULL) {
		g_free (entry->pattern);
		g_free (entry);
		return NULL;
	}

	g_hash_table_insert (font_list_table, entry->pattern, entry);

	g_assert (g_hash_table_lookup (font_list_table, entry->pattern) == entry);

	if (compare_function != NULL) {
		nautilus_string_list_sort_by_function (entry->font_list, compare_function);
	}
	return entry->font_list;
}

/* Return the font name - an xlfd string used by Gdk to allocate the font */
static char *
font_get_name (const GdkFont *font)
{
	GdkFontPrivate *font_private;
	const char *font_name;

	g_return_val_if_fail (font != NULL, NULL);

	font_private = (GdkFontPrivate *)font;

	if (font_private->names == NULL) {
		return NULL;
	}

	/* FIXME bugzilla.eazel.com xxxx:
	 * When do we need to look beyong the first entry ?
	 */
	font_name = g_slist_nth_data (font_private->names, 0);

	return font_name ? g_strdup (font_name) : NULL;
}

/* Return the size of a font in pixels.  If the source font's
 * XLFD spec is in something other than pixels (say points)
 * then the result is 0.  If the font is scalable, then the
 * result is 0 as well.  This function is only useful with
 * bitmap fonts. 
 */
static guint
font_get_size_in_pixels (const GdkFont *font)
{
	char *name;
	int size_in_pixels = 0;

	g_return_val_if_fail (font != NULL, 0);

	name = font_get_name (font);
	size_in_pixels = xlfd_string_get_nth_as_int (name, XLFD_SIZE_IN_PIXELS_INDEX);
	g_free (name);

	return (size_in_pixels != XLFD_INVALID_VALUE) ? size_in_pixels : 0;
}

static GdkFont *fixed_font = NULL;

static void
unref_fixed_font (void)
{
	gdk_font_unref (fixed_font);
}


/* FIXME bugzilla.eazel.com 7204:
 * Instead of picking fixed, we could create a temporary GtkStyle
 * and fetch the default font from there.  That way we wash our
 * hands on the matter and let gtk be the one to pick the fallback
 * font.  One tricky aspect of this strategy is where to put such
 * a call since this file contains on gdk_font stuff.  Perhaps
 * nautilus-gtk-extensions.[ch] ?
 */

/**
 * nautilus_gdk_font_get_fixed
 *
 * Returns: A fixed GdkFont that is guranteed to exist even in the
 *          most limited user environment.  The fixed font is
 *          useful in making code that deals with fonts simple by always
 *          having the ability to fallback to this font.
 *
 *          You should free the font with gdk_font_unref() when you are
 *          done with it.
 */
GdkFont *
nautilus_gdk_font_get_fixed (void)
{
	if (fixed_font == NULL) {
		/* Note to localizers: This is the name of the font used
		 * when no other font can be found. It must be guaranteed
		 * to exist, * even in the most limited user environment
		 */
		fixed_font = gdk_fontset_load (_("fixed"));
		g_assert (fixed_font != NULL);
		g_atexit (unref_fixed_font);
	}

	gdk_font_ref (fixed_font);
	return fixed_font;
}

/* Return a new string with just the nth XLFD string entry.  */
static char *
xlfd_string_get_nth (const char *xlfd_string,
		     guint n)
{
	NautilusStringList *list;
	char *result;
	
	g_return_val_if_fail (xlfd_string != NULL, NULL);
	g_return_val_if_fail (n <= XLFD_MAX_INDEX, NULL);
	
	list = nautilus_string_list_new_from_tokens (xlfd_string, "-", FALSE);

	if (nautilus_string_list_get_length (list) != (XLFD_MAX_INDEX + 1)) {
		nautilus_string_list_free (list);
		return NULL;
	}

	result = nautilus_string_list_nth (list, n);

	nautilus_string_list_free (list);

	return result;
}

/* Replace the nth entry of an XLFD string.  Return the new string. */
static char *
xlfd_string_replace_nth (const char *xlfd_string,
			 guint n,
			 const char *replace_string)
{
	char *new_xlfd_string;
	NautilusStringList *list;
	
	g_return_val_if_fail (xlfd_string != NULL, NULL);

	if (n > XLFD_MAX_INDEX) {
		return NULL;
	}

	list = nautilus_string_list_new_from_tokens (xlfd_string, "-", FALSE);

	if (nautilus_string_list_get_length (list) != (XLFD_MAX_INDEX + 1)) {
		nautilus_string_list_free (list);
		return NULL;
	}

	nautilus_string_list_modify_nth (list, n, replace_string);
	new_xlfd_string = nautilus_string_list_as_concatenated_string (list, "-");
	nautilus_string_list_free (list);

	return new_xlfd_string;
}

/* Return the nth entry as an integer.  Wildcards get the special
 * XLFD_WILDCARD_VALUE value. */
static int
xlfd_string_get_nth_as_int (const char *xlfd_string,
			    guint n)
{
	char *nth;
	int value;
	
	g_return_val_if_fail (xlfd_string != NULL, XLFD_INVALID_VALUE);

	if (n > XLFD_MAX_INDEX) {
		return XLFD_INVALID_VALUE;
	}
	
	nth = xlfd_string_get_nth (xlfd_string, n);

	if (nth == NULL) {
		return XLFD_INVALID_VALUE;
	}

	if (nautilus_str_is_equal (nth, "*")) {
		return XLFD_WILDCARD_VALUE;
	}
	
	if (!nautilus_eat_str_to_int (nth, &value)) {
		return XLFD_INVALID_VALUE;
	}

	return value;
}

/* Return whether the given xlfd string pattern represents a 
 * scalable non bitmap font */
static gboolean
xlfd_string_is_scalable_non_bitmap (const char *xlfd_string)
{
	int size_in_pixels;
	int size_in_points;
	int horizontal_resolution;
	int vertical_resolution;

	size_in_pixels = xlfd_string_get_nth_as_int (xlfd_string,
						     XLFD_SIZE_IN_PIXELS_INDEX);
	size_in_points = xlfd_string_get_nth_as_int (xlfd_string,
						     XLFD_SIZE_IN_POINTS_INDEX);
	horizontal_resolution = xlfd_string_get_nth_as_int (xlfd_string,
							    XLFD_HORIZONTAL_RESOLUTION_INDEX);
	vertical_resolution = xlfd_string_get_nth_as_int (xlfd_string,
							  XLFD_VERTICAL_RESOLUTION_INDEX);
	
	return size_in_pixels == 0
		&& size_in_points == 0
		&& horizontal_resolution == 0
		&& vertical_resolution == 0;
}

/* Return whether the given xlfd string pattern indicates a 
 * scalable non bitmap font */
static gboolean
xlfd_string_could_be_scalable_non_bitmap (const char *xlfd_string)
{
	char *temp[4];
	gboolean is_scalable_non_bitmap;
	const NautilusStringList *font_list;
	char *scalable_non_bitmap_match;
	
	g_return_val_if_fail (xlfd_string != NULL, FALSE);

	/* Replace all the size entries with wildcards */
	temp[0] = xlfd_string_replace_nth (xlfd_string, XLFD_SIZE_IN_PIXELS_INDEX, "*");
	temp[1] = xlfd_string_replace_nth (temp[0], XLFD_SIZE_IN_POINTS_INDEX, "*");
	temp[2] = xlfd_string_replace_nth (temp[1], XLFD_HORIZONTAL_RESOLUTION_INDEX, "*");
	temp[3] = xlfd_string_replace_nth (temp[2], XLFD_VERTICAL_RESOLUTION_INDEX, "*");
	
	font_list = font_list_fonts_cached (temp[3], NULL);

	/* Look for the entry that indicate this is a scalable non bitmap font */
	scalable_non_bitmap_match = nautilus_string_list_find_by_function (font_list,
									   font_entry_is_scalable_non_bitmap_test,
									   NULL);
	
	is_scalable_non_bitmap = scalable_non_bitmap_match != NULL;
	g_free (scalable_non_bitmap_match);

	g_free (temp[0]);
	g_free (temp[1]);
	g_free (temp[2]);
	g_free (temp[3]);

	return is_scalable_non_bitmap;
}

/* Allocate a new xlfd string with the given attrbitues. */
char *
nautilus_gdk_font_xlfd_string_new (const char *foundry,
				   const char *family,
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
				     family,
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

/* Test whether the given XLFD string has a bold weight */
static gboolean
font_entry_has_bold_weight_test (const NautilusStringList *string_list,
				 const char *string,
				 gpointer callback_data)
{
 	gboolean result;
	char *weight;

	g_return_val_if_fail (string_list != NULL, FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	weight = xlfd_string_get_nth (string, XLFD_WEIGHT_INDEX);

	/* FIXME bugzilla.eazel.com xxxx:
	 * Are there any other bold weights besides there 3 ?
	 */
 	result = 
 		nautilus_str_is_equal (weight, "bold")
 		|| nautilus_str_is_equal (weight, "demibold")
 		|| nautilus_str_is_equal (weight, "black");

 	g_free (weight);

 	return result;
}

/* Test whether the given XLFD string has an italic slant */
static gboolean
font_entry_has_italic_slant_test (const NautilusStringList *string_list,
				  const char *string,
				  gpointer callback_data)
{
 	gboolean result;
	char *slant;

	g_return_val_if_fail (string_list != NULL, FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	slant = xlfd_string_get_nth (string, XLFD_SLANT_INDEX);

	/* FIXME bugzilla.eazel.com xxxx:
	 * Are there any other italic slants besides these 2 ?
	 * i = italic
	 * o = oblique
	 */
 	result = 
 		nautilus_str_is_equal (slant, "i")
 		|| nautilus_str_is_equal (slant, "o");

 	g_free (slant);

 	return result;
}

/* Test whether the given XLFD string indicates a scalable non bitmap font */
static gboolean
font_entry_is_scalable_non_bitmap_test (const NautilusStringList *string_list,
					const char *string,
					gpointer callback_data)
{
	g_return_val_if_fail (string_list != NULL, FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	return xlfd_string_is_scalable_non_bitmap (string);
}

/* Compare the "size_in_points" entry of 2 XLFD strings */
static int
compare_xlfd_by_size_in_points (gconstpointer string_a,
				gconstpointer string_b)
{
	int a;
	int b;

	g_return_val_if_fail (string_a != NULL, 0);
	g_return_val_if_fail (string_b != NULL, 0);
	
	a = xlfd_string_get_nth_as_int (string_a, XLFD_SIZE_IN_POINTS_INDEX);
	b = xlfd_string_get_nth_as_int (string_b, XLFD_SIZE_IN_POINTS_INDEX);

 	if (a < b) {
 		return -1;
	}

 	if (a == b) {
 		return 0;
 	}

	return 1;
}

/* Compare the "size_in_pixels" entry of 2 XLFD strings */
static int
compare_xlfd_by_size_in_pixels (gconstpointer string_a,
				gconstpointer string_b)
{
	int a;
	int b;

	g_return_val_if_fail (string_a != NULL, 0);
	g_return_val_if_fail (string_b != NULL, 0);
	
	a = xlfd_string_get_nth_as_int (string_a, XLFD_SIZE_IN_PIXELS_INDEX);
	b = xlfd_string_get_nth_as_int (string_b, XLFD_SIZE_IN_PIXELS_INDEX);

 	if (a < b) {
 		return -1;
	}

 	if (a == b) {
 		return 0;
 	}

	return 1;
}

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

typedef enum {
	NAUTILUS_ELLIPSIZE_START,
	NAUTILUS_ELLIPSIZE_MIDDLE,
	NAUTILUS_ELLIPSIZE_END
} NautilusEllipsizeMode;

/* Testing string truncation is tough because we do not know what font/
 * font metrics to expect on a given system. To work around this we use
 * a substring of the original, measure it's length using the given font, 
 * add the length of the "..." string and use that for truncation.
 * The result should then be the substring prepended with a "..."
 */
static char *
nautilus_self_check_ellipsize (const char *string, const char *truncate_to_length_string, NautilusEllipsizeMode mode)
{
	GdkFont *font;
	int truncation_length;
	char *result;


	/* any old font will do */
	font = nautilus_gdk_font_get_fixed ();
	g_assert (font);

	/* measure the length we want to truncate to */
	truncation_length = gdk_string_width (font, truncate_to_length_string);
	truncation_length += gdk_string_width (font, ELLIPSIS);

	switch (mode) {
		case NAUTILUS_ELLIPSIZE_START:
			result = nautilus_string_ellipsize_start (string, font, truncation_length);
			break;
		case NAUTILUS_ELLIPSIZE_MIDDLE:
			result = nautilus_string_ellipsize_middle (string, font, truncation_length);
			break;
		case NAUTILUS_ELLIPSIZE_END:
			result = nautilus_string_ellipsize_end (string, font, truncation_length);
			break;
		default:
			g_assert_not_reached ();
			result = NULL;
			break;
	};
	
	gdk_font_unref (font);

	return result;
}

static char *
nautilus_self_check_ellipsize_start (const char *string, const char *truncate_to_length_string)
{
	return nautilus_self_check_ellipsize (string, truncate_to_length_string, NAUTILUS_ELLIPSIZE_START);
}

static char *
nautilus_self_check_ellipsize_middle (const char *string, const char *truncate_to_length_string)
{
	return nautilus_self_check_ellipsize (string, truncate_to_length_string, NAUTILUS_ELLIPSIZE_MIDDLE);
}

static char *
nautilus_self_check_ellipsize_end (const char *string, const char *truncate_to_length_string)
{
	return nautilus_self_check_ellipsize (string, truncate_to_length_string, NAUTILUS_ELLIPSIZE_END);
}

void
nautilus_self_check_gdk_font_extensions (void)
{
	GdkFont *font;

	/* used to test ellipsize routines */
	font = nautilus_gdk_font_get_fixed ();
	g_assert (font);

	/* nautilus_string_ellipsize_start */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "0012345678"), "012345678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "012345678"), "012345678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "45678"), "...45678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "5678"), "...5678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "678"), "...678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "78"), "...78");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_start ("012345678", "8"), "...8");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_start ("", font, 100), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_start ("test", font, 0), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_start ("test", font, gdk_string_width (font, "...") - 1), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_start ("test", font, gdk_string_width (font, "...")), "...");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "0123456789"), "012345678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "012345678"), "012345678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "01278"), "012...78");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "0178"), "01...78");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "018"), "01...8");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "08"), "0...8");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("012345678", "0"), "0...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "0123456789"), "0123456789");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "012789"), "012...789");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "01289"), "012...89");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "0189"), "01...89");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "019"), "01...9");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "09"), "0...9");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_middle ("0123456789", "0"), "0...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_middle ("", font, 100), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_middle ("test", font, 0), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_middle ("test", font, gdk_string_width (font, "...") - 1), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_middle ("test", font, gdk_string_width (font, "...")), "...");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "0123456789"), "012345678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "012345678"), "012345678");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "01234"), "01234...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "0123"), "0123...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "012"), "012...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "01"), "01...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_self_check_ellipsize_end ("012345678", "0"), "0...");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_end ("", font, 100), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_end ("test", font, 0), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_end ("test", font, gdk_string_width (font, "...") - 1), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_ellipsize_end ("test", font, gdk_string_width (font, "...")), "...");

	gdk_font_unref (font);

	/* xlfd_string_get_nth */
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), "x");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-", 1), "x");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x-", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 1), "1");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 2), "2");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", XLFD_WEIGHT_INDEX), "3");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", XLFD_SIZE_IN_PIXELS_INDEX), "7");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", XLFD_SIZE_IN_POINTS_INDEX), "8");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", XLFD_MAX_INDEX), "d");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_get_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", XLFD_MAX_INDEX - 1), "c");

	/* xlfd_string_get_nth_as_int */
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x-", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x-x", 1), XLFD_INVALID_VALUE);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", 1), 1);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", 2), 2);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", XLFD_WEIGHT_INDEX), 3);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", XLFD_SIZE_IN_PIXELS_INDEX), 7);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", XLFD_SIZE_IN_POINTS_INDEX), 8);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", XLFD_MAX_INDEX), 14);
	NAUTILUS_CHECK_INTEGER_RESULT (xlfd_string_get_nth_as_int ("-1-2-3-4-5-6-7-8-9-10-11-12-13-14", XLFD_MAX_INDEX - 1), 13);
	
	/* xlfd_string_replace_nth */
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("", 1, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-", 1, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b", 1, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-", 1, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c", 1, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 1, "X"), "-X-2-3-4-5-6-7-8-9-0-a-b-c-d");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 2, "X"), "-1-X-3-4-5-6-7-8-9-0-a-b-c-d");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 15, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 16, "X"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 14, "X"), "-1-2-3-4-5-6-7-8-9-0-a-b-c-X");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 13, "X"), "-1-2-3-4-5-6-7-8-9-0-a-b-X-d");
	NAUTILUS_CHECK_STRING_RESULT (xlfd_string_replace_nth ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d", 12, "X"), "-1-2-3-4-5-6-7-8-9-0-a-X-c-d");


	/* xlfd_string_is_scalable_non_bitmap */
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap (""), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-c"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-c-"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-*-*-*-*-*-*-*-*-*-*-*-*-*-*"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-*-*-*-*-*-*-0-0-0-0-*-*-*-*"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-*-*-*-*-*-*-0-0-0-0-*-*-*-*-"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-*-*-*-*-*-*-0-0-0-0-*-*-*-*-*"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-0-0-0-0-0-0-0-0-0-0-0-0-0-0"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-0-0-0-0-0-0-0-0-0-0-0-0-0-0-"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d-e"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d-e-"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (xlfd_string_is_scalable_non_bitmap ("-1-2-3-4-5-6-7-8-9-0-a-b-c-d-e-f"), FALSE);
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
