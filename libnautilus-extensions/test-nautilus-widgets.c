
#include <nautilus-widgets/nautilus-radio-button-group.h>
#include <gtk/gtk.h>
#include <stdio.h>

static void test_radio_group (void);
static void test_radio_changed_signal (GtkWidget *button_group, gpointer user_data);

int
main (int argc, char * argv[])
{
	gtk_init (&argc, &argv);
	
	test_radio_group ();
		
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
			    GTK_SIGNAL_FUNC (test_radio_changed_signal),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), buttons);

	gtk_widget_show (buttons);

	gtk_widget_show (window);
}

static void
test_radio_changed_signal (GtkWidget *buttons, gpointer user_data)
{
	gint i;

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));

	printf ("test_radio_changed_signal (%d)\n", i);
}
