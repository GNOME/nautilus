NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GNET_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(OAF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	$(NULL)

gnorbadir = $(sysconfdir)/CORBA/servers

bin_PROGRAMS = nautilus-installer

nautilus_installer_SOURCES =	        	\
	nautilus-installer.c			\
	$(NULL)

nautilus_installer_LDFLAGS =	\
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)	\
	$(OAF_LIBS) \
	$(XML_LIBS) \
	$(GNET_LIBS) \
	$(RPM_LIBS) \
	$(NULL)
