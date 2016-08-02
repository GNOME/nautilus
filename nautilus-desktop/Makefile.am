include $(top_srcdir)/Makefile.shared

bin_PROGRAMS=					\
	nautilus-desktop			\
	$(NULL)

AM_CPPFLAGS =							\
	-I$(top_srcdir) 					\
	-I$(top_srcdir)/libgd                                   \
	-I$(top_srcdir)/src                                     \
	-I$(top_builddir)/src                                     \
	$(BASE_CFLAGS) 						\
	$(COMMON_CFLAGS)					\
	$(NAUTILUS_CFLAGS)					\
	$(WARNING_CFLAGS)					\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLOCALEDIR=\""$(localedir)"\"				\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 		\
	-DPREFIX=\""$(prefix)"\" 				\
	-DVERSION="\"$(VERSION)\"" 				\
	$(DISABLE_DEPRECATED) 		                        \
	$(NULL)

LDADD =\
	$(top_builddir)/src/libnautilus.la \
	$(NULL)

nautilus_desktop_SOURCES= 				\
	main-desktop.c					\
	nautilus-desktop-application.c			\
	nautilus-desktop-application.h			\
	nautilus-desktop-canvas-view.c		\
	nautilus-desktop-canvas-view.h		\
	nautilus-desktop-canvas-view-container.c	\
	nautilus-desktop-canvas-view-container.h	\
	nautilus-desktop-directory.c \
	nautilus-desktop-directory.h \
	nautilus-desktop-directory-file.c \
	nautilus-desktop-directory-file.h \
	nautilus-desktop-icon-file.c \
	nautilus-desktop-icon-file.h \
	nautilus-desktop-link.c \
	nautilus-desktop-link.h \
	nautilus-desktop-link-monitor.c \
	nautilus-desktop-link-monitor.h \
	nautilus-desktop-metadata.c \
	nautilus-desktop-metadata.h \
	nautilus-desktop-window.c		\
	nautilus-desktop-window.h		\
	nautilus-desktop-window-slot.c		\
	nautilus-desktop-window-slot.h		\
	$(NULL)

-include $(top_srcdir)/git.mk
