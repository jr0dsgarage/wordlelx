#!/bin/zsh
# Source this before building: . ./env-watcom.sh
export WATCOM="$HOME/watcom-src/rel"
export PATH="$WATCOM/armo64:$PATH"
export INCLUDE="$WATCOM/h"   # wcc requires this to find system headers
echo "OpenWatcom ready.  Run: wmake        (build)"
echo "                   Run: wmake run    (build + launch DOSBox-X)"
echo "                   Run: wmake clean  (remove objects)"
