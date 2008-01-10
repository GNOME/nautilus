NULL=

xml_in_files = nautilus.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

nautilusdatadir = $(datadir)/nautilus

nautilusdata_DATA = \
	browser.xml 			\
	nautilus-extras.placeholder 	\
	nautilus-suggested.placeholder 	\
	$(NULL)

EXTRA_DIST = $(nautilusdata_DATA) \
	$(xml_in_files) \
	$(NULL)

CLEANFILES = $(xml_files)

SUBDIRS = \
	icons    \
	patterns \
	$(NULL)

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif
