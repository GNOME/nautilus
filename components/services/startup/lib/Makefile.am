NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

noinst_LIBRARIES = libstartup.a

libstartup_a_SOURCES =		        	\
	eazel-register.h	 		\
	eazel-register.c	 		\
	$(NULL)

