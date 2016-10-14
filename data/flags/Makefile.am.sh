#!/bin/sh
#
# Regenerate Makefile.am based on actual contents of directory.
# This is all so 'make distcheck' will work.
#

shopt -s extglob

cat <<EOF
## Process this file with automake to produce Makefile.in
# Note: After adding new flag files, 'make Makefile.am'

## Override automake so that "make install" puts these in proper place:
pkgdatadir = \$(datadir)/\$(PACKAGE)/flags

flag_files = \\
`find * -name "*.png" -print | grep -v "shield" |sed -e 's/.*png$/		& \\\/' -e '$s/.$//'`

shield_files = \\
`find * -name "*.png" -print | grep ".*-shield\(-large\)\?.png" | sed -e 's/.*png$/		& \\\/' -e '$s/.$//'`

svg_files = \\
`find * -name "*.svg" -print | sed -e 's/.*svg$/		& \\\/' -e '$s/.$//'`

pkgdata_DATA = \$(flag_files) \$(shield_files)

EXTRA_DIST = \$(pkgdata_DATA) \$(svg_files) credits convert_png mask.png Makefile.am.sh
EOF
