/* 
   rsvg-bpath-util.c: Data structure and convenience functions for creating bezier paths.
 
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
#include <math.h>
#include <string.h>
#include "rsvg-bpath-util.h"

/* This is adapted from gnome-canvas-bpath-util in libgnomeprint
   (originally developed as part of Gill). */

RsvgBpathDef *
rsvg_bpath_def_new (void)
{
  RsvgBpathDef *bpd;

  bpd = g_new (RsvgBpathDef, 1);
  bpd->n_bpath = 0;
  bpd->n_bpath_max = 16;
  bpd->moveto_idx = -1;
  bpd->bpath = g_new (ArtBpath, bpd->n_bpath_max);
  bpd->ref_count = 1;

  return bpd;
}

RsvgBpathDef *
rsvg_bpath_def_new_from (ArtBpath *path)
{
  RsvgBpathDef *bpd;
  int i;

  g_return_val_if_fail (path != NULL, NULL);

  for (i = 0; path[i].code != ART_END; i++)
    ;
  if (i <= 0)
    return rsvg_bpath_def_new ();

  bpd = g_new (RsvgBpathDef, 1);

  bpd->n_bpath = i;
  bpd->n_bpath_max = i;
  bpd->moveto_idx = -1;
  bpd->ref_count = 1;
  bpd->bpath = g_new (ArtBpath, i);

  memcpy (bpd->bpath, path, i * sizeof (ArtBpath));
  return bpd;
}

RsvgBpathDef *
rsvg_bpath_def_ref (RsvgBpathDef *bpd)
{
  g_return_val_if_fail (bpd != NULL, NULL);

  bpd->ref_count += 1;
  return bpd;
}

void
rsvg_bpath_def_free (RsvgBpathDef *bpd)
{
  g_return_if_fail (bpd != NULL);

  bpd->ref_count -= 1;
  if (bpd->ref_count == 0)
    {
      g_free (bpd->bpath);
      g_free (bpd);
    }
}

void
rsvg_bpath_def_moveto (RsvgBpathDef *bpd, double x, double y)
{
  ArtBpath *bpath;
  int n_bpath;

  g_return_if_fail (bpd != NULL);

  n_bpath = bpd->n_bpath++;

  if (n_bpath == bpd->n_bpath_max)
    bpd->bpath = g_realloc (bpd->bpath,
			    (bpd->n_bpath_max <<= 1) * sizeof (ArtBpath));
  bpath = bpd->bpath;
  bpath[n_bpath].code = ART_MOVETO_OPEN;
  bpath[n_bpath].x3 = x;
  bpath[n_bpath].y3 = y;
  bpd->moveto_idx = n_bpath;
}

void
rsvg_bpath_def_lineto (RsvgBpathDef *bpd, double x, double y)
{
  ArtBpath *bpath;
  int n_bpath;

  g_return_if_fail (bpd != NULL);
  g_return_if_fail (bpd->moveto_idx >= 0);

  n_bpath = bpd->n_bpath++;

  if (n_bpath == bpd->n_bpath_max)
    bpd->bpath = g_realloc (bpd->bpath,
			    (bpd->n_bpath_max <<= 1) * sizeof (ArtBpath));
  bpath = bpd->bpath;
  bpath[n_bpath].code = ART_LINETO;
  bpath[n_bpath].x3 = x;
  bpath[n_bpath].y3 = y;
}

void
rsvg_bpath_def_curveto (RsvgBpathDef *bpd, double x1, double y1, double x2, double y2, double x3, double y3)
{
  ArtBpath *bpath;
  int n_bpath;

  g_return_if_fail (bpd != NULL);
  g_return_if_fail (bpd->moveto_idx >= 0);

  n_bpath = bpd->n_bpath++;

  if (n_bpath == bpd->n_bpath_max)
    bpd->bpath = g_realloc (bpd->bpath,
			    (bpd->n_bpath_max <<= 1) * sizeof (ArtBpath));
  bpath = bpd->bpath;
  bpath[n_bpath].code = ART_CURVETO;
  bpath[n_bpath].x1 = x1;
  bpath[n_bpath].y1 = y1;
  bpath[n_bpath].x2 = x2;
  bpath[n_bpath].y2 = y2;
  bpath[n_bpath].x3 = x3;
  bpath[n_bpath].y3 = y3;
}

void
rsvg_bpath_def_closepath (RsvgBpathDef *bpd)
{
  ArtBpath *bpath;
  int n_bpath;

  g_return_if_fail (bpd != NULL);
  g_return_if_fail (bpd->moveto_idx >= 0);
  g_return_if_fail (bpd->n_bpath > 0);

  bpath = bpd->bpath;
  n_bpath = bpd->n_bpath;

  /* Add closing vector if we need it. */
  if (bpath[n_bpath - 1].x3 != bpath[bpd->moveto_idx].x3 ||
      bpath[n_bpath - 1].y3 != bpath[bpd->moveto_idx].y3)
    {
      rsvg_bpath_def_lineto (bpd, bpath[bpd->moveto_idx].x3,
			     bpath[bpd->moveto_idx].y3);
      bpath = bpd->bpath;
    }
  bpath[bpd->moveto_idx].code = ART_MOVETO;
  bpd->moveto_idx = -1;
}

void
rsvg_bpath_def_art_finish (RsvgBpathDef *bpd)
{
  int n_bpath;

  g_return_if_fail (bpd != NULL);

  n_bpath = bpd->n_bpath++;

  if (n_bpath == bpd->n_bpath_max)
    bpd->bpath = g_realloc (bpd->bpath,
			    (bpd->n_bpath_max <<= 1) * sizeof (ArtBpath));
  bpd->bpath[n_bpath].code = ART_END;
}
