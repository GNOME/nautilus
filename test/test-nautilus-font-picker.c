#include "test.h"

#include <libnautilus-extensions/nautilus-font-picker.h>
#include <libnautilus-extensions/nautilus-font-manager.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>

static void
update_font (NautilusLabel *label,
	     const char *font_file_name)
{
	NautilusScalableFont *font;
	
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (font_file_name != NULL);

	font = nautilus_scalable_font_new (font_file_name);
	nautilus_label_set_smooth_font (NAUTILUS_LABEL (label), font);
	gtk_object_unref (GTK_OBJECT (font));

	nautilus_preferences_set (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT, font_file_name);
}

static void
font_changed_update_label_callback (NautilusFontPicker *font_picker,
				    gpointer callback_data)
{
	char *font_file_name;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (callback_data));

	font_file_name = nautilus_font_picker_get_selected_font (font_picker);
	update_font (NAUTILUS_LABEL (callback_data), font_file_name);
	g_free (font_file_name);
}

static void
font_changed_update_file_name_callback (NautilusFontPicker *font_picker,
					gpointer callback_data)
{
	char *font_file_name;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (NAUTILUS_IS_TEXT_CAPTION (callback_data));

	font_file_name = nautilus_font_picker_get_selected_font (font_picker);
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (callback_data), font_file_name);
	g_free (font_file_name);
}

static void
use_defalt_font_callback (GtkWidget *button,
			  gpointer callback_data)
{
	char *default_font;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_LABEL (callback_data));

	default_font = nautilus_font_manager_get_default_font ();
	update_font (NAUTILUS_LABEL (callback_data), default_font);
	g_free (default_font);
}

static void
use_defalt_bold_font_callback (GtkWidget *button,
			       gpointer callback_data)
{
	char *default_bold_font;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_LABEL (callback_data));

	default_bold_font = nautilus_font_manager_get_default_bold_font ();
	update_font (NAUTILUS_LABEL (callback_data), default_bold_font);
	g_free (default_bold_font);
}

static void
use_defalt_font_update_picker_callback (GtkWidget *button,
					gpointer callback_data)
{
	char *default_font;
	
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	default_font = nautilus_font_manager_get_default_font ();
	nautilus_font_picker_set_selected_font (NAUTILUS_FONT_PICKER (callback_data),
						default_font);
	g_free (default_font);
}

static void
use_defalt_bold_font_update_picker_callback (GtkWidget *button,
					     gpointer callback_data)
{
	char *default_bold_font;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	default_bold_font = nautilus_font_manager_get_default_bold_font ();
	nautilus_font_picker_set_selected_font (NAUTILUS_FONT_PICKER (callback_data),
						default_bold_font);
	g_free (default_bold_font);
}

static void
print_selected_font_callback (GtkWidget *button,
			      gpointer callback_data)
{
	char *selected_font;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	selected_font = nautilus_font_picker_get_selected_font (NAUTILUS_FONT_PICKER (callback_data));

	g_free (selected_font);
}

int
main (int argc, char * argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *font_picker;
	GtkWidget *label;
	GtkWidget *file_name_caption;
	GtkWidget *default_font_caption;
	GtkWidget *default_bold_font_caption;
	NautilusScalableFont *font;
	GtkWidget *use_defailt_button;
	GtkWidget *use_defailt_bold_button;
	GtkWidget *default_hbox;
	GtkWidget *default_bold_hbox;
	GtkWidget *print_selected_font_button;
	char *current_font;
	char *default_font;
	char *default_bold_font;

	test_init (&argc, &argv);

	nautilus_global_preferences_initialize ();

	window = test_window_new ("Font Picker Test", 10);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	default_font = nautilus_font_manager_get_default_font ();
	default_bold_font = nautilus_font_manager_get_default_bold_font ();

	font_picker = nautilus_font_picker_new ();

	current_font = nautilus_preferences_get (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT);
	nautilus_font_picker_set_selected_font (NAUTILUS_FONT_PICKER (font_picker),
						current_font);

	label = nautilus_label_new ("Something\nハードウェア");
	nautilus_label_set_is_smooth (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_make_larger (NAUTILUS_LABEL (label), 40);

	font = nautilus_scalable_font_new (current_font);
	nautilus_label_set_smooth_font (NAUTILUS_LABEL (label), font);
	gtk_object_unref (GTK_OBJECT (font));
	
	gtk_signal_connect (GTK_OBJECT (font_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (font_changed_update_label_callback),
			    label);

	file_name_caption = nautilus_text_caption_new ();
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (file_name_caption),
					  "Current Font");
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (file_name_caption), current_font);

	gtk_signal_connect (GTK_OBJECT (font_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (font_changed_update_file_name_callback),
			    file_name_caption);
	

	default_hbox = gtk_hbox_new (FALSE, 0);
	default_font_caption = nautilus_text_caption_new ();
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (default_font_caption),
					  "Default Font");
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (default_font_caption), default_font);
	use_defailt_button = gtk_button_new_with_label ("Use");
	gtk_signal_connect (GTK_OBJECT (use_defailt_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (use_defalt_font_callback),
			    label);
	gtk_signal_connect (GTK_OBJECT (use_defailt_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (use_defalt_font_update_picker_callback),
			    font_picker);
	gtk_box_pack_start (GTK_BOX (default_hbox), default_font_caption, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (default_hbox), use_defailt_button, FALSE, FALSE, 0);


	default_bold_hbox = gtk_hbox_new (FALSE, 0);
	default_bold_font_caption = nautilus_text_caption_new ();
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (default_bold_font_caption),
					  "Default Bold Font");
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (default_bold_font_caption), default_bold_font);
	use_defailt_bold_button = gtk_button_new_with_label ("Use");
	gtk_signal_connect (GTK_OBJECT (use_defailt_bold_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (use_defalt_bold_font_callback),
			    label);
	gtk_signal_connect (GTK_OBJECT (use_defailt_bold_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (use_defalt_bold_font_update_picker_callback),
			    font_picker);
	gtk_box_pack_start (GTK_BOX (default_bold_hbox), default_bold_font_caption, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (default_bold_hbox), use_defailt_bold_button, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), font_picker, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 10);
	gtk_box_pack_start (GTK_BOX (vbox), file_name_caption, TRUE, TRUE, 10);
	gtk_box_pack_start (GTK_BOX (vbox), default_hbox, TRUE, TRUE, 10);
	gtk_box_pack_start (GTK_BOX (vbox), default_bold_hbox, TRUE, TRUE, 10);

	print_selected_font_button = gtk_button_new_with_label ("Print selected font");
	gtk_signal_connect (GTK_OBJECT (print_selected_font_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (print_selected_font_callback),
			    font_picker);
	gtk_box_pack_start (GTK_BOX (vbox), print_selected_font_button, FALSE, FALSE, 10);

	g_free (current_font);
	g_free (default_font);

	gtk_widget_show (font_picker);
	gtk_widget_show (label);
	gtk_widget_show (file_name_caption);
	gtk_widget_show (print_selected_font_button);
	gtk_widget_show_all (default_hbox);
	gtk_widget_show_all (default_bold_hbox);
	gtk_widget_show (vbox);
	gtk_widget_show (window);

	gtk_main ();
	return test_quit (EXIT_SUCCESS);
}
