#!/usr/bin/env bash

usage="Usage: $0 bundle.c amalgam.h"
target=${1:?$usage}
source=${2:?$usage}

[[ -f $source ]] || (echo source missing && exit)

declare -A seen

cat /dev/null > "$target"

bundle() {
    local file=$1

    [[ -z "${seen[$file]}" ]] || return 1
    seen[$file]=1

    echo "// Inlined $file: //" >> "$target"
    IFS=""
    while read -r line; do
        if [[ $line =~ ^\#include\ \"(.+)\" && -f ${BASH_REMATCH[1]} ]]; then
            include=${BASH_REMATCH[1]}
            if bundle $include; then
                echo "$line" >> "$target"
            else
                echo "//$line" >> "$target"
            fi
        else
            echo "$line" >> "$target"
        fi
    done < $file
    echo "// End of inlined $file //" >> "$target"
    return 0
}

bundle "$source"
