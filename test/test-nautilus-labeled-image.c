#include "test.h"

#include <libnautilus-extensions/nautilus-labeled-image.h>


static const char pixbuf_name[] = "/usr/share/pixmaps/gnome-globe.png";
static const char tile_name[] = "/gnome/share/nautilus/patterns/camouflage.png";

static GtkWidget *
labeled_image_new (const char *text,
		   GdkPixbuf *pixbuf,
		   const char *tile_file_name)
{
	GtkWidget *labeled_image;

	labeled_image = nautilus_labeled_image_new (text, pixbuf);

	if (tile_file_name != NULL) {
		nautilus_labeled_image_set_tile_pixbuf_from_file_name (NAUTILUS_LABELED_IMAGE (labeled_image),
								       tile_file_name);
	}

	return labeled_image;
}

static GtkWidget *
labeled_image_window_new (const char *title,
			  GdkPixbuf *pixbuf,
			  const char *tile_file_name)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *labeled_image;
	GtkWidget *tiled_labeled_image;

	window = test_window_new (title, 20);
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	labeled_image = labeled_image_new ("Labeled Image", pixbuf, NULL);
	tiled_labeled_image = labeled_image_new ("Labeled Image", pixbuf, tile_file_name);
	if (tile_file_name != NULL) {
		nautilus_labeled_image_set_fill (NAUTILUS_LABELED_IMAGE (tiled_labeled_image), TRUE);
	}	
	if (labeled_image) gtk_box_pack_start (GTK_BOX (vbox), labeled_image, TRUE, TRUE, 0);
	if (tiled_labeled_image) gtk_box_pack_start (GTK_BOX (vbox), tiled_labeled_image, TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);
	
	return window;
}

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

	window = test_window_new (title, 20);
	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	if (1) button = nautilus_labeled_image_button_new ("GtkButton with LabeledImage", pixbuf);
	if (1) toggle_button = nautilus_labeled_image_toggle_button_new ("GtkToggleButton with LabeledImage", pixbuf);
	if (1) check_button = nautilus_labeled_image_check_button_new ("GtkCheckButton with LabeledImage", pixbuf);

	if (button) gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
	if (toggle_button) gtk_box_pack_start (GTK_BOX (vbox), toggle_button, TRUE, TRUE, 0);
	if (check_button) gtk_box_pack_start (GTK_BOX (vbox), check_button, TRUE, TRUE, 0);

	if (button) {
		gtk_signal_connect (GTK_OBJECT (button), "enter", GTK_SIGNAL_FUNC (button_callback), "enter");
		gtk_signal_connect (GTK_OBJECT (button), "leave", GTK_SIGNAL_FUNC (button_callback), "leave");
		gtk_signal_connect (GTK_OBJECT (button), "pressed", GTK_SIGNAL_FUNC (button_callback), "pressed");
		gtk_signal_connect (GTK_OBJECT (button), "released", GTK_SIGNAL_FUNC (button_callback), "released");
		gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (button_callback), "clicked");
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
	if (1) labeled_image_window = labeled_image_window_new ("LabeledImage Test", pixbuf, tile_name);
	if (1) labeled_image_button_window = labeled_image_button_window_new ("LabeledImage in GtkButton Test", pixbuf);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);

	if (labeled_image_window) gtk_widget_show (labeled_image_window);
	if (labeled_image_button_window) gtk_widget_show (labeled_image_button_window);
	
	gtk_main ();

	return 0;
}
