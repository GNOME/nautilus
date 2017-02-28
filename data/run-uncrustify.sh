#!/bin/bash
DATA=$(dirname "$BASH_SOURCE")
if [ -x "$DATA/lineup-parameters" ];
then
    for DIR in "$DATA/../"{src,nautilus-desktop,test,libnautilus-extension,eel,nautilus-sendto-extension}
    do
        for FILE in $(find "$DIR" -name "*.c")
        do
            # Aligning prototypes is not working yet, so avoid headers
            uncrustify -c "$DATA/uncrustify.cfg" --no-backup "$FILE"
            "$DATA/lineup-parameters" "$FILE" > "$FILE.temp" && mv "$FILE.temp" "$FILE"
       done
    done
else
    echo "Script lineup-parameters does not exists here in (source directory)/data, probably because Nautilus was built in a different directory than the source directory.
Copy the program in the (build directory)/data/lineup-parameters here in (source directory)/data and run again run-uncrustify.sh."
fi
