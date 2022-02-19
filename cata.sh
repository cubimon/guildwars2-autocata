#!/bin/bash
SCRIPT=`realpath $0`
SCRIPTPATH=`dirname $SCRIPT`
ENDPROGRESS=`rofi -dmenu`
$SCRIPTPATH/build/cata $ENDPROGRESS
