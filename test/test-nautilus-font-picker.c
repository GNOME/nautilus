#include <config.h>

#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-init.h>
#include <libnautilus-extensions/nautilus-font-picker.h>

static void
font_picker_changed_callback (GtkWidget *font_picker, gpointer user_data)
{
	char *family;
	char *weight;
	char *slant;
	char *set_width;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	family = nautilus_font_picker_get_selected_family (NAUTILUS_FONT_PICKER (font_picker));
	weight = nautilus_font_picker_get_selected_weight (NAUTILUS_FONT_PICKER (font_picker));
	slant = nautilus_font_picker_get_selected_slant (NAUTILUS_FONT_PICKER (font_picker));
	set_width = nautilus_font_picker_get_selected_set_width (NAUTILUS_FONT_PICKER (font_picker));

	g_print ("%s (%s,%s,%s,%s)\n", __FUNCTION__, family, weight, slant, set_width);

	g_free (family);
	g_free (weight);
	g_free (slant);
	g_free (set_width);
}

int
main (int argc, char * argv[])
{
	GtkWidget	*window;
	GtkWidget	*font_picker;

	gnome_init ("foo", "bar", argc, argv);
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	font_picker = nautilus_font_picker_new ();
	
	gtk_container_add (GTK_CONTAINER (window), font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker),
			    "selected_font_changed",
			    GTK_SIGNAL_FUNC (font_picker_changed_callback),
			    (gpointer) NULL);

	gtk_widget_show (font_picker);
	gtk_widget_show (window);
	gtk_main ();

	return 0;
}
