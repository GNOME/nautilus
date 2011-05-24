NULL=

xml_in_files = nautilus.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	nautilus.desktop.in				\
	nautilus-autorun-software.desktop.in

autostartdir     = $(sysconfdir)/xdg/autostart
autostart_in_files = nautilus-autostart.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Nautilus.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.gnome.Nautilus.service: org.gnome.Nautilus.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

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
	dbus-interfaces.xml		\
	$(xml_in_files) 		\
	$(desktop_in_files)		\
	$(service_in_files)		\
	$(autostart_in_files)		\
	$(NULL)

CLEANFILES = $(xml_files)	\
	$(desktop_DATA)		\
	$(service_DATA)		\
	$(autostart_DATA)	\
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
