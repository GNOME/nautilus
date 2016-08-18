include $(top_srcdir)/Makefile.shared

DIRS =				\
	eel			\
	libnautilus-extension	\
	libgd                   \
	src 			\
	test 			\
	data 			\
	po 			\
	docs			\
	$(NULL)

if ENABLE_DESKTOP
DIRS += nautilus-desktop
endif

DIST_SUBDIRS = $(DIRS) nautilus-sendto-extension

SUBDIRS = $(DIRS)
if ENABLE_NST_EXTENSION
SUBDIRS += nautilus-sendto-extension
endif

EXTRA_DIST=					\
	COPYING.LIB				\
	COPYING.EXTENSIONS			\
	HACKING					\
	MAINTAINERS				\
	README.commits				\
	Makefile.shared				\
	$(NULL)

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb --enable-gtk-doc

ACLOCAL_AMFLAGS = -I m4 -I libgd ${ACLOCAL_FLAGS}

-include $(top_srcdir)/git.mk
