
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-text-layout.h>

int 
main (int argc, char* argv[])
{
	GdkPixbuf		*pixbuf;
	NautilusScalableFont	*font;
	ArtIRect		clip_area;
	ArtIRect		whole_area;
	ArtIRect		multi_lines_area;

	const char   *text = "\nLine Two\n\nLine Four\n\n\nLine Seven";
	const guint  font_size = 48;
	const guint  pixbuf_width = 500;
	const guint  pixbuf_height = 700;
	const guint  line_offset = 2;
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

	/* Measure some text lines */
	{
		guint num_text_lines;
		NautilusDimensions *text_line_dimensions;
		guint max_width_out;
		guint total_height_out;
		
		num_text_lines = nautilus_str_count_characters (text, '\n') + 1;
		
		text_line_dimensions = g_new (NautilusDimensions, num_text_lines);
		
		nautilus_scalable_font_measure_text_lines (font,
							   font_size,
							   text,
							   num_text_lines,
							   empty_line_height,
							   text_line_dimensions,
							   &max_width_out,
							   &total_height_out);

		multi_lines_area.x1 = multi_lines_area.x0 + max_width_out;
		multi_lines_area.y1 = multi_lines_area.y0 + total_height_out + ((num_text_lines - 1) * line_offset);
		
		g_print ("num_text_lines = %d, max_width = %d, total_height = %d\n",
			 num_text_lines,
			 max_width_out,
			 total_height_out);

		g_free (text_line_dimensions);
	}

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
	 * Multiple text lines test.
	 */
	nautilus_scalable_font_draw_text_lines (font,
						pixbuf,
						multi_line_x,
						multi_line_y,
						&whole_area,
						font_size,
						text,
						GTK_JUSTIFY_LEFT,
						line_offset,
						empty_line_height,
						NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
						NAUTILUS_OPACITY_FULLY_OPAQUE);

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

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    layout_area.x0,
							    layout_area.y0,
							    layout_area.x1,
							    layout_area.y1,
							    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
							    NAUTILUS_OPACITY_FULLY_OPAQUE,
							    -1);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_LEFT,
					    NAUTILUS_RGBA_COLOR_OPAQUE_BLACK,
					    FALSE);
		
		layout_area.x0 += (max_text_width + 20);
		layout_area.x1 += (max_text_width + 20);

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    layout_area.x0,
							    layout_area.y0,
							    layout_area.x1,
							    layout_area.y1,
							    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
							    NAUTILUS_OPACITY_FULLY_OPAQUE,
							    -1);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_CENTER,
					    NAUTILUS_RGBA_COLOR_OPAQUE_BLACK,
					    FALSE);
		
		layout_area.x0 += (max_text_width + 20);
		layout_area.x1 += (max_text_width + 20);
		
		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    layout_area.x0,
							    layout_area.y0,
							    layout_area.x1,
							    layout_area.y1,
							    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
							    NAUTILUS_OPACITY_FULLY_OPAQUE,
							    -1);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_RIGHT,
					    NAUTILUS_RGBA_COLOR_OPAQUE_BLACK,
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

		nautilus_debug_pixbuf_draw_rectangle_inset (pixbuf,
							    FALSE,
							    layout_area.x0,
							    layout_area.y0,
							    layout_area.x1,
							    layout_area.y1,
							    NAUTILUS_RGBA_COLOR_OPAQUE_RED,
							    NAUTILUS_OPACITY_FULLY_OPAQUE,
							    -1);
		
		nautilus_text_layout_paint (text_layout,
					    pixbuf,
					    layout_area.x0, 
					    layout_area.y0, 
					    GTK_JUSTIFY_CENTER,
					    NAUTILUS_RGBA_COLOR_OPAQUE_BLACK,
					    TRUE);
		
		nautilus_text_layout_free (text_layout);
	}

	nautilus_debug_show_pixbuf_in_eog (pixbuf);
	
	gdk_pixbuf_unref (pixbuf);

	gnome_vfs_shutdown ();

	return 0;
}
