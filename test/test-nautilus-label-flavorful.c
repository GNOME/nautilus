
#include <config.h>

#include <gtk/gtk.h>

#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-gdk-font-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>

#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}

static char *
font_get_name (const GdkFont *font)
{
	GdkFontPrivate *font_private;
	const char *font_name;

	font_private = (GdkFontPrivate *)font;

	if (font_private->names == NULL) {
		return NULL;
	}

	font_name = g_slist_nth_data (font_private->names, 0);

	return font_name ? g_strdup (font_name) : NULL;
}

static void
label_set_label_to_font_name (GtkLabel *label)
{
	char *font_name;

	g_return_if_fail (GTK_IS_LABEL (label));

	font_name = font_get_name (GTK_WIDGET (label)->style->font);

	gtk_label_set_text (label, font_name);

	g_free (font_name);
}

static void
increasing_label_window_new (void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	guint i;

	const guint num_labels = 20;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Labels with increasing sizes");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	for (i = 0; i < num_labels; i++) {
		char *text;
		GtkWidget *label;

		if (i > 0) {
			text = g_strdup_printf ("A Label %d size(s) Larger", i);
		} else {
			text = g_strdup ("A standard Label");
		}
		
		label = nautilus_label_new (text);
		gtk_widget_ensure_style (label);

		gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

		if (i > 0) {
			nautilus_label_make_larger (NAUTILUS_LABEL (label), i);
		}
		
		gtk_widget_show (label);

		label_set_label_to_font_name (GTK_LABEL (label));

		g_free (text);
	}
	
	gtk_widget_show (vbox);
	gtk_widget_show (window);
}

static void
decreasing_label_window_new (void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	guint i;

	const guint num_labels = 6;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Labels with increasing sizes");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	for (i = 0; i < num_labels; i++) {
		char *text;
		GtkWidget *label;

// 		if (i > 0) {
// 			text = g_strdup_printf ("A Label %d size(s) Smaller", i);
// 		} else {
			text = g_strdup ("A standard Label");
// 		}
		
		label = nautilus_label_new (text);
		gtk_widget_ensure_style (label);

		gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
		
		if (i > 0) {
			nautilus_label_make_smaller (NAUTILUS_LABEL (label), i);
		}
		
		gtk_widget_show (label);
		
		g_free (text);
	}
	
	gtk_widget_show (vbox);
	gtk_widget_show (window);
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *bold_label;
	GtkWidget *large_label;
	GtkWidget *small_label;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Foo");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

 	label = nautilus_label_new ("This is a label");
 	bold_label = nautilus_label_new ("This is a label");
 	large_label = nautilus_label_new ("This is a label");
 	small_label = nautilus_label_new ("This is a label");
	
	gtk_widget_ensure_style (label);
	gtk_widget_ensure_style (bold_label);
	gtk_widget_ensure_style (large_label);
	gtk_widget_ensure_style (small_label);

	nautilus_gtk_label_make_bold (GTK_LABEL (bold_label));

	nautilus_gtk_label_make_larger (GTK_LABEL (large_label), 2);
	
	nautilus_gtk_label_make_smaller (GTK_LABEL (small_label), 4);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), bold_label, FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (vbox), large_label, FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (vbox), small_label, FALSE, FALSE, 20);

	gtk_widget_show_all (window);

	if (1) increasing_label_window_new ();
	if (1) decreasing_label_window_new ();
	
	gtk_main ();

	return 0;
}

