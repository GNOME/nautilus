NULL=

LANG=C

DOC_MODULE=gnugpl

SGML_DIR=$(datadir)/gnome/help

TARGET_DIR=$(SGML_DIR)/$(DOC_MODULE)/$(LANG)/

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	installfiles=`echo $(srcdir)/*.sgml`;	 		\
	if test "$$installfiles" = '$(srcdir)/*.sgml'; 		\
	then echo '-- Nothing to install' ; 			\
	else 							\
	for i in $$installfiles; do				\
		basefile=`basename $$i`;  			\
		$(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)$$basefile; 	\
	done; 							\
	fi

dist-hook:
	-cp $(srcdir)/*.sgml $(distdir)
