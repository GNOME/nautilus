include $(top_srcdir)/Makefile.shared

EXTRA_DIST=			\
	nautilus-verify-rpm.sh  \
	nautilus-restore-settings-to-default.sh \
	$(NULL)

install-data-hook: nautilus-verify-rpm.sh nautilus-restore-settings-to-default.sh
	$(mkinstalldirs) $(DESTDIR)$(prefix)/bin
	@list='nautilus-verify-rpm.sh nautilus-restore-settings-to-default.sh'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)$(prefix)/bin/$$p; \
	  else :; fi; \
	done

