#!/bin/sh
#
# Regenerate Makefile.am based on actual contents of directory.
# This is all so 'make distcheck' will work.
#
cat <<EOF
## Process this file with automake to produce Makefile.in
# Note: After adding a new nation file, 'make Makefile.am'

pkgdata_DATA = \\
`find * -name "*.ruleset" -print | sed -e 's/.*ruleset$/		& \\\/' -e '$s/.$//'`

EXTRA_DIST = \$(pkgdata_DATA) Makefile.am.sh

Makefile.am: Makefile.am.sh \$(shell echo *.ruleset)
	sh Makefile.am.sh >Makefile.am

EOF
