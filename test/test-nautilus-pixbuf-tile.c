
#include <config.h>

#include "test.h"

#if 0
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-string-picker.h>
#include <libnautilus-extensions/nautilus-font-picker.h>
#include <libnautilus-extensions/nautilus-text-caption.h>

#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-string.h>

#include <libnautilus-extensions/nautilus-debug-drawing.h>

#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath_stroke.h>
#include <libart_lgpl/art_rgb_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rgb.h>

#include <libgnomevfs/gnome-vfs-init.h>

/* Danger! Many Gremlins live here. */

/* FIXME bugzilla.eazel.com 5031: Need to account for word endianess in these macros */
#define ART_OPACITY_NONE 255
#define ART_OPACITY_FULL 0

/* Pack RGBA values */
#define ART_RGBA_COLOR_PACK(_r, _g, _b, _a)	\
( ((_a) << 0) |					\
  ((_r) << 24) |				\
  ((_g) << 16) |				\
  ((_b) <<  8) )

#define ART_RGB_COLOR_PACK(_r, _g, _b)			\
( (ART_OPACITY_NONE << 0) |					\
  ((_r) << 24) |				\
  ((_g) << 16) |				\
  ((_b) <<  8) )

/* Access the individual RGBA components */
#define ART_RGBA_GET_R(_color) (((_color) >> 24) & 0xff)
#define ART_RGBA_GET_G(_color) (((_color) >> 16) & 0xff)
#define ART_RGBA_GET_B(_color) (((_color) >>  8) & 0xff)
#define ART_RGBA_GET_A(_color) (((_color) >>  0) & 0xff)

#define RED		ART_RGB_COLOR_PACK (255, 0, 0)
#define GREEN		ART_RGB_COLOR_PACK (0, 255, 0)
#define BLUE		ART_RGB_COLOR_PACK (0, 0, 255)
#define WHITE		ART_RGB_COLOR_PACK (255, 255, 255)
#define BLACK		ART_RGB_COLOR_PACK (0, 0, 0)
#define TRANSPARENT	ART_RGBA_COLOR_PACK (255, 255, 255, 0)

static GdkPixbuf *
create_named_background (const char *name) 
{
	GdkPixbuf	*pixbuf;
	char		*path;
	
	g_return_val_if_fail (name != NULL, NULL);
	
	path = nautilus_make_path (NAUTILUS_DATADIR "/patterns", name);

	if (path == NULL) {
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	return pixbuf;
}

static void
rgba_run_alpha (art_u8 *buf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n)
{
  int i;
  int v;

  for (i = 0; i < n; i++)
    {
      v = *buf;
      *buf++ = v + (((r - v) * alpha + 0x80) >> 8);
      v = *buf;
      *buf++ = v + (((g - v) * alpha + 0x80) >> 8);
      v = *buf;
      *buf++ = v + (((b - v) * alpha + 0x80) >> 8);

      *buf++ = 255;
    }
}

typedef void (*RunFunc) (art_u8 *buf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n);

static void
pixbuf_draw_rectangle (GdkPixbuf	*pixbuf,
		       const ArtIRect	*rectangle,
		       guint32		color,
		       gboolean		filled)
{
	guchar		red;
	guchar		green;
	guchar		blue;
	guchar		alpha;

	guint		width;
	guint		height;
	guchar		*pixels;
	guint		rowstride;
 	int		y;
	gboolean	has_alpha;
	guint		pixel_offset;
	guchar		*offset;

	guint		rect_width;
	guint		rect_height;

	ArtIRect	draw_area;

	RunFunc		run_func;

	g_return_if_fail (pixbuf != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	pixel_offset = has_alpha ? 4 : 3;

	red = ART_RGBA_GET_R (color);
	green = ART_RGBA_GET_G (color);
	blue = ART_RGBA_GET_B (color);
	alpha = ART_RGBA_GET_A (color);

	run_func = has_alpha ? rgba_run_alpha : art_rgb_run_alpha;

	if (rectangle != NULL) {
		g_return_if_fail (rectangle->x1 >  rectangle->x0);
		g_return_if_fail (rectangle->y1 >  rectangle->y0);
		
		rect_width = rectangle->x1 - rectangle->x0;
		rect_height = rectangle->y1 - rectangle->y0;

		draw_area = *rectangle;
	}
	else {
		rect_width = width;
		rect_height = height;

		draw_area.x0 = 0;
		draw_area.y0 = 0;
		draw_area.x1 = width;
		draw_area.y1 = height;
	}

	if (filled) {
		offset = pixels + (draw_area.y0 * rowstride) + (draw_area.x0 * pixel_offset);

		for (y = draw_area.y0; y < draw_area.y1; y++) {
			(*run_func) (offset, red, green, blue, 255, rect_width);
			offset += rowstride;
		}
	}
	else {
		/* top */
		offset = pixels + (draw_area.y0 * rowstride) + (draw_area.x0 * pixel_offset);
		(*run_func) (offset, red, green, blue, 255, rect_width);
		
		/* bottom */
		offset += ((rect_height - 1) * rowstride);
		(*run_func) (offset, red, green, blue, 255, rect_width);
	
		for (y = draw_area.y0 + 1; y < (draw_area.y1 - 1); y++) {
			/* left */
			offset = pixels + (y * rowstride) + (draw_area.x0 * pixel_offset);
			(*run_func) (offset, red, green, blue, 255, 1);
			
			/* right */
			offset += (rect_width - 1) * pixel_offset;
			(*run_func) (offset, red, green, blue, 255, 1);
		}
	}
}

static void
pixbuf_draw_rectangle_around (GdkPixbuf	*pixbuf,
			      const ArtIRect	*rectangle,
			      guint32		color)
{
	ArtIRect area;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (rectangle != NULL);
	g_return_if_fail (rectangle->x1 >  rectangle->x0);
	g_return_if_fail (rectangle->y1 >  rectangle->y0);

	area = *rectangle;
	
	area.x0 -= 1;
	area.y0 -= 1;
	area.x1 += 1;
	area.y1 += 1;
	pixbuf_draw_rectangle (pixbuf, &area, color, FALSE);
	area.x0 += 1;
	area.y0 += 1;
	area.x1 -= 1;
	area.y1 -= 1;
}
#endif

int 
main (int argc, char* argv[])
{
	const guint pixbuf_width = 500;
	const guint pixbuf_height = 700;

	GdkPixbuf *pixbuf;
	GdkPixbuf *tile_pixbuf;
	ArtIRect tile_area;

	test_init (&argc, &argv);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, pixbuf_width, pixbuf_height);
	g_assert (pixbuf != NULL);

	tile_pixbuf = test_pixbuf_new_named ("patterns/pale_coins.png", 1.0);
	g_assert (tile_pixbuf != NULL);

	nautilus_art_irect_assign (&tile_area, 200, 50, 100, 100);

	nautilus_gdk_pixbuf_draw_to_pixbuf_tiled (tile_pixbuf,
						  pixbuf,
						  &tile_area,
						  gdk_pixbuf_get_width (tile_pixbuf),
						  gdk_pixbuf_get_height (tile_pixbuf),
						  0,
						  0,
						  NAUTILUS_OPACITY_FULLY_OPAQUE,
						  GDK_INTERP_BILINEAR);
	

	nautilus_art_irect_assign (&tile_area, 200, 150, 100, 100);

	nautilus_gdk_pixbuf_draw_to_pixbuf_tiled (tile_pixbuf,
						  pixbuf,
						  &tile_area,
						  gdk_pixbuf_get_width (tile_pixbuf),
						  gdk_pixbuf_get_height (tile_pixbuf),
						  0,
						  0,
						  NAUTILUS_OPACITY_FULLY_OPAQUE,
						  GDK_INTERP_BILINEAR);
	
	gdk_pixbuf_unref (tile_pixbuf);

	nautilus_debug_show_pixbuf_in_eog (pixbuf);

	gdk_pixbuf_unref (pixbuf);

	test_quit (0);

	return 0;
}
