/* A module for handling SVG defs */

typedef struct _RsvgDefs RsvgDefs;
typedef struct _RsvgDefVal RsvgDefVal;

typedef enum {
  /* todo: general question: should this be high level, ie a generic
     paint server, coupled with a paint server interface; or low level,
     ie specific definable things? For now, we're going low level,
     but it's not clear that's the best way to go. */
  RSVG_DEF_LINGRAD,
  RSVG_DEF_RADGRAD
} RsvgDefType;

struct _RsvgDefVal {
  RsvgDefType type;
  void (*free) (RsvgDefVal *self);
};

RsvgDefs *
rsvg_defs_new (void);

RsvgDefVal *
rsvg_defs_lookup (const RsvgDefs *defs, const char *name);

void
rsvg_defs_set (RsvgDefs *defs, const char *name, RsvgDefVal *val);

void
rsvg_defs_free (RsvgDefs *defs);
