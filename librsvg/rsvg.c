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

#include "art_render.h"
#include "art_render_gradient.h"
#include "art_render_svp.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "SAX.h"

#include "rsvg-bpath-util.h"
#include "rsvg-defs.h"
#include "rsvg-path.h"
#include "rsvg-css.h"
#include "rsvg-paint-server.h"
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
};

struct _RsvgState {
  double affine[6];

  RsvgPaintServer *fill;
  gint fill_opacity; /* 0...255 */

  RsvgPaintServer *stroke;
  gint stroke_opacity; /* 0..255 */
  double stroke_width;

  ArtPathStrokeCapType cap;
  ArtPathStrokeJoinType join;

  double font_size;

  guint32 stop_color; /* rgb */
  gint stop_opacity; /* 0..255 */

  gboolean in_defs;
};

struct _RsvgSaxHandler {
  void (*free) (RsvgSaxHandler *self);
  void (*start_element) (RsvgSaxHandler *self, const xmlChar *name, const xmlChar **atts);
  void (*end_element) (RsvgSaxHandler *self, const xmlChar *name);
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
  return result;
}

static void
rsvg_state_init (RsvgState *state)
{
  art_affine_identity (state->affine);

  state->fill = rsvg_paint_server_parse (NULL, "#000");
  state->fill_opacity = 0xff;
  state->stroke = NULL;
  state->stroke_opacity = 0xff;
  state->stroke_width = 1;
  state->cap = ART_PATH_STROKE_CAP_BUTT;
  state->join = ART_PATH_STROKE_JOIN_MITER;
  state->stop_color = 0;
  state->stop_opacity = 0xff;

  state->in_defs = FALSE;
}

static void
rsvg_state_clone (RsvgState *dst, const RsvgState *src)
{
  *dst = *src;
  rsvg_paint_server_ref (dst->fill);
  rsvg_paint_server_ref (dst->stroke);
}

static void
rsvg_state_finalize (RsvgState *state)
{
  rsvg_paint_server_unref (state->fill);
  rsvg_paint_server_unref (state->stroke);
}

/* does not destroy the pixbuf */
static void
rsvg_ctx_free (RsvgCtx *ctx)
{
  int i;

  rsvg_defs_free (ctx->defs);

  for (i = 0; i < ctx->n_state; i++)
    rsvg_state_finalize (&ctx->state[i]);
  free (ctx->state);

  free (ctx);
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

/* Parse a CSS2 style argument, setting the SVG context attributes. */
static void
rsvg_parse_style_arg (RsvgCtx *ctx, RsvgState *state, const char *str)
{
  int arg_off;

  arg_off = rsvg_css_param_arg_offset (str);
  if (rsvg_css_param_match (str, "opacity"))
    {
      /* state->opacity = rsvg_css_parse_opacity (str + arg_off); */
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
	    rsvg_parse_style (ctx, &ctx->state[ctx->n_state - 1],
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

  state = &ctx->state[ctx->n_state - 1];
  pixbuf = ctx->pixbuf;
  affine_bpath = art_bpath_affine_transform (bpath,
					     state->affine);
	
  vpath = art_bez_path_to_vec (affine_bpath, 0.25);
  art_free (affine_bpath);

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

      rsvg_render_svp (ctx, svp, state->fill, state->fill_opacity);
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
      rsvg_render_svp (ctx, svp, state->stroke, state->stroke_opacity);
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
  RsvgSaxHandlerGstops *gstops = g_new (RsvgSaxHandlerGstops, 1);
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
      
      /* pop the state stack */
      ctx->n_state--;
      rsvg_state_finalize (&ctx->state[ctx->n_state]);

#ifdef VERBOSE
      fprintf (stdout, "SAX.endElement(%s)\n", (char *) name);
#endif
    }
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
