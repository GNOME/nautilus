NULL=

LANG=C

DOC_MODULE=nautilus

HTML_DIR=$(datadir)/gnome/help

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)/$(LANG)/


install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR) 
	installfiles=`echo $(srcdir)/html/*.html 		\
	$(srcdir)/html/*.jpg $(srcdir)/html/*.css`; 		\
	if test "$$installfiles" = '$(srcdir)/html/*.html'; 	\
	then echo '-- Nothing to install' ; 			\
	else 							\
	for i in $$installfiles; do				\
		basefile=`basename $$i`;  			\
		$(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)$$basefile; 	\
	done; 							\
	fi

dist-hook:
	mkdir $(distdir)/html 
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.jpg $(srcdir)/html/*.css $(distdir)/html

.PHONY : html
