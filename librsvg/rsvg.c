/* 
   rsvg.c: SAX-based renderer for SVG files into a GdkPixbuf.
 
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

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <glib.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_rgb_svp.h>
#include <libart_lgpl/art_svp_vpath_stroke.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_svp_wind.h>

#include "art_rgba_svp.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "SAX.h"

#include "rsvg-bpath-util.h"
#include "rsvg-path.h"
#include "rsvg.h"

#define noVERBOSE

typedef struct _RsvgCtx RsvgCtx;
typedef struct _RsvgState RsvgState;

struct _RsvgCtx {
  GdkPixbuf *pixbuf;

  double zoom;

  /* stack; there is a state for each element */
  RsvgState *state;
  int n_state;
  int n_state_max;
};

struct _RsvgState {
  double affine[6];

  gboolean fill;
  guint32 fill_color; /* rgb */
  gint fill_opacity; /* 0...255 */

  gboolean stroke;
  guint32 stroke_color; /* rgb */
  gint stroke_opacity; /* 0..255 */
  double stroke_width;

  ArtPathStrokeCapType cap;
  ArtPathStrokeJoinType join;

  double font_size;
};

static RsvgCtx *
rsvg_ctx_new (void)
{
  RsvgCtx *result;

  result = g_new (RsvgCtx, 1);
  result->pixbuf = NULL;
  result->zoom = 1.0;
  result->n_state = 0;
  result->n_state_max = 16;
  result->state = g_new (RsvgState, result->n_state_max);
  return result;
}

/* does not destroy the pixbuf */
static void
rsvg_ctx_free (RsvgCtx *ctx)
{
  free (ctx->state);
  free (ctx);
}

static void
rsvg_state_init (RsvgState *state)
{
  art_affine_identity (state->affine);

  state->fill = 0;
  state->fill_color = 0;
  state->fill_opacity = 0xff;
  state->stroke = 0;
  state->stroke_color = 0;
  state->stroke_opacity = 0xff;
  state->stroke_width = 1;
  state->cap = ART_PATH_STROKE_CAP_BUTT;
  state->join = ART_PATH_STROKE_JOIN_MITER;
}

/**
 * rsvg_css_parse_length: Parse CSS2 length to a pixel value.
 * @str: Original string.
 * @fixed: Where to store boolean value of whether length is fixed.
 *
 * Parses a CSS2 length into a pixel value.
 *
 * Returns: returns the length.
 **/
static double
rsvg_css_parse_length (const char *str, gint *fixed)
{
  char *p;
  
  /* 
   *  The supported CSS length unit specifiers are: 
   *  em, ex, px, pt, pc, cm, mm, in, and percentages. 
   */
  
  *fixed = FALSE;

  p = strstr (str, "px");
  if (p != NULL)
    {
      *fixed = TRUE;
      return atof (str);
    }
  p = strstr (str, "in");
  if (p != NULL)
    {
      *fixed = TRUE;
      /* return svg->pixels_per_inch * atof (str); */
    }
  return atof (str);
}

static void
rsvg_pixmap_destroy (guchar *pixels, gpointer data)
{
  g_free (pixels);
}

static void
rsvg_start_svg (RsvgCtx *ctx, const xmlChar **atts)
{
  int i;
  int width = -1, height = -1;
  int rowstride;
  art_u8 *pixels;
  gint fixed;
  RsvgState *state;
  gboolean has_alpha = 1;

  if (atts != NULL)
    {
      for (i = 0; atts[i] != NULL; i += 2)
	{
	  if (!strcmp ((char *)atts[i], "width"))
	    width = rsvg_css_parse_length ((char *)atts[i + 1], &fixed);
	  else if (!strcmp ((char *)atts[i], "height"))
	    height = rsvg_css_parse_length ((char *)atts[i + 1], &fixed);
	}
#ifdef VERBOSE
      fprintf (stdout, "rsvg_start_svg: width = %d, height = %d\n",
	       width, height);
#endif

      /* Scale size of target pixbuf */
      width = ceil (width * ctx->zoom);
      height = ceil (height * ctx->zoom);

      state = &ctx->state[ctx->n_state - 1];
      art_affine_scale (state->affine, ctx->zoom, ctx->zoom);

      rowstride = (width * (has_alpha ? 4 : 3) + 3) & -4;
      pixels = g_new (art_u8, rowstride * height);
      memset (pixels, has_alpha ? 0 : 255, rowstride * height);
      ctx->pixbuf = gdk_pixbuf_new_from_data (pixels,
					      GDK_COLORSPACE_RGB,
					      has_alpha, 8,
					      width, height,
					      rowstride,
					      rsvg_pixmap_destroy,
					      NULL);
    }
}

static gboolean
rsvg_css_param_match (const char *str, const char *param_name)
{
  int i;

  for (i = 0; str[i] != '\0' && str[i] != ':'; i++)
    if (param_name[i] != str[i])
      return FALSE;
  return str[i] == ':' && param_name[i] == '\0';
}

static int
rsvg_css_param_arg_offset (const char *str)
{
  int i;

  for (i = 0; str[i] != '\0' && str[i] != ':'; i++);
  if (str[i] != '\0') i++;
  for (; str[i] == ' '; i++);
  return i;
}

/* Parse a CSS2 color, returning rgb */
static guint32
rsvg_css_parse_color (const char *str)
{
  gint val = 0;
  static GHashTable *colors = NULL;

  /* 
   * todo: handle the rgb (r, g, b) and rgb ( r%, g%, b%), syntax 
   * defined in http://www.w3.org/TR/REC-CSS2/syndata.html#color-units 
   */
#ifdef VERBOSE
  g_print ("color = %s\n", str);
#endif
  if (str[0] == '#')
    {
      int i;
      for (i = 1; str[i]; i++)
	{
	  int hexval;
	  if (str[i] >= '0' && str[i] <= '9')
	    hexval = str[i] - '0';
	  else if (str[i] >= 'A' && str[i] <= 'F')
	    hexval = str[i] - 'A' + 10;
	  else if (str[i] >= 'a' && str[i] <= 'f')
	    hexval = str[i] - 'a' + 10;
	  else
	    break;
	  val = (val << 4) + hexval;
	}
      /* handle #rgb case */
      if (i == 4)
	{
	  val = ((val & 0xf00) << 8) |
	    ((val & 0x0f0) << 4) |
	    (val & 0x00f);
	  val |= val << 4;
	}
#ifdef VERBOSE
      printf ("val = %x\n", val);
#endif
    } 
  else
    {
      GString * string;
      if (!colors)
	{
	  colors = g_hash_table_new (g_str_hash, g_str_equal);
	  
	  g_hash_table_insert (colors, "black",    GINT_TO_POINTER (0x000000));
	  g_hash_table_insert (colors, "silver",   GINT_TO_POINTER (0xc0c0c0));
	  g_hash_table_insert (colors, "gray",     GINT_TO_POINTER (0x808080));
	  g_hash_table_insert (colors, "white",    GINT_TO_POINTER (0xFFFFFF));
	  g_hash_table_insert (colors, "maroon",   GINT_TO_POINTER (0x800000));
	  g_hash_table_insert (colors, "red",      GINT_TO_POINTER (0xFF0000));
	  g_hash_table_insert (colors, "purple",   GINT_TO_POINTER (0x800080));
	  g_hash_table_insert (colors, "fuchsia",  GINT_TO_POINTER (0xFF00FF));
	  g_hash_table_insert (colors, "green",    GINT_TO_POINTER (0x008000));
	  g_hash_table_insert (colors, "lime",     GINT_TO_POINTER (0x00FF00));
	  g_hash_table_insert (colors, "olive",    GINT_TO_POINTER (0x808000));
	  g_hash_table_insert (colors, "yellow",   GINT_TO_POINTER (0xFFFF00));
	  g_hash_table_insert (colors, "navy",     GINT_TO_POINTER (0x000080));
	  g_hash_table_insert (colors, "blue",     GINT_TO_POINTER (0x0000FF));
	  g_hash_table_insert (colors, "teal",     GINT_TO_POINTER (0x008080));
	  g_hash_table_insert (colors, "aqua",     GINT_TO_POINTER (0x00FFFF));
	}

      string = g_string_down (g_string_new (str));


      /* this will default to black on a failed lookup */
      val = GPOINTER_TO_INT (g_hash_table_lookup (colors, string->str)); 
    }

  return val;
}

static guint
rsvg_css_parse_opacity (const char *str)
{
  char *end_ptr;
  double opacity;

  opacity = strtod (str, &end_ptr);

  if (end_ptr[0] == '%')
    opacity *= 0.01;

  return floor (opacity * 255 + 0.5);
}

static double
rsvg_css_parse_fontsize (RsvgState *state, const char *str)
{
  char *end_ptr;
  double size;

  /* todo: handle absolute-size and relative-size tags and proper units */
  size = strtod (str, &end_ptr);

  if (end_ptr[0] == '%')
    size = (state->font_size * size * 0.01);

  return size;
}

/* Parse a CSS2 style argument, setting the SVG context attributes. */
static void
rsvg_parse_style_arg (RsvgState *state, const char *str)
{
  int arg_off;

  if (rsvg_css_param_match (str, "opacity"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      /* state->opacity = rsvg_css_parse_opacity (str + arg_off); */
    }
  else if (rsvg_css_param_match (str, "fill"))
    {
      arg_off = rsvg_css_param_arg_offset (str); 
      if (!strcmp (str + arg_off, "none"))
	state->fill = 0;
      else
	{
	  state->fill = 1;
	  state->fill_color = rsvg_css_parse_color (str + arg_off);
	}
    }
  else if (rsvg_css_param_match (str, "fill-opacity"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      state->fill_opacity = rsvg_css_parse_opacity (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stroke"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      if (!strcmp (str + arg_off, "none"))
	state->stroke = 0;
      else
	{
	  state->stroke = 1;
	  state->stroke_color = rsvg_css_parse_color (str + arg_off);
	}
    }
  else if (rsvg_css_param_match (str, "stroke-width"))
    {
      int fixed; 
      arg_off = rsvg_css_param_arg_offset (str);
      state->stroke_width = rsvg_css_parse_length (str + arg_off, &fixed);
    }
  else if (rsvg_css_param_match (str, "stroke-linecap"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      if (!strcmp (str + arg_off, "butt"))
	state->cap = ART_PATH_STROKE_CAP_BUTT;
      else if (!strcmp (str + arg_off, "round"))
	state->cap = ART_PATH_STROKE_CAP_ROUND;
      else if (!strcmp (str + arg_off, "square"))
	state->cap = ART_PATH_STROKE_CAP_SQUARE;
      else
	g_warning ("unknown line cap style %s", str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stroke-opacity"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      state->stroke_opacity = rsvg_css_parse_opacity (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stroke-linejoin"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      if (!strcmp (str + arg_off, "miter"))
	state->join = ART_PATH_STROKE_JOIN_MITER;
      else if (!strcmp (str + arg_off, "round"))
	state->join = ART_PATH_STROKE_JOIN_ROUND;
      else if (!strcmp (str + arg_off, "bevel"))
	state->join = ART_PATH_STROKE_JOIN_BEVEL;
      else
	g_warning ("unknown line join style %s", str + arg_off);
    }
  else if (rsvg_css_param_match (str, "font-size"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      state->font_size = rsvg_css_parse_fontsize (state, str + arg_off);
    }
  else if (rsvg_css_param_match (str, "font-family"))
    {
      arg_off = rsvg_css_param_arg_offset (str);
      /* state->font_family = g_strdup (str + arg_off); */
    }

}

/* Split a CSS2 style into individual style arguments, setting attributes
   in the SVG context.

   It's known that this is _way_ out of spec. A more complete CSS2
   implementation will happen later.
*/
static void
rsvg_parse_style (RsvgState *state, const char *str)
{
  int start, end;
  char *arg;

  start = 0;
  while (str[start] != '\0')
    {
      for (end = start; str[end] != '\0' && str[end] != ';'; end++);
      arg = g_new (char, 1 + end - start);
      memcpy (arg, str + start, end - start);
      arg[end - start] = '\0';
      rsvg_parse_style_arg (state, arg);
      g_free (arg);
      start = end;
      if (str[start] == ';') start++;
      while (str[start] == ' ') start++;
    }
}

/**
 * rsvg_parse_style_attrs: Parse style attribute.
 * @ctx: Rsvg context.
 * @atts: Attributes in SAX style.
 *
 * Parses style attribute and modifies state at top of stack.
 *
 * Note: this routine will also be responsible for parsing transforms.
 **/
static void
rsvg_parse_style_attrs (RsvgCtx *ctx, const xmlChar **atts)
{
  int i;

  if (atts != NULL)
    {
      for (i = 0; atts[i] != NULL; i += 2)
	{
	  if (!strcmp ((char *)atts[i], "style"))
	    rsvg_parse_style (&ctx->state[ctx->n_state - 1],
			      (char *)atts[i + 1]);
	}
    }
}

static void
rsvg_start_g (RsvgCtx *ctx, const xmlChar **atts)
{
  rsvg_parse_style_attrs (ctx, atts);
}

/**
 * rsvg_close_vpath: Close a vector path.
 * @src: Source vector path.
 *
 * Closes any open subpaths in the vector path.
 *
 * Return value: Closed vector path, allocated with g_new.
 **/
static ArtVpath *
rsvg_close_vpath (const ArtVpath *src)
{
  ArtVpath *result;
  int n_result, n_result_max;
  int src_ix;
  double beg_x, beg_y;
  gboolean open;

  n_result = 0;
  n_result_max = 16;
  result = g_new (ArtVpath, n_result_max);

  beg_x = 0;
  beg_y = 0;
  open = FALSE;

  for (src_ix = 0; src[src_ix].code != ART_END; src_ix++)
    {
      if (n_result == n_result_max)
	result = g_renew (ArtVpath, result, n_result_max <<= 1);
      result[n_result].code = src[src_ix].code == ART_MOVETO_OPEN ?
	ART_MOVETO : src[src_ix].code;
      result[n_result].x = src[src_ix].x;
      result[n_result].y = src[src_ix].y;
      n_result++;
      if (src[src_ix].code == ART_MOVETO_OPEN)
	{
	  beg_x = src[src_ix].x;
	  beg_y = src[src_ix].y;
	  open = TRUE;
	}
      else if (src[src_ix + 1].code != ART_LINETO)
	{
	  if (open && (beg_x != src[src_ix].x || beg_y != src[src_ix].y))
	    {
	      if (n_result == n_result_max)
		result = g_renew (ArtVpath, result, n_result_max <<= 1);
	      result[n_result].code = ART_LINETO;
	      result[n_result].x = beg_x;
	      result[n_result].y = beg_y;
	      n_result++;
	    }
	  open = FALSE;
	}
    }
  if (n_result == n_result_max)
    result = g_renew (ArtVpath, result, n_result_max <<= 1);
  result[n_result].code = ART_END;
  result[n_result].x = 0.0;
  result[n_result].y = 0.0;
  return result;
}

/**
 * rsvg_render_svp: Render an SVP.
 * @ctx: Context in which to render.
 * @svp: SVP to render.
 * @rgba: Color.
 *
 * Renders the SVP over the pixbuf in @ctx.
 **/
static void
rsvg_render_svp (RsvgCtx *ctx, const ArtSVP *svp, guint32 rgba)
{
  GdkPixbuf *pixbuf;
  pixbuf = ctx->pixbuf;

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    art_rgba_svp_alpha (svp, 0, 0, gdk_pixbuf_get_width (pixbuf),
			gdk_pixbuf_get_height (pixbuf),
			rgba,
			gdk_pixbuf_get_pixels (pixbuf),
			gdk_pixbuf_get_rowstride (pixbuf),
			NULL);
  else
    art_rgb_svp_alpha (svp, 0, 0, gdk_pixbuf_get_width (pixbuf),
		       gdk_pixbuf_get_height (pixbuf),
		       rgba,
		       gdk_pixbuf_get_pixels (pixbuf),
		       gdk_pixbuf_get_rowstride (pixbuf),
		       NULL);
}

static void
rsvg_render_bpath (RsvgCtx *ctx, const ArtBpath *bpath)
{
  RsvgState *state;
  ArtBpath *affine_bpath;
  ArtVpath *vpath;
  ArtSVP *svp;
  GdkPixbuf *pixbuf;

  state = &ctx->state[ctx->n_state - 1];
  pixbuf = ctx->pixbuf;
  affine_bpath = art_bpath_affine_transform (bpath,
					     state->affine);
	
  vpath = art_bez_path_to_vec (affine_bpath, 0.25);
  art_free (affine_bpath);

  if (state->fill)
    {
      ArtVpath *closed_vpath;
      ArtVpath *perturbed_vpath;
      ArtSVP *tmp_svp;
      ArtWindRule art_wind;
			
      closed_vpath = rsvg_close_vpath (vpath);
      perturbed_vpath = art_vpath_perturb (closed_vpath);
      g_free (closed_vpath);
      svp = art_svp_from_vpath (perturbed_vpath);
      art_free (perturbed_vpath);
      tmp_svp = art_svp_uncross (svp);
      art_svp_free (svp);
      art_wind = ART_WIND_RULE_NONZERO; /* todo - get from state */
      svp = art_svp_rewind_uncrossed (tmp_svp, art_wind);
      art_svp_free (tmp_svp);

      rsvg_render_svp (ctx, svp,
		       (state->fill_color << 8) | state->fill_opacity);
      art_svp_free (svp);
    }

  if (state->stroke)
    {
      double stroke_width = state->stroke_width;

      if (stroke_width < 0.25)
	stroke_width = 0.25;

      svp = art_svp_vpath_stroke (vpath, state->join, state->cap,
				  stroke_width, 4, 0.25);
      rsvg_render_svp (ctx, svp,
		       (state->stroke_color << 8) | state->stroke_opacity);
      art_svp_free (svp);
    }
  art_free (vpath);
}

static void
rsvg_start_path (RsvgCtx *ctx, const xmlChar **atts)
{
  int i;
  char *d = NULL;

  if (atts != NULL)
    {
      for (i = 0; atts[i] != NULL; i += 2)
	{
	  if (!strcmp ((char *)atts[i], "d"))
	    d = (char *)atts[i + 1];
	}
    }
  if (d != NULL)
    {
      RsvgBpathDef *bpath_def;

      bpath_def = rsvg_parse_path (d);
      rsvg_bpath_def_art_finish (bpath_def);

      rsvg_render_bpath (ctx, bpath_def->bpath);

      rsvg_bpath_def_free (bpath_def);
    }
}

static void
rsvg_start_element (void *data, const xmlChar *name, const xmlChar **atts)
{
  RsvgCtx *ctx = (RsvgCtx *)data;
#ifdef VERBOSE
  int i;
#endif

#ifdef VERBOSE
  fprintf (stdout, "SAX.startElement(%s", (char *) name);
  if (atts != NULL) {
    for (i = 0;(atts[i] != NULL);i++) {
      fprintf (stdout, ", %s='", atts[i++]);
      fprintf (stdout, "%s'", atts[i]);
    }
  }
  fprintf (stdout, ")\n");
#endif

  /* push the state stack */
  if (ctx->n_state == ctx->n_state_max)
    ctx->state = g_renew (RsvgState, ctx->state, ctx->n_state_max <<= 1);
  if (ctx->n_state)
    ctx->state[ctx->n_state] = ctx->state[ctx->n_state - 1];
  else
    rsvg_state_init (ctx->state);
  ctx->n_state++;

  if (!strcmp ((char *)name, "svg"))
    rsvg_start_svg (ctx, atts);
  else if (!strcmp ((char *)name, "g"))
    rsvg_start_g (ctx, atts);
  else if (!strcmp ((char *)name, "path"))
    rsvg_start_path (ctx, atts);
}

static void
rsvg_end_element (void *data, const xmlChar *name)
{
  RsvgCtx *ctx = (RsvgCtx *)data;

  /* pop the state stack */
  ctx->n_state--;

#ifdef VERBOSE
  fprintf (stdout, "SAX.endElement(%s)\n", (char *) name);
#endif
}

xmlSAXHandler emptySAXHandlerStruct = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    rsvg_start_element, /* startElement */
    rsvg_end_element, /* endElement */
    NULL, /* reference */
    NULL, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    NULL, /* xmlParserWarning */
    NULL, /* xmlParserError */
    NULL, /* xmlParserError */
    NULL, /* getParameterEntity */
};

xmlSAXHandlerPtr emptySAXHandler = &emptySAXHandlerStruct;

GdkPixbuf *
rsvg_render_file (FILE *f, double zoom)
{
  int res;
  char chars[10];
  xmlParserCtxtPtr ctxt;
  RsvgCtx *ctx;
  GdkPixbuf *result;

  ctx = rsvg_ctx_new ();
  ctx->zoom = zoom;
  res = fread(chars, 1, 4, f);
  if (res > 0) {
    ctxt = xmlCreatePushParserCtxt(emptySAXHandler, ctx,
				   chars, res, "filename XXX");
    while ((res = fread(chars, 1, 3, f)) > 0) {
      xmlParseChunk(ctxt, chars, res, 0);
    }
    xmlParseChunk(ctxt, chars, 0, 1);
    xmlFreeParserCtxt(ctxt);
  }
  result = ctx->pixbuf;
  rsvg_ctx_free (ctx);
  return result;
}

#ifdef RSVG_MAIN
static void
write_pixbuf (ArtPixBuf *pixbuf)
{
  int y;
  printf ("P6\n%d %d\n255\n", pixbuf->width, pixbuf->height);
  for (y = 0; y < pixbuf->height; y++)
    fwrite (pixbuf->pixels + y * pixbuf->rowstride, 1, pixbuf->width * 3,
	    stdout);
}

int
main (int argc, char **argv)
{
  FILE *f;
  ArtPixBuf *pixbuf;

  if (argc == 1)
    f = stdin;
  else
    {
      f = fopen (argv[1], "r");
      if (f == NULL)
	{
	  fprintf (stderr, "Error opening source file %s\n", argv[1]);
	}
    }

  pixbuf = rsvg_render_file (f);

  if (f != stdin)
    fclose (f);

  write_pixbuf (pixbuf);

  return 0;
}
#endif
