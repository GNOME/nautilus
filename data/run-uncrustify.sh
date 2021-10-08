#!/bin/bash
DATA=$(dirname "$BASH_SOURCE")
UNCRUSTIFY=$(command -v uncrustify)

if [ -z "$UNCRUSTIFY" ];
then
    echo "Uncrustify is not installed on your system."
    exit 1
fi

if [ ! -x "$DATA/lineup-parameters" ];
then
    echo "Script lineup-parameters does not exists."
    exit 1
fi

for DIR in "$DATA/../"{src,test,libnautilus-extension,eel,extensions}
do
    for FILE in $(find "$DIR" -name "*.c" -not -path "*/gtk/*" -not -path "*/animation/*" -not -path "*/audio-video-properties/*")
    do
        # Aligning prototypes is not working yet, so avoid headers
        "$UNCRUSTIFY" -c "$DATA/uncrustify.cfg" --no-backup "$FILE"
        "$DATA/lineup-parameters" "$FILE" > "$FILE.temp" && mv "$FILE.temp" "$FILE"
   done
done
