#! /bin/bash
LIST=`ls $1/*rpm`
for FILE in $LIST; do
    echo `rpm -qp --queryformat="$2:%{NAME}:%{VERSION}:%{RELEASE}:%{ARCH}:%{SIZE}:%{SUMMARY}" $FILE`
done
