include $(top_srcdir)/Makefile.shared

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES=nautilus.desktop.in nautilus-help.desktop.in
DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)

SUBDIRS =			\
	libnautilus 		\
	cut-n-paste-code 	\
	libnautilus-private 	\
	libnautilus-adapter	\
	src 			\
	components		\
	po 			\
	data 			\
	icons 			\
	user-guide		\
	help			\
	omf-install		\
	$(NULL)

XXX = \
	test 			\
	$(NULL)

EXTRA_DIST=					\
	COPYING.LIB				\
	COPYING-DOCS				\
	HACKING					\
	MAINTAINERS				\
	nautilus.spec.in			\
	nautilus.desktop			\
	nautilus.desktop.in			\
	nautilus-help.desktop			\
	nautilus-help.desktop.in		\
	nautilus-launch-icon.png		\
	gnome-starthere-96.png			\
	gnome-starthere.png			\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	add-include-prefix			\
	$(DESKTOP_IN_FILES)			\
	$(DESKTOP_FILES)			\
	$(NULL)

appicondir = $(datadir)/pixmaps
appicon_DATA = 	nautilus-launch-icon.png 	\
		gnome-starthere-96.png		\
		gnome-starthere.png

Applicationsdir = $(datadir)/gnome/apps/Applications
Applications_DATA = $(DESKTOP_FILES)

Toplevelmenudir = $(datadir)/gnome/apps
Toplevelmenu_DATA = nautilus-help.desktop

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)
