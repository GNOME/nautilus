
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

int 
main (int argc, char* argv[])
{
	GdkPixbuf		*pixbuf;
	NautilusScalableFont	*font;

	const char		*text = "\nLine Two\n\nLine Four\n\n\nLine Seven\n";
	const guint		font_width = 48;
	const guint		font_height = 48;

	const guint pixbuf_width = 500;
	const guint pixbuf_height = 700;

	GdkRectangle blue_area;
	ArtIRect     clip_area;
	ArtIRect     whole_area;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("Nimbus Sans L", NULL, NULL, NULL));
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	nautilus_gdk_pixbuf_fill_rectangle_with_color (pixbuf, NULL, NAUTILUS_RGBA_COLOR_PACK (255, 255, 255, 0));

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
							   text_line_widths,
							   text_line_heights,
							   &max_width_out,
							   &total_height_out);

		g_print ("num_text_lines = %d, max_width = %d, total_height = %d\n",
			 num_text_lines,
			 max_width_out,
			 total_height_out);
		
		g_free (text_line_widths);
		g_free (text_line_heights);
	}

	blue_area.x = 300;
	blue_area.y = 20;
	blue_area.width = 100;
	blue_area.height = 30;

	nautilus_gdk_pixbuf_fill_rectangle_with_color (pixbuf, &blue_area, NAUTILUS_RGBA_COLOR_PACK (0, 0, 255, 255));
	
	clip_area.x0 = blue_area.x;
	clip_area.y0 = blue_area.y;
	clip_area.x1 = blue_area.x + blue_area.width;
	clip_area.y1 = blue_area.y + blue_area.height;

	whole_area.x0 = 0;
	whole_area.y0 = 0;
	whole_area.x1 = whole_area.x0 + pixbuf_width;
	whole_area.y1 = whole_area.y0 + pixbuf_height;

	/* Draw some green text clipped by the whole pixbuf area */
	nautilus_scalable_font_draw_text_lines (font,
						pixbuf,
						0,
						0,
						&whole_area,
						font_width,
						font_height,
						text,
						GTK_JUSTIFY_LEFT,
						2,
						NAUTILUS_RGBA_COLOR_PACK (0, 255, 0, 255),
						255);

	/* Draw some red text clipped by the blue area */
	nautilus_scalable_font_draw_text (font,
					  pixbuf,
					  clip_area.x0,
					  clip_area.y0,
					  &clip_area,
					  font_width,
					  font_height,
					  "Something",
					  strlen ("Something"),
					  NAUTILUS_RGBA_COLOR_PACK (255, 0, 0, 255),
					  255);

	nautilus_gdk_pixbuf_save_to_file (pixbuf, "font_test.png");

	g_print ("saving test png file to font_test.png\n");
		
	gdk_pixbuf_unref (pixbuf);

	return 0;
}
