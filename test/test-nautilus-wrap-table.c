#include "test.h"

#include <eel/eel-wrap-table.h>
#include <eel/eel-labeled-image.h>
#include <libnautilus-private/nautilus-customization-data.h>
#include <libnautilus-private/nautilus-icon-info.h>

int 
main (int argc, char* argv[])
{
	NautilusCustomizationData *customization_data;
	GtkWidget *window;
	GtkWidget *emblems_table, *button, *scroller;
	char *emblem_name, *dot_pos;
	GdkPixbuf *pixbuf;
	char *label;

	test_init (&argc, &argv);

	window = test_window_new ("Wrap Table Test", 10);

	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

	/* The emblems wrapped table */
	emblems_table = eel_wrap_table_new (TRUE);

	gtk_widget_show (emblems_table);
	gtk_container_set_border_width (GTK_CONTAINER (emblems_table), GNOME_PAD);
	
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	/* Viewport */
 	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroller), 
 					       emblems_table);

	gtk_container_add (GTK_CONTAINER (window), scroller);

	gtk_widget_show (scroller);

#if 0
	/* Get rid of default lowered shadow appearance. 
	 * This must be done after the widget is realized, due to
	 * an apparent bug in gtk_viewport_set_shadow_type.
	 */
 	g_signal_connect (GTK_BIN (scroller->child), 
			  "realize", 
			  remove_default_viewport_shadow, 
			  NULL);
#endif


	/* Use nautilus_customization to make the emblem widgets */
	customization_data = nautilus_customization_data_new ("emblems", TRUE,
							      NAUTILUS_ICON_SIZE_SMALL, 
							      NAUTILUS_ICON_SIZE_SMALL);
	
	while (nautilus_customization_data_get_next_element_for_display (customization_data,
									 &emblem_name,
									 &pixbuf,
									 &label) == GNOME_VFS_OK) {	

		/* strip the suffix, if any */
		dot_pos = strrchr(emblem_name, '.');
		if (dot_pos) {
			*dot_pos = '\0';
		}
		
		if (strcmp (emblem_name, "erase") == 0) {
			g_object_unref (pixbuf);
			g_free (label);
			g_free (emblem_name);
			continue;
		}
		
		button = eel_labeled_image_check_button_new (label, pixbuf);
		g_free (label);
		g_object_unref (pixbuf);

		/* Attach parameters and signal handler. */
		g_object_set_data_full (G_OBJECT (button),
					"nautilus_property_name",
					emblem_name,
					(GDestroyNotify) g_free);
				     
		gtk_container_add (GTK_CONTAINER (emblems_table), button);
	}

	gtk_widget_show_all (emblems_table);

	gtk_widget_show (window);
	
	gtk_main ();
	
	return 0;
}
