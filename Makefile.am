include $(top_srcdir)/Makefile.shared

@XML_I18N_MERGE_DESKTOP_RULE@

DESKTOP_IN_FILES=nautilus.desktop.in
DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)

if HAVE_APPLETS
APPLETS_SUBDIRS = applets
endif

SUBDIRS =			\
	data 			\
	icons 			\
	intl 			\
	cut-n-paste-code 	\
	librsvg 		\
	libnautilus 		\
	libnautilus-adapter	\
	libnautilus-extensions 	\
	helper-utilities 	\
	helper-scripts		\
	src 			\
	components		\
	$(APPLETS_SUBDIRS)	\
	test 			\
	user-guide		\
	po 			\
	$(NULL)

EXTRA_DIST=					\
	COPYING.LIB				\
	RPMs-README				\
	HACKING					\
	MAINTAINERS				\
	nautilus.spec.in			\
	nautilus-clean.sh			\
	nautilus.desktop			\
	nautilus.desktop.in			\
	nautilus-launch-icon.png		\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	xml-i18n-extract			\
	xml-i18n-merge				\
	xml-i18n-update				\
	Makefile.shared				\
	$(DESKTOP_IN_FILES)			\
	$(DESKTOP_FILES)			\
	$(NULL)

appicondir = $(datadir)/pixmaps
appicon_DATA = nautilus-launch-icon.png

Applicationsdir = $(datadir)/gnome/apps/Applications
Applications_DATA = $(DESKTOP_FILES)

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)

install-data-hook: nautilus-clean.sh
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='nautilus-clean.sh'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p; \
	  else :; fi; \
	done
