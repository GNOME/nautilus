
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-glyph.h>

static NautilusGlyph *
glyph_new (const char *text, int font_size, gboolean bold)
{
	NautilusScalableFont *font;
	NautilusScalableFont *bold_font;
	NautilusGlyph *glyph;
	
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);
	g_return_val_if_fail (font_size >= 5, NULL);
	g_return_val_if_fail (font_size <= 200, NULL);
	
	font = nautilus_scalable_font_get_default_font ();
	g_return_val_if_fail (font != NULL, NULL);
	
	if (bold) {
		bold_font = nautilus_scalable_font_make_bold (font);
		g_return_val_if_fail (bold_font != NULL, NULL);
		gtk_object_unref (GTK_OBJECT (font));
		font = bold_font;
	}

	glyph = nautilus_glyph_new (font, font_size, text, strlen (text));
	g_return_val_if_fail (glyph != NULL, NULL);

	gtk_object_unref (GTK_OBJECT (font));

	return glyph;
}

int 
main (int argc, char* argv[])
{
	GdkPixbuf *pixbuf;
	NautilusScalableFont *font;
	NautilusGlyph *glyph;
	int x;
	int y;
	const guint font_size = 60;
	const guint underlined_font_size = 100;
	const int opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;

	const guint pixbuf_width = 640;
	const guint pixbuf_height = 480;
	const gboolean has_alpha = FALSE;
	const guint32 background_color = NAUTILUS_RGB_COLOR_WHITE;
	const char text[] = "Somethin";

	test_init (&argc, &argv);

	font = nautilus_scalable_font_get_default_font ();
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, has_alpha, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	nautilus_debug_pixbuf_draw_rectangle (pixbuf,
					      TRUE,
					      -1, -1, -1, -1,
					      background_color,
					      NAUTILUS_OPACITY_FULLY_OPAQUE);
	
	glyph = glyph_new (text, font_size, FALSE);

	x = 50;
	y = 10;

	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       x,
				       y,
				       NULL,
				       NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
				       opacity);
	y += nautilus_glyph_get_height (glyph) + 10;
	nautilus_glyph_free (glyph);

	glyph = glyph_new (text, font_size, TRUE);
	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       x,
				       y,
				       NULL,
				       NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
				       opacity);
	y += nautilus_glyph_get_height (glyph) + 10;
	nautilus_glyph_free (glyph);


	glyph = glyph_new (text, underlined_font_size, FALSE);
	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       x,
				       y,
				       NULL,
				       NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
				       opacity);

	{
		NautilusScalableFont *font;
		int underline_height;
		int baseline;
		ArtIRect glyph_rect;
		ArtIRect underline_rect;

		font = nautilus_scalable_font_get_default_font ();
		underline_height = nautilus_scalable_font_get_underline_height (font, underlined_font_size);
		baseline = nautilus_scalable_font_get_baseline (font, underlined_font_size);
		gtk_object_unref (GTK_OBJECT (font));

		glyph_rect = nautilus_glyph_intersect (glyph, x, y, NULL);

		if (0) nautilus_debug_pixbuf_draw_rectangle (pixbuf,
							     FALSE,
							     glyph_rect.x0,
							     glyph_rect.y0,
							     glyph_rect.x1,
							     glyph_rect.y1,
							     0xFF0000,
							     NAUTILUS_OPACITY_FULLY_OPAQUE);
		
		underline_rect = glyph_rect;
		
		underline_rect.y1 -= ABS (baseline);
		underline_rect.y0 = underline_rect.y1 - underline_height;
		
		nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						      TRUE,
						      underline_rect.x0,
						      underline_rect.y0,
						      underline_rect.x1,
						      underline_rect.y1,
						      NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
						      NAUTILUS_OPACITY_FULLY_OPAQUE);
		
		g_print ("underline_height = %d\n", underline_height);
		g_print ("baseline = %d\n", baseline);
	}		
	nautilus_glyph_free (glyph);

	nautilus_debug_show_pixbuf_in_eog (pixbuf);
	
	gdk_pixbuf_unref (pixbuf);
	gtk_object_unref (GTK_OBJECT (font));

	test_quit (0);

	return 0;
}
