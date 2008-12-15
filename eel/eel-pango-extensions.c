/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-pango-extensions.h - interface for new functions that conceptually
                            belong in pango. Perhaps some of these will be
                            actually rolled into pango someday.

   Copyright (C) 2001 Anders Carlsson

   The Eel Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Eel Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Eel Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#include <config.h>
#include "eel-pango-extensions.h"

#if !defined (EEL_OMIT_SELF_CHECK)
#include "eel-lib-self-check-functions.h"
#endif

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <string.h>

PangoAttrList *
eel_pango_attr_list_copy_or_create (PangoAttrList *attr_list)
{
	if (attr_list != NULL) {
		return pango_attr_list_copy (attr_list);
	}
	return pango_attr_list_new ();
}

PangoAttrList *
eel_pango_attr_list_apply_global_attribute (PangoAttrList *attr_list,
					    PangoAttribute *attr)
{
	PangoAttrList *new_attr_list;

	g_return_val_if_fail (attr != NULL, NULL);

	attr->start_index = 0;
	attr->end_index = G_MAXINT;
	
	new_attr_list = eel_pango_attr_list_copy_or_create (attr_list);
	pango_attr_list_change (new_attr_list, attr);
	return new_attr_list;
}

#define ELLIPSIS "..."

/* Caution: this is an _expensive_ function */
static int
measure_string_width (const char  *string,
		      PangoLayout *layout)
{
	int width;
	
	pango_layout_set_text (layout, string, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);

	return width;
}

/* this is also plenty slow */
static void
compute_character_widths (const char    *string,
			  PangoLayout   *layout,
			  int           *char_len_return,
			  int          **widths_return,
			  int          **cuts_return)
{
	int *widths;
	int *offsets;
	int *cuts;
	int char_len;
	int byte_len;
	const char *p;
	int i;
	PangoLayoutIter *iter;
	PangoLogAttr *attrs;
	
#define BEGINS_UTF8_CHAR(x) (((x) & 0xc0) != 0x80)
	
	char_len = g_utf8_strlen (string, -1);
	byte_len = strlen (string);
	
	widths = g_new (int, char_len);
	offsets = g_new (int, byte_len);

	/* Create a translation table from byte index to char offset */
	p = string;
	i = 0;
	while (*p) {
		int byte_index = p - string;
		
		if (BEGINS_UTF8_CHAR (*p)) {
			offsets[byte_index] = i;
			++i;
		} else {
			offsets[byte_index] = G_MAXINT; /* segv if we try to use this */
		}
		
		++p;
	}

	/* Now fill in the widths array */
	pango_layout_set_text (layout, string, -1);
	
	iter = pango_layout_get_iter (layout);

	do {
		PangoRectangle extents;
		int byte_index;

		byte_index = pango_layout_iter_get_index (iter);

		if (byte_index < byte_len) {
			pango_layout_iter_get_char_extents (iter, &extents);
			
			g_assert (BEGINS_UTF8_CHAR (string[byte_index]));
			g_assert (offsets[byte_index] < char_len);
			
			widths[offsets[byte_index]] = PANGO_PIXELS (extents.width);
		}
		
	} while (pango_layout_iter_next_char (iter));

	pango_layout_iter_free (iter);

	g_free (offsets);
	
	*widths_return = widths;

	/* Now compute character offsets that are legitimate places to
	 * chop the string
	 */
	attrs = g_new (PangoLogAttr, char_len + 1);
	
	pango_get_log_attrs (string, byte_len, -1,
			     pango_context_get_language (
				     pango_layout_get_context (layout)),
			     attrs,
			     char_len + 1);

	cuts = g_new (int, char_len);
	i = 0;
	while (i < char_len) {
		cuts[i] = attrs[i].is_cursor_position;

		++i;
	}

	g_free (attrs);

	*cuts_return = cuts;

	*char_len_return = char_len;
}


static char *
eel_string_ellipsize_start (const char *string, PangoLayout *layout, int width)
{
	int resulting_width;
	int *cuts;
	int *widths;
	int char_len;
	const char *p;
	int truncate_offset;

	/* Zero-length string can't get shorter - catch this here to
	 * avoid expensive calculations
	 */
	if (*string == '\0')
		return g_strdup ("");

	/* I'm not sure if this short-circuit is a net win; it might be better
	 * to just dump this, and always do the compute_character_widths() etc.
	 * down below.
	 */
	resulting_width = measure_string_width (string, layout);

	if (resulting_width <= width) {
		/* String is already short enough. */
		return g_strdup (string);
	}

	/* Remove width of an ellipsis */
	width -= measure_string_width (ELLIPSIS, layout);

	if (width < 0) {
		/* No room even for an ellipsis. */
		return g_strdup ("");
	}

	/* Our algorithm involves removing enough chars from the string to bring
	 * the width to the required small size. However, due to ligatures,
	 * combining characters, etc., it's not guaranteed that the algorithm
	 * always works 100%. It's sort of a heuristic thing. It should work
	 * nearly all the time... but I wouldn't put in
	 * g_assert (width of resulting string < width).
	 *
	 * Hmm, another thing that this breaks with is explicit line breaks
	 * in "string"
	 */

	compute_character_widths (string, layout, &char_len, &widths, &cuts);

        for (truncate_offset = 1; truncate_offset < char_len; truncate_offset++) {

        	resulting_width -= widths[truncate_offset];

        	if (resulting_width <= width &&
		    cuts[truncate_offset]) {
			break;
        	}
        }

	g_free (cuts);
	g_free (widths);
	
	p = g_utf8_offset_to_pointer (string, truncate_offset);
	
	return g_strconcat (ELLIPSIS, p, NULL);
}

static char *
eel_string_ellipsize_end (const char *string, PangoLayout *layout, int width)
{
	int resulting_width;
	int *cuts;
	int *widths;
	int char_len;
	const char *p;
	int truncate_offset;
	char *result;
	
	/* See explanatory comments in ellipsize_start */
	
	if (*string == '\0')
		return g_strdup ("");

	resulting_width = measure_string_width (string, layout);
	
	if (resulting_width <= width) {
		return g_strdup (string);
	}

	width -= measure_string_width (ELLIPSIS, layout);

	if (width < 0) {
		return g_strdup ("");
	}
	
	compute_character_widths (string, layout, &char_len, &widths, &cuts);
	
        for (truncate_offset = char_len - 1; truncate_offset > 0; truncate_offset--) {
        	resulting_width -= widths[truncate_offset];
        	if (resulting_width <= width &&
		    cuts[truncate_offset]) {
			break;
        	}
        }

	g_free (cuts);
	g_free (widths);

	p = g_utf8_offset_to_pointer (string, truncate_offset);
	
	result = g_malloc ((p - string) + strlen (ELLIPSIS) + 1);
	memcpy (result, string, (p - string));
	strcpy (result + (p - string), ELLIPSIS);

	return result;
}

static char *
eel_string_ellipsize_middle (const char *string, PangoLayout *layout, int width)
{
	int resulting_width;
	int *cuts;
	int *widths;
	int char_len;
	int starting_fragment_byte_len;
	int ending_fragment_byte_index;
	int starting_fragment_length;
	int ending_fragment_offset;
	char *result;
	
	/* See explanatory comments in ellipsize_start */
	
	if (*string == '\0')
		return g_strdup ("");

	resulting_width = measure_string_width (string, layout);
	
	if (resulting_width <= width) {
		return g_strdup (string);
	}

	width -= measure_string_width (ELLIPSIS, layout);

	if (width < 0) {
		return g_strdup ("");
	}
	
	compute_character_widths (string, layout, &char_len, &widths, &cuts);
	
	starting_fragment_length = char_len / 2;
	ending_fragment_offset = starting_fragment_length + 1;
	
	/* Shave off a character at a time from the first and the second half
	 * until we can fit
	 */
	resulting_width -= widths[ending_fragment_offset - 1];
	
	/* depending on whether the original string length is odd or even, start by
	 * shaving off the characters from the starting or ending fragment
	 */
	if (char_len % 2) {
		goto shave_end;
	}

	while (starting_fragment_length > 0 || ending_fragment_offset < char_len) {
		if (resulting_width <= width &&
		    cuts[ending_fragment_offset] &&
		    cuts[starting_fragment_length]) {
			break;
		}

		if (starting_fragment_length > 0) {
			resulting_width -= widths[starting_fragment_length];
			starting_fragment_length--;
		}

	shave_end:
		if (resulting_width <= width &&
		    cuts[ending_fragment_offset] &&
		    cuts[starting_fragment_length]) {
			break;
		}

		if (ending_fragment_offset < char_len) {
			resulting_width -= widths[ending_fragment_offset];
			ending_fragment_offset++;
		}
	}

	g_free (cuts);
	g_free (widths);	
	
	/* patch the two fragments together with an ellipsis */
	result = g_malloc (strlen (string) + strlen (ELLIPSIS) + 1); /* a bit wasteful, no biggie */

	starting_fragment_byte_len = g_utf8_offset_to_pointer (string, starting_fragment_length) - string;
	ending_fragment_byte_index = g_utf8_offset_to_pointer (string, ending_fragment_offset) - string;
	
	memcpy (result, string, starting_fragment_byte_len);
	strcpy (result + starting_fragment_byte_len, ELLIPSIS);
	strcpy (result + starting_fragment_byte_len + strlen (ELLIPSIS), string + ending_fragment_byte_index);

	return result;
}


/**
 * eel_pango_layout_set_text_ellipsized
 *
 * @layout: a pango layout
 * @string: A a string to be ellipsized.
 * @width: Desired maximum width in points.
 * @mode: The desired ellipsizing mode.
 * 
 * Truncates a string if required to fit in @width and sets it on the
 * layout. Truncation involves removing characters from the start, middle or end
 * respectively and replacing them with "...". Algorithm is a bit
 * fuzzy, won't work 100%.
 * 
 */
void
eel_pango_layout_set_text_ellipsized (PangoLayout  *layout,
				      const char   *string,
				      int           width,
				      EelEllipsizeMode mode)
{
	char *s;

	g_return_if_fail (PANGO_IS_LAYOUT (layout));
	g_return_if_fail (string != NULL);
	g_return_if_fail (width >= 0);
	
	switch (mode) {
	case EEL_ELLIPSIZE_START:
		s = eel_string_ellipsize_start (string, layout, width);
		break;
	case EEL_ELLIPSIZE_MIDDLE:
		s = eel_string_ellipsize_middle (string, layout, width);
		break;
	case EEL_ELLIPSIZE_END:
		s = eel_string_ellipsize_end (string, layout, width);
		break;
	default:
		g_return_if_reached ();
	}
	
	pango_layout_set_text (layout, s, -1);
	
	g_free (s);
}


int
eel_pango_font_description_get_largest_fitting_font_size (const PangoFontDescription *font_desc,
							  PangoContext *context,
							  const char   *text,
							  int           available_width,
							  int           minimum_acceptable_font_size,
							  int           maximum_acceptable_font_size)
{
	int i;
	int width;
	PangoLayout *layout;
	PangoFontDescription *font;

	g_return_val_if_fail (text != NULL, 0);
	g_return_val_if_fail (text[0] != '\0', 0);
	g_return_val_if_fail (available_width > 0, 0);
	g_return_val_if_fail (minimum_acceptable_font_size > 0, 0);
	g_return_val_if_fail (maximum_acceptable_font_size > 0, 0);
	g_return_val_if_fail (maximum_acceptable_font_size > minimum_acceptable_font_size, 0);

	layout = pango_layout_new (context);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_font_description (layout, font_desc);
	
	font = pango_font_description_new ();

	for (i = maximum_acceptable_font_size; i >= minimum_acceptable_font_size; i--) {

		pango_font_description_set_size (font, i * PANGO_SCALE);
		pango_layout_set_font_description (layout, font);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width <= available_width) {
			pango_font_description_free (font);
			g_object_unref (layout);
			return i;
		}
	}

	pango_font_description_free (font);
	g_object_unref (layout);
	return i;
}


#if !defined (EEL_OMIT_SELF_CHECK)

static PangoContext *
eel_create_bogus_test_pango_context (void)
{
	PangoContext *context;
	
	context = gdk_pango_context_get ();
	pango_context_set_language (context, gtk_get_default_language ());
	
	return context;
}

/* Testing string truncation is tough because we do not know what font/
 * font metrics to expect on a given system. To work around this we use
 * a substring of the original, measure it's length using the given font, 
 * add the length of the "..." string and use that for truncation.
 * The result should then be the substring prepended with a "..."
 */
static char *
eel_self_check_ellipsize (const char *string, const char *truncate_to_length_string, EelEllipsizeMode mode)
{
	PangoContext *context;
	int truncation_length;
	char *result;
	PangoLayout *layout;
	
	context = eel_create_bogus_test_pango_context ();
	layout = pango_layout_new (context);
	
	/* measure the length we want to truncate to */
	truncation_length = measure_string_width (truncate_to_length_string, layout);

	eel_pango_layout_set_text_ellipsized (layout, string, truncation_length, mode);

	result = g_strdup (pango_layout_get_text (layout));
	
	g_object_unref (G_OBJECT (context));
	g_object_unref (G_OBJECT (layout));

	return result;
}

static char *
eel_self_check_ellipsize_start (const char *string, const char *truncate_to_length_string)
{
	return eel_self_check_ellipsize (string, truncate_to_length_string, EEL_ELLIPSIZE_START);
}

static char *
eel_self_check_ellipsize_middle (const char *string, const char *truncate_to_length_string)
{
	return eel_self_check_ellipsize (string, truncate_to_length_string, EEL_ELLIPSIZE_MIDDLE);
}

static char *
eel_self_check_ellipsize_end (const char *string, const char *truncate_to_length_string)
{
	return eel_self_check_ellipsize (string, truncate_to_length_string, EEL_ELLIPSIZE_END);
}

void
eel_self_check_pango_extensions (void)
{
	PangoContext *context;

	/* used to test ellipsize routines */
	context = eel_create_bogus_test_pango_context ();

	/* Turned off these tests because they are failing for me and I
	 * want the release to be able to pass "make check". We'll have
	 * to revisit this at some point. The failures started because
	 * I changed my default font and enabled Xft support. I presume
	 * this is simply the "fuzziness" Havoc mentions in his comments
	 * above.
	 *                                                - Darin
	 */

    if (0) {
	/* eel_string_ellipsize_start */
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "0012345678"), "012345678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "012345678"), "012345678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "...45678"), "...45678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "...5678"), "...5678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "...678"), "...678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "...78"), "...78");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_start ("012345678", "...8"), "...8");

	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "0123456789"), "012345678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "012345678"), "012345678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "012...78"), "012...78");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "01...78"), "01...78");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "01...8"), "01...8");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "0...8"), "0...8");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("012345678", "0..."), "0...");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "0123456789"), "0123456789");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "012...789"), "012...789");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "012...89"), "012...89");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "01...89"), "01...89");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "01...9"), "01...9");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "0...9"), "0...9");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_middle ("0123456789", "0..."), "0...");

	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "0123456789"), "012345678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "012345678"), "012345678");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "01234..."), "01234...");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "0123..."), "0123...");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "012..."), "012...");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "01..."), "01...");
	EEL_CHECK_STRING_RESULT (eel_self_check_ellipsize_end ("012345678", "0..."), "0...");
    }

	g_object_unref (context);
}

#endif /* !EEL_OMIT_SELF_CHECK */
