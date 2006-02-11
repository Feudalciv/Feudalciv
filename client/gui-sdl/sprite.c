/********************************************************************** 
 Freeciv - Copyright (C) 2005 The Freeciv Team
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

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

/* utility */
#include "fcintl.h"
#include "log.h"

/* gui-sdl */
#include "graphics.h"
#include "gui_dither.h"

#include "sprite.h"

static struct sprite *ctor_sprite(SDL_Surface *pSurface);

/****************************************************************************
  Return a NULL-terminated, permanently allocated array of possible
  graphics types extensions.  Extensions listed first will be checked
  first.
****************************************************************************/
const char **gfx_fileextensions(void)
{
  static const char *ext[] = {
    "png",
    "xpm",
    NULL
  };

  return ext;
}

/****************************************************************************
  Load the given graphics file into a sprite.  This function loads an
  entire image file, which may later be broken up into individual sprites
  with crop_sprite.
****************************************************************************/
struct sprite * load_gfxfile(const char *filename)
{
  SDL_Surface *pNew = NULL;
  SDL_Surface *pBuf = NULL;

  if ((pBuf = IMG_Load(filename)) == NULL) {
    freelog(LOG_ERROR,
	    _("load_gfxfile: Unable to load graphic file %s!"),
	    filename);
    return NULL;		/* Should I use abotr() ? */
  }

  if (pBuf->flags & SDL_SRCCOLORKEY) {
    SDL_SetColorKey(pBuf, SDL_SRCCOLORKEY, pBuf->format->colorkey);
  }
  
  if (correct_black(pBuf)) {
    pNew = pBuf;
    freelog(LOG_DEBUG, _("%s load with own %d bpp format !"), filename,
	    pNew->format->BitsPerPixel);
  } else {
    Uint32 color;
    
    freelog(LOG_DEBUG, _("%s (%d bpp) load with screen (%d bpp) format !"),
	    filename, pBuf->format->BitsPerPixel,
	    Main.screen->format->BitsPerPixel);

    pNew = create_surf(pBuf->w, pBuf->h, SDL_SWSURFACE);
    color = SDL_MapRGB(pNew->format, 255, 0, 255);
    SDL_FillRect(pNew, NULL, color);
    
    if (SDL_BlitSurface(pBuf, NULL, pNew, NULL)) {
      FREESURFACE(pNew);
      return ctor_sprite(pBuf);
    }

    FREESURFACE(pBuf);
    SDL_SetColorKey(pNew, SDL_SRCCOLORKEY, color);
    
  }

  return ctor_sprite(pNew);
}

/****************************************************************************
  Create a new sprite by cropping and taking only the given portion of
  the image.

  source gives the sprite that is to be cropped.

  x,y, width, height gives the rectangle to be cropped.  The pixel at
  position of the source sprite will be at (0,0) in the new sprite, and
  the new sprite will have dimensions (width, height).

  mask gives an additional mask to be used for clipping the new
  sprite. Only the transparency value of the mask is used in
  crop_sprite. The formula is: dest_trans = src_trans *
  mask_trans. Note that because the transparency is expressed as an
  integer it is common to divide it by 256 afterwards.

  mask_offset_x, mask_offset_y is the offset of the mask relative to the
  origin of the source image.  The pixel at (mask_offset_x,mask_offset_y)
  in the mask image will be used to clip pixel (0,0) in the source image
  which is pixel (-x,-y) in the new image.
****************************************************************************/
struct sprite *crop_sprite(struct sprite *source,
			   int x, int y, int width, int height,
			   struct sprite *mask,
			   int mask_offset_x, int mask_offset_y)
{
  SDL_Rect src_rect = {(Sint16) x, (Sint16) y, (Uint16) width, (Uint16) height};
  SDL_Surface *pTmp = crop_rect_from_surface(GET_SURF(source), &src_rect);
  SDL_Surface *pSrc = NULL;
  SDL_Surface *pMask = NULL;
  SDL_Surface *pDest = NULL;
  SDL_Surface *pTmp2 = NULL;
      
  if (pTmp->format->Amask) {
    SDL_SetAlpha(pTmp, SDL_SRCALPHA, 255);
    pSrc = pTmp;
  } else {
    SDL_SetColorKey(pTmp, SDL_SRCCOLORKEY | SDL_RLEACCEL, pTmp->format->colorkey);
    pSrc = SDL_ConvertSurface(pTmp, pTmp->format, pTmp->flags);

    if (!pSrc) {
      return ctor_sprite(pTmp);
    }

    FREESURFACE(pTmp);
  }

  if (mask) {

    pMask = mask->psurface;

    /* make sure all surfaces have an alpha channel */

    if (!pMask->format->Amask) {
      pMask = SDL_DisplayFormatAlpha(pMask);
    }
    
    if (!pSrc->format->Amask) {
      pTmp2 = SDL_DisplayFormatAlpha(pSrc);
      FREESURFACE(pSrc);
      pSrc = pTmp2;
    }
   
    pDest = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
       pSrc->format->BitsPerPixel, pSrc->format->Rmask, pSrc->format->Gmask, 
       pSrc->format->Bmask, pSrc->format->Amask);
    
    SDL_FillRect(pDest, NULL, pSrc->format->colorkey);
    SDL_SetColorKey(pDest, SDL_SRCCOLORKEY | SDL_RLEACCEL, pSrc->format->colorkey);
    
    dither_surface(pSrc, pMask, pDest, x - mask_offset_x, y - mask_offset_y);

    FREESURFACE(pSrc);
    
    return ctor_sprite(pDest);
  }

  return ctor_sprite(pSrc);
}

/****************************************************************************
  Find the dimensions of the sprite.
****************************************************************************/
void get_sprite_dimensions(struct sprite *sprite, int *width, int *height)
{
  *width = GET_SURF(sprite)->w;
  *height = GET_SURF(sprite)->h;
}

/****************************************************************************
  Free a sprite and all associated image data.
****************************************************************************/
void free_sprite(struct sprite *s)
{
  FREESURFACE(GET_SURF(s));
  FC_FREE(s);
}

/*************************************************************************/

/**************************************************************************
  Create a sprite struct and fill it with SDL_Surface pointer
**************************************************************************/
static struct sprite * ctor_sprite(SDL_Surface *pSurface)
{
  struct sprite *result = fc_malloc(sizeof(struct sprite));

  result->psurface = pSurface;

  return result;
}
