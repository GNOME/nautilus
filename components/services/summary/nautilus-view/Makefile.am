NULL =

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"					\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-summary\"		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/nautilus-dependent-shared	\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/summary/lib			\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	$(VFS_CFLAGS) 							\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =								\
	nautilus-summary-view.oafinfo


bin_PROGRAMS =								\
	nautilus-summary-view

nautilus_summary_view_SOURCES =						\
	nautilus-summary-view.h						\
	nautilus-summary-view.c		    				\
	main.c								\
	$(NULL)

nautilus_summary_view_LDADD =						\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/nautilus-dependent-shared/libnautilusdepshared.a \
	$(top_builddir)/components/services/summary/lib/libsummary.a \
	$(top_builddir)/libnautilus/libnautilus.la			\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(GHTTP_LIBS)							\
	$(OAF_LIBS)							\
	$(AMMONITE_LIBS)						\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
