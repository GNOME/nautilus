
#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-graphic.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>

static GdkPixbuf*
create_background (void)
{
	GdkPixbuf *background;
		
	background = gdk_pixbuf_new_from_file ("/gnome/share/nautilus/backgrounds/pale_coins.png");

	g_assert (background != NULL);

	return background;
}

static GdkPixbuf*
create_pixbuf (const char *name)
{
	char		*icon_path;
	GdkPixbuf	*pixbuf = NULL;

	g_assert (name != NULL);
	
	icon_path = nautilus_pixmap_file (name);
	g_assert (icon_path != NULL);

	pixbuf = gdk_pixbuf_new_from_file (icon_path);

	g_assert (pixbuf != NULL);

	g_free (icon_path);

	return pixbuf;
}

static GtkWidget*
create_graphic (const char *name, GdkPixbuf *background)
{
	GtkWidget	*graphic;
	GdkPixbuf	*pixbuf;

	g_assert (background != NULL);

	graphic = nautilus_graphic_new ();
	g_assert (graphic != NULL);

	nautilus_graphic_set_background_type (NAUTILUS_GRAPHIC (graphic), NAUTILUS_GRAPHIC_BACKGROUND_PIXBUF);
	nautilus_graphic_set_background_pixbuf (NAUTILUS_GRAPHIC (graphic), background);

	if (name != NULL)
	{
		pixbuf = create_pixbuf (name);
		g_assert (pixbuf != NULL);
		
		nautilus_graphic_set_pixbuf (NAUTILUS_GRAPHIC (graphic), pixbuf);
		
		gdk_pixbuf_unref (pixbuf);
	}

	return graphic;
}

static void
alpha_scale_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *graphic_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	graphic_list = (GList *) client_data;

	while (graphic_list)
	{
		g_assert (graphic_list->data != NULL);
		g_assert (NAUTILUS_IS_GRAPHIC (graphic_list->data));

		nautilus_graphic_set_overall_alpha (NAUTILUS_GRAPHIC (graphic_list->data), (guchar) adjustment->value);

		graphic_list = graphic_list->next;
	}
}

static void
red_color_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *graphic_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	graphic_list = (GList *) client_data;

	while (graphic_list)
	{
		NautilusGraphic	 *graphic;
		guint32	 color;

		g_assert (graphic_list->data != NULL);
		g_assert (NAUTILUS_IS_GRAPHIC (graphic_list->data));

		graphic = NAUTILUS_GRAPHIC (graphic_list->data);

		color = nautilus_graphic_get_background_color (graphic);

		color = NAUTILUS_RGBA_COLOR_PACK ((guchar) adjustment->value,
						  NAUTILUS_RGBA_COLOR_GET_G (color),
						  NAUTILUS_RGBA_COLOR_GET_B (color),
						  NAUTILUS_RGBA_COLOR_GET_A (color));
		
		nautilus_graphic_set_background_color (graphic, color);

		graphic_list = graphic_list->next;
	}
}

static void
green_color_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *graphic_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	graphic_list = (GList *) client_data;

	while (graphic_list)
	{
		NautilusGraphic	 *graphic;
		guint32	 color;

		g_assert (graphic_list->data != NULL);
		g_assert (NAUTILUS_IS_GRAPHIC (graphic_list->data));

		graphic = NAUTILUS_GRAPHIC (graphic_list->data);

		color = nautilus_graphic_get_background_color (graphic);

		color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
						  (guchar) adjustment->value,
						  NAUTILUS_RGBA_COLOR_GET_B (color),
						  NAUTILUS_RGBA_COLOR_GET_A (color));
		
		nautilus_graphic_set_background_color (graphic, color);

		graphic_list = graphic_list->next;
	}
}

static void
blue_color_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *graphic_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	graphic_list = (GList *) client_data;

	while (graphic_list)
	{
		NautilusGraphic	 *graphic;
		guint32	 color;

		g_assert (graphic_list->data != NULL);
		g_assert (NAUTILUS_IS_GRAPHIC (graphic_list->data));

		graphic = NAUTILUS_GRAPHIC (graphic_list->data);

		color = nautilus_graphic_get_background_color (graphic);

		color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
						  NAUTILUS_RGBA_COLOR_GET_G (color),
						  (guchar) adjustment->value,
						  NAUTILUS_RGBA_COLOR_GET_A (color));
		
		nautilus_graphic_set_background_color (graphic, color);

		graphic_list = graphic_list->next;
	}
}

static void
toggle_background_type_callback (GtkWidget *widget, gpointer client_data)
{
	NautilusGraphic *graphic;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_return_if_fail (client_data != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (client_data));

	graphic = NAUTILUS_GRAPHIC (client_data);

	if (nautilus_graphic_get_background_type (graphic) == NAUTILUS_GRAPHIC_BACKGROUND_PIXBUF)
	{
		nautilus_graphic_set_background_type (graphic, NAUTILUS_GRAPHIC_BACKGROUND_SOLID);
	}
	else
	{
		nautilus_graphic_set_background_type (graphic, NAUTILUS_GRAPHIC_BACKGROUND_PIXBUF);
	}
}

static GtkWidget*
create_color_scale (guint num_colors, GtkSignalFunc callback, gpointer callback_data)
{
	GtkAdjustment	*adjustment;
	GtkWidget	*scale;

	g_assert (num_colors > 0);
	g_assert (callback > 0);

	adjustment = (GtkAdjustment *) gtk_adjustment_new (num_colors,
							   0,
							   num_colors,
							   1,
							   num_colors / 10,
							   0);
	
	scale = gtk_hscale_new (adjustment);

	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed", callback, callback_data);

	gtk_widget_set_usize (scale, 150, 0);

	return scale;
}

int 
main (int argc, char* argv[])
{
	GtkWidget	*window;
	GtkWidget	*main_box;
	GtkWidget	*graphic_box;
	GtkWidget	*tool_box;
	GtkWidget	*toggle_background_type;

	GtkWidget	*alpha_scale;

	GtkWidget	*red_scale;
	GtkWidget	*green_scale;
	GtkWidget	*blue_scale;

	GdkPixbuf	*background;

	GtkWidget	*graphic1;
	GtkWidget	*graphic2;
	GtkWidget	*graphic3;

	GtkWidget	*background_graphic;

	GList		*graphic_list = NULL;

	const char	*file_name1 = "eazel-services-logo.png";
	const char	*file_name2 = "eazel-services-logo-tile.png";
	const char	*file_name3 = "eazel-services-logo-tile.png";

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "Graphic Test");
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_box);

	background = create_background ();

	graphic1 = create_graphic (file_name1, background);
	graphic2 = create_graphic (file_name2, background);
	graphic3 = create_graphic (file_name3, background);
	background_graphic = create_graphic (NULL, background);

	graphic_list = g_list_append (graphic_list, graphic1);
	graphic_list = g_list_append (graphic_list, graphic2);
	graphic_list = g_list_append (graphic_list, graphic3);
	graphic_list = g_list_append (graphic_list, background_graphic);

	nautilus_graphic_set_placement_type (NAUTILUS_GRAPHIC (graphic2), NAUTILUS_GRAPHIC_PLACEMENT_TILE);
	nautilus_graphic_set_placement_type (NAUTILUS_GRAPHIC (graphic3), NAUTILUS_GRAPHIC_PLACEMENT_TILE);

	{
		GdkFont *font;

		font = nautilus_font_factory_get_font_by_family ("helvetica", 20);

		nautilus_graphic_set_label_text (NAUTILUS_GRAPHIC (graphic3), "Welcome Back, Arlo!");
		nautilus_graphic_set_label_font (NAUTILUS_GRAPHIC (graphic3), font);

		gdk_font_unref (font);
	}
	
	graphic_box = gtk_hbox_new (FALSE, 0);
	tool_box = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (main_box), graphic_box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), background_graphic, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (main_box), tool_box, FALSE, FALSE, 10);

	gtk_box_pack_start (GTK_BOX (graphic_box), graphic1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (graphic_box), graphic2, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (graphic_box), graphic3, FALSE, FALSE, 0);

	alpha_scale = create_color_scale (255, alpha_scale_value_changed, graphic_list);

	toggle_background_type = gtk_button_new_with_label ("Toggle Background Type");
	red_scale = create_color_scale (255, red_color_value_changed, graphic_list);
	green_scale = create_color_scale (255, green_color_value_changed, graphic_list);
	blue_scale = create_color_scale (255, blue_color_value_changed, graphic_list);
	
	gtk_box_pack_start (GTK_BOX (tool_box), alpha_scale, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), toggle_background_type, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), red_scale, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), green_scale, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), blue_scale, FALSE, FALSE, 5);

	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) graphic1);

	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) graphic2);
	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) graphic3);

	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) background_graphic);

	gtk_widget_show_all (window);

	gtk_main ();
	
	return 0;
}
