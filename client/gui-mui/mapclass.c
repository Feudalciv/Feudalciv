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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <graphics/gfxmacros.h>
#include <graphics/rpattr.h>
#include <libraries/mui.h>
#include <guigfx/guigfx.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/guigfx.h>

#include "fcintl.h"
#include "map.h"
#include "support.h"
#include "timing.h"

#include "civclient.h"
#include "citydlg.h"
#include "control.h"
#include "dialogs.h"
#include "goto.h"
#include "graphics.h"
#include "gui_main.h"
#include "mapctrl.h"
#include "mapview.h"
#include "options.h"
#include "tilespec.h"

#include "mapclass.h"
#include "muistuff.h"
#include "overviewclass.h"
#include "scrollbuttonclass.h"

/**************************************************************************
  Find the "best" city to associate with the selected tile.  
    a.  A city working the tile is the best
    b.  If no city is working the tile, choose a city that could work the tile
    c.  If multiple cities could work it, choose the most recently "looked at"
    d.  If none of the cities were looked at last, choose "randomly"
    e.  If another player is working the tile, or no cities can work it,
	return NULL
**************************************************************************/
static struct city *find_city_near_tile(int x, int y)
{
  struct tile *ptile=map_get_tile(x, y);
  struct city *pcity, *pcity2;
  int i,j;
  static struct city *last_pcity=NULL;

  if((pcity=ptile->worked))  {
    if(pcity->owner==game.player_idx)  return last_pcity=pcity;   /* rule a */
    else return NULL;	 /* rule e */
  }

  pcity2 = NULL;
  city_map_iterate(i, j)  {
    pcity = map_get_city(x+i-2, y+j-2);
    if(pcity && pcity->owner==game.player_idx && 
       get_worker_city(pcity,4-i,4-j)==C_TILE_EMPTY)  {  /* rule b */
      if(pcity==last_pcity) return pcity;  /* rule c */
      pcity2 = pcity;
    }
  }
  return last_pcity = pcity2;
}

/****************************************************************
 TilePopWindow Custom Class
*****************************************************************/

static struct MUI_CustomClass *CL_TilePopWindow;	/* only here used */


static ULONG TilePopWindow_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  Object *obj = NULL;

  int xtile = GetTagData(MUIA_TilePopWindow_X, 0, msg->ops_AttrList);
  int ytile = GetTagData(MUIA_TilePopWindow_Y, 0, msg->ops_AttrList);

  /* from mapctrl.c/popit */
  static struct map_position cross_list[2 + 1];
  struct map_position *cross_head = cross_list;
  char s[512];
  struct city *pcity;
  struct unit *punit;
  struct tile *ptile = map_get_tile(xtile, ytile);

  if (ptile->known >= TILE_KNOWN_FOGGED)
  {
    Object *group;

    obj = (Object *) DoSuperNew(cl, o,
				MUIA_Window_Activate, FALSE,
				MUIA_Window_Borderless, TRUE,
				MUIA_Window_CloseGadget, FALSE,
				MUIA_Window_DepthGadget, FALSE,
				MUIA_Window_DragBar, FALSE,
				MUIA_Window_SizeGadget, FALSE,
				MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Moused,
				MUIA_Window_TopEdge, MUIV_Window_TopEdge_Moused,
				WindowContents, group = VGroup,
				TextFrame,
				End,
				TAG_MORE, msg->ops_AttrList);

    if (obj)
    {
      Object *text_obj;
      my_snprintf(s, sizeof(s), _("Terrain: %s"), map_get_tile_info_text(xtile, ytile));
      text_obj = TextObject, MUIA_Text_Contents, s, End;
      DoMethod(group, OM_ADDMEMBER, text_obj);

      if (ptile->special & S_HUT)
      {
	text_obj = TextObject, MUIA_Text_Contents, _("Minor Tribe Village"), End;
	DoMethod(group, OM_ADDMEMBER, text_obj);
      }

      if ((pcity = map_get_city(xtile, ytile)))
      {
	my_snprintf(s, sizeof(s), _("City: %s(%s)"), pcity->name, get_nation_name(game.players[pcity->owner].nation));
	text_obj = TextObject, MUIA_Text_Contents, s, End;

	if(text_obj)
	  DoMethod(group, OM_ADDMEMBER, text_obj);

	if (city_got_citywalls(pcity)) {
	  text_obj = TextObject, MUIA_Text_Contents, _("with City Walls"), End;
	  if (text_obj)
	    DoMethod(group, OM_ADDMEMBER, text_obj);
	}
      }

      if (get_tile_infrastructure_set(ptile))
      {
	sz_strlcpy(s, _("Infrastructure: "));
	sz_strlcat(s, map_get_infrastructure_text(ptile->special));
	text_obj = TextObject, MUIA_Text_Contents, s, End;
	DoMethod(group, OM_ADDMEMBER, text_obj);
      }

      if ((punit = find_visible_unit(ptile)) && !pcity)
      {
	char cn[64];
	struct unit_type *ptype = get_unit_type(punit->type);
	cn[0] = '\0';
	if (punit->owner == game.player_idx)
	{
	  struct city *pcity;
	  pcity = player_find_city_by_id(game.player_ptr, punit->homecity);
	  if (pcity)
	    my_snprintf(cn, sizeof(cn), "/%s", pcity->name);
	}
	my_snprintf(s, sizeof(s), _("Unit: %s(%s%s)"), ptype->name, get_nation_name(game.players[punit->owner].nation), cn);

	text_obj = TextObject, MUIA_Text_Contents, s, End;
	DoMethod(group, OM_ADDMEMBER, text_obj);

	if (punit->owner == game.player_idx)
	{
	  char uc[64] = "";
	  if (unit_list_size(&ptile->units) >= 2)
	    my_snprintf(uc, sizeof(uc), _("  (%d more)"), unit_list_size(&ptile->units) - 1);

	  my_snprintf(s, sizeof(s), _("A:%d D:%d FP:%d HP:%d/%d%s%s"), ptype->attack_strength,
		  ptype->defense_strength, ptype->firepower, punit->hp,
		  ptype->hp, punit->veteran ? " V" : "", uc);

	  if(punit->activity==ACTIVITY_GOTO || punit->connecting) 
	  {
	    cross_head->x = punit->goto_dest_x;
	    cross_head->y = punit->goto_dest_y;
	    cross_head++;
	  }
	}
	else
	{
	  my_snprintf(s, sizeof(s), _("A:%d D:%d FP:%d HP:%d0%%"), ptype->attack_strength,
		  ptype->defense_strength, ptype->firepower,
		  (punit->hp * 100 / ptype->hp + 9) / 10);
	}

	text_obj = TextObject, MUIA_Text_Contents, s, End;
	DoMethod(group, OM_ADDMEMBER, text_obj);
      }

      cross_head->x = xtile;
      cross_head->y = ytile;
      cross_head++;
      cross_head->x = -1;
/*
   for (i = 0; cross_list[i].x >= 0; i++)
   {
   put_cross_overlay_tile(cross_list[i].x,cross_list[i].y);
   }
 */
    }
  }

  return ((ULONG) obj);
}

DISPATCHERPROTO(TilePopWindow_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return (TilePopWindow_New(cl, obj, (struct opSet *) msg));
  }

  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 Map Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_Map;

Object *MakeMap(void)
{
  return MapObject,
    InnerSpacing(0,0),
    VirtualFrame,
    MUIA_Font, MUIV_Font_Big,
    End;
}

struct Map_Data
{
  /* �ber Tags (2 Sprites) */
  struct Sprite *intro_gfx_sprite;
  struct Sprite *radar_gfx_sprite;

  LONG black_pen;
  LONG white_pen;
  LONG yellow_pen;
  LONG cyan_pen;
  LONG red_pen;

  /* Connected Objects */
  Object *overview_object;
  Object *hscroller_object;
  Object *vscroller_object;
  Object *scrollbutton_object;

  /* Owns Object */
  Object *pop_wnd;
  Object *context_menu;

  LONG horiz_first;
  LONG vert_first;
  LONG shown;			/* if object is displayed */
  struct Hook newpos_hook;
  struct Map_Click click;	/* Clicked Tile on the Map */

  APTR drawhandle;

  struct BitMap *map_bitmap;
  struct Layer_Info *map_layerinfo;
  struct Layer *map_layer;

  struct BitMap *unit_bitmap;
  struct Layer_Info *unit_layerinfo;
  struct Layer *unit_layer;

  LONG update;			/* 1 = Tile; 2 = MoveUnit; 3 = Scroll; 4 = ShowCityNames */
  LONG x, y, width, height, write_to_screen;	/* Drawing (1) */
  LONG dest_x, dest_y, x0, y0, dx, dy;	/* Drawing (2) */
  struct unit *punit;		/* Drawing (2) */
  LONG old_horiz_first;		/* Drawing (3) */
  LONG old_vert_first;		/* Drawing (3) */
  struct unit *explode_unit; /* Drawing (5) */
  LONG mushroom_x0; /* Mushroom(6) */
  LONG mushroom_y0; /* Mushroom(6) */
  struct city *worker_pcity; /* City Workers(7) */
  LONG worker_iteratecolor; /* City Workers(7) */
  LONG worker_colors[3];
  LONG worker_colornum;
  LONG segment_src_x; /* (Un)DrawSegment (8 and 9) */
  LONG segment_src_y; /* (Un)DrawSegment (8 and 9) */
  LONG segment_dir; /* (Un)DrawSegment (8 and 9) */

  /* True if map has been showed at least once, so the scrolling can be optimized */
  LONG map_shown;
};

struct NewPosData
{
  struct Map_Data *data;
  ULONG type;
};


enum
{
  MAP_POSITION_OVERVIEW_NEWPOSITION,
  MAP_POSITION_HORIZONTAL_NEWPOSITION,
  MAP_POSITION_VERTICAL_NEWPOSITION,
  MAP_POSITION_SCROLLBUTTON_PRESSED,
  MAP_POSITION_SCROLLBUTTON_NEWPOSITION
};

HOOKPROTONH(Map_NewPosFunc, void, Object * map_object, struct NewPosData *npd)
{
  switch (npd->type)
  {
    /* The Overviewmap has been clicked */
    case  MAP_POSITION_OVERVIEW_NEWPOSITION:
	  SetAttrs(map_object,
		   MUIA_Map_HorizFirst, xget(npd->data->overview_object, MUIA_Overview_RectLeft),
		   MUIA_Map_VertFirst, xget(npd->data->overview_object, MUIA_Overview_RectTop),
		   TAG_DONE);
	  break;

    /* The horizontal scroller has been moved */
    case  MAP_POSITION_HORIZONTAL_NEWPOSITION:
	  set(map_object, MUIA_Map_HorizFirst, xget(npd->data->hscroller_object, MUIA_Prop_First));
	  break;

    /* The vertical scroller has been moved */
    case  MAP_POSITION_VERTICAL_NEWPOSITION:
	  set(map_object, MUIA_Map_VertFirst, xget(npd->data->vscroller_object, MUIA_Prop_First));
	  break;

    /* The scroll button has been pressed */
    case  MAP_POSITION_SCROLLBUTTON_PRESSED:
    	  {
	    LONG x = xget(npd->data->hscroller_object, MUIA_Prop_First);
            LONG y = xget(npd->data->vscroller_object, MUIA_Prop_First);

	    SetAttrs(npd->data->scrollbutton_object,
			MUIA_ScrollButton_XPosition, x,
			MUIA_ScrollButton_YPosition, y,
			TAG_DONE);
	  }
	  break;

    /* Mouse has been moved while the scroll button is pressed */
    case  MAP_POSITION_SCROLLBUTTON_NEWPOSITION:
    	  {
            ULONG pos = xget(npd->data->scrollbutton_object, MUIA_ScrollButton_NewPosition);
            WORD x = pos >> 16;
            WORD y = pos & 0xffff;
            SetAttrs(map_object,
		MUIA_Map_HorizFirst, x,
		MUIA_Map_VertFirst, y,
            	TAG_DONE);
          }
	  break;
  }
}


struct Command_Node
{
  struct MinNode node;
  STRPTR name;
  ULONG id;
};

struct Command_List
{
  struct MinList list;
  APTR pool;
};

static VOID Command_List_Sort(struct Command_List *list)
{
  BOOL notfinished = TRUE;

  /* Sort list (quick & dirty bubble sort) */
  while (notfinished)
  {
    struct Command_Node *first;

    /* Reset not finished flag */
    notfinished = FALSE;

    /* Get first node */
    if ((first = (struct Command_Node *) List_First(list)))
    {
      struct Command_Node *second;

      /* One bubble sort round */
      while ((second = (struct Command_Node *) Node_Next(first)))
      {
	if (strcmp(first->name, second->name) > 0)
	{
	  Remove((struct Node *) first);
	  Insert((struct List *) list, (struct Node *) first, (struct Node *) second);
	  notfinished = TRUE;
	}
	else
	  first = second;
      }
    }
  }
}


#define PACK_USERDATA(punit,command) ((punit?((punit->id+1) << 8):0) | command)
#define PACK_CITY_USERDATA(punit,command) ((punit?((punit->id+1) << 8):0) | command)
#define UNPACK_UNIT(packed) ((packed>>8)?(find_unit_by_id((packed >> 8)-1)):NULL)
#define UNPACK_CITY(packed) ((packed>>8)?(find_city_by_id((packed >> 8)-1)):NULL)
#define UNPACK_COMMAND(packed) (packed&0xff)

VOID Map_MakeContextItem(Object * menu_title, STRPTR name, ULONG udata)
{
  Object *entry = MUI_MakeObject(MUIO_Menuitem, name, NULL, MUIO_Menuitem_CopyStrings, 0);
  if (entry)
  {
    set(entry, MUIA_UserData, udata);
    DoMethod(menu_title, MUIM_Family_AddTail, entry);
  }
}

VOID Map_MakeContextBarlabel(Object * menu_title)
{
  Object *entry = MUI_MakeObject(MUIO_Menuitem, ~0, NULL, 0, 0);
  if (entry)
  {
    set(entry, MUIA_UserData, NULL);
    DoMethod(menu_title, MUIM_Family_AddTail, entry);
  }
}

struct Command_Node *Map_InsertCommand(struct Command_List *list, STRPTR name, ULONG id)
{
  struct Command_Node *node = (struct Command_Node *) AllocVec(sizeof(struct Command_Node), 0);
  if (node)
  {
    node->name = name;
    node->id = id;
    AddTail((struct List *) list, (struct Node *) node);
  }
  return node;
}

static VOID Map_ReallyShowCityDescriptions(Object *o, struct Map_Data *data)
{
  struct TextFont *new_font;
  struct RastPort *rp;

  if (!draw_city_names && !draw_city_productions)
    return;

  if((new_font = _font(o)))
  {
    int width,height,map_view_x0,map_view_y0,x,y;
    struct TextFont *org_font;
    APTR cliphandle = MUI_AddClipping(muiRenderInfo(o), _mleft(o), _mtop(o), _mwidth(o), _mheight(o));

    rp = _rp(o);

    width = xget(o, MUIA_Map_HorizVisible);
    height = xget(o, MUIA_Map_VertVisible);
    map_view_x0 = xget(o, MUIA_Map_HorizFirst);
    map_view_y0 = xget(o, MUIA_Map_VertFirst);

    GetRPAttrs(rp, RPTAG_Font, &org_font, TAG_DONE);
    SetFont(rp, new_font);

    for (y = 0; y < height; ++y)
    {
      int ry = map_view_y0 + y;
      for (x = 0; x < width; ++x)
      {
	int rx = (map_view_x0 + x) % map.xsize;
	struct city *pcity;
	if ((pcity = map_get_city(rx, ry)))
	{
	  int w,pix_x,pix_y;

	  pix_y = _mtop(o) + (y + 1) * get_normal_tile_height() + 1 + rp->TxBaseline - 2;

	  if (draw_city_names)
	  {
	    w = TextLength(rp, pcity->name, strlen(pcity->name));
	    pix_x = _mleft(o) + x * get_normal_tile_width() + get_normal_tile_width() / 2 - w / 2 + 1;

	    SetAPen(rp, data->black_pen);
	    Move(rp, pix_x, pix_y);
	    Text(rp, pcity->name, strlen(pcity->name));
	    SetAPen(rp, data->white_pen);
	    Move(rp, pix_x - 1, pix_y - 1);
	    Text(rp, pcity->name, strlen(pcity->name));
	    pix_y += rp->TxHeight;
	  }

	  if (draw_city_productions && (pcity->owner==game.player_idx))
	  {
	    int turns;
	    struct unit_type *punit_type;
	    struct impr_type *pimprovement_type;
	    static char buffer[256];

	    turns = city_turns_to_build(pcity, pcity->currently_building,
				        pcity->is_building_unit);

	    if (pcity->is_building_unit)
	    {
	      punit_type = get_unit_type(pcity->currently_building);
	      my_snprintf(buffer, sizeof(buffer), "%s %d",
			  punit_type->name, turns);
	    } else
	    {
	      pimprovement_type =
		  get_improvement_type(pcity->currently_building);
	      my_snprintf(buffer, sizeof(buffer), "%s %d",
			  pimprovement_type->name, turns);
	    }

	    w = TextLength(rp, buffer, strlen(buffer));
	    pix_x = _mleft(o) + x * get_normal_tile_width() + get_normal_tile_width() / 2 - w / 2 + 1;

	    SetAPen(rp, data->black_pen);
	    Move(rp, pix_x, pix_y);
	    Text(rp, buffer, strlen(buffer));
	    SetAPen(rp, data->white_pen);
	    Move(rp, pix_x - 1, pix_y - 1);
	    Text(rp, buffer, strlen(buffer));
	  }
	}
      }
    }
    MUI_RemoveClipping(muiRenderInfo(o), cliphandle);

    SetFont(rp, org_font);
  }
}

STATIC void Map_Priv_MoveUnit(Object *o, struct Map_Data *data)
{
  int x0, y0, dx, dy, dest_x, dest_y, w, h;
  struct unit *punit;
  static struct timer *anim_timer = NULL; 

  punit = data->punit;
  x0 = data->x0;
  y0 = data->y0;
  dx = data->dx;
  dy = data->dy;
  dest_x = data->dest_x;
  dest_y = data->dest_y;

  {
    int i, steps;
    int start_x, start_y;
    int this_x, this_y;
    int canvas_dx, canvas_dy;
    int ox,oy;

    if (is_isometric) {
      if (dx == 0) {
	canvas_dx = -NORMAL_TILE_WIDTH/2 * dy;
	canvas_dy = NORMAL_TILE_HEIGHT/2 * dy;
      } else if (dy == 0) {
	canvas_dx = NORMAL_TILE_WIDTH/2 * dx;
	canvas_dy = NORMAL_TILE_HEIGHT/2 * dx;
      } else {
	if (dx > 0) {
	  if (dy > 0) {
	    canvas_dx = 0;
	    canvas_dy = NORMAL_TILE_HEIGHT;
	  } else { /* dy < 0 */
	    canvas_dx = NORMAL_TILE_WIDTH;
	    canvas_dy = 0;
	  }
	} else { /* dx < 0 */
	  if (dy > 0) {
	    canvas_dx = -NORMAL_TILE_WIDTH;
	    canvas_dy = 0;
	  } else { /* dy < 0 */
	    canvas_dx = 0;
	    canvas_dy = -NORMAL_TILE_HEIGHT;
	  }
	}
      }
    } else {
      canvas_dx = NORMAL_TILE_WIDTH * dx;
      canvas_dy = NORMAL_TILE_HEIGHT * dy;
    }

    if (smooth_move_unit_steps < 2) {
      steps = 2;
    } else if (smooth_move_unit_steps > MAX(abs(canvas_dx), abs(canvas_dy))) {
      steps = MAX(abs(canvas_dx), abs(canvas_dy));
    } else {
      steps = smooth_move_unit_steps;
    }

    get_canvas_xy(x0, y0, &start_x, &start_y);
    if (is_isometric) {
      start_y -= NORMAL_TILE_HEIGHT/2;
    }

    this_x = start_x;
    this_y = start_y;

    ox = NORMAL_TILE_WIDTH / steps;
    oy = NORMAL_TILE_HEIGHT / steps;

    for (i = 1; i <= steps; i++) {
      anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

      w = UNIT_TILE_WIDTH + 2*ox;
      h = UNIT_TILE_HEIGHT + 2*oy;

      this_x = start_x + ((i * canvas_dx)/steps) - ox;
      this_y = start_y + ((i * canvas_dy)/steps) - oy;

      if (this_x + w > _mwidth(o)) w = _mwidth(o) - this_x;
      if (this_y + h > _mheight(o)) h = _mheight(o) - this_y;

      MyBltBitMapRastPort(data->map_bitmap, this_x, this_y,
			  data->unit_layer->rp, 0, 0, w, h, 0xc0);

      put_unit_tile(data->unit_layer->rp, punit, ox, oy);

      MyBltBitMapRastPort(data->unit_bitmap, 0, 0,
			 _rp(o), _mleft(o) + this_x, _mtop(o) + this_y, w, h, 0xc0);

      if (i < steps) {
	usleep_since_timer_start(anim_timer, 10000);
      }
    }
  }
}


static ULONG Map_New(struct IClass * cl, Object * o, struct opSet * msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
    data->newpos_hook.h_Entry = (HOOKFUNC) Map_NewPosFunc;

    set(o, MUIA_ContextMenu, 1);
  }
  return (ULONG) o;
}

static ULONG Map_Dispose(struct IClass * cl, Object * o, Msg msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  if (data->context_menu)
    MUI_DisposeObject(data->context_menu);
  return DoSuperMethodA(cl, o, msg);
}

static ULONG Map_Set(struct IClass * cl, Object * o, struct opSet * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  struct TagItem *tl = msg->ops_AttrList;
  struct TagItem *ti;

  BOOL redraw = FALSE;

  LONG new_horiz_first = data->horiz_first;
  LONG new_vert_first = data->vert_first;

  while ((ti = NextTagItem(&tl)))
  {
    switch (ti->ti_Tag)
    {
      case  MUIA_Map_HorizFirst:
	    {
	      LONG visible = xget(o, MUIA_Map_HorizVisible);
	      LONG maxx = map.xsize + visible - 1;

	      new_horiz_first = ti->ti_Data;
	      if (new_horiz_first + visible > maxx)
		new_horiz_first = maxx - visible;
	      if (new_horiz_first < 0)
	        new_horiz_first = 0;
	      redraw = TRUE;
	    }
	    break;

      case  MUIA_Map_VertFirst:
	    {
	      LONG visible = xget(o, MUIA_Map_VertVisible);
	      LONG maxy = map.ysize;

	      new_vert_first = ti->ti_Data;
	      if (new_vert_first + visible > maxy)
	        new_vert_first = maxy - visible;
	      if (new_vert_first < 0)
	        new_vert_first = 0;
	      redraw = TRUE;
	    }
	    break;

      case  MUIA_Map_Overview:
	    if ((data->overview_object = (Object *) ti->ti_Data))
	    {
	      DoMethod(data->overview_object, MUIM_Notify, MUIA_Overview_NewPos, MUIV_EveryTime, o, 4, MUIM_CallHook, &data->newpos_hook, data, MAP_POSITION_OVERVIEW_NEWPOSITION);
	      if (data->overview_object)
	      {
		SetAttrs(data->overview_object,
			 data->radar_gfx_sprite ? MUIA_Overview_RadarPicture : TAG_IGNORE, data->radar_gfx_sprite ? data->radar_gfx_sprite->picture : NULL,
			 MUIA_Overview_RectWidth, xget(o, MUIA_Map_HorizVisible),
			 MUIA_Overview_RectHeight, xget(o, MUIA_Map_VertVisible),
			 TAG_DONE);
	      }
	    }
	    break;

      case  MUIA_Map_HScroller:
	    if ((data->hscroller_object = (Object *) ti->ti_Data))
	    {
	      DoMethod(data->hscroller_object, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, o, 4, MUIM_CallHook, &data->newpos_hook, data, MAP_POSITION_HORIZONTAL_NEWPOSITION);
            }
	    break;

      case  MUIA_Map_VScroller:
	    if ((data->vscroller_object = (Object *) ti->ti_Data))
	    {
	      DoMethod(data->vscroller_object, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, o, 4, MUIM_CallHook, &data->newpos_hook, data, MAP_POSITION_VERTICAL_NEWPOSITION);
	    }
	    break;

      case  MUIA_Map_ScrollButton:
	    if ((data->scrollbutton_object = (Object *)ti->ti_Data))
	    {
	      DoMethod(data->scrollbutton_object, MUIM_Notify, MUIA_Pressed, TRUE, o, 4, MUIM_CallHook, &data->newpos_hook, data, MAP_POSITION_SCROLLBUTTON_PRESSED);
              DoMethod(data->scrollbutton_object, MUIM_Notify, MUIA_ScrollButton_NewPosition, MUIV_EveryTime, o, 4, MUIM_CallHook, &data->newpos_hook, data, MAP_POSITION_SCROLLBUTTON_NEWPOSITION);
	    }
	    break;
    }
  }

  if (redraw && get_client_state() == CLIENT_GAME_RUNNING_STATE)
  {
    data->old_horiz_first = data->horiz_first;
    data->old_vert_first = data->vert_first;
    data->horiz_first = new_horiz_first;
    data->vert_first = new_vert_first;

    nnset(data->hscroller_object, MUIA_Prop_First, data->horiz_first);
    nnset(data->vscroller_object, MUIA_Prop_First, data->vert_first);

    SetAttrs(data->overview_object,
	     MUIA_NoNotify, TRUE,
	     MUIA_Overview_RectLeft, data->horiz_first,
	     MUIA_Overview_RectTop, data->vert_first,
	     TAG_DONE);

    data->update = 3;
    MUI_Redraw(o, MADF_DRAWUPDATE);
  }

  return DoSuperMethodA(cl, o, (Msg) msg);
}

static ULONG Map_Get(struct IClass * cl, Object * o, struct opGet * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  switch (msg->opg_AttrID)
  {
  case MUIA_Map_HorizVisible:
    if (get_normal_tile_width() && data->shown)
    {
      *msg->opg_Storage = (_mwidth(o) + get_normal_tile_width() - 1) / get_normal_tile_width();
    }
    else
      *msg->opg_Storage = 0;
    break;

  case MUIA_Map_VertVisible:
    if (get_normal_tile_height() && data->shown)
    {
      *msg->opg_Storage = (_mheight(o) + get_normal_tile_height() - 1) / get_normal_tile_height();
    }
    else
      *msg->opg_Storage = 0;
    break;

  case MUIA_Map_HorizFirst:
    *msg->opg_Storage = data->horiz_first;
    break;

  case MUIA_Map_VertFirst:
    *msg->opg_Storage = data->vert_first;
    break;

    case  MUIA_Map_MouseX:
    	  {
    	    struct Window *wnd = (struct Window *)xget(_win(o),MUIA_Window_Window);
    	    if (wnd)
    	    {
    	      LONG x = wnd->MouseX - _mleft(o);
    	      if (x<0) x=0;
    	      *msg->opg_Storage = x;
    	    }
    	  }
	  break;

    case  MUIA_Map_MouseY:
    	  {
    	    struct Window *wnd = (struct Window *)xget(_win(o),MUIA_Window_Window);
    	    if (wnd)
    	    {
    	      LONG y = wnd->MouseY - _mtop(o);
    	      if (y<0) y=0;
    	      *msg->opg_Storage = y;
	    }
    	  }
	  break;

  default:
    return DoSuperMethodA(cl, o, (Msg) msg);
  }
  return 1;
}

static ULONG Map_Setup(struct IClass * cl, Object * o, Msg msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;
  APTR dh;

  if (!DoSuperMethodA(cl, o, msg))
    return FALSE;

  cm = _screen(o)->ViewPort.ColorMap;
  data->black_pen = ObtainBestPenA(cm, 0, 0, 0, NULL);
  data->white_pen = ObtainBestPenA(cm, 0xffffffff, 0xffffffff, 0xffffffff, NULL);
  data->worker_colors[0] = data->yellow_pen = ObtainBestPenA(cm, 0xffffffff, 0xffffffff, 0, NULL);
  data->worker_colors[1] = data->cyan_pen = ObtainBestPenA(cm, 0, 0xffffffff, 0xc8c8c8c8, NULL);
  data->worker_colors[2] = data->red_pen = ObtainBestPenA(cm, 0xffffffff, 0, 0, NULL);

  data->intro_gfx_sprite = load_sprite(main_intro_filename, FALSE);
  data->radar_gfx_sprite = load_sprite(minimap_intro_filename, FALSE);

  if ((dh = data->drawhandle = ObtainDrawHandle(pen_shared_map, &_screen(o)->RastPort, _screen(o)->ViewPort.ColorMap,
					   GGFX_DitherMode, DITHERMODE_NONE,
						TAG_DONE)))
  {
    if (data->overview_object)
      set(data->overview_object, MUIA_Overview_RadarPicture, data->radar_gfx_sprite->picture);

    if ((data->unit_bitmap = AllocBitMap(UNIT_TILE_WIDTH * 3, UNIT_TILE_HEIGHT * 3, GetBitMapAttr(_screen(o)->RastPort.BitMap, BMA_DEPTH), BMF_MINPLANES, _screen(o)->RastPort.BitMap)))
    {
      if ((data->unit_layerinfo = NewLayerInfo()))
      {
	InstallLayerInfoHook(data->unit_layerinfo, LAYERS_NOBACKFILL);
	if ((data->unit_layer = CreateBehindHookLayer(data->unit_layerinfo, data->unit_bitmap, 0, 0, UNIT_TILE_WIDTH * 3 - 1, UNIT_TILE_HEIGHT * 3 - 1, LAYERSIMPLE, LAYERS_NOBACKFILL, NULL)))
	{
	  if (render_sprites( /*_screen(o),*/ dh))
	  {
	    MUI_RequestIDCMP(o, IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE);
	    return TRUE;
	  }
	}
      }
    }
  }

  CoerceMethod(cl, o, MUIM_Cleanup);
  return FALSE;
}

static ULONG Map_Cleanup(struct IClass * cl, Object * o, Msg msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;

  MUI_RejectIDCMP(o, IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE);

  if (data->overview_object)
    set(data->overview_object, MUIA_Overview_RadarPicture, NULL);

  if (data->radar_gfx_sprite)
  {
    real_free_sprite(data->radar_gfx_sprite);
    data->radar_gfx_sprite = NULL;
  }
  if (data->intro_gfx_sprite)
  {
    real_free_sprite(data->intro_gfx_sprite);
    data->intro_gfx_sprite = NULL;
  }

  cleanup_sprites();
  if (data->drawhandle)
  {
    ReleaseDrawHandle(data->drawhandle);
    data->drawhandle = NULL;
  }
  if (data->unit_layer)
  {
    DeleteLayer(0, data->unit_layer);
    data->unit_layer = NULL;
    WaitBlit();
  }
  if (data->unit_layerinfo)
  {
    DisposeLayerInfo(data->unit_layerinfo);
    data->unit_layerinfo = NULL;
  }
  if (data->unit_bitmap)
  {
    FreeBitMap(data->unit_bitmap);
    data->unit_bitmap = NULL;
  }

  cm = _screen(o)->ViewPort.ColorMap;

  ReleasePen(cm, data->black_pen);
  ReleasePen(cm, data->white_pen);
  ReleasePen(cm, data->yellow_pen);
  ReleasePen(cm, data->cyan_pen);
  ReleasePen(cm, data->red_pen);

  return DoSuperMethodA(cl, o, msg);
}

static ULONG Map_AskMinMax(struct IClass * cl, Object * o, struct MUIP_AskMinMax * msg)
{
  DoSuperMethodA(cl, o, (Msg) msg);

  msg->MinMaxInfo->MinWidth += 2;
  msg->MinMaxInfo->DefWidth += 200;	/*get_normal_tile_height()*5;*/

  msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;

  msg->MinMaxInfo->MinHeight += 2;
  msg->MinMaxInfo->DefHeight += 100;	/*get_normal_tile_height()*5;*/

  msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
  return 0;
}

static ULONG Map_Show(struct IClass * cl, Object * o, Msg msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  ULONG flags;
  ULONG depth;

  DoSuperMethodA(cl, o, msg);
  data->shown = TRUE;

  flags = (GetBitMapAttr(_screen(o)->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD) ? 0 : BMF_MINPLANES;
  depth = GetBitMapAttr(_screen(o)->RastPort.BitMap, BMA_DEPTH);

  if (data->map_bitmap = AllocBitMap(_mwidth(o), _mheight(o), depth, flags, _screen(o)->RastPort.BitMap))
  {
    if ((data->map_layerinfo = NewLayerInfo()))
    {
      InstallLayerInfoHook(data->map_layerinfo, LAYERS_NOBACKFILL);
      if ((data->map_layer = CreateBehindHookLayer(data->map_layerinfo, data->map_bitmap, 0, 0, _mwidth(o) - 1, _mheight(o) - 1, LAYERSIMPLE, LAYERS_NOBACKFILL, NULL)))
      {
	if (data->hscroller_object)
	{
	  SetAttrs(data->hscroller_object,
		   MUIA_Prop_Entries, map.xsize + xget(o, MUIA_Map_HorizVisible) - 1,
		   MUIA_Prop_Visible, xget(o, MUIA_Map_HorizVisible),
		   MUIA_NoNotify, TRUE,
		   TAG_DONE);
	}

	if (data->vscroller_object)
	{
	  SetAttrs(data->vscroller_object,
		   MUIA_Prop_Entries, map.ysize,
		   MUIA_Prop_Visible, xget(o, MUIA_Map_VertVisible),
		   MUIA_NoNotify, TRUE,
		   TAG_DONE);
	}

	if (data->overview_object)
	{
	  SetAttrs(data->overview_object,
		   MUIA_Overview_RectWidth, xget(o, MUIA_Map_HorizVisible),
		   MUIA_Overview_RectHeight, xget(o, MUIA_Map_VertVisible),
		   MUIA_NoNotify, TRUE,
		   TAG_DONE);
	}
	return 1;
      }
    }
  }
  CoerceMethod(cl, o, MUIM_Hide);
  return 0;
}

static ULONG Map_Hide(struct IClass * cl, Object * o, Msg msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  if (data->map_layer)
  {
    DeleteLayer(0, data->map_layer);
    data->map_layer = NULL;
    WaitBlit();			/* DeleteLayer() may use the blitter */
  }
  if (data->map_layerinfo)
  {
    DisposeLayerInfo(data->map_layerinfo);
    data->map_layerinfo = NULL;
  }
  if (data->map_bitmap)
  {
    FreeBitMap(data->map_bitmap);
    data->map_bitmap = NULL;
  }

  data->shown = FALSE;

  return DoSuperMethodA(cl, o, (Msg) msg);
}

static ULONG Map_Draw(struct IClass * cl, Object * o, struct MUIP_Draw * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  ULONG drmd = GetDrMd(_rp(o));

  DoSuperMethodA(cl, o, (Msg) msg);

  if (get_client_state() == CLIENT_GAME_RUNNING_STATE)
  {
    BOOL drawmap = FALSE;
    if (msg->flags & MADF_DRAWUPDATE)
    {
      if (data->update == 5)
      {
      }

/*      if (data->update == 5)
      {
      	/* Explode Unit */
	static struct timer *anim_timer = NULL; 
	int i;

	int map_view_x0 = data->horiz_first;
	int map_view_y0 = data->vert_first;

	int xpix = (data->explode_unit->x - map_view_x0)*get_normal_tile_width();
	int ypix = (data->explode_unit->y - map_view_y0)*get_normal_tile_height();
	int width = get_normal_tile_width();
	int height = get_normal_tile_height();

	if (xpix < 0)
	{
	  width += xpix;
	  xpix = 0;
	}

	if (ypix < 0)
	{
	  height += ypix;
	  ypix = 0;
	}


	if (width > 0 && height > 0)
	{
	  for (i = 0; i < num_tiles_explode_unit; i++)
	  {
	    anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

            put_tile( data->unit_layer->rp, 0, 0, 0, 0, data->explode_unit->x, data->explode_unit->y, 0);
            put_unit_tile( data->unit_layer->rp, data->explode_unit, 0, 0);
            put_sprite_overlay( data->unit_layer->rp, sprites.explode.unit[i],0,0);

	    BltBitMapRastPort( data->unit_bitmap,0,0,
	  	_rp(o),_mleft(o)+xpix,_mtop(o)+ypix, width, height,0xc0);

	    usleep_since_timer_start(anim_timer, 20000);
	  }
	}

      	return 0;
      }

      if (data->update == 6)
      {
      	/* Draw Mushroom */
	int x, y, w, h;
	APTR cliphandle = MUI_AddClipping(muiRenderInfo(o), _mleft(o), _mtop(o), _mwidth(o), _mheight(o));

	for(y=0; y<3; y++) {
	  for(x=0; x<3; x++) {
/*	    int map_x = map_canvas_adjust_x(x - 1 + data->mushroom_x0) * get_normal_tile_width();
	    int map_y = map_canvas_adjust_y(y - 1 + data->mushroom_y0) * get_normal_tile_height();
	    struct Sprite *mysprite = sprites.explode.nuke[y][x];

	    put_sprite_overlay( _rp(o), mysprite, _mleft(o) + map_x, _mtop(o) + map_y);*/
	  }
	}

	TimeDelay( UNIT_VBLANK, 1,0);

	/* Restore the map */
//	x = map_canvas_adjust_x(data->mushroom_x0-1) * get_normal_tile_width();
//	y = map_canvas_adjust_y(data->mushroom_y0-1) * get_normal_tile_height();

	w = get_normal_tile_width() * 3;
	h = get_normal_tile_height() * 3;

	if (x<0) {
	  w +=x;
	  x = 0;
	}

	if (y<0) {
	  h +=y;
	  y = 0;
	}

	BltBitMapRastPort(data->map_bitmap, x, y,
			  _rp(o), _mleft(o) + x, _mtop(o) + y,
			  w, h, 0xc0);

	MUI_RemoveClipping(muiRenderInfo(o), cliphandle);
      	return 0;
      }

      if (data->update == 7)
      {
      	/* Draw City Workers */
	APTR cliphandle = MUI_AddClipping(muiRenderInfo(o), _mleft(o), _mtop(o), _mwidth(o), _mheight(o));
	int color;
	int i,j,x,y;
	struct city *pcity = data->worker_pcity;

	color = data->worker_colors[data->worker_colornum];

	if (data->worker_iteratecolor)
	{
	  data->worker_colornum++;
	  if (data->worker_colornum == 3) data->worker_colornum = 0;
	}

//	x = map_canvas_adjust_x(pcity->x);
//	y = map_canvas_adjust_y(pcity->y);

	SetAPen(_rp(o),color);

	city_map_iterate(i, j)
	{
	  enum city_tile_type t = get_worker_city(pcity, i, j);

	  if (!(i==2 && j==2))
	  {
	    int pix_x = (x + i - 2) * get_normal_tile_width() + _mleft(o);
	    int pix_y = (y + j - 2) * get_normal_tile_height() + _mtop(o);

	    if (t == C_TILE_EMPTY)
	    {
	      static unsigned short pattern[] = {0x4444,0x1111};
	      SetAfPt( _rp(o), pattern, 1);
	    } else
	    if (t == C_TILE_WORKER)
	    {
	      static unsigned short pattern[] = {0xaaaa,0x5555};
	      SetAfPt( _rp(o), pattern, 1);
	    } else continue;

	    RectFill(_rp(o),pix_x, pix_y,
	             pix_x + get_normal_tile_width() - 1,
	             pix_y + get_normal_tile_height() - 1);

	    SetAfPt( _rp(o), NULL, 0);

	  }

	  if (t == C_TILE_WORKER)
	  {
	    put_city_output_tile(_rp(o),
				 city_get_food_tile(i, j, pcity),
				 city_get_shields_tile(i, j, pcity),
				 city_get_trade_tile(i, j, pcity),
				 _mleft(o), _mtop(o), x + i - 2, y + j - 2);
	  }
	}

	MUI_RemoveClipping(muiRenderInfo(o), cliphandle);
	return 0;
      }*/

      if (data->update == 8)
      {
      	/* Draw Segment */
      	int src_x = data->segment_src_x;
      	int src_y = data->segment_src_y;
      	int dir = data->segment_dir;
	APTR cliphandle = MUI_AddClipping(muiRenderInfo(o), _mleft(o), _mtop(o), _mwidth(o), _mheight(o));

	if (is_isometric) {
	  increment_drawn(src_x, src_y, dir);
	  if (get_drawn(src_x, src_y, dir) > 1) {
	    MUI_RemoveClipping(muiRenderInfo(o), cliphandle);
	    return 0;
	  } else {
	    really_draw_segment(data->map_layer->rp, 0, 0, src_x, src_y, dir, 0);
	    really_draw_segment(_rp(o), _mleft(o), _mtop(o), src_x, src_y, dir, 0);
	  }
	} else {
	  int dest_x, dest_y;
	  dest_x = src_x + DIR_DX[dir];
	  dest_y = src_y + DIR_DY[dir];
	  assert(normalize_map_pos(&dest_x, &dest_y));

	  /* A previous line already marks the place */
	  if (get_drawn(src_x, src_y, dir)) {
	    increment_drawn(src_x, src_y, dir);
	  } else {
	   if (tile_visible_mapcanvas(src_x, src_y)) {
	      put_line(data->map_layer->rp, 0,0,src_x, src_y, dir);
              put_line(_rp(o), _mleft(o),_mtop(o),src_x, src_y, dir);
	    }
	    if (tile_visible_mapcanvas(dest_x, dest_y)) {
	      put_line(data->map_layer->rp, 0,0,dest_x, dest_y, 7-dir);
	      put_line(_rp(o), _mleft(o),_mtop(o),dest_x, dest_y, 7-dir);
	    }
	    increment_drawn(src_x, src_y, dir);
	  }
	}

	MUI_RemoveClipping(muiRenderInfo(o), cliphandle);
	return 0;
      }

      if (data->update == 9)
      {
      	/* Undraw Segment */
      	int src_x = data->segment_src_x;
      	int src_y = data->segment_src_y;
      	int dir = data->segment_dir;
	APTR cliphandle = MUI_AddClipping(muiRenderInfo(o), _mleft(o), _mtop(o), _mwidth(o), _mheight(o));

	if (is_isometric) {
	  int dest_x, dest_y;

	  dest_x = src_x + DIR_DX[dir];
	  dest_y = src_y + DIR_DY[dir];
	  assert(normalize_map_pos(&dest_x, &dest_y));

	  assert(get_drawn(src_x, src_y, dir));
	  decrement_drawn(src_x, src_y, dir);

	  /* somewhat inefficient */
	  if (!get_drawn(src_x, src_y, dir)) {
	    update_map_canvas(MIN(src_x, dest_x), MIN(src_y, dest_y),
			src_x == dest_x ? 1 : 2,
			src_y == dest_y ? 1 : 2,
			1);
	  }
	} else {
	  int dest_x = src_x + DIR_DX[dir];
	  int dest_y = src_y + DIR_DY[dir];
	  int drawn = get_drawn(src_x, src_y, dir);

	  assert(drawn > 0);
	  /* If we walk on a path twice it looks just like walking on it once. */
	  if (drawn > 1) {
	    decrement_drawn(src_x, src_y, dir);
	  } else {
	    decrement_drawn(src_x, src_y, dir);
	    refresh_tile_mapcanvas(src_x, src_y, 1); /* !! */
	    assert(normalize_map_pos(&dest_x, &dest_y));
	    refresh_tile_mapcanvas(dest_x, dest_y, 1); /* !! */
	    if (NORMAL_TILE_WIDTH%2 == 0 || NORMAL_TILE_HEIGHT%2 == 0) {
	      if (dir == 2) { /* Since the tle doesn't have a middle we draw an extra pixel
				 on the adjacent tile when drawing in this direction. */
		dest_x = src_x + 1;
		dest_y = src_y;
		assert(normalize_map_pos(&dest_x, &dest_y));
		refresh_tile_mapcanvas(dest_x, dest_y, 1); /* !! */
	      } else if (dir == 5) { /* the same */
		dest_x = src_x;
		dest_y = src_y + 1;
		assert(normalize_map_pos(&dest_x, &dest_y));
		refresh_tile_mapcanvas(dest_x, dest_y, 1); /* !! */
	      }
	    }
	  }
	}

	MUI_RemoveClipping(muiRenderInfo(o), cliphandle);
	return 0;
      }

      if (data->update == 2)
      {
	Map_Priv_MoveUnit(o,data);
      	
	/* Move the Unit Smoothly (from mapview.c) */
/*	int map_view_x0 = data->horiz_first;
	int map_view_y0 = data->vert_first;
	int i, x, y, x0 = data->x0, y0 = data->y0, dx = data->dx, dy = data->dy;
	struct unit *punit = data->punit;

	if (x0 >= map_view_x0)
	  x = (x0 - map_view_x0) * get_normal_tile_width();
	else
	  x = (map.xsize - map_view_x0 + x0) * get_normal_tile_width();

	y = (y0 - map_view_y0) * get_normal_tile_height();

	for (i = 0; i < get_normal_tile_width() / 2; i++)
	{
	  LONG x1, y1, w, h, ox, oy;
	  TimeDelay(0, 0, 1000000 / 65);

	  x += dx * 2;
	  y += dy * 2;

	  x1 = x - 2;
	  y1 = y - 2;
	  w = get_normal_tile_width() + 4;
	  h = get_normal_tile_height() + 4;
	  ox = oy = 0;

	  if (x1 < 0)
	  {
	    ox = -x1;
	    w += x1;
	    x1 = 0;
	  }

	  if (y1 < 0)
	  {
	    oy = -y1;
	    h += y1;
	    y1 = 0;
	  }

	  if (x1 + w > _mwidth(o))
	    w = _mwidth(o) - x1;
	  if (y1 + h > _mheight(o))
	    h = _mheight(o) - y1;

	  if (w > 0 && h > 0)
	  {
	    BltBitMapRastPort(data->map_bitmap, x1, y1,
			      data->unit_layer->rp, ox, oy, w, h, 0xc0);

	    put_unit_tile(data->unit_layer->rp, punit, 2, 2);

	    BltBitMapRastPort(data->unit_bitmap, ox, oy,
			 _rp(o), _mleft(o) + x1, _mtop(o) + y1, w, h, 0xc0);
	  }
	}*/


      }
/*      else
	drawmap = TRUE;*/

      if (data->update == 3) drawmap = TRUE;
      else if (data->update == 1) drawmap = TRUE;
    } else drawmap = TRUE;

    if (drawmap)
    {
      int x,y,width,height,write_to_screen;

      if ((msg->flags & MADF_DRAWUPDATE) && (data->update == 1))
      {
      	/* MUIM_Map_Refresh has been called */
	x = data->x;
	y = data->y;
	width = data->width;
	height = data->height;
	write_to_screen = data->write_to_screen;
      } else
      {
	x = xget(o,MUIA_Map_HorizFirst);
	y = xget(o,MUIA_Map_VertFirst);
	width = xget(o, MUIA_Map_HorizVisible);
	height = xget(o, MUIA_Map_VertVisible);
	write_to_screen = 1;

	/* see update_map_canvas_visible() in mapview.c */
	if (is_isometric)
	{
	  y -= height;
	  width = height = width + height;
	}

	if ((msg->flags & MADF_DRAWUPDATE) && (data->update == 3))
	{
	  /* Map has been scrolled */
/*	  int dx = data->horiz_first - data->old_horiz_first;
	  int dy = data->vert_first - data->old_vert_first;

	  if (abs(dx) < width && abs(dy) < height && data->map_shown)
	  {
	    ScrollRaster(data->map_layer->rp, dx * get_normal_tile_width(), dy * get_normal_tile_height(),
			 0, 0, _mwidth(o) - 1, _mheight(o) - 1);

	    bltall = TRUE;

	    if (abs(dx) < width && dx)
	    {
	      if (dx > 0)
	      {
		tile_x = width - dx - 1;
		if (tile_x < 0)
		  tile_x = 0;
		width = dx + 1;
	      }
	      else
		width = -dx;
	    }

	    if (abs(dy) < height && dy)
	    {
	      if (dy > 0)
	      {
		tile_y = height - dy - 1;
		if (tile_y < 0)
		  tile_y = 0;
		height = dy + 1;
	      }
	      else
		height = -dy;
	    }

	    if (dx && dy && abs(dx) < xget(o, MUIA_Map_HorizVisible) && abs(dy) < xget(o, MUIA_Map_VertVisible))
	    {
	      LONG newwidth = xget(o, MUIA_Map_HorizVisible);
	      need_put = FALSE;

	      if ((dy > 0 && dx > 0) || (dy < 0 && dx > 0))	/* dy > 0 = Nach unten */
	      {
		for (y = tile_y; y < tile_y + height; y++)
		{
		  for (x = 0; x < newwidth - width; x++)
		    put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
		}
	      }
	      else
	      {
		for (y = tile_y; y < tile_y + height; y++)
		{
		  for (x = tile_x; x < newwidth; x++)
		    put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
		}
	      }

	      height = xget(o, MUIA_Map_VertVisible);

	      for (y = 0; y < height; y++)
		for (x = tile_x; x < tile_x + width; x++)
		  put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
	    }
	  } else
	  {
	    data->map_shown = TRUE;
	  }*/
	}
      }

      if (is_isometric)
      {
	int i;
	int x_itr, y_itr;

	/*** First refresh the tiles above the area to remove the old tiles'
	     overlapping gfx ***/
	put_one_tile(/*_rp(o)*/data->map_layer->rp, x-1, y-1, D_B_LR); /* top_left corner */

	for (i=0; i<height-1; i++) { /* left side - last tile. */
	  int x1 = x - 1;
	  int y1 = y + i;
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x1, y1, D_MB_LR);
	}

	put_one_tile(/*_rp(o)*/data->map_layer->rp,x-1, y+height-1, D_TMB_R); /* last tile left side. */

	for (i=0; i<width-1; i++) { /* top side */
	  int x1 = x + i;
	  int y1 = y - 1;
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x1, y1, D_MB_LR);
	}

	if (width > 1) /* last tile top side. */
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x + width-1, y-1, D_TMB_L);
	else
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x + width-1, y-1, D_MB_L);

	/*** Now draw the tiles to be refreshed, from the top down to get the
	     overlapping areas correct ***/
	for (x_itr = x; x_itr < x+width; x_itr++) {
	  for (y_itr = y; y_itr < y+height; y_itr++) {
	   put_one_tile(/*_rp(o)*/data->map_layer->rp,x_itr, y_itr, D_FULL);
	  }
	}

	/*** Then draw the tiles underneath to refresh the parts of them that
	     overlap onto the area just drawn ***/
	put_one_tile(/*_rp(o)*/data->map_layer->rp,x, y+height, D_TM_R);  /* bottom side */
	for (i=1; i<width; i++) {
	  int x1 = x + i;
	  int y1 = y + height;
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x1, y1, D_TM_R);
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x1, y1, D_T_L);
	}

	put_one_tile(/*_rp(o)*/data->map_layer->rp,x+width, y, D_TM_L); /* right side */
	for (i=1; i < height; i++) {
	  int x1 = x + width;
	  int y1 = y + i;
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x1, y1, D_TM_L);
	  put_one_tile(/*_rp(o)*/data->map_layer->rp, x1, y1, D_T_R);
	}

	put_one_tile(/*_rp(o)*/data->map_layer->rp, x+width, y+height, D_T_LR); /* right-bottom corner */

	/*** Draw the goto line on top of the whole thing. Done last as
	     we want it completely on top. ***/
	/* Drawing is cheap; we just draw all the lines.
	   Perhaps this should be optimized, though... */
	for (x_itr = x-1; x_itr <= x+width; x_itr++) {
	  for (y_itr = y-1; y_itr <= y+height; y_itr++) {
	    int x1 = x_itr;
	    int y1 = y_itr;
	    if (normalize_map_pos(&x1, &y1)) {
	      int dir;
	      for (dir = 0; dir < 8; dir++) {
		if (get_drawn(x1, y1, dir)) {
		  int x2 = x1 + DIR_DX[dir];
	          int y2 = y1 + DIR_DY[dir];
	          if (normalize_map_pos(&x2, &y2)) {
		    really_draw_segment(data->map_layer->rp, 0, 0, x1, y1, dir, 1);
	          }
	        }
	      }
	    }
          }
        }

        /*** Lastly draw our changes to the screen. ***/
        if (write_to_screen) {
          int canvas_start_x, canvas_start_y;
          get_canvas_xy(x, y, &canvas_start_x, &canvas_start_y); /* top left corner */
          /* top left corner in isometric view */
          canvas_start_x -= height * NORMAL_TILE_WIDTH/2;

          /* because of where get_canvas_xy() sets canvas_x */
          canvas_start_x += NORMAL_TILE_WIDTH/2;

          /* And because units fill a little extra */
          canvas_start_y -= NORMAL_TILE_HEIGHT/2;

          /* here we draw a rectangle that includes the updated tiles. */
/*          gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
		      canvas_start_x, canvas_start_y,
		      canvas_start_x, canvas_start_y,
		      (height + width) * NORMAL_TILE_WIDTH/2,
		      (height + width) * NORMAL_TILE_HEIGHT/2 + NORMAL_TILE_HEIGHT/2);*/
/*	  BltBitMapRastPort(data->map_bitmap, canvas_start_x, canvas_start_y,
			      _rp(o), _mleft(o) + canvas_start_x, _mtop(o) + canvas_start_y,
			      (height + width) * NORMAL_TILE_WIDTH/2,
			      (height + width) * NORMAL_TILE_HEIGHT/2 + NORMAL_TILE_HEIGHT/2, 0xc0);*/

	  BltBitMapRastPort(data->map_bitmap, 0, 0,
			      _rp(o), _mleft(o), _mtop(o),
			      _mwidth(o),_mheight(o), 0xc0);
	}
      } else
      {
	int x_itr, y_itr;
	int canvas_x, canvas_y;

	for (y_itr=y; y_itr<y+height; y_itr++) {
	  for (x_itr=x; x_itr<x+width; x_itr++) {
	    int map_x = map_adjust_x(x_itr);
	    int map_y = y_itr; /* not adjusted;, we want to draw black tiles */

	    get_canvas_xy(map_x, map_y, &canvas_x, &canvas_y);
	    if (tile_visible_mapcanvas(map_x, map_y)) {
	      put_tile(data->map_layer->rp,map_x, map_y, canvas_x, canvas_y, 0);
	    }
	  }
	}

	if (write_to_screen) {
	  LONG pix_width = width * get_normal_tile_width();
	  LONG pix_height = height * get_normal_tile_height();

	  get_canvas_xy(x, y, &canvas_x, &canvas_y);

	  /* Should create a SafeBltBitMapRastPort() function */
          if (canvas_x < 0) {
            pix_width += canvas_x;
            canvas_x = 0;
          }
          if (canvas_y < 0) {
            pix_height += canvas_y;
            canvas_y = 0;
          }
	  if (pix_width + canvas_x > _mwidth(o)) pix_width = _mwidth(o) - canvas_x;
	  if (pix_height + canvas_y > _mheight(o)) pix_height = _mheight(o) - canvas_y;

	  if (pix_width > 0 && pix_height > 0) {
	    BltBitMapRastPort(data->map_bitmap, canvas_x, canvas_y,
			      _rp(o), _mleft(o) + canvas_x, _mtop(o) + canvas_y,
			      pix_width, pix_height, 0xc0);
	  }
	}
      }


/*
      LONG tile_x;
      LONG tile_y;
      LONG width;
      LONG height;

      LONG map_view_x0 = xget(o, MUIA_Map_HorizFirst);
      LONG map_view_y0 = xget(o, MUIA_Map_VertFirst);

      int x, y;
      BOOL bltall = FALSE;
      BOOL citynames = FALSE;
      BOOL need_put = TRUE;

      if ((msg->flags & MADF_DRAWUPDATE) && (data->update == 1))
      {
	tile_x = data->tilex;
	tile_y = data->tiley;
	width = data->width;
	height = data->height;

	if (tile_x < 0)
	{
	  width += tile_x;
	  tile_x = 0;
	}

	if (tile_y < 0)
	{
	  height += tile_y;
	  tile_y = 0;
	}

	if (width > 1 && height > 1)
	  citynames = TRUE;
      }
      else
      {
	tile_x = 0;
	tile_y = 0;
	width = xget(o, MUIA_Map_HorizVisible);
	height = xget(o, MUIA_Map_VertVisible);

	citynames = TRUE;

	if ((msg->flags & MADF_DRAWUPDATE) && (data->update == 3))
	{
	  LONG dx = data->horiz_first - data->old_horiz_first;
	  LONG dy = data->vert_first - data->old_vert_first;
	  if (abs(dx) < width && abs(dy) < height && data->map_shown)
	  {
	    ScrollRaster(data->map_layer->rp, dx * get_normal_tile_width(), dy * get_normal_tile_height(),
			 0, 0, _mwidth(o) - 1, _mheight(o) - 1);

	    bltall = TRUE;

	    if (abs(dx) < width && dx)
	    {
	      if (dx > 0)
	      {
		tile_x = width - dx - 1;
		if (tile_x < 0)
		  tile_x = 0;
		width = dx + 1;
	      }
	      else
		width = -dx;
	    }

	    if (abs(dy) < height && dy)
	    {
	      if (dy > 0)
	      {
		tile_y = height - dy - 1;
		if (tile_y < 0)
		  tile_y = 0;
		height = dy + 1;
	      }
	      else
		height = -dy;
	    }

	    if (dx && dy && abs(dx) < xget(o, MUIA_Map_HorizVisible) && abs(dy) < xget(o, MUIA_Map_VertVisible))
	    {
	      LONG newwidth = xget(o, MUIA_Map_HorizVisible);
	      need_put = FALSE;

	      if ((dy > 0 && dx > 0) || (dy < 0 && dx > 0))	/* dy > 0 = Nach unten */
	      {
		for (y = tile_y; y < tile_y + height; y++)
		{
		  for (x = 0; x < newwidth - width; x++)
		    put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
		}
	      }
	      else
	      {
		for (y = tile_y; y < tile_y + height; y++)
		{
		  for (x = tile_x; x < newwidth; x++)
		    put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
		}
	      }

	      height = xget(o, MUIA_Map_VertVisible);

	      for (y = 0; y < height; y++)
		for (x = tile_x; x < tile_x + width; x++)
		  put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
	    }
	  } else
	  {
	    data->map_shown = TRUE;
	  }
	}
      }


      if (need_put)
      {
	for (y = tile_y; y < tile_y + height; y++)
	  for (x = tile_x; x < tile_x + width; x++)
	    put_tile(data->map_layer->rp, 0, 0, x, y, map_view_x0 + x, map_view_y0 + y, 0);
      }

      x = tile_x * get_normal_tile_width();
      y = tile_y * get_normal_tile_height();

      if (!(data->update == 1 && !data->write_to_screen) || data->update != 4)
      {
	if (bltall)
	{
	  BltBitMapRastPort(data->map_bitmap, 0, 0,
		_rp(o), _mleft(o), _mtop(o), _mwidth(o), _mheight(o), 0xc0);
	}
	else
	{
	  /* Own simple cliping is a lot of faster */
	  LONG pix_width = width * get_normal_tile_width();
	  LONG pix_height = height * get_normal_tile_height();

	  /* Should create a SafeBltBitMapRastPort() function */
          if (x < 0)
          {
            pix_width += x;
            x = 0;
          }

          if (y < 0)
          {
            pix_height += y;
            y = 0;
          }

	  if (pix_width + x > _mwidth(o))
	    pix_width = _mwidth(o) - x;
	  if (pix_height + y > _mheight(o))
	    pix_height = _mheight(o) - y;

	  if (pix_width > 0 && pix_height > 0)
	  {
	    BltBitMapRastPort(data->map_bitmap, x, y,
			      _rp(o), _mleft(o) + x, _mtop(o) + y,
			      pix_width, pix_height, 0xc0);
	  }
	}
      }

      if (citynames)
      {
      	Map_ReallyShowCityDescriptions(o,data);
      }*/
    }

    data->update = 0;
  }
  else
  {
    if (data->intro_gfx_sprite && data->intro_gfx_sprite->picture)
    {
      APTR dh = ObtainDrawHandleA(pen_shared_map, _rp(o), _screen(o)->ViewPort.ColorMap, NULL);
      if (dh)
      {
	DrawPicture(dh, data->intro_gfx_sprite->picture, _mleft(o), _mtop(o),
		    GGFX_DestWidth, _mwidth(o),
		    GGFX_DestHeight, _mheight(o),
		    TAG_DONE);

	ReleaseDrawHandle(dh);
      }
    }

    data->map_shown = FALSE;
  }
  return 0;
}

static ULONG Map_HandleInput(struct IClass * cl, Object * o, struct MUIP_HandleInput * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);

  switch (msg->muikey)
  {
    case MUIKEY_LEFT:
      if (data->hscroller_object)
        set (data->hscroller_object, MUIA_Prop_First, xget(data->hscroller_object, MUIA_Prop_First)-1);
      break;

    case MUIKEY_RIGHT:
      if (data->hscroller_object)
        set (data->hscroller_object, MUIA_Prop_First, xget(data->hscroller_object, MUIA_Prop_First)+1);
      break;

    case MUIKEY_UP:
      if (data->vscroller_object)
        set (data->vscroller_object, MUIA_Prop_First, xget(data->vscroller_object, MUIA_Prop_First)-1);
      break;

    case MUIKEY_DOWN:
      if (data->vscroller_object)
        set (data->vscroller_object, MUIA_Prop_First, xget(data->vscroller_object, MUIA_Prop_First)+1);
      break;
  }


  if (msg->imsg && (get_client_state() == CLIENT_GAME_RUNNING_STATE))
  {
    UWORD qual = msg->imsg->Qualifier;

    switch (msg->imsg->Class)
    {
    case IDCMP_MOUSEMOVE:
      set(o,MUIA_Map_MouseMoved,TRUE);
      break;
    case IDCMP_MOUSEBUTTONS:
      if (msg->imsg->Code == SELECTDOWN)
      {
	if (_isinobject(msg->imsg->MouseX, msg->imsg->MouseY))
	{
          int x, y;
          get_map_xy(msg->imsg->MouseX - _mleft(o), msg->imsg->MouseY - _mtop(o), &x, &y);

	  if ((qual & IEQUALIFIER_LSHIFT) || (qual & IEQUALIFIER_RSHIFT))
	  {
	    if (!data->pop_wnd)
	    {
	      data->pop_wnd = TilePopWindowObject,
		MUIA_TilePopWindow_X, x,
		MUIA_TilePopWindow_Y, y,
		End;

	      if (data->pop_wnd)
	      {
		DoMethod(_app(o), OM_ADDMEMBER, data->pop_wnd);
		set(data->pop_wnd, MUIA_Window_Open, TRUE);
	      }
	    }
	  }
	  else if ((qual & IEQUALIFIER_CONTROL))
	  {
	    struct city *pcity;

	    if ((pcity = find_city_near_tile(x,y)))
	    {
	      DoMethod(o,MUIM_Map_PutCityWorkers,pcity,0);
	    }
	  }
	  else
	  {
	    data->click.x = x;
	    data->click.y = y;

	    set(o, MUIA_Map_Click, &data->click);
	  }
	}
      }
      else
      {
	if (msg->imsg->Code == SELECTUP)
	{
	  if (data->pop_wnd)
	  {
	    set(data->pop_wnd, MUIA_Window_Open, FALSE);
	    DoMethod(_app(o), OM_REMMEMBER, data->pop_wnd);
	    MUI_DisposeObject(data->pop_wnd);
	    data->pop_wnd = NULL;
	  }
	}
      }

      break;
    }
  }
  return 0;
}

static ULONG Map_ContextMenuBuild(struct IClass * cl, Object * o, struct MUIP_ContextMenuBuild * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  Object *context_menu = NULL;

  if (data->context_menu)
    MUI_DisposeObject(data->context_menu);

  if (get_client_state() == CLIENT_GAME_RUNNING_STATE)
  {
    if (_isinobject(msg->mx, msg->my))
    {
      struct city *pcity;
      struct unit *punit, *focus;
      struct tile *ptile;

      LONG x = msg->mx - _mleft(o);
      LONG y = msg->my - _mtop(o);
      x /= get_normal_tile_width();
      y /= get_normal_tile_height();
      x += data->horiz_first;
      y += data->vert_first;

      x = map_adjust_x(x);
      y = map_adjust_y(y);

      ptile = map_get_tile(x, y);

      if (ptile->known >= TILE_KNOWN_FOGGED)
      {
	Object *menu_title;
	static char title[256];

	pcity = map_get_city(x, y);
	punit = find_visible_unit(ptile);
	focus = get_unit_in_focus();

	if (pcity)
	  my_snprintf(title, sizeof(title), _("City %s"), pcity->name);
	else if (punit)
	  my_snprintf(title, sizeof(title), _("Unit %s"), unit_name(punit->type));
	else
	  my_snprintf(title, sizeof(title), _("Tile %s"), map_get_tile_info_text(x, y));

	context_menu = MenustripObject,
	  Child, menu_title = MenuObjectT(title),
	  End,
	  End;

	if (context_menu)
	{
	  BOOL need_barlabel = FALSE;

	  if (pcity && pcity->owner == game.player_idx)
	  {
	    Map_MakeContextItem(menu_title, _("Popup City"), PACK_CITY_USERDATA(pcity, CITY_POPUP));
	    need_barlabel = TRUE;
	  } else
	  {
	    if (punit && punit->owner == game.player_idx)
	    {
	      struct Command_List list;
	      NewList((struct List *) &list);
	      if ((list.pool = CreatePool(0, 1024, 1024)))
	      {
	      	/* Note: Must be better done (linked with the pull down menu */
		if (can_unit_build_city(punit))
		  Map_InsertCommand(&list, _("Build City"), PACK_USERDATA(punit, MENU_ORDER_BUILD_CITY));
		if (can_unit_add_to_city(punit))
		  Map_InsertCommand(&list, _("Add to City"), PACK_USERDATA(punit, MENU_ORDER_BUILD_CITY));
		if (can_unit_do_activity(punit, ACTIVITY_ROAD))
		  Map_InsertCommand(&list, _("Build Road"), PACK_USERDATA(punit, MENU_ORDER_ROAD));
		if (can_unit_do_activity(punit, ACTIVITY_RAILROAD))
		  Map_InsertCommand(&list, _("Build Railroad"), PACK_USERDATA(punit, MENU_ORDER_ROAD));
		if (can_unit_do_activity(punit, ACTIVITY_FORTRESS))
		  Map_InsertCommand(&list, _("Build Fortress"), PACK_USERDATA(punit, MENU_ORDER_FORTRESS));
		if (can_unit_do_activity(punit, ACTIVITY_AIRBASE))
		  Map_InsertCommand(&list, _("Build Airbase"), PACK_USERDATA(punit, MENU_ORDER_AIRBASE));
		if (can_unit_do_activity(punit, ACTIVITY_POLLUTION))
		  Map_InsertCommand(&list, _("Clean Pollution"), PACK_USERDATA(punit, MENU_ORDER_POLLUTION));
		if (can_unit_do_activity(punit, ACTIVITY_FALLOUT))
		  Map_InsertCommand(&list, _("Clean Nuclear Fallout"), PACK_USERDATA(punit, MENU_ORDER_FALLOUT));
		if (can_unit_do_activity(punit, ACTIVITY_FORTIFYING))
		  Map_InsertCommand(&list, _("Fortify"), PACK_USERDATA(punit, MENU_ORDER_FORTIFY));
		if (can_unit_do_activity(punit, ACTIVITY_SENTRY))
		  Map_InsertCommand(&list, _("Sentry"), PACK_USERDATA(punit, MENU_ORDER_SENTRY));
		if (can_unit_do_activity(punit, ACTIVITY_PILLAGE))
		  Map_InsertCommand(&list, _("Pillage"), PACK_USERDATA(punit, MENU_ORDER_PILLAGE));
		if (can_unit_do_auto(punit) && unit_flag(punit->type, F_SETTLERS))
		  Map_InsertCommand(&list, _("Auto Settler"), PACK_USERDATA(punit, MENU_ORDER_AUTO_SETTLER));
		if (can_unit_do_auto(punit) && !unit_flag(punit->type, F_SETTLERS))
		  Map_InsertCommand(&list, _("Auto Attack"), PACK_USERDATA(punit, MENU_ORDER_AUTO_ATTACK));
		if (can_unit_do_activity(punit, ACTIVITY_EXPLORE))
		  Map_InsertCommand(&list, _("Auto Explore"), PACK_USERDATA(punit, MENU_ORDER_AUTO_EXPLORE));
		if (can_unit_paradrop(punit))
		  Map_InsertCommand(&list, _("Paradrop"), PACK_USERDATA(punit, MENU_ORDER_POLLUTION));
		if (unit_flag(punit->type, F_NUCLEAR))
		  Map_InsertCommand(&list, _("Explode Nuclear"), PACK_USERDATA(punit, MENU_ORDER_NUKE));
		if (get_transporter_capacity(punit) > 0)
		  Map_InsertCommand(&list, _("Unload"), PACK_USERDATA(punit, MENU_ORDER_UNLOAD));
		if (is_unit_activity_on_tile(ACTIVITY_SENTRY, punit->x, punit->y))
		  Map_InsertCommand(&list, _("Wake up"), PACK_USERDATA(punit, MENU_ORDER_WAKEUP_OTHERS));
		if (punit != focus)
		  Map_InsertCommand(&list, _("Activate"), PACK_USERDATA(punit, UNIT_ACTIVATE));

		if (can_unit_do_activity(punit, ACTIVITY_IRRIGATE))
		{
		  static char irrtext[64];
		  if ((map_get_tile(punit->x, punit->y)->special & S_IRRIGATION) &&
		      player_knows_techs_with_flag(game.player_ptr, TF_FARMLAND))
		  {
		    sz_strlcpy(irrtext, _("Build Farmland"));
		  }
		  else
		    sz_strlcpy(irrtext, _("Build Irrigation"));
		  Map_InsertCommand(&list, irrtext, PACK_USERDATA(punit, MENU_ORDER_IRRIGATE));
		}

		if (can_unit_do_activity(punit, ACTIVITY_MINE))
		{
		  static char mintext[64];
		  sz_strlcpy(mintext, _("Build Mine"));
		  Map_InsertCommand(&list, mintext, PACK_USERDATA(punit, MENU_ORDER_MINE));
		}

		if (can_unit_do_activity(punit, ACTIVITY_TRANSFORM))
		{
		  static char transtext[64];
		  sz_strlcpy(transtext, _("Transform terrain"));
		  Map_InsertCommand(&list, transtext, PACK_USERDATA(punit, MENU_ORDER_TRANSFORM));
		}

		if (!IsListEmpty((struct List *) &list))
		{
		  struct Command_Node *node;

		  Command_List_Sort(&list);

		  while ((node = (struct Command_Node *) RemHead((struct List *) &list)))
		  {
		    Map_MakeContextItem(menu_title, node->name, node->id);
		  }
		  need_barlabel = TRUE;
		}

		DeletePool(list.pool);
	      }
	    }
	  }

	  if (unit_list_size(&ptile->units) > 1 && punit)
	  {
	    if (need_barlabel)
	    {
	      Map_MakeContextBarlabel(menu_title);
	    }

	    Map_MakeContextItem(menu_title, _("List all units"), PACK_USERDATA(punit, UNIT_POPUP_UNITLIST));
	    need_barlabel = TRUE;
	  }

	  if (focus)
	  {
	    if (punit != focus)
	    {
	      if (need_barlabel)
	      {
		Map_MakeContextBarlabel(menu_title);
	      }

	      if (can_unit_do_connect(focus, ACTIVITY_IDLE))
	      {
		Map_MakeContextItem(menu_title, _("Connect to this location"), PACK_USERDATA(focus, UNIT_CONNECT_TO));
	      }

	      Map_MakeContextItem(menu_title, _("Goto this location"), PACK_USERDATA(focus, UNIT_GOTOLOC));

	      data->click.x = x;
	      data->click.y = y;
	    }
	  }
	}
      }
    }
  }

  data->context_menu = context_menu;

  return (ULONG) context_menu;
}

static ULONG Map_ContextMenuChoice(struct IClass * cl, Object * o, struct MUIP_ContextMenuChoice * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  ULONG udata = muiUserData(msg->item);
  ULONG command = UNPACK_COMMAND(udata);

  switch (command)
  {
  case CITY_POPUP:
    {
      struct city *pcity = UNPACK_CITY(udata);
      if (pcity)
      {
	popup_city_dialog(pcity, 0);
      }
    }
    break;

  case UNIT_GOTOLOC:
    {
      struct unit *punit = UNPACK_UNIT(udata);
      if (punit)
      {
	send_goto_unit(punit, data->click.x, data->click.y);
      }
    }
    break;

  case UNIT_CONNECT_TO:
    {
      struct unit *punit = UNPACK_UNIT(udata);
      if (punit)
      {
        popup_unit_connect_dialog(punit, data->click.x, data->click.y);
      }
    }
    break;

  default:
    {
      struct unit *punit = UNPACK_UNIT(udata);
      if (punit)
      {
	do_unit_function(punit, command);
      }
    }
    break;
  }
  return (ULONG) NULL;
}

static ULONG Map_Refresh(struct IClass * cl, Object * o, struct MUIP_Map_Refresh * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);

  data->update = 1;
  data->x = msg->x;
  data->y = msg->y;
  data->width = msg->width;
  data->height = msg->height;
  data->write_to_screen = msg->write_to_screen;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}

static ULONG Map_MoveUnit(struct IClass * cl, Object * o, struct MUIP_Map_MoveUnit * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);

  data->punit = msg->punit;
  data->x0 = msg->x0;
  data->y0 = msg->y0;
  data->dx = msg->dx;
  data->dy = msg->dy;
  data->dest_x = msg->dest_x;
  data->dest_y = msg->dest_y;
  data->update = 2;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}

static ULONG Map_ShowCityDescriptions(struct IClass * cl, Object * o/*, Msg msg*/)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  data->update = 4;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}

static ULONG Map_PutCityWorkers(struct IClass * cl, Object * o, struct MUIP_Map_PutCityWorkers * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);

  data->update = 7;
  data->worker_pcity = msg->pcity;
  if (msg->color == -1 && msg->pcity == city_workers_display) {
    data->worker_iteratecolor = 0;
  } else data->worker_iteratecolor = 1;
  MUI_Redraw(o, MADF_DRAWUPDATE);

  return 0;
}
/*
static ULONG Map_PutCrossTile(struct IClass * cl, Object * o, struct MUIP_Map_PutCrossTile * msg)
{
  return NULL;
}
*/

static ULONG Map_DrawMushroom(struct IClass * cl, Object *o, struct MUIP_Map_DrawMushroom *msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  data->update = 6;
  data->mushroom_x0 = msg->abs_x0;
  data->mushroom_y0 = msg->abs_y0;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}

static ULONG Map_ExplodeUnit(struct IClass * cl, Object * o, struct MUIP_Map_ExplodeUnit * msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  data->update = 5;
  data->explode_unit = msg->punit;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}

static ULONG Map_DrawSegment(struct IClass *cl, Object *o, struct MUIP_Map_DrawSegment *msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  data->update = 8;
  data->segment_src_x = msg->src_x;
  data->segment_src_y = msg->src_y;
  data->segment_dir = msg->dir;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}

static ULONG Map_UndrawSegment(struct IClass *cl, Object *o, struct MUIP_Map_DrawSegment *msg)
{
  struct Map_Data *data = (struct Map_Data *) INST_DATA(cl, o);
  data->update = 9;
  data->segment_src_x = msg->src_x;
  data->segment_src_y = msg->src_y;
  data->segment_dir = msg->dir;
  MUI_Redraw(o, MADF_DRAWUPDATE);
  return 0;
}


DISPATCHERPROTO(Map_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return Map_New(cl, obj, (struct opSet *) msg);
  case OM_DISPOSE:
    return Map_Dispose(cl, obj, msg);
  case OM_GET:
    return Map_Get(cl, obj, (struct opGet *) msg);
  case OM_SET:
    return Map_Set(cl, obj, (struct opSet *) msg);
  case MUIM_Setup:
    return Map_Setup(cl, obj, msg);
  case MUIM_Cleanup:
    return Map_Cleanup(cl, obj, msg);
  case MUIM_AskMinMax:
    return Map_AskMinMax(cl, obj, (struct MUIP_AskMinMax *) msg);
  case MUIM_Show:
    return Map_Show(cl, obj, msg);
  case MUIM_Hide:
    return Map_Hide(cl, obj, msg);
  case MUIM_Draw:
    return Map_Draw(cl, obj, (struct MUIP_Draw *) msg);
  case MUIM_HandleInput:
    return Map_HandleInput(cl, obj, (struct MUIP_HandleInput *) msg);
  case MUIM_ContextMenuBuild:
    return Map_ContextMenuBuild(cl, obj, (struct MUIP_ContextMenuBuild *) msg);
  case MUIM_ContextMenuChoice:
    return Map_ContextMenuChoice(cl, obj, (struct MUIP_ContextMenuChoice *) msg);

  case MUIM_Map_Refresh:
    return Map_Refresh(cl, obj, (struct MUIP_Map_Refresh *) msg);
  case MUIM_Map_MoveUnit:
    return Map_MoveUnit(cl, obj, (struct MUIP_Map_MoveUnit *) msg);
  case MUIM_Map_ShowCityDescriptions:
    return Map_ShowCityDescriptions(cl, obj/*, msg*/);
  case MUIM_Map_PutCityWorkers:
    return Map_PutCityWorkers(cl, obj, (struct MUIP_Map_PutCityWorkers *) msg);
  case MUIM_Map_PutCrossTile:
/*    return Map_PutCrossTile(cl, obj, (struct MUIP_Map_PutCrossTile *) msg);*/
    return 0;
  case MUIM_Map_ExplodeUnit:
    return Map_ExplodeUnit(cl, obj, (struct MUIP_Map_ExplodeUnit *) msg);
  case MUIM_Map_DrawMushroom:
    return Map_DrawMushroom(cl, obj, (struct MUIP_Map_DrawMushroom *)msg);
  case MUIM_Map_DrawSegment:
    return Map_DrawSegment(cl, obj, (struct MUIP_Map_DrawSegment *)msg);
  case MUIM_Map_UndrawSegment:
    return Map_UndrawSegment(cl, obj, (struct MUIP_Map_DrawSegment *)msg);
  }

  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 CityMap Custom Class
*****************************************************************/

/**************************************************************************
...
**************************************************************************/
static void city_get_canvas_xy(int map_x, int map_y, int *canvas_x, int *canvas_y)
{
  if (is_isometric) {
    int diff_xy;

    /* The line at y=0 isometric has constant x+y=1(tiles) */
    diff_xy = (map_x + map_y) - (1);
    *canvas_y = diff_xy/2 * NORMAL_TILE_HEIGHT + (diff_xy%2) * (NORMAL_TILE_HEIGHT/2);

    /* The line at x=0 isometric has constant x-y=-3(tiles) */
    diff_xy = map_x - map_y;
    *canvas_x = (diff_xy + 3) * NORMAL_TILE_WIDTH/2;
  } else {
    *canvas_x = map_x * NORMAL_TILE_WIDTH;
    *canvas_y = map_y * NORMAL_TILE_HEIGHT;
  }
}

/**************************************************************************
...
**************************************************************************/
static void city_get_map_xy(int canvas_x, int canvas_y, int *map_x, int *map_y)
{
  if (is_isometric) {
    *map_x = -2;
    *map_y = 2;

    /* first find an equivalent position on the left side of the screen. */
    *map_x += canvas_x/NORMAL_TILE_WIDTH;
    *map_y -= canvas_x/NORMAL_TILE_WIDTH;
    canvas_x %= NORMAL_TILE_WIDTH;

    /* Then move op to the top corner. */
    *map_x += canvas_y/NORMAL_TILE_HEIGHT;
    *map_y += canvas_y/NORMAL_TILE_HEIGHT;
    canvas_y %= NORMAL_TILE_HEIGHT;

    assert(NORMAL_TILE_WIDTH == 2*NORMAL_TILE_HEIGHT);
    canvas_y *= 2; /* now we have a square. */
    if (canvas_x + canvas_y > NORMAL_TILE_WIDTH/2) (*map_x)++;
    if (canvas_x + canvas_y > 3 * NORMAL_TILE_WIDTH/2) (*map_x)++;
    if (canvas_x - canvas_y > NORMAL_TILE_WIDTH/2)  (*map_y)--;
    if (canvas_y - canvas_x > NORMAL_TILE_WIDTH/2)  (*map_y)++;
  } else {
    *map_x = canvas_x/NORMAL_TILE_WIDTH;
    *map_y = canvas_y/NORMAL_TILE_HEIGHT;
  }
}


struct MUI_CustomClass *CL_CityMap;

Object *MakeCityMap(struct city *pcity)
{
  return CityMapObject,
    MUIA_CityMap_City, pcity,
    End;
}

struct CityMap_Data
{
  struct city *pcity;
  struct Map_Click click;	/* Clicked Tile on the Map */

  ULONG red_color;
  ULONG black_color;
};


static ULONG CityMap_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    struct CityMap_Data *data = (struct CityMap_Data *) INST_DATA(cl, o);
    struct TagItem *tl = msg->ops_AttrList;
    struct TagItem *ti;

    set(o,MUIA_FillArea,FALSE);

    while ((ti = NextTagItem(&tl)))
    {
      switch (ti->ti_Tag)
      {
      case MUIA_CityMap_City:
	data->pcity = (struct city *) ti->ti_Data;
	break;
      }
    }
  }
  return (ULONG) o;
}

static ULONG CityMap_Set(struct IClass * cl, Object * o, struct opSet * msg)
{
  struct CityMap_Data *data = (struct CityMap_Data *) INST_DATA(cl, o);
  struct TagItem *tl = msg->ops_AttrList;
  struct TagItem *ti;

  while ((ti = NextTagItem(&tl)))
  {
    switch (ti->ti_Tag)
    {
    case MUIA_CityMap_City:
      {
	struct city *newcity = (struct city *) ti->ti_Data;
	if (newcity != data->pcity)
	{
	  data->pcity = newcity;
/*          MUI_Redraw(o,MADF_DRAWUPDATE); */
	}
      }
      break;
    }
  }

  return DoSuperMethodA(cl, o, (Msg) msg);
}

static ULONG CityMap_Setup(struct IClass * cl, Object * o, Msg msg)
{
  struct CityMap_Data *data = (struct CityMap_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;

  if (!DoSuperMethodA(cl, o, msg))
    return FALSE;

  cm = _screen(o)->ViewPort.ColorMap;
  data->red_color = ObtainBestPenA(cm, 0xffffffff, 0, 0, NULL);
  data->black_color = ObtainBestPenA(cm, 0, 0, 0, NULL);

  MUI_RequestIDCMP(o, IDCMP_MOUSEBUTTONS);

  return TRUE;
}

static ULONG CityMap_Cleanup(struct IClass * cl, Object * o, Msg msg)
{
  struct CityMap_Data *data = (struct CityMap_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;
  cm = _screen(o)->ViewPort.ColorMap;

  MUI_RejectIDCMP(o, IDCMP_MOUSEBUTTONS);

  ReleasePen(cm, data->red_color);
  ReleasePen(cm, data->black_color);

  DoSuperMethodA(cl, o, msg);
  return 0;
}

static ULONG CityMap_AskMinMax(struct IClass * cl, Object * o, struct MUIP_AskMinMax * msg)
{
  DoSuperMethodA(cl, o, (Msg) msg);

  if (is_isometric)
  {
    msg->MinMaxInfo->MinWidth += get_normal_tile_width() * 4;
    msg->MinMaxInfo->DefWidth += get_normal_tile_width() * 4;
    msg->MinMaxInfo->MaxWidth += get_normal_tile_width() * 4;
    msg->MinMaxInfo->MinHeight += get_normal_tile_height() * 4;
    msg->MinMaxInfo->DefHeight += get_normal_tile_height() * 4;
    msg->MinMaxInfo->MaxHeight += get_normal_tile_height() * 4;
  } else
  {
    msg->MinMaxInfo->MinWidth += get_normal_tile_width() * 5;
    msg->MinMaxInfo->DefWidth += get_normal_tile_width() * 5;
    msg->MinMaxInfo->MaxWidth += get_normal_tile_width() * 5;
    msg->MinMaxInfo->MinHeight += get_normal_tile_height() * 5;
    msg->MinMaxInfo->DefHeight += get_normal_tile_height() * 5;
    msg->MinMaxInfo->MaxHeight += get_normal_tile_height() * 5;
  }
  return 0;
}

static ULONG CityMap_Draw(struct IClass * cl, Object * o, struct MUIP_Draw * msg)
{
  struct CityMap_Data *data = (struct CityMap_Data *) INST_DATA(cl, o);
  struct RastPort *rp = _rp(o);

  DoSuperMethodA(cl, o, (Msg) msg);

  {
    int x, y;
    struct city *pcity = data->pcity;

    if (is_isometric)
    {
      /* First make it all black. */
      SetAPen(rp,data->black_color);
      RectFill(rp,_mleft(o),_mtop(o),_mright(o),_mbottom(o));

      /* This macro happens to iterate correct to draw the top tiles first,
         so getting the overlap right.
         Furthermore, we don't have to redraw fog on the part of a fogged tile
         that overlaps another fogged tile, as on the main map, as no tiles in
         the city radius can be fogged. */

      city_map_iterate(x, y) {
      	int map_x = pcity->x + x - 2;
      	int map_y = pcity->y + y - 2;
	if (normalize_map_pos(&map_x, &map_y) && tile_is_known(map_x, map_y)) {
	  int canvas_x, canvas_y;
	  city_get_canvas_xy(x, y, &canvas_x, &canvas_y);
	  put_one_tile_full(_rp(o), map_x, map_y, canvas_x + _mleft(o), canvas_y + _mtop(o), 1);
	}
      }

      /* We have to put the output afterwards or it will be covered. */
      city_map_iterate(x, y) {
	int map_x = pcity->x + x - 2;
	int map_y = pcity->y + y - 2;
	if (normalize_map_pos(&map_x, &map_y) && tile_is_known(map_x, map_y)) {
	  int canvas_x, canvas_y;
	  city_get_canvas_xy(x, y, &canvas_x, &canvas_y);
	  if (pcity->city_map[x][y]==C_TILE_WORKER) {
	    put_city_output_tile(_rp(o),
			     city_get_food_tile(x, y, pcity),
			     city_get_shields_tile(x, y, pcity), 
			     city_get_trade_tile(x, y, pcity),
			     _mleft(o) + canvas_x, _mtop(o) + canvas_y,0,0);
          }
        }
      }

      /* This sometimes will draw one of the lines on top of a city or
         unit pixmap. This should maybe be moved to put_one_tile_pixmap()
         to fix this, but maybe it wouldn't be a good idea because the
         lines would get obscured. */
      city_map_iterate(x, y) {
	int map_x = pcity->x + x - 2;
	int map_y = pcity->y + y - 2;
	if (normalize_map_pos(&map_x, &map_y) && tile_is_known(map_x, map_y)) {
	  int canvas_x, canvas_y;
	  city_get_canvas_xy(x, y, &canvas_x, &canvas_y);
	  if (pcity->city_map[x][y]==C_TILE_UNAVAILABLE) {
/*	    pixmap_frame_tile_red(pdialog->map_canvas_store,
				     canvas_x, canvas_y);*/
          }
        }
      }
    } else
    {
      for (y = 0; y < CITY_MAP_SIZE; y++)
      {
        for (x = 0; x < CITY_MAP_SIZE; x++)
        {
	  LONG tilex = pcity->x + x - CITY_MAP_SIZE / 2;
	  LONG tiley = pcity->y + y - CITY_MAP_SIZE / 2;

	  LONG x1 = _mleft(o) + x * get_normal_tile_width();
	  LONG y1 = _mtop(o) + y * get_normal_tile_height();
	  LONG x2 = x1 + get_normal_tile_width() - 1;
	  LONG y2 = y1 + get_normal_tile_height() - 1;

	  if (!(x == 0 && y == 0) && !(x == 0 && y == CITY_MAP_SIZE - 1) &&
	      !(x == CITY_MAP_SIZE - 1 && y == 0) &&
	      !(x == CITY_MAP_SIZE - 1 && y == CITY_MAP_SIZE - 1) &&
	       (tiley >= 0 && tiley < map.ysize))
	  {
	    if (tile_is_known(tilex,tiley))
	    {
	      put_tile(_rp(o), tilex, tiley, x1, y1, 1);

	      if (pcity->city_map[x][y] == C_TILE_WORKER)
	      {
	        put_city_output_tile(_rp(o),
	                             city_get_food_tile(x, y, pcity),
	                             city_get_shields_tile(x, y, pcity),
	                             city_get_trade_tile(x, y, pcity),
				     _mleft(o), _mtop(o), x, y);

	      } else
	      {
	        if (pcity->city_map[x][y] == C_TILE_UNAVAILABLE)
	        {
	          SetAPen(rp, data->red_color);
	          Move(rp, x1, y1);
	          Draw(rp, x2-1, y1);
	          Draw(rp, x2-1, y2-1);
	          Draw(rp, x1, y2-1);
	          Draw(rp, x1, y1 + 1);
	        }
	      }
	    } else
	    {
	      SetAPen(rp, data->black_color);
	      RectFill(rp, x1, y1, x2, y2);
	    }
	  } else
	  {
	    if (msg->flags & MADF_DRAWOBJECT)
	    {
	      DoMethod(o,MUIM_DrawBackground,x1,y1,x2-x1+1,y2-y1+1,x1,y1,0);
	    }
	  }
	}
      }
    }
  }
  return 0;
}

static ULONG CityMap_HandleInput(struct IClass * cl, Object * o, struct MUIP_HandleInput * msg)
{
  struct CityMap_Data *data = (struct CityMap_Data *) INST_DATA(cl, o);

  if (msg->imsg)
  {
    switch (msg->imsg->Class)
    {
    case IDCMP_MOUSEBUTTONS:
      if (msg->imsg->Code == SELECTDOWN)
      {
	if (_isinobject(msg->imsg->MouseX, msg->imsg->MouseY))
	{
	  int x,y;
	  city_get_map_xy(msg->imsg->MouseX - _mleft(o), msg->imsg->MouseY - _mtop(o), &x, &y);
	  data->click.x = x;
	  data->click.y = y;
	  set(o, MUIA_CityMap_Click, &data->click);
	}
      }
      break;
    }
  }
  return 0;
}

DISPATCHERPROTO(CityMap_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return CityMap_New(cl, obj, (struct opSet *) msg);
  case OM_SET:
    return CityMap_Set(cl, obj, (struct opSet *) msg);
  case MUIM_Setup:
    return CityMap_Setup(cl, obj, msg);
  case MUIM_Cleanup:
    return CityMap_Cleanup(cl, obj, msg);
  case MUIM_AskMinMax:
    return CityMap_AskMinMax(cl, obj, (struct MUIP_AskMinMax *) msg);
  case MUIM_Draw:
    return CityMap_Draw(cl, obj, (struct MUIP_Draw *) msg);
  case MUIM_HandleInput:
    return CityMap_HandleInput(cl, obj, (struct MUIP_HandleInput *) msg);
  case MUIM_CityMap_Refresh:
    MUI_Redraw(obj, MADF_DRAWUPDATE);
    return 0;
  }
  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 SpaceShip Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_SpaceShip;

Object *MakeSpaceShip(struct player_spaceship *ship)
{
  return SpaceShipObject,
    MUIA_SpaceShip_Ship, ship,
    End;
}

struct SpaceShip_Data
{
  struct player_spaceship *ship;
};


static ULONG SpaceShip_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    struct SpaceShip_Data *data = (struct SpaceShip_Data *) INST_DATA(cl, o);
    struct TagItem *tl = msg->ops_AttrList;
    struct TagItem *ti;

    while ((ti = NextTagItem(&tl)))
    {
      switch (ti->ti_Tag)
      {
      case MUIA_SpaceShip_Ship:
	data->ship = (struct player_spaceship *) ti->ti_Data;
	break;
      }
    }
  }
  return (ULONG) o;
}

static ULONG SpaceShip_AskMinMax(struct IClass * cl, Object * o, struct MUIP_AskMinMax * msg)
{
  LONG width, height;
  DoSuperMethodA(cl, o, (Msg) msg);

  width = sprites.spaceship.habitation->width * 7;
  height = sprites.spaceship.habitation->height * 7;

  msg->MinMaxInfo->MinWidth += width;
  msg->MinMaxInfo->DefWidth += width;
  msg->MinMaxInfo->MaxWidth += width;

  msg->MinMaxInfo->MinHeight += height;
  msg->MinMaxInfo->DefHeight += height;
  msg->MinMaxInfo->MaxHeight += height;
  return 0;
}

static ULONG SpaceShip_Draw(struct IClass * cl, Object * o, struct MUIP_Draw * msg)
{
  struct SpaceShip_Data *data = (struct SpaceShip_Data *) INST_DATA(cl, o);
  struct RastPort *rp = _rp(o);

  DoSuperMethodA(cl, o, (Msg) msg);

  {
    int i, j, k, x, y;
    struct Sprite *sprite = sprites.spaceship.habitation;	/* for size */
    struct player_spaceship *ship = data->ship;

    SetAPen(rp, 1);		/* black */
    RectFill(rp, _mleft(o), _mtop(o), _mright(o), _mbottom(o));

    for (i = 0; i < NUM_SS_MODULES; i++)
    {
      j = i / 3;
      k = i % 3;
      if ((k == 0 && j >= ship->habitation) || (k == 1 && j >= ship->life_support) || (k == 2 && j >= ship->solar_panels))
	continue;

      x = modules_info[i].x * sprite->width / 4 - sprite->width / 2;
      y = modules_info[i].y * sprite->height / 4 - sprite->height / 2;

      sprite = (k == 0 ? sprites.spaceship.habitation :
		k == 1 ? sprites.spaceship.life_support :
		sprites.spaceship.solar_panels);

      put_sprite_overlay(rp, sprite, _mleft(o) + x, _mtop(o) + y);
    }

    for (i = 0; i < NUM_SS_COMPONENTS; i++)
    {
      j = i / 2;
      k = i % 2;
      if ((k == 0 && j >= ship->fuel) || (k == 1 && j >= ship->propulsion))
	continue;

      x = components_info[i].x * sprite->width / 4 - sprite->width / 2;
      y = components_info[i].y * sprite->height / 4 - sprite->height / 2;

      sprite = (k == 0) ? sprites.spaceship.fuel : sprites.spaceship.propulsion;

      put_sprite_overlay(rp, sprite, _mleft(o) + x, _mtop(o) + y);
    }

    sprite = sprites.spaceship.structural;

    for (i = 0; i < NUM_SS_STRUCTURALS; i++)
    {
      if (!ship->structure[i])
	continue;
      x = structurals_info[i].x * sprite->width / 4 - sprite->width / 2;
      y = structurals_info[i].y * sprite->height / 4 - sprite->height / 2;

      put_sprite_overlay(rp, sprite, _mleft(o) + x, _mtop(o) + y);
    }
  }
  return 0;
}

DISPATCHERPROTO(SpaceShip_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return SpaceShip_New(cl, obj, (struct opSet *) msg);
  case MUIM_AskMinMax:
    return SpaceShip_AskMinMax(cl, obj, (struct MUIP_AskMinMax *) msg);
  case MUIM_Draw:
    return SpaceShip_Draw(cl, obj, (struct MUIP_Draw *) msg);
  }
  return (DoSuperMethodA(cl, obj, msg));
}


/****************************************************************
 Sprite Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_Sprite;

Object *MakeSprite(struct Sprite *sprite)
{
  return SpriteObject,
    MUIA_Sprite_Sprite, sprite,
    MUIA_InputMode, MUIV_InputMode_RelVerify,
    MUIA_ShowSelState, FALSE,
    End;
}

Object *MakeBorderSprite(struct Sprite *sprite)
{
  return SpriteObject,
    TextFrame,
    InnerSpacing(0,0),
    MUIA_Sprite_Sprite, sprite,
    End;
}

struct Sprite_Data
{
  struct Sprite *sprite;
  struct Sprite *overlay_sprite;
  ULONG transparent;

  ULONG bgcol;
  LONG bgpen;
  BOOL ownbg; /* TRUE if own Backgroundcolor */

  BOOL setup; /* TUR if between Setup and Cleanup */
};


static ULONG Sprite_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    struct Sprite_Data *data = (struct Sprite_Data *) INST_DATA(cl, o);
    struct TagItem *tl = msg->ops_AttrList;
    struct TagItem *ti;

    while ((ti = NextTagItem(&tl)))
    {
      switch (ti->ti_Tag)
      {
	case  MUIA_Sprite_Sprite:
	      data->sprite = (struct Sprite *) ti->ti_Data;
	      break;

	case  MUIA_Sprite_OverlaySprite:
	      data->overlay_sprite = (struct Sprite *) ti->ti_Data;
	      break;

	case  MUIA_Sprite_Transparent:
	      data->transparent = ti->ti_Data;
	      break;

	case  MUIA_Sprite_Background:
	      data->bgcol = ti->ti_Data;
	      data->ownbg = TRUE;
	      break;
      }
    }
  }
  return (ULONG) o;
}

static ULONG Sprite_Set(struct IClass * cl, Object * o, struct opSet * msg)
{
  struct Sprite_Data *data = (struct Sprite_Data *) INST_DATA(cl, o);
  struct TagItem *tl = msg->ops_AttrList;
  struct TagItem *ti;
  BOOL redraw = FALSE;
  BOOL pen_changed = FALSE;

  while ((ti = NextTagItem(&tl)))
  {
    switch (ti->ti_Tag)
    {
      case  MUIA_Sprite_Sprite:
	    data->sprite = (struct Sprite *) ti->ti_Data;
	    redraw = TRUE;
            break;

      case  MUIA_Sprite_Background:
            if (data->bgcol != ti->ti_Data)
            {
	      redraw = pen_changed = TRUE;
	      data->bgcol = ti->ti_Data;
	      data->ownbg = TRUE;
            }
	    break;
    }
  }

  if (redraw)
  {
    struct ColorMap *cm;
    LONG old_pen;

    cm = _screen(o)->ViewPort.ColorMap;
    old_pen = data->bgpen;
    if (pen_changed)
    {
      data->bgpen = ObtainBestPenA(cm,
             MAKECOLOR32(((data->bgcol >> 16)&0xff)),
             MAKECOLOR32(((data->bgcol >> 8)&0xff)),
             MAKECOLOR32((data->bgcol & 0xff)), NULL);
    }
    if (data->transparent) MUI_Redraw(o, MADF_DRAWOBJECT);
    else MUI_Redraw(o, MADF_DRAWUPDATE);

    if (old_pen != -1 && pen_changed) ReleasePen(cm,old_pen);
  }

  return DoSuperMethodA(cl, o, (Msg) msg);
}

static ULONG Sprite_Setup(struct IClass * cl, Object * o, Msg msg)
{
  struct Sprite_Data *data = (struct Sprite_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;

  if (!DoSuperMethodA(cl, o, msg))
    return FALSE;

  cm = _screen(o)->ViewPort.ColorMap;

  if (data->ownbg)
  {
    data->bgpen = ObtainBestPenA(cm,
                                 MAKECOLOR32(((data->bgcol >> 16)&0xff)),
                                 MAKECOLOR32(((data->bgcol >> 8)&0xff)),
                                 MAKECOLOR32((data->bgcol & 0xff)), NULL);
  } else data->bgpen = -1;
  return TRUE;
}

static ULONG Sprite_Cleanup(struct IClass * cl, Object * o, Msg msg)
{
  struct Sprite_Data *data = (struct Sprite_Data *) INST_DATA(cl, o);

  data->setup = FALSE;

  if (data->bgpen != -1)
  {
    struct ColorMap *cm = _screen(o)->ViewPort.ColorMap;
    ReleasePen(cm, data->bgpen);
  }

  return DoSuperMethodA(cl, o, msg);
}


static ULONG Sprite_AskMinMax(struct IClass * cl, Object * o, struct MUIP_AskMinMax * msg)
{
  struct Sprite_Data *data = (struct Sprite_Data *) INST_DATA(cl, o);
  DoSuperMethodA(cl, o, (Msg) msg);

  if (data->sprite)
  {
    msg->MinMaxInfo->MinWidth += data->sprite->width;
    msg->MinMaxInfo->DefWidth += data->sprite->width;
    msg->MinMaxInfo->MaxWidth += data->sprite->width;

    msg->MinMaxInfo->MinHeight += data->sprite->height;
    msg->MinMaxInfo->DefHeight += data->sprite->height;
    msg->MinMaxInfo->MaxHeight += data->sprite->height;
  }
  else
  {
    msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
    msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
  }
  return 0;
}

static ULONG Sprite_Draw(struct IClass * cl, Object * o, struct MUIP_Draw * msg)
{
  struct Sprite_Data *data = (struct Sprite_Data *) INST_DATA(cl, o);
  DoSuperMethodA(cl, o, (Msg) msg);

  if (data->bgpen != -1)
  {
    SetAPen(_rp(o), data->bgpen);
    RectFill(_rp(o), _mleft(o),_mtop(o),_mright(o),_mbottom(o));
  }

  if (data->transparent)
    put_sprite_overlay(_rp(o), data->sprite, _mleft(o), _mtop(o));
  else
    put_sprite(_rp(o), data->sprite, _mleft(o), _mtop(o));
  if (data->overlay_sprite)
    put_sprite_overlay(_rp(o), data->overlay_sprite, _mleft(o), _mtop(o));
  return 0;
}

DISPATCHERPROTO(Sprite_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return Sprite_New(cl, obj, (struct opSet *) msg);
  case OM_SET:
    return Sprite_Set(cl, obj, (struct opSet *) msg);
  case MUIM_Setup:
    return Sprite_Setup(cl, obj, msg);
  case MUIM_Cleanup:
    return Sprite_Cleanup(cl, obj, msg);
  case MUIM_AskMinMax:
    return Sprite_AskMinMax(cl, obj, (struct MUIP_AskMinMax *) msg);
  case MUIM_Draw:
    return Sprite_Draw(cl, obj, (struct MUIP_Draw *) msg);
  }
  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 Unit Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_Unit;

Object *MakeUnit(struct unit *punit, LONG upkeep)
{
  return UnitObject,
    MUIA_Unit_Unit, punit,
    MUIA_Unit_Upkeep, upkeep,
    MUIA_InputMode, MUIV_InputMode_RelVerify,
    End;
}

struct Unit_Data
{
  struct unit *punit;
  enum unit_activity activity;
  BOOL upkeep;
};


static ULONG Unit_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    struct Unit_Data *data = (struct Unit_Data *) INST_DATA(cl, o);
    struct TagItem *tl = msg->ops_AttrList;
    struct TagItem *ti;

    while ((ti = NextTagItem(&tl)))
    {
      switch (ti->ti_Tag)
      {
      case MUIA_Unit_Unit:
	data->punit = (struct unit *) ti->ti_Data;
	break;

      case MUIA_Unit_Upkeep:
	data->upkeep = ti->ti_Data ? TRUE : FALSE;
	break;
      }
    }
  }
  return (ULONG) o;
}

static ULONG Unit_Dispose(struct IClass * cl, Object * o, Msg msg)
{
  return DoSuperMethodA(cl, o, msg);
}

static ULONG Unit_Set(struct IClass * cl, Object * o, struct opSet * msg)
{
  struct Unit_Data *data = (struct Unit_Data *) INST_DATA(cl, o);
  struct TagItem *tl = msg->ops_AttrList;
  struct TagItem *ti;

  while ((ti = NextTagItem(&tl)))
  {
    switch (ti->ti_Tag)
    {
    case MUIA_Unit_Unit:
      {
	struct unit *punit = (struct unit *) ti->ti_Data;
	if (data->punit == punit)
	{
	  if (punit)
	  {
	    if (data->activity == punit->activity)
	      break;
	  }
	}

	data->punit = punit;
	data->activity = punit ? punit->activity : 0;
	MUI_Redraw(o, MADF_DRAWOBJECT);
      }
      break;
    }
  }

  return DoSuperMethodA(cl, o, (Msg) msg);
}

static ULONG Unit_Get(struct IClass * cl, Object * o, struct opGet * msg)
{
  struct Unit_Data *data = (struct Unit_Data *) INST_DATA(cl, o);
  if (msg->opg_AttrID == MUIA_Unit_Unit) *msg->opg_Storage = (LONG)data->punit;
  else return DoSuperMethodA(cl, o, (Msg) msg);
  return 1;
}


static ULONG Unit_Setup(struct IClass * cl, Object * o, Msg msg)
{
  if (!DoSuperMethodA(cl, o, msg))
    return FALSE;

  MUI_RequestIDCMP(o, IDCMP_MOUSEBUTTONS);

  return TRUE;

}

static ULONG Unit_Cleanup(struct IClass * cl, Object * o, Msg msg)
{
  MUI_RejectIDCMP(o, IDCMP_MOUSEBUTTONS);

  DoSuperMethodA(cl, o, msg);
  return 0;
}

static ULONG Unit_AskMinMax(struct IClass * cl, Object * o, struct MUIP_AskMinMax * msg)
{
  struct Unit_Data *data = (struct Unit_Data *) INST_DATA(cl, o);
  LONG w = get_normal_tile_width();
  LONG h = get_normal_tile_height();
  DoSuperMethodA(cl, o, (Msg) msg);

  if (data->upkeep)
    h += get_normal_tile_height() / 2;

  msg->MinMaxInfo->MinWidth += w;
  msg->MinMaxInfo->DefWidth += w;
  msg->MinMaxInfo->MaxWidth += w;

  msg->MinMaxInfo->MinHeight += h;
  msg->MinMaxInfo->DefHeight += h;
  msg->MinMaxInfo->MaxHeight += h;
  return 0;
}

static ULONG Unit_Draw(struct IClass * cl, Object * o, struct MUIP_Draw * msg)
{
  struct Unit_Data *data = (struct Unit_Data *) INST_DATA(cl, o);
  struct unit *punit = data->punit;

  DoSuperMethodA(cl, o, (Msg) msg);

  if (punit)
  {
    put_unit_tile(_rp(o), punit, _mleft(o), _mtop(o));
    if (data->upkeep)
    {
      LONG y = _mtop(o) + get_normal_tile_height();

      /* draw overlay pixmaps */
      /* FIXME:
       * For now only two food, one shield and two masks can be drawn per unit,
       * the proper way to do this is probably something like what Civ II does.
       * (One food/shield/mask drawn N times, possibly one top of itself. -- SKi */

      {				/* from put_unit_gpixmap_city_overlay */

	int upkeep_food = CLIP(0, punit->upkeep_food, 2);
	int unhappy = CLIP(0, punit->unhappiness, 2);
	int x = _mleft(o);

	/* draw overlay pixmaps */
	if (punit->upkeep > 0)
	  put_sprite_overlay_height(_rp(o), sprites.upkeep.shield, x, y, get_normal_tile_height() / 2);
	if (upkeep_food > 0)
	  put_sprite_overlay_height(_rp(o), sprites.upkeep.food[upkeep_food - 1], x, y, get_normal_tile_height() / 2);
	if (unhappy > 0)
	  put_sprite_overlay_height(_rp(o), sprites.upkeep.unhappy[unhappy - 1], x, y, get_normal_tile_height() / 2);
      }
    }
  }
  return 0;
}

DISPATCHERPROTO(Unit_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return Unit_New(cl, obj, (struct opSet *) msg);
  case OM_DISPOSE:
    return Unit_Dispose(cl, obj, msg);
  case OM_SET:
    return Unit_Set(cl, obj, (struct opSet *) msg);
  case OM_GET:
    return Unit_Get(cl, obj, (struct opGet *) msg);
  case MUIM_Setup:
    return Unit_Setup(cl, obj, msg);
  case MUIM_Cleanup:
    return Unit_Cleanup(cl, obj, msg);
  case MUIM_AskMinMax:
    return Unit_AskMinMax(cl, obj, (struct MUIP_AskMinMax *) msg);
  case MUIM_Draw:
    return Unit_Draw(cl, obj, (struct MUIP_Draw *) msg);
  }
  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 PresentUnit Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_PresentUnit;

Object *MakePresentUnit(struct unit *punit)
{
  return PresentUnitObject,
    MUIA_Unit_Unit, punit,
    MUIA_Unit_Upkeep, FALSE,
    MUIA_InputMode, MUIV_InputMode_RelVerify,
    End;
}

struct PresentUnit_Data
{
  Object *context_menu;
};


static ULONG PresentUnit_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    set(o, MUIA_ContextMenu, 1);
  }
  return (ULONG) o;
}

static ULONG PresentUnit_Dispose(struct IClass * cl, Object * o, Msg msg)
{
  struct PresentUnit_Data *data = (struct PresentUnit_Data *) INST_DATA(cl, o);
  if (data->context_menu)
    MUI_DisposeObject(data->context_menu);
  
  return DoSuperMethodA(cl, o, msg);
}

static ULONG PresentUnit_ContextMenuBuild(struct IClass * cl, Object * o, struct MUIP_ContextMenuBuild * msg)
{
  Object *context_menu = NULL;
  struct PresentUnit_Data *data = (struct PresentUnit_Data *) INST_DATA(cl, o);
  struct unit *punit;

  if (data->context_menu)
    MUI_DisposeObject(data->context_menu);

  if ((punit = (struct unit*)xget(o,MUIA_Unit_Unit)))
  {
    if (_isinobject(msg->mx, msg->my))
    {
      Object *menu_title;

      context_menu = MenustripObject,
	  Child, menu_title = MenuObjectT(unit_name(punit->type)),
	  End,
      End;

      if (context_menu)
      {
      	Object *entry;
      	struct city *pcity = map_get_city(punit->x,punit->y);

	if ((entry = MUI_MakeObject(MUIO_Menuitem, _("Activate"), NULL, MUIO_Menuitem_CopyStrings, 0)))
	{
	  set(entry, MUIA_UserData, PACK_USERDATA(punit, UNIT_ACTIVATE));
	  DoMethod(menu_title, MUIM_Family_AddTail, entry);
	}

	if ((entry = MUI_MakeObject(MUIO_Menuitem, _("Disband"), NULL, MUIO_Menuitem_CopyStrings, 0)))
	{
	  set(entry, MUIA_UserData, PACK_USERDATA(punit, MENU_ORDER_DISBAND));
	  DoMethod(menu_title, MUIM_Family_AddTail, entry);
	}

	if (punit->activity != ACTIVITY_FORTIFYING &&
	    can_unit_do_activity(punit, ACTIVITY_FORTIFYING))
	{

	  if ((entry = MUI_MakeObject(MUIO_Menuitem, _("Fortify"), NULL, MUIO_Menuitem_CopyStrings, 0)))
	  {
	    set(entry, MUIA_UserData, PACK_USERDATA(punit, MENU_ORDER_FORTIFY));
	    DoMethod(menu_title, MUIM_Family_AddTail, entry);
	  }
	}

	if (pcity && pcity->id != punit->homecity)
	{
	  if ((entry = MUI_MakeObject(MUIO_Menuitem, _("Make new homecity"), NULL, MUIO_Menuitem_CopyStrings, 0)))
	  {
	    set(entry, MUIA_UserData, PACK_USERDATA(punit, MENU_ORDER_HOMECITY));
	    DoMethod(menu_title, MUIM_Family_AddTail, entry);
	  }
	}

	if (punit->activity != ACTIVITY_SENTRY &&
	    can_unit_do_activity(punit, ACTIVITY_SENTRY))
	{
	  if (pcity && pcity->id != punit->homecity)
	  {
	    if ((entry = MUI_MakeObject(MUIO_Menuitem, _("Sentry"), NULL, MUIO_Menuitem_CopyStrings, 0)))
	    {
	      set(entry, MUIA_UserData, PACK_USERDATA(punit, MENU_ORDER_SENTRY));
	      DoMethod(menu_title, MUIM_Family_AddTail, entry);
	    }
	  }
	}

	if (can_upgrade_unittype(game.player_ptr,punit->type) != -1)
	{
	  if ((entry = MUI_MakeObject(MUIO_Menuitem, _("Upgrade"), NULL, MUIO_Menuitem_CopyStrings, 0)))
	  {
	    set(entry, MUIA_UserData, PACK_USERDATA(punit, UNIT_UPGRADE));
	    DoMethod(menu_title, MUIM_Family_AddTail, entry);
	  }
	}
      }
    }
  }

  data->context_menu = context_menu;

  return (ULONG) context_menu;
}

static ULONG PresentUnit_ContextMenuChoice(/*struct IClass * cl,*/ Object * o, struct MUIP_ContextMenuChoice * msg)
{
  ULONG udata = muiUserData(msg->item);
  ULONG command = UNPACK_COMMAND(udata);

  struct unit *punit = UNPACK_UNIT(udata);
  if (punit)
  {
    do_unit_function(punit, command);
  }
  return 0;
}


DISPATCHERPROTO(PresentUnit_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return PresentUnit_New(cl, obj, (struct opSet *) msg);
  case OM_DISPOSE:
    return PresentUnit_Dispose(cl, obj, msg);
  case MUIM_ContextMenuBuild:
    return PresentUnit_ContextMenuBuild(cl, obj, (struct MUIP_ContextMenuBuild *) msg);
  case MUIM_ContextMenuChoice:
    return PresentUnit_ContextMenuChoice(/*cl,*/ obj, (struct MUIP_ContextMenuChoice *) msg);
  }
  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 SupportedUnit Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_SupportedUnit;

Object *MakeSupportedUnit(struct unit *punit)
{
  return SupportedUnitObject,
    MUIA_Unit_Unit, punit,
    MUIA_Unit_Upkeep, TRUE,
    MUIA_InputMode, MUIV_InputMode_RelVerify,
    End;
}

struct SupportedUnit_Data
{
  Object *context_menu;
};


static ULONG SupportedUnit_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperMethodA(cl, o, (Msg) msg)))
  {
    set(o, MUIA_ContextMenu, 1);
  }
  return (ULONG) o;
}

static ULONG SupportedUnit_Dispose(struct IClass * cl, Object * o, Msg msg)
{
  struct SupportedUnit_Data *data = (struct SupportedUnit_Data *) INST_DATA(cl, o);
  if (data->context_menu)
    MUI_DisposeObject(data->context_menu);
  
  return DoSuperMethodA(cl, o, msg);
}

static ULONG SupportedUnit_ContextMenuBuild(struct IClass * cl, Object * o, struct MUIP_ContextMenuBuild * msg)
{
  Object *context_menu = NULL;
  struct SupportedUnit_Data *data = (struct SupportedUnit_Data *) INST_DATA(cl, o);
  struct unit *punit;

  if (data->context_menu)
    MUI_DisposeObject(data->context_menu);

  if ((punit = (struct unit*)xget(o,MUIA_Unit_Unit)))
  {
    if (_isinobject(msg->mx, msg->my))
    {
      Object *menu_title;

      context_menu = MenustripObject,
	  Child, menu_title = MenuObjectT(unit_name(punit->type)),
	      MUIA_Family_Child, MenuitemObject,
	          MUIA_Menuitem_Title, _("Activate"),
	          MUIA_UserData, PACK_USERDATA(punit, UNIT_ACTIVATE),
	          End,
	      MUIA_Family_Child, MenuitemObject,
	          MUIA_Menuitem_Title, _("Disband"),
	          MUIA_UserData, PACK_USERDATA(punit, MENU_ORDER_DISBAND),
	          End,
	      End,
	  End;
    }
  }

  data->context_menu = context_menu;

  return (ULONG) context_menu;
}

static ULONG SupportedUnit_ContextMenuChoice(/*struct IClass * cl,*/ Object * o, struct MUIP_ContextMenuChoice * msg)
{
  ULONG udata = muiUserData(msg->item);
  ULONG command = UNPACK_COMMAND(udata);

  struct unit *punit = UNPACK_UNIT(udata);
  if (punit)
  {
    do_unit_function(punit, command);
  }
  return 0;
}


DISPATCHERPROTO(SupportedUnit_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return SupportedUnit_New(cl, obj, (struct opSet *) msg);
  case OM_DISPOSE:
    return SupportedUnit_Dispose(cl, obj, msg);
  case MUIM_ContextMenuBuild:
    return SupportedUnit_ContextMenuBuild(cl, obj, (struct MUIP_ContextMenuBuild *) msg);
  case MUIM_ContextMenuChoice:
    return SupportedUnit_ContextMenuChoice(/*cl,*/ obj, (struct MUIP_ContextMenuChoice *) msg);
  }
  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 MyGauge Custom Class
*****************************************************************/

struct MUI_CustomClass *CL_MyGauge;

struct MyGauge_Data
{
  STRPTR info_text;
};

static ULONG MyGauge_Dispose(struct IClass *cl, Object * o, Msg msg)
{
  struct MyGauge_Data *data = (struct MyGauge_Data *) INST_DATA(cl, o);
  if (data->info_text)
    FreeVec(data->info_text);
  return DoSuperMethodA(cl, o, msg);
}

static ULONG MyGauge_SetGauge(struct IClass * cl, Object * o, struct MUIP_MyGauge_SetGauge * msg)
{
  struct MyGauge_Data *data = (struct MyGauge_Data *) INST_DATA(cl, o);
  if (data->info_text)
    FreeVec(data->info_text);
  data->info_text = StrCopy(msg->info);

  SetAttrs(o,
	   MUIA_Gauge_Max, msg->max,
	   MUIA_Gauge_Current, msg->current,
	   MUIA_Gauge_InfoText, data->info_text,
	   TAG_DONE);

  return 0;
}

DISPATCHERPROTO(MyGauge_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_DISPOSE:
    return MyGauge_Dispose(cl, obj, msg);
  case MUIM_MyGauge_SetGauge:
    return MyGauge_SetGauge(cl, obj, (struct MUIP_MyGauge_SetGauge *) msg);
  }
  return (DoSuperMethodA(cl, obj, msg));
}

/****************************************************************
 Initialize all custom classes
*****************************************************************/
BOOL create_map_class(void)
{
  if ((CL_TilePopWindow = MUI_CreateCustomClass(NULL, MUIC_Window, NULL, 4, (APTR) TilePopWindow_Dispatcher)))
    if ((CL_Map = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct Map_Data), (APTR) Map_Dispatcher)))
      if ((CL_CityMap = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct CityMap_Data), (APTR) CityMap_Dispatcher)))
	if ((CL_SpaceShip = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct SpaceShip_Data), (APTR) SpaceShip_Dispatcher)))
	  if ((CL_Sprite = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct Sprite_Data), (APTR) Sprite_Dispatcher)))
	    if ((CL_Unit = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct Unit_Data), (APTR) Unit_Dispatcher)))
	      if ((CL_PresentUnit = MUI_CreateCustomClass(NULL, NULL, CL_Unit, sizeof(struct PresentUnit_Data), (APTR) PresentUnit_Dispatcher)))
		if ((CL_SupportedUnit = MUI_CreateCustomClass(NULL, NULL, CL_Unit, sizeof(struct SupportedUnit_Data), (APTR) SupportedUnit_Dispatcher)))
		  if ((CL_MyGauge = MUI_CreateCustomClass(NULL, MUIC_Gauge, NULL, sizeof(struct MyGauge_Data), (APTR) MyGauge_Dispatcher)))
		    return TRUE;
  return FALSE;
}

/****************************************************************
 Remove all custom classes
*****************************************************************/
VOID delete_map_class(void)
{
  if (CL_MyGauge)
    MUI_DeleteCustomClass(CL_MyGauge);
  if (CL_PresentUnit)
    MUI_DeleteCustomClass(CL_PresentUnit);
  if (CL_SupportedUnit)
    MUI_DeleteCustomClass(CL_SupportedUnit);
  if (CL_Unit)
    MUI_DeleteCustomClass(CL_Unit);
  if (CL_Sprite)
    MUI_DeleteCustomClass(CL_Sprite);
  if (CL_SpaceShip)
    MUI_DeleteCustomClass(CL_SpaceShip);
  if (CL_CityMap)
    MUI_DeleteCustomClass(CL_CityMap);
  if (CL_Map)
    MUI_DeleteCustomClass(CL_Map);
  if (CL_TilePopWindow)
    MUI_DeleteCustomClass(CL_TilePopWindow);
}
