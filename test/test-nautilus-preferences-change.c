#include "test.h"

#include <eel/eel-string-picker.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <unistd.h>

static void
fruits_changed_callback (gpointer callback_data)
{
	g_print ("Something underneath 'fruits' changed, dunno what\n");
}

static void
int_picker_changed_callback (EelStringPicker *string_picker,
			     gpointer callback_data)
{
	char *selected_string;
	int new_value;

	g_return_if_fail (EEL_IS_STRING_PICKER (string_picker));
	g_return_if_fail (callback_data != NULL);

	selected_string = eel_string_picker_get_selected_string (string_picker);
	
	new_value = eel_string_picker_get_index_for_string (string_picker, selected_string);

	eel_preferences_set_integer ((const char *) callback_data, new_value);

	g_free (selected_string);
}

static GtkWidget *
picker_new (const char *name,
	    const EelStringList *entries)
{
	GtkWidget *string_picker;
	
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (entries != NULL, NULL);
	
	string_picker = eel_string_picker_new ();
	eel_caption_set_title_label (EEL_CAPTION (string_picker), name);
	g_signal_connect (string_picker, "changed",
			  G_CALLBACK (int_picker_changed_callback), (gpointer) name);
	
	eel_string_picker_set_string_list (EEL_STRING_PICKER (string_picker), entries);
	eel_string_picker_set_selected_string_index (EEL_STRING_PICKER (string_picker), 
						     eel_preferences_get_integer (name));

	return string_picker;
}

int 
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *green_picker;
	GtkWidget *yellow_picker;
	GtkWidget *red_picker;
	GtkWidget *fruits_apple_picker;
	GtkWidget *fruits_orange_picker;
	GtkWidget *fruits_pear_picker;

	GtkWidget *vbox;

	EelStringList *user_level_entries;
	EelStringList *color_entries;
	EelStringList *fruits_entries;

	test_init (&argc, &argv);

	nautilus_global_preferences_init ();

	user_level_entries = eel_string_list_new_from_tokens ("Beginner,Intermediate,Advanced", ",", TRUE);
	color_entries = eel_string_list_new_from_tokens ("0,1,2,3,4,5,6,7,8,9,10", ",", TRUE);
	fruits_entries = eel_string_list_new_from_tokens ("0,1,2,3", ",", TRUE);
	
	eel_preferences_set_emergency_fallback_integer ("green", 3);

	eel_preferences_set_emergency_fallback_integer ("yellow", 9);

	eel_preferences_set_emergency_fallback_integer ("red", 7);

	eel_preferences_set_emergency_fallback_integer ("fruits/apple", 1);
	eel_preferences_set_emergency_fallback_integer ("fruits/orange", 2);
	eel_preferences_set_emergency_fallback_integer ("fruits/pear", 3);

	//sleep (10);

	window = test_window_new (NULL, 4);
	test_window_set_title_with_pid (GTK_WINDOW (window), "Preferences Change");

	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	green_picker = picker_new ("green", color_entries);
	yellow_picker = picker_new ("yellow", color_entries);
	red_picker = picker_new ("red", color_entries);
	fruits_apple_picker = picker_new ("fruits/apple", fruits_entries);
	fruits_orange_picker = picker_new ("fruits/orange", fruits_entries);
	fruits_pear_picker = picker_new ("fruits/pear", fruits_entries);
        
	gtk_box_pack_start (GTK_BOX (vbox), green_picker, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), yellow_picker, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), red_picker, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), fruits_apple_picker, FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (vbox), fruits_orange_picker, FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (vbox), fruits_pear_picker, FALSE, FALSE, 20);

	eel_string_list_free (user_level_entries);
	eel_string_list_free (color_entries);
	eel_string_list_free (fruits_entries);

	eel_preferences_add_callback ("fruits", fruits_changed_callback, NULL);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
