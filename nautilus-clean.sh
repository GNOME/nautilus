#!/bin/sh

# This is a quick hack to check if any nautilus auxiliary processes
# are running, and if so, list them and kill them.  It is not
# portable, and should be be expected to be used in any kind of
# production capacity.

extreme=no
medusa=no
nokill=no
quiet=no

while ( [ $# -gt 0 ] )
do
    case "$1" in 
        '-a')
            quiet=yes
            shift
        ;;
	'-m')
	    medusa=yes
	    shift
	;;
	'-n')
	    nokill=yes
	    shift
	;;
        '-x')
            extreme=yes
            shift
        ;;
        *)
            echo "nautilus-clean.sh unknown option: $1"
            shift
        ;;
    esac
done

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
eazel-proxy \
eazel-proxy-util \
gnome-vfs-slave \
hyperbola \
nautilus-adapter \
nautilus-change-password-view \
nautilus-content-loser \
nautilus-hardware-view \
nautilus-history-view \
nautilus-image-view \
nautilus-inventory-view \
nautilus-mozilla-content-view \
nautilus-mpg123 \
nautilus-music-view \
nautilus-news \
nautilus-notes \
nautilus-rpm-view \
nautilus-sample-content-view \
nautilus-sample-service-view \
nautilus-service-install-view \
nautilus-service-startup-view \
nautilus-sidebar-loser \
nautilus-summary-view \
nautilus-text-view \
nautilus-throbber \
nautilus-tree-view \
trilobite-eazel-install-service  \
trilobite-eazel-time-view \
"

if [ "$extreme" = "yes" ]
then
    AUX_PROGS="gconfd-1 $AUX_PROGS oafd"
fi

unset FOUND_ANY

sysname=`uname -s`

if [ "$sysname" = "SunOS" ]; then
	killcmd="pkill"
else
	killcmd="killall"
fi

for NAME in $AUX_PROGS; do
    EGREP_PATTERN=`echo $NAME | sed -e 's/\(.\)\(.*\)/[\1]\2/' | sed -e 's/\[\\\^\]/\[\\^\]/'`
    COUNT=`ps auxww | egrep \ $EGREP_PATTERN | grep -v emacs | wc -l`

    if [ $COUNT -gt 0 ]; then
	if [ -z $FOUND_ANY ]; then
	    echo_unless_quiet "nautilus-clean: Stale processes found."
	    FOUND_ANY=true
	fi
	echo_unless_quiet "$NAME: $COUNT"

	if [ "$nokill" != "yes" ]; then
	    if [ "$quiet" != "yes" ]; then
		$killcmd "$NAME"
	    else
	        $killcmd "$NAME" > /dev/null 2>&1
	    fi
	    if [ "$NAME" = "gconfd-1" ]; then
		rm -f "$HOME/.gconfd/saved_state"
	    fi
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
