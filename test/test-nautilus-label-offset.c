#include <config.h>

#include "test.h"

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *frame;
	GtkWidget *pixmap;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *text;
	GtkWidget *main_box;
	char *file_name;

	test_init (&argc, &argv);

	window = test_window_new ("Caveat", 10);
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_box);
	gtk_widget_show (main_box);
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);
	gtk_window_set_wmclass (GTK_WINDOW (window), "caveat", "Nautilus");

  	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
  	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
  	gtk_widget_show (hbox);
  	gtk_box_pack_start (GTK_BOX (main_box), 
  			    hbox,
  			    FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	file_name = nautilus_pixmap_file ("About_Image.png");
	if (file_name != NULL) {
		pixmap = nautilus_image_new (file_name);
		g_free (file_name);

		if (pixmap != NULL) {
			frame = gtk_frame_new (NULL);
			gtk_widget_show (frame);
			gtk_frame_set_shadow_type (GTK_FRAME (frame),
						   GTK_SHADOW_IN);
			gtk_box_pack_start (GTK_BOX (vbox), frame,
					    FALSE, FALSE, 0);

			gtk_widget_show (pixmap);
			gtk_container_add (GTK_CONTAINER (frame), pixmap);
		}
	}

  	text = nautilus_label_new
		(_("Thank you for your interest in Nautilus.\n "
		   "\n"
		   "As with any software under development, you should exercise caution when "
		   "using Nautilus.  Eazel does not provide any guarantee that it will work "
		   "properly, or assume any liability for your use of it.  Please use it at your "
		   "own risk.\n"
		   "\n"
		   "Please visit http://www.eazel.com/feedback.html to provide feedback, "
		   "comments, and suggestions."));
	nautilus_label_make_larger (NAUTILUS_LABEL (text), 1);
	nautilus_label_set_justify (NAUTILUS_LABEL (text), GTK_JUSTIFY_LEFT);
	nautilus_label_set_wrap (NAUTILUS_LABEL (text), TRUE);
	gtk_widget_show (text);
  	gtk_box_pack_start (GTK_BOX (hbox), text, FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (window));

	gtk_main ();

	return 0;
}
