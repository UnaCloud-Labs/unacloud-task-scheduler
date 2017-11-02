#!/bin/bash

# This installer is based on Ubuntu 14.04 repositories 
if [[ `lsb_release -rs` != "14.04" ]]; then
  echo "NOTE: This script has been tested on Ubuntu 14.04"
fi

# install kernel compilation requisites
sudo apt-get install build-essential kernel-package libncurses5-dev fakeroot wget bzip2 git dpkg-dev devscripts

# install build dependencies
sudo apt-get build-dep --no-install-recommends linux-image-4.2.0-27-generic
