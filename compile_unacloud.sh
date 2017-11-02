#!/bin/bash

# Unacloud compilation
# ====================


# generate the control file
fakeroot debian/rules debian/control

# remove the old files
rm -rf debian/linux-image-extra-*/lib/modules/*/kernel/kernel

# compile the kernel
DEB_BUILD_OPTIONS=parallel=2 do_tools=false no_dumpfile=1 \
   fakeroot debian/rules binary-generic
