include $(top_srcdir)/Makefile.shared

@INTLTOOL_DESKTOP_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	nautilus.desktop.in				\
	nautilus-home.desktop.in			\
	nautilus-computer.desktop.in			\
	nautilus-file-management-properties.desktop.in

SUBDIRS =			\
	libnautilus 		\
	libnautilus-extension	\
	cut-n-paste-code 	\
	libbackground		\
	libnautilus-private 	\
	libnautilus-adapter	\
	src 			\
	test 			\
	components		\
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
	nautilus.spec.in			\
	nautilus.desktop			\
	nautilus.desktop.in			\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	add-include-prefix			\
	$(desktop_in_files)			\
	$(NULL)

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)
