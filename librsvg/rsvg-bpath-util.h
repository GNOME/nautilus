/* 
   rsvg-bpath-util.h: Data structure and convenience functions for creating bezier paths.
 
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

#ifndef RSVG_BPATH_UTIL_H
#define RSVG_BPATH_UTIL_H

#include <libart_lgpl/art_bpath.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _RsvgBpathDef RsvgBpathDef;

struct _RsvgBpathDef {
	int ref_count;
	ArtBpath *bpath;
	int n_bpath;
	int n_bpath_max;
	int moveto_idx;
};


RsvgBpathDef *rsvg_bpath_def_new (void);
RsvgBpathDef *rsvg_bpath_def_new_from (ArtBpath *bpath);
RsvgBpathDef *rsvg_bpath_def_ref (RsvgBpathDef *bpd);

#define rsvg_bpath_def_unref rsvg_bpath_def_free
void rsvg_bpath_def_free       (RsvgBpathDef *bpd);

void rsvg_bpath_def_moveto     (RsvgBpathDef *bpd,
					double x, double y);
void rsvg_bpath_def_lineto     (RsvgBpathDef *bpd,
					double x, double y);
void rsvg_bpath_def_curveto    (RsvgBpathDef *bpd,
					double x1, double y1,
					double x2, double y2,
					double x3, double y3);
void rsvg_bpath_def_closepath  (RsvgBpathDef *bpd);

void rsvg_bpath_def_art_finish (RsvgBpathDef *bpd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

