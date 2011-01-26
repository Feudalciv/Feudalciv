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

#ifndef FC__NETFILE_H
#define FC__NETFILE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*nf_errmsg)(const char *msg, void *data);

struct section_file *netfile_get_section_file(const char *URL,
                                              nf_errmsg cb, void *data);

bool netfile_download_file(const char *URL, const char *filename,
                           nf_errmsg cb, void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__NETFILE_H */
