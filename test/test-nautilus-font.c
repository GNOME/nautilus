
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
	ArtIRect		area;
	NautilusScalableFont	*font;
	guint			num_text_lines;
	char			**text_lines;
	guint			*text_line_widths;
	guint			*text_line_heights;
	guint			max_width_out;
	guint			total_height_out;

	const char		*text = "\nLine Two\n\nLine Four\n\n\nLine Seven\n";
	const guint		font_width = 48;
	const guint		font_height = 48;

	const guint pixbuf_width = 500;
	const guint pixbuf_height = 700;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("Nimbus Sans L", NULL, NULL, NULL));
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);
	
	area.x0 = 0;
	area.y0 = 0;
	
	area.x1 = pixbuf_width;
	area.y1 = pixbuf_height;

	num_text_lines = nautilus_str_count_characters (text, '\n') + 1;

	text_lines = g_strsplit (text, "\n", -1);

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

	nautilus_scalable_font_draw_text_lines (font,
						pixbuf,
						&area,
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
	
	nautilus_gdk_pixbuf_save_to_file (pixbuf, "font_test.png");

	g_print ("saving test png file to font_test.png\n");
		
	gdk_pixbuf_unref (pixbuf);

	return 0;
}
