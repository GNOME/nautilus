
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-glyph.h>

static NautilusGlyph *
glyph_new (const char *text, int font_size)
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
	g_return_val_if_fail (glyph != NULL, NULL);

	gtk_object_unref (GTK_OBJECT (font));

	return glyph;
}

int 
main (int argc, char* argv[])
{
	GdkPixbuf *pixbuf;
	NautilusScalableFont *font;
	ArtIRect clip_area;
	ArtIRect dest_area;
	NautilusGlyph *glyph;
	const guint font_size = 60;
	const int opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;

	const guint pixbuf_width = 640;
	const guint pixbuf_height = 480;
	const gboolean has_alpha = FALSE;
	const guint32 background_color = NAUTILUS_RGB_COLOR_WHITE;
	const char text[] = "Something";

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
	
	nautilus_art_irect_assign (&clip_area,
				   60,
				   20,
				   200,
				   100);
	
	if (0) nautilus_debug_pixbuf_draw_rectangle (pixbuf,
						     FALSE,
						     clip_area.x0,
						     clip_area.y0,
						     clip_area.x1,
						     clip_area.y1,
						     NAUTILUS_RGBA_COLOR_OPAQUE_GREEN,
						     NAUTILUS_OPACITY_FULLY_OPAQUE);
	
	glyph = glyph_new (text, font_size);

	nautilus_art_irect_assign (&dest_area,
				   50,
				   200,
				   1000,
				   1000);

	nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       100,
				       0,
				       &dest_area,
				       NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
				       opacity);
	
	if (0) nautilus_glyph_draw_to_pixbuf (glyph,
				       pixbuf,
				       30,
				       40,
				       NULL, //&clip_area,
				       NAUTILUS_RGBA_COLOR_OPAQUE_GREEN,
				       opacity);
	
	nautilus_glyph_free (glyph);
		
	nautilus_debug_show_pixbuf_in_eog (pixbuf);
	
	gdk_pixbuf_unref (pixbuf);
	gtk_object_unref (GTK_OBJECT (font));

	test_quit (0);

	return 0;
}
