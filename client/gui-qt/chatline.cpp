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
#include <fc_config.h>
#endif

// client
#include "climisc.h"      /* for write_chatline_content */

// gui-qt
#include "fc_client.h"
#include "qtg_cxxside.h"

#include "chatline.h"

/**************************************************************************
  Appends the string to the chat output window.  The string should be
  inserted on its own line, although it will have no newline.
**************************************************************************/
void qtg_real_output_window_append(const char *astring,
                                   const struct text_tag_list *tags,
                                   int conn_id)
{
  QString str;
  str = QString::fromUtf8(astring);
  gui()->append_output_window(str);
  gui()->set_status_bar(str);
}

/**************************************************************************
  Get the text of the output window, and call write_chatline_content() to
  log it.
**************************************************************************/
void log_output_window(void)
{
  /* PORTME */
  write_chatline_content(NULL);
}

/**************************************************************************
  Clear all text from the output window.
**************************************************************************/
void clear_output_window(void)
{
  /* PORTME */
#if 0
  set_output_window_text(_("Cleared output window."));
#endif
}

/**************************************************************************
  Got version message from metaserver thread.
**************************************************************************/
void qtg_version_message(char *vertext)
{
  /* FIXME - this will crash at some point - event will come 
   * later with non existent pointer 
  output_window_append(ftc_client, vertext);
  */
}
