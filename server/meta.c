/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

/*
 * This bit sends "I'm here" packages to the metaserver.
 */

/*
The desc block should look like this:
1) GameName   - Freeciv
2) Version    - 1.5.0
3) State      - Running(Open for new players)
4) Host       - Empty
5) Port       - 5555
6) NoPlayers  - 3
7) InfoText   - Warfare - starts at 8:00 EMT

The info string should look like this:
  GameSpecific text block
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK
#include <winsock.h>
#endif

#ifdef HAVE_OPENTRANSPORT
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#endif

#include "dataio.h"
#include "fcintl.h"
#include "log.h"
#include "netintf.h"
#include "support.h"
#include "timing.h"
#include "version.h"

#include "console.h"
#include "srv_main.h"

#include "meta.h"

bool server_is_open = FALSE;

#ifdef GENERATING_MAC    /* mac network globals */
TEndpointInfo meta_info;
EndpointRef meta_ep;
InetAddress serv_addr;
#else /* Unix network globals */
static int			sockfd=0;
#endif /* end network global selector */


/*************************************************************************
  Return static string with default info line to send to metaserver.
  This is a function (instead of a define) to keep meta.h clean of
  including config.h and version.h
*************************************************************************/
const char *default_meta_server_info_string(void)
{
#if IS_BETA_VERSION
  return "unstable pre-" NEXT_STABLE_VERSION ": beware";
#else
#if IS_DEVEL_VERSION
  return "development version: beware";
#else
  return "(default)";
#endif
#endif
}

/*************************************************************************
...
*************************************************************************/
void meta_addr_split(void)
{
  char *metaserver_port_separator = strchr(srvarg.metaserver_addr, ':');

  if (metaserver_port_separator) {
    sscanf(metaserver_port_separator + 1, "%hu", &srvarg.metaserver_port);
  }
}

/*************************************************************************
...
*************************************************************************/
char *meta_addr_port(void)
{
  static char retstr[300];

  if (srvarg.metaserver_port == DEFAULT_META_SERVER_PORT) {
    sz_strlcpy(retstr, srvarg.metaserver_addr);
  } else {
    my_snprintf(retstr, sizeof(retstr),
		"%s:%d", srvarg.metaserver_addr, srvarg.metaserver_port);
  }

  return retstr;
}

/*************************************************************************
...
  Returns true if able to send 
*************************************************************************/
static bool send_to_metaserver(char *desc, char *info)
{
  unsigned char buffer[MAX_LEN_PACKET];
  struct data_out dout;
  size_t size;

  dio_output_init(&dout, buffer, sizeof(buffer));

#ifdef GENERATING_MAC       /* mac alternate networking */
  struct TUnitData xmit;
  OSStatus err;
  xmit.udata.maxlen = MAX_LEN_PACKET;
  xmit.udata.buf=buffer;
#else  
  if(sockfd<=0)
    return FALSE;
#endif

  dio_put_uint16(&dout, 0);
  dio_put_uint8(&dout, PACKET_UDP_PCKT);
  dio_put_string(&dout, desc);
  dio_put_string(&dout, info);

  size = dio_output_used(&dout);

  dio_output_rewind(&dout);
  dio_put_uint16(&dout, size);

#ifdef GENERATING_MAC  /* mac networking */
  xmit.udata.len=strlen((const char *)buffer);
  err=OTSndUData(meta_ep, &xmit);
#else
  my_writesocket(sockfd, buffer, size);
#endif
  return TRUE;
}

/*************************************************************************
...
*************************************************************************/
void server_close_udp(void)
{
  server_is_open = FALSE;

#ifdef GENERATING_MAC  /* mac networking */
  OTUnbind(meta_ep);
#else
  if(sockfd<=0)
    return;
  my_closesocket(sockfd);
  sockfd=0;
#endif
}

/*************************************************************************
...
*************************************************************************/
static void metaserver_failed(void)
{
  con_puts(C_METAERROR, _("Not reporting to the metaserver in this game."));
  con_flush();
}

/*************************************************************************
...
*************************************************************************/
void server_open_udp(void)
{
  char *servername=srvarg.metaserver_addr;
  bool bad;
#ifdef GENERATING_MAC  /* mac networking */
  OSStatus err1;
  OSErr err2;
  InetSvcRef ref=OTOpenInternetServices(kDefaultInternetServicesPath, 0, &err1);
  InetHostInfo hinfo;
#else
  struct sockaddr_in serv_addr;
#endif
  
  /*
   * Fill in the structure "serv_addr" with the address of the
   * server that we want to connect with, checks if server address 
   * is valid, both decimal-dotted and name.
   */
#ifdef GENERATING_MAC  /* mac networking */
  if (err1 == 0) {
    err1=OTInetStringToAddress(ref, servername, &hinfo);
    serv_addr.fHost=hinfo.addrs[0];
    bad = ((serv_addr.fHost == 0) || (err1 != 0));
  } else {
    freelog(LOG_NORMAL, _("Error opening OpenTransport (Id: %g)"),
	    err1);
    bad=true;
  }
#else
  bad = !fc_lookup_host(servername, &serv_addr);
  serv_addr.sin_port = htons(srvarg.metaserver_port);
#endif
  if (bad) {
    freelog(LOG_ERROR, _("Metaserver: bad address: [%s]."), servername);
    metaserver_failed();
    return;
  }

  /*
   * Open a UDP socket (an Internet datagram socket).
   */
#ifdef GENERATING_MAC  /* mac networking */
  meta_ep=OTOpenEndpoint(OTCreateConfiguration(kUDPName), 0, &meta_info, &err1);
  bad = (err1 != 0);
#else  
  bad = ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1);
#endif
  if (bad) {
    freelog(LOG_ERROR, "Metaserver: can't open datagram socket: %s",
	    mystrerror(errno));
    metaserver_failed();
    return;
  }

  /*
   * Bind any local address for us and
   * associate datagram socket with server.
   */
#ifdef GENERATING_MAC  /* mac networking */
  if (OTBind(meta_ep, NULL, NULL) != 0) {
    freelog(LOG_ERROR, "Metaserver: can't bind local address: %s",
	    mystrerror(errno));
    metaserver_failed();
    return;
  }
#else
  /* no, this is not weird, see man connect(2) --vasc */
  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))==-1) {
    freelog(LOG_ERROR, "Metaserver: connect failed: %s", mystrerror(errno));
    metaserver_failed();
    return;
  }
#endif

  server_is_open = TRUE;
}


/**************************************************************************
...
**************************************************************************/
bool send_server_info_to_metaserver(bool do_send, bool reset_timer)
{
  static struct timer *time_since_last_send = NULL;
  char desc[4096], info[4096];
  int num_nonbarbarians = get_num_human_and_ai_players();

  if (reset_timer && time_since_last_send)
  {
    free_timer(time_since_last_send);
    time_since_last_send = NULL;
    return TRUE;
     /* use when we close the connection to a metaserver */
  }

  if (!do_send && time_since_last_send
      && ((int) read_timer_seconds(time_since_last_send)
	  < METASERVER_UPDATE_INTERVAL)) {
    return FALSE;
  }
  if (!time_since_last_send) {
    time_since_last_send = new_timer(TIMER_USER, TIMER_ACTIVE);
  }

  /* build description block */
  desc[0]='\0';
  
  cat_snprintf(desc, sizeof(desc), "Freeciv\n");
  cat_snprintf(desc, sizeof(desc), VERSION_STRING"\n");
  /* note: the following strings are not translated here;
     we mark them so they may be translated when received by a client */
  switch(server_state) {
  case PRE_GAME_STATE:
    cat_snprintf(desc, sizeof(desc), N_("Pregame"));
    break;
  case RUN_GAME_STATE:
    cat_snprintf(desc, sizeof(desc), N_("Running"));
    break;
  case GAME_OVER_STATE:
    cat_snprintf(desc, sizeof(desc), N_("Game over"));
    break;
  default:
    cat_snprintf(desc, sizeof(desc), N_("Waiting"));
  }
  cat_snprintf(desc, sizeof(desc), "\n");
  cat_snprintf(desc, sizeof(desc), "%s\n", "UNSET");
  cat_snprintf(desc, sizeof(desc), "%d\n", srvarg.port);
  cat_snprintf(desc, sizeof(desc), "%d\n", num_nonbarbarians);
  cat_snprintf(desc, sizeof(desc), "%s %s", srvarg.metaserver_info_line,
	       srvarg.extra_metaserver_info);

  /* now build the info block */
  info[0]='\0';
  cat_snprintf(info, sizeof(info),
	  "Players:%d  Min players:%d  Max players:%d\n",
	  num_nonbarbarians,  game.min_players, game.max_players);
  cat_snprintf(info, sizeof(info),
	  "Timeout:%d  Year: %s\n",
	  game.timeout, textyear(game.year));
    
    
  cat_snprintf(info, sizeof(info),
	       "NO:  NAME:               HOST:\n");
  cat_snprintf(info, sizeof(info),
	       "----------------------------------------\n");

  players_iterate(pplayer) {
    if (!is_barbarian(pplayer)) {
      /* Fixme: how should metaserver handle multi-connects?
       * Uses player_addr_hack() for now.
       */
      cat_snprintf(info, sizeof(info), "%2d   %-20s %s\n",
		   pplayer->player_no, pplayer->name,
		   player_addr_hack(pplayer));
    }
  } players_iterate_end;

  clear_timer_start(time_since_last_send);
  return send_to_metaserver(desc, info);
}
