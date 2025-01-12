#!/bin/bash

if [[ $# -ne 2 ]]; then
    echo "parameter fault $#"
    exit 1
fi
if [[ ! -d $1 ]]; then
    echo "'$1' does not exist"
    exit 1
fi

FILESDIR=$1
SEARCHSTR=$2
X=$(find $FILESDIR/. -type f | wc -l)
Y=$(grep -r "$SEARCHSTR" $FILESDIR/. | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"