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
                          gui_dither.c  -  description
                             -------------------
    begin                : Wed Sep 04 2002
    copyright            : (C) 2002 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
 **********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <SDL/SDL.h>
#include <SDL/SDL_endian.h>

#include "tilespec.h"
#include "graphics.h"
#include "log.h"
#include "gui_dither.h"

/**************************************************************************
  ...
**************************************************************************/
static void dither_surface8(SDL_Surface * pDither, SDL_Surface * pMask,
                            SDL_Surface * pDest,
                            int mask_offset_x, int mask_offset_y)
{
  int row, col;
    
  Uint8 *pDither_Pixel = (Uint8 *)pDither->pixels;
  Uint8 *pDest_Pixel = (Uint8 *)pDest->pixels;
  Uint8 *pMask_Pixel = NULL;

  for (row = 0; row < pDither->h; row++) {
      
    pMask_Pixel = (Uint8 *)pMask->pixels
                  + pMask->w * (row + mask_offset_y)
                  + mask_offset_x;
    
    for (col = 0; col < pDither->w; col++) {
      if (*pMask_Pixel != pMask->format->colorkey) {
        *pDest_Pixel = *pDither_Pixel;
      }
      
      pDither_Pixel++; pDest_Pixel++; pMask_Pixel++;
    }
  }
}

static void dither_surface16(SDL_Surface * pDither, SDL_Surface * pMask,
                             SDL_Surface * pDest,
                             int mask_offset_x, int mask_offset_y)
{
  int row, col;
    
  Uint16 *pDither_Pixel = (Uint16 *)pDither->pixels;
  Uint16 *pDest_Pixel = (Uint16 *)pDest->pixels;
  Uint16 *pMask_Pixel = NULL;

  for (row = 0; row < pDither->h; row++) {
      
    pMask_Pixel = (Uint16 *)pMask->pixels
                  + pMask->w * (row + mask_offset_y)
                  + mask_offset_x;
    
    for (col = 0; col < pDither->w; col++) {
      if (*pMask_Pixel != pMask->format->colorkey) {
        *pDest_Pixel = *pDither_Pixel;
      }
      
      pDither_Pixel++; pDest_Pixel++; pMask_Pixel++;
    }
  }
}

static void dither_surface24(SDL_Surface * pDither, SDL_Surface * pMask,
                             SDL_Surface * pDest,
                             int mask_offset_x, int mask_offset_y)
{
  /* IMPLEMENT ME */
  SDL_BlitSurface(pDither, NULL, pDest, NULL);
}

static void dither_surface32(SDL_Surface * pDither, SDL_Surface * pMask,
                             SDL_Surface * pDest,
                             int mask_offset_x, int mask_offset_y)
{
  int row, col;
    
  Uint32 *pDither_Pixel = (Uint32 *)pDither->pixels;
  Uint32 *pDest_Pixel = (Uint32 *)pDest->pixels;
  Uint32 *pMask_Pixel = NULL;

  for (row = 0; row < pDither->h; row++) {
      
    pMask_Pixel = (Uint32 *)pMask->pixels
                  + pMask->w * (row + mask_offset_y)
                  + mask_offset_x;
    
    for (col = 0; col < pDither->w; col++) {
      if (*pMask_Pixel != pMask->format->colorkey) {
        *pDest_Pixel = *pDither_Pixel;
      }
      
      pDither_Pixel++; pDest_Pixel++; pMask_Pixel++;
    }
  }
}

void dither_surface(SDL_Surface * pDither, SDL_Surface * pMask,
                    SDL_Surface * pDest,
                    int mask_offset_x, int mask_offset_y)
{
  int Bpp = pDither->format->BytesPerPixel;

  lock_surf(pDither);
  lock_surf(pMask);
  lock_surf(pDest);
  if (Bpp > 1) {
    if (Bpp > 2) {
      if (Bpp > 3) {
	dither_surface32(pDither, pMask, pDest, mask_offset_x, mask_offset_y);
      } else {
	dither_surface24(pDither, pMask, pDest, mask_offset_x, mask_offset_y);
      }
    } else {
      dither_surface16(pDither, pMask, pDest, mask_offset_x, mask_offset_y);
    }
  } else {
    dither_surface8(pDither, pMask, pDest, mask_offset_x, mask_offset_y);
  }
  unlock_surf(pDither);
  unlock_surf(pMask);
  unlock_surf(pDest);
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_north8(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2, h = 0, w = 0;
  Uint8 *pDither_Pixel = (Uint8 *) pDither->pixels + step;
  Uint8 *pMask_Pixel = (Uint8 *) pMask->pixels + step;
  Uint8 *pDest_Pixel = (Uint8 *) pDest->pixels + step;

  while (TRUE) {
    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_north16(SDL_Surface * pDither, SDL_Surface * pMask,
			   SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2, h = 0, w = 0;
  Uint16 *pDither_Pixel = (Uint16 *) pDither->pixels + step;
  Uint16 *pMask_Pixel = (Uint16 *) pMask->pixels + step;
  Uint16 *pDest_Pixel = (Uint16 *) pDest->pixels + step;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ..
**************************************************************************/
static void dither_north24(SDL_Surface * pDither, SDL_Surface * pMask,
			   SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = 0, w = 0;
  Uint8 *pDither_Pixel, *pMask_Pixel, *pDest_Pixel;

  step *= 3;

  pDither_Pixel = (Uint8 *) pDither->pixels + step;
  pMask_Pixel = (Uint8 *) pMask->pixels + step;
  pDest_Pixel = (Uint8 *) pDest->pixels + step;

  while (TRUE) {
    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

static void dither_north32(SDL_Surface * pDither, SDL_Surface * pMask,
			   SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2, h = 0, w = 0;
  Uint32 *pDither_Pixel = (Uint32 *) pDither->pixels + step;
  Uint32 *pMask_Pixel = (Uint32 *) pMask->pixels + step;
  Uint32 *pDest_Pixel = (Uint32 *) pDest->pixels + step;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
void dither_north(SDL_Surface * pDither, SDL_Surface * pMask,
		  SDL_Surface * pDest)
{
  int Bpp = pDither->format->BytesPerPixel;

  lock_surf(pDither);
  lock_surf(pMask);
  lock_surf(pDest);
  if (Bpp > 1) {
    if (Bpp > 2) {
      if (Bpp > 3) {
	dither_north32(pDither, pMask, pDest);
      } else {
	dither_north24(pDither, pMask, pDest);
      }
    } else {
      dither_north16(pDither, pMask, pDest);
    }
  } else {
    dither_north8(pDither, pMask, pDest);
  }
  unlock_surf(pDither);
  unlock_surf(pMask);
  unlock_surf(pDest);
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_south8(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint16 step = tileset_tile_width(tileset) / 2;
  register Uint16 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;

  Uint8 *pDither_Pixel = (Uint8 *) pDither->pixels + h;
  Uint8 *pMask_Pixel = (Uint8 *) pMask->pixels + h;
  Uint8 *pDest_Pixel = (Uint8 *) pDest->pixels + h;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_south16(SDL_Surface * pDither, SDL_Surface * pMask,
			   SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;

  Uint16 *pDither_Pixel = (Uint16 *) pDither->pixels + h;
  Uint16 *pMask_Pixel = (Uint16 *) pMask->pixels + h;
  Uint16 *pDest_Pixel = (Uint16 *) pDest->pixels + h;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_south24(SDL_Surface * pDither, SDL_Surface * pMask,
			   SDL_Surface * pDest)
{
  register Uint32 step = (tileset_tile_width(tileset) / 2);
  register Uint32 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;
  Uint8 *pDither_Pixel, *pMask_Pixel, *pDest_Pixel;

  step *= 3;
  h *= 3;

  pDither_Pixel = (Uint8 *) pDither->pixels + h;
  pMask_Pixel = (Uint8 *) pMask->pixels + h;
  pDest_Pixel = (Uint8 *) pDest->pixels + h;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_south32(SDL_Surface * pDither, SDL_Surface * pMask,
			   SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;

  Uint32 *pDither_Pixel = (Uint32 *) pDither->pixels + h;
  Uint32 *pMask_Pixel = (Uint32 *) pMask->pixels + h;
  Uint32 *pDest_Pixel = (Uint32 *) pDest->pixels + h;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
void dither_south(SDL_Surface * pDither, SDL_Surface * pMask,
		  SDL_Surface * pDest)
{
  int Bpp = pDither->format->BytesPerPixel;

  lock_surf(pDither);
  lock_surf(pMask);
  lock_surf(pDest);
  if (Bpp > 1) {
    if (Bpp > 2) {
      if (Bpp > 3) {
	dither_south32(pDither, pMask, pDest);
      } else {
	dither_south24(pDither, pMask, pDest);
      }
    } else {
      dither_south16(pDither, pMask, pDest);
    }
  } else {
    dither_south8(pDither, pMask, pDest);
  }
  unlock_surf(pDither);
  unlock_surf(pMask);
  unlock_surf(pDest);

}

/**************************************************************************
  ...
**************************************************************************/
static void dither_east8(SDL_Surface * pDither, SDL_Surface * pMask,
			 SDL_Surface * pDest)
{
  register Uint16 step = tileset_tile_width(tileset) / 2;
  register Uint16 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;

  Uint8 *pDither_Pixel = (Uint8 *) pDither->pixels + h + step;
  Uint8 *pMask_Pixel = (Uint8 *) pMask->pixels + h + step;
  Uint8 *pDest_Pixel = (Uint8 *) pDest->pixels + h + step;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h > tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_east16(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint16 step = tileset_tile_width(tileset) / 2;
  register Uint16 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;

  Uint16 *pDither_Pixel = (Uint16 *) pDither->pixels + h + step;
  Uint16 *pMask_Pixel = (Uint16 *) pMask->pixels + h + step;
  Uint16 *pDest_Pixel = (Uint16 *) pDest->pixels + h + step;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_east24(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;
  Uint8 *pDither_Pixel, *pMask_Pixel, *pDest_Pixel;

  step *= 3;
  h *= 3;

  pDither_Pixel = (Uint8 *) pDither->pixels + h + step;
  pMask_Pixel = (Uint8 *) pMask->pixels + h + step;
  pDest_Pixel = (Uint8 *) pDest->pixels + h + step;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_east32(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = tileset_tile_width(tileset) * (tileset_tile_height(tileset) / 2), w = 0;

  Uint32 *pDither_Pixel = (Uint32 *) pDither->pixels + h + step;
  Uint32 *pMask_Pixel = (Uint32 *) pMask->pixels + h + step;
  Uint32 *pDest_Pixel = (Uint32 *) pDest->pixels + h + step;

  h = 0;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
void dither_east(SDL_Surface * pDither, SDL_Surface * pMask,
		 SDL_Surface * pDest)
{
  int Bpp = pDither->format->BytesPerPixel;

  lock_surf(pDither);
  lock_surf(pMask);
  lock_surf(pDest);
  if (Bpp > 1) {
    if (Bpp > 2) {
      if (Bpp > 3) {
	dither_east32(pDither, pMask, pDest);
      } else {
	dither_east24(pDither, pMask, pDest);
      }
    } else {
      dither_east16(pDither, pMask, pDest);
    }
  } else {
    dither_east8(pDither, pMask, pDest);
  }
  unlock_surf(pDither);
  unlock_surf(pMask);
  unlock_surf(pDest);
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_west8(SDL_Surface * pDither, SDL_Surface * pMask,
			 SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = 0, w = 0;

  Uint8 *pDither_Pixel = (Uint8 *) pDither->pixels;
  Uint8 *pMask_Pixel = (Uint8 *) pMask->pixels;
  Uint8 *pDest_Pixel = (Uint8 *) pDest->pixels;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h >= tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_west16(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = 0, w = 0;

  Uint16 *pDither_Pixel = (Uint16 *) pDither->pixels;
  Uint16 *pMask_Pixel = (Uint16 *) pMask->pixels;
  Uint16 *pDest_Pixel = (Uint16 *) pDest->pixels;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h > tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_west24(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = 0, w = 0;

  Uint8 *pDither_Pixel = (Uint8 *) pDither->pixels;
  Uint8 *pMask_Pixel = (Uint8 *) pMask->pixels;
  Uint8 *pDest_Pixel = (Uint8 *) pDest->pixels;

  step *= 3;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h > tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void dither_west32(SDL_Surface * pDither, SDL_Surface * pMask,
			  SDL_Surface * pDest)
{
  register Uint32 step = tileset_tile_width(tileset) / 2;
  register Uint32 h = 0, w = 0;

  Uint32 *pDither_Pixel = (Uint32 *) pDither->pixels;
  Uint32 *pMask_Pixel = (Uint32 *) pMask->pixels;
  Uint32 *pDest_Pixel = (Uint32 *) pDest->pixels;

  while (TRUE) {

    if (*pMask_Pixel != pMask->format->colorkey) {
      *pDest_Pixel = *pDither_Pixel;
    }

    pMask_Pixel++;
    pDest_Pixel++;
    pDither_Pixel++;
    w++;

    if (w == step) {
      pMask_Pixel += step;
      pDither_Pixel += step;
      pDest_Pixel += step;

      w = 0;

      if (++h > tileset_tile_height(tileset) / 2) {
	break;
      }
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
void dither_west(SDL_Surface * pDither, SDL_Surface * pMask,
		 SDL_Surface * pDest)
{
  int Bpp = pDither->format->BytesPerPixel;
  lock_surf(pDither);
  lock_surf(pMask);
  lock_surf(pDest);
  if (Bpp > 1) {
    if (Bpp > 2) {
      if (Bpp > 3) {
	dither_west32(pDither, pMask, pDest);
      } else {
	dither_west24(pDither, pMask, pDest);
      }
    } else {
      dither_west16(pDither, pMask, pDest);
    }
  } else {
    dither_west8(pDither, pMask, pDest);
  }
  unlock_surf(pDither);
  unlock_surf(pMask);
  unlock_surf(pDest);
}
