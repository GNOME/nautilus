#ifndef _IO_PNG_H
#define _IO_PNG_H

void image_save (Bonobo_Stream stream, GdkPixbuf *pixbuf,
		 CORBA_Environment *ev);

#endif /* _IO_PNG_H */
