
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-scalable-font.h>

int 
main (int argc, char* argv[])
{
	GdkPixbuf		*pixbuf;
	NautilusScalableFont	*font;
	ArtIRect		clip_area;
	ArtIRect		whole_area;
	ArtIRect		multi_lines_area;

	const guint  font_size = 48;
	const guint  pixbuf_width = 500;
	const guint  pixbuf_height = 700;
	const guint  empty_line_height = font_size;
	const int    multi_line_x = 10;
	const int    multi_line_y = 10;

	g_print ("font_size = %d, empty_line_height = %d\n", font_size, empty_line_height);

	gtk_init (&argc, &argv);
	gdk_rgb_init ();
	gnome_vfs_init ();

	font = nautilus_scalable_font_get_default_font ();
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	nautilus_debug_pixbuf_draw_rectangle (pixbuf,
					      TRUE,
					      -1, -1, -1, -1,
					      NAUTILUS_RGB_COLOR_WHITE,
					      NAUTILUS_OPACITY_FULLY_OPAQUE);

	multi_lines_area.x0 = multi_line_x;
	multi_lines_area.y0 = multi_line_y;

	clip_area.x0 = 300;
	clip_area.y0 = 20;
	clip_area.x1 = clip_area.x0 + 100;
	clip_area.y1 = clip_area.y0 + 30;
	
	nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
						    FALSE,
						    clip_area.x0,
						    clip_area.y0,
						    clip_area.x1,
						    clip_area.y1,
						    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
						    NAUTILUS_OPACITY_FULLY_OPAQUE,
						    1);
	
	whole_area.x0 = 0;
	whole_area.y0 = 0;
	whole_area.x1 = whole_area.x0 + pixbuf_width;
	whole_area.y1 = whole_area.y0 + pixbuf_height;

	nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
						    FALSE,
						    multi_lines_area.x0,
						    multi_lines_area.y0,
						    multi_lines_area.x1,
						    multi_lines_area.y1,
						    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
						    NAUTILUS_OPACITY_FULLY_OPAQUE,
						    -1);

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
					  NULL,
					  80,
					  "Something",
					  strlen ("Something"),
					  NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
					  NAUTILUS_OPACITY_FULLY_OPAQUE);

	/*
	 * Composited text lines test.
	 */
	{
		ArtIRect composited_area;
		GdkPixbuf *tile_pixbuf;
		
		tile_pixbuf = test_pixbuf_new_named ("patterns/purple_marble.png", 1.0);
		
		composited_area.x0 = 270;
		composited_area.y0 = 80;
		composited_area.x1 = composited_area.x0 + 200;
		composited_area.y1 = composited_area.y0 + 200;
		
		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    composited_area.x0,
							    composited_area.y0,
							    composited_area.x1,
							    composited_area.y1,
							    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
							    NAUTILUS_OPACITY_FULLY_OPAQUE,
							    -1);
		
		nautilus_gdk_pixbuf_draw_to_pixbuf_tiled (tile_pixbuf,
							  pixbuf,
							  &composited_area,
							  gdk_pixbuf_get_width (tile_pixbuf),
							  gdk_pixbuf_get_height (tile_pixbuf),
							  0,
							  0,
							  NAUTILUS_OPACITY_FULLY_OPAQUE,
							  GDK_INTERP_BILINEAR);
		
		gdk_pixbuf_unref (tile_pixbuf);
	}

	nautilus_debug_show_pixbuf_in_eog (pixbuf);
	
	gdk_pixbuf_unref (pixbuf);

	gnome_vfs_shutdown ();

	return 0;
}
