#!/bin/bash
# The make.sh of FCPP, on this project: ./make.sh test all, ./make.sh gui run rovers, ./make.sh run rovers ...
cd "$(dirname "$0")"
if [ ! -f fcpp/src/make.sh ]; then
    git submodule update --init || exit 1
fi
./fcpp/src/make.sh "$@"
