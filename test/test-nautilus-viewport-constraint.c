#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-viewport.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>

GtkWidget *service_name;
GtkWidget *service_description;
GtkWidget *services_row;

void widget_set_nautilus_background_color (GtkWidget *widget, const char *color);

static void
delete_event (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

void
widget_set_nautilus_background_color (GtkWidget *widget, const char *color)
{
	NautilusBackground      *background;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (color != NULL);

	background = nautilus_get_widget_background (widget);

	nautilus_background_reset (background);
	nautilus_background_set_color (background, color);
}

static GtkWidget *
summary_view_button_new (char *label_text)
{
	GtkWidget *button;
	GtkWidget *label;

	button = gtk_button_new ();
	gtk_widget_set_usize (button, 80, -1);

	label = gtk_label_new (label_text);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (button), label);

	return button;
}

static GtkWidget *
create_nautilus_label (const char *text,
		       guint drop_shadow_offset,
		       float xalign,
		       float yalign,
		       gint xpadding,
		       gint ypadding,
		       guint32 text_color,
		       guint32 background_color,
		       const char *tile_name,
		       gint num_larger_sizes,
		       gboolean bold)
{
	GtkWidget *label;

	label = nautilus_label_new_solid (text,
					  drop_shadow_offset,
					  NAUTILUS_RGB_COLOR_WHITE,
					  NAUTILUS_RGB_COLOR_BLACK,
					  xalign, yalign, xpadding, ypadding,
					  background_color, NULL);

	if (num_larger_sizes < 0) {
		nautilus_label_make_smaller (NAUTILUS_LABEL (label), ABS (num_larger_sizes));
	}
	if (bold) {
		nautilus_label_make_bold (NAUTILUS_LABEL (label));
	}

	return label;
}

static GtkWidget *
summary_view_item_label_new (char *label_text,
			     int relative_font_size,
			     gboolean bold)
{
	GtkWidget *label;

	label = create_nautilus_label (label_text,
				       0, 0.5, 0.5, 0, 0,
				       NAUTILUS_RGB_COLOR_BLACK,
				       NAUTILUS_RGB_COLOR_WHITE,
				       NULL,
				       relative_font_size,
				       bold);

	nautilus_label_set_wrap (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	nautilus_label_set_adjust_wrap_on_resize (NAUTILUS_LABEL (label), TRUE);

	return label;
}

static GtkWidget *
summary_view_item_body_label_new (char *label_text)
{
	return summary_view_item_label_new (label_text,
					    -2,
					    FALSE);
}

static GtkWidget *
summary_view_item_header_label_new (char *label_text)
{
	return summary_view_item_label_new (label_text,
					    0,
					    TRUE);
}	

int
main (int argc, char *argv[])
{
	GtkWidget *form_vbox;
	GtkWidget *pane;
	GtkWidget *summary_pane;
	GtkWidget *viewport;
	GtkWidget *nautilus_window;

	GtkWidget *icon_box;
	GtkWidget *description_vbox;
	GtkWidget *button_vbox;
	GtkWidget *button_hbox;
	GtkWidget *button;

	gtk_init (&argc, &argv);

	nautilus_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy (GTK_WINDOW (nautilus_window), TRUE, TRUE, FALSE);

	gtk_signal_connect (GTK_OBJECT (nautilus_window), "delete_event", delete_event, NULL);

	form_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (nautilus_window), form_vbox);
	gtk_widget_show (form_vbox);
	
	pane = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (pane);

	viewport = nautilus_viewport_new (NULL, NULL);
	nautilus_viewport_set_constrain_width (NAUTILUS_VIEWPORT (viewport), TRUE);
	widget_set_nautilus_background_color (viewport, "rgb:FFFF/FFFF/FFFF");
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (pane), viewport);
	gtk_widget_show (viewport);

	summary_pane = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (viewport), summary_pane);
	gtk_widget_show (summary_pane);
	
	gtk_box_pack_start (GTK_BOX (form_vbox), pane, TRUE, TRUE, 0);

	services_row = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (summary_pane), services_row, FALSE, FALSE, 0);
	gtk_widget_show (services_row);

	/* Generate first box with service icon */
	icon_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (services_row), icon_box, FALSE, FALSE, 2);
	gtk_widget_show (icon_box);

	/* Generate second box with service title and summary */
	description_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (services_row), description_vbox, TRUE, TRUE, 0);
	gtk_widget_show (description_vbox);

	/* Header */
	service_name = summary_view_item_header_label_new ("Eazel Online Storage");
	gtk_box_pack_start (GTK_BOX (description_vbox), service_name, FALSE, FALSE, 2);
	gtk_widget_show (service_name);

	/* Body */
	service_description = summary_view_item_body_label_new ("Store files online and access them through Nautilus or from any browser.");
	gtk_box_pack_start (GTK_BOX (description_vbox), service_description, FALSE, FALSE, 2);
	gtk_widget_show (service_description);

	/* Add the redirect button to the third box */
	button_vbox = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_end (GTK_BOX (services_row), button_vbox, FALSE, FALSE, 2);
	gtk_widget_show (button_vbox);

	button_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_vbox), button_hbox, FALSE, FALSE, 2);
	gtk_widget_show (button_hbox);

	button = summary_view_button_new ("Go There");
	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (button_hbox), button, FALSE, FALSE, 3);

	gtk_widget_show (nautilus_window);

	gtk_main ();

	return 0;
}
