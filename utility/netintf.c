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

/********************************************************************** 
  Common network interface.
**********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif 
#ifdef HAVE_WINSOCK
#include <winsock.h>
#endif

#include "log.h"
#include "support.h"
#include "shared.h"		/* TRUE, FALSE */

#include "netintf.h"

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif


/***************************************************************
  Read from a socket.
***************************************************************/
int my_readsocket(int sock, void *buf, size_t size)
{
#ifdef HAVE_WINSOCK
  return recv(sock, buf, size, 0);
#else
  return read(sock, buf, size);
#endif
}

/***************************************************************
  Write to a socket.
***************************************************************/
int my_writesocket(int sock, const void *buf, size_t size)
{
#ifdef HAVE_WINSOCK
  return send(sock, buf, size, 0);
#else
  return write(sock, buf, size);
#endif
}

/***************************************************************
  Close a socket.
***************************************************************/
void my_closesocket(int sock)
{
#ifdef HAVE_WINSOCK
  closesocket(sock);
#else
  close(sock);
#endif
}

/***************************************************************
  Initialize network stuff.
***************************************************************/
void my_init_network(void)
{
#ifdef HAVE_WINSOCK
  WSADATA wsa;

  if (WSAStartup(MAKEWORD(1, 1), &wsa) != 0) {
    freelog(LOG_ERROR, "no usable WINSOCK.DLL: %s", mystrerror(errno));
  }
#endif

  /* broken pipes are ignored. */
#ifdef HAVE_SIGPIPE
  (void) signal(SIGPIPE, SIG_IGN);
#endif
}

/***************************************************************
  Shutdown network stuff.
***************************************************************/
void my_shutdown_network(void)
{
#ifdef HAVE_WINSOCK
  WSACleanup();
#endif
}

/***************************************************************
  Set socket to non-blocking.
***************************************************************/
void my_nonblock(int sockfd)
{
#ifdef NONBLOCKING_SOCKETS
#ifdef HAVE_FCNTL
  int f_set;

  if ((f_set=fcntl(sockfd, F_GETFL)) == -1) {
    freelog(LOG_ERROR, "fcntl F_GETFL failed: %s", mystrerror(errno));
  }

  f_set |= O_NONBLOCK;

  if (fcntl(sockfd, F_SETFL, f_set) == -1) {
    freelog(LOG_ERROR, "fcntl F_SETFL failed: %s", mystrerror(errno));
  }
#else
#ifdef HAVE_IOCTL
  long value=1;

  if (ioctl(sockfd, FIONBIO, (char*)&value) == -1) {
    freelog(LOG_ERROR, "ioctl failed: %s", mystrerror(errno));
  }
#endif
#endif
#else
  freelog(LOG_DEBUG, "NONBLOCKING_SOCKETS not available");
#endif
}

/***************************************************************************
  Look up the service at hostname:port and fill in *sa.
***************************************************************************/
bool net_lookup_service(const char *name,int port,struct sockaddr *sa,int len)
{
  struct hostent *hp;
  struct sockaddr_in *sock = NULL;

  if (len != sizeof(*sock)) {
    return FALSE;
  }

  sock = (struct sockaddr_in *) sa;
  sock->sin_family = AF_INET;
  sock->sin_port = htons(port);

#ifdef HAVE_INET_ATON
  if (inet_aton(name, &sock->sin_addr) != 0) {
    return TRUE;
  }
#else
  if ((sock->sin_addr.s_addr = inet_addr(name)) != INADDR_NONE) {
    return TRUE;
  }
#endif
  hp = gethostbyname(name);
  if (!hp || hp->h_addrtype != AF_INET) {
    return FALSE;
  }

  memcpy(&sock->sin_addr, hp->h_addr, hp->h_length);
  return TRUE;
}

/*************************************************************************
  Writes buf to socket and returns the response in an fz_FILE.
  Use only on blocking sockets.
*************************************************************************/
fz_FILE *my_querysocket(int sock, void *buf, size_t size)
{
  FILE *fp;

#ifdef HAVE_FDOPEN
  fp = fdopen(sock, "r+b");
  if (fwrite(buf, 1, size, fp) != size) {
    die("socket %d: write error", sock);
  }
  fflush(fp);

  /* we don't use my_closesocket on sock here since when fp is closed
   * sock will also be closed. fdopen doesn't dup the socket descriptor. */
#else
  {
    char tmp[4096];
    int n;

    fp = tmpfile();
    my_writesocket(sock, buf, size);

    while ((n = my_readsocket(sock, tmp, sizeof(tmp))) > 0) {
      if (fwrite(tmp, 1, n, fp) != n) {
	die("socket %d: write error", sock);
      }
    }
    fflush(fp);

    my_closesocket(sock);

    rewind(fp);
  }
#endif

  return fz_from_stream(fp);
}
