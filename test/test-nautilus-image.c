
#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-image.h>
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
create_image (const char *name, GdkPixbuf *background)
{
	GtkWidget	*image;
	GdkPixbuf	*pixbuf;

	g_assert (background != NULL);

	image = nautilus_image_new ();
	g_assert (image != NULL);

	nautilus_image_set_background_type (NAUTILUS_IMAGE (image), NAUTILUS_IMAGE_BACKGROUND_PIXBUF);
	nautilus_image_set_background_pixbuf (NAUTILUS_IMAGE (image), background);

	if (name != NULL)
	{
		pixbuf = create_pixbuf (name);
		g_assert (pixbuf != NULL);
		
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);
		
		gdk_pixbuf_unref (pixbuf);
	}

	return image;
}

static void
alpha_scale_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *image_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	image_list = (GList *) client_data;

	while (image_list)
	{
		g_assert (image_list->data != NULL);
		g_assert (NAUTILUS_IS_IMAGE (image_list->data));

		nautilus_image_set_overall_alpha (NAUTILUS_IMAGE (image_list->data), (guchar) adjustment->value);

		image_list = image_list->next;
	}
}

static void
red_color_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *image_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	image_list = (GList *) client_data;

	while (image_list)
	{
		NautilusImage	 *image;
		guint32	 color;

		g_assert (image_list->data != NULL);
		g_assert (NAUTILUS_IS_IMAGE (image_list->data));

		image = NAUTILUS_IMAGE (image_list->data);

		color = nautilus_image_get_background_color (image);

		color = NAUTILUS_RGBA_COLOR_PACK ((guchar) adjustment->value,
						  NAUTILUS_RGBA_COLOR_GET_G (color),
						  NAUTILUS_RGBA_COLOR_GET_B (color),
						  NAUTILUS_RGBA_COLOR_GET_A (color));
		
		nautilus_image_set_background_color (image, color);

		image_list = image_list->next;
	}
}

static void
green_color_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *image_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	image_list = (GList *) client_data;

	while (image_list)
	{
		NautilusImage	 *image;
		guint32	 color;

		g_assert (image_list->data != NULL);
		g_assert (NAUTILUS_IS_IMAGE (image_list->data));

		image = NAUTILUS_IMAGE (image_list->data);

		color = nautilus_image_get_background_color (image);

		color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
						  (guchar) adjustment->value,
						  NAUTILUS_RGBA_COLOR_GET_B (color),
						  NAUTILUS_RGBA_COLOR_GET_A (color));
		
		nautilus_image_set_background_color (image, color);

		image_list = image_list->next;
	}
}

static void
blue_color_value_changed (GtkAdjustment *adjustment, gpointer client_data)
{
	GList *image_list;

	g_return_if_fail (adjustment != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (client_data != NULL);

	image_list = (GList *) client_data;

	while (image_list)
	{
		NautilusImage	 *image;
		guint32	 color;

		g_assert (image_list->data != NULL);
		g_assert (NAUTILUS_IS_IMAGE (image_list->data));

		image = NAUTILUS_IMAGE (image_list->data);

		color = nautilus_image_get_background_color (image);

		color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
						  NAUTILUS_RGBA_COLOR_GET_G (color),
						  (guchar) adjustment->value,
						  NAUTILUS_RGBA_COLOR_GET_A (color));
		
		nautilus_image_set_background_color (image, color);

		image_list = image_list->next;
	}
}

static void
toggle_background_type_callback (GtkWidget *widget, gpointer client_data)
{
	NautilusImage *image;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_return_if_fail (client_data != NULL);
	g_return_if_fail (NAUTILUS_IS_IMAGE (client_data));

	image = NAUTILUS_IMAGE (client_data);

	if (nautilus_image_get_background_type (image) == NAUTILUS_IMAGE_BACKGROUND_PIXBUF)
	{
		nautilus_image_set_background_type (image, NAUTILUS_IMAGE_BACKGROUND_SOLID);
	}
	else
	{
		nautilus_image_set_background_type (image, NAUTILUS_IMAGE_BACKGROUND_PIXBUF);
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
	GtkWidget	*image_box;
	GtkWidget	*tool_box;
	GtkWidget	*toggle_background_type;

	GtkWidget	*alpha_scale;

	GtkWidget	*red_scale;
	GtkWidget	*green_scale;
	GtkWidget	*blue_scale;

	GdkPixbuf	*background;

	GtkWidget	*image1;
	GtkWidget	*image2;
	GtkWidget	*image3;

	GtkWidget	*background_image;

	GList		*image_list = NULL;

	const char	*file_name1 = "eazel-services-logo.png";
	const char	*file_name2 = "eazel-services-logo-tile.png";
	const char	*file_name3 = "eazel-services-logo-tile.png";

	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "Image Test");
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_box);

	background = create_background ();

	image1 = create_image (file_name1, background);
	image2 = create_image (file_name2, background);
	image3 = create_image (file_name3, background);
	background_image = create_image (NULL, background);

	image_list = g_list_append (image_list, image1);
	image_list = g_list_append (image_list, image2);
	image_list = g_list_append (image_list, image3);
	image_list = g_list_append (image_list, background_image);

	nautilus_image_set_placement_type (NAUTILUS_IMAGE (image2), NAUTILUS_IMAGE_PLACEMENT_TILE);
	nautilus_image_set_placement_type (NAUTILUS_IMAGE (image3), NAUTILUS_IMAGE_PLACEMENT_TILE);

	{
		GdkFont *font;

		font = nautilus_font_factory_get_font_by_family ("helvetica", 20);

		nautilus_image_set_label_text (NAUTILUS_IMAGE (image3), "Welcome Back, Arlo!");
		nautilus_image_set_label_font (NAUTILUS_IMAGE (image3), font);

		gdk_font_unref (font);

		nautilus_image_set_extra_width (NAUTILUS_IMAGE (image3), 8);
		nautilus_image_set_right_offset (NAUTILUS_IMAGE (image3), 8);
		nautilus_image_set_top_offset (NAUTILUS_IMAGE (image3), 3);
	}
	
	image_box = gtk_hbox_new (FALSE, 0);
	tool_box = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (main_box), image_box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), background_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (main_box), tool_box, FALSE, FALSE, 10);

	gtk_box_pack_start (GTK_BOX (image_box), image1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (image_box), image2, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (image_box), image3, FALSE, FALSE, 0);

	alpha_scale = create_color_scale (255, alpha_scale_value_changed, image_list);

	toggle_background_type = gtk_button_new_with_label ("Toggle Background Type");
	red_scale = create_color_scale (255, red_color_value_changed, image_list);
	green_scale = create_color_scale (255, green_color_value_changed, image_list);
	blue_scale = create_color_scale (255, blue_color_value_changed, image_list);
	
	gtk_box_pack_start (GTK_BOX (tool_box), alpha_scale, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), toggle_background_type, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), red_scale, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), green_scale, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (tool_box), blue_scale, FALSE, FALSE, 5);

	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) image1);

	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) image2);
	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) image3);

	gtk_signal_connect (GTK_OBJECT (toggle_background_type), 
			    "clicked",
			    GTK_SIGNAL_FUNC (toggle_background_type_callback),
			    (gpointer) background_image);

	gtk_widget_show_all (window);

	gtk_main ();
	
	return 0;
}
