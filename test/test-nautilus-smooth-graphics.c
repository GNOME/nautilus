#include <config.h>

#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-init.h>
#include <libnautilus-private/nautilus-global-preferences.h>

static void
button_toggled (GtkWidget *button,
		gpointer callback_data)
{
	eel_preferences_set_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
				     GTK_TOGGLE_BUTTON (button)->active);
}

static void
smooth_graphics_mode_changed_callback (gpointer callback_data)
{
	gboolean is_smooth;

	is_smooth = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (callback_data),
				      eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE));
}

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}
	
int
main (int argc, char * argv[])
{
	GtkWidget *window;
	GtkWidget *button;
	
	gnome_init ("foo", "bar", argc, argv);

	nautilus_global_preferences_initialize ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	
	button = gtk_toggle_button_new_with_label ("Smooth Graphics");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE));
	
	gtk_container_add (GTK_CONTAINER (window), button);

	gtk_signal_connect (GTK_OBJECT (button),
			    "toggled",
			    GTK_SIGNAL_FUNC (button_toggled),
			    NULL);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
				      smooth_graphics_mode_changed_callback, 
					   button);

	gtk_widget_show (button);
	gtk_widget_show (window);

	gtk_main ();

	return 0;
}
