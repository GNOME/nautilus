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
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	add-include-prefix			\
	$(desktop_in_files)			\
	$(NULL)

nautilus_connect_server_SOURCES= \
	nautilus-connect-server-dialog.c	\
	nautilus-connect-server-dialog.h	\
	nautilus-connect-server-dialog-main.c	\
	nautilus-connect-server-dialog-main.h	\
	nautilus-location-entry.c		\
	nautilus-location-entry.h		\
	$(NULL)

nautilus_connect_server_CFLAGS= \
	-DNAUTILUS_CONNECT_SERVER_PROGRAM	\
	$(NULL)


CLEANFILES = $(desktop_DATA)

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update
