
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
	guint			num_text_lines;
	guint			*text_line_widths;
	guint			*text_line_heights;
	guint			max_width_out;
	guint			total_height_out;

	const char		*text = "\nLine Two\n\nLine Four\n\n\nLine Seven\n";
	const guint		font_width = 48;
	const guint		font_height = 48;

	const guint pixbuf_width = 500;
	const guint pixbuf_height = 700;

	GdkRectangle blue_area;
	ArtIRect     clip_area;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("Nimbus Sans L", NULL, NULL, NULL));
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	blue_area.x = 20;
	blue_area.y = 20;
	blue_area.width = 100;
	blue_area.height = 400;

	nautilus_gdk_pixbuf_fill_rectangle_with_color (pixbuf, &blue_area, NAUTILUS_RGBA_COLOR_PACK (0, 0, 255, 255));
	
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

	g_print ("max_width = %d, total_height = %d\n", max_width_out, total_height_out);

	clip_area.x0 = blue_area.x;
	clip_area.y0 = blue_area.y;
	clip_area.x1 = blue_area.x + blue_area.width;
	clip_area.y1 = blue_area.y + blue_area.height;

#if 0	
	nautilus_scalable_font_draw_text_lines (font,
						pixbuf,
						0,
						0,
						&clip_area,
						font_width,
						font_height,
						text,
						num_text_lines,
						text_line_widths,
						text_line_heights,
						GTK_JUSTIFY_LEFT,
						2,
						NAUTILUS_RGB_COLOR_RED,
						255);
#else
	nautilus_scalable_font_draw_text (font,
					  pixbuf,
					  10,
					  30,
					  &clip_area,
					  font_width,
					  font_height,
					  "Something",
					  strlen ("Something"),
					  NAUTILUS_RGB_COLOR_RED,
					  255);
#endif

	nautilus_gdk_pixbuf_save_to_file (pixbuf, "font_test.png");

	g_print ("saving test png file to font_test.png\n");
		
	gdk_pixbuf_unref (pixbuf);

	return 0;
}
