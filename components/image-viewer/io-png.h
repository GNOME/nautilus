#ifndef IO_PNG_H
#define IO_PNG_H

#include <bonobo/Bonobo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

void image_save (Bonobo_Stream      stream,
		 GdkPixbuf         *pixbuf,
		 CORBA_Environment *ev);

#endif /* IO_PNG_H */
