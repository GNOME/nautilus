NULL=

@XML_I18N_MERGE_DESKTOP_RULE@
@XML_I18N_MERGE_DIRECTORY_RULE@

DESKTOP_IN_FILES=	favorites.desktop.in		\
			programs.desktop.in		\
			starthere.desktop.in		\
			sysconfig.desktop.in		\
			preferences.desktop.in		\
			serverconfig.desktop.in		\
			starthere-link.desktop.in	\
			$(NULL)

DIRECTORY_IN_FILES=	serverconfig.directory.in	\
			starthere.directory.in		\
			sysconfig.directory.in		\
			$(NULL)

DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)
DIRECTORY_FILES=$(DIRECTORY_IN_FILES:.directory.in=.directory)

startheredir= $(sysconfdir)/X11/starthere
starthere_DATA= 	favorites.desktop	\
			programs.desktop	\
			sysconfig.desktop	\
			preferences.desktop	\
			serverconfig.desktop	\
			$(NULL)

DESKTOP_FILES=$(DESKTOP_IN_FILES:.in=)

nautilusdatadir = $(datadir)/nautilus

nautilusdata_DATA = \
	browser.xml 			\
	static_bookmarks.xml 		\
	nautilus-extras.placeholder 	\
	nautilus-suggested.placeholder 	\
	starthere.desktop		\
	starthere-link.desktop		\
	$(NULL)

EXTRA_DIST = $(nautilusdata_DATA) \
	$(DESKTOP_IN_FILES)			\
	$(DESKTOP_FILES)			\
	$(DIRECTORY_IN_FILES)			\
	$(DIRECTORY_FILES)			\
	$(NULL)

SUBDIRS = \
	emblems \
	linksets \
	patterns \
	$(NULL)

install-data-local: $(DIRECTORY_FILES)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/X11/starthere
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/X11/sysconfig
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/X11/serverconfig
	$(INSTALL_DATA) starthere.directory $(DESTDIR)$(sysconfdir)/X11/starthere/.directory
	$(INSTALL_DATA) sysconfig.directory $(DESTDIR)$(sysconfdir)/X11/sysconfig/.directory
	$(INSTALL_DATA) serverconfig.directory $(DESTDIR)$(sysconfdir)/X11/serverconfig/.directory
