NULL=

SUBDIRS =\
	data \
	icons \
	idl \
	intl \
	libnautilus \
	libnautilus-extensions \
	librsvg \
	nautilus-widgets \
	helper-utilities \
	components \
	po \
	src \
	test \
	$(NULL)

EXTRA_DIST=\
	COPYING.LIB \
	nautilus.spec.in \
	$(NULL)

appicondir = $(datadir)/pixmaps

appicon_DATA = nautilus.png

Applicationsdir = $(datadir)/gnome/apps/Applications

Applications_DATA = nautilus.desktop

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)

EXTRA_DIST=nautilus-clean.sh nautilus.desktop nautilus.png

install-data-hook: nautilus-clean.sh
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='nautilus-clean.sh'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p; \
	  else :; fi; \
	done
