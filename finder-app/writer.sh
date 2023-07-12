#!/bin/sh

WRITEFILE="$1"
WRITESTR="$2"

if [ $# -ne 2 ]; then 
    echo "ERROR invalid number of arguments"
    exit 1
fi

if [ -d "$FILESDIR" ]; then 
    rm $WRITEFILE
fi

if ! [ -d "$(dirname $WRITEFILE)" ]; then 
    mkdir -p "$(dirname $WRITEFILE)"
fi

echo "$WRITESTR" > "$WRITEFILE"

if [ $? -ne 0 ]; then
    echo "ERROR file cannot be created"
    exit 1
fi
