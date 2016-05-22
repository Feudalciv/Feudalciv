#!/bin/bash

# Freeciv-web Travis CI Bootstrap Script - play.freeciv.org 
#
# https://travis-ci.org/freeciv/freeciv-web
#
# script is run to install Freeciv-web on Travis CI continuous integration.
# Travis-CI currenctly uses Ubuntu 14.04 as the base Ubuntu version.
#
echo "Installing Feudalciv on Travis CI."
basedir=$(pwd)
logfile="${basedir}/feudalciv-travis.log"


# Redirect copy of output to a log file.
exec > >(tee ${logfile})
exec 2>&1
set -e

echo "================================="
echo "Running Freeciv setup script."
echo "================================="

uname -a
echo basedir  $basedir
echo logfile $logfile

# Based on fresh install of Ubuntu 14.04
dependencies="libcurl4-openssl-dev subversion pngcrush libtool automake autoconf autotools-dev language-pack-en python3-setuptools python3.4 python3.4-dev imagemagick liblzma-dev xvfb libicu-dev libsdl1.2-dev libjansson-dev dos2unix libgtk-3-dev"

## dependencies
echo "==== Installing Updates and Dependencies ===="
echo "apt-get update"
apt-get -y update
echo "apt-get install dependencies"
apt-get -y install ${dependencies}

echo "==== Building feudalciv ===="
./autogen.sh
make

echo "=============================="
echo "Feudalciv: Build successful!"
