#include "test.h"

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *label;

	test_init (&argc, &argv);

	window = test_window_new ("Simple Label Test", 20);

	if (0) {
		GtkWidget *frame;
		GtkWidget *main_vbox;

		main_vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (window), main_vbox);
		frame = gtk_frame_new ("Foo");
		label = nautilus_label_new ("This is a very intersting label\nThat has more\nthan one line.");
		gtk_container_add (GTK_CONTAINER (frame), label);
		gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
	} else {
		label = nautilus_label_new ("This is a very intersting label\nThat has more\nthan one line.");
		gtk_container_add (GTK_CONTAINER (window), label);
	}

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
