NULL=

LANG=C

DOC_MODULE=nautilus

SGML_DIR=$(datadir)/gnome/help

TARGET_DIR=$(SGML_DIR)/$(DOC_MODULE)/$(LANG)/

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR) 
	installfiles=`echo $(srcdir)/sgml/*.sgml 		\
	$(srcdir)/img/*.png`; 					\
	if test "$$installfiles" = '$(srcdir)/sgml/*.sgml'; 	\
	then echo '-- Nothing to install' ; 			\
	else 							\
	for i in $$installfiles; do				\
		basefile=`basename $$i`;  			\
		$(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)$$basefile; 	\
	done; 							\
	fi

#	echo "installing $$i"			\

dist-hook:
	mkdir $(distdir)/sgml 
	mkdir $(distdir)/img
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/img/*.png $(distdir)/img

.PHONY : html
