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
                          gui_string.h  -  description
                             -------------------
    begin                : Thu May 30 2002
    copyright            : (C) 2002 by Rafa� Bursig
    email                : Rafa� Bursig <bursig@poczta.fm>
 **********************************************************************/

#ifndef __STRING_H
#define __STRING_H

#include "gui_iconv.h"

#define SF_CENTER	8
#define SF_CENTER_RIGHT	16

/* styles:
	TTF_STYLE_NORMAL	0
	TTF_STYLE_BOLD		1
	TTF_STYLE_ITALIC	2
	TTF_STYLE_UNDERLINE	4
	SF_CENTER	 	8	- use with multi string
	SF_CENTER_RIGHT		16	- use with multi string
*/

typedef struct SDL_String16 {
  Uint8 style;
  Uint8 render;
  Uint16 ptsize;

  SDL_Color forecol;
  SDL_Color backcol;
  TTF_Font *font;
  Uint16 *text;
} SDL_String16;

SDL_String16 *create_string16(Uint16 * pInTextString, Uint16 ptsize);
int write_text16(SDL_Surface * pDest, Sint16 x, Sint16 y,
		 SDL_String16 * pString);
SDL_Surface *create_text_surf_from_str16(SDL_String16 * pString);
SDL_Rect str16size(SDL_String16 * pString16);
void change_ptsize16(SDL_String16 * pString, Uint16 new_ptsize);

void unload_font(Uint16 ptsize);


#define str16len(pString16) str16size(pString16).w

/*
 *	here we use ordinary free( ... ) becouse check is made 
 *	on start.
 */
#define FREESTRING16( pString16 )		\
do {						\
	if (pString16) {			\
		FREE(pString16->text);		\
		unload_font(pString16->ptsize);	\
		free(pString16); 		\
		pString16 = NULL;		\
	}					\
} while(0)

#define create_str16_from_char(pInCharString, iPtsize) \
	create_string16(convert_to_utf16(pInCharString), iPtsize)

#endif
