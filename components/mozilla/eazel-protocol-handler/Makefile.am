NULL=

libdir=$(prefix)/share/nautilus/mozilla-components

lib_LTLIBRARIES=libeazel-protocol-handler.la

INCLUDES = \
	-I. \
	$(MOZILLA_COMPONENT_CFLAGS) \
	-DMOZILLA_MILESTONE=$(MOZILLA_MILESTONE) \
	$(NULL)

libeazel_protocol_handler_la_LDFLAGS=\
	$(MOZILLA_COMPONENT_LDFLAGS) \
	$(MOZILLA_PROTOCOL_HANDLER_LIBS) \
	$(NULL)

libeazel_protocol_handler_la_SOURCES =\
	eazel-protocol-channel.cpp \
	eazel-protocol-handler.cpp \
	eazel-protocol-module.cpp \
	$(NULL)

noinst_HEADERS =				\
	eazel-protocol-channel.h \
	eazel-protocol-handler.h \
	$(NULL)

EXTRA_DIST = $(NULL)
