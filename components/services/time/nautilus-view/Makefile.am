NULL = 

CPPFLAGS =					    	    	\
	-DPREFIX=\"$(prefix)\"	    	    	    	    	\
	-DG_LOG_DOMAIN=\"trilobite-eazel-time-service\"     	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"    	    	\
	${NULL}

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite \
	-I$(top_builddir)/components/services/trilobite/libtrilobite \
	-I$(top_srcdir)/components/services/trilobite \
	-I$(top_builddir)/components/services/trilobite \
	$(GCONF_CFLAGS)     	    	    	\
	$(GNOMEUI_CFLAGS)   	    	    	\
	$(GNOME_CFLAGS) 	    	    	\
	$(GNOME_INCLUDEDIR) 	    	    	\
	$(OAF_CFLAGS) 	    	    	    	\
	$(XML_CFLAGS) 	    	    	    	\
	$(NULL)


IDLDIR = $(top_srcdir)/components/services/time/idl
IDL = $(IDLDIR)/trilobite-eazel-time-service.idl

CORBA_GENERATED = \
	trilobite-eazel-time-service.h 		\
	trilobite-eazel-time-service-skels.c	\
	trilobite-eazel-time-service-stubs.c	\
	trilobite-eazel-time-service-common.c	\
	$(NULL)

# make the auto-generated files depend on a timestamp file.
# this is to ensure the individual files wait for each other before
# proceeding which is needed in parallel makes

$(CORBA_GENERATED): idl_stamp

idl_stamp: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(IDL)
	touch idl_stamp

main.c trilobite-eazel-time-view.c: $(CORBA_GENERATED)

CLEANFILES+=$(CORBA_GENERATED) idl_stamp



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

trilobite_eazel_time_view_LDADD =			\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(OAF_LIBS)	

EXTRA_DIST = $(oaf_DATA)
