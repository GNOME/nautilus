typedef struct _RsvgFTCtx RsvgFTCtx;
typedef struct _RsvgFTGlyph RsvgFTGlyph;

struct _RsvgFTGlyph {
	int refcnt;
	int width, height;
	double xpen, ypen; /* location of pen after the glyph */
	int rowstride;
	guchar *buf;
};

typedef int RsvgFTFontHandle;

RsvgFTCtx *
rsvg_ft_ctx_new (void);

void
rsvg_ft_ctx_done (RsvgFTCtx *ctx);

RsvgFTFontHandle
rsvg_ft_intern (RsvgFTCtx *ctx, const char *font_file_name);

void
rsvg_ft_font_attach (RsvgFTCtx *ctx, RsvgFTFontHandle fh,
		     char *font_file_name);

#if 0
void
rsvg_ft_font_ref (RsvgFTFont *font);

void
rsvg_ft_font_unref (RsvgFTFont *font);
#endif

RsvgFTGlyph *
rsvg_ft_render_string (RsvgFTCtx *ctx, RsvgFTFontHandle fh,
		       const char *str, double sx, double sy,
		       const double affine[6], int xy[2]);

void
rsvg_ft_glyph_unref (RsvgFTGlyph *glyph);
