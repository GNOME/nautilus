#!/bin/sh

echo $*
URL="http://testmachine/RPMS/"

cd /home/ftp/pub/grande

NAME=`echo $QUERY_STRING | sed 's/filename=\(.*\)&.*/\1/'`
if test x$NAME = x$QUERY_STRING; then
	NAME=`echo $QUERY_STRING | sed 's/name=\([^&]*\)&[a-z].*/\1-[0-9]\*i386.rpm/'`

fi
if test x$NAME = x$QUERY_STRING; then
	PROVIDES=`echo $QUERY_STRING | sed 's/provides=\([^&]*\)&[a-z].*/\1/'`
fi

if test x$NAME != x$QUERY_STRING; then
	export FILE=`find ./ -name $NAME | sed 's/\..\(.*\)/\1/'`
elif test x$PROVIDES != x; then
	LIST=`ls *rpm`
	for F in $LIST; do
		HITS=`rpm -qp $F --provides |grep $PROVIDES|wc -l|awk '{printf $1}'`
		if test $HITS != 0; then
			FILE=$F
		fi
	done
fi

if test $FILE; then
	cp $PWD/$FILE /home/httpd/html/RPMS/
        echo -n "$URL$FILE"
else
 	echo PANIC
fi


