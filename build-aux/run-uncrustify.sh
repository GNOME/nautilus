#!/bin/bash
BUILD_AUX=$(dirname "$BASH_SOURCE")
UNCRUSTIFY=$(command -v uncrustify)

if [ -z "$UNCRUSTIFY" ];
then
    echo "Uncrustify is not installed on your system."
    exit 1
fi

if [ ! -x "$BUILD_AUX/lineup-parameters" ];
then
    echo "Script lineup-parameters does not exists."
    exit 1
fi

for DIR in "$BUILD_AUX/../"{src,test,libnautilus-extension,eel,extensions}
do
    for FILE in $(find "$DIR" -name "*.c")
    do
        # Aligning prototypes is not working yet, so avoid headers
        "$UNCRUSTIFY" -c "$BUILD_AUX/uncrustify.cfg" --no-backup "$FILE" &&
        "$BUILD_AUX/lineup-parameters" "$FILE" > "$FILE.temp" && mv "$FILE.temp" "$FILE" &
   done
done

wait
