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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <glib.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_rgb_svp.h>
#include <libart_lgpl/art_svp_vpath_stroke.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_svp_wind.h>

#include "art_rgba.h"

#include "art_render.h"
#include "art_render_gradient.h"
#include "art_render_svp.h"
#include "art_render_mask.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gnome-xml/SAX.h>
#include <gnome-xml/xmlmemory.h>

#include "rsvg-bpath-util.h"
#include "rsvg-defs.h"
#include "rsvg-path.h"
#include "rsvg-css.h"
#include "rsvg-paint-server.h"
#include "rsvg-ft.h"
#include "rsvg.h"

#define noVERBOSE

typedef struct _RsvgCtx RsvgCtx;
typedef struct _RsvgState RsvgState;
typedef struct _RsvgSaxHandler RsvgSaxHandler;

struct _RsvgCtx {
  GdkPixbuf *pixbuf;

  double zoom;

  /* stack; there is a state for each element */
  RsvgState *state;
  int n_state;
  int n_state_max;

  RsvgDefs *defs;

  RsvgSaxHandler *handler; /* should this be a handler stack? */
  int handler_nest;

  GHashTable *entities; /* g_malloc'd string -> xmlEntityPtr */

  RsvgFTCtx *ft_ctx;
};

struct _RsvgState {
  double affine[6];

  gint opacity; /* 0..255 */

  RsvgPaintServer *fill;
  gint fill_opacity; /* 0..255 */

  RsvgPaintServer *stroke;
  gint stroke_opacity; /* 0..255 */
  double stroke_width;

  ArtPathStrokeCapType cap;
  ArtPathStrokeJoinType join;

  double font_size;

  guint32 stop_color; /* rgb */
  gint stop_opacity; /* 0..255 */

  gboolean in_defs;

  GdkPixbuf *save_pixbuf;
};

struct _RsvgSaxHandler {
  void (*free) (RsvgSaxHandler *self);
  void (*start_element) (RsvgSaxHandler *self, const xmlChar *name, const xmlChar **atts);
  void (*end_element) (RsvgSaxHandler *self, const xmlChar *name);
  void (*characters) (RsvgSaxHandler *self, const xmlChar *ch, int len);
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
  result->defs = rsvg_defs_new ();
  result->handler = NULL;
  result->handler_nest = 0;
  result->entities = g_hash_table_new (g_str_hash, g_str_equal);
  result->ft_ctx = NULL;
  return result;
}

static void
rsvg_state_init (RsvgState *state)
{
  memset (state, 0, sizeof (*state));

  art_affine_identity (state->affine);

  state->opacity = 0xff;
  state->fill = rsvg_paint_server_parse (NULL, "#000");
  state->fill_opacity = 0xff;
  state->stroke_opacity = 0xff;
  state->stroke_width = 1;
  state->cap = ART_PATH_STROKE_CAP_BUTT;
  state->join = ART_PATH_STROKE_JOIN_MITER;
  state->stop_opacity = 0xff;
}

static void
rsvg_state_clone (RsvgState *dst, const RsvgState *src)
{
  *dst = *src;
  rsvg_paint_server_ref (dst->fill);
  rsvg_paint_server_ref (dst->stroke);
  dst->save_pixbuf = NULL;
}

static void
rsvg_state_finalize (RsvgState *state)
{
  rsvg_paint_server_unref (state->fill);
  rsvg_paint_server_unref (state->stroke);
}

static void
rsvg_ctx_free_helper (gpointer key, gpointer value, gpointer user_data)
{
  xmlEntityPtr entval = (xmlEntityPtr)value;

  /* key == entval->name, so it's implicitly freed below */

  g_free ((xmlChar *)entval->name);
  g_free ((xmlChar *)entval->ExternalID);
  g_free ((xmlChar *)entval->SystemID);
  xmlFree (entval->content);
  xmlFree (entval->orig);
  g_free (entval);
}

/* does not destroy the pixbuf */
static void
rsvg_ctx_free (RsvgCtx *ctx)
{
  int i;

  if (ctx->ft_ctx != NULL)
    rsvg_ft_ctx_done (ctx->ft_ctx);
  rsvg_defs_free (ctx->defs);

  for (i = 0; i < ctx->n_state; i++)
    rsvg_state_finalize (&ctx->state[i]);
  g_free (ctx->state);

  g_hash_table_foreach (ctx->entities, rsvg_ctx_free_helper, NULL);
  g_hash_table_destroy (ctx->entities);

  g_free (ctx);
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

      if (width < 0 || height < 0)
	{
	  g_warning ("rsvg_start_svg: width and height attributes are not present in SVG\n");
	  if (width < 0) width = 500;
	  if (height < 0) height = 500;
	}

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

/* Parse a CSS2 style argument, setting the SVG context attributes. */
static void
rsvg_parse_style_arg (RsvgCtx *ctx, RsvgState *state, const char *str)
{
  int arg_off;

  arg_off = rsvg_css_param_arg_offset (str);
  if (rsvg_css_param_match (str, "opacity"))
    {
      state->opacity = rsvg_css_parse_opacity (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "fill"))
    {
      rsvg_paint_server_unref (state->fill);
      state->fill = rsvg_paint_server_parse (ctx->defs, str + arg_off);
    }
  else if (rsvg_css_param_match (str, "fill-opacity"))
    {
      state->fill_opacity = rsvg_css_parse_opacity (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stroke"))
    {
      rsvg_paint_server_unref (state->stroke);
      state->stroke = rsvg_paint_server_parse (ctx->defs, str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stroke-width"))
    {
      int fixed; 
      state->stroke_width = rsvg_css_parse_length (str + arg_off, &fixed);
    }
  else if (rsvg_css_param_match (str, "stroke-linecap"))
    {
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
      state->stroke_opacity = rsvg_css_parse_opacity (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stroke-linejoin"))
    {
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
      state->font_size = rsvg_css_parse_fontsize (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "font-family"))
    {
      /* state->font_family = g_strdup (str + arg_off); */
    }
  else if (rsvg_css_param_match (str, "stop-color"))
    {
      state->stop_color = rsvg_css_parse_color (str + arg_off);
    }
  else if (rsvg_css_param_match (str, "stop-opacity"))
    {
      state->stop_opacity = rsvg_css_parse_opacity (str + arg_off);
    }
}

/* Split a CSS2 style into individual style arguments, setting attributes
   in the SVG context.

   It's known that this is _way_ out of spec. A more complete CSS2
   implementation will happen later.
*/
static void
rsvg_parse_style (RsvgCtx *ctx, RsvgState *state, const char *str)
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
      rsvg_parse_style_arg (ctx, state, arg);
      g_free (arg);
      start = end;
      if (str[start] == ';') start++;
      while (str[start] == ' ') start++;
    }
}

/* Parse an SVG transform string into an affine matrix. Reference: SVG
   working draft dated 1999-07-06, section 8.5. Return TRUE on
   success. */
static gboolean
rsvg_parse_transform (double dst[6], const char *src)
{
  int idx;
  char keyword[32];
  double args[6];
  int n_args;
  guint key_len;
  double tmp_affine[6];

  art_affine_identity (dst);

  idx = 0;
  while (src[idx])
    {
      /* skip initial whitespace */
      while (isspace (src[idx]))
	idx++;

      /* parse keyword */
      for (key_len = 0; key_len < sizeof (keyword); key_len++)
	{
	  char c;

	  c = src[idx];
	  if (isalpha (c) || c == '-')
	    keyword[key_len] = src[idx++];
	  else
	    break;
	}
      if (key_len >= sizeof (keyword))
	return FALSE;
      keyword[key_len] = '\0';

      /* skip whitespace */
      while (isspace (src[idx]))
	idx++;

      if (src[idx] != '(')
	return FALSE;
      idx++;

      for (n_args = 0; ; n_args++)
	{
	  char c;
	  char *end_ptr;

	  /* skip whitespace */
	  while (isspace (src[idx]))
	    idx++;
	  c = src[idx];
	  if (isdigit (c) || c == '+' || c == '-' || c == '.')
	    {
	      if (n_args == sizeof(args) / sizeof(args[0]))
		return FALSE; /* too many args */
	      args[n_args] = strtod (src + idx, &end_ptr);
	      idx = end_ptr - src;

	      while (isspace (src[idx]))
		idx++;

	      /* skip optional comma */
	      if (src[idx] == ',')
		idx++;
	    }
	  else if (c == ')')
	    break;
	  else
	    return FALSE;
	}
      idx++;

      /* ok, have parsed keyword and args, now modify the transform */
      if (!strcmp (keyword, "matrix"))
	{
	  if (n_args != 6)
	    return FALSE;
	  art_affine_multiply (dst, args, dst);
	}
      else if (!strcmp (keyword, "translate"))
	{
	  if (n_args == 1)
	    args[1] = 0;
	  else if (n_args != 2)
	    return FALSE;
	  art_affine_translate (tmp_affine, args[0], args[1]);
	  art_affine_multiply (dst, tmp_affine, dst);
	}
      else if (!strcmp (keyword, "scale"))
	{
	  if (n_args == 1)
	    args[1] = args[0];
	  else if (n_args != 2)
	    return FALSE;
	  art_affine_scale (tmp_affine, args[0], args[1]);
	  art_affine_multiply (dst, tmp_affine, dst);
	}
      else if (!strcmp (keyword, "rotate"))
	{
	  if (n_args != 1)
	    return FALSE;
	  art_affine_rotate (tmp_affine, args[0]);
	  art_affine_multiply (dst, tmp_affine, dst);
	}
      else if (!strcmp (keyword, "skewX"))
	{
	  if (n_args != 1)
	    return FALSE;
	  art_affine_shear (tmp_affine, args[0]);
	  art_affine_multiply (dst, tmp_affine, dst);
	}
      else if (!strcmp (keyword, "skewY"))
	{
	  if (n_args != 1)
	    return FALSE;
	  art_affine_shear (tmp_affine, args[0]);
	  /* transpose the affine, given that we know [1] is zero */
	  tmp_affine[1] = tmp_affine[2];
	  tmp_affine[2] = 0;
	  art_affine_multiply (dst, tmp_affine, dst);
	}
      else
	return FALSE; /* unknown keyword */
    }
  return TRUE;
}

/**
 * rsvg_parse_transform_attr: Parse transform attribute and apply to state.
 * @ctx: Rsvg context.
 * @state: State in which to apply the transform.
 * @str: String containing transform.
 *
 * Parses the transform attribute in @str and applies it to @state.
 **/
static void
rsvg_parse_transform_attr (RsvgCtx *ctx, RsvgState *state, const char *str)
{
  double affine[6];

  if (rsvg_parse_transform (affine, str))
    {
      art_affine_multiply (state->affine, affine, state->affine);
    }
  else
    {
      /* parse error for transform attribute. todo: report */
    }
}

/**
 * rsvg_parse_style_attrs: Parse style attribute.
 * @ctx: Rsvg context.
 * @atts: Attributes in SAX style.
 *
 * Parses style and transform attributes and modifies state at top of
 * stack.
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
	    rsvg_parse_style (ctx, &ctx->state[ctx->n_state - 1],
			      (char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "transform"))
	    rsvg_parse_transform_attr (ctx, &ctx->state[ctx->n_state - 1],
				       (char *)atts[i + 1]);
	}
    }
}

/**
 * rsvg_push_opacity_group: Begin a new transparency group.
 * @ctx: Context in which to push.
 *
 * Pushes a new transparency group onto the stack. The top of the stack
 * is stored in the context, while the "saved" value is in the state
 * stack.
 **/
static void
rsvg_push_opacity_group (RsvgCtx *ctx)
{
  RsvgState *state;
  GdkPixbuf *pixbuf;
  art_u8 *pixels;
  int width, height, rowstride;

  state = &ctx->state[ctx->n_state - 1];
  pixbuf = ctx->pixbuf;

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      g_warning ("push/pop transparency group on non-alpha buffer nyi");
      return;
    }

  state->save_pixbuf = pixbuf;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = g_new (art_u8, rowstride * height);
  memset (pixels, 0, rowstride * height);

  pixbuf = gdk_pixbuf_new_from_data (pixels,
				     GDK_COLORSPACE_RGB,
				     TRUE,
				     gdk_pixbuf_get_bits_per_sample (pixbuf),
				     width,
				     height,
				     rowstride,
				     rsvg_pixmap_destroy,
				     NULL);
  ctx->pixbuf = pixbuf;
}

/**
 * rsvg_pop_opacity_group: End a transparency group.
 * @ctx: Context in which to push.
 * @opacity: Opacity for blending (0..255).
 *
 * Pops a new transparency group from the stack, recompositing with the
 * next on stack.
 **/
static void
rsvg_pop_opacity_group (RsvgCtx *ctx, int opacity)
{
  RsvgState *state = &ctx->state[ctx->n_state - 1];
  GdkPixbuf *tos, *nos;
  art_u8 *tos_pixels, *nos_pixels;
  int width;
  int height;
  int rowstride;
  int x, y;
  int tmp;

  tos = ctx->pixbuf;
  nos = state->save_pixbuf;

  if (!gdk_pixbuf_get_has_alpha (nos))
    {
      g_warning ("push/pop transparency group on non-alpha buffer nyi");
      return;
    }

  width = gdk_pixbuf_get_width (tos);
  height = gdk_pixbuf_get_height (tos);
  rowstride = gdk_pixbuf_get_rowstride (tos);

  tos_pixels = gdk_pixbuf_get_pixels (tos);
  nos_pixels = gdk_pixbuf_get_pixels (nos);

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
	{
	  art_u8 r, g, b, a;
	  a = tos_pixels[4 * x + 3];
	  if (a)
	    {
	      r = tos_pixels[4 * x];
	      g = tos_pixels[4 * x + 1];
	      b = tos_pixels[4 * x + 2];
	      tmp = a * opacity + 0x80;
	      a = (tmp + (tmp >> 8)) >> 8;
	      art_rgba_run_alpha (nos_pixels + 4 * x, r, g, b, a, 1);
	    }
	}
      tos_pixels += rowstride;
      nos_pixels += rowstride;
    }

  gdk_pixbuf_unref (tos);
  ctx->pixbuf = nos;
}

static void
rsvg_start_g (RsvgCtx *ctx, const xmlChar **atts)
{
  RsvgState *state = &ctx->state[ctx->n_state - 1];

  rsvg_parse_style_attrs (ctx, atts);

  if (state->opacity != 0xff)
    rsvg_push_opacity_group (ctx);
}

static void
rsvg_end_g (RsvgCtx *ctx)
{
  RsvgState *state = &ctx->state[ctx->n_state - 1];

  if (state->opacity != 0xff)
    rsvg_pop_opacity_group (ctx, state->opacity);
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
 * @ps: Paint server for rendering.
 * @opacity: Opacity as 0..0xff.
 *
 * Renders the SVP over the pixbuf in @ctx.
 **/
static void
rsvg_render_svp (RsvgCtx *ctx, const ArtSVP *svp,
		 RsvgPaintServer *ps, int opacity)
{
  GdkPixbuf *pixbuf;
  ArtRender *render;
  gboolean has_alpha;

  pixbuf = ctx->pixbuf;
  /* if a pixbuf hasn't been allocated, the svg is probably misformed.  Exit
   * to avoid crashing.
   */  
  if (pixbuf == NULL) {
  	return;
  }
  
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  render = art_render_new (0, 0, 
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf),
			   gdk_pixbuf_get_pixels (pixbuf),
			   gdk_pixbuf_get_rowstride (pixbuf),
			   gdk_pixbuf_get_n_channels (pixbuf) -
			   (has_alpha ? 1 : 0),
			   gdk_pixbuf_get_bits_per_sample (pixbuf),
			   has_alpha ? ART_ALPHA_SEPARATE : ART_ALPHA_NONE,
			   NULL);

  art_render_svp (render, svp);
  art_render_mask_solid (render, (opacity << 8) + opacity + (opacity >> 7));
  rsvg_render_paint_server (render, ps, NULL); /* todo: paint server ctx */
  art_render_invoke (render);
}

static void
rsvg_render_bpath (RsvgCtx *ctx, const ArtBpath *bpath)
{
  RsvgState *state;
  ArtBpath *affine_bpath;
  ArtVpath *vpath;
  ArtSVP *svp;
  GdkPixbuf *pixbuf;
  gboolean need_tmpbuf;
  int opacity;
  int tmp;

  state = &ctx->state[ctx->n_state - 1];
  pixbuf = ctx->pixbuf;
  affine_bpath = art_bpath_affine_transform (bpath,
					     state->affine);
	
  vpath = art_bez_path_to_vec (affine_bpath, 0.25);
  art_free (affine_bpath);

  need_tmpbuf = (state->fill != NULL) && (state->stroke != NULL) &&
    state->opacity != 0xff;

  if (need_tmpbuf)
    rsvg_push_opacity_group (ctx);

  if (state->fill != NULL)
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

      opacity = state->fill_opacity;
      if (!need_tmpbuf && state->opacity != 0xff)
	{
	  tmp = opacity * state->opacity + 0x80;
	  opacity = (tmp + (tmp >> 8)) >> 8;
	}
      rsvg_render_svp (ctx, svp, state->fill, opacity);
      art_svp_free (svp);
    }

  if (state->stroke != NULL)
    {
      /* todo: libart doesn't yet implement anamorphic scaling of strokes */
      double stroke_width = state->stroke_width *
	art_affine_expansion (state->affine);

      if (stroke_width < 0.25)
	stroke_width = 0.25;

      svp = art_svp_vpath_stroke (vpath, state->join, state->cap,
				  stroke_width, 4, 0.25);
      opacity = state->stroke_opacity;
      if (!need_tmpbuf && state->opacity != 0xff)
	{
	  tmp = opacity * state->opacity + 0x80;
	  opacity = (tmp + (tmp >> 8)) >> 8;
	}
      rsvg_render_svp (ctx, svp, state->stroke, opacity);
      art_svp_free (svp);
    }

  if (need_tmpbuf)
    rsvg_pop_opacity_group (ctx, state->opacity);

  art_free (vpath);
}

static void
rsvg_start_path (RsvgCtx *ctx, const xmlChar **atts)
{
  int i;
  char *d = NULL;

  rsvg_parse_style_attrs (ctx, atts);
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

/* begin text - this should likely get split into its own .c file */

typedef struct _RsvgSaxHandlerText RsvgSaxHandlerText;

struct _RsvgSaxHandlerText {
  RsvgSaxHandler super;
  RsvgCtx *ctx;
  double xpos;
  double ypos;
};

static void
rsvg_text_handler_free (RsvgSaxHandler *self)
{
  g_free (self);
}

static void
rsvg_text_handler_characters (RsvgSaxHandler *self, const xmlChar *ch, int len)
{
  RsvgSaxHandlerText *z = (RsvgSaxHandlerText *)self;
  RsvgCtx *ctx = z->ctx;
  char *string;
  int beg, end;
  RsvgFTFontHandle fh;
  RsvgFTGlyph *glyph;
  int glyph_xy[2];
  RsvgState *state;
  ArtRender *render;
  GdkPixbuf *pixbuf;
  gboolean has_alpha;
  int opacity;

  /* Copy ch into string, chopping off leading and trailing whitespace */
  for (beg = 0; beg < len; beg++)
    if (!isspace (ch[beg]))
      break;

  for (end = len; end > beg; end--)
    if (!isspace (ch[end - 1]))
      break;

  string = g_malloc (end - beg + 1);
  memcpy (string, ch + beg, end - beg);
  string[end - beg] = 0;


#ifdef VERBOSE
  fprintf (stderr, "text characters(%s, %d)\n", string, len);
#endif

  if (ctx->ft_ctx == NULL)
    ctx->ft_ctx = rsvg_ft_ctx_new ();

  /* FIXME bugzilla.eazel.com 3904: We need to make rsvg use the 
   * Nautilus font mapping stuff in NautilusScalableFont.  See bug
   * for details.
   */
  fh = rsvg_ft_intern (ctx->ft_ctx,
		       NAUTILUS_DATADIR "/fonts/urw/n019003l.pfb");
  rsvg_ft_font_attach (ctx->ft_ctx, fh,
		       NAUTILUS_DATADIR "/fonts/urw/n019003l.afm");

  state = &ctx->state[ctx->n_state - 1];

  if (state->fill != NULL && state->font_size > 0)
    {
      pixbuf = ctx->pixbuf;
      has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

      render = art_render_new (0, 0, 
			       gdk_pixbuf_get_width (pixbuf),
			       gdk_pixbuf_get_height (pixbuf),
			       gdk_pixbuf_get_pixels (pixbuf),
			       gdk_pixbuf_get_rowstride (pixbuf),
			       gdk_pixbuf_get_n_channels (pixbuf) -
			       (has_alpha ? 1 : 0),
			       gdk_pixbuf_get_bits_per_sample (pixbuf),
			       has_alpha ? ART_ALPHA_SEPARATE : ART_ALPHA_NONE,
			       NULL);

      glyph = rsvg_ft_render_string (ctx->ft_ctx, fh, 
				     string,
				     strlen (string),
				     state->font_size, state->font_size,
				     state->affine, glyph_xy);

      rsvg_render_paint_server (render, state->fill, NULL); /* todo: paint server ctx */
      opacity = state->fill_opacity * state->opacity;
      opacity = opacity + (opacity >> 7) + (opacity >> 14);
#ifdef VERBOSE
      fprintf (stderr, "opacity = %d\n", opacity);
#endif
      art_render_mask_solid (render, opacity);
      art_render_mask (render,
		       glyph_xy[0], glyph_xy[1],
		       glyph_xy[0] + glyph->width, glyph_xy[1] + glyph->height,
		       glyph->buf, glyph->rowstride);
      art_render_invoke (render);
      rsvg_ft_glyph_unref (glyph);
    }

  g_free (string);
}

static void
rsvg_start_text (RsvgCtx *ctx, const xmlChar **atts)
{
  RsvgSaxHandlerText *handler = g_new0 (RsvgSaxHandlerText, 1);

  handler->super.free = rsvg_text_handler_free;
  handler->super.characters = rsvg_text_handler_characters;
  handler->ctx = ctx;

  /* todo: parse "x" and "y" attributes */
  handler->xpos = 0;
  handler->ypos = 0;

  rsvg_parse_style_attrs (ctx, atts);
  ctx->handler = &handler->super;
#ifdef VERBOSE
  fprintf (stderr, "begin text!\n");
#endif
}

/* end text */

static void
rsvg_start_defs (RsvgCtx *ctx, const xmlChar **atts)
{
  RsvgState *state = &ctx->state[ctx->n_state - 1];

  state->in_defs = TRUE;
}

typedef struct _RsvgSaxHandlerGstops RsvgSaxHandlerGstops;

struct _RsvgSaxHandlerGstops {
  RsvgSaxHandler super;
  RsvgCtx *ctx;
  RsvgGradientStops *stops;
};

static void
rsvg_gradient_stop_handler_free (RsvgSaxHandler *self)
{
  g_free (self);
}

static void
rsvg_gradient_stop_handler_start (RsvgSaxHandler *self, const xmlChar *name,
				  const xmlChar **atts)
{
  RsvgSaxHandlerGstops *z = (RsvgSaxHandlerGstops *)self;
  RsvgGradientStops *stops = z->stops;
  int i;
  double offset = 0;
  gboolean got_offset = FALSE;
  gint fixed;
  RsvgState state;
  int n_stop;

  if (strcmp ((char *)name, "stop"))
    {
      g_warning ("unexpected <%s> element in gradient\n", name);
      return;
    }

  rsvg_state_init (&state);

  if (atts != NULL)
    {
      for (i = 0; atts[i] != NULL; i += 2)
	{
	  if (!strcmp ((char *)atts[i], "offset"))
	    {
	      offset = rsvg_css_parse_length ((char *)atts[i + 1], &fixed);
	      got_offset = TRUE;
	    }
	  else if (!strcmp ((char *)atts[i], "style"))
	    rsvg_parse_style (z->ctx, &state, (char *)atts[i + 1]);
	}
    }

  rsvg_state_finalize (&state);

  if (!got_offset)
    {
      g_warning ("gradient stop must specify offset\n");
      return;
    }

  n_stop = stops->n_stop++;
  if (n_stop == 0)
    stops->stop = g_new (RsvgGradientStop, 1);
  else if (!(n_stop & (n_stop - 1)))
    /* double the allocation if size is a power of two */
    stops->stop = g_renew (RsvgGradientStop, stops->stop, n_stop << 1);
  stops->stop[n_stop].offset = offset;
  stops->stop[n_stop].rgba = (state.stop_color << 8) | state.stop_opacity;
}

static void
rsvg_gradient_stop_handler_end (RsvgSaxHandler *self, const xmlChar *name)
{
}

static RsvgSaxHandler *
rsvg_gradient_stop_handler_new (RsvgCtx *ctx, RsvgGradientStops **p_stops)
{
  RsvgSaxHandlerGstops *gstops = g_new0 (RsvgSaxHandlerGstops, 1);
  RsvgGradientStops *stops = g_new (RsvgGradientStops, 1);

  gstops->super.free = rsvg_gradient_stop_handler_free;
  gstops->super.start_element = rsvg_gradient_stop_handler_start;
  gstops->super.end_element = rsvg_gradient_stop_handler_end;
  gstops->ctx = ctx;
  gstops->stops = stops;

  stops->n_stop = 0;
  stops->stop = NULL;

  *p_stops = stops;
  return &gstops->super;
}

static void
rsvg_linear_gradient_free (RsvgDefVal *self)
{
  RsvgLinearGradient *z = (RsvgLinearGradient *)self;

  g_free (z->stops->stop);
  g_free (z->stops);
  g_free (self);
}

static void
rsvg_start_linear_gradient (RsvgCtx *ctx, const xmlChar **atts)
{
  RsvgState *state = &ctx->state[ctx->n_state - 1];
  RsvgLinearGradient *grad;
  int i;
  char *id = NULL;
  double x1 = 0, y1 = 0, x2 = 100, y2 = 0;
  ArtGradientSpread spread = ART_GRADIENT_PAD;

  /* todo: only handles numeric coordinates in gradientUnits = userSpace */
  if (atts != NULL)
    {
      for (i = 0; atts[i] != NULL; i += 2)
	{
	  if (!strcmp ((char *)atts[i], "id"))
	    id = (char *)atts[i + 1];
	  else if (!strcmp ((char *)atts[i], "x1"))
	    x1 = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "y1"))
	    y1 = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "x2"))
	    x2 = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "y2"))
	    y2 = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "spreadMethod"))
	    {
	      if (!strcmp ((char *)atts[i + 1], "pad"))
		spread = ART_GRADIENT_PAD;
	      else if (!strcmp ((char *)atts[i + 1], "reflect"))
		spread = ART_GRADIENT_REFLECT;
	      else if (!strcmp ((char *)atts[i + 1], "repeat"))
		spread = ART_GRADIENT_REPEAT;
	    }
	}
    }

  grad = g_new (RsvgLinearGradient, 1);
  grad->super.type = RSVG_DEF_LINGRAD;
  grad->super.free = rsvg_linear_gradient_free;

  ctx->handler = rsvg_gradient_stop_handler_new (ctx, &grad->stops);

  rsvg_defs_set (ctx->defs, id, &grad->super);

  for (i = 0; i < 6; i++)
    grad->affine[i] = state->affine[i];
  grad->x1 = x1;
  grad->y1 = y1;
  grad->x2 = x2;
  grad->y2 = y2;
  grad->spread = spread;
}

static void
rsvg_radial_gradient_free (RsvgDefVal *self)
{
  RsvgRadialGradient *z = (RsvgRadialGradient *)self;

  g_free (z->stops->stop);
  g_free (z->stops);
  g_free (self);
}

static void
rsvg_start_radial_gradient (RsvgCtx *ctx, const xmlChar **atts)
{
  RsvgState *state = &ctx->state[ctx->n_state - 1];
  RsvgRadialGradient *grad;
  int i;
  char *id = NULL;
  double cx = 50, cy = 50, r = 50, fx = 50, fy = 50;

  /* todo: only handles numeric coordinates in gradientUnits = userSpace */
  if (atts != NULL)
    {
      for (i = 0; atts[i] != NULL; i += 2)
	{
	  if (!strcmp ((char *)atts[i], "id"))
	    id = (char *)atts[i + 1];
	  else if (!strcmp ((char *)atts[i], "cx"))
	    cx = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "cy"))
	    cy = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "r"))
	    r = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "fx"))
	    fx = atof ((char *)atts[i + 1]);
	  else if (!strcmp ((char *)atts[i], "fy"))
	    fy = atof ((char *)atts[i + 1]);
	}
    }

  grad = g_new (RsvgRadialGradient, 1);
  grad->super.type = RSVG_DEF_RADGRAD;
  grad->super.free = rsvg_radial_gradient_free;

  ctx->handler = rsvg_gradient_stop_handler_new (ctx, &grad->stops);

  rsvg_defs_set (ctx->defs, id, &grad->super);

  for (i = 0; i < 6; i++)
    grad->affine[i] = state->affine[i];
  grad->cx = cx;
  grad->cy = cy;
  grad->r = r;
  grad->fx = fx;
  grad->fy = fy;
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

  if (ctx->handler)
    {
      ctx->handler_nest++;
      if (ctx->handler->start_element != NULL)
	ctx->handler->start_element (ctx->handler, name, atts);
    }
  else
    {
      /* push the state stack */
      if (ctx->n_state == ctx->n_state_max)
	ctx->state = g_renew (RsvgState, ctx->state, ctx->n_state_max <<= 1);
      if (ctx->n_state)
	rsvg_state_clone (&ctx->state[ctx->n_state],
			  &ctx->state[ctx->n_state - 1]);
      else
	rsvg_state_init (ctx->state);
      ctx->n_state++;

      if (!strcmp ((char *)name, "svg"))
	rsvg_start_svg (ctx, atts);
      else if (!strcmp ((char *)name, "g"))
	rsvg_start_g (ctx, atts);
      else if (!strcmp ((char *)name, "path"))
	rsvg_start_path (ctx, atts);
      else if (!strcmp ((char *)name, "text"))
	rsvg_start_text (ctx, atts);
      else if (!strcmp ((char *)name, "defs"))
	rsvg_start_defs (ctx, atts);
      else if (!strcmp ((char *)name, "linearGradient"))
	rsvg_start_linear_gradient (ctx, atts);
      else if (!strcmp ((char *)name, "radialGradient"))
	rsvg_start_radial_gradient (ctx, atts);
    }
}

static void
rsvg_end_element (void *data, const xmlChar *name)
{
  RsvgCtx *ctx = (RsvgCtx *)data;

  if (ctx->handler_nest > 0)
    {
      if (ctx->handler->end_element != NULL)
	ctx->handler->end_element (ctx->handler, name);
      ctx->handler_nest--;
    }
  else
    {
      if (ctx->handler != NULL)
	{
	  ctx->handler->free (ctx->handler);
	  ctx->handler = NULL;
	}
      
      if (!strcmp ((char *)name, "g"))
	rsvg_end_g (ctx);

      /* pop the state stack */
      ctx->n_state--;
      rsvg_state_finalize (&ctx->state[ctx->n_state]);

#ifdef VERBOSE
      fprintf (stdout, "SAX.endElement(%s)\n", (char *) name);
#endif
    }
}

static void
rsvg_characters (void *data, const xmlChar *ch, int len)
{
  RsvgCtx *ctx = (RsvgCtx *)data;

  if (ctx->handler && ctx->handler->characters != NULL)
    ctx->handler->characters (ctx->handler, ch, len);
}

static xmlEntityPtr
rsvg_get_entity (void *data, const xmlChar *name)
{
  RsvgCtx *ctx = (RsvgCtx *)data;

  return (xmlEntityPtr)g_hash_table_lookup (ctx->entities, name);
}

static void
rsvg_entity_decl (void *data, const xmlChar *name, int type,
		  const xmlChar *publicId, const xmlChar *systemId, xmlChar *content)
{
  RsvgCtx *ctx = (RsvgCtx *)data;
  GHashTable *entities = ctx->entities;
  xmlEntityPtr entity;
  char *dupname;

  entity = g_new (xmlEntity, 1);
  entity->type = type;
  entity->len = strlen (name);
  dupname = g_strdup (name);
  entity->name = dupname;
  entity->ExternalID = g_strdup (publicId);
  entity->SystemID = g_strdup (systemId);
  entity->content = xmlMemStrdup (content);
  entity->length = strlen (content);
  entity->orig = NULL;
  g_hash_table_insert (entities, dupname, entity);
}

static xmlSAXHandler rsvgSAXHandlerStruct = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    rsvg_get_entity, /* getEntity */
    rsvg_entity_decl, /* entityDecl */
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
    rsvg_characters, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    NULL, /* xmlParserWarning */
    NULL, /* xmlParserError */
    NULL, /* xmlParserError */
    NULL, /* getParameterEntity */
};

static xmlSAXHandlerPtr rsvgSAXHandler = &rsvgSAXHandlerStruct;

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
    ctxt = xmlCreatePushParserCtxt(rsvgSAXHandler, ctx,
				   chars, res, "filename XXX");
    ctxt->replaceEntities = TRUE;
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
