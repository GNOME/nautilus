#include "test.h"

#include <libnautilus-extensions/nautilus-image-table.h>
#include <libnautilus-extensions/nautilus-viewport.h>

static const char pixbuf_name[] = "/usr/share/pixmaps/gnome-globe.png";

static const char *names[] =
{
	"Tomaso Albinoni",
	"Isaac Albéniz",
	"Georges Bizet",
	"Luigi Boccherini",
	"Alexander Borodin",
	"Johannes Brahms",
	"Max Bruch",
	"Anton Bruckner",
	"Frédéric Chopin",
	"Aaron Copland",
	"John Corigliano",
	"Claude Debussy",
	"Léo Delibes",
	"Antonín Dvorák",
	"Edward Elgar",
	"Manuel de Falla",
	"George Gershwin",
	"Alexander Glazunov",
	"Mikhail Glinka",
	"Enrique Granados",
	"Edvard Grieg",
	"Joseph Haydn",
	"Scott Joplin",
	"Franz Liszt",
	"Gustav Mahler",
	"Igor Markevitch",
	"Felix Mendelssohn",
	"Modest Mussorgsky",
	"Sergei Prokofiev",
	"Giacomo Puccini",
	"Maurice Ravel",
	"Ottorino Respighi",
	"Joaquin Rodrigo",
	"Gioachino Rossini",
	"Domenico Scarlatti",
	"Franz Schubert",
	"Robert Schumann",
	"Jean Sibelius",
	"Bedrich Smetana",
	"Johann Strauss",
	"Igor Stravinsky",
	"Giuseppe Verdi",
	"Antonio Vivaldi",
	"Richard Wagner",
};

static GtkWidget *
labeled_image_new (const char *text,
		   const char *icon_name)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	
	if (icon_name) {
		const float sizes[] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
					1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0 };
		pixbuf = test_pixbuf_new_named (icon_name, sizes[random () % NAUTILUS_N_ELEMENTS (sizes)]);
	}

	image = nautilus_labeled_image_new (text, pixbuf);

	nautilus_labeled_image_set_background_mode (NAUTILUS_LABELED_IMAGE (image),
						    NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_labeled_image_set_solid_background_color (NAUTILUS_LABELED_IMAGE (image),
							   0xFFFFFF);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);

	return image;
}


static void
image_table_child_enter_callback (GtkWidget *image_table,
			       GtkWidget *item,
			       gpointer callback_data)
{
	char *text;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (image_table));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (item));

	text = nautilus_labeled_image_get_text (NAUTILUS_LABELED_IMAGE (item));

//	g_print ("%s(%s)\n", __FUNCTION__, text);
}

static void
image_table_child_leave_callback (GtkWidget *image_table,
			       GtkWidget *item,
			       gpointer callback_data)
{
	char *text;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (image_table));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (item));

	text = nautilus_labeled_image_get_text (NAUTILUS_LABELED_IMAGE (item));

//	g_print ("%s(%s)\n", __FUNCTION__, text);
}

static void
image_table_child_pressed_callback (GtkWidget *image_table,
			       GtkWidget *item,
			       gpointer callback_data)
{
	char *text;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (image_table));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (item));

	text = nautilus_labeled_image_get_text (NAUTILUS_LABELED_IMAGE (item));

	g_print ("%s(%s)\n", __FUNCTION__, text);
}

static void
image_table_child_released_callback (GtkWidget *image_table,
				  GtkWidget *item,
				  gpointer callback_data)
{
	char *text;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (image_table));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (item));

	text = nautilus_labeled_image_get_text (NAUTILUS_LABELED_IMAGE (item));

	g_print ("%s(%s)\n", __FUNCTION__, text);
}

static void
image_table_child_clicked_callback (GtkWidget *image_table,
				 GtkWidget *item,
				 gpointer callback_data)
{
	char *text;

	g_return_if_fail (NAUTILUS_IS_IMAGE_TABLE (image_table));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (item));

	text = nautilus_labeled_image_get_text (NAUTILUS_LABELED_IMAGE (item));

	g_print ("%s(%s)\n", __FUNCTION__, text);
}

static GtkWidget *
image_table_new_scrolled (void)
{
	GtkWidget *scrolled;
	GtkWidget *viewport;
	GtkWidget *window;
	GtkWidget *image_table;
	int i;

	window = test_window_new ("Image Table Test", 10);

	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

	/* Scrolled window */
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (window), scrolled);

	/* Viewport */
 	viewport = nautilus_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (scrolled), viewport);

	image_table = nautilus_image_table_new (FALSE);
	nautilus_wrap_table_set_x_justification (NAUTILUS_WRAP_TABLE (image_table),
						 NAUTILUS_JUSTIFICATION_MIDDLE);
	nautilus_wrap_table_set_y_justification (NAUTILUS_WRAP_TABLE (image_table),
						 NAUTILUS_JUSTIFICATION_END);

	gtk_container_add (GTK_CONTAINER (viewport), image_table);

	gtk_signal_connect (GTK_OBJECT (image_table),
			    "child_enter",
			    GTK_SIGNAL_FUNC (image_table_child_enter_callback),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (image_table),
			    "child_leave",
			    GTK_SIGNAL_FUNC (image_table_child_leave_callback),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (image_table),
			    "child_pressed",
			    GTK_SIGNAL_FUNC (image_table_child_pressed_callback),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (image_table),
			    "child_released",
			    GTK_SIGNAL_FUNC (image_table_child_released_callback),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (image_table),
			    "child_clicked",
			    GTK_SIGNAL_FUNC (image_table_child_clicked_callback),
			    NULL);

	//test_gtk_widget_set_background_color (viewport, "white");
	nautilus_gtk_widget_set_background_color (viewport, "white:red");

	for (i = 0; i < 100; i++) {
		char *text;
		GtkWidget *image;

		text = g_strdup_printf ("%s %d",
					names[random () % NAUTILUS_N_ELEMENTS (names)],
					i);
		image = labeled_image_new (text, pixbuf_name);
		g_free (text);

		gtk_container_add (GTK_CONTAINER (image_table), image);
		gtk_widget_show (image);
	}

	gtk_widget_show (viewport);
	gtk_widget_show (scrolled);
	gtk_widget_show (image_table);
	
	return window;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window = NULL;

	test_init (&argc, &argv);

	window = image_table_new_scrolled ();
	
	gtk_widget_show (window);
	
	gtk_main ();
	
	return 0;
}
