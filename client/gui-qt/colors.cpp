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

/* utility */
#include "mem.h"

// gui-qt
#include "qtg_cxxside.h"

#include "colors.h"

/****************************************************************************
  Allocate a color (adjusting it for our colormap if necessary on paletted
  systems) and return a pointer to it.
****************************************************************************/
struct color *qtg_color_alloc(int r, int g, int b)
{
  struct color *pcolor = new color;

  pcolor->qcolor.setRgb(r, g, b);

  return pcolor;
}

/****************************************************************************
  Free a previously allocated color.  See qtg_color_alloc.
****************************************************************************/
void qtg_color_free(struct color *pcolor)
{
  delete pcolor;
}
