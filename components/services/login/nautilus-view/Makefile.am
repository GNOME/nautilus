NULL =

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"					\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-login\"		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/nautilus-dependant-shared	\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/login/lib			\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	$(VFS_CFLAGS) 							\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =								\
	nautilus-login-view.oafinfo


bin_PROGRAMS =								\
	nautilus-login-view

nautilus_login_view_SOURCES =						\
	nautilus-login-view.h						\
	nautilus-login-view.c		    				\
	main.c								\
	$(NULL)

nautilus_login_view_LDADD =						\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/nautilus-dependant-shared/libnautilusdepshared.a \
	$(top_builddir)/libnautilus/libnautilus.la			\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(GHTTP_LIBS)							\
	$(OAF_LIBS)							\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
