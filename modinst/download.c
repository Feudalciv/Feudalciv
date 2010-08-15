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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <unistd.h>
#include <errno.h>

/* utility */
#include "capability.h"
#include "fcintl.h"
#include "log.h"
#include "netintf.h"
#include "registry.h"

/* modinst */
#include "download.h"

static bool download_file(const char *URL, const char *local_filename)
{
  const char *path;
  char srvname[MAX_LEN_ADDR];
  int port;
  static union fc_sockaddr addr;
  int sock;
  char buf[50000];
  char hdr_buf[2048];
  int hdr_idx = 0;
  int msglen;
  int result;
  FILE *fp;
  int header_end_chars = 0;

  path = fc_lookup_httpd(srvname, &port, URL);
  if (path == NULL) {
    return FALSE;
  }

  if (!net_lookup_service(srvname, port, &addr, FALSE)) {
    return FALSE;
  }

  sock = socket(addr.saddr.sa_family, SOCK_STREAM, 0);
  if (sock == -1) {
    return FALSE;
  }

  if (fc_connect(sock, &addr.saddr, sockaddr_size(&addr)) == -1) {
    fc_closesocket(sock);
    return FALSE;
  }

  msglen = fc_snprintf(buf, sizeof(buf),
                       "GET %s HTTP/1.1\r\n"
                       "HOST: %s:%d\r\n"
                       "User-Agent: Freeciv-modpack/%s\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n",
                       path, srvname, port, VERSION_STRING);
  if (fc_writesocket(sock, buf, msglen) != msglen) {
    fc_closesocket(sock);
    return FALSE;
  }

  fp = fopen(local_filename, "w+b");
  if (fp == NULL) {
    fc_closesocket(sock);
    return FALSE;
  }

  result = 1;
  while (result) {
    result = fc_readsocket(sock, buf, sizeof(buf));

    if (result < 0) {
      if (errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK) {
        fc_closesocket(sock);
        return FALSE;
      }
      sleep(1);
    } else if (result > 0) {
      int left;
      int total_written = 0;
      int i;

      for (i = 0; i < result && header_end_chars < 4; i++) {
        if (hdr_idx < sizeof(hdr_buf)) {
          hdr_buf[hdr_idx++] = buf[i];
        }
        switch (header_end_chars) {
         case 0:
         case 2:
           if (buf[i] == '\r') {
             header_end_chars++;
           } else {
             header_end_chars =  0;
           }
           break;
         case 1:
         case 3:
           if (buf[i] == '\n') {
             header_end_chars++;
           } else {
             header_end_chars =  0;
           }
           break;
        }
      }

      left = result - i;
      total_written = i;

      while (left > 0) {
        int written;

        written = fwrite(buf + total_written, 1, left, fp);
        if (written <= 0) {
          fclose(fp);
          fc_closesocket(sock);
          return FALSE;
        }
        left -= written;
        total_written += written;
      }
    }
  }

  fclose(fp);

  fc_closesocket(sock);
  return TRUE;
}

/**************************************************************************
  Download modpack from a given URL
**************************************************************************/
const char *download_modpack(const char *URL,
                             dl_msg_callback mcb,
                             dl_pb_callback pbcb)
{
  char local_name[2048];
  const char *home;
  int start_idx;
  int filenbr, total_files;
  struct section_file *control;
  const char *control_capstr;
  const char *baseURL;
  char fileURL[2048];
  const char *src_name;
  bool partial_failure = FALSE;

  if (URL == NULL || URL[0] == '\0') {
    return _("No URL given");
  }

  if (strlen(URL) < strlen(MODPACK_SUFFIX)
      || strcmp(URL + strlen(URL) - strlen(MODPACK_SUFFIX), MODPACK_SUFFIX)) {
    return _("This does not look like modpack URL");
  }

  for (start_idx = strlen(URL) - strlen(MODPACK_SUFFIX);
       start_idx > 0 && URL[start_idx - 1] != '/';
       start_idx--) {
    /* Nothing */
  }

  log_normal("Installing modpack %s from %s", URL + start_idx, URL);

  home = getenv("HOME");
  if (home == NULL) {
    return _("Environment variable HOME not set");
  }

  fc_snprintf(local_name, sizeof(local_name),
              "%s/.freeciv/" DATASUBDIR "/control",
              home);

  if (!make_dir(local_name)) {
    return _("Cannot create required directories");
  }

  cat_snprintf(local_name, sizeof(local_name), "/%s", URL + start_idx);

  if (mcb != NULL) {
    mcb(_("Downloading modpack control file."));
  }
  if (!download_file(URL, local_name)) {
    return _("Failed to download modpack control file from given URL");
  }

  control = secfile_load(local_name, FALSE);

  if (control == NULL) {
    return _("Cannot parse modpack control file");
  }

  control_capstr = secfile_lookup_str(control, "info.options");
  if (control_capstr == NULL) {
    secfile_destroy(control);
    return _("Modpack control file has no capability string");
  }

  if (!has_capabilities(MODPACK_CAPSTR, control_capstr)) {
    log_error("Incompatible control file:");
    log_error("  control file options: %s", control_capstr);
    log_error("  supported options:    %s", MODPACK_CAPSTR);

    secfile_destroy(control);

    return _("Modpack control file is incompatible");  
  }

  baseURL = secfile_lookup_str(control, "info.baseURL");

  total_files = 0;
  do {
    src_name = secfile_lookup_str_default(control, NULL,
                                          "files.list%d.src", total_files);

    if (src_name != NULL) {
      total_files++;
    }
  } while (src_name != NULL);

  if (pbcb != NULL) {
    /* Control file already downloaded */
    double fraction = (double) 1 / (total_files + 1);

    pbcb(fraction);
  }

  filenbr = 0;
  for (filenbr = 0; filenbr < total_files; filenbr++) {
    const char *dest_name;
    int i;
    bool illegal_filename = FALSE;

    src_name = secfile_lookup_str_default(control, NULL,
                                          "files.list%d.src", filenbr);

    dest_name = secfile_lookup_str_default(control, NULL,
                                           "files.list%d.dest", filenbr);

    if (dest_name == NULL || dest_name[0] == '\0') {
      /* Missing dest name is ok, we just default to src_name */
      dest_name = src_name;
    }

    for (i = 0; dest_name[i] != '\0'; i++) {
      if (dest_name[i] == '.' && dest_name[i+1] == '.') {
        if (mcb != NULL) {
          char buf[2048];

          fc_snprintf(buf, sizeof(buf), _("Illegal path for %s"),
                      dest_name);
          mcb(buf);
        }
        partial_failure = TRUE;
        illegal_filename = TRUE;
      }
    }

    if (!illegal_filename) {
      fc_snprintf(local_name, sizeof(local_name),
                  "%s/.freeciv/" DATASUBDIR "/%s",
                  home, dest_name);
      for (i = strlen(local_name) - 1 ; local_name[i] != '/' ; i--) {
        /* Nothing */
      }
      local_name[i] = '\0';
      if (!make_dir(local_name)) {
        secfile_destroy(control);
        return _("Cannot create required directories");
      }
      local_name[i] = '/';

      if (mcb != NULL) {
        char buf[2048];

        fc_snprintf(buf, sizeof(buf), _("Downloading %s"), src_name);
        mcb(buf);
      }

      fc_snprintf(fileURL, sizeof(fileURL), "%s/%s", baseURL, src_name);
      if (!download_file(fileURL, local_name)) {
        if (mcb != NULL) {
          char buf[2048];

          fc_snprintf(buf, sizeof(buf), _("Failed to download %s"),
                      src_name);
          mcb(buf);
        }
        partial_failure = TRUE;
      }
    }

    if (pbcb != NULL) {
      /* Count download of control file also */
      double fraction = (double)(filenbr + 2) / (total_files + 1);

      pbcb(fraction);
    }
  }

  secfile_destroy(control);

  if (partial_failure) {
    return _("Some parts of the modpack failed to install.");
  }

  return NULL;
}
