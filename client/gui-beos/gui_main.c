/* gui_main.c -- PLACEHOLDER */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gui_main.h"

#include <stdio.h>

const char *client_string = "gui-beos";

char logfile[256] = "";

void
ui_init(void)
{
}

void
ui_main(int argc, char *argv[])		/* EXTERNAL HOOK */
{
	app_main( argc, argv );
}

void
add_net_input(int sock)		/* HOOK */
{
	/* NOT_FINISHED( "add_net_input" ); */
}

void
remove_net_input(void)		/* HOOK */
{
	/* NOT_FINISHED( "remove_net_input" ); */
}

void
sound_bell(void)		/* HOOK */
{
	/* NOT_FINISHED( "sound_bell" ); */
}
