/* 
   rsvg-paint-server.c: Implement the SVG paint server abstraction.
 
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

#include <string.h>
#include <ctype.h>

#include <glib.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_affine.h>
#include "art_render.h"
#include "art_render_gradient.h"

#include "rsvg-css.h"
#include "rsvg-defs.h"
#include "rsvg-paint-server.h"

typedef struct _RsvgPaintServerSolid RsvgPaintServerSolid;
typedef struct _RsvgPaintServerLinGrad RsvgPaintServerLinGrad;
typedef struct _RsvgPaintServerRadGrad RsvgPaintServerRadGrad;

struct _RsvgPaintServer {
  int refcnt;
  void (*free) (RsvgPaintServer *self);
  void (*render) (RsvgPaintServer *self, ArtRender *ar, const RsvgPSCtx *ctx);
};

struct _RsvgPaintServerSolid {
  RsvgPaintServer super;
  guint32 rgb;
};

struct _RsvgPaintServerLinGrad {
  RsvgPaintServer super;
  RsvgLinearGradient *gradient;
  ArtGradientLinear *agl;
};

struct _RsvgPaintServerRadGrad {
  RsvgPaintServer super;
  RsvgRadialGradient *gradient;
  ArtGradientRadial *agr;
};

static void
rsvg_paint_server_solid_free (RsvgPaintServer *self)
{
  g_free (self);
}

static void
rsvg_paint_server_solid_render (RsvgPaintServer *self, ArtRender *ar,
				const RsvgPSCtx *ctx)
{
  RsvgPaintServerSolid *z = (RsvgPaintServerSolid *)self;
  guint32 rgb = z->rgb;
  ArtPixMaxDepth color[3];

  color[0] = ART_PIX_MAX_FROM_8 (rgb >> 16);
  color[1] = ART_PIX_MAX_FROM_8 ((rgb >> 8) & 0xff);
  color[2] = ART_PIX_MAX_FROM_8 (rgb  & 0xff);

  art_render_image_solid (ar, color);
}

static RsvgPaintServer *
rsvg_paint_server_solid (guint32 rgb)
{
  RsvgPaintServerSolid *result = g_new (RsvgPaintServerSolid, 1);

  result->super.refcnt = 1;
  result->super.free = rsvg_paint_server_solid_free;
  result->super.render = rsvg_paint_server_solid_render;

  result->rgb = rgb;

  return &result->super;
}

static void
rsvg_paint_server_lin_grad_free (RsvgPaintServer *self)
{
  RsvgPaintServerLinGrad *z = (RsvgPaintServerLinGrad *)self;

  g_free (z->agl->stops);
  g_free (z->agl);
  g_free (self);
}

static ArtGradientStop *
rsvg_paint_art_stops_from_rsvg (RsvgGradientStops *rstops)
{
  ArtGradientStop *stops;
  int n_stop = rstops->n_stop;
  int i;

  stops = g_new (ArtGradientStop, n_stop);
  for (i = 0; i < n_stop; i++)
    {
      guint32 rgba;
      guint32 r, g, b, a;

      stops[i].offset = rstops->stop[i].offset;
      rgba = rstops->stop[i].rgba;
      /* convert from separated to premultiplied alpha */
      a = rgba & 0xff;
      r = (rgba >> 24) * a + 0x80;
      r = (r + (r >> 8)) >> 8;
      g = ((rgba >> 16) & 0xff) * a + 0x80;
      g = (g + (g >> 8)) >> 8;
      b = ((rgba >> 8) & 0xff) * a + 0x80;
      b = (b + (b >> 8)) >> 8;
      stops[i].color[0] = ART_PIX_MAX_FROM_8(r);
      stops[i].color[1] = ART_PIX_MAX_FROM_8(g);
      stops[i].color[2] = ART_PIX_MAX_FROM_8(b);
      stops[i].color[3] = ART_PIX_MAX_FROM_8(a);
    }
  return stops;
}

static void
rsvg_paint_server_lin_grad_render (RsvgPaintServer *self, ArtRender *ar,
				   const RsvgPSCtx *ctx)
{
  RsvgPaintServerLinGrad *z = (RsvgPaintServerLinGrad *)self;
  RsvgLinearGradient *rlg = z->gradient;
  ArtGradientLinear *agl;
  double x1, y1, x2, y2;
  double dx, dy, scale;

  agl = z->agl;
  if (agl == NULL)
    {
      agl = g_new (ArtGradientLinear, 1);
      agl->n_stops = rlg->stops->n_stop;
      agl->stops = rsvg_paint_art_stops_from_rsvg (rlg->stops);
      z->agl = agl;
    }

  /* compute [xy][12] in pixel space */
  /* todo: this code implicitly implements gradientUnits = userSpace */
  x1 = rlg->x1 * rlg->affine[0] + rlg->y1 * rlg->affine[2] + rlg->affine[4];
  y1 = rlg->x1 * rlg->affine[1] + rlg->y1 * rlg->affine[3] + rlg->affine[5];
  x2 = rlg->x2 * rlg->affine[0] + rlg->y2 * rlg->affine[2] + rlg->affine[4];
  y2 = rlg->x2 * rlg->affine[1] + rlg->y2 * rlg->affine[3] + rlg->affine[5];

  /* solve a, b, c so ax1 + by1 + c = 0 and ax2 + by2 + c = 1, maximum
     gradient is in x1,y1 to x2,y2 dir */
  dx = x2 - x1;
  dy = y2 - y1;
  scale = 1.0 / (dx * dx + dy * dy);
  agl->a = dx * scale;
  agl->b = dy * scale;
  agl->c = -(x1 * agl->a + y1 * agl->b);

  agl->spread = rlg->spread;
  art_render_gradient_linear (ar, agl, ART_FILTER_NEAREST);
}

static RsvgPaintServer *
rsvg_paint_server_lin_grad (RsvgLinearGradient *gradient)
{
  RsvgPaintServerLinGrad *result = g_new (RsvgPaintServerLinGrad, 1);

  result->super.refcnt = 1;
  result->super.free = rsvg_paint_server_lin_grad_free;
  result->super.render = rsvg_paint_server_lin_grad_render;

  result->gradient = gradient;
  result->agl = NULL;

  return &result->super;
}

static void
rsvg_paint_server_rad_grad_free (RsvgPaintServer *self)
{
  RsvgPaintServerRadGrad *z = (RsvgPaintServerRadGrad *)self;

  g_free (z->agr->stops);
  g_free (z->agr);
  g_free (self);
}

static void
rsvg_paint_server_rad_grad_render (RsvgPaintServer *self, ArtRender *ar,
				   const RsvgPSCtx *ctx)
{
  RsvgPaintServerRadGrad *z = (RsvgPaintServerRadGrad *)self;
  RsvgRadialGradient *rrg = z->gradient;
  ArtGradientRadial *agr;
  double aff1[6], aff2[6];

  agr = z->agr;
  if (agr == NULL)
    {
      agr = g_new (ArtGradientRadial, 1);
      agr->n_stops = rrg->stops->n_stop;
      agr->stops = rsvg_paint_art_stops_from_rsvg (rrg->stops);
      z->agr = agr;
    }

  /* todo: this code implicitly implements gradientUnits = userSpace */
  art_affine_scale (aff1, rrg->r, rrg->r);
  art_affine_translate (aff2, rrg->cx, rrg->cy);
  art_affine_multiply (aff1, aff1, aff2);
  art_affine_multiply (aff1, aff1, rrg->affine);
  art_affine_invert (agr->affine, aff1);

  agr->fx = (rrg->fx - rrg->cx) / rrg->r;
  agr->fy = (rrg->fy - rrg->cy) / rrg->r;

  art_render_gradient_radial (ar, agr, ART_FILTER_NEAREST);
}

static RsvgPaintServer *
rsvg_paint_server_rad_grad (RsvgRadialGradient *gradient)
{
  RsvgPaintServerRadGrad *result = g_new (RsvgPaintServerRadGrad, 1);

  result->super.refcnt = 1;
  result->super.free = rsvg_paint_server_rad_grad_free;
  result->super.render = rsvg_paint_server_rad_grad_render;

  result->gradient = gradient;
  result->agr = NULL;

  return &result->super;
}

/**
 * rsvg_paint_server_parse: Parse an SVG paint specification.
 * @defs: Defs for looking up gradients.
 * @str: The SVG paint specification string to parse.
 *
 * Parses the paint specification @str, creating a new paint server
 * object.
 *
 * Return value: The newly created paint server, or NULL on error.
 **/
RsvgPaintServer *
rsvg_paint_server_parse (const RsvgDefs *defs, const char *str)
{
  guint32 rgb;

  if (!strcmp (str, "none"))
    return NULL;
  if (!strncmp (str, "url(", 4))
    {
      const char *p = str + 4;
      int ix;
      char *name;
      RsvgDefVal *val;

      while (isspace (*p)) p++;
      if (*p != '#')
	return NULL;
      p++;
      for (ix = 0; p[ix]; ix++)
	if (p[ix] == ')') break;
      if (p[ix] != ')')
	return NULL;
      name = g_strndup (p, ix);
      val = rsvg_defs_lookup (defs, name);
      g_free (name);
      if (val == NULL)
	return NULL;
      switch (val->type)
	{
	case RSVG_DEF_LINGRAD:
	  return rsvg_paint_server_lin_grad ((RsvgLinearGradient *)val);
	case RSVG_DEF_RADGRAD:
	  return rsvg_paint_server_rad_grad ((RsvgRadialGradient *)val);
	default:
	  return NULL;
	}
    }
  else
    {
      rgb = rsvg_css_parse_color (str);
      return rsvg_paint_server_solid (rgb);
    }
}

/**
 * rsvg_render_paint_server: Render paint server as image source for libart.
 * @ar: Libart render object.
 * @ps: Paint server object.
 *
 * Hooks up @ps as an image source for a libart rendering operation.
 **/
void
rsvg_render_paint_server (ArtRender *ar, RsvgPaintServer *ps,
			  const RsvgPSCtx *ctx)
{
  if (ps != NULL)
    ps->render (ps, ar, ctx);
}

/**
 * rsvg_paint_server_ref: Reference a paint server object.
 * @ps: The paint server object to reference.
 **/
void
rsvg_paint_server_ref (RsvgPaintServer *ps)
{
  if (ps == NULL)
    return;
  ps->refcnt++;
}

/**
 * rsvg_paint_server_unref: Unreference a paint server object.
 * @ps: The paint server object to unreference.
 **/
void
rsvg_paint_server_unref (RsvgPaintServer *ps)
{
  if (ps == NULL)
    return;
  if (--ps->refcnt == 0)
    ps->free (ps);
}
