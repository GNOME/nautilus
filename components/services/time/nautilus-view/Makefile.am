NULL = 

CPPFLAGS =					    	    	\
	-DPREFIX=\"$(prefix)\"	    	    	    	    	\
	-DG_LOG_DOMAIN=\"trilobite-eazel-time-service\"     	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"    	    	\
    	${NULL}
	

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/components/services/trilobite \
	$(GCONF_CFLAGS)     	    	    	\
	$(GNOMEUI_CFLAGS)   	    	    	\
	$(GNOME_CFLAGS) 	    	    	\
	$(GNOME_INCLUDEDIR) 	    	    	\
	$(OAF_CFLAGS) 	    	    	    	\
	$(XML_CFLAGS) 	    	    	    	\
	$(WERROR)   	    	    	    	\
	$(NULL)


IDLDIR = $(top_srcdir)/components/services/time/idl
IDL = $(IDLDIR)/trilobite-eazel-time-service.idl

CORBA_GENERATED =   \
	time-service-stubs.c    \
	time-service-common.c   \
	time-service-skels.c    \
	$(NULL)

CORBA_HEADER = trilobite-eazel-time-service.h

$(CORBA_HEADER): $(IDL)
	$(prefix)/bin/orbit-idl -I${datadir}/idl $(IDL)

main.c trilobite-eazel-time-view.c $(CORBA_GENERATED): $(CORBA_HEADER)

CLEANFILES+=$(CORBA_GENERATED) $(CORBA_HEADER)



oafdir = $(datadir)/oaf

oaf_DATA =					\
	trilobite-eazel-time-view.oafinfo


bin_PROGRAMS =					\
	trilobite-eazel-time-view

trilobite_eazel_time_view_SOURCES =		\
	trilobite-eazel-time-view.h          \
	trilobite-eazel-time-view.c          \
	trilobite-eazel-time-service-common.c   \
	trilobite-eazel-time-service-stubs.c    \
	main.c	    	    	    	    	\
	${NULL}

trilobite_eazel_time_view_LDADD =	\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(NULL)


trilobite_eazel_time_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/librsvg/librsvg.la		\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(OAF_LIBS)	

EXTRA_DIST = $(oaf_DATA)
