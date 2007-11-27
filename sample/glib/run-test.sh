#!/bin/sh

export BASE_DIR="`dirname $0`"

if test x"$NO_MAKE" != x"yes"; then
    make -C $BASE_DIR/../../ > /dev/null || exit 1
fi

export CUT_RUNNER_MODULE_DIR=$BASE_DIR/../../cutter/module/runner/.libs
export CUT_RUNNER_FACTORY_MODULE_DIR=$BASE_DIR/../../cutter/module/runner-factory/.libs
CUTTER=$BASE_DIR/../../cutter/cutter
if test x"$CUTTER_DEBUG" = x"yes"; then
    CUTTER="$BASE_DIR/../../libtool --mode=execute gdb --args $CUTTER"
fi
CUTTER_ARGS="--multi-thread -s $BASE_DIR"
if test x"$USE_GTK" = x"yes"; then
        CUTTER_ARGS="-r gtk $CUTTER_ARGS"
else
        CUTTER_ARGS="--color-auto $CUTTER_ARGS"
fi
$CUTTER $CUTTER_ARGS "$@" $BASE_DIR
