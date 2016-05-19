## Process this file with automake to produce Makefile.in
# Note: After adding a new nation file, 'make Makefile.am'

## Override automake so that "make install" puts these in proper place:
pkgdatadir = $(datadir)/@PACKAGE@/feudalciv/nation

pkgdata_DATA = \
		barbarian.ruleset \
		english.ruleset \
		pirate.ruleset \
		saxon.ruleset \
		viking.ruleset 

EXTRA_DIST = $(pkgdata_DATA) Makefile.am.sh

