include $(top_srcdir)/Makefile.shared

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES= 					\
	nautilus.desktop.in				\
	nautilus-home.desktop.in			\
	nautilus-computer.desktop.in			\
	nautilus-file-management-properties.desktop.in
DESKTOP_APPLICATIONS_FILES=		\
	nautilus.desktop		\
	nautilus-home.desktop		\
	nautilus-computer.desktop
DESKTOP_SETTINGS_FILES=				\
	nautilus-file-management-properties.desktop

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
	intltool-modules/XML/Parser/Style/OrigTree.pm \
	Makefile.shared				\
	add-include-prefix			\
	$(DESKTOP_IN_FILES)			\
	$(DESKTOP_APPLICATIONS_FILES)			\
	$(DESKTOP_SETTINGS_FILES)			\
	$(NULL)

Applicationsdir = $(datadir)/applications
Applications_DATA = $(DESKTOP_APPLICATIONS_FILES)

Settingsdir = $(datadir)/control-center-2.0/capplets
Settings_DATA = $(DESKTOP_SETTINGS_FILES)

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)
