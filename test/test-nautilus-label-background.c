#include "test.h"

#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-label-with-background.h>

static const char tile_name[] = "patterns/camouflage.png";
//static const char *tile_name = NULL;

static GtkWidget *
window_new_with_nautilus_background_image (void)
{
	GtkWidget *window;
	GtkWidget *label;
	
	window = test_window_new ("Label with a NautilusBackground label", 10);
	test_gtk_widget_set_background_image (window, "patterns/pale_coins.png");

	label = test_label_new ("Something", tile_name, TRUE, 30);
	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_widget_show (label);

	return window;
}

static GtkWidget *
window_new_with_nautilus_background_gradient (void)
{
	GtkWidget *window;
	GtkWidget *label;
	
	window = test_window_new ("Label with a NautilusBackground gradient", 10);
	test_gtk_widget_set_background_color (window, "rgb:0000/0000/ffff-rgb:ffff/ffff/ffff:h");

	label = test_label_new ("Something", tile_name, TRUE, 30);
	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_widget_show (label);

	return window;
}

static GtkWidget *
window_new_with_gtk_background (void)
{
	GtkWidget *window;
	GtkWidget *label;
	
	window = test_window_new ("Label with a regular GTK+ background", 10);

	label = test_label_new ("Something", tile_name, FALSE, 30);
	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_widget_show (label);

	return window;
}

static GtkWidget *
window_new_with_gtk_background_hacked (void)
{
	GtkWidget *window;
	GtkWidget *label;
	
	window = test_window_new ("Label with a hacked GTK+ background", 10);
	test_gtk_widget_set_background_color (window, "rgb:ffff/0000/0000-rgb:0000/0000/ffff");

	label = test_label_new ("Something", tile_name, FALSE, 30);
	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_widget_show (label);

	return window;
}

static GtkWidget *
window_new_with_solid_background (void)
{
	GtkWidget *window;
	GtkWidget *label;
	
	window = test_window_new ("Label with a solid background", 10);

	test_gtk_widget_set_background_color (window, "white");

	label = test_label_new ("Something", tile_name, FALSE, 30);
	nautilus_label_set_background_mode (NAUTILUS_LABEL (label), NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_label_set_solid_background_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);

	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_widget_show (label);

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
