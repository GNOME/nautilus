#!/bin/sh

# This is a quick hack to check if any nautilus auxiliary processes
# are running, and if so, list them and kill them.  It is not
# portable, and should be be expected to be used in any kind of
# production capacity.

quiet=no
extreme=no
medusa=no

if [ $# -gt 0 ]
then
    arg=$1

    if [ "$arg" = "-q" ]
    then
	quiet=yes
    elif [ "$arg" = "-x" ]
    then
	extreme=yes
    elif [ "$arg" = "-m" ]
    then
	medusa=yes
    fi
fi

if [ $# -gt 1 ]
then
    arg=$2

    if [ "$arg" = "-q" ]
    then
	quiet=yes
    elif [ "$arg" = "-x" ]
    then
	extreme=yes
    elif [ "$arg" = "-m" ]
    then
	medusa=yes
    fi
fi

if [ $# -gt 2 ]
then
    arg=$3

    if [ "$arg" = "-q" ]
    then
	quiet=yes
    elif [ "$arg" = "-x" ]
    then
	extreme=yes
    elif [ "$arg" = "-m" ]
    then
	medusa=yes
    fi
fi


echo_unless_quiet ()
{
    if [ "$quiet" != "yes" ]
    then
	echo "$*"
    fi
}

# Add any new auxiliary programs here.
AUX_PROGS="\
bonobo-application-x-pdf \
bonobo-image-generic \
bonobo-text-plain \
gnome-vfs-slave \
hyperbola \
nautilus-history-view \
nautilus-content-loser \
nautilus-hardware-view \
nautilus-mozilla-content-view \
nautilus-notes \
nautilus-rpm-view \
nautilus-sample-content-view \
nautilus-service-startup-view \
nautilus-sidebar-loser \
nautilus-tree-view \
ntl-history-view \
ntl-notes \
ntl-web-search \
ntl-web-browser \
"

if [ "$extreme" = "yes" ]
then
    AUX_PROGS="oafd gconfd gconfd-0.6 gconfd-1 eazel-proxy $AUX_PROGS"
fi

unset FOUND_ANY

for NAME in $AUX_PROGS; do
    EGREP_PATTERN=`echo $NAME | sed -e 's/\(.\)\(.*\)/[\1]\2/' | sed -e 's/\[\\\^\]/\[\\^\]/'`
    COUNT=`ps auxww | egrep $EGREP_PATTERN | grep -v emacs | wc -l`

    if [ $COUNT -gt 0 ]; then
	if [ -z $FOUND_ANY ]; then
	    echo_unless_quiet "nautilus-clean: Stale processes found."
	    FOUND_ANY=true
	fi
	echo_unless_quiet "$NAME: $COUNT"

	if [ "$quiet" != "yes" ]
	then
	    killall "$NAME"
	else
	    killall "$NAME" > /dev/null 2>&1
	fi
    fi
done

if [ -z $FOUND_ANY ]; then
    echo_unless_quiet "nautilus-clean: No stale processes found."
fi


if [ "$medusa" = "yes" ]; then
    if [ -f `which medusa-restart 2> /dev/null || echo xxx` ]; then
	echo_unless_quiet "Restarting medusa search and index servers."
	medusa-restart
    fi
fi


