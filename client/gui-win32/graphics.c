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

#include <string.h>  
#include <stdlib.h>
#include <windows.h>
#include <png.h>

#include "game.h"
#include "log.h"
#include "mem.h"
#include "shared.h"
#include "support.h"
#include "unit.h"
#include "version.h"
 
#include "climisc.h"
#include "colors.h"
#include "gui_main.h"
#include "mapview_g.h"
#include "tilespec.h"   

#include "graphics.h"
#define CACHE_SIZE 32

struct Sprite_cache {
  SPRITE *sprite;
  HBITMAP bmp;
  HBITMAP mask;
};

static struct Sprite_cache sprite_cache[CACHE_SIZE];
static int cache_id_count=0;
static SPRITE *sprcache;
static HBITMAP bitmapcache;
SPRITE *intro_gfx_sprite=NULL;
SPRITE *radar_gfx_sprite=NULL;
static SPRITE fog_sprite;
static HDC hdcbig,hdcsmall;

/**************************************************************************

**************************************************************************/
void
load_intro_gfx(void)
{
  intro_gfx_sprite=load_gfxfile(main_intro_filename);
  radar_gfx_sprite = load_gfxfile(minimap_intro_filename);     
}

/**************************************************************************

**************************************************************************/
void
load_cursors(void)
{
	/* PORTME */
}

/**************************************************************************

**************************************************************************/
void
free_intro_radar_sprites(void)
{
  if (intro_gfx_sprite)
    {
      free_sprite(intro_gfx_sprite);
      intro_gfx_sprite=NULL;
    }
  if (radar_gfx_sprite)
    {
      free_sprite(radar_gfx_sprite);
      radar_gfx_sprite=NULL;
    }
}

/**************************************************************************

**************************************************************************/
const char **
gfx_fileextensions(void)
{
  static const char *ext[] =
  {
    "png",
    NULL
  };

  return ext;
}


/*************************************************************************

 *************************************************************************/
HBITMAP BITMAP2HBITMAP(BITMAP *bmp)
{
  return CreateBitmap(bmp->bmWidth,bmp->bmHeight,bmp->bmPlanes,
                      bmp->bmBitsPixel,bmp->bmBits);
}


/**************************************************************************

**************************************************************************/
static void HBITMAP2BITMAP(HBITMAP hbmp,BITMAP *bmp)
{
  int bmpsize;
  GetObject(hbmp,sizeof(BITMAP),bmp);
  bmpsize=bmp->bmHeight*bmp->bmWidthBytes;
  bmp->bmBits=fc_malloc(bmpsize);
  GetBitmapBits(hbmp,bmpsize,bmp->bmBits);
}
/**************************************************************************

**************************************************************************/
static void sprite2hbitmap(struct Sprite *s,HBITMAP *bmp, HBITMAP *mask)
{
  struct Sprite_cache *sprc;
  sprc=&sprite_cache[s->cache_id];
  if (sprc->sprite==s) {
    *bmp=sprc->bmp;
    *mask=sprc->mask;
  } else {
    cache_id_count++;
    if (cache_id_count>=CACHE_SIZE)
      cache_id_count=0;
    sprc=&sprite_cache[cache_id_count];
    DeleteObject(sprc->bmp);
    if (sprc->mask)
      DeleteObject(sprc->mask);
    sprc->sprite=s;
    s->cache_id=cache_id_count;
    sprc->bmp=BITMAP2HBITMAP(&s->bmp);
    if (s->has_mask) {
      sprc->mask=BITMAP2HBITMAP(&s->mask);
    } else {
      sprc->mask=NULL;
    }
    *bmp=sprc->bmp;
    *mask=sprc->mask;
  }
}

/**************************************************************************

**************************************************************************/
struct Sprite *
crop_sprite(struct Sprite *source,
                           int x, int y, int width, int height)
{
  SPRITE *mysprite;
  HDC hdc;
  HBITMAP smallbitmap;
  HBITMAP smallmask;
  HBITMAP bigbitmap;
  HBITMAP bigmask;
  HBITMAP bigsave;
  HBITMAP smallsave;
  hdc=GetDC(root_window);
  mysprite=NULL;
  if (!hdcbig)
    hdcbig=CreateCompatibleDC(hdc);
  if (!hdcsmall)
    hdcsmall=CreateCompatibleDC(hdc);
  if (sprcache!=source)
    {
      if (bitmapcache) DeleteObject(bitmapcache);
      sprcache=source;
      bitmapcache=BITMAP2HBITMAP(&(source->bmp));
    }
  bigbitmap=bitmapcache;
  if (!bigbitmap)
    {
      freelog(LOG_FATAL,"BITMAP2HBITMAP failed");
      return NULL;
    }
  if (!(hdcsmall&&hdcbig))
    {
      freelog(LOG_FATAL,"CreateCompatibleDC failed");
    }
  if (!(smallbitmap=CreateCompatibleBitmap(hdc,width,height)))
    {
      freelog(LOG_FATAL,"CreateCompatibleBitmap failed");
      return NULL;
    }
  
  bigsave=SelectObject(hdcbig,bigbitmap);
  smallsave=SelectObject(hdcsmall,smallbitmap);
  BitBlt(hdcsmall,0,0,width,height,hdcbig,x,y,SRCCOPY);
  smallmask=NULL;

  if (source->has_mask)
    {
      bigmask=BITMAP2HBITMAP(&source->mask);
      SelectObject(hdcbig,bigmask);
      if ((smallmask=CreateBitmap(width,height,1,1,NULL)))
        {
          SelectObject(hdcsmall,smallmask);
          BitBlt(hdcsmall,0,0,width,height,hdcbig,x,y,SRCCOPY);
        }
      SelectObject(hdcbig,bigbitmap);
      DeleteObject(bigmask);
    }

  mysprite=fc_malloc(sizeof(struct Sprite));
  mysprite->cache_id=0;
  mysprite->width=width;
  mysprite->height=height;
  HBITMAP2BITMAP(smallbitmap,&mysprite->bmp);
  mysprite->has_mask=0;
  if (smallmask)
    {
      mysprite->has_mask=1;
      HBITMAP2BITMAP(smallmask,&mysprite->mask);
    }

  SelectObject(hdcbig,bigsave);
  SelectObject(hdcsmall,smallsave);
  ReleaseDC(root_window,hdc);
  if (smallmask) DeleteObject(smallmask);
  DeleteObject(smallbitmap);
     
   
  return mysprite;
}

/****************************************************************************
  Find the dimensions of the sprite.
****************************************************************************/
void get_sprite_dimensions(struct Sprite *sprite, int *width, int *height)
{
  *width = sprite->width;
  *height = sprite->height;
}

/**************************************************************************

***************************************************************************/
void init_fog_bmp(void)
{
  int x,y;
  HBITMAP old;
  HDC hdc;
  HBITMAP fog;
  if (!is_isometric)
    return;
  hdc=CreateCompatibleDC(NULL);
  fog=CreateCompatibleBitmap(hdc,NORMAL_TILE_WIDTH,NORMAL_TILE_HEIGHT);
  old=SelectObject(hdc,fog);
  BitBlt(hdc,0,0,NORMAL_TILE_WIDTH,NORMAL_TILE_HEIGHT,NULL,0,0,BLACKNESS);
  SelectObject(hdc,old);
  fog_sprite.width=NORMAL_TILE_WIDTH;
  fog_sprite.height=NORMAL_TILE_HEIGHT;
  HBITMAP2BITMAP(fog,&fog_sprite.bmp);
  DeleteObject(fog);
  fog_sprite.has_mask=1;
  fog=BITMAP2HBITMAP(&sprites.black_tile->mask);
  old=SelectObject(hdc,fog);
  for(x=0;x<NORMAL_TILE_WIDTH;x++)
    for(y=0;y<NORMAL_TILE_HEIGHT;y++)
      {
        if (!GetPixel(hdc,x,y))
          {
            if ((x+y)&1)
              SetPixel(hdc,x,y,RGB(255,255,255));
          }
      }
  SelectObject(hdc,old);
  HBITMAP2BITMAP(fog,&fog_sprite.mask);
  DeleteObject(fog);
  DeleteObject(hdc);
  
}

/***************************************************************************
...
***************************************************************************/
struct Sprite *load_gfxfile(const char *filename)
{
  png_structp pngp;
  png_infop infop;
  png_uint_32 sig_read=0;
  png_int_32 width, height, row, col;
  int bit_depth, color_type, interlace_type;
  FILE *fp;

  png_bytep *row_pointers;
   
  struct Sprite *mysprite;
  int has_mask;
  BITMAPINFO bi;
  void *buf;
  BYTE *pb, *p;
  HDC hdc;
  HBITMAP bmp;
  HBITMAP dib;

  if (!(fp=fopen(filename, "rb"))) {
    MessageBox(NULL, "failed reading", filename, MB_OK);
    freelog(LOG_FATAL, "Failed reading PNG file: %s", filename);
    exit(EXIT_FAILURE);
  }
    
  if (!(pngp=png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {

    freelog(LOG_FATAL, "Failed creating PNG struct");
    exit(EXIT_FAILURE);
  }
 
  if (!(infop=png_create_info_struct(pngp))) {
    freelog(LOG_FATAL, "Failed creating PNG struct");
    exit(EXIT_FAILURE);
  }
   
  if (setjmp(pngp->jmpbuf)) {
    freelog(LOG_FATAL, "Failed while reading PNG file: %s", filename);
    exit(EXIT_FAILURE);
  }

  png_init_io(pngp, fp);
  png_set_sig_bytes(pngp, sig_read);

  png_read_info(pngp, infop);

  png_set_strip_16(pngp);
  png_set_gray_to_rgb(pngp);
  png_set_packing(pngp);
  png_set_palette_to_rgb(pngp);
  png_set_tRNS_to_alpha(pngp);
  png_set_filler(pngp, 0xFF, PNG_FILLER_AFTER);
  png_set_bgr(pngp);
  png_set_invert_alpha(pngp); 

  png_read_update_info(pngp, infop);
  png_get_IHDR(pngp, infop, &width, &height, &bit_depth, &color_type,
	       &interlace_type, NULL, NULL);
  
  has_mask=(color_type & PNG_COLOR_MASK_ALPHA);

  row_pointers=fc_malloc(sizeof(png_bytep)*height);

  for (row=0; row<height; row++)
    row_pointers[row]=fc_malloc(png_get_rowbytes(pngp, infop));

  png_read_image(pngp, row_pointers);
  png_read_end(pngp, infop);
  fclose(fp);


  mysprite=fc_malloc(sizeof(struct Sprite));

  /* init DIB BITMAPINFOHEADER */
  bi.bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth          = width;
  bi.bmiHeader.biHeight         = height;
  bi.bmiHeader.biPlanes         = 1;
  bi.bmiHeader.biBitCount       = 32;

  bi.bmiHeader.biCompression    = BI_RGB;
  bi.bmiHeader.biSizeImage      = 0;
  bi.bmiHeader.biXPelsPerMeter  = 0;
  bi.bmiHeader.biYPelsPerMeter  = 0;
  bi.bmiHeader.biClrUsed        = 0;
  bi.bmiHeader.biClrImportant   = 0;

  hdc=GetDC(root_window);
  dib=CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &buf, NULL, 0);
  bmp=CreateCompatibleBitmap(hdc,bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);


  for (row = height-1, pb = buf; row>=0; row--) {
    for (col = 0, p = row_pointers[row]; col<width; col++) {
      *pb++=*p++;
      *pb++=*p++;
      *pb++=*p++;
      *pb++=0;
      p++;
    } 
  }
  
  SetDIBits(hdc, bmp, 0, bi.bmiHeader.biHeight,
	    buf, &bi, DIB_RGB_COLORS);
  DeleteObject(dib);
  ReleaseDC(root_window, hdc);
  HBITMAP2BITMAP(bmp,&mysprite->bmp);
  DeleteObject(bmp);
  if (has_mask) {
    hdc=CreateCompatibleDC(NULL);
    dib=CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &buf, NULL, 0);
    for (row = height-1, pb = buf; row >= 0; row--) {
      for (col=0, p=row_pointers[row]; col<width; col++) {
        p+=3;
        *pb++=*p;
        *pb++=*p;
        *pb++=*p;
        *pb++=0;
        p+=1;
      }
    }
    bmp=CreateCompatibleBitmap(hdc, bi.bmiHeader.biWidth,
			       bi.bmiHeader.biHeight);
    SetDIBits(hdc, bmp, 0, bi.bmiHeader.biHeight, buf, 
	      &bi, DIB_RGB_COLORS);
    DeleteDC(hdc);
    HBITMAP2BITMAP(bmp,&mysprite->mask);
    DeleteObject(dib);
    DeleteObject(bmp);
  }

  mysprite->has_mask=has_mask;
  mysprite->cache_id=0;
  mysprite->width=width;
  mysprite->height=height;


  for (row=0; row<height; row++)
    free(row_pointers[row]);
  free(row_pointers);
  png_destroy_read_struct(&pngp, &infop, NULL);
  return mysprite;
		     }


/**************************************************************************

**************************************************************************/
void draw_sprite(struct Sprite *sprite, HDC hdc, int x, int y)
{
  draw_sprite_part(sprite,hdc,x,y,sprite->width,sprite->height,0,0);
}

/**************************************************************************

**************************************************************************/
void  draw_sprite_part_with_mask(struct Sprite *sprite,
				 struct Sprite *sprite_mask,
				 HDC hdc,
				 int x, int y,int w, int h,
				 int xsrc, int ysrc)
{
  HDC hdccomp;
  HDC hdcmask;
  HBITMAP tempbit;
  HBITMAP tempmask;
  HBITMAP bitmap;
  HBITMAP maskbit;
  if (!sprite) return;
  hdccomp=CreateCompatibleDC(NULL);
  hdcmask=CreateCompatibleDC(NULL);
  sprite2hbitmap(sprite,&bitmap,&maskbit);
  if (sprite_mask->has_mask)
    {
      HBITMAP dummy;
      sprite2hbitmap(sprite_mask,&dummy,&maskbit);
      tempmask=SelectObject(hdcmask,maskbit); 
      tempbit=SelectObject(hdccomp,bitmap);
      BitBlt(hdc,x,y,w,h,hdccomp,xsrc,ysrc,SRCINVERT);
      BitBlt(hdc,x,y,w,h,hdcmask,xsrc,ysrc,SRCAND);
      BitBlt(hdc,x,y,w,h,hdccomp,xsrc,ysrc,SRCINVERT);
      SelectObject(hdcmask,tempmask);
    }
  else
    {
      tempbit=SelectObject(hdccomp,bitmap);
      BitBlt(hdc,x,y,w,h,hdccomp,xsrc,ysrc,SRCCOPY);
    }
  SelectObject(hdccomp,tempbit);
  DeleteDC(hdccomp);
  DeleteDC(hdcmask);
}
/**************************************************************************

**************************************************************************/
void draw_sprite_part(struct Sprite *sprite,HDC hdc,
		      int x,int y,int w,int h,int xsrc,int ysrc)
{
  draw_sprite_part_with_mask(sprite,sprite,hdc,x,y,w,h,xsrc,ysrc);
}

/**************************************************************************

**************************************************************************/
void draw_fog_part(HDC hdc,int x, int y,int w, int h,
		   int xsrc, int ysrc)
{
  if (is_isometric)
    draw_sprite_part(&fog_sprite,hdc,x,y,w,h,xsrc,ysrc);
}

#if 0
/**************************************************************************

**************************************************************************/
static void crop_sprite_real(struct Sprite *source)
{
} 
#endif
        
/**************************************************************************

**************************************************************************/
void free_sprite(struct Sprite *s)
{
  if (s->has_mask) {
    free(s->mask.bmBits);
  }

  free(s->bmp.bmBits);

  free(s);
  if (bitmapcache) {
    DeleteObject(bitmapcache);
    bitmapcache=NULL;
  }
  sprcache = NULL;
}

/**************************************************************************

**************************************************************************/
bool isometric_view_supported(void)
{
  return TRUE;
}

/**************************************************************************

**************************************************************************/
bool overhead_view_supported(void)
{
  return TRUE;
}
