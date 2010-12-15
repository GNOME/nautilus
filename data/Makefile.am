NULL=

xml_in_files = nautilus.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	nautilus.desktop.in				\
	nautilus-home.desktop.in			\
	nautilus-computer.desktop.in			\
	nautilus-folder-handler.desktop.in		\
	nautilus-browser.desktop.in                     \
	nautilus-autorun-software.desktop.in
@INTLTOOL_DESKTOP_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

nautilusdatadir = $(datadir)/nautilus

nautilusdata_DATA = \
	nautilus-extras.placeholder 	\
	nautilus-suggested.placeholder 	\
	nautilus.css			\
	$(NULL)

EXTRA_DIST = $(nautilusdata_DATA) 	\
	nautilus.desktop		\
	nautilus.desktop.in		\
	nautilus.css			\
	$(xml_in_files) 		\
	$(desktop_in_files)		\
	$(NULL)

CLEANFILES = $(xml_files)	\
	$(desktop_DATA)		\
	$(NULL)

SUBDIRS = \
	icons    \
	$(NULL)

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif
