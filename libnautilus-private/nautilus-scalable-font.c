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
#include "nautilus-scalable-font.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-background.h"
#include "nautilus-string.h"
#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-string-map.h"

#include <librsvg/rsvg-ft.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_affine.h>
#include <librsvg/art_render.h>
#include <librsvg/art_render_mask.h>

#include <stdio.h>
#include <libgnome/gnome-util.h>
#include <ctype.h>

/* FontEntry */
typedef struct {
	char			*weight;
	char			*slant;
	char			*set_width;
	RsvgFTFontHandle	font_handle;
	char			*path;
} FontEntry;

/* FontFamilyEntry */
typedef struct {
	char			*family;
	GList			*fonts;
} FontFamilyEntry;


/* Detail member struct */
struct _NautilusScalableFontDetail
{
	RsvgFTFontHandle	font_handle;
};

/* Global things */
static GHashTable		*global_font_family_table = NULL;
static RsvgFTCtx		*global_rsvg_ft_context = NULL;
static NautilusScalableFont	*global_default_font = NULL;
static NautilusStringMap	*global_family_string_map = NULL;

static const RsvgFTFontHandle UNDEFINED_FONT_HANDLE = -1;

/* GtkObjectClass methods */
static void nautilus_scalable_font_initialize_class   (NautilusScalableFontClass *font_class);
static void nautilus_scalable_font_initialize         (NautilusScalableFont      *font);
static void nautilus_scalable_font_destroy            (GtkObject                 *object);
static void initialize_global_stuff_if_needed         (void);


/* 'atexit' destructors for global stuff */
static void default_font_at_exit_destructor           (void);
static void font_family_table_at_exit_destructor      (void);
static void font_family_string_map_at_exit_destructor (void);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusScalableFont, nautilus_scalable_font, GTK_TYPE_OBJECT)

/* Class init methods */
static void
nautilus_scalable_font_initialize_class (NautilusScalableFontClass *font_class)
{
	GtkObjectClass		*object_class = GTK_OBJECT_CLASS (font_class);

	initialize_global_stuff_if_needed ();

	/* GtkObjectClass */
	object_class->destroy = nautilus_scalable_font_destroy;
}

void
nautilus_scalable_font_initialize (NautilusScalableFont *font)
{
	font->detail = g_new (NautilusScalableFontDetail, 1);

	font->detail->font_handle = UNDEFINED_FONT_HANDLE;
}

/* GtkObjectClass methods */
static void
nautilus_scalable_font_destroy (GtkObject *object)
{
 	NautilusScalableFont *font;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (object));

	font = NAUTILUS_SCALABLE_FONT (object);

	g_free (font->detail);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static char *
file_as_string (const char *file_name)
{
	struct stat stat_info;
	FILE	    *stream;
	char	    *result;
	size_t	    num_read;

	g_return_val_if_fail (file_name != NULL, NULL);
	g_return_val_if_fail (g_file_exists (file_name), NULL);

	if (stat (file_name, &stat_info) != 0) {
		return NULL;
	}

	if (stat_info.st_size == 0) {
		return NULL;
	}
	
	stream = fopen (file_name, "r");
	
	if (!stream) {
		return NULL;
	}

	result = g_malloc (sizeof (char) * stat_info.st_size + 1);

	num_read = fread (result, sizeof (char), stat_info.st_size, stream);

	fclose (stream);

	if (num_read != stat_info.st_size) {
		g_free (result);
		return NULL;
	}

	result[stat_info.st_size] = '\0';

	return result;
}

static gboolean
parse_font_description_file (const char		*directory, 
			     NautilusStringList **font_pfb_list_out,
			     NautilusStringList **font_xfld_list_out)
{
	char			*fonts_dir_path;
	char			*fonts_dir_content;
	NautilusStringList	*tokenized_list;
	guint			i;
	gint			count;

	g_return_val_if_fail (directory != NULL, FALSE);
	g_return_val_if_fail (g_file_exists (directory), FALSE);
	g_return_val_if_fail (font_pfb_list_out != NULL, FALSE);
	g_return_val_if_fail (font_xfld_list_out != NULL, FALSE);

	*font_pfb_list_out = NULL;
	*font_xfld_list_out = NULL;

	fonts_dir_path = g_strdup_printf ("%s/%s", directory, "fonts.dir");
	fonts_dir_content = file_as_string (fonts_dir_path);
	g_free (fonts_dir_path);
	
	if (fonts_dir_content == NULL) {
		return FALSE;
	}

	tokenized_list = nautilus_string_list_new_from_tokens (fonts_dir_content, "\n", FALSE);
	g_free (fonts_dir_content);

	if (tokenized_list == NULL) {
		return FALSE;
	}

	if (nautilus_string_list_get_length (tokenized_list) <= 1) {
		nautilus_string_list_free (tokenized_list);
		return FALSE;
	}

	if (!nautilus_eat_str_to_int (nautilus_string_list_nth (tokenized_list, 0), &count)) {
		return FALSE;
	}

	*font_pfb_list_out = nautilus_string_list_new (FALSE);
	*font_xfld_list_out = nautilus_string_list_new (FALSE);

	for (i = 0; i < count; i++) {
		char *line = nautilus_string_list_nth (tokenized_list, i + 1);

		if (line != NULL) {
			char *delimeter;
			
			/* Look for the delimiting space */
			delimeter = strstr (line, " ");
			if (delimeter != NULL) {
				
				char *font_pfb;
				char *font_pfb_path;
				guint pfb_length;

				pfb_length = delimeter - line;
				font_pfb = g_malloc (sizeof (char) * pfb_length + 1);
				strncpy (font_pfb, line, pfb_length);
				font_pfb[pfb_length] = '\0';
				
				font_pfb_path = g_strdup_printf ("%s/%s", directory, font_pfb);

				/* Make sure the pfb file exists */
				if (g_file_exists (font_pfb_path)) {
					while (isspace (*delimeter)) {
						delimeter++;
					}

					nautilus_string_list_insert (*font_pfb_list_out, font_pfb_path);
					nautilus_string_list_insert (*font_xfld_list_out, delimeter);
				}
				
				g_free (font_pfb);
				g_free (font_pfb_path);
			}
			
			g_free (line);
		}
	}

	nautilus_string_list_free (tokenized_list);

	return TRUE;
}

/* 
 * FontEntry things
 */
static FontEntry*
font_entry_new (const char *weight, 
		const char *slant, 
		const char *set_width, 
		const char *path)
{
	FontEntry *entry;
	
	g_return_val_if_fail (weight != NULL, NULL);
	g_return_val_if_fail (slant != NULL, NULL);
	g_return_val_if_fail (set_width != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	entry = g_new (FontEntry, 1);
	
	entry->weight = g_strdup (weight);
	entry->slant = g_strdup (slant);
	entry->set_width = g_strdup (set_width);
	entry->path = g_strdup (path);
	entry->font_handle = UNDEFINED_FONT_HANDLE;

	return entry;
}

static void
font_entry_free (FontEntry* entry)
{
	g_return_if_fail (entry != NULL);

	g_free (entry->weight);
	g_free (entry->slant);
	g_free (entry->set_width);
	g_free (entry->path);

	/* These fonts arent refcounted because they are catched internally by librsvg */
	entry->font_handle = UNDEFINED_FONT_HANDLE;
	
	g_free (entry);
}

/* 
 * FontFamilyEntry things
 */
static FontFamilyEntry*
font_family_new (const char *family)
{
	FontFamilyEntry *entry;
	
	g_return_val_if_fail (family != NULL, NULL);

	entry = g_new (FontFamilyEntry, 1);
	
	entry->family = g_strdup (family);
	g_strdown (entry->family);
	entry->fonts = NULL;

	return entry;
}

static void
free_font_entry (gpointer data, gpointer user_data)
{
	font_entry_free ((FontEntry*) data);
}

static void
font_family_free (FontFamilyEntry* entry)
{
	g_return_if_fail (entry != NULL);

	g_free (entry->family);

	nautilus_g_list_free_deep_custom (entry->fonts, free_font_entry, NULL);
	entry->fonts = NULL;

	g_free (entry);
}

static void
font_family_insert_font (FontFamilyEntry *family_entry, FontEntry *font_entry)
{
	g_return_if_fail (family_entry != NULL);
	g_return_if_fail (font_entry != NULL);

	family_entry->fonts = g_list_append (family_entry->fonts, (gpointer) font_entry);
}

#define EQUAL 0
#define NOT_EQUAL 1

static gint
font_compare (gconstpointer a, gconstpointer b)
{
	FontEntry *font_entry_a = (FontEntry *) a;
	FontEntry *font_entry_b = (FontEntry *) b;

	g_return_val_if_fail (font_entry_a != NULL, NOT_EQUAL);
	g_return_val_if_fail (font_entry_b != NULL, NOT_EQUAL);

	g_return_val_if_fail (font_entry_a->weight != NULL, NOT_EQUAL);
	g_return_val_if_fail (font_entry_a->slant != NULL, NOT_EQUAL);
	g_return_val_if_fail (font_entry_a->set_width != NULL, NOT_EQUAL);
	g_return_val_if_fail (font_entry_b->weight != NULL, NOT_EQUAL);
	g_return_val_if_fail (font_entry_b->slant != NULL, NOT_EQUAL);
	g_return_val_if_fail (font_entry_b->set_width != NULL, NOT_EQUAL);

	return (nautilus_str_is_equal (font_entry_a->weight, font_entry_b->weight)
		&& nautilus_str_is_equal (font_entry_a->slant, font_entry_b->slant)
		&& nautilus_str_is_equal (font_entry_a->set_width, font_entry_b->set_width)) ? EQUAL : NOT_EQUAL;
}

static FontEntry*
font_family_find_font (const FontFamilyEntry	*family_entry, 
		       const char		*weight, 
		       const char		*slant, 
		       const char		*set_width)
{
	FontEntry fake_font_entry;

	GList *node;

	g_return_val_if_fail (family_entry != NULL, NULL);
	g_return_val_if_fail (weight != NULL, NULL);
	g_return_val_if_fail (slant != NULL, NULL);
	g_return_val_if_fail (set_width != NULL, NULL);

	fake_font_entry.weight = (char *) weight;
	fake_font_entry.slant = (char *) slant;
	fake_font_entry.set_width = (char *) set_width;

	node = g_list_find_custom (family_entry->fonts, &fake_font_entry, font_compare);

	return node ? node->data : NULL;
}

static FontFamilyEntry *
font_family_lookup (GHashTable *font_family_table, const char *family)
{
	gpointer value;
	
	g_return_val_if_fail (font_family_table != NULL, NULL);
	g_return_val_if_fail (family != NULL, NULL);
	
	value = g_hash_table_lookup (font_family_table, (gconstpointer) family);
	
	return (FontFamilyEntry *) value;
}

static FontFamilyEntry *
font_family_lookup_with_insertion (GHashTable *font_family_table, const char *family)
{
	FontFamilyEntry *entry;
	
	g_return_val_if_fail (font_family_table != NULL, NULL);
	g_return_val_if_fail (family != NULL, NULL);

	entry = g_hash_table_lookup (font_family_table, (gconstpointer) family);

	if (entry == NULL) {

		entry = font_family_new (family);

		g_hash_table_insert (font_family_table, entry->family, entry);
	}
	
	return entry;
}

static void
font_family_table_add_fonts (GHashTable *font_family_table, const char *font_path)
{
	NautilusStringList	*font_pfb_list = NULL;
	NautilusStringList	*font_xfld_list = NULL;
	guint			i;

	g_return_if_fail (font_family_table != NULL);
	g_return_if_fail (font_path != NULL);

	if (!g_file_exists (font_path)) {
		return;
	}

	if (!parse_font_description_file (font_path, &font_pfb_list, &font_xfld_list)) {
		return;
	}

	if (nautilus_string_list_get_length (font_pfb_list) != nautilus_string_list_get_length (font_xfld_list)) {
		nautilus_string_list_free (font_pfb_list);
		nautilus_string_list_free (font_xfld_list);
		return;
	}

	for (i = 0; i < nautilus_string_list_get_length (font_pfb_list); i++) {
		NautilusStringList	*tokenized_xfld;
		char			*path;
		char			*xfld;

		char			*foundry;
		char			*family;
		char			*weight;
		char			*slant;
		char			*set_width;

		FontFamilyEntry		*family_entry;
		FontEntry		*font_entry;

		path = nautilus_string_list_nth (font_pfb_list, i);
		g_assert (path != NULL);

		xfld = nautilus_string_list_nth (font_xfld_list, i);
		g_assert (xfld != NULL);

		tokenized_xfld = nautilus_string_list_new_from_tokens (xfld, "-", FALSE);
		
		foundry = nautilus_string_list_nth (tokenized_xfld, 1);
		family = nautilus_string_list_nth (tokenized_xfld, 2);
		g_strdown (family);
		weight = nautilus_string_list_nth (tokenized_xfld, 3);
		slant = nautilus_string_list_nth (tokenized_xfld, 4);
		set_width = nautilus_string_list_nth (tokenized_xfld, 5);

		family_entry = font_family_lookup_with_insertion (font_family_table, family);
		g_assert (family_entry != NULL);

		font_entry = font_family_find_font (family_entry, weight, slant, set_width);

		if (font_entry != NULL) {
			g_warning ("Dude, the font '%s-%s-%s-%s' already exists", family, weight, slant, set_width);
		}
		else {
			font_entry = font_entry_new (weight, slant, set_width, path);

			font_family_insert_font (family_entry, font_entry);
		}
		
		nautilus_string_list_free (tokenized_xfld);
		g_free (foundry);
		g_free (family);
		g_free (weight);
		g_free (slant);
		g_free (set_width);

		g_free (path);
		g_free (xfld);
	}

	nautilus_string_list_free (font_pfb_list);
	nautilus_string_list_free (font_xfld_list);
}

static NautilusStringMap *
font_family_string_map_new (void)
{
	NautilusStringMap *map;

	map = nautilus_string_map_new (FALSE);

	/*
	 * The idea behind the family map here is that users of NautilusScalableFont
	 * dont need to know what the exact name of the font is.
	 *
	 * For example, old urw fonts use the name 'helvetica', but newer ones use
	 * 'numbus sans l'
	 *
	 * So, we a map for 'helvetica' to 'nimbus sans l' if needed.
	 *
	 * Of course, specifying the font by its exact name will continue to work.
	 */
	if (font_family_lookup (global_font_family_table, "nimbus sans l")) {
		nautilus_string_map_add (map, "nimbus sans l", "helvetica");
	}
	else if (font_family_lookup (global_font_family_table, "helvetica")) {
		nautilus_string_map_add (map, "helvetica", "nimbus sans l");
	}

	return map;
}

/* Public NautilusScalableFont methods */
GtkObject*
nautilus_scalable_font_new (const char	*family,
			    const char	*weight,
			    const char	*slant,
			    const char	*set_width)
{
	/* const char	*foundry = "URW"; */
	FontFamilyEntry		 *family_entry;
	FontEntry		 *font_entry;
	NautilusScalableFont	 *font;

	initialize_global_stuff_if_needed ();

	family_entry = font_family_lookup (global_font_family_table, family);

	/* If the family entry was not found, try a mapped family name */
	if (family_entry == NULL) {
		char *mapped_family;

		mapped_family = nautilus_string_map_lookup (global_family_string_map, family);

		if (mapped_family != NULL) {
			family_entry = font_family_lookup (global_font_family_table, mapped_family);
			g_free (mapped_family);
		}
	}

	if (family_entry == NULL) {
		g_warning ("There is no such font: %s", family);
		return NULL;
	}

	weight = weight ? weight : "medium";
	slant = slant ? slant : "r";
	set_width = set_width ? set_width : "normal";

	font_entry = font_family_find_font (family_entry, weight, slant, set_width);

	if (font_entry == NULL) {
		g_warning ("There is no such font '%s-%s-%s-%s'", family, weight, slant, set_width);
		return NULL;
	}

	/* 'Intern' the rsvg font handle if needed */
	if (font_entry->font_handle == UNDEFINED_FONT_HANDLE) {
		font_entry->font_handle = rsvg_ft_intern (global_rsvg_ft_context, font_entry->path);

	}

#if 0
	g_print ("%s (%s, %s, %s %s) handle = %d\n" , 
		 __FUNCTION__, family, weight, slant, set_width, font_entry->font_handle);
#endif
	
	font = NAUTILUS_SCALABLE_FONT (gtk_type_new (nautilus_scalable_font_get_type ()));
	
	font->detail->font_handle = font_entry->font_handle;

	return GTK_OBJECT (font);
}

void
nautilus_scalable_font_measure_text (const NautilusScalableFont	*font,
				     guint			font_width,
				     guint			font_height,
				     const char			*text,
				     guint			text_length,
				     guint			*text_width_out,
				     guint			*text_height_out)
{
	RsvgFTGlyph	*glyph;
 	double		affine[6];
	int		glyph_xy[2];

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (text_width_out != NULL);
	g_return_if_fail (text_height_out != NULL);
	g_return_if_fail (font_width > 0);
	g_return_if_fail (font_height > 0);

	*text_width_out = 0;
	*text_height_out = 0;

	if (text == NULL || text[0] == '\0' || text_length == 0) {
		return;
	}

	g_return_if_fail (text_length <= strlen (text));

	art_affine_identity (affine);

	/* FIXME bugzilla.eazel.com 2544: We need to change rsvg_ft_render_string() to accept 
	 * a 'do_render' flag so that we can use to to compute metrics
	 * without actually having to render
	 */
	glyph = rsvg_ft_render_string (global_rsvg_ft_context,
				       font->detail->font_handle,
				       text,
				       text_length,
				       font_width,
				       font_height,
				       affine,
				       glyph_xy);

	*text_width_out = glyph->width;
	*text_height_out = glyph->height;

	rsvg_ft_glyph_unref (glyph);
}

void
nautilus_scalable_font_draw_text (const NautilusScalableFont *font,
				  GdkPixbuf		     *destination_pixbuf,
				  int			     x,
				  int			     y,
				  const ArtIRect              *clip_area,
				  guint			     font_width,
				  guint			     font_height,
				  const char		     *text,
				  guint			     text_length,
				  guint32		     color,
				  guchar		     overall_alpha)
{
	RsvgFTGlyph	*glyph;
 	double		affine[6];
	int		glyph_xy[2];

	guint		pixbuf_width;
	guint		pixbuf_height;
	guint		pixbuf_rowstride;
	guchar		*pixbuf_pixels;

	ArtRender	*art_render;
	ArtPixMaxDepth	art_color_array[3];

	ArtIRect	area;
	ArtIRect	glyph_area;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (gdk_pixbuf_get_has_alpha (destination_pixbuf));
	g_return_if_fail (font_width > 0);
	g_return_if_fail (font_height > 0);

	if (text == NULL || text[0] == '\0' || text_length == 0) {
		return;
	}

	g_return_if_fail (text_length <= strlen (text));

	if (clip_area != NULL) {
		g_return_if_fail (clip_area->x1 > clip_area->x0);
		g_return_if_fail (clip_area->y1 > clip_area->y0);
	}

	art_affine_identity (affine);

	glyph = rsvg_ft_render_string (global_rsvg_ft_context,
				       font->detail->font_handle,
				       text,
				       text_length,
				       font_width,
				       font_height,
				       affine,
				       glyph_xy);

	pixbuf_width = gdk_pixbuf_get_width (destination_pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (destination_pixbuf);
	pixbuf_rowstride = gdk_pixbuf_get_rowstride (destination_pixbuf);
	pixbuf_pixels = gdk_pixbuf_get_pixels (destination_pixbuf);

   	glyph_xy[0] = 0;
   	glyph_xy[1] = 0;

	art_render = art_render_new (0,
				     0,
				     pixbuf_width,
				     pixbuf_height,
				     pixbuf_pixels,
				     pixbuf_rowstride,
				     3,
				     8,
				     ART_ALPHA_SEPARATE,
				     NULL);

	art_color_array[0] = ART_PIX_MAX_FROM_8 (NAUTILUS_RGBA_COLOR_GET_R (color));
	art_color_array[1] = ART_PIX_MAX_FROM_8 (NAUTILUS_RGBA_COLOR_GET_G (color));
	art_color_array[2] = ART_PIX_MAX_FROM_8 (NAUTILUS_RGBA_COLOR_GET_B (color));
	
	art_render_image_solid (art_render, art_color_array);

	glyph_area.x0 = glyph_xy[0] + x;
	glyph_area.y0 = glyph_xy[1] + y;

	glyph_area.x1 = glyph_area.x0 + glyph->width;
	glyph_area.y1 = glyph_area.y0 + glyph->height;

	if (clip_area != NULL) {
		art_irect_intersect (&area, &glyph_area, clip_area);
	}
	else {
		area = glyph_area;
	}

/* 	glyph_xy[0] += destination_point->x; */
/*  	glyph_xy[1] += destination_point->y; */

	art_render_mask (art_render,
			 area.x0,
			 area.y0,
			 area.x1,
			 area.y1,
			 glyph->buf, 
			 glyph->rowstride);

	art_render_invoke (art_render);

	rsvg_ft_glyph_unref (glyph);
}

void
nautilus_scalable_font_measure_text_lines (const NautilusScalableFont	*font,
					   guint                        font_width,
					   guint                        font_height,
					   const char                  *text,
					   guint			num_text_lines,
					   guint                        text_line_widths[],
					   guint                        text_line_heights[],
					   guint                       *max_width_out,
					   guint                       *total_height_out)
{
	guint i;
	const char *line;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (font_width > 0);
	g_return_if_fail (font_height > 0);
	g_return_if_fail (text != NULL);
	g_return_if_fail (text_line_widths != NULL);
	g_return_if_fail (text_line_heights != NULL);
	g_return_if_fail (num_text_lines > 0);
	g_return_if_fail (num_text_lines <= (nautilus_str_count_characters (text, '\n') + 1));

	if (max_width_out != NULL) {
		*max_width_out = 0;
	}

	if (total_height_out != NULL) {
		*total_height_out = 0;
	}

	line = text;

	/*
	 * We can safely iterate for 'num_text_lines' since we already checked that the 
	 * string does indeed contain as many lines.
	 */
	for (i = 0; i < num_text_lines; i++) {
		const char	*next_new_line;
		guint		length;

		g_assert (line != NULL);

		/* Look for the next new line */
		next_new_line = strchr (line, '\n');

		if (next_new_line != NULL) {
			length = (next_new_line - line);
		}
		else {
			length = strlen (line);
		}
		
		/* Deal with empty lines */
		if (length == 0) {
			text_line_widths[i] = 0;
			text_line_heights[i] = font_height;
		}
		else {
			nautilus_scalable_font_measure_text (font,
							     font_width,
							     font_height,
							     line,
							     length,
							     &text_line_widths[i],
							     &text_line_heights[i]);
		}
		
		if (next_new_line != NULL) {
			line = next_new_line + 1;
		}
		else {
			line = NULL;
		}
		
		/* Keep track of total height */
 		if (total_height_out != NULL) {
 			*total_height_out += text_line_heights[i];
 		}
		
		/* Keep track of max width */
 		if ((max_width_out != NULL) && (text_line_widths[i] > *max_width_out)) {
 			*max_width_out = text_line_widths[i];
 		}
	}
}

void
nautilus_scalable_font_draw_text_lines_with_dimensions (const NautilusScalableFont  *font,
							GdkPixbuf                   *destination_pixbuf,
							int                          x,
							int                          y,
							const ArtIRect              *clip_area,
							guint                        font_width,
							guint                        font_height,
							const char                  *text,
							guint                        num_text_lines,
							const guint                 *text_line_widths,
							const guint                 *text_line_heights,
							GtkJustification             justification,
							guint                        line_offset,
							guint32                      color,
							guchar                       overall_alpha)
{
	guint		i;
	const char	*line;
	guint		available_width;
	guint		available_height;

//	guint max_num_text_lines;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (gdk_pixbuf_get_has_alpha (destination_pixbuf));
	g_return_if_fail (clip_area != NULL);
	g_return_if_fail (font_width > 0);
	g_return_if_fail (font_height > 0);
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	g_return_if_fail (clip_area->x1 > clip_area->x0);
	g_return_if_fail (clip_area->y1 > clip_area->y0);
	g_return_if_fail (num_text_lines > 0);
	g_return_if_fail (num_text_lines <= (nautilus_str_count_characters (text, '\n') + 1));
	g_return_if_fail (text_line_widths != NULL);
	g_return_if_fail (text_line_widths != NULL);

	available_width = clip_area->x1 - clip_area->x0;
	available_height = clip_area->y1 - clip_area->y0;

//	max_num_text_lines = (available_height / font_height);

	line = text;

	/*
	 * We can safely iterate for 'num_text_lines' since we already checked that the 
	 * string does indeed contain as many lines.
	 */
	for (i = 0; i < num_text_lines; i++) {
		const char	*next_new_line;
		guint		length;

		g_assert (line != NULL);

		/* Look for the next new line */
		next_new_line = strchr (line, '\n');

		if (next_new_line != NULL) {
			length = (next_new_line - line);
		}
		else {
			length = strlen (line);
		}
		
		/* Deal with empty lines */
		if (length == 0) {
			y += font_height;
		}
		else {
			int text_x;
			int text_y;
			
			switch (justification) {
			case GTK_JUSTIFY_LEFT:
				text_x = x;
				break;
				
			case GTK_JUSTIFY_CENTER:
			case GTK_JUSTIFY_FILL:
				if (text_line_widths[i] <= available_width) {
					text_x = x + ((available_width - text_line_widths[i]) / 2);
				}
				else {
					text_x = x - ((text_line_widths[i] - available_width) / 2);
				}
				break;

			case GTK_JUSTIFY_RIGHT:
				text_x = x + available_width - text_line_widths[i];
				break;
				
			default:
				g_assert_not_reached ();
			}
			
			text_y = y;
			
			nautilus_scalable_font_draw_text (font,
							  destination_pixbuf,
							  text_x,
							  text_y,
							  clip_area,
							  font_width,
							  font_height,
							  line,
							  length,
							  color,
							  overall_alpha);
			
			y += (line_offset + text_line_heights[i]);
		}
		
		if (next_new_line != NULL) {
			line = next_new_line + 1;
		}
		else {
			line = NULL;
		}
	}
}

void
nautilus_scalable_font_draw_text_lines (const NautilusScalableFont  *font,
					GdkPixbuf                   *destination_pixbuf,
					int			     x,
					int			     y,
					const ArtIRect              *clip_area,
					guint                        font_width,
					guint                        font_height,
					const char                  *text,
					GtkJustification             justification,
					guint                        line_offset,
					guint32                      color,
					guchar                       overall_alpha)
{
	guint	num_text_lines;
	guint	*text_line_widths;
	guint	*text_line_heights;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (gdk_pixbuf_get_has_alpha (destination_pixbuf));
	g_return_if_fail (clip_area != NULL);
	g_return_if_fail (font_width > 0);
	g_return_if_fail (font_height > 0);
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	g_return_if_fail (clip_area->x1 > clip_area->x0);
	g_return_if_fail (clip_area->y1 > clip_area->y0);

 	if (text == NULL || text[0] == '\0') {
 		return;
 	}

 	num_text_lines = nautilus_str_count_characters (text, '\n') + 1;
	text_line_widths = g_new (guint, num_text_lines);
	text_line_heights = g_new (guint, num_text_lines);
	
	nautilus_scalable_font_measure_text_lines (font,
						   font_width,
						   font_height,
						   text,
						   num_text_lines,
						   text_line_widths,
						   text_line_heights,
						   NULL,
						   NULL);

	nautilus_scalable_font_draw_text_lines_with_dimensions (font,
								destination_pixbuf,
								x,
								y,
								clip_area,
								font_width,
								font_height,
								text,
								num_text_lines,
								text_line_widths,
								text_line_heights,
								justification,
								line_offset,
								color,
								overall_alpha);
	
	g_free (text_line_widths);
	g_free (text_line_heights);
}

guint
nautilus_scalable_font_largest_fitting_font_size (const NautilusScalableFont  *font,
						  const char                  *text,
						  guint                        available_width,
						  const guint		       font_sizes[],
						  guint			       num_font_sizes)
{
	NautilusStringList	*tokenized_string;
	guint			i;
	char			*longest_string;
	guint			longest_string_length;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), 0);
	g_return_val_if_fail (font_sizes != NULL, 0);
	g_return_val_if_fail (num_font_sizes > 0, 0);

 	if (text == NULL || text[0] == '\0' || available_width < 1) {
 		return font_sizes[num_font_sizes - 1];
 	}

	tokenized_string = nautilus_string_list_new_from_tokens (text, "\n", FALSE);
	longest_string = nautilus_string_list_get_longest_string (tokenized_string);
	g_assert (longest_string != NULL);
	nautilus_string_list_free (tokenized_string);
	longest_string_length = strlen (longest_string);

	for (i = 0; i < num_font_sizes; i++) {
		guint	text_width;
		guint	text_height;

		nautilus_scalable_font_measure_text (font,
						     font_sizes[i],
						     font_sizes[i],
						     longest_string,
						     longest_string_length,
						     &text_width,
						     &text_height);

		if (text_width <= available_width) {
			g_free (longest_string);
			return font_sizes[i];
		}
	}

	g_free (longest_string);

	return font_sizes[num_font_sizes - 1];
}

NautilusScalableFont *
nautilus_scalable_font_get_default_font (void)
{
	if (global_default_font == NULL) {
		global_default_font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("helvetica", NULL, NULL, NULL));
		g_assert (global_default_font != NULL);
		g_atexit (default_font_at_exit_destructor);
	}

	gtk_object_ref (GTK_OBJECT (global_default_font));

	return global_default_font;
}

static void
font_family_table_for_each_append (gpointer	key,
				   gpointer	value,
				   gpointer	user_data)
{
	NautilusStringList *list = (NautilusStringList *) user_data;
	FontFamilyEntry	   *family_entry = (FontFamilyEntry *) value;

	g_assert (family_entry != NULL);
 	g_assert (!nautilus_string_list_contains (list, family_entry->family));

	nautilus_string_list_insert (list, family_entry->family);
}

static void
font_family_table_for_each_free (gpointer	key,
				 gpointer	value,
				 gpointer	user_data)
{
	FontFamilyEntry *family_entry = (FontFamilyEntry *) value;
	g_assert (family_entry != NULL);

	font_family_free (family_entry);
}

static NautilusStringList *
font_family_table_get_family_list (GHashTable *font_family_table)
{
	NautilusStringList *list;

	g_return_val_if_fail (font_family_table != NULL, NULL);

	list = nautilus_string_list_new (FALSE);
	
	g_hash_table_foreach (font_family_table, font_family_table_for_each_append, list);

	return list;
}

NautilusStringList *
nautilus_scalable_font_get_font_family_list (void)
{
	initialize_global_stuff_if_needed ();

	return font_family_table_get_family_list (global_font_family_table);
}

static void
font_family_table_free (GHashTable *font_family_table)
{
	g_assert (font_family_table != NULL);
	
	g_hash_table_foreach (font_family_table, font_family_table_for_each_free, NULL);

	g_hash_table_destroy (font_family_table);
}

gboolean
nautilus_scalable_font_query_font (const char		*family,
				   NautilusStringList	**weights_out,
				   NautilusStringList	**slants_out,
				   NautilusStringList	**set_widths_out)
{
	FontFamilyEntry	*family_entry;
	GList		 *iterator;

	g_return_val_if_fail (family != NULL, FALSE);

	if (weights_out != NULL) {
		*weights_out = NULL;
	}

	if (slants_out != NULL) {
		*slants_out = NULL;
	}

	if (set_widths_out != NULL) {
		*set_widths_out = NULL;
	}

	family_entry = font_family_lookup (global_font_family_table, family);

	if (family_entry == NULL) {
		return FALSE;
	}

	if (weights_out == NULL && slants_out == NULL && set_widths_out == NULL) {
		return TRUE;
	}

	if (weights_out != NULL) {
		*weights_out = nautilus_string_list_new (FALSE);
	}

	if (slants_out != NULL) {
		*slants_out = nautilus_string_list_new (FALSE);
	}

	if (set_widths_out != NULL) {
		*set_widths_out = nautilus_string_list_new (FALSE);
	}

	for (iterator = family_entry->fonts; iterator != NULL; iterator = iterator->next) {
		FontEntry *font_entry = (FontEntry *) iterator->data;
		g_assert (font_entry != NULL);

		if (weights_out != NULL) {
			nautilus_string_list_insert (*weights_out, font_entry->weight);
		}

		if (slants_out != NULL) {
			nautilus_string_list_insert (*slants_out, font_entry->slant);
		}

		if (set_widths_out != NULL) {
			nautilus_string_list_insert (*set_widths_out, font_entry->set_width);
		}
	}

	return TRUE;
}

#if 0
static const char * global_default_font_path[] = 
{
	"/usr/lib/X11/fonts/Type1",
	"/usr/share/fonts/default/Type1"
};
#endif

/* 'atexit' destructors for global stuff */
static void
default_font_at_exit_destructor (void)
{
	if (global_default_font != NULL) {
		gtk_object_unref (GTK_OBJECT (global_default_font));
		global_default_font = NULL;
	}
}

static void
font_family_table_at_exit_destructor (void)
{
	if (global_font_family_table != NULL) {
		font_family_table_free (global_font_family_table);
		global_font_family_table = NULL;
	}
}

static void
font_family_string_map_at_exit_destructor (void)
{
	if (global_family_string_map != NULL) {
		nautilus_string_map_free (global_family_string_map);
		global_family_string_map = NULL;
	}
}

static void
initialize_global_stuff_if_needed (void)
{
	static gboolean fonts_initialized = FALSE;

	/* Initialize the rsvg font context shared by all fonts */
	if (global_rsvg_ft_context == NULL) {
		global_rsvg_ft_context = rsvg_ft_ctx_new ();
	}

	/* Initialize the global font table */
	if (fonts_initialized == FALSE) {
		fonts_initialized = TRUE;
		global_font_family_table = g_hash_table_new (g_str_hash, g_str_equal);
		font_family_table_add_fonts (global_font_family_table, "/usr/share/fonts/default/Type1");

		g_atexit (font_family_table_at_exit_destructor);
	}

	if (global_family_string_map == NULL) {
		global_family_string_map = font_family_string_map_new ();

		g_atexit (font_family_string_map_at_exit_destructor);
	}
}
