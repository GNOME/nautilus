NULL=

desktopdir       = $(datadir)/applications
desktop_DATA     = 					\
	$(desktop_in_files:.desktop.in=.desktop)	\
	nautilus-classic.desktop
desktop_in_files = 					\
	org.gnome.Nautilus.desktop.in			\
	nautilus-autorun-software.desktop.in		\
	nautilus-connect-server.desktop.in

autostartdir     = $(sysconfdir)/xdg/autostart
autostart_in_files = nautilus-autostart.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

appdatadir = $(datadir)/appdata
appdata_in_files = org.gnome.Nautilus.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

@INTLTOOL_DESKTOP_RULE@

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.freedesktop.FileManager1.service.in	\
	org.gnome.Nautilus.service.in

org.gnome.Nautilus.service: org.gnome.Nautilus.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

org.freedesktop.FileManager1.service: org.freedesktop.FileManager1.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

searchproviderdir = $(datadir)/gnome-shell/search-providers
searchprovider_DATA = nautilus-search-provider.ini

EXTRA_DIST =				\
	org.gnome.Nautilus.desktop	\
	org.gnome.Nautilus.desktop.in	\
	dbus-interfaces.xml		\
	freedesktop-dbus-interfaces.xml	\
	shell-search-provider-dbus-interfaces.xml \
	$(desktop_in_files)		\
	$(service_in_files)		\
	$(autostart_in_files)		\
	$(searchprovider_DATA)		\
	$(appdata_in_files)		\
	$(NULL)

CLEANFILES =			\
	$(desktop_DATA)		\
	$(service_DATA)		\
	$(autostart_DATA)	\
	$(appdata_DATA)		\
	$(NULL)

-include $(top_srcdir)/git.mk
