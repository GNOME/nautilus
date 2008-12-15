#include "test.h"

#include <eel/eel-labeled-image.h>


static const char pixbuf_name[] = "/usr/share/pixmaps/gnome-globe.png";

static void
button_callback (GtkWidget *button,
		 gpointer callback_data)
{
	const char *info = callback_data;
	g_return_if_fail (GTK_IS_BUTTON (button));
	
	g_print ("%s(%p)\n", info, button);
}

static GtkWidget *
labeled_image_button_window_new (const char *title,
				 GdkPixbuf *pixbuf)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *toggle_button;
	GtkWidget *check_button;
	GtkWidget *plain;

	window = test_window_new (title, 20);
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	if (1) button = eel_labeled_image_button_new ("GtkButton with LabeledImage", pixbuf);
	if (1) toggle_button = eel_labeled_image_toggle_button_new ("GtkToggleButton with LabeledImage", pixbuf);
	if (1) check_button = eel_labeled_image_check_button_new ("GtkCheckButton with LabeledImage", pixbuf);
	if (1) {
		plain = eel_labeled_image_new ("Plain LabeledImage", pixbuf);
		eel_labeled_image_set_can_focus (EEL_LABELED_IMAGE (plain), TRUE);
	}

	if (button) gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
	if (toggle_button) gtk_box_pack_start (GTK_BOX (vbox), toggle_button, TRUE, TRUE, 0);
	if (check_button) gtk_box_pack_start (GTK_BOX (vbox), check_button, TRUE, TRUE, 0);
	if (plain) gtk_box_pack_start (GTK_BOX (vbox), plain, TRUE, TRUE, 0);

	if (button) {
		g_signal_connect (button, "enter", G_CALLBACK (button_callback), "enter");
		g_signal_connect (button, "leave", G_CALLBACK (button_callback), "leave");
		g_signal_connect (button, "pressed", G_CALLBACK (button_callback), "pressed");
		g_signal_connect (button, "released", G_CALLBACK (button_callback), "released");
		g_signal_connect (button, "clicked", G_CALLBACK (button_callback), "clicked");
	}

	gtk_widget_show_all (vbox);
	
	return window;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *labeled_image_window = NULL;
	GtkWidget *labeled_image_button_window = NULL;
	GdkPixbuf *pixbuf = NULL;

	test_init (&argc, &argv);

	if (1) pixbuf = test_pixbuf_new_named (pixbuf_name, 1.0);
	if (1) labeled_image_button_window = labeled_image_button_window_new ("LabeledImage in GtkButton Test", pixbuf);

	eel_gdk_pixbuf_unref_if_not_null (pixbuf);

	if (labeled_image_window) gtk_widget_show (labeled_image_window);
	if (labeled_image_button_window) gtk_widget_show (labeled_image_button_window);
	
	gtk_main ();

	return test_quit (EXIT_SUCCESS);
}
