#!/bin/sh

# This is a quick hack to check if any nautilus auxiliary processes
# are running, and if so, list them and kill them.  It is not
# portable, and should be be expected to be used in any kind of
# production capacity.


# Add any new auxiliary programs here.
AUX_PROGS="hyperbola ntl-history-view ntl-notes ntl-web-search ntl-web-browser nautilus-sample-content-view nautilus-hardware-view
bonobo-text-plain bonobo-image-generic gnome-vfs-slave nautilus-rpm-view nautilus-service-startup-view nautilus-mozilla-content-view";

unset FOUND_ANY;

for NAME in $AUX_PROGS; do

    EGREP_PATTERN=`echo $NAME | sed -e 's/\(.\)\(.*\)/[\1]\2/' | sed -e 's/\[\\\^\]/\[\\^\]/'`;

    COUNT=`ps auxww | egrep $EGREP_PATTERN | grep -v emacs | wc -l`;

    if [ $COUNT -gt 0 ]; then
	if [ -z $FOUND_ANY ]; then
	    echo "Stale Processes Found";
	    FOUND_ANY=true;
	fi
	echo "$NAME: $COUNT";
	killall "$NAME";
    fi
done


if [ -z $FOUND_ANY ]; then
    echo "No Stale Processes Found";
    exit 0;
fi

exit -1;
