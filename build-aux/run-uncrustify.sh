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
    echo "Script lineup-parameters does not exist."
    exit 1
fi

format_file()
{
    local FILE="$1"
    local TMP_FILE
    local ALIGNED_FILE

    TMP_FILE=$(mktemp) || return 1
    ALIGNED_FILE=$(mktemp) || { rm -f "$TMP_FILE"; return 1; }

    # Aligning prototypes is not working yet, so avoid headers
    "$UNCRUSTIFY" -c "$BUILD_AUX/uncrustify.cfg" -f "$FILE" -o "$TMP_FILE" &&
    "$BUILD_AUX/lineup-parameters" "$TMP_FILE" > "$ALIGNED_FILE" &&
    if ! cmp -s "$FILE" "$ALIGNED_FILE"; then
        mv "$ALIGNED_FILE" "$FILE"
    fi

    rm -f "$TMP_FILE" "$ALIGNED_FILE"
}

for DIR in "$BUILD_AUX/../"{src,test,libnautilus-extension,extensions}
do
    for FILE in $(find "$DIR" -name "*.c")
    do
        format_file "$FILE" &
   done
done

wait
