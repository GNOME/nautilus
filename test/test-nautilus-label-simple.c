#include <config.h>

#include <gtk/gtk.h>

#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>

#include <libgnomevfs/gnome-vfs-init.h>

typedef struct
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *entry;
	GtkWidget *hbox;
	GtkWidget *smooth_toggle;
	GtkWidget *frame;
	GtkWidget *label;
} Window;

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}

#if 0
static void
toggle_smooth_callback (GtkWidget *widget, gpointer callback_data)
{
	Window *window;
	NautilusLabel *label;

	window = (Window *) callback_data;

	if (!NAUTILUS_IS_LABEL (window->label)) {
		return;
	}

	label = NAUTILUS_LABEL (window->label);

	nautilus_label_set_is_smooth (label, !nautilus_label_get_is_smooth (label));
}
#endif

static Window *
window_new (const char *title, guint border_width)
{
	Window *window;
	GtkWidget *main_vbox;

	window = g_new0 (Window, 1);

	window->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	main_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window->window), main_vbox);

	if (title != NULL) {

		gtk_window_set_title (GTK_WINDOW (window->window), title);
	}

	gtk_signal_connect (GTK_OBJECT (window->window),
			    "delete_event",
			    GTK_SIGNAL_FUNC (delete_event),
			    NULL);

	gtk_window_set_policy (GTK_WINDOW (window->window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window->window), border_width);

	window->vbox = gtk_vbox_new (FALSE, 0);
	window->entry = gtk_entry_new ();
	window->hbox = gtk_hbox_new (FALSE, 0);
//	window->smooth_toggle = gtk_check_button_new_with_label ("Smooth");
	
	gtk_box_pack_start (GTK_BOX (main_vbox), window->vbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (main_vbox), window->hbox, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (main_vbox), window->entry, FALSE, FALSE, 0);

//	gtk_box_pack_start (GTK_BOX (window->hbox), window->smooth_toggle, FALSE, FALSE, 0);
	
	gtk_widget_show (main_vbox);
	gtk_widget_show (window->vbox);
	gtk_widget_show (window->hbox);
	gtk_widget_show (window->entry);
//	gtk_widget_show (window->smooth_toggle);

#if 0	
	gtk_signal_connect (GTK_OBJECT (window->smooth_toggle),
			    "toggled",
			    GTK_SIGNAL_FUNC (toggle_smooth_callback),
			    window);
#endif
	
	return window;
}

static Window *
label_window_new (const char *title,
		   guint border_width,
		   const char *file_name,
		   const char *tile_file_name)
{
	Window *window;
	
	window = window_new (title, border_width);

	window->frame = gtk_frame_new ("Foo");
	window->label = nautilus_label_new (file_name);
	
	if (tile_file_name != NULL) {
		nautilus_label_set_tile_pixbuf_from_file_name (NAUTILUS_LABEL (window->label),
								tile_file_name);
	}

	gtk_container_add (GTK_CONTAINER (window->frame), window->label);
	
	gtk_box_pack_start (GTK_BOX (window->vbox), window->frame, TRUE, TRUE, 0);
	
	gtk_widget_show (window->label);
	gtk_widget_show (window->frame);

	//gtk_widget_set_sensitive (window->smooth_toggle, TRUE);
/* 	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->smooth_toggle), */
/* 				      nautilus_label_get_is_smooth (NAUTILUS_LABEL (window->label))); */

	return window;
}

int 
main (int argc, char* argv[])
{
	Window *window;
	
	gtk_init (&argc, &argv);
	gdk_rgb_init ();
	gnome_vfs_init ();

	window = label_window_new ("Nautilus Label",
				    100,
				    "This is a very intersting label\nThat has more\nthan one line.",
				    NULL);
	//"/gnome/share/nautilus/patterns/pale_coins.png");
	//window = label_window_new ("Nautilus Label", 100, "eazel-logo-right-side-logo.png");
	//window = label_window_new ("Nautilus Label", 100, "foo.png");

//	debug_widget_set_background_label (window->window, "pale_coins.png");
//	debug_widget_set_background_label (window->label, "pale_coins.png");
	
	gtk_widget_show (window->window);

	gtk_main ();

	return 0;
}
