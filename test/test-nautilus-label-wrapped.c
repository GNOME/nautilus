#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}


static const char text[] = 
"The Nautilus shell is under development; it's not "
"ready for daily use. Some features are not yet done, "
"partly done, or unstable. The program doesn't look "
"or act exactly the way it will in version 1.0."
"\n\n"
"If you do decide to test this version of Nautilus,  "
"beware. The program could do something  "
"unpredictable and may even delete or overwrite  "
"files on your computer."
"\n\n"
"For more information, visit http://nautilus.eazel.com.";

static GtkWidget *
create_gtk_label ()
{
	GtkWidget *label;

 	label = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	
	return label;
}

static GtkWidget *
create_nautilus_label (gboolean adjust_wrap_on_resize)
{
	GtkWidget *label;

 	label = nautilus_label_new (text);
	nautilus_label_set_wrap (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	nautilus_label_set_smooth_drop_shadow_offset (NAUTILUS_LABEL (label), 1);
 	nautilus_label_set_background_mode (NAUTILUS_LABEL (label), NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_label_set_solid_background_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);
	nautilus_label_set_smooth_drop_shadow_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_BLUE);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_RED);
	nautilus_label_set_adjust_wrap_on_resize (NAUTILUS_LABEL (label), adjust_wrap_on_resize);

	return label;
}

static GtkWidget *
create_gtk_label_window (void)
{
	GtkWidget *gtk_window;
	GtkWidget *gtk_vbox;
	GtkWidget *gtk_label;

	gtk_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	nautilus_gtk_widget_set_background_color (gtk_window, "white");
	gtk_signal_connect (GTK_OBJECT (gtk_window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (gtk_window), "Gtk Wrapped Label Test");
	gtk_window_set_policy (GTK_WINDOW (gtk_window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (gtk_window), 10);
	gtk_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (gtk_window), gtk_vbox);
 	gtk_label = create_gtk_label ();
	gtk_box_pack_start (GTK_BOX (gtk_vbox), gtk_label, TRUE, TRUE, 0);

	return gtk_window;
}

static GtkWidget *
create_nautilus_label_window (gboolean adjust_wrap_on_resize)
{
	GtkWidget *nautilus_window;
	GtkWidget *nautilus_vbox;
	GtkWidget *nautilus_label;

	nautilus_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	nautilus_gtk_widget_set_background_color (nautilus_window, "white");
	gtk_signal_connect (GTK_OBJECT (nautilus_window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (nautilus_window), "Nautilus Wrapped Label Test");
	gtk_window_set_policy (GTK_WINDOW (nautilus_window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (nautilus_window), 10);
	nautilus_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (nautilus_window), nautilus_vbox);
 	nautilus_label = create_nautilus_label (adjust_wrap_on_resize);
	gtk_box_pack_start (GTK_BOX (nautilus_vbox), nautilus_label, TRUE, TRUE, 0);

	return nautilus_window;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *nautilus_window = NULL;
	GtkWidget *nautilus_window_wrapped = NULL;
	GtkWidget *gtk_window = NULL;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	if (0) nautilus_window = create_nautilus_label_window (FALSE);
	if (1) nautilus_window_wrapped = create_nautilus_label_window (TRUE);
	if (0) gtk_window =  create_gtk_label_window ();

	if (nautilus_window) gtk_widget_show_all (nautilus_window);
	if (nautilus_window_wrapped) gtk_widget_show_all (nautilus_window_wrapped);
	if (gtk_window) gtk_widget_show_all (gtk_window);

	gtk_main ();

	return 0;
}
