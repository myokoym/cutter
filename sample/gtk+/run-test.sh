#!/bin/sh

export BASE_DIR="`dirname $0`"

if test x"$NO_MAKE" != x"yes"; then
    make -C ../../ > /dev/null || exit 1
fi

$BASE_DIR/cutter-gtk --color=auto \
    -s $BASE_DIR "$@" $BASE_DIR
