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

/* See comments in speclist.h about how to use this file.  Notice this
   is kind of a .c file (it provides function bodies), but it should
   only be used included, so its also kind of a .h file.  The name
   speclist_c.h is intended to indicate this, and it also makes
   automake do the right thing. (?)

   Some of the following is duplicated from speclist.h, because speclist.h
   undefs its defines to avoid pollution.  However the file which includes
   this _must_ also include speclist.h appropriately so that the list type
   is defined.  With the usual arrangement of .c and .h files this normally
   happens anyway, so this restriction may be considered beneficial.
*/

#ifndef SPECLIST_TAG
#error Must define a SPECLIST_TAG to use this header
#endif

#ifndef SPECLIST_TYPE
#define SPECLIST_TYPE struct SPECLIST_TAG
#endif

#if defined (SPECLIST_STATIC) || defined (SPECLIST_STATIC1)
#define SPECLIST_FUNC static
#else
#define SPECLIST_FUNC
#endif

#define SPECLIST_PASTE_(x,y) x ## y
#define SPECLIST_PASTE(x,y) SPECLIST_PASTE_(x,y)

#define SPECLIST_LIST struct SPECLIST_PASTE(SPECLIST_TAG, _list)

#define SPECLIST_FOO(suffix) SPECLIST_PASTE(SPECLIST_TAG, suffix)

SPECLIST_FUNC void SPECLIST_FOO(_list_init) (SPECLIST_LIST *tthis)
{
  genlist_init(&tthis->list);
}

SPECLIST_FUNC void SPECLIST_FOO(_list_insert) (SPECLIST_LIST *tthis, SPECLIST_TYPE *pfoo)
{
  genlist_insert(&tthis->list, pfoo, 0);
}

SPECLIST_FUNC void SPECLIST_FOO(_list_unlink) (SPECLIST_LIST *tthis, SPECLIST_TYPE *pfoo)
{
  genlist_unlink(&tthis->list, pfoo);
}

#if !defined (SPECLIST_STATIC1)
SPECLIST_FUNC int SPECLIST_FOO(_list_size) (SPECLIST_LIST *tthis)
{
  return genlist_size(&tthis->list);
}

SPECLIST_FUNC SPECLIST_TYPE *SPECLIST_FOO(_list_get) (SPECLIST_LIST *tthis, int index)
{
  return (SPECLIST_TYPE *)genlist_get(&tthis->list, index);
}
#endif

#if !defined (SPECLIST_STATIC) && !defined (SPECLIST_STATIC1)
SPECLIST_FUNC void SPECLIST_FOO(_list_insert_back) (SPECLIST_LIST *tthis, SPECLIST_TYPE *pfoo)
{
  genlist_insert(&tthis->list, pfoo, -1);
}

SPECLIST_FUNC void SPECLIST_FOO(_list_unlink_all) (SPECLIST_LIST *tthis)
{
  genlist_unlink_all(&tthis->list);
}

SPECLIST_FUNC void SPECLIST_FOO(_list_sort) (SPECLIST_LIST * tthis, int (*compar) (const void *, const void *))
{
  genlist_sort(&tthis->list, compar);
}
#endif

#undef SPECLIST_TAG
#undef SPECLIST_TYPE
#undef SPECLIST_PASTE_
#undef SPECLIST_PASTE
#undef SPECLIST_LIST
#undef SPECLIST_FOO
#undef SPECLIST_FUNC
#undef SPECLIST_STATIC
#undef SPECLIST_STATIC1
