NULL=

LANG=C

DOC_MODULE=nautilus

SGML_DIR=$(datadir)/gnome/help

TARGET_DIR=$(SGML_DIR)/$(DOC_MODULE)/$(LANG)/

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR) 
	installfiles=`echo $(srcdir)/sgml/*.sgml 		\
	$(srcdir)/img/*.png					\
	$(srcdir)/html/*.html`;					\
	if test "$$installfiles" = '$(srcdir)/sgml/*.sgml'; 	\
	then echo '-- Nothing to install' ; 			\
	else 							\
	for i in $$installfiles; do				\
		basefile=`basename $$i`;  			\
		$(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)$$basefile; 	\
	done; 							\
	fi

dist-hook:
	mkdir $(distdir)/sgml 
	mkdir $(distdir)/html 
	mkdir $(distdir)/img
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/img/*.png $(distdir)/img
	-cp $(srcdir)/html/*.html $(distdir)/html

