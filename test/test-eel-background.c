/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <eel/eel-background.h>
#include <gtk/gtk.h>

#define PATTERNS_DIR "/gnome-source/eel/data/patterns"

int
main  (int argc, char *argv[])
{
	GtkWidget *window;
	EelBackground *background;
	char *image_uri;
#if 0
	char *titles[] = { "test 1", "test 2", "test 3"};
#endif

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
#if 0
	ctree = gtk_ctree_new_with_titles (3, 2, titles);
	gtk_container_add (GTK_CONTAINER (window), ctree);
#endif
	g_signal_connect (window, "destroy",
			    gtk_main_quit, NULL);

	background = eel_get_widget_background (window);

	eel_background_set_color (background,
				  "red-blue:h");

	image_uri = g_filename_to_uri (PATTERNS_DIR "/50s.png", NULL, NULL);

#if 1
	eel_background_set_image_uri (background, image_uri);
#endif
	g_free (image_uri);


	gtk_widget_show_all (window);
	gtk_main ();

	return 0;
}
