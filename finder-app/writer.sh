#!/bin/bash

if [[ $# -ne 2 ]]; then
    echo "parameter fault $#"
    exit 1
fi

WRITEFILE=$1
WRITESTR=$2

mkdir -p $(dirname $WRITEFILE)
echo $WRITESTR > $WRITEFILE

if [[ $? -eq 0 ]]; then
    echo "The string '$WRITESTR' was written to the file '$WRITEFILE'"
else
    echo "The string '$WRITESTR' was not written to the file '$WRITEFILE'"
fi
