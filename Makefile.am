include $(top_srcdir)/Makefile.shared

@INTLTOOL_DESKTOP_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	nautilus.desktop.in				\
	nautilus-home.desktop.in			\
	nautilus-computer.desktop.in			\
	nautilus-folder-handler.desktop.in		\
	nautilus-file-management-properties.desktop.in  \
	nautilus-autorun-software.desktop.in

SUBDIRS =			\
	libnautilus-extension	\
	cut-n-paste-code 	\
	libbackground		\
	libnautilus-private 	\
	src 			\
	test 			\
	po 			\
	data 			\
	icons 			\
	docs			\
	$(NULL)

EXTRA_DIST=					\
	COPYING.LIB				\
	COPYING-DOCS				\
	HACKING					\
	MAINTAINERS				\
	nautilus.spec				\
	nautilus.spec.in			\
	nautilus.desktop			\
	nautilus.desktop.in			\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	ChangeLog-20010201			\
	ChangeLog-20010420			\
	ChangeLog-20041029			\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	add-include-prefix			\
	$(desktop_in_files)			\
	$(NULL)

CLEANFILES = $(desktop_DATA)

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb --enable-gtk-doc
