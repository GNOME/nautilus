NULL =

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-change-password\"		\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/nautilus-dependent-shared	\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/components/services/trilobite 		\
        -I$(top_builddir)/components/services/trilobite/libtrilobite 	\
	-I$(top_srcdir)/components/services/login/lib			\
	-I$(top_builddir)						\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	$(VFS_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(NULL)

bin_PROGRAMS =								\
	nautilus-change-password-view

nautilus_change_password_view_SOURCES =					\
	nautilus-change-password-view.h					\
	nautilus-change-password-view.c		    			\
	password-box.h							\
	password-box.c							\
	main.c								\
	$(NULL)

nautilus_change_password_view_LDADD =						\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/nautilus-dependent-shared/libnautilusdepshared.a \
	$(top_builddir)/libnautilus/libnautilus.la			\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(GHTTP_LIBS)							\
	$(OAF_LIBS)							\
	$(AMMONITE_LIBS)						\
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files = Nautilus_View_change-password.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = \
	$(oaf_DATA) \
	$(oaf_in_files) \
	$(NULL)
