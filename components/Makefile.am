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

# These components are broken:
#
#       websearch
#

SUBDIRS = \
	hardware \
	help \
	history \
	html \
	loser \
	music \
	notes \
	sample \
	tree \
	websearch \
	$(SERVICE_SUBDIRS) \
	rpmview \
	$(RPMVIEW_COMPONENT_SUBDIRS) \
	$(MOZILLA_COMPONENT_SUBDIRS) \
	$(NULL)
