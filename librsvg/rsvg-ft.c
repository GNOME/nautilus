/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   rsvg-ft.c: Basic functions for freetype/libart integration.
 
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
  
   Author: Raph Levien <raph@artofcode.com>
*/

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <freetype/freetype.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_affine.h>
#include "art_render.h"
#include "art_render_mask.h"

#include "rsvg-ft.h"

#define FT_FLOOR( x )  (   (x)        & -64 )
#define FT_CEIL( x )   ( ( (x) + 63 ) & -64 )
#define FT_TRUNC( x )  (   (x) >> 6 )
#define FT_FROMFLOAT(x) ((int)floor ((x) * 64.0 + 0.5))
#define FT_TOFLOAT(x) ((x) * (1.0 / 64.0))

typedef struct _RsvgFTFont RsvgFTFont;
typedef struct _RsvgFTFontCacheEntry RsvgFTFontCacheEntry;
typedef struct _RsvgFTGlyphDesc RsvgFTGlyphDesc;
typedef struct _RsvgFTGlyphCacheEntry RsvgFTGlyphCacheEntry;

struct _RsvgFTCtx {
	FT_Library ftlib;

	GHashTable *font_hash_table; /* map filename to RsvgFTFontCacheEntry */

	int n_font_list;
	RsvgFTFontCacheEntry **font_list;
	RsvgFTFontCacheEntry *first, *last;

	int n_loaded_fonts;
	int n_loaded_fonts_max;

	GHashTable *glyph_hash_table; /* map GlyphDesc to GlyphCacheEntry */
	int glyph_bytes;
	int glyph_bytes_max;
	RsvgFTGlyphCacheEntry *glyph_first, *glyph_last;
};

struct _RsvgFTFont {
	/* refcnt is likely to be obsoleted - clients now hold handles
	   which don't really need refcounting */
	int refcnt;
	RsvgFTCtx *ctx;
	FT_Face face;
};

struct _RsvgFTFontCacheEntry {
	RsvgFTFontCacheEntry *prev, *next; /* for lru list */
	char *fn;
	char *fn_attached;
	RsvgFTFont *font;
	RsvgFTFontHandle handle; /* index in ctx->font_cache */
};

struct _RsvgFTGlyphDesc {
	RsvgFTFontHandle fh;
	FT_F26Dot6 char_width;
	FT_F26Dot6 char_height;
	FT_UInt glyph_index;
	unsigned char x_subpixel;
	unsigned char y_subpixel;
};

struct _RsvgFTGlyphCacheEntry {
	RsvgFTGlyphCacheEntry *prev, *next; /* for lru list */
	RsvgFTGlyph *glyph;
};

/* Glyph cache fun stuff */

static guint
rsvg_ft_glyph_desc_hash (gconstpointer v)
{
	const RsvgFTGlyphDesc *desc = (const RsvgFTGlyphDesc *)v;

	/* not known to be a good mixing function */
	return (desc->fh << 16) + desc->char_width +
		((desc->char_width ^ desc->char_height) << 10) +
		(desc->x_subpixel << 22) +
		(desc->y_subpixel << 16) +
		desc->glyph_index;
}

static gint
rsvg_ft_glyph_desc_equal (gconstpointer v, gconstpointer v2)
{
	return !memcmp ((const gchar *)v, (const gchar *)v2,
			sizeof(RsvgFTGlyphDesc));
}

#if 0
/**
 * rsvg_ft_glyph_lookup: Look up a glyph in the glyph cache.
 * @ctx: The Rsvg FT context.
 * @desc: Glyph descriptor.
 *
 * Looks up the glyph in the glyph cache. If found, moves it to the front
 * of the LRU list. It does not bump the refcount on the glyph - most
 * of the time, the caller will want to do that.
 *
 * Return value: The glyph if found, otherwise NULL.
 **/
static RsvgFTGlyph *
rsvg_ft_glyph_lookup (RsvgFTCtx *ctx, const RsvgFTGlyphDesc *desc)
{
	RsvgFTGlyphCacheEntry *entry;

	entry = g_hash_table_lookup (ctx->glyph_hash_table, desc);
	if (entry == NULL)
		return NULL;

	/* move entry to front of LRU list */
	if (entry->prev != NULL) {
		entry->prev->next = entry->next;
		if (entry->next != NULL) {
			entry->next->prev = entry->prev;
		} else {
			ctx->glyph_last = entry->prev;
		}
		entry->prev = NULL;
		entry->next = ctx->glyph_first;
		ctx->glyph_first->prev = entry;
		ctx->glyph_first = entry;

	}
	return entry->glyph;
}
#endif

RsvgFTCtx *
rsvg_ft_ctx_new (void) {
	RsvgFTCtx *result = g_new (RsvgFTCtx, 1);
	FT_Error error;

	error = FT_Init_FreeType (&result->ftlib);
	if (error) {
		g_free (result);
		result = NULL;
	}
	result->font_hash_table = g_hash_table_new (g_str_hash, g_str_equal);
	result->n_font_list = 0;
	result->font_list = NULL;
	result->first = NULL;
	result->last = NULL;
	result->n_loaded_fonts = 0;
	result->n_loaded_fonts_max = 10;

	result->glyph_bytes = 0;
	result->glyph_bytes_max = 0x100000; /* 1 meg */
	result->glyph_first = NULL;
	result->glyph_last = NULL;

	result->glyph_hash_table = g_hash_table_new (rsvg_ft_glyph_desc_hash,
						     rsvg_ft_glyph_desc_equal);

	return result;
}

void
rsvg_ft_ctx_done (RsvgFTCtx *ctx) {
	int i;

	for (i = 0; i < ctx->n_font_list; i++) {
		RsvgFTFontCacheEntry *entry = ctx->font_list[i];
		RsvgFTFont *font = entry->font;
		g_free (entry->fn);
		g_free (entry->fn_attached);
		if (font != NULL) {
			FT_Done_Face (font->face);
			g_free (font);
		}
	}
	g_free (ctx->font_list);
	FT_Done_FreeType (ctx->ftlib);
	g_free (ctx);
}

/**
 * rsvg_ft_load: Load a font.
 * @ctx: Rsvg FT context.
 * @font_file_name: File name.
 *
 * Return value: Newly created RsvgFont structure.
 **/
static RsvgFTFont *
rsvg_ft_load (RsvgFTCtx *ctx, const char *font_file_name)
{
	FT_Error error;
	FT_Face face;
	RsvgFTFont *result;

	error = FT_New_Face (ctx->ftlib, font_file_name,
			     0, &face);
	if (error)
		result = NULL;
	else {
		result = g_new (RsvgFTFont, 1);
		result->refcnt = 1;
		result->ctx = ctx;
		result->face = face;
	}
	return result;
}

/**
 * rsvg_ft_intern: Intern a font.
 * @ctx: Rsvg FT context.
 * @font_file_name: File name.
 *
 * This routine checks the font list to see if the font has already been
 * interned. If so, it just returns the existing handle. Otherwise, it
 * adds the font to the font list with the new font handle.
 *
 * Return value: The font handle for the font.
 **/
RsvgFTFontHandle
rsvg_ft_intern (RsvgFTCtx *ctx, const char *font_file_name)
{
	RsvgFTFontCacheEntry *entry;

	entry = g_hash_table_lookup (ctx->font_hash_table, font_file_name);
	if (entry != NULL) {
		/* found in font list */

		/* move entry to front of LRU list */
		if (entry->prev != NULL) {
			entry->prev->next = entry->next;
			if (entry->next != NULL) {
				entry->next->prev = entry->prev;
			} else {
				ctx->last = entry->prev;
			}
			entry->prev = NULL;
			entry->next = ctx->first;
			ctx->first->prev = entry;
			ctx->first = entry;
		}
	} else {
		/* not found in font list */
		int n_font_list;

		n_font_list = ctx->n_font_list++;
		entry = g_new (RsvgFTFontCacheEntry, 1);
		entry->fn = g_strdup (font_file_name);
		entry->fn_attached = NULL;
		entry->handle = n_font_list;
		entry->font = NULL;
		entry->prev = NULL;
		entry->next = ctx->first;
		if (ctx->first != NULL)
			ctx->first->prev = entry;
		else
			ctx->last = entry;
		ctx->first = entry;
		if (n_font_list == 0) {
			ctx->font_list = g_new (RsvgFTFontCacheEntry *, 1);
		} else if (!(n_font_list & (n_font_list - 1))) {
			ctx->font_list = g_renew (RsvgFTFontCacheEntry *,
						   ctx->font_list,
						   n_font_list << 1);
		}
		ctx->font_list[n_font_list] = entry;
	}

/* 	fprintf (stderr, "handle = %d\n", entry->handle); */
	return entry->handle;
}

/**
 * rsvg_ft_font_attach: Attach an additional font file.
 * @ctx: Rsvg FT context.
 * @fh: Font handle.
 * @font_file_name: The filename of an additional file to attach.
 *
 * Attaches an additional font file to @font. For Type1 fonts, use
 * rsvg_ft_load() to load the .pfb file, then this one to attach
 * the afm.
 **/
void
rsvg_ft_font_attach (RsvgFTCtx *ctx, RsvgFTFontHandle fh,
		     char *font_file_name)
{
	RsvgFTFontCacheEntry *entry;
	RsvgFTFont *font;
	FT_Error error;

	if (fh < 0 || fh >= ctx->n_font_list)
		return;
	entry = ctx->font_list[fh];
	if (entry->fn_attached != NULL)
		return;
	entry->fn_attached = g_strdup (font_file_name);
	font = entry->font;
	if (font != NULL) {
		error = FT_Attach_File (font->face, font_file_name);
	}
}

/**
 * rsvg_ft_font_resolve: Resolve a font handle.
 * @ctx: Rsvg FT context.
 * @fh: Font handle.
 *
 * Resolves the font handle @fh to an actual font data structure.
 * This includes loading the font if it hasn't been loaded yet, or if
 * it's been evicted from the cache.
 *
 * Return value: RsvgFTFont structure.
 **/
static RsvgFTFont *
rsvg_ft_font_resolve (RsvgFTCtx *ctx, RsvgFTFontHandle fh)
{
	RsvgFTFontCacheEntry *entry = NULL;
	RsvgFTFont *font = NULL;

	if (fh < 0 || fh >= ctx->n_font_list)
		return NULL;
	entry = ctx->font_list[fh];
	if (entry->font == NULL) {
		ctx->n_loaded_fonts++;
		while (ctx->n_loaded_fonts > ctx->n_loaded_fonts_max) {
			/* todo: evict lru */
			break;
		}
		font = rsvg_ft_load (ctx, entry->fn);
		if (entry->fn_attached != NULL) {
			FT_Error error;

			error = FT_Attach_File (font->face,
						entry->fn_attached);
		}
		entry->font = font;
	}
	else {
		font = entry->font;
	}
	return font;
}

/**
 * rsvg_ft_glyph_composite: Composite glyph using saturation.
 * @dst: Destination glyph over which to composite.
 * @src: Source glyph for compositing.
 * @dx: X offset of src glyph relative to dst.
 * @dy: Y offset of src glyph relative to dst.
 *
 * Composites @src over @dst using saturation arithmetic. This results
 * in "perfect" results when glyphs are disjoint (including the case
 * when glyphs abut), but somewhat darker than "perfect" results for
 * geometries involving overlap.
 **/
static void
rsvg_ft_glyph_composite (RsvgFTGlyph *dst, const RsvgFTGlyph *src,
			 int dx, int dy)
{
	int x, y;
	int x0, y0;
	int x1, y1;
	int width;
	guchar *dst_line, *src_line;

	x0 = MAX (0, dx);
	x1 = MIN (dst->width, dx + src->width);
	width = x1 - x0;
	if (width <= 0)
		return;

	y0 = MAX (0, dy);
	y1 = MIN (dst->height, dy + src->height);
	src_line = src->buf + (y0 - dy) * src->rowstride + x0 - dx;
	dst_line = dst->buf + y0 * dst->rowstride + x0;
	for (y = y0; y < y1; y++) {
		for (x = 0; x < width; x++) {
			int v = src_line[x] + dst_line[x];
			v |= -(v >> 8); /* fast for v = v > 255 ? 255 : v */
			dst_line[x] = v;
		}
		src_line += src->rowstride;
		dst_line += dst->rowstride;
	}
}

/**
 * rsvg_ft_get_glyph: Get a rendered glyph.
 * @font: The font.
 * @glyph_ix: Glyph index.
 * @sx: Width of em in pixels.
 * @sy: Height of em in pixels.
 * @affine: Affine transformation.
 * @xy: Where to store the top left coordinates.
 *
 * Note: The nominal resolution is 72 dpi. For rendering at other resolutions,
 * scale the @affine by resolution/(72 dpi).
 *
 * Return value: The rendered glyph.
 **/
static RsvgFTGlyph *
rsvg_ft_get_glyph (RsvgFTFont *font, FT_UInt glyph_ix, double sx, double sy,
		   const double affine[6], int xy[2])
{
	RsvgFTGlyph *result;
	FT_Error error;
	FT_GlyphSlot glyph;
	FT_Face face = font->face;
	int x0, y0, x1, y1;
	int width, height;
	FT_Matrix matrix;
	FT_Vector delta;
	double expansion, scale;

	if (glyph_ix == 0)
		return NULL;

	expansion = art_affine_expansion (affine);
	scale = 0x10000 / expansion;

	error = FT_Set_Char_Size (face,
				  FT_FROMFLOAT(sx * expansion),
				  FT_FROMFLOAT(sy * expansion),
				  72, 72);

	if (error)
		return NULL;

	matrix.xx = (int)floor (affine[0] * scale + 0.5);
	matrix.yx = -(int)floor (affine[1] * scale + 0.5);
	matrix.xy = -(int)floor (affine[2] * scale + 0.5);
	matrix.yy = (int)floor (affine[3] * scale + 0.5);
	delta.x = FT_FROMFLOAT(affine[4]);
	delta.y = FT_FROMFLOAT(-affine[5]);

	FT_Set_Transform (face, &matrix, &delta);

	error = FT_Load_Glyph (face, glyph_ix, FT_LOAD_NO_HINTING);
	if (error)
		return NULL;

	glyph = face->glyph;

	x0 = FT_TRUNC(FT_FLOOR(glyph->metrics.horiBearingX));
	x1 = FT_TRUNC(FT_CEIL(glyph->metrics.horiBearingX + glyph->metrics.width));
	y0 = FT_TRUNC(FT_FLOOR(glyph->metrics.horiBearingY - glyph->metrics.height));
	y1 = FT_TRUNC(FT_CEIL(glyph->metrics.horiBearingY));
	width = x1 - x0;
	height = y1 - y0;


	if (glyph->format == ft_glyph_format_outline) {
		FT_Bitmap *bitmap;
		guchar *buf;
		int bufsize;

		error = FT_Render_Glyph (glyph, ft_render_mode_normal);
		if (error) {
			return NULL;
		}

		bitmap = &glyph->bitmap;

		result = g_new (RsvgFTGlyph, 1);
		result->refcnt = 1;
		xy[0] = glyph->bitmap_left;
		xy[1] = -glyph->bitmap_top;
		result->width = bitmap->width;
		result->height = bitmap->rows;
		result->xpen = FT_TOFLOAT (delta.x + glyph->advance.x) - xy[0];
		result->ypen = -FT_TOFLOAT (delta.y + glyph->advance.y) - xy[1];
		result->rowstride = bitmap->pitch;
		bufsize = bitmap->pitch * bitmap->rows;
		buf = g_malloc (bufsize);
		memcpy (buf, bitmap->buffer, bufsize);
		result->buf = buf;
	}
	return result;
}

/**
 * rsvg_ft_render_string: Render a string into a glyph image.
 * @ctx: The Rsvg FT context.
 * @fh: Font handle for the font.
 * @str: String, in ISO-8859-1 encoding.
 * @sx: Width of em in pixels.
 * @sy: Height of em in pixels.
 * @affine: Affine transformation.
 * @xy: Where to store the top left coordinates.
 *
 * Return value: A glyph containing the rendered string.
 **/
RsvgFTGlyph *
rsvg_ft_render_string (RsvgFTCtx *ctx, RsvgFTFontHandle fh,
		       const char *str, double sx, double sy,
		       const double affine[6], int xy[2])
{
	RsvgFTFont *font;
	RsvgFTGlyph *result;
	const int len = strlen (str);
	RsvgFTGlyph **glyphs;
	int *glyph_xy;
	int i, j;
	ArtIRect bbox, glyph_bbox;
	int rowstride;
	guchar *buf;
	double glyph_affine[6];
	FT_UInt glyph_index;
	FT_UInt last_glyph = 0; /* for kerning */

	font = rsvg_ft_font_resolve (ctx, fh);
	if (font == NULL)
		return NULL;

	bbox.x0 = bbox.x1 = 0;

	glyphs = g_new (RsvgFTGlyph *, len);
	glyph_xy = g_new (int, len * 2);

	for (j = 0; j < 6; j++)
		glyph_affine[j] = affine[j];

	for (i = 0; i < len; i++) {
		RsvgFTGlyph *glyph;

		glyph_index = FT_Get_Char_Index (font->face,
						 ((unsigned char *)str)[i]);

		if (last_glyph != 0 && glyph_index != 0) {
			FT_Vector kern;
			double kx, ky;

			/* note: ft_kerning_unscaled seems to do the
			   right thing with non-trivial affine transformations.
			   However, ft_kerning_default may be a better choice
			   for straight text rendering. This probably needs
			   a little more thought. */
			FT_Get_Kerning (font->face, last_glyph, glyph_index,
					ft_kerning_unscaled,
					&kern);
/* 			fprintf (stderr, "kern = (%ld, %ld)\n", kern.x, kern.y); */
			kx = FT_TOFLOAT (kern.x);
			ky = FT_TOFLOAT (kern.y);
			glyph_affine[4] += glyph_affine[0] * kx +
				glyph_affine[2] * ky;
			glyph_affine[5] += glyph_affine[1] * kx +
				glyph_affine[3] * ky;
		}
		if (glyph_index != 0)
			last_glyph = glyph_index;

		glyph = rsvg_ft_get_glyph (font, glyph_index,
					   sx, sy, glyph_affine,
					   glyph_xy + i * 2);
		/* todo: handle glyph == NULL */
		glyphs[i] = glyph;

		glyph_bbox.x0 = glyph_xy[i * 2];
		glyph_bbox.y0 = glyph_xy[i * 2 + 1];
		glyph_bbox.x1 = glyph_bbox.x0 + glyph->width;
		glyph_bbox.y1 = glyph_bbox.y0 + glyph->height;

		art_irect_union (&bbox, &bbox, &glyph_bbox);

		glyph_affine[4] = glyph->xpen + glyph_bbox.x0;
		glyph_affine[5] = glyph->ypen + glyph_bbox.y0;
	}

	rowstride = (bbox.x1 - bbox.x0 + 3) & -4;
	buf = g_malloc0 (rowstride * (bbox.y1 - bbox.y0));

	result = g_new (RsvgFTGlyph, 1);
	result->refcnt = 1;
	result->width = bbox.x1 - bbox.x0;
	result->height = bbox.y1 - bbox.y0;
	xy[0] = bbox.x0;
	xy[1] = bbox.y0;
	result->xpen = glyph_affine[4] - bbox.x0;
	result->ypen = glyph_affine[5] - bbox.y0;
	result->rowstride = rowstride;
	result->buf = buf;

	for (i = 0; i < len; i++) {
		rsvg_ft_glyph_composite (result, glyphs[i],
					 glyph_xy[i * 2] - bbox.x0,
					 glyph_xy[i * 2 + 1] - bbox.y0);
		rsvg_ft_glyph_unref (glyphs[i]);
	}

	g_free (glyphs);
	g_free (glyph_xy);

	return result;
}

#if 0
void
rsvg_ft_font_ref (RsvgFTFont *font)
{
	font->refcnt++;
}

void
rsvg_ft_font_unref (RsvgFTFont *font)
{
	if (--font->refcnt == 0) {
		FT_Done_Face (font->face);
		g_free (font);
	}
}
#endif

void
rsvg_ft_glyph_unref (RsvgFTGlyph *glyph)
{
	if (--glyph->refcnt == 0) {
		g_free (glyph->buf);
		g_free (glyph);
	}
}

