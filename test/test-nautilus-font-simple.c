
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-scalable-font.h>

int 
main (int argc, char* argv[])
{
	GdkPixbuf *pixbuf;
	NautilusScalableFont *font;

	test_init (&argc, &argv);
	
	font = nautilus_scalable_font_get_default_font ();
	g_assert (font != NULL);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 800, 400);
	g_assert (pixbuf != NULL);
	
	nautilus_debug_pixbuf_draw_rectangle (pixbuf,
					      TRUE,
					      -1, -1, -1, -1,
					      NAUTILUS_RGB_COLOR_WHITE,
					      NAUTILUS_OPACITY_FULLY_OPAQUE);

	nautilus_scalable_font_draw_text (font,
					  pixbuf,
					  10,
					  100,
					  NULL,
					  80,
					  "Something",
					  strlen ("Something"),
					  NAUTILUS_RGBA_COLOR_OPAQUE_BLUE,
					  NAUTILUS_OPACITY_FULLY_OPAQUE);

	nautilus_debug_show_pixbuf_in_eog (pixbuf);
	
	gdk_pixbuf_unref (pixbuf);
	
	gnome_vfs_shutdown ();

	return 0;
}
