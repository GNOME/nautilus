include $(top_srcdir)/Makefile.shared

SUBDIRS = icons

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-summary\"			\
	-DDATADIR=\"$(datadir)\"					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/nautilus-dependent-shared	\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/summary/lib			\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	$(VFS_CFLAGS) 							\
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-summary-view-ui.xml

bin_PROGRAMS =								\
	nautilus-summary-view

nautilus_summary_view_SOURCES =						\
	nautilus-summary-view-private.h					\
	nautilus-summary-callbacks.h					\
	nautilus-summary-callbacks.c					\
	nautilus-summary-menu-items.h					\
	nautilus-summary-menu-items.c					\
	nautilus-summary-dialogs.h					\
	nautilus-summary-dialogs.c					\
	nautilus-summary-footer.h					\
	nautilus-summary-footer.c					\
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

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_services-summary.oaf.in	\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(ui_DATA) 
