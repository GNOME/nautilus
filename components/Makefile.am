NULL=

if EAZEL_SERVICES
SERVICE_SUBDIRS = services
endif

if BUILD_MOZILLA_COMPONENT
MOZILLA_COMPONENT_SUBDIRS = mozilla
endif

if BUILD_RPMVIEW_COMPONENT
RPMVIEW_COMPONENT_SUBDIRS = rpmview
endif

SUBDIRS = \
	hardware \
	help \
	history \
	html \
	loser \
	music \
	notes \
	$(RPMVIEW_COMPONENT_SUBDIRS) \
	sample \
	websearch \
	$(SERVICE_SUBDIRS) \
	$(MOZILLA_COMPONENT_SUBDIRS) \
	$(NULL)
