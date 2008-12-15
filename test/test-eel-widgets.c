#include <config.h>

#include <eel/eel-radio-button-group.h>
#include <eel/eel-string-picker.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-ui-init.h>

static GdkPixbuf*
create_pixbuf (const char *name)
{
	char *path;
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (name != NULL, NULL);

	path = g_strdup_printf ( DATADIR "/pixmaps/nautilus/%s", name);
	
	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	g_assert (pixbuf != NULL);
	
	return pixbuf;
}

static void test_radio_group                     (void);
static void test_radio_group_horizontal          (void);
static void test_string_picker                   (void);
static void test_ok_dialog                       (void);

/* Callbacks */
static void test_radio_changed_callback          (GtkWidget *button_group,
						  gpointer   user_data);
static void string_picker_changed_callback       (GtkWidget *string_picker,
						  gpointer   user_data);
int
main (int argc, char * argv[])
{
	gtk_init (&argc, &argv);

	test_radio_group ();
	test_radio_group_horizontal ();
	test_string_picker ();
	test_ok_dialog ();

	gtk_main ();

	return 0;
}

static void
radio_group_load_it_up (EelRadioButtonGroup	*group, 
			gboolean			use_icons,
			gboolean			use_descriptions)
{
	g_return_if_fail (group != NULL);
	g_return_if_fail (EEL_IS_RADIO_BUTTON_GROUP (group));

	eel_radio_button_group_insert (EEL_RADIO_BUTTON_GROUP (group), "Apples");
	eel_radio_button_group_insert (EEL_RADIO_BUTTON_GROUP (group), "Oranges");
	eel_radio_button_group_insert (EEL_RADIO_BUTTON_GROUP (group), "Strawberries");
	
	if (use_descriptions)
	{
		eel_radio_button_group_set_entry_description_text (EEL_RADIO_BUTTON_GROUP (group), 0, "Apple description");
		eel_radio_button_group_set_entry_description_text (EEL_RADIO_BUTTON_GROUP (group), 1, "Oranges description");
		eel_radio_button_group_set_entry_description_text (EEL_RADIO_BUTTON_GROUP (group), 2, "Strawberries description");
	}

	if (use_icons)
	{
		GdkPixbuf *pixbufs[3];
		
		pixbufs[0] = create_pixbuf ("colors.png");
		pixbufs[1] = create_pixbuf ("backgrounds.png");
		pixbufs[2] = create_pixbuf ("emblems.png");
		
		eel_radio_button_group_set_entry_pixbuf (EEL_RADIO_BUTTON_GROUP (group), 0, pixbufs[0]);
		eel_radio_button_group_set_entry_pixbuf (EEL_RADIO_BUTTON_GROUP (group), 1, pixbufs[1]);
		eel_radio_button_group_set_entry_pixbuf (EEL_RADIO_BUTTON_GROUP (group), 2, pixbufs[2]);
		
		g_object_unref (pixbufs[0]);
		g_object_unref (pixbufs[1]);
		g_object_unref (pixbufs[2]);
	}
}

static void
test_radio_group (void)
{
	GtkWidget *window;
	GtkWidget *buttons;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "radio group test");

	buttons = eel_radio_button_group_new (FALSE);

	radio_group_load_it_up (EEL_RADIO_BUTTON_GROUP (buttons), TRUE, TRUE);

	g_signal_connect (buttons,
			    "changed",
			    G_CALLBACK (test_radio_changed_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), buttons);

	gtk_widget_show (buttons);

	gtk_widget_show (window);
}

static void
test_radio_group_horizontal (void)
{
	GtkWidget *window;
	GtkWidget *buttons;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "horizontal radio group test");

	buttons = eel_radio_button_group_new (TRUE);

	radio_group_load_it_up (EEL_RADIO_BUTTON_GROUP (buttons), FALSE, FALSE);

	g_signal_connect (buttons,
			    "changed",
			    G_CALLBACK (test_radio_changed_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), buttons);

	gtk_widget_show (buttons);

	gtk_widget_show (window);
}

static void
test_string_picker (void)
{
	GtkWidget		*window;
	GtkWidget		*picker;
	EelStringList	*font_list;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "string picker test");

	picker = eel_string_picker_new ();
	
	eel_caption_set_title_label (EEL_CAPTION (picker), "Icon Font Family:");

	font_list = eel_string_list_new (TRUE);

	eel_string_list_insert (font_list, "Helvetica");
	eel_string_list_insert (font_list, "Times");
	eel_string_list_insert (font_list, "Courier");
	eel_string_list_insert (font_list, "Lucida");
	eel_string_list_insert (font_list, "Fixed");

	eel_string_picker_set_string_list (EEL_STRING_PICKER (picker), font_list);

	eel_string_list_free (font_list);

	gtk_container_add (GTK_CONTAINER (window), picker);

	g_signal_connect (picker,
			    "changed",
			    G_CALLBACK (string_picker_changed_callback),
			    (gpointer) NULL);

	eel_string_picker_set_selected_string (EEL_STRING_PICKER (picker), "Fixed");

	gtk_widget_show_all (window);
}

static void
string_picker_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	char	  *text;

	g_assert (string_picker != NULL);
	g_assert (EEL_IS_STRING_PICKER (string_picker));

	text = eel_string_picker_get_selected_string (EEL_STRING_PICKER (string_picker));

	g_print ("string_picker_changed_callback(%s)\n", text);

	g_free (text);
}

static void
test_radio_changed_callback (GtkWidget *buttons, gpointer user_data)
{
	gint i;

	i = eel_radio_button_group_get_active_index (EEL_RADIO_BUTTON_GROUP (buttons));

	g_print ("test_radio_changed_callback (%d)\n", i);
}

static void
test_ok_dialog (void)
{
	GtkDialog *dialog;

	dialog = eel_show_error_dialog_with_details ("Some test information",
						     "the secondary text",
						     "the dialog title",
						     "the details text",
						     NULL);
	gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
	
	dialog = eel_show_info_dialog ("Some test information",
				       "the secondary text",
				       "the dialog title",
				       NULL);
	gtk_dialog_run (dialog);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}
