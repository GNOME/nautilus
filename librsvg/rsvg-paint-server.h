typedef struct _RsvgGradientStop RsvgGradientStop;
typedef struct _RsvgGradientStops RsvgGradientStops;
typedef struct _RsvgLinearGradient RsvgLinearGradient;
typedef struct _RsvgRadialGradient RsvgRadialGradient;

typedef struct _RsvgPaintServer RsvgPaintServer;

typedef struct _RsvgPSCtx RsvgPSCtx;

struct _RsvgPSCtx {
/* todo: we need to take in some context information, including:

   1. The global affine transformation.

   2. User coordinates at time of reference (to implement
   gradientUnits = "userSpaceOnUse").

   3. Object bounding box (to implement gradientUnits =
   "objectBoundingBox").

   Maybe signal for lazy evaluation of object bbox.
*/
};

struct _RsvgGradientStop {
  double offset;
  guint32 rgba;
};

struct _RsvgGradientStops {
  int n_stop;
  RsvgGradientStop *stop;
};

struct _RsvgLinearGradient {
  RsvgDefVal super;
  double affine[6]; /* user space to actual at time of gradient def */
  double x1, y1;
  double x2, y2;
  ArtGradientSpread spread;
  RsvgGradientStops *stops;
};

struct _RsvgRadialGradient {
  RsvgDefVal super;
  double affine[6]; /* user space to actual at time of gradient def */
  double cx, cy;
  double r;
  double fx, fy;
  RsvgGradientStops *stops;
};

/* Create a new paint server based on a specification string. */
RsvgPaintServer *
rsvg_paint_server_parse (const RsvgDefs *defs, const char *str);

void
rsvg_render_paint_server (ArtRender *ar, RsvgPaintServer *ps,
			  const RsvgPSCtx *ctx);

void
rsvg_paint_server_ref (RsvgPaintServer *ps);

void
rsvg_paint_server_unref (RsvgPaintServer *ps);

