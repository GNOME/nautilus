
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

int 
main (int argc, char* argv[])
{
	GdkPixbuf		*pixbuf;
	guint			text_width;
	guint			text_height;
	ArtIRect		area;
	NautilusScalableFont	*font;

	const char		*text = "Something";
	const guint		font_width = 64;
	const guint		font_height = 64;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("Nimbus Sans L", NULL, NULL, NULL));
	g_assert (font != NULL);
		
	nautilus_scalable_font_measure_text (font,
					     font_width,
					     font_height,
					     text,
					     &text_width,
					     &text_height);

	g_print ("size of '%s' = (%d,%d)\n", text, text_width, text_height);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 512, 256);
	g_assert (pixbuf != NULL);
	
	area.x0 = 0;
	area.y0 = 0;
	
	area.x1 = 512;
	area.y1 = 256;

	nautilus_scalable_font_draw_text (font,
					  pixbuf,
					  &area,
					  font_width,
					  font_height,
					  text,
					  NAUTILUS_RGB_COLOR_RED,
					  255);

	g_assert (pixbuf != NULL);

	nautilus_gdk_pixbuf_save_to_file (pixbuf, "font_test.png");

	g_print ("saving test png file to font_test.png\n");
		
	gdk_pixbuf_unref (pixbuf);

	return 0;
}
