helpdir = $(datadir)/gnome/help/$(docname)/$(lang)

#Scrollkeeper related stuff
omf_dir=$(top_srcdir)/omf-install

EXTRA_DIST = $(htmls) $(omffiles) $(figs)

CLEANFILES = omf_timestamp

all: omf

omf: omf_timestamp

omf_timestamp: $(omffiles)
	-for omffile in $(omffiles); do \
	  scrollkeeper-preinstall $(DESTDIR)$(helpdir)/index.html $$omffile $(omf_dir)/$$omffile; \
	done
	touch omf_timestamp

app-dist-hook:
	-$(mkinstalldirs) $(distdir)/figures
	-if [ -e topic.dat ]; then \
	  cp $(srcdir)/topic.dat $(distdir); \
	fi

install-data-am: omf
	-$(mkinstalldirs) $(DESTDIR)$(helpdir)/figures
	-for file in $(srcdir)/*.html; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/figures/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/figures/$$basefile; \
	done

