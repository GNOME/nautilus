#include "test.h"
#include <libnautilus-extensions/nautilus-label.h>

#if 0
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

	window->window = test_window_new (title, border_width);

	main_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window->window), main_vbox);

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

	return window;
}
#endif

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
label_window_new (void)
{
	GtkWidget *window;
	GtkWidget *label;
	NautilusBackground *background;

	window = test_window_new ("Scrolled Label Test", 10);

	background = nautilus_get_widget_background (GTK_WIDGET (window));
	nautilus_background_set_color (background, "white");

	/* Label */
	label = nautilus_label_new (text);
	nautilus_label_set_wrap (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	nautilus_label_set_background_mode (NAUTILUS_LABEL (label), NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_label_set_solid_background_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_RED);
	
	gtk_container_add (GTK_CONTAINER (window), label);

	gtk_widget_show (label);

	return window;
}

static GtkWidget *
label_window_new_scrolled (void)
{
	GtkWidget *window;
	GtkWidget *scrolled;
	GtkWidget *viewport;
	GtkWidget *label;
	NautilusBackground *background;

	window = test_window_new ("Scrolled Label Test", 10);

	/* Scrolled window */
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (window), scrolled);

	/* Viewport */
 	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (scrolled), viewport);

	background = nautilus_get_widget_background (GTK_WIDGET (viewport));
	nautilus_background_set_color (background, "white");

	/* Label */
	label = nautilus_label_new (text);
	nautilus_label_set_wrap (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	nautilus_label_set_background_mode (NAUTILUS_LABEL (label), NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_label_set_solid_background_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_RED);
	
	gtk_container_add (GTK_CONTAINER (viewport), label);

	gtk_widget_show (label);
	gtk_widget_show (viewport);
	gtk_widget_show (scrolled);

	return window;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *label_window;
	GtkWidget *scrolled_label_window;
	
	test_init (&argc, &argv);

	label_window = label_window_new ();
	scrolled_label_window = label_window_new_scrolled ();

	gtk_widget_show (scrolled_label_window);
	gtk_widget_show (label_window);

	gtk_main ();

	return 0;
}
