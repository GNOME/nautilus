
#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-string-picker.h>
#include <libnautilus-extensions/nautilus-font-picker.h>
#include <libnautilus-extensions/nautilus-text-caption.h>

#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-string.h>

#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath_stroke.h>
#include <libart_lgpl/art_rgb_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rgb.h>

#include <libgnomevfs/gnome-vfs-init.h>

/* Danger! Many Gremlins live here. */

/* FIXME bugzilla.eazel.com 5031: Need to account for word endianess in these macros */
#define ART_OPACITY_NONE 255
#define ART_OPACITY_FULL 0

/* Pack RGBA values */
#define ART_RGBA_COLOR_PACK(_r, _g, _b, _a)	\
( ((_a) << 0) |					\
  ((_r) << 24) |				\
  ((_g) << 16) |				\
  ((_b) <<  8) )

#define ART_RGB_COLOR_PACK(_r, _g, _b)			\
( (ART_OPACITY_NONE << 0) |					\
  ((_r) << 24) |				\
  ((_g) << 16) |				\
  ((_b) <<  8) )

/* Access the individual RGBA components */
#define ART_RGBA_GET_R(_color) (((_color) >> 24) & 0xff)
#define ART_RGBA_GET_G(_color) (((_color) >> 16) & 0xff)
#define ART_RGBA_GET_B(_color) (((_color) >>  8) & 0xff)
#define ART_RGBA_GET_A(_color) (((_color) >>  0) & 0xff)

#define RED		ART_RGB_COLOR_PACK (255, 0, 0)
#define GREEN		ART_RGB_COLOR_PACK (0, 255, 0)
#define BLUE		ART_RGB_COLOR_PACK (0, 0, 255)
#define WHITE		ART_RGB_COLOR_PACK (255, 255, 255)
#define BLACK		ART_RGB_COLOR_PACK (0, 0, 0)
#define TRANSPARENT	ART_RGBA_COLOR_PACK (255, 255, 255, 0)

static GdkPixbuf *
create_named_background (const char *name) 
{
	GdkPixbuf	*pixbuf;
	char		*path;
	
	g_return_val_if_fail (name != NULL, NULL);
	
	path = nautilus_make_path (NAUTILUS_DATADIR "/patterns", name);

	if (path == NULL) {
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	return pixbuf;
}

static void
rgba_run_alpha (art_u8 *buf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n)
{
  int i;
  int v;

  for (i = 0; i < n; i++)
    {
      v = *buf;
      *buf++ = v + (((r - v) * alpha + 0x80) >> 8);
      v = *buf;
      *buf++ = v + (((g - v) * alpha + 0x80) >> 8);
      v = *buf;
      *buf++ = v + (((b - v) * alpha + 0x80) >> 8);

      *buf++ = 255;
    }
}

typedef void (*RunFunc) (art_u8 *buf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n);

static void
pixbuf_draw_rectangle (GdkPixbuf	*pixbuf,
		       const ArtIRect	*rectangle,
		       guint32		color,
		       gboolean		filled)
{
	guchar		red;
	guchar		green;
	guchar		blue;
	guchar		alpha;

	guint		width;
	guint		height;
	guchar		*pixels;
	guint		rowstride;
 	int		y;
	gboolean	has_alpha;
	guint		pixel_offset;
	guchar		*offset;

	guint		rect_width;
	guint		rect_height;

	ArtIRect	draw_area;

	RunFunc		run_func;

	g_return_if_fail (pixbuf != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	pixel_offset = has_alpha ? 4 : 3;

	red = ART_RGBA_GET_R (color);
	green = ART_RGBA_GET_G (color);
	blue = ART_RGBA_GET_B (color);
	alpha = ART_RGBA_GET_A (color);

	run_func = has_alpha ? rgba_run_alpha : art_rgb_run_alpha;

	if (rectangle != NULL) {
		g_return_if_fail (rectangle->x1 >  rectangle->x0);
		g_return_if_fail (rectangle->y1 >  rectangle->y0);
		
		rect_width = rectangle->x1 - rectangle->x0;
		rect_height = rectangle->y1 - rectangle->y0;

		draw_area = *rectangle;
	}
	else {
		rect_width = width;
		rect_height = height;

		draw_area.x0 = 0;
		draw_area.y0 = 0;
		draw_area.x1 = width;
		draw_area.y1 = height;
	}

	if (filled) {
		offset = pixels + (draw_area.y0 * rowstride) + (draw_area.x0 * pixel_offset);

		for (y = draw_area.y0; y < draw_area.y1; y++) {
			(*run_func) (offset, red, green, blue, 255, rect_width);
			offset += rowstride;
		}
	}
	else {
		/* top */
		offset = pixels + (draw_area.y0 * rowstride) + (draw_area.x0 * pixel_offset);
		(*run_func) (offset, red, green, blue, 255, rect_width);
		
		/* bottom */
		offset += ((rect_height - 1) * rowstride);
		(*run_func) (offset, red, green, blue, 255, rect_width);
	
		for (y = draw_area.y0 + 1; y < (draw_area.y1 - 1); y++) {
			/* left */
			offset = pixels + (y * rowstride) + (draw_area.x0 * pixel_offset);
			(*run_func) (offset, red, green, blue, 255, 1);
			
			/* right */
			offset += (rect_width - 1) * pixel_offset;
			(*run_func) (offset, red, green, blue, 255, 1);
		}
	}
}

static void
pixbuf_draw_rectangle_around (GdkPixbuf	*pixbuf,
			      const ArtIRect	*rectangle,
			      guint32		color)
{
	ArtIRect area;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (rectangle != NULL);
	g_return_if_fail (rectangle->x1 >  rectangle->x0);
	g_return_if_fail (rectangle->y1 >  rectangle->y0);

	area = *rectangle;
	
	area.x0 -= 1;
	area.y0 -= 1;
	area.x1 += 1;
	area.y1 += 1;
	pixbuf_draw_rectangle (pixbuf, &area, color, FALSE);
	area.x0 += 1;
	area.y0 += 1;
	area.x1 -= 1;
	area.y1 -= 1;
}

int 
main (int argc, char* argv[])
{
	GdkPixbuf		*pixbuf;
	NautilusScalableFont	*font;
	ArtIRect		clip_area;
	ArtIRect		whole_area;
	ArtIRect		multi_lines_area;

	const char   *text = "\nLine Two\n\nLine Four\n\n\nLine Seven";
	const guint  font_width = 48;
	const guint  font_height = 48;
	const guint  pixbuf_width = 500;
	const guint  pixbuf_height = 700;
	const guint  line_offset = 2;
	const guint  empty_line_height = font_height;
	const int    multi_line_x = 10;
	const int    multi_line_y = 10;

	g_print ("font_height = %d, empty_line_height = %d\n", font_height, empty_line_height);

	gtk_init (&argc, &argv);
	gdk_rgb_init ();
	gnome_vfs_init ();

	font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("Nimbus Sans L", NULL, NULL, NULL));
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	pixbuf_draw_rectangle (pixbuf, NULL, TRANSPARENT, TRUE);

	multi_lines_area.x0 = multi_line_x;
	multi_lines_area.y0 = multi_line_y;

	/* Measure some text lines */
	{
		guint	num_text_lines;
		guint	*text_line_widths;
		guint	*text_line_heights;
		guint	max_width_out;
		guint	total_height_out;

		num_text_lines = nautilus_str_count_characters (text, '\n') + 1;
		
		text_line_widths = g_new (guint, num_text_lines);
		text_line_heights = g_new (guint, num_text_lines);
		
		nautilus_scalable_font_measure_text_lines (font,
							   font_width,
							   font_height,
							   text,
							   num_text_lines,
							   empty_line_height,
							   text_line_widths,
							   text_line_heights,
							   &max_width_out,
							   &total_height_out);

		multi_lines_area.x1 = multi_lines_area.x0 + max_width_out;
		multi_lines_area.y1 = multi_lines_area.y0 + total_height_out + ((num_text_lines - 1) * line_offset);
		
		g_print ("num_text_lines = %d, max_width = %d, total_height = %d\n",
			 num_text_lines,
			 max_width_out,
			 total_height_out);

		
		g_free (text_line_widths);
		g_free (text_line_heights);
	}

	clip_area.x0 = 300;
	clip_area.y0 = 20;
	clip_area.x1 = clip_area.x0 + 100;
	clip_area.y1 = clip_area.y0 + 30;
	
	pixbuf_draw_rectangle_around (pixbuf, &clip_area, RED);

	whole_area.x0 = 0;
	whole_area.y0 = 0;
	whole_area.x1 = whole_area.x0 + pixbuf_width;
	whole_area.y1 = whole_area.y0 + pixbuf_height;

	pixbuf_draw_rectangle_around (pixbuf, &multi_lines_area, RED);

	/*
	 * Multiple text lines test.
	 */
	nautilus_scalable_font_draw_text_lines (font,
						pixbuf,
						multi_line_x,
						multi_line_y,
						&whole_area,
						font_width,
						font_height,
						text,
						GTK_JUSTIFY_LEFT,
						line_offset,
						empty_line_height,
						BLUE,
						255,
						FALSE);

	/*
	 * Clipped text test.  The "Something" string should be clipped such
	 * that horizontally you can only see "Som" and a tiny fraction of
	 * the "e".
	 *
	 * Vertically, you should see about 90% of the "Som"
	 */
	nautilus_scalable_font_draw_text (font,
					  pixbuf,
					  clip_area.x0,
					  clip_area.y0,
					  &clip_area,
					  font_width,
					  font_height,
					  "Something",
					  strlen ("Something"),
					  GREEN,
					  255,
					  FALSE);

	/*
	 * Inverted text test.
	 */
	nautilus_scalable_font_draw_text (font,
					  pixbuf,
					  50,
					  350,
					  NULL,
					  50,
					  50,
					  "This text is inverted",
					  strlen ("This text is inverted"),
					  GREEN,
					  255,
					  TRUE);


	/*
	 * Composited text lines test.
	 */
	{
		ArtIRect composited_area;
		GdkPixbuf *background_pixbuf;
		GdkPixbuf *text_pixbuf;
		GdkRectangle dest_rect;

		const char *text = "Foo Bar";
		const guint font_size = 50;

		background_pixbuf = create_named_background ("pale_coins.png");
		
		composited_area.x0 = 270;
		composited_area.y0 = 80;
		composited_area.x1 = composited_area.x0 + 200;
		composited_area.y1 = composited_area.y0 + 200;
		
		pixbuf_draw_rectangle_around (pixbuf, &composited_area, RED);
		
		dest_rect.x = composited_area.x0;
		dest_rect.y = composited_area.y0;
		dest_rect.width = composited_area.x1 - composited_area.x0;
		dest_rect.height = composited_area.y1 - composited_area.y0;

		nautilus_gdk_pixbuf_render_to_pixbuf_tiled (background_pixbuf,
							    pixbuf,
							    &dest_rect,
							    0,
							    0);

		gdk_pixbuf_unref (background_pixbuf);

		text_pixbuf = nautilus_gdk_pixbuf_new_from_text (font,
								 font_size,
								 font_size,
								 text,
								 strlen (text),
								 BLACK,
								 255,
								 FALSE);
		g_assert (text_pixbuf != NULL);

		gdk_pixbuf_composite (text_pixbuf,
				      pixbuf,
				      composited_area.x0,
				      composited_area.y0,
				      gdk_pixbuf_get_width (text_pixbuf),
				      gdk_pixbuf_get_height (text_pixbuf),
				      (double) composited_area.x0,
				      (double) composited_area.y0,
				      1.0,
				      1.0,
				      GDK_INTERP_BILINEAR,
				      255);

		gdk_pixbuf_unref (text_pixbuf);
	}

	/*
	 * Text layout test.
	 */
	{
		NautilusTextLayout *text_layout;
		const guint max_text_width = 100;
		const char *separators = " -_,;.?/&";
		const char *text = "This is a long piece of text!-This is the second piece-Now we have the third piece-And finally the fourth piece";
		const guint font_size = 14;
		ArtIRect layout_area;
		
		text_layout = nautilus_text_layout_new (font,
						      font_size,
						      text,
						      separators,
						      max_text_width, 
						      TRUE);
		g_assert (text_layout != NULL);
		
		layout_area.x0 = 20;
		layout_area.y0 = 550;
		layout_area.x1 = layout_area.x0 + max_text_width;
		layout_area.y1 = layout_area.y0 + 130;

		pixbuf_draw_rectangle_around (pixbuf, &layout_area, RED);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_LEFT,
					    BLACK,
					    FALSE,
					    FALSE);
		
		layout_area.x0 += (max_text_width + 20);
		layout_area.x1 += (max_text_width + 20);

		pixbuf_draw_rectangle_around (pixbuf, &layout_area, RED);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_CENTER,
					    BLACK,
					    FALSE,
					    FALSE);
		
		layout_area.x0 += (max_text_width + 20);
		layout_area.x1 += (max_text_width + 20);
		
		pixbuf_draw_rectangle_around (pixbuf, &layout_area, RED);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_RIGHT,
					    BLACK,
					    FALSE,
					    FALSE);
		
		nautilus_text_layout_free (text_layout);
	}

	/*
	 * Underlined text test.
	 */
	{
		NautilusTextLayout *text_layout;
		const guint max_text_width = pixbuf_width / 2;
		const char *separators = "-";
		const char *text = "This is multi line-text (g) that should-be centered and-(q) underlined";
		const guint font_size = 30;
		ArtIRect layout_area;
		
		text_layout = nautilus_text_layout_new (font,
							font_size,
							text,
							separators,
							max_text_width, 
							TRUE);
		g_assert (text_layout != NULL);
		
		layout_area.x0 = (pixbuf_width - text_layout->width) / 2;
		layout_area.y0 = 410;
		layout_area.x1 = layout_area.x0 + text_layout->width;
		layout_area.y1 = layout_area.y0 + text_layout->height;

		pixbuf_draw_rectangle_around (pixbuf, &layout_area, RED);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_CENTER,
					    BLACK,
					    FALSE,
					    TRUE);
		
		nautilus_text_layout_free (text_layout);
	}

	nautilus_gdk_pixbuf_save_to_file (pixbuf, "font_test.png");

	g_print ("saving test png file to font_test.png\n");
		
	gdk_pixbuf_unref (pixbuf);

	gnome_vfs_shutdown ();

	return 0;
}
