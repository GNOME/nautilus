#ifndef RSVG_BPATH_UTIL_H
#define RSVG_BPATH_UTIL_H

#include <libart_lgpl/art_bpath.h>

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

#endif

