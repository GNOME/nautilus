#include "test.h"

#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-image-with-background.h>

static const char pixbuf_name[] = "/usr/share/pixmaps/gnome-globe.png";
//static const char pixbuf_name[] = "/gnome/share/pixmaps/nautilus/arlo/i-directory-aa.png";
//static const char tile_name[] = "/usr/share/pixmaps/gnome-monitor.png";
static const char tile_name[] = "patterns/camouflage.png";
static const float pixbuf_scale = 4.0;

static GtkWidget *
window_new_with_nautilus_background_image (void)
{
	GtkWidget *window;
	GtkWidget *image;
	
	window = test_window_new ("Image with a NautilusBackground image", 10);
	test_gtk_widget_set_background_image (window, "patterns/pale_coins.png");

	image = test_image_new (pixbuf_name, tile_name, pixbuf_scale, TRUE);
	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show (image);

	return window;
}

static GtkWidget *
window_new_with_nautilus_background_gradient (void)
{
	GtkWidget *window;
	GtkWidget *image;
	
	window = test_window_new ("Image with a NautilusBackground gradient", 10);
	test_gtk_widget_set_background_color (window, "rgb:0000/0000/ffff-rgb:ffff/ffff/ffff:h");

	image = test_image_new (pixbuf_name, tile_name, pixbuf_scale, TRUE);
	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show (image);

	return window;
}

static GtkWidget *
window_new_with_gtk_background (void)
{
	GtkWidget *window;
	GtkWidget *image;
	
	window = test_window_new ("Image with a regular GTK+ background", 10);

	image = test_image_new (pixbuf_name, tile_name, pixbuf_scale, FALSE);
	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show (image);

	return window;
}

static GtkWidget *
window_new_with_gtk_background_hacked (void)
{
	GtkWidget *window;
	GtkWidget *image;
	
	window = test_window_new ("Image with a hacked GTK+ background", 10);
	test_gtk_widget_set_background_color (window, "rgb:ffff/0000/0000-rgb:0000/0000/ffff");

	image = test_image_new (pixbuf_name, tile_name, pixbuf_scale, FALSE);
	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show (image);

	return window;
}

static GtkWidget *
window_new_with_solid_background (void)
{
	GtkWidget *window;
	GtkWidget *image;
	
	window = test_window_new ("Image with a solid background", 10);

	test_gtk_widget_set_background_color (window, "white");

	image = test_image_new (pixbuf_name, tile_name, pixbuf_scale, FALSE);
	nautilus_image_set_background_mode (NAUTILUS_IMAGE (image), NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_image_set_solid_background_color (NAUTILUS_IMAGE (image), NAUTILUS_RGB_COLOR_WHITE);

	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show (image);

	return window;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window[4];
	
	test_init (&argc, &argv);
	
	if (1) window[0] = window_new_with_nautilus_background_image ();
	if (1) window[1] = window_new_with_nautilus_background_gradient ();
	if (1) window[2] = window_new_with_gtk_background ();
	if (1) window[3] = window_new_with_gtk_background_hacked ();
	if (1) window[4] = window_new_with_solid_background ();

	if (1) gtk_widget_show (window[0]);
	if (1) gtk_widget_show (window[1]);
	if (1) gtk_widget_show (window[2]);
	if (1) gtk_widget_show (window[3]);
	if (1) gtk_widget_show (window[4]);

	gtk_main ();

	return 0;
}
