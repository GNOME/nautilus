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
#define XLFD_SIZE_IN_PIXELS_INDEX		7
#define XLFD_SIZE_IN_POINTS_INDEX		8
#define XLFD_HORIZONTAL_RESOLUTION_INDEX	9
#define XLFD_VERTICAL_RESOLUTION_INDEX		10
#define XLFD_MAX_INDEX				14

/* Indicates an invalid xlfd string */
#define XLFD_INVALID_VALUE			-1

/* Indicates a xlfd wildcard: * */
#define XLFD_WILDCARD_VALUE			-2

/* Font functions that could be public */
static NautilusStringList *nautilus_gdk_font_list_fonts             (const char               *pattern);
static char *              nautilus_gdk_font_get_name               (const GdkFont            *font);
static guint               nautilus_gdk_font_get_size_in_pixels               (const GdkFont            *font);

/* XLFD string operations */
static char *              xlfd_string_get_nth                      (const char               *xlfd_string,
								     guint                     n);
static char *              xlfd_string_replace_nth                  (const char               *xlfd_string,
								     guint                     n,
								     const char               *replace_string);
static int                 xlfd_string_get_nth_as_int               (const char               *xlfd_string,
								     guint                     n);
static gboolean            xlfd_string_is_scalable_non_bitmap       (const char               *xlfd_string);
static gboolean            xlfd_string_could_be_scalable_non_bitmap (const char               *xlfd_string);

/* Test functions for searching font lists */
static gboolean            font_entry_has_bold_weight_test          (const NautilusStringList *string_list,
								     const char               *string,
								     gpointer                  callback_data);
static gboolean            font_entry_is_scalable_non_bitmap_test   (const NautilusStringList *string_list,
								     const char               *string,
								     gpointer                  callback_data);
/* Comparison functions for sorting fonts lists */
static int                 compare_xlfd_by_size_in_points           (gconstpointer             string_a,
								     gconstpointer             string_b);
static int                 compare_xlfd_by_size_in_pixels           (gconstpointer             string_a,
								     gconstpointer             string_b);

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
	char *weight_pattern;
	GdkFont *result = NULL;
	char *pattern_match;
	NautilusStringList *font_list;

	name = nautilus_gdk_font_get_name (font);

	/* Replace the weight with a wildard */
	weight_pattern = xlfd_string_replace_nth (name, XLFD_WEIGHT_INDEX, "*");

	font_list = nautilus_gdk_font_list_fonts (weight_pattern);
	
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
		g_assert (result != NULL);

		g_free (bold_name);
		g_free (weight_name);
	} else {
		/* If no font was found, return the source font */
		gdk_font_ref ((GdkFont *) font);
		result = (GdkFont *) font;
	}

	nautilus_string_list_free (font_list);	
	g_free (pattern_match);
	g_free (weight_pattern);
	g_free (name);

	return result;
}

/* Return a scalable font matching the given target size */
static GdkFont *
font_scalable_get_by_size (const char *xlfd_string,
			   guint target_size,
			   guint index)
{
	GdkFont *larger_font;
	char *larger_size_string;
	char *larger_font_name;
	
	g_return_val_if_fail (xlfd_string != NULL, NULL);
	g_return_val_if_fail (target_size > 0, NULL);
	g_return_val_if_fail (index == XLFD_SIZE_IN_POINTS_INDEX || index == XLFD_SIZE_IN_PIXELS_INDEX, NULL);

	larger_size_string = g_strdup_printf ("%d", target_size);
	
	larger_font_name = xlfd_string_replace_nth (xlfd_string,
						    index,
						    larger_size_string);

	larger_font = gdk_fontset_load (larger_font_name);
	g_assert (larger_font != NULL);
	
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
	NautilusStringList *font_list;
	
	g_return_val_if_fail (xlfd_string != NULL, NULL);
	g_return_val_if_fail (target_size > 0, NULL);
	g_return_val_if_fail (index == XLFD_SIZE_IN_POINTS_INDEX || index == XLFD_SIZE_IN_PIXELS_INDEX, NULL);
	g_return_val_if_fail (compare_function != NULL, NULL);
	
 	larger_pattern_xlfd = xlfd_string_replace_nth (xlfd_string, index, "*");

	/* Make a query */
	font_list = nautilus_gdk_font_list_fonts (larger_pattern_xlfd);

	/* Sort the fonts by size */
	nautilus_string_list_sort_by_function (font_list, compare_function);
	
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
		/* Look for the closes matching font */
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

		g_assert (found_size > 0);
		
		size_string = g_strdup_printf ("%d", found_size);
		larger_font_name = xlfd_string_replace_nth (xlfd_string, index, size_string);
		g_free (size_string);
		larger_font = gdk_fontset_load (larger_font_name);
		g_free (larger_font_name);
		
	}
	
	nautilus_string_list_free (font_list);
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

	name = nautilus_gdk_font_get_name (font);

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
		gdk_font_ref ((GdkFont *) font);
		result = (GdkFont *)font;
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

	
	tokenized_string = nautilus_string_list_new_from_tokens (text, "\n", FALSE);
	longest_string = nautilus_string_list_get_longest_string (tokenized_string);
	g_assert (longest_string != NULL);
	nautilus_string_list_free (tokenized_string);
	longest_string_length = strlen (longest_string);

	/* Make sure the font was specified in pixels */
	if (nautilus_gdk_font_get_size_in_pixels (font) > 0) {
		int candidate_size;
		char *font_name;
		gboolean done;
		
		font_name = nautilus_gdk_font_get_name (font);
		
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
			g_assert (candidate_font != NULL);

			candidate_width = gdk_string_width (candidate_font, longest_string);

			if ((candidate_width <= available_width) 
			    || (candidate_size <= minimum_acceptable_font_size)) {
				done = TRUE;
				largest_fitting_font = candidate_font;
			} else {
				gdk_font_unref (candidate_font);
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

	if (gdk_string_width (font, string) <= (int) width) {
		/* String is already short enough. */
		return g_strdup (string);
	}
	
	/* Account for the width of the ellipsis. */
	width -= gdk_string_width (font, "...");
	

	if (width < 0) {
		/* No room even for a an ellipsis. */
		return g_strdup ("");
	}

	/* We could have the following optimization here:
	 * check if the desired width and original width are considerably different,
	 * if so, use a binary stride to figure out the resulting string truncation
	 * offset.
	 * For now we assume that we are only truncating by a small number of 
	 * characters, in which a linear scan is faster
	 */
        for (truncate_offset = 0; ; truncate_offset++) {
        	if (string[truncate_offset] == '\0') {
			break;
        	}
        	
        	if (gdk_string_width (font, string + truncate_offset) <= (int) width) {
			break;
        	}
        }

	return g_strdup_printf ("...%s", string + truncate_offset);
}

/* Private font things */

/* A wrapper for XListFonts() */
static NautilusStringList *
nautilus_gdk_font_list_fonts (const char *pattern)
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

/* Return the font name - an xlfd string used by Gdk to allocate the font */
static char *
nautilus_gdk_font_get_name (const GdkFont *font)
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
nautilus_gdk_font_get_size_in_pixels (const GdkFont *font)
{
	char *name;
	int size_in_pixels = 0;

	g_return_val_if_fail (font != NULL, 0);

	name = nautilus_gdk_font_get_name (font);

	size_in_pixels = xlfd_string_get_nth_as_int (name, XLFD_SIZE_IN_PIXELS_INDEX);

	g_free (name);

	return size_in_pixels;
}

static GdkFont *fixed_font = NULL;

static void
unref_fixed_font (void)
{
	gdk_font_unref (fixed_font);
}

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
	NautilusStringList *font_list;
	char *scalable_non_bitmap_match;
	
	g_return_val_if_fail (xlfd_string != NULL, FALSE);

	/* Replace all the size entries with wildcards */
	temp[0] = xlfd_string_replace_nth (xlfd_string, XLFD_SIZE_IN_PIXELS_INDEX, "*");
	temp[1] = xlfd_string_replace_nth (temp[0], XLFD_SIZE_IN_POINTS_INDEX, "*");
	temp[2] = xlfd_string_replace_nth (temp[1], XLFD_HORIZONTAL_RESOLUTION_INDEX, "*");
	temp[3] = xlfd_string_replace_nth (temp[2], XLFD_VERTICAL_RESOLUTION_INDEX, "*");
	
	font_list = nautilus_gdk_font_list_fonts (temp[3]);

	/* Look for the entry that indicate this is a scalable non bitmap font */
	scalable_non_bitmap_match = nautilus_string_list_find_by_function (font_list,
									   font_entry_is_scalable_non_bitmap_test,
									   NULL);
	
	is_scalable_non_bitmap = scalable_non_bitmap_match != NULL;
	g_free (scalable_non_bitmap_match);

	nautilus_string_list_free (font_list);
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
/* Testing string truncation is tough because we do not know what font/
 * font metrics to expect on a given system. To work around this we use
 * a substring of the original, measure it's length using the given font, 
 * add the length of the "..." string and use that for truncation.
 * The result should then be the substring prepended with a "..."
 */
static char *
nautilus_self_check_ellipsize_start (const char *string, const char *truncate_to_length_string)
{
	GdkFont *font;
	int truncation_length;
	char *result;

	/* any old font will do */
	font = nautilus_gdk_font_get_fixed ();
	g_assert (font);

	/* measure the length we want to truncate to */
	truncation_length = gdk_string_width (font, truncate_to_length_string);
	truncation_length += gdk_string_width (font, "...");

	result = nautilus_string_ellipsize_start (string, font, truncation_length);
	
	gdk_font_unref (font);

	return result;
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
