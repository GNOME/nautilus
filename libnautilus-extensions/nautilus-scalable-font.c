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
#include "nautilus-lib-self-check-functions.h"

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
	g_assert (glyph != NULL);

	*text_width_out = glyph->width;
	*text_height_out = glyph->height;

	rsvg_ft_glyph_unref (glyph);
}

guint
nautilus_scalable_font_text_width (const NautilusScalableFont  *font,
				   guint                        font_width,
				   guint                        font_height,
				   const char                  *text,
				   guint                        text_length)
{
	guint	text_width = 0;
	guint	text_height = 0;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), 0);
	g_return_val_if_fail (font_width > 0, 0);
	g_return_val_if_fail (font_height > 0, 0);

	if (text == NULL || text[0] == '\0' || text_length == 0) {
		return 0;
	}

	g_return_val_if_fail (text_length <= strlen (text), 0);

	nautilus_scalable_font_measure_text (font,
					     font_width,
					     font_height,
					     text,
					     text_length,
					     &text_width,
					     &text_height);

	return text_width;
}

/* Lifted from Raph's test-ft-gtk.c sample program */
static void
invert_glyph (guchar *buf, int rowstride, int width, int height)
{
	int x, y;
	int first;
	int n_words;
	int last;
	guint32 *middle;
	
	if (width >= 8 && ((rowstride & 3) == 0)) {
		first = (-(int)buf) & 3;
		n_words = (width - first) >> 2;
		last = first + (n_words << 2);
		
		for (y = 0; y < height; y++) {
			middle = (guint32 *)(buf + first);
			for (x = 0; x < first; x++)
				buf[x] = ~buf[x];
			for (x = 0; x < n_words; x++)
				middle[x] = ~middle[x];
			for (x = last; x < width; x++)
				buf[x] = ~buf[x];
			buf += rowstride;
		}
	} else {
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++)
				buf[x] = ~buf[x];
			buf += rowstride;
		}
	}
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
				  guchar		     overall_alpha,
				  gboolean		     inverted)
{
	RsvgFTGlyph	*glyph;
 	double		affine[6];
	int		glyph_xy[2];
	ArtIRect	render_area;
	ArtIRect	glyph_area;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
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

	/* Make a glyph for the given string  */
	glyph = rsvg_ft_render_string (global_rsvg_ft_context,
				       font->detail->font_handle,
				       text,
				       text_length,
				       font_width,
				       font_height,
				       affine,
				       glyph_xy);
	g_assert (glyph != NULL);

   	glyph_xy[0] = 0;
   	glyph_xy[1] = 0;
	
	glyph_area.x0 = glyph_xy[0];
	glyph_area.y0 = glyph_xy[1];
	glyph_area.x1 = glyph_area.x0 + glyph->width;
	glyph_area.y1 = glyph_area.y0 + glyph->height;
	
	/* Invert the glyph if needed */
	if (inverted) {
		ArtIRect invert_area;
		
		if (clip_area != NULL) {
			art_irect_intersect (&invert_area, &glyph_area, clip_area);
		}
		else {
			invert_area = glyph_area;
		}

		//if (!art_irect_empty (&invert_area)) {
		{
			guchar *glyph_pixels;
			guint glyph_rowstride;

			glyph_rowstride = glyph->rowstride;
			
			glyph_pixels = glyph->buf;

			glyph_pixels = 
				glyph->buf + 
				(invert_area.x0 - glyph_area.x0) + 
				glyph_rowstride * (invert_area.y0 - glyph_area.y0);

			invert_glyph (glyph_pixels, 
				      glyph_rowstride,
				      invert_area.x1 - invert_area.x0,
				      invert_area.y1 - invert_area.y0);
		}
	}

	/* Translate the glyph area to the (x,y) where its to be rendered */
	glyph_area.x0 += x;
	glyph_area.y0 += y;
	glyph_area.x1 += x;
	glyph_area.y1 += y;

	/* Clip the glyph_area against the clip_area if needed */
	if (clip_area != NULL) {
		art_irect_intersect (&render_area, &glyph_area, clip_area);
	}
	else {
		render_area = glyph_area;
	}

	/* Render the glyph */
	if (!art_irect_empty (&render_area)) {
		guint		pixbuf_width;
		guint		pixbuf_height;
		guint		pixbuf_rowstride;
		guchar		*pixbuf_pixels;
		
		ArtRender	*art_render;
		ArtPixMaxDepth	art_color_array[3];
		ArtAlphaType	alpha_type;
		
		pixbuf_width = gdk_pixbuf_get_width (destination_pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (destination_pixbuf);
		pixbuf_rowstride = gdk_pixbuf_get_rowstride (destination_pixbuf);
		pixbuf_pixels = gdk_pixbuf_get_pixels (destination_pixbuf);
		
		alpha_type = gdk_pixbuf_get_has_alpha (destination_pixbuf) ? 
			ART_ALPHA_SEPARATE : 
			ART_ALPHA_NONE;
		
		art_render = art_render_new (0,
					     0,
					     pixbuf_width,
					     pixbuf_height,
					     pixbuf_pixels,
					     pixbuf_rowstride,
					     3,
					     8,
					     alpha_type,
					     NULL);
		
		art_color_array[0] = ART_PIX_MAX_FROM_8 (NAUTILUS_RGBA_COLOR_GET_R (color));
		art_color_array[1] = ART_PIX_MAX_FROM_8 (NAUTILUS_RGBA_COLOR_GET_G (color));
		art_color_array[2] = ART_PIX_MAX_FROM_8 (NAUTILUS_RGBA_COLOR_GET_B (color));
		
		art_render_image_solid (art_render, art_color_array);
		
		art_render_mask (art_render,
				 render_area.x0,
				 render_area.y0,
				 render_area.x1,
				 render_area.y1,
				 glyph->buf, 
				 glyph->rowstride);
		
		art_render_invoke (art_render);
	}
	
	rsvg_ft_glyph_unref (glyph);
	}

void
nautilus_scalable_font_measure_text_lines (const NautilusScalableFont	*font,
					   guint                        font_width,
					   guint                        font_height,
					   const char                  *text,
					   guint			num_text_lines,
					   double			empty_line_height,
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
			text_line_heights[i] = empty_line_height;
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
							double			     empty_line_height,
							guint32                      color,
							guchar                       overall_alpha,
							gboolean		     inverted)
{
	guint		i;
	const char	*line;
	guint		available_width;
	guint		available_height;

//	guint max_num_text_lines;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
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
			y += (line_offset + text_line_heights[i]);
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
				text_x = 0;
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
							  overall_alpha,
							  inverted);
			
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
					double			     empty_line_height,
					guint32                      color,
					guchar                       overall_alpha,
					gboolean		     inverted)
{
	guint	num_text_lines;
	guint	*text_line_widths;
	guint	*text_line_heights;

	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (clip_area != NULL);
	g_return_if_fail (font_width > 0);
	g_return_if_fail (font_height > 0);
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	g_return_if_fail (clip_area->x1 > clip_area->x0);
	g_return_if_fail (clip_area->y1 > clip_area->y0);

 	if (text == NULL || text[0] == '\0') {
 		return;
 	}

	/* FIXME bugzilla.eazel.com 2785: We need to optimize this code to measure a minimum
	 * number of text lines.  We need to look at the clip rectangle and compute the 
	 * maximum number of text lines that actually fit.
	 */

 	num_text_lines = nautilus_str_count_characters (text, '\n') + 1;
	text_line_widths = g_new (guint, num_text_lines);
	text_line_heights = g_new (guint, num_text_lines);
	
	nautilus_scalable_font_measure_text_lines (font,
						   font_width,
						   font_height,
						   text,
						   num_text_lines,
						   empty_line_height,
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
								empty_line_height,
								color,
								overall_alpha,
								inverted);
	
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

/*
 * The following text_layout stuff was shamelessly plundered
 * from libgnomeui/gnome-icon-text.[ch] by Federico Mena.
 *
 * It was hacked to use NautilusScalableFont and GdkPixbuf
 * instead of GdkFont and GdkDrawable.  We want to use the
 * same layout algorithm in Nautilus so that both the smooth
 * and not smooth text rendering cases have predictably 
 * similar result.
 *
 * I also made some minor Nautilus-like style changes. -re
 *
 */
static void
text_layout_free_row (gpointer data, gpointer user_data)
{
	NautilusTextLayoutRow *row;

	if (data) {
		row = data;
		g_free (row->text);
		g_free (row);
	}
}

/**
 * nautilus_text_layout_free:
 * @ti: An icon text info structure.
 *
 * Frees a &NautilusTextLayout structure.  You should call this instead of
 * freeing the structure yourself.
 */
void
nautilus_text_layout_free (NautilusTextLayout *text_info)
{
	g_list_foreach (text_info->rows, text_layout_free_row, NULL);
	g_list_free (text_info->rows);
	g_free (text_info);
}

/**
 * nautilus_text_layout_new:
 * @font:       Name of the font that will be used to render the text.
 * @text:       Text to be formatted.
 * @separators: Separators used for word wrapping, can be NULL.
 * @max_width:  Width in pixels to be used for word wrapping.
 * @confine:    Whether it is mandatory to wrap at @max_width.
 *
 * Creates a new &NautilusTextLayout structure by wrapping the specified
 * text.  If non-NULL, the @separators argument defines a set of characters
 * to be used as word delimiters for performing word wrapping.  If it is
 * NULL, then only spaces will be used as word delimiters.
 *
 * The @max_width argument is used to specify the width at which word
 * wrapping will be performed.  If there is a very long word that does not
 * fit in a single line, the @confine argument can be used to specify
 * whether the word should be unconditionally split to fit or whether
 * the maximum width should be increased as necessary.
 *
 * Return value: A newly-created &NautilusTextLayout structure.
 */
NautilusTextLayout *
nautilus_text_layout_new (const NautilusScalableFont *font,
			  guint font_size,
			  const char *text,
			  const char *separators,
			  int max_width,
			  gboolean confine)
{
	NautilusTextLayout *text_info;
	NautilusTextLayoutRow *row;
	const char *row_end;
	const char *s, *word_start, *word_end, *old_word_end;
	char *sub_text;
	int i, w_len, w;
	const char *text_iter;
	int text_len, separators_len;

	g_return_val_if_fail (font != NULL, NULL);
	g_return_val_if_fail (font_size > 0, NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (nautilus_strlen (text) > 0, NULL);

	if (!separators)
		separators = " ";

	text_len = strlen (text);

	separators_len = strlen (separators);

	text_info = g_new (NautilusTextLayout, 1);

	text_info->rows = NULL;
	text_info->font = font;
	text_info->font_size = font_size;
	text_info->width = 0;
	text_info->height = 0;
	text_info->baseline_skip = font_size;

	word_end = NULL;

	text_iter = text;
	while (*text_iter) {
		for (row_end = text_iter; *row_end != 0 && *row_end != '\n'; row_end++);

		/* Accumulate words from this row until they don't fit in the max_width */

		s = text_iter;

		while (s < row_end) {
			word_start = s;
			old_word_end = word_end;
			for (word_end = word_start; *word_end; word_end++) {
				const char *p;
				for (p = separators; *p; p++) {
					if (*word_end == *p)
						goto found;
				}
			}
		  found:
			if (word_end < row_end)
				word_end++;

			if (nautilus_scalable_font_text_width (font, font_size, font_size, text_iter, word_end - text_iter) > max_width) {
				if (word_start == text_iter) {
					if (confine) {
						/* We must force-split the word.  Look for a proper
                                                 * place to do it.
						 */

						w_len = word_end - word_start;
						
						for (i = 1; i < w_len; i++) {
							w = nautilus_scalable_font_text_width (font, font_size, font_size, word_start, i);
							if (w > max_width) {
								if (i == 1)
									/* Shit, not even a single character fits */
									max_width = w;
								else
									break;
							}
						}

						/* Create sub-row with the chars that fit */

						sub_text = g_new (char, i);
						memcpy (sub_text, word_start, (i - 1) * sizeof (char));
						sub_text[i - 1] = 0;
						
						row = g_new (NautilusTextLayoutRow, 1);
						row->text = sub_text;
						row->text_length = i - 1;
						row->width = nautilus_scalable_font_text_width (font, font_size, font_size, 
												sub_text, 
												strlen (sub_text));
						row->text = g_strdup (sub_text);
						if (row->text == NULL)
							row->text = g_strdup("");

						text_info->rows = g_list_append (text_info->rows, row);

						if (row->width > text_info->width)
							text_info->width = row->width;

						text_info->height += text_info->baseline_skip;

						/* Bump the text pointer */

						text_iter += i - 1;
						s = text_iter;

						continue;
					} else
						max_width = nautilus_scalable_font_text_width (font, font_size, font_size, word_start, word_end - word_start);

					continue; /* Retry split */
				} else {
					word_end = old_word_end; /* Restore to region that does fit */
					break; /* Stop the loop because we found something that doesn't fit */
				}
			}

			s = word_end;
		}

		/* Append row */

		if (text_iter == row_end) {
			/* We are on a newline, so append an empty row */

			text_info->rows = g_list_append (text_info->rows, NULL);
			text_info->height += text_info->baseline_skip / 2;

			/* Next! */

			text_iter = row_end + 1;
		} else {
			/* Create subrow and append it to the list */

			int sub_len;
			sub_len = word_end - text_iter;

			sub_text = g_new (char, sub_len + 1);
			memcpy (sub_text, text_iter, sub_len * sizeof (char));
			sub_text[sub_len] = 0;

			row = g_new (NautilusTextLayoutRow, 1);
			row->text = sub_text;
			row->text_length = sub_len;
			row->width = nautilus_scalable_font_text_width (font, font_size, font_size, sub_text, sub_len);
			row->text = g_strdup (sub_text);
			if (row->text == NULL)
				row->text = g_strdup("");

			text_info->rows = g_list_append (text_info->rows, row);

			if (row->width > text_info->width)
				text_info->width = row->width;

			text_info->height += text_info->baseline_skip;

			/* Next! */

			text_iter = word_end;
		}
	}

	return text_info;
}

/**
 * nautilus_text_layout_paint:
 * @ti:       An icon text info structure.
 * @drawable: Target drawable.
 * @gc:       GC used to render the string.
 * @x:        Left coordinate for text.
 * @y:        Upper coordinate for text.
 * @just:     Justification for text.
 *
 * Paints the formatted text in the icon text info structure onto a drawable.
 * This is just a sample implementation; applications can choose to use other
 * rendering functions.
 */
void
nautilus_text_layout_paint (const NautilusTextLayout *text_info,
			    GdkPixbuf *destination_pixbuf,
			    int x, 
			    int y, 
			    GtkJustification just,
			    guint32 color,
			    gboolean		    inverted)
{
	GList *item;
	const NautilusTextLayoutRow *row;
	int xpos;

	g_return_if_fail (text_info != NULL);
	g_return_if_fail (destination_pixbuf != NULL);

 	/* y += text_info->font->ascent; */

	for (item = text_info->rows; item; item = item->next) {
		if (item->data) {
			row = item->data;

			switch (just) {
			case GTK_JUSTIFY_LEFT:
				xpos = 0;
				break;

			case GTK_JUSTIFY_RIGHT:
				xpos = text_info->width - row->width;
				break;

			case GTK_JUSTIFY_CENTER:
				xpos = (text_info->width - row->width) / 2;
				break;

			default:
				/* Anyone care to implement GTK_JUSTIFY_FILL? */
				g_warning ("Justification type %d not supported.  Using left-justification.",
					   (int) just);
				xpos = 0;
			}
			
			nautilus_scalable_font_draw_text (text_info->font,
							  destination_pixbuf,
							  x + xpos,
							  y,
							  NULL,
							  text_info->font_size,
							  text_info->font_size,
							  row->text,
							  row->text_length,
							  color,
							  255,
							  inverted);

			y += text_info->baseline_skip;
		} else
			y += text_info->baseline_skip / 2;
	}
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_scalable_font (void)
{
	/* const char *fonts_place = NAUTILUS_DATADIR "/fonts/urw"; */
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
