SUBDIRS = 
NULL =

bin_PROGRAMS = eazel-proxy-util

# idl_sources are generated files
#eazel_proxy_util_idl_sources = \
#	eazelproxy-common.c	\
#	eazelproxy-stubs.c	\
#	eazelproxy.h		\
#	$(NULL)

eazel_proxy_util_SOURCES = \
	eazel-proxy-util.c		\
	$(NULL)

#	$(eazel_proxy_util_idl_sources)  \


eazel_proxy_util_LDADD = \
	$(top_srcdir)/components/services/trilobite/libammonite-gtk/libammonite-gtk.a \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(AMMONITE_LIBS) 				\
	$(GLIB_LIBS)					\
	$(ORBIT_LIBS)					\
	$(OAF_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)/components/services/trilobite/libammonite-gtk \
	$(GLIB_CFLAGS)	\
	$(GNOMEUI_CFLAGS) \
	$(OAF_CFLAGS)   \
	$(WERROR)	\
	$(AMMONITE_CFLAGS) \
	$(NULL)



# Files that depend on the corba-generated files
# $(eazel_proxy_util_SOURCES): $(eazel_proxy_util_idl_sources)

#
# For gnome-vfs
#

ammonite-methods.conf: ammonite-methods.conf.sub
	sed -e "s|%bindir%|$(bindir)|" <$< >$@

vfs_confdir = $(sysconfdir)/vfs/modules
vfs_conf_DATA = ammonite-methods.conf


EXTRA_DIST=\
	$(vfs_conf_DATA) \
	ammonite-methods.conf.sub \
	$(NULL)
