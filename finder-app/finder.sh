#!/bin/sh
filesdir=$1
searchstr=$2

if [ $# -lt 2 ]; then
    exit 1
fi

if [ ! -d $filesdir ]; then
	exit 1
fi

file_count=$(find $filesdir -type f | wc -l)
line_count=0

for file in $filesdir/*
do
	# only file
	if [ -f $file ]
	then
		line_count=$((line_count + $(grep -c "$searchstr" "$file")))
	fi
done

echo "The number of files are $file_count and the number of matching lines are $line_count"
