typedef struct _RsvgFTCtx RsvgFTCtx;
typedef struct _RsvgFTGlyph RsvgFTGlyph;

struct _RsvgFTGlyph {
	int refcnt;
	int width, height;
	int underline_position, underline_thickness;
	double xpen, ypen; /* relative location of pen after the glyph */
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
		     const char *font_file_name);

#if 0
void
rsvg_ft_font_ref (RsvgFTFont *font);

void
rsvg_ft_font_unref (RsvgFTFont *font);
#endif

RsvgFTGlyph *
rsvg_ft_render_string (RsvgFTCtx *ctx, RsvgFTFontHandle fh,
		       const char *str, 
		       unsigned int length,
		       double sx, double sy,
		       const double affine[6], int xy[2]);

void
rsvg_ft_measure_string (RsvgFTCtx *ctx,
			RsvgFTFontHandle fh,
			const char *str, 
			unsigned int length,
			double sx, double sy,
			const double affine[6], 
			int xy[2],
			unsigned int dimensions[2]);

void
rsvg_ft_glyph_unref (RsvgFTGlyph *glyph);
