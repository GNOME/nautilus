#include "test.h"

static void
use_system_font_callback (GtkButton *button,
			  GtkLabel *label)
{
	GdkFont *system_font;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_LABEL (label));

	system_font = nautilus_gtk_get_system_font ();
	
	g_return_if_fail (system_font != NULL);

	nautilus_gtk_widget_set_font (GTK_WIDGET (label), system_font);

	gdk_font_unref (system_font);
}

static void
use_system_font_bold_callback (GtkButton *button,
			       GtkLabel *label)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_LABEL (label));

	use_system_font_callback (button, label);
	nautilus_label_make_bold (NAUTILUS_LABEL (label));
}

int 
main (int argc, char* argv[])
{
	//const char *text = "This is a very intersting label\nThat has more\nthan one line.";
	const char *text = "Something";
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *use_system_font;

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

	use_system_font = gtk_button_new_with_label ("Use System Font");
	gtk_box_pack_start (GTK_BOX (vbox), use_system_font, TRUE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (use_system_font),
			    "clicked",
			    GTK_SIGNAL_FUNC (use_system_font_callback),
			    label1);

	gtk_signal_connect (GTK_OBJECT (use_system_font),
			    "clicked",
			    GTK_SIGNAL_FUNC (use_system_font_bold_callback),
			    label2);
	
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
