#include "test.h"

#include <libnautilus-extensions/nautilus-font-picker.h>
#include <libnautilus-extensions/nautilus-font-manager.h>

static void
font_picker_changed_callback (NautilusFontPicker *font_picker,
			      gpointer callback_data)
{
	NautilusScalableFont *font;
	char *font_file_name;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (callback_data));

	font_file_name = nautilus_font_picker_get_selected_font (font_picker);
	g_print ("font changed  = %s\n", font_file_name);

	font = nautilus_scalable_font_new_from_file_name (font_file_name);
	nautilus_label_set_smooth_font (NAUTILUS_LABEL (callback_data), font);
	gtk_object_unref (GTK_OBJECT (font));

	g_free (font_file_name);
}

int
main (int argc, char * argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *font_picker;
	GtkWidget *label;

	NautilusScalableFont *font;

	char *fallback_font;

	test_init (&argc, &argv);

	window = test_window_new ("Font Picker Test", 10);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	fallback_font = nautilus_font_manager_get_fallback_font ();
	g_print ("fallback_font = %s\n", fallback_font);

	font_picker = nautilus_font_picker_new ();

	label = nautilus_label_new ("Something");
	nautilus_label_set_is_smooth (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_make_larger (NAUTILUS_LABEL (label), 40);

	font = nautilus_scalable_font_new_from_file_name ("/usr/share/fonts/default/truetype/times.ttf");

	nautilus_label_set_smooth_font (NAUTILUS_LABEL (label), font);
	
	gtk_object_unref (GTK_OBJECT (font));

	gtk_signal_connect (GTK_OBJECT (font_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (font_picker_changed_callback),
			    label);

	nautilus_font_picker_set_selected_font (NAUTILUS_FONT_PICKER (font_picker),
						"/usr/share/fonts/ISO8859-2/Type1/gatsb___.pfb");
	
	gtk_box_pack_start (GTK_BOX (vbox), font_picker, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 10);

	gtk_widget_show_all (window);

	gtk_main ();
	return test_quit (EXIT_SUCCESS);
}
