include $(top_srcdir)/Makefile.shared

EXTRA_DIST=			\
	nautilus-verify-rpm.sh  \
	nautilus-restore-settings-to-default.sh \
	$(NULL)

install-data-hook: nautilus-verify-rpm.sh
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='nautilus-verify-rpm.sh'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/nautilus-restore-settings-to-default.sh $(DESTDIR)/$(prefix)/bin/nautilus-restore-settings-to-default.sh;" \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p; \
	    $(INSTALL_PROGRAM) $(srcdir)/nautilus-restore-settings-to-default.sh $(DESTDIR)/$(prefix)/bin/nautilus-restore-settings-to-default.sh; \
	  else :; fi; \
	done

