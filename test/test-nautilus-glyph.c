
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-glyph.h>

static NautilusGlyph *
glyph_new (const char *text,
	   int font_size)
{
	NautilusScalableFont *font;
	NautilusGlyph *glyph;
	
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);
	g_return_val_if_fail (font_size >= 5, NULL);
	g_return_val_if_fail (font_size <= 200, NULL);
	
	font = nautilus_scalable_font_get_default_font ();
	g_return_val_if_fail (font != NULL, NULL);
	
	glyph = nautilus_glyph_new (font, font_size, text, strlen (text));
	gtk_object_unref (GTK_OBJECT (font));

	g_return_val_if_fail (glyph != NULL, NULL);

	return glyph;
}

int 
main (int argc, char* argv[])
{
	const guint pixbuf_width = 640;
	const guint pixbuf_height = 480;
	const gboolean has_alpha = FALSE;
	const guint32 background_color = NAUTILUS_RGB_COLOR_WHITE;
	const char text[] = "ù È É Ê Ë Ì Í Î Ï Ð Ñ Ò Ó Ô Õ Ö Ø Ù Ú Û Ü Ý Þ";
	const gboolean solid_background = FALSE;
	const guint font_size = 100;
	const int opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;

	GdkPixbuf *pixbuf;
	NautilusScalableFont *font;
	int x;
	int y;
	ArtIRect clip_area;
	NautilusGlyph *glyph;

	test_init (&argc, &argv);

	font = nautilus_scalable_font_get_default_font ();
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, has_alpha, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	if (solid_background) {
		nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						      TRUE,
						      -1, -1, -1, -1,
						      background_color,
						      NAUTILUS_OPACITY_FULLY_OPAQUE);
	} else {
		test_pixbuf_draw_rectangle_tiled (pixbuf, 
						  "patterns/brushed_metal.png",
						  -1, -1, -1, -1,
						  NAUTILUS_OPACITY_FULLY_OPAQUE);
	}
	
	x = 10;
	y = 50;
	
	nautilus_art_irect_assign (&clip_area,
				   50,
				   30,
				   200,
				   80);
	
	glyph = glyph_new (text, font_size);
	
	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       x,
				       y,
				       NULL,
				       NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
				       opacity);
	
	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       -200,
				       y + 3 * font_size,
				       NULL,
				       NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
				       opacity);
	
	if (solid_background) {
		nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						      TRUE,
						      clip_area.x0,
						      clip_area.y0,
						      clip_area.x1,
						      clip_area.y1,
						      background_color,
						      NAUTILUS_OPACITY_FULLY_OPAQUE);
	} else {
		test_pixbuf_draw_rectangle_tiled (pixbuf, 
						  "patterns/brushed_metal.png",
						  clip_area.x0,
						  clip_area.y0,
						  clip_area.x1,
						  clip_area.y1,
						  NAUTILUS_OPACITY_FULLY_OPAQUE);
	}
	
	if (1) nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						     FALSE,
						     clip_area.x0 - 1,
						     clip_area.y0 - 1,
						     clip_area.x1 + 1,
						     clip_area.y1 + 1,
						     NAUTILUS_RGBA_COLOR_OPAQUE_GREEN,
						     NAUTILUS_OPACITY_FULLY_OPAQUE);
	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       x,
				       y,
				       &clip_area,
				       NAUTILUS_RGBA_COLOR_OPAQUE_RED,
				       opacity);
	
	nautilus_glyph_free (glyph);
	
	nautilus_art_irect_assign (&clip_area,
				   50,
				   100 + font_size + 4,
				   200,
				   80);
	
	if (1) nautilus_scalable_font_draw_text (font,
						 pixbuf,
						 x,
						 y + font_size + 4,
						 NULL,
						 font_size,
						 text,
						 strlen (text),
						 NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
						 opacity);
	
	if (solid_background) {
		nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						      TRUE,
						      clip_area.x0,
						      clip_area.y0,
						      clip_area.x1,
						      clip_area.y1,
						      background_color,
						      NAUTILUS_OPACITY_FULLY_OPAQUE);
	} else {
		test_pixbuf_draw_rectangle_tiled (pixbuf, 
						  "patterns/brushed_metal.png",
						  clip_area.x0,
						  clip_area.y0,
						  clip_area.x1,
						  clip_area.y1,
						  NAUTILUS_OPACITY_FULLY_OPAQUE);
	}
	
	if (1) nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						     FALSE,
						     clip_area.x0 - 1,
						     clip_area.y0 - 1,
						     clip_area.x1 + 1,
						     clip_area.y1 + 1,
						     NAUTILUS_RGBA_COLOR_OPAQUE_GREEN,
						     NAUTILUS_OPACITY_FULLY_OPAQUE);
	if (1) nautilus_scalable_font_draw_text (font,
						 pixbuf,
						 x,
						 y + font_size + 4,
						 &clip_area,
						 font_size,
						 text,
						 strlen (text),
						 NAUTILUS_RGBA_COLOR_OPAQUE_RED,
						 opacity);

	{
		const int glyph_x = 400;
		const int glyph_y = 300;
		ArtIRect glyph_rect;
		glyph = glyph_new ("x", 50);
		
		glyph_rect = nautilus_glyph_intersect (glyph, glyph_x, glyph_y, NULL);

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    glyph_rect.x0,
							    glyph_rect.y0,
							    glyph_rect.x1,
							    glyph_rect.y1,
							    0xeebbaa,
							    0xff,
							    -1);
		nautilus_glyph_draw_to_pixbuf (glyph,
					       pixbuf,
					       glyph_x,
					       glyph_y,
					       NULL,
					       0x0,
					       0xff);

		nautilus_glyph_free (glyph);
	}

	{
		const int glyph_x = 400;
		const int glyph_y = 350;
		ArtIRect glyph_rect;
		glyph = glyph_new ("x   y", 50);
		
		glyph_rect = nautilus_glyph_intersect (glyph, glyph_x, glyph_y, NULL);

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    glyph_rect.x0,
							    glyph_rect.y0,
							    glyph_rect.x1,
							    glyph_rect.y1,
							    0xeebbaa,
							    0xff,
							    -1);
		nautilus_glyph_draw_to_pixbuf (glyph,
					       pixbuf,
					       glyph_x,
					       glyph_y,
					       NULL,
					       0x0,
					       0xff);

		nautilus_glyph_free (glyph);
	}

	{
		const int glyph_x = 400;
		const int glyph_y = 400;
		ArtIRect glyph_rect;
		glyph = glyph_new (" ", 50);
		
		glyph_rect = nautilus_glyph_intersect (glyph, glyph_x, glyph_y, NULL);

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    glyph_rect.x0,
							    glyph_rect.y0,
							    glyph_rect.x1,
							    glyph_rect.y1,
							    0xeebbaa,
							    0xff,
							    -1);
		nautilus_glyph_draw_to_pixbuf (glyph,
					       pixbuf,
					       glyph_x,
					       glyph_y,
					       NULL,
					       0x0,
					       0xff);

		nautilus_glyph_free (glyph);
	}

	{
		const int glyph_x = 400;
		const int glyph_y = 420;
		ArtIRect glyph_rect;
		glyph = glyph_new ("  ", 50);
		
		glyph_rect = nautilus_glyph_intersect (glyph, glyph_x, glyph_y, NULL);

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    glyph_rect.x0,
							    glyph_rect.y0,
							    glyph_rect.x1,
							    glyph_rect.y1,
							    0xeebbaa,
							    0xff,
							    -1);
		nautilus_glyph_draw_to_pixbuf (glyph,
					       pixbuf,
					       glyph_x,
					       glyph_y,
					       NULL,
					       0x0,
					       0xff);

		nautilus_glyph_free (glyph);
	}


	/* This should not work.  A "" glyph is invalid */
	if (0) {
		const int glyph_x = 400;
		const int glyph_y = 450;
		ArtIRect glyph_rect;
		glyph = glyph_new ("", 50);
		
		glyph_rect = nautilus_glyph_intersect (glyph, glyph_x, glyph_y, NULL);

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    glyph_rect.x0,
							    glyph_rect.y0,
							    glyph_rect.x1,
							    glyph_rect.y1,
							    0xeebbaa,
							    0xff,
							    -1);
		nautilus_glyph_draw_to_pixbuf (glyph,
					       pixbuf,
					       glyph_x,
					       glyph_y,
					       NULL,
					       0x0,
					       0xff);

		nautilus_glyph_free (glyph);
	}
	
	nautilus_debug_show_pixbuf_in_eog (pixbuf);
	
	gdk_pixbuf_unref (pixbuf);

	gtk_object_unref (GTK_OBJECT (font));

	test_quit (0);

	return 0;
}
