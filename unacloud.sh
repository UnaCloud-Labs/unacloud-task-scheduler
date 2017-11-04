#!/bin/sh

# Unacloud compilation
# ====================

case "$1" in

   deps)
     # This installer is based on Ubuntu 14.04 repositories
     if [[ `lsb_release -rs` != "14.04" ]]; then
       echo "NOTE: This script has been tested on Ubuntu 14.04"
     fi

     # install kernel compilation requisites
     sudo apt-get install build-essential kernel-package libncurses5-dev fakeroot wget bzip2 git dpkg-dev devscripts

     # install build dependencies
     sudo apt-get build-dep --no-install-recommends linux-image-4.2.0-27-generic
     ;;

   clean)
      fakeroot debian/rules clean
      ;;

   control)
      # generate the control file
      fakeroot debian/rules debian/control

      # remove the old files
      rm -rf debian/linux-image-extra-*/lib/modules/*/kernel/kernel
      ;;

   compile)
      # compile the kernel
      DEB_BUILD_OPTIONS="parallel=4 no_dumpfile=1" \
          fakeroot debian/rules binary-generic
      ;;

   install)
      # install deb files at parent folder
      sudo dpkg -i ../linux-*.deb 
      ;;

   *)
      echo "Usage: $0 deps|clean|control|compile|install" >&2
      exit 3
      ;;
esac
