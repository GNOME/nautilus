#include "test.h"

int 
main (int argc, char* argv[])
{
	//const char *text = "This is a very intersting label\nThat has more\nthan one line.";
	const char *text = "Something";
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label1;
	GtkWidget *label2;

	test_init (&argc, &argv);

	window = test_window_new ("Simple Label Test", 20);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	label1 = nautilus_label_new ("");
	nautilus_label_make_larger (NAUTILUS_LABEL (label1), 40);
	nautilus_label_set_wrap (NAUTILUS_LABEL (label1), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label1), GTK_JUSTIFY_CENTER);
	nautilus_label_set_text (NAUTILUS_LABEL (label1), text);
	gtk_box_pack_start (GTK_BOX (vbox), label1, TRUE, TRUE, 0);

	label2 = nautilus_label_new ("");
	nautilus_label_make_larger (NAUTILUS_LABEL (label2), 40);
	nautilus_label_make_bold (NAUTILUS_LABEL (label2));
	nautilus_label_set_wrap (NAUTILUS_LABEL (label2), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label2), GTK_JUSTIFY_CENTER);
	nautilus_label_set_text (NAUTILUS_LABEL (label2), text);
	gtk_box_pack_start (GTK_BOX (vbox), label2, TRUE, TRUE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
