NULL =

SUBDIR = icons

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"					\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-login\"		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/login/lib			\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	$(WERROR)							\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =								\
	nautilus-login-view.oafinfo


bin_PROGRAMS =								\
	nautilus-login-view

nautilus_login_view_SOURCES =				\
	nautilus-login-view.h					\
	nautilus-login-view.c		    			\
	main.c								\
	$(NULL)

nautilus_login_view_LDFLAGS =				\
	$(top_builddir)/components/services/login/lib/liblogin_corba.a  \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/libnautilus/libnautilus.la			\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la  \
	$(top_builddir)/librsvg/librsvg.la				\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(GHTTP_LIBS)							\
	$(OAF_LIBS)							\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
