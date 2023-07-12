#!/bin/sh

FILESDIR="$1"
SEARCHSTR="$2"

if [ $# -ne 2 ]; then 
    echo "ERROR invalid number of arguments"
    exit 1
fi

if ! [ -d "$FILESDIR" ]; then 
    echo "ERROR file doesn't exist"
    exit 1
fi


X=$(find "$FILESDIR" -type f | wc -l)
Y=$(grep -wsr "$SEARCHSTR" "$FILESDIR"/* | wc -l)

echo "The number of files are ${X} and the number of matching lines are ${Y}"
