
#include <config.h>

#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libnautilus-extensions/nautilus-password-dialog.h>
#include <libnautilus-extensions/nautilus-radio-button-group.h>
#include <libnautilus-extensions/nautilus-string-picker.h>
#include <libnautilus-extensions/nautilus-text-caption.h>

#include <gtk/gtk.h>

static void test_radio_group                     (void);
static void test_caption_table                   (void);
static void test_password_dialog                 (void);
static void test_string_picker                   (void);
static void test_text_caption                    (void);

/* Callbacks */
static void test_radio_changed_callback          (GtkWidget *button_group,
						  gpointer   user_data);
static void test_authenticate_boink_callback     (GtkWidget *button,
						  gpointer   user_data);
static void string_picker_changed_callback       (GtkWidget *string_picker,
						  gpointer   user_data);
static void text_caption_changed_callback        (GtkWidget *text_caption,
						  gpointer   user_data);
static void test_caption_table_activate_callback (GtkWidget *button_group,
						  gint       active_index,
						  gpointer   user_data);

int
main (int argc, char * argv[])
{
	gnome_init ("foo", "bar", argc, argv);

	test_radio_group ();
	test_caption_table ();
	test_password_dialog ();
	test_string_picker ();
	test_text_caption ();

	gtk_main ();

	return 0;
}

static void
test_radio_group (void)
{
	GtkWidget * window;
	GtkWidget * buttons;


	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	buttons = nautilus_radio_button_group_new ();

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (buttons), "Apples");
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (buttons), "Oranges");
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (buttons), "Strawberries");

	gtk_signal_connect (GTK_OBJECT (buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (test_radio_changed_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), buttons);

	gtk_widget_show (buttons);

	gtk_widget_show (window);
}

static void
test_caption_table (void)
{
	GtkWidget * window;
	GtkWidget * table;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	table = nautilus_caption_table_new (4);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     0,
					     "Something",
					     "Text",
					     TRUE,
					     FALSE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     1,
					     "ReadOnly",
					     "Cant Change Me",
					     TRUE,
					     TRUE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     2,
					     "Password",
					     "sekret",
					     FALSE,
					     FALSE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     3,
					     "This is a very long label",
					     "Text",
					     TRUE,
					     FALSE);

	gtk_signal_connect (GTK_OBJECT (table),
			    "activate",
			    GTK_SIGNAL_FUNC (test_caption_table_activate_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), table);

	gtk_widget_show (table);

	gtk_widget_show (window);
}

static void
test_string_picker (void)
{
	GtkWidget		*window;
	GtkWidget		*picker;
	NautilusStringList	*font_list;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	picker = nautilus_string_picker_new ();
	
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (picker), "Icon Font Family:");

	font_list = nautilus_string_list_new ();

	nautilus_string_list_insert (font_list, "Helvetica");
	nautilus_string_list_insert (font_list, "Times");
	nautilus_string_list_insert (font_list, "Courier");
	nautilus_string_list_insert (font_list, "Lucida");
	nautilus_string_list_insert (font_list, "Fixed");

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (picker), font_list);

	nautilus_string_list_free (font_list);

	gtk_container_add (GTK_CONTAINER (window), picker);

	gtk_signal_connect (GTK_OBJECT (picker),
			    "changed",
			    GTK_SIGNAL_FUNC (string_picker_changed_callback),
			    (gpointer) NULL);

	gtk_widget_show_all (window);
}

static void
test_text_caption (void)
{
	GtkWidget		*window;
	GtkWidget		*picker;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	picker = nautilus_text_caption_new ();
	
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (picker), "Home Page:");

	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (picker), "file:///tmp");
	
	gtk_container_add (GTK_CONTAINER (window), picker);
	
	gtk_signal_connect (GTK_OBJECT (picker),
			    "changed",
			    GTK_SIGNAL_FUNC (text_caption_changed_callback),
			    (gpointer) NULL);

	gtk_widget_show_all (window);
}

static void
test_authenticate_boink_callback (GtkWidget *button, gpointer user_data)
{
	GtkWidget *dialog;
	gboolean  rv;
	char	  *username;
	char	  *password;

	dialog = nautilus_password_dialog_new ("Authenticate Me",
					       "foouser",
					       "sekret",
					       FALSE);

	rv = nautilus_password_dialog_run_and_block (NAUTILUS_PASSWORD_DIALOG (dialog));

	username = nautilus_password_dialog_get_username (NAUTILUS_PASSWORD_DIALOG (dialog));
	password = nautilus_password_dialog_get_password (NAUTILUS_PASSWORD_DIALOG (dialog));

	g_assert (username != NULL);
	g_assert (password != NULL);

	g_print ("test_authenticate_boink_callback (rv=%d , username='%s' , password='%s')\n",
		 rv,
		 username,
		 password);

	g_free (username);
	g_free (password);
}

static void
string_picker_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	char	  *text;

	g_assert (string_picker != NULL);
	g_assert (NAUTILUS_IS_STRING_PICKER (string_picker));

	text = nautilus_string_picker_get_text (NAUTILUS_STRING_PICKER (string_picker));

	g_print ("string_picker_changed_callback(%s)\n", text);
	
	g_free (text);
}

static void
text_caption_changed_callback (GtkWidget *text_caption, gpointer user_data)
{
	char	  *text;

	g_assert (text_caption != NULL);
	g_assert (NAUTILUS_IS_TEXT_CAPTION (text_caption));

	text = nautilus_text_caption_get_text (NAUTILUS_TEXT_CAPTION (text_caption));

	g_print ("text_caption_changed_callback(%s)\n", text);
	
	g_free (text);
}

static void
test_password_dialog (void)
{
	GtkWidget * window;
	GtkWidget * button;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	button = gtk_button_new_with_label ("Boink me to authenticate");

	gtk_signal_connect (GTK_OBJECT (button),
			    "clicked",
			    GTK_SIGNAL_FUNC (test_authenticate_boink_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), button);

	gtk_widget_show (button);

	gtk_widget_show (window);
}

static void
test_radio_changed_callback (GtkWidget *buttons, gpointer user_data)
{
	gint i;

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));

	g_print ("test_radio_changed_callback (%d)\n", i);
}

static void
test_caption_table_activate_callback (GtkWidget  *button_group,
				      gint        active_index,
				      gpointer    user_data)
{
	g_print ("test_caption_table_activate_callback (active_index=%d)\n", active_index);
}
