/*
 * io-png.c by David Welton <davidw@linuxcare.com>.  Most of the code
 * borrowed from Imlib, Bonobo Stream API by Michael Meeks <mmeeks@gnu.org>
 */

#include <config.h>
#include "io-png.h"

#include <bonobo/bonobo-stream-client.h>
#include <png.h>

typedef struct {
	Bonobo_Stream      stream;
	CORBA_Environment *ev;
} BStreamData;

static void
png_write_data_fn (png_structp png_ptr, png_bytep data, png_size_t len)
{
	BStreamData *sd = png_get_io_ptr (png_ptr);

	if (sd->ev->_major != CORBA_NO_EXCEPTION)
		return;

	bonobo_stream_client_write (sd->stream, data, len, sd->ev);
}

static void
png_flush_fn (png_structp png_ptr)
{
	g_warning ("Flush nothing");
}

void
image_save (Bonobo_Stream stream, GdkPixbuf *pixbuf,
	    CORBA_Environment *ev)
{
	png_structp         png_ptr;
	png_infop           info_ptr;
	guint8              *ptr;
	int                 x, y, j;
	png_bytep           row_ptr;
	volatile png_bytep  data = NULL;
	png_color_8         sig_bit;
	int                 w, h, rowstride;
	int                 has_alpha;
	int                 bpc;
	BStreamData         sdata;

	bpc = gdk_pixbuf_get_bits_per_sample (pixbuf);
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);
	if (!png_ptr)
		goto png_err;

	sdata.stream = stream;
	sdata.ev     = ev;
	png_set_write_fn (png_ptr, &sdata, png_write_data_fn, png_flush_fn);

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr ||
	    setjmp (png_ptr->jmpbuf)) {
		png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
		goto png_err;
	}

	if (has_alpha) {
		png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
			      PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
#ifdef WORDS_BIGENDIAN
		png_set_swap_alpha (png_ptr);
#else
		png_set_bgr (png_ptr);
#endif
	} else {
		png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
			      PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		data = g_malloc (w * 3 * sizeof(char));
	}
	sig_bit.red = bpc;
	sig_bit.green = bpc;
	sig_bit.blue = bpc;
	sig_bit.alpha = bpc;
	png_set_sBIT    (png_ptr, info_ptr, &sig_bit);
	png_write_info  (png_ptr, info_ptr);
	png_set_shift   (png_ptr, &sig_bit);
	png_set_packing (png_ptr);

	ptr = gdk_pixbuf_get_pixels (pixbuf);
	for (y = 0; y < h; y++) {
		if (has_alpha)
			row_ptr = (png_bytep)ptr;
		else {
			for (j = 0, x = 0; x < w; x++)
				memcpy (&(data [x * 3]), &(ptr [x * 3]), 3);

			row_ptr = (png_bytep)data;
		}
		png_write_rows (png_ptr, &row_ptr, 1);
		ptr += rowstride;
	}
	g_free (data);
	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
	return;

 png_err:
	g_free (data);
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Stream_IOError, NULL);
	return;
}
