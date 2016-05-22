## Process this file with automake to produce Makefile.in
# Note: After adding a new nation file, 'make Makefile.am'

## Override automake so that "make install" puts these in proper place:
pkgdatadir = $(datadir)/@PACKAGE@/feudalciv/nation

pkgdata_DATA = \
		barbarian.ruleset \
		cornwall.ruleset \
		danelaw.ruleset \
		gwent.ruleset \
		hampshire.ruleset \
		northumbria.ruleset \
		pirate.ruleset 

EXTRA_DIST = $(pkgdata_DATA) Makefile.am.sh

