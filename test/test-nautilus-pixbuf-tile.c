#include "test.h"

#include <libnautilus-extensions/nautilus-labeled-image.h>


static const char pixbuf_name[] = "/usr/share/pixmaps/gnome-globe.png";
static const char tile_name[] = "/gnome/share/nautilus/patterns/camouflage.png";

static int
pixbuf_drawing_area_expose_event (GtkWidget *widget,
				  GdkEventExpose *event,
				  gpointer callback_data)
{
	static GdkPixbuf *tile = NULL;
	GdkPixbuf *buffer;
	ArtIRect dest;
	ArtIRect tile_area;

	buffer = nautilus_gdk_pixbuf_get_global_buffer (widget->allocation.width, widget->allocation.height);

	if (tile == NULL) {
		tile = gdk_pixbuf_new_from_file (tile_name);
		g_assert (tile != NULL);
	}

	tile_area.x0 = 0;
	tile_area.y0 = 0;
	tile_area.x1 = widget->allocation.width;
	tile_area.y1 = widget->allocation.height;

	nautilus_gdk_pixbuf_draw_to_pixbuf_tiled (tile,
						  buffer,
						  &tile_area,
						  gdk_pixbuf_get_width (tile),
						  gdk_pixbuf_get_height (tile),
						  0,
						  0,
						  NAUTILUS_OPACITY_FULLY_OPAQUE,
						  GDK_INTERP_NEAREST);

	dest = nautilus_irect_gtk_widget_get_bounds (widget);
	nautilus_gdk_pixbuf_draw_to_drawable (buffer,
					      widget->window,
					      widget->style->white_gc,
					      0,
					      0,
					      &dest,
					      GDK_RGB_DITHER_NONE,
					      GDK_PIXBUF_ALPHA_BILEVEL,
					      NAUTILUS_STANDARD_ALPHA_THRESHHOLD);

	nautilus_debug_draw_rectangle_and_cross (widget->window, &dest, 0xFF0000, TRUE);
	{
		ArtIRect one_tile;
		one_tile.x0 = widget->allocation.x;
		one_tile.y0 = widget->allocation.y;
		one_tile.x1 = gdk_pixbuf_get_width (tile);
		one_tile.y1 = gdk_pixbuf_get_height (tile);
		
		nautilus_debug_draw_rectangle_and_cross (widget->window, &one_tile, 0x0000FF, TRUE);
	}

	return TRUE;
}

static int
drawable_drawing_area_expose_event (GtkWidget *widget,
				    GdkEventExpose *event,
				    gpointer callback_data)
{
	static GdkPixbuf *tile = NULL;
	ArtIRect dest;

	if (tile == NULL) {
		tile = gdk_pixbuf_new_from_file (tile_name);
		g_assert (tile != NULL);
	}

	dest = nautilus_irect_gtk_widget_get_bounds (widget);
	nautilus_gdk_pixbuf_draw_to_drawable_tiled (tile,
						    widget->window,
						    widget->style->white_gc,
						    &dest,
						    gdk_pixbuf_get_width (tile),
						    gdk_pixbuf_get_height (tile),
						    0,
						    0,
						    GDK_RGB_DITHER_NONE,
						    GDK_PIXBUF_ALPHA_BILEVEL,
						    NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
	
	nautilus_debug_draw_rectangle_and_cross (widget->window, &dest, 0xFF0000, TRUE);
	{
		ArtIRect one_tile;
		one_tile.x0 = widget->allocation.x;
		one_tile.y0 = widget->allocation.y;
		one_tile.x1 = gdk_pixbuf_get_width (tile);
		one_tile.y1 = gdk_pixbuf_get_height (tile);
		
		nautilus_debug_draw_rectangle_and_cross (widget->window, &one_tile, 0x0000FF, TRUE);
	}

	return TRUE;
}

int 
main (int argc, char* argv[])
{
	GtkWidget *pixbuf_window;
	GtkWidget *pixbuf_drawing_area;
	GtkWidget *pixbuf_vbox;
	GtkWidget *drawable_window;
	GtkWidget *drawable_drawing_area;
	GtkWidget *drawable_vbox;
	
	test_init (&argc, &argv);

	pixbuf_window = test_window_new ("Pixbuf To Pixbuf Tile Test", 0);
	pixbuf_vbox = gtk_vbox_new (FALSE, 0);
	pixbuf_drawing_area = gtk_drawing_area_new ();
	gtk_signal_connect (GTK_OBJECT (pixbuf_drawing_area),
			    "expose_event",
			    GTK_SIGNAL_FUNC (pixbuf_drawing_area_expose_event),
			    NULL);
	gtk_box_pack_start (GTK_BOX (pixbuf_vbox), pixbuf_drawing_area, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (pixbuf_window), pixbuf_vbox);
	gtk_widget_show_all (pixbuf_window);


	drawable_window = test_window_new ("Pixbuf To Drawable Tile Test", 0);
	drawable_vbox = gtk_vbox_new (FALSE, 0);
	drawable_drawing_area = gtk_drawing_area_new ();
	gtk_signal_connect (GTK_OBJECT (drawable_drawing_area),
			    "expose_event",
			    GTK_SIGNAL_FUNC (drawable_drawing_area_expose_event),
			    NULL);
	gtk_box_pack_start (GTK_BOX (drawable_vbox), drawable_drawing_area, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (drawable_window), drawable_vbox);
	gtk_widget_show_all (drawable_window);

	gtk_main ();

	return 0;
}
