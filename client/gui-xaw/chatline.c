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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiText.h>

#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "packets.h"
#include "shared.h"		/* wordwrap_string() */
#include "support.h"

#include "climisc.h"
#include "clinet.h"
#include "gui_main.h"
#include "gui_stuff.h"

#include "chatline.h"

/**************************************************************************
...
**************************************************************************/
void chatline_key_send(Widget w)
{
  struct packet_generic_message apacket;
  String theinput;
  String empty="";

  XtVaGetValues(w, XtNstring, &theinput, NULL);
  
  if(*theinput) {
    mystrlcpy(apacket.message, theinput, MAX_LEN_MSG-MAX_LEN_USERNAME+1);
    send_packet_generic_message(&aconnection, PACKET_CHAT_MSG, &apacket);
  }

  XtVaSetValues(w, XtNstring, empty, NULL);
}

/**************************************************************************
 this is properly a bad way to append to a text widget. Using the 
 "useStringInPlace" resource and doubling mem alloc'ing would be better.  
 Nope - tried it and many other variations and it wasn't any better. 
 I'll replace this widget with a widget supportting hyperlinks later, so
 leth's forget the problem.

 There seems to be an Xaw problem with doing both wrap and scroll:
 its supposed to automatically scroll to end when we change the insertion
 point, but if a line is wrapped the scroll lags behind a line, and
 stays behind on subsequent additions, until the too-long line scrolls
 off the top.  (I tried setting the insert position to the last char,
 instead of the start of line as below, but that didn't help.)  So we
 split the line ourselves.  I'm just using a fixed length split; should
 perhaps check and use width of output window (but font size?)  -dwp

 Now uses window's font size and width.  Assumes fixed-width font.  --jjm
**************************************************************************/
void real_append_output_window(const char *input_string)
{
  static int m_width=0;

  Dimension windowwth;
  int maxlinelen;
  String theoutput;
  char *newout, *rmcr, *astring = mystrdup(input_string);

  if (!m_width) {
    XFontStruct *out_font;
    XtVaGetValues(outputwindow_text, XtNfont, &out_font, NULL);
    if (out_font)
      m_width=XTextWidth(out_font, "M", 1);
    else
      m_width=10;
  }

  XtVaGetValues(outputwindow_text, XtNwidth, &windowwth, NULL);
  maxlinelen=(windowwth/m_width)-1;
  if (maxlinelen<32) maxlinelen=32;

  freelog(LOG_DEBUG, "m_width %d windowwth %d maxlinelen %d",
	  m_width, (int)windowwth, maxlinelen);

  if (strlen(astring) > maxlinelen) {
    wordwrap_string(astring, maxlinelen);
  }
  
  XtVaGetValues(outputwindow_text, XtNstring, &theoutput, NULL);
  newout=fc_malloc(strlen(astring)+strlen(theoutput)+2);
  sprintf(newout, "%s\n%s", theoutput, astring);

  /* calc carret position - last line, first pos */ 
  for(rmcr=newout+strlen(newout); rmcr>newout; rmcr--)
    if(*rmcr=='\n')
      break;

  /* shit happens when setting both values at the same time */
  XawTextDisableRedisplay(outputwindow_text);
  XtVaSetValues(outputwindow_text, XtNstring, newout, NULL);
  XtVaSetValues(outputwindow_text, XtNinsertPosition, rmcr-newout+1, NULL);
  XawTextEnableRedisplay(outputwindow_text);
  
  free(newout);
  free(astring);
}

/**************************************************************************
 I have no idea what module this belongs in -- Syela
 I've decided to put output_window routines in chatline.c, because
 the are somewhat related and append_output_window is already here.  --dwp
**************************************************************************/
void log_output_window(void)
{
  String theoutput;

  XtVaGetValues(outputwindow_text, XtNstring, &theoutput, NULL);
  write_chatline_content(theoutput);
}

/**************************************************************************
...
**************************************************************************/
void clear_output_window(void)
{
  XtVaSetValues(outputwindow_text, XtNstring, _("Cleared output window."), NULL);
}
