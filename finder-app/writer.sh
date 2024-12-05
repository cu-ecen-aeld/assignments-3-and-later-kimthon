#!/bin/bash

if [ $# -lt 2 ]; then
    exit 1
fi

file_path=$1
dir_path="$(dirname "${file_path}")"
content=$2

echo $file_path
echo $dir_path
echo $content
# when folder isn't there
mkdir -p $dir_path
echo $content > $file_path
