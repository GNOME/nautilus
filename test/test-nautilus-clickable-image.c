#include "test.h"

#include <libnautilus-extensions/nautilus-clickable-image.h>

static void
clicked_callback (GtkWidget *widget,
		  gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (widget));

	g_print ("%s(%p)\n", __FUNCTION__, widget);
}

static void
enter_callback (GtkWidget *widget,
		gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (widget));

	g_print ("%s(%p)\n", __FUNCTION__, widget);
}


static void
leave_callback (GtkWidget *widget,
		gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (widget));

	g_print ("%s(%p)\n", __FUNCTION__, widget);
}

static GtkWidget *
clickable_image_new (const char *text, GdkPixbuf *pixbuf)
{
	GtkWidget *clickable_image;

	clickable_image = nautilus_clickable_image_new (text, pixbuf);

	gtk_signal_connect (GTK_OBJECT (clickable_image), "clicked", GTK_SIGNAL_FUNC (clicked_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (clickable_image), "enter", GTK_SIGNAL_FUNC (enter_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (clickable_image), "leave", GTK_SIGNAL_FUNC (leave_callback), NULL);

	return clickable_image;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *clickable_images[3];
	GtkWidget *event_box;
	GdkPixbuf *pixbuf;

	test_init (&argc, &argv);
	
	window = test_window_new ("Simple Label Test", 20);
	event_box = gtk_event_box_new ();
	vbox = gtk_vbox_new (TRUE, 10);
	gtk_container_add (GTK_CONTAINER (window), event_box);
	gtk_container_add (GTK_CONTAINER (event_box), vbox);

	clickable_images[0] = NULL;
	clickable_images[1] = NULL;
	clickable_images[2] = NULL;

	pixbuf = test_pixbuf_new_named ("/usr/share/pixmaps/gnome-globe.png", 1.0);
	if (1) clickable_images[0] = clickable_image_new ("Clickable Image", pixbuf);
	if (1) clickable_images[1] = clickable_image_new ("Clickable Image No pixbuf", NULL);
	if (1) clickable_images[2] = clickable_image_new (NULL, pixbuf);
	gdk_pixbuf_unref (pixbuf);

	if (clickable_images[0]) gtk_box_pack_start (GTK_BOX (vbox), clickable_images[0], FALSE, FALSE, 0);
	if (clickable_images[1]) gtk_box_pack_start (GTK_BOX (vbox), clickable_images[1], FALSE, FALSE, 0);
	if (clickable_images[2]) gtk_box_pack_start (GTK_BOX (vbox), clickable_images[2], FALSE, FALSE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
