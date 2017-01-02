/* atrogue/dungeon.c - dungeon storeys, sections and rooms; map handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "dungeon.h"
#include "creature.h"
#include "random.h"
#include "stat.h"
#include "message.h"

static const char strSeconn[] = "section connector";

const char* const strMfdk[mfdkMax + 1] =
{ "nothing/rock", "wall", "creature", "barrier", "obstacle", "object",
  strSeconn, "platform", "ground feature", "ground", "mist",
  "chain", "iron ball"
};

tDude dude;
tCoordinate screen_width, screen_height, max_section_width, max_section_height;
tSectionSizeMeasure sectionsizemeasure;
tBoolean mrm_fov, mrm_light, mrm_screen, must_recalc_origin;
static tBoolean may_display_section = falsE;

static struct { tCoordinate x1, y1, w, h; } viewrect; /* screen coordinates */
static tCoordinate origin_x = -42, origin_y = -42;
  /* section square at top left corner of view */

my_enum1 enum
{ /* "material" effects */
  sekUnknown = 0, sekWater = 1, sekCreature = 2, sekCreatureDelayed = 3,
  sekSwamp = 4, sekFire = 5, sekLava = 6, sekWanderingWall = 7, sekMist = 8,
  sekFreezingLake = 9,
  /* "non-material" effects */
  sekLightCircle = 10, sekLightSwitch = 11, sekZapBack = 12, sekMapMagic = 13,
  sekRemoveTempWall = 14
} my_enum2(unsigned char) tSectionEventKind;
#define event_is_material(kind) ((kind) < sekLightCircle)

typedef tUint8 tSectionEventFlags;

typedef struct tSectionEvent
{ tTickout tickout; /* when this event shall proceed */
  struct tSectionEvent *next, *prev;
  tCoordinate x, y, /* origin of the event */
    ex1, ey1, ex2, ey2; /* further coordinates, depending on kind */
  tCreatureClass cc; /* for sekCreature and sekCreatureDelayed */
  unsigned char failure; /* how many attempts to proceed failed */
  tSectionEventKind kind; /* kind of the event */
  tSectionEventCount count; /* meaning depends on kind; e.g. "how many" */
  tSectionEventFlags flags; /* meaning depends on kind */
} tSectionEvent;

tSection* current_section;
#define TRAPPEDVAL (2)

tCoordinate map_width, map_height; /* number of squares */
static size_t mapsize, mbsize; /* number of bytes */
tMapField* currmap;
#define mapflags(x, y) (mapfieldptr(x, y)->mf)
#define M_SEEABLE(x, y) ( (m->losq > 0) && (map_br(x, y) >= BRIGHTNESS_VAGUE) )
tLightIntensity* map_brightness;
  /* "actual", "total" brightness of each field; separated for my_memclr() */
static unsigned char* mapdirt; /* aka dimtrap aka madtrip :-) */
static unsigned short mapdirtsize;
#define mapdirty(y) my_bit_test(mapdirt, y)
#define mapdirt_set(x, y) my_bit_set(mapdirt, y)
#define mapdirt_clr(y) my_bit_clear(mapdirt, y)

static __my_inline void mapdirt_all(void)
{ my_memset(mapdirt, ~0, mapdirtsize); }

static void map_reset(void)
{ tCoordinate y;
  for (y = 0; y < screen_height - 2; y++)
  { (void) move(y, 0); (void) clrtoeol(); }
  my_memclr(currmap, mapsize); mapdirt_all();
}

my_inline void map_update(const tCoordinate x, const tCoordinate y,
  const tMapFlags flags)
/* tells the map handler that the location (x, y) _might_ have to be updated on
   the screen; whether it actually _has_ to be updated depends on flags etc.
   and is decided when map_do_update_all() gets called! */
{ mapflags(x, y) |= flags; mrm_screen = truE; mapdirt_set(x, y);
}

/*@null@*/ tSeconn* location2seconn(const tCoordinate x, const tCoordinate y)
{ tSeconn* s = current_section->seconns;
  while (s != NULL)
  { if (s->flags & scfExit) goto next_s; /* ignore exit-only seconns */
    if ( (x == s->x) && (y == s->y) ) break; /* found */
    next_s: s = s->next;
  }
  return(s);
}

static chtype seconn_ch(const tCoordinate x, const tCoordinate y)
{ chtype retval;
  const tSeconn* const seconn = location2seconn(x, y);
  if (seconn == NULL) { retval = charBug | cbSeconn; goto out; }
  switch (seconn->kind)
  { case sckHoleDown: retval = 'o' | cbSeconn; break;
    case sckHoleUp: retval = 'O' | cbSeconn; break;
    default:
      switch (seconn_downup(seconn))
      { default: retval = '>' | cbSeconn; break;
        case 2: retval = '<' | cbSeconn; break;
      }
      break;
  }
  out: return(retval);
}

unsigned char sck_downup(const tSeconnKind kind)
{ unsigned char retval;
  if (sck_down(kind)) retval = 1; /* only down */
  else if (kind != sckElevator) retval = 2; /* only up */
  else /* possibly up and down (details depend on storey and tSeconnData!) */
    retval = 1 | 2;
  return(retval);
}

unsigned char seconn_downup_at(const tCoordinate x, const tCoordinate y)
{ const tSeconn* const s = location2seconn(x, y);
  return( (s != NULL) ? seconn_downup(s) : 0 );
}

const char* seconn_desc(const tSeconn* seconn, const unsigned char flags0)
{ static const char strElev[] = "magic elevator";
  static char buf[50];
  const char* retval;
  const tSeconnKind sck = seconn->kind;
  if (flags0 & 2) /* detailed text */
  { switch (sck)
    { case sckStairsDown: case sckStairsUp: retval = "stairs"; break;
      case sckLadderDown: case sckLadderUp: retval = "ladder"; break;
      case sckHoleDown: retval = "hole in the ground"; goto out;
        /*@notreached@*/ break;
      case sckHoleUp: retval = "hole in the ceiling"; goto out;
        /*@notreached@*/ break;
      case sckElevator: retval = strElev; goto out; /*@notreached@*/ break;
      default: retval = strSeconn; goto out; /*@notreached@*/ break;
    }
    sprintf(buf, "%s, %s", retval, (sck_down(sck) ? strDown : strUp));
    retval = buf;
  }
  else
  { switch (sck)
    { case sckStairsDown: case sckStairsUp: retval = "staircase"; break;
      case sckLadderDown: case sckLadderUp:
      case sckHoleDown: case sckHoleUp: retval = "hole"; break;
      case sckElevator: retval = strElev; break;
      default: retval = strSeconn; break;
    }
  }
  out: return(retval);
}

tBoolean ground_is_hard(const tCoordinate x, const tCoordinate y)
{ tBoolean retval;
  const tSquareFlags sf = squareflags_get(x, y);
  if (sf & sfPlatform) retval = truE;
  else if (sf & (sfWater | sfSwamp)) retval = falsE;
  else retval = cond2boolean(_ground_is_hard);
  return(retval);
}

tBoolean scrloc2secloc(const tCoordinate scrx, const tCoordinate scry,
  /*@out@*/ tCoordinate* _sectx, /*@out@*/ tCoordinate* _secty)
{ tBoolean retval;
  const tCoordinate x1 = viewrect.x1, y1 = viewrect.y1;
  if ( (scrx < x1) || (scrx >= x1 + viewrect.w) ||
       (scry < y1) || (scry >= y1 + viewrect.h) )
  { retval = falsE; goto out; }
  *_sectx = scrx - viewrect.x1 + origin_x;
  *_secty = scry - viewrect.y1 + origin_y;
  retval = truE;
  out: return(retval);
}

tBoolean secloc2scrloc(const tCoordinate sectx, const tCoordinate secty,
  /*@out@*/ tCoordinate* _scrx, /*@out@*/ tCoordinate* _scry)
{ tBoolean retval;
  if ( (sectx < origin_x) || (sectx >= origin_x + viewrect.w) ||
       (secty < origin_y) || (secty >= origin_y + viewrect.h) )
  { retval = falsE; goto out; }
  *_scrx = sectx - origin_x + viewrect.x1;
  *_scry = secty - origin_y + viewrect.y1;
  retval = truE;
  out: return(retval);
}

void cursor_reset_position(void)
{ tCoordinate scrx, scry;
  if ( (rogue != NULL) && (secloc2scrloc(rogue->x, rogue->y, &scrx, &scry)) )
    (void) move(scry, scrx);
  else (void) move(0, 0);
}

static __my_inline tRoomIndex section_randroom(void)
/* returns a random room index */
{ return((tRoomIndex) (randbytemod(current_section->num_rooms))); }

static void section_randlocation(/*@out@*/ tCoordinate* x,
  /*@out@*/ tCoordinate* y)
/* returns a random location anywhere in the current section */
{ (*x) = randcoord(current_section->width);
  (*y) = randcoord(current_section->height);
}

#define section_randlocation_nonshop section_randlocation /* FIXME for shops!*/

void section_randlocationinroom(/*@out@*/ tCoordinate* x, /*@out@*/
  tCoordinate* y, /*@null@*/ /*@out@*/ tRoomIndex* _r)
/* returns a random location inside a random room */
{ const tRoomIndex r = section_randroom();
  const tRoom* const ro = &(current_section->room[r]);
  (*x) = randcoordfromrange(ro->left + 1, ro->right - 1);
  (*y) = randcoordfromrange(ro->top + 1, ro->bottom - 1);
  if (_r != NULL) { *_r = r; }
}

void section_randlocationinroom_nonshop(/*@out@*/ tCoordinate* _x, /*@out@*/
  tCoordinate* _y, /*@null@*/ /*@out@*/ tRoomIndex* _r)
{ tRoomIndex r;
  do
  { section_randlocationinroom(_x, _y, &r);
  } while (current_section->room[r].kind == rokShop);
  /* CHECKME: avoid any (theoretically possible?) infinite loops! */
  if (_r != NULL) *_r = r;
}

static tBoolean section_randseconnlocation(const tBoolean important,
  /*@out@*/ tCoordinate* _x, /*@out@*/ tCoordinate* _y)
/* tries to find a random location which is appropriate for putting a section
   connection there */
{ tBoolean retval;
  tUint8 try = 0;
  tUint16 count;
  tSquareFlags forbidden = (sfFatalGround | sfPlatform | sfBarrier | sfSeconn |
    sfObstacle | sfCreature | sfObject | sfWall);
  retry: count = (important ? 50000 : 1000);
  while (--count > 0)
  { tCoordinate x, y;
    tRoomIndex r;
    tSquareFlags sflags;
    section_randlocationinroom_nonshop(&x, &y, &r);
    /* IMPLEMENTME: try to find a room in which no other seconn exists yet! */
    sflags = squareflags_get(x, y);
    if (!(sflags & forbidden)) /* found */
    { *_x = x; *_y = y; retval = truE; goto out; }
  }
  switch (try++)
  { case 0: forbidden &= ~sfObject; goto retry; /*@notreached@*/ break;
    case 1: forbidden &= ~sfCreature; goto retry; /*@notreached@*/ break;
    /* "default": let it fail */
  }
  retval = falsE; /* no appropriate location found; "rare" */
  out: return(retval);
}

void section_randdoorway(const tCoordinate currx, const tCoordinate curry,
  /*@out@*/ tCoordinate* xptr, /*@out@*/ tCoordinate* yptr)
/* returns the location of a random doorway (except the "nearest" doorway, if
   possible) of the room which is given by (currx, curry) */
{ tDoorwayIdx dwnum, selected, nearest;
  tCoordinate px, py;
  const tRoomIndex r = location2room(currx, curry);
  const tRoom* ro;
  selected = nearest = doorwayNone;
  if (r == roomNone) /* "should not happen" */
  { dwnum = 0; none: px = py = coordNone; goto out; }
  ro = &(current_section->room[r]); dwnum = ro->num_doorways;
  if (dwnum <= 0) /* no doorways exist (e.g. in a single-room section) */
  { goto none; }
  else if (dwnum == 1) selected = 0; /* no choice */
  else
  { tDoorwayIdx count;
    tCoordinate mindist = HIGHEST_ALLOWED_NUMBER;
    unsigned char try = 20;
    for (count = 0; count < dwnum; count++)
    { const tCoordinate dista = ABS(ro->doorway_x[count] - currx),
        distb = ABS(ro->doorway_y[count] - curry), /* yum... curry! :-) */
        distance = dista * dista + distb * distb;
      if (mindist > distance) { mindist = distance; nearest = count; }
    }
    do
    { selected = randbytemod(dwnum);
    } while ( (--try > 0) && (selected == nearest) );
  }
  px = ro->doorway_x[selected]; py = ro->doorway_y[selected];
  out: (*xptr) = px; (*yptr) = py;
}

static tSeconn* seconn_create(tSection* section, const tCoordinate x,
  const tCoordinate y, const tSeconnKind sck, /*@null@*/ tSeconnData* scd)
{ tSeconn* const retval = memory_allocate(sizeof(tSeconn));
  retval->section = section; retval->x = x; retval->y = y; retval->kind = sck;
  if (scd != NULL) scd->to = retval;
  else { scd = memory_allocate(sizeof(tSeconnData)); scd->from = retval; }
  retval->data = scd; scd->refcount++; retval->next = section->seconns;
  section->seconns = retval; squareflags_or(x, y, sfSeconn);
  map_update(x, y, (sck_down(sck) ? mfStairsDown : mfStairsUp));
  return(retval);
}

static tDepIdx department_create(const tDungeonDepKind ddk,
  const tStoreyNumber min_storey, const tStoreyNumber max_storey)
{ tDepIdx retval;
  tDepData *arr = dude.dd, *d;
  tDepIdx num = dude.num_dd, maxnum = dude.maxnum_dd;
  if (num >= maxnum) /* need more space */
  { if (maxnum > 200) { retval = 0; goto out; } /* a bug somewhere? */
    maxnum += 4; dude.maxnum_dd = maxnum;
    dude.dd = arr = memory_reallocate(arr, maxnum * sizeof(tDepData));
  }
  dude.num_dd++; retval = num; d = &(arr[num]); my_memclr(d, sizeof(tDepData));
  d->kind = ddk; d->min_storey = min_storey; d->max_storey = max_storey;
  out: return(retval);
}


/* Beginning of section creation code */

my_enum1 enum
{ srcEmpty = 0, srcRoom = 1, srcMaze = 2
} my_enum2(tUint8) tSectionRectangleClass;

typedef tUint8 tRectIndex;
#define MAXNUM_RECTS (MAXNUM_ROOMS)
#define rectNone (roomNone)

typedef struct
{ tCoordinate x1, y1, x2, y2; /* rectangle coordinates */
  tRectIndex western, northern; /* neighbor rectangles towards left/top */
  tSectionRectangleClass src;
  tRoomIndex room; /* roomNone for non-room rectangles */
} tSectRect; /* section rectangle; a rectangular area within a section */

static tSectRect sectrect[MAXNUM_RECTS];
static tRectIndex num_rects;

static void section_set_rectangleclass(const tSectionRectangleClass src)
/* tries to assign the (non-room) kind to a random (currently room) rectangle*/
{ tUint8 try = 2 * MAXNUM_RECTS;
  while (--try > 0)
  { const tRectIndex r = randbytemod(num_rects);
    tSectRect* const rect = &(sectrect[r]);
    if (rect->src == srcRoom) { rect->src = src; return; }
  }
  /* If we reach this point, we don't set the kind. Bad luck, don't care. */
}

static void section_calculate_rectangles(void)
{ tUint8 loopcount = 50, /* (only to definitely avoid infinite loops) */
    rectrowcount = 0, /* number of completed "rows of rectangles" */
    num_empty, num_maze;
  const tCoordinate sec_width = current_section->width,
    sec_height = current_section->height;
  tCoordinate minrow[HARD_MAXSECWIDTH], /* minimal "free" row in column */
    left SHUT_UP_COMPILER(0), right, top, bottom, defaultwidth, defaultheight,
    minwidth, maxwidth, width, minheight, maxheight, height, i,
    nncbottom; /* bottom row of nnc */
  tRectIndex norneigh[HARD_MAXSECWIDTH], /* northern neighbor, no horse :-) */
    r, nnc; /* northern-neighbor candidate */
  tBoolean is_leftmost_rect = truE;
  tStoreyNumber storey;
  tSectRect* rect;

  /* IMPLEMENTME: sections which consist of a single room (and lots of
     barriers); bonus/malus/fire/swamp/maze/... sections (e.g. when the player
     applies the Amulet)! */
  num_rects = 0;
  for (i = 0; i < sec_width; i++) { minrow[i] = 0; norneigh[i] = rectNone; }
  defaultwidth = sec_width / 7; minwidth = MAX(defaultwidth - 5, 5);
  defaultheight = sec_height / 5; minheight = MAX(defaultheight - 3, 4);

  loop: if (--loopcount <= 0) goto finish;

  /* FIXME: make sure we don't produce only one "row" of rectangles! (This can
     currently happen on terminals with about 25 lines. -- CHECKME: still?) */

  /* Calculate rectangle width */

  if (is_leftmost_rect) left = randcoord(3);
  maxwidth = MIN(defaultwidth + 5, sec_width - left - 2);
  if (maxwidth < minwidth)
  { /* No more rectangle would fit into this "row" of rectangles. */
    rectrowcount++; is_leftmost_rect = truE; goto loop;
  }
  width = randcoordfromrange(minwidth, maxwidth); right = left + width;

  /* Calculate "top row" of rectangle */

  top = 0;
  for (i = MAX(left - 2, 0); i <= MIN(right + 2, sec_width - 1); i++)
  { if (top < minrow[i]) top = minrow[i]; }
  if (top) top += 2; /* not the first row of rectangles, need more distance */
  top += randcoord(3);

  /* Calculate rectangle height */

  maxheight = MIN(defaultheight + 3, sec_height - top - 2);
  if (maxheight < minheight)
  { /* No more rectangle would fit into this section, so we're done here. */
    goto finish;
  }
  height = randcoordfromrange(minheight, maxheight);
  bottom = top + height;

  /* Store the data for this rectangle */

  rect = &(sectrect[num_rects]); rect->room = roomNone;
  rect->src = srcRoom; /* (might be changed below) */
  rect->x1 = left; rect->y1 = top; rect->x2 = right; rect->y2 = bottom;
  rect->western = (is_leftmost_rect ? rectNone : (num_rects-1)); num_rects++;

  /* Try to find the best (i.e. nearest) candidate for a northern neighbor */

  nnc = rectNone; nncbottom = -1;
  for (i = MAX(left - 2, 0); i <= MIN(right + 2, sec_width - 1); i++)
  { const tRectIndex n = norneigh[i];
    if ( (n != rectNone) && (nncbottom < sectrect[n].y2) )
    { nnc = n; nncbottom = sectrect[n].y2; } /* found a better candidate */
  }
  rect->northern = nnc;

  /* Prepare for next rectangle */

  if (num_rects < MAXNUM_RECTS)
  { for (i = left; i <= right; i++)
    { minrow[i] = bottom + 1; norneigh[i] = num_rects - 1; }
    left = right + randcoordfromrange(5, 8);
    is_leftmost_rect = falsE;
    goto loop;
  }

  /* Finish */

  finish:
  if (num_rects <= 0) /* "can't happen" */
    do_quit_msg(ecRuntime, "Could not create section");

  /* Now we know how many rectangles we finally have in this section, so we can
     decide how many of these rectangles should become rooms (default, already
     set above), empty spaces resp. mazes: */

  num_empty = num_rects / 5;
  if ( (num_empty > 1) && (randcointoss()) ) num_empty--;
  while (num_empty-- > 0) section_set_rectangleclass(srcEmpty);

  if ( ( (storey = current_storeynum) < 7 - difficulty ) ||
    (randpercent(99 - 8 * difficulty - 4 * pref_expl - (3 * storey) / 2)) )
  { num_maze = 0; }
  else
  { tUint8 hard_maxnum_maze = MIN(num_rects / 4, MAX(difficulty, 2)),
      maxnum_maze = (4 * difficulty + 2 * pref_expl + storey) / 10;
    if (maxnum_maze > hard_maxnum_maze) maxnum_maze = hard_maxnum_maze;
    if ( (difficulty < dkCrazy) && (maxnum_maze > 3) ) maxnum_maze = 3;
    if (maxnum_maze <= 1) num_maze = 1;
    else num_maze = randbytemod(maxnum_maze) + 1;
  }
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "num_maze=%d", num_maze);
  message_showdebug(debugstrbuf);
#endif
  while (num_maze-- > 0) section_set_rectangleclass(srcMaze);

  /* At last, we check whether we have created adjacent non-room rectangles of
     the same kind. We wouldn't like that, e.g. two adjacent mazes. */

  for (r = 0; r < num_rects; r++)
  { tSectionRectangleClass src;
    tBoolean vert;
    tRectIndex neighbor;
    rect = &(sectrect[r]); src = rect->src;
    if (src == srcRoom) continue; /* this rectangle can't cause problems */
    vert = truE; horvertloop:
    neighbor = ( (vert) ? (rect->northern) : (rect->western) );
    if ( (neighbor != rectNone) && (sectrect[neighbor].src == src) )
    { rect->src = srcRoom; continue; /* (needn't check other neighbor then) */
    }
    if (vert) { vert = falsE; goto horvertloop; }
  }
}

static one_caller void section_create_room(const tRectIndex reidx)
{ const tStoreyNumber storey = current_storeynum;
  tSectRect* const rect = &(sectrect[reidx]);
  const tCoordinate x1 = rect->x1, y1 = rect->y1, x2 = rect->x2, y2 = rect->y2;
  tCoordinate x, y;
  const tRoomIndex roidx = current_section->num_rooms++;
  tRoom* const ro = &(current_section->room[roidx]);
#if 0
  const tRandomByte rb = randbytemod(100);
  if (rb > 100)
  { /* IMPLEMENTME: non-rectangular rooms (change "rb > 100" as appropriate
       when that works) */
  }
  else
#endif
  { for (y = y1; y <= y2; y++)
    { for (x = x1; x <= x2; x++)
      { tSquare* const s = squareptr(x, y);
        s->lightmode = lmRefRoom; s->lightvalue = roidx; s->room = roidx;
      }
    }
    for (x = x1; x <= x2; x++)
    { squareflags_set(x, y1, sfHWall);
      squareflags_set(x, y2, sfHWall);
    }
    for (y = y1 + 1; y < y2; y++)
    { squareflags_set(x1, y, sfVWall);
      squareflags_set(x2, y, sfVWall);
      for (x = x1 + 1; x < x2; x++) squareflags_set(x, y, sfRoom);
    }
    ro->left = x1; ro->top = y1; ro->right = x2; ro->bottom = y2;
    if ( (storey >= dude.sn_water) &&
      (randpercent(10 * difficulty + pref_feat + storey / 2)) )
    { signed short maxcount = ((x2 - x1) * (y2 - y1)) / 10, _count =
        (difficulty / 2) + (storey / 10) + randbytemod(3) - 1, count = _count;
      if (count < 1) count = 1;
      else
      { if (count > maxcount) count = maxcount;
        if (count > 5) count = 5;
      }
      /* FIXME: don't put water into shop rooms! */
#if CONFIG_DEBUG
      sprintf(debugstrbuf, "scr(%d) water: %d, %d, %d", roidx, _count, count,
        maxcount);
      message_log(debugstrbuf);
#endif
      while (count-- > 0)
      { squareflags_or(randcoordfromrange(x1 + 2, x2 - 2),
          randcoordfromrange(y1 + 2, y2 - 2), sfWater);
        /* IMPLEMENTME: use water of a "nice", "non-fatal" buoyancy, at least
           in the first storeys! */
      }
    }
  }

  /* Set the "master" light intensity for this room: */
  if ( (storey < 6 - difficulty) ||
       (!__randpercent(5 * difficulty + (5 * storey) / 4)) )
  { ro->lightmode = lmEqual; ro->lightvalue = 255; }
  else
  { ro->lightmode = lmAroundPc;
    ro->lightvalue = (randpercent(10 + 5 * difficulty + 3 * storey) ? 1 : 2);
    if ( ( (difficulty <= dkNormal) || (storey < 5) ) ||
      (randpercent(70 - 10 * difficulty - 2 * storey)) )
    { ro->lightvalue++; }
  }
  rect->room = roidx;
}

static void set_tunnel(const tCoordinate x, const tCoordinate y)
{ tSquare* const s = squareptr(x, y);
  s->lightmode = lmAroundPc; s->lightvalue = 1;
  squareflags_or(x, y, sfTunnel);
  /* IMPLEMENTME: different tunnels should have different brightness: some are
     fully lit, some only a little, some are dark; sfHidden! */
}

static tCoordinate maze_x1, maze_y1, maze_x2, maze_y2;
  /* boundaries for maze creation; _global_ variables in order to save stack
     space on recursive function calls */

#define TEST(x, y) (squareflags_get((x), (y)) == sfTunnel)

static void do_create_maze(const tCoordinate x, const tCoordinate y)
{ tUint8 i;
  tDirection dir[4];
  set_tunnel(x, y); randhorvertdir_permutate();
  for (i = 0; i <= 3; i++) dir[i] = _randhorvertdir[i + 1];
  for (i = 0; i <= 3; i++)
  { const tDirectionOffset dx = xdir[dir[i]], dy = ydir[dir[i]];
    const tCoordinate nx = x + dx, ny = y + dy; /* next x and y values */
    if ( (nx < maze_x1) || (nx > maze_x2) ||
         (ny < maze_y1) || (ny > maze_y2) )
      continue; /* would leave allowed rectangle */
    if (dx != 0) /* horizontal direction */
    { if ( TEST(nx, y-1) || TEST(nx, y) || TEST(nx, y+1) || TEST(nx+dx, y) )
      { /* We would run into (or connect to) some other tunnel, so: */
         continue;
      }
    }
    else /* dx == 0, dy != 0 -- vertical direction */
    { if ( TEST(x-1, ny) || TEST(x, ny) || TEST(x+1, ny) || TEST(x, ny+dy) )
        continue;
    }
    /* Now we know that we have found good next coordinates, so we use them: */
    do_create_maze(nx, ny);
  }
}

#undef TEST

static one_caller void section_create_maze(const tRectIndex r)
{ const tCoordinate w = current_section->width, h = current_section->height;
  const tSectRect* const rect = &(sectrect[r]);
  maze_x1 = MAX(rect->x1, 1); maze_y1 = MAX(rect->y1, 1);
  maze_x2 = MIN(rect->x2, w - 2); maze_y2 = MIN(rect->y2, h - 2);
  do_create_maze(randcoordfromrange(maze_x1 + 1, maze_x2 - 1),
    randcoordfromrange(maze_y1 + 1, maze_y2 - 1));
#if CONFIG_EXDEBUG
  sprintf(debugstrbuf,"MAZE(%d,%d,%d,%d,%d)", maze_x1, maze_y1, maze_x2,
    maze_y2, mazecount);
  message_showdebug(debugstrbuf);
#endif
}

static void set_door(const tCoordinate x, const tCoordinate y)
{ const tStoreyNumber storey = current_storeynum;
  const tPercentage p = randbytemod(100), lim = (3 * storey) / 2;
  tObstacle* obstacle;
  tDoorState ds;
  _tDoorMaterial dm;
  tColorCode cc;
  if (__randpercent(1 + 10 * difficulty + 5 * pref_expl)) /* closed/locked */
    ds = (__randpercent(5 + 3 * pref_expl + storey / 2) ? dsLocked : dsClosed);
  else ds = dsOpen;
  if (p < lim)
  { dm = dmMetal; cc = (randpercent(2 * storey) ? ccGray : ccBlue); }
  else if (p < 3 * lim)
  { dm = dmStone; cc = (__randpercent(storey) ? ccBlack : ccGray); }
  else
  { dm = dmWood;
    switch (p & 7) /* just any random information; the color ain't crucial */
    { case 0: cc = ccBlack; break;
      case 1: case 2: cc = ccRed; break;
      case 3: case 4: case 5: cc = ccBrown; break;
      default: cc = ccGray; break;
    }
  }
  obstacle = section_obstacle_create(obkDoor, cc | (ds << doorstateshift) |
    (dm << doormaterialshift), x, y);
  if (squareflags_get(x, y) & sfVWall) obstacle->obsdetail |= DOOR_VERTWALL;
}

static void set_doorway(const tRoomIndex roidx, const tCoordinate x,
  const tCoordinate y)
{ const tStoreyNumber storey = current_storeynum;
  tSquareFlags sf = sfDoorway;
  if  ( (storey > 3) && (randpercent(2 * difficulty + 2 * pref_expl +
    storey / 3 + sectionsizemeasure / 2500)) )
  { sf |= sfHidden; }
  squareflags_or(x, y, sf);
  if ( (pref_feat > 0) && (storey > 3 - difficulty / 2) ) /* maybe door */
  { if ( (!(squareflags_get(x, y) & sfObstacle)) &&
      (__randpercent(10 + 3 * pref_expl + storey / 2)) )
    { set_door(x, y); }
  }
  if (roidx != roomNone)
  { tRoom* const r = &(current_section->room[roidx]);
    const tDoorwayIdx nd = r->num_doorways;
    if (nd < MAXNUM_DOORWAYS) /* store the doorway coordinates */
    { r->doorway_x[nd] = x; r->doorway_y[nd] = y; r->num_doorways++; }
  }
}

#define HV(h, v) ( (vert) ? (v) : (h) )
#define SET_ENDPOINT(tunnelcond, x, y, rect) \
  do \
  { if (tunnelcond) set_tunnel(x, y); \
    else set_doorway(sectrect[rect].room, x, y); \
  } while (0)

static one_caller void section_connect(const tCoordinate x1,
  const tCoordinate y1, const tCoordinate x2, const tCoordinate y2,
  const unsigned char flags0, const tRectIndex startrect,
  const tRectIndex endrect)
/* "flags0 & 1": whether to connect vertically (1) or horizontally (0);
   "flags0 & 2": don't set doorway at beginning square ('cause in a maze);
   "flags0 & 4": ditto for end square */
{ tBoolean vert = cond2boolean(flags0 & 1);
  tUint8 state = 0; /* for our little finite-state machine */
  enum { final_state = 9 };
  tCoordinate x = x1, y = y1,
    middle = randcoordfromrange(HV(x2, y2) + 1, HV(x1, y1) - 1);
  tDirectionOffset dx SHUT_UP_COMPILER(0), dy SHUT_UP_COMPILER(0);

  stateloop:
  switch (state)
  { case 0: /* beginning: doorway for room, tunnel for maze */
      SET_ENDPOINT(flags0 & 2, x, y, startrect);
      dx = HV(-1, 0); dy = HV(0, -1); state = 1; break;
    case 1:
      if (HV(x, y) <= middle)
      { if (HV(y, x) == HV(y2, x2)) state = 3;
        else
        { if (vert) { dy = 0; dx = SIGN(x2-x); }
          else      { dx = 0; dy = SIGN(y2-y); }
          state = 2;
        }
      }
      break;
    case 2:
      if (HV(y, x) == HV(y2, x2))
      { if (vert) { dy = -1; dx = 0; }
        else      { dx = -1; dy = 0; }
        state = 3;
      }
      break;
    case 3: if (HV(x, y) <= HV(x2, y2) - 2) { state = 4; } break;
    case 4: /* end: doorway for room, tunnel for maze */
      SET_ENDPOINT(flags0 & 4, x + HV(-1, 0), y + HV(0, -1), endrect);
      state = final_state; break;
  }
  if (state != final_state)
  { tSquareFlags flags;
    x += dx; y += dy;
    if (!is_location_valid(x, y)) return; /* "should not happen" */
    flags = squareflags_get(x, y);
    if (flags & (sfTunnel | sfDoorway)) state = final_state;
    else if (flags & sfWall)
    { if (!(flags0 & 4)) set_doorway(location2room(x, y), x, y);
      state = final_state;
    }
    else
    { tDirection _dir;
      set_tunnel(x, y);
      /* Check if we have hit another, already existing tunnel: */
      for_each_horvertdir(_dir)
      { const tDirectionOffset _dx = xdir[_dir], _dy = ydir[_dir];
        tCoordinate _x, _y;
        if ( (_dx == -dx) && (_dy == -dy) )
        { /* That's where we came from and dug a tunnel ourselves... */
          continue;
        }
        _x = x + _dx; _y = y + _dy;
        if (!is_location_valid(_x, _y)) continue;
        if (squareflags_get(_x, _y) & (sfDoorway | sfTunnel))
        { state = final_state; break; } /* hit another tunnel, so we're done */
      }
    }
    if (state != final_state) goto stateloop;
  }
}

#undef HV
#undef SET_ENDPOINT

#define CONFIG_CONNDEBUG (CONFIG_EXDEBUG)
#define NEIGHBOR(x) \
  ( (vert) ? (sectrect[x].northern) : (sectrect[x].western) )

static one_caller void section_connect_rectangles(void)
{ tRectIndex r;
  unsigned short connflags;
#if CONFIG_CONNDEBUG
  char* s = debugstrbuf;
  s += sprintf(s, "CONN");
#endif
  /* IMPLEMENTME: don't actually draw _all_ possible connections; that's only
     appropriate for dkVeryEasy... */
  for (r = 0; r < num_rects; r++)
  { tRectIndex n; /* neighbor */
    tCoordinate x1, y1, x2, y2;
    tBoolean vert;
    if (sectrect[r].src == srcEmpty) continue; /* nothing there */
    vert = truE;

    horvertloop:
    n = NEIGHBOR(r);
    if (n == rectNone) goto otherdir; /* no neighbor there */
    neighborloop:
    if (sectrect[n].src == srcEmpty)
    { const tRectIndex n0 = NEIGHBOR(n);
      if (n0 != rectNone) { n = n0; goto neighborloop; }
    }
    if (sectrect[n].src == srcEmpty)
    { /* no final neighbor there, so nothing to connect */
      goto otherdir;
    }
    /* At this point, we know that we want to dig a tunnel from rect r to n. */
#if CONFIG_CONNDEBUG
    s += sprintf(s, ";%c(%d,%d)", (vert ? 'V' : 'H'), r, n);
#endif
    if (vert)
    { x2 = randcoordfromrange(sectrect[n].x1 + 1, sectrect[n].x2 - 1);
      x1 = randcoordfromrange(sectrect[r].x1 + 1, sectrect[r].x2 - 1);
      y2 = sectrect[n].y2; y1 = sectrect[r].y1;
    }
    else
    { x2 = sectrect[n].x2; x1 = sectrect[r].x1;
      y2 = randcoordfromrange(sectrect[n].y1 + 1, sectrect[n].y2 - 1);
      y1 = randcoordfromrange(sectrect[r].y1 + 1, sectrect[r].y2 - 1);
    }
    connflags = boolean2bool(vert);
    if (sectrect[r].src == srcMaze)
    { connflags |= 2;
      if (vert)
      { const tCoordinate height = current_section->height;
        while ( (!(squareflags_get(x1, y1) & sfTunnel)) && (y1 < height - 1) )
        { y1++; }
      }
      else
      { const tCoordinate width = current_section->width;
        while ( (!(squareflags_get(x1, y1) & sfTunnel)) && (x1 < width - 1) )
        { x1++; }
      }
    }
    if (sectrect[n].src == srcMaze) connflags |= 4;
    section_connect(x1, y1, x2, y2, connflags, r, n);
    otherdir:
    if (vert) { vert = falsE; goto horvertloop; } /*now the horizontal tunnel*/
  }
#if CONFIG_CONNDEBUG
  message_showdebug(debugstrbuf);
#endif
}

#undef CONFIG_CONNDEBUG
#undef NEIGHBOR

static tSection* section_allocate(void)
{ tSection* retval;
  if (dude.num_ds >= dude.maxnum_ds)
  { if (dude.maxnum_ds < 8) dude.maxnum_ds = 8;
    else dude.maxnum_ds <<= 1;
    dude.sections = memory_reallocate(dude.sections, dude.maxnum_ds *
      sizeof(tSection*));
  }
  retval = dude.sections[dude.num_ds++] = memory_allocate(sizeof(tSection));
  return(retval);
}

static void section_create(tSection* section, const tDepIdx dep_idx,
  const tStoreyNumber storey, /*@null@*/ const tSeconn* seconn)
/* digs the new section and populates it */
{ const tCoordinate width = max_section_width, height = max_section_height;
    /* IMPLEMENTME: some different sizes, e.g. depending on section kind! */
  tCoordinate backx = coordNone, backy, x, y;
  tRectIndex r;
  tTrapsLeft traps_left = difficulty + randbytemod(2);
  const tDepData* const dd = &(dude.dd[dep_idx]);
  const tDungeonDepKind ddk = dd->kind;
#if CONFIG_EXDEBUG
  char* s = debugstrbuf;
#endif

  section->squares = memory_allocate(width * height * sizeof(tSquare));
  section->storeynum = storey; section->width = width; section->height=height;
  section->rogue_lastx = section->rogue_lasty = coordNone;
  /* section->rogue_lastroom = roomNone; */ section->dep_idx = dep_idx;
  section->traps_left = TRAPPEDVAL * zero2one(traps_left);
  section_calculate_rectangles(); /* plans the layout (like an architect) */
#if CONFIG_EXDEBUG
  s += sprintf(s, "WEST");
  for (r = 0; r < num_rects; r++)
  { if (sectrect[r].western != rectNone) s += sprintf(s, ",%d", r); }
  message_showdebug(debugstrbuf);
#endif
  for (y = 0; y < height; y++)
  { for (x = 0; x < width; x++) _squareptr(section, x, y)->room = roomNone; }
  for (r = 0; r < num_rects; r++)
  { const tSectionRectangleClass src = sectrect[r].src;
    if (src == srcRoom) section_create_room(r); /* dig a room */
    else if (src == srcMaze) section_create_maze(r); /* dig a maze */
    /* "else": leave that rectangle empty */
  }
  section_connect_rectangles(); /* digs doorways and tunnels */

  if (seconn != NULL)
  { /* we are coming from somewhere, so we should ensure that a connector back
       to that section exists */
    tSeconnData* const scd = seconn->data;
    if (scd->to == NULL)
    { if ( (section_randseconnlocation(truE, &backx, &backy)) &&
        (seconn_create(section, backx, backy, sckStairsUp, scd) != NULL) )
      { /* fine */ }
    }
  }

  /* Put some down-stairs into the section */
  if (storey < dd->max_storey)
  { if ( (section_randseconnlocation(truE, &x, &y)) &&
      (seconn_create(section, x, y, sckStairsDown, NULL) != NULL) )
    { /* fine */ }
    else /* "should not happen" */
    { message_show("BUG: could not create stairs to next storey!"); }
  }

  /* Put a shop into the section */
  if ( (ddk == ddkMain) && (pref_shops > 0) && (pref_feat > 0) &&
    (storey > dude.sn_deepest_shop + 1) )
  { tPercentage p = 10 * pref_shops - 10 * difficulty + 5 * (storey -
      dude.sn_deepest_shop);
    if (p < 10) p = 10;
    else if (p > 90) p = 90;
    if (__randpercent(p))
    { /* IMPLEMENTME! And if actually created a shop:
         dude.sn_deepest_shop = storey; */
    }
  }

  /* Put an entrance to a "dead end" department somewhere */
  if ( (ddk == ddkMain) && (pref_feat > 0) && (pref_expl >= defpref_expl) &&
    (storey > dude.sn_deepest_dead_end_entr + 5 - (pref_expl + difficulty) / 3)
    && (storey < dd->max_storey) && (randpercent(30+10*difficulty+3*storey)) )
  { tSeconn* sc;
    if ( (section_randseconnlocation(falsE, &x, &y)) &&
      ( (sc = seconn_create(section, x, y, sckStairsDown, NULL)) != NULL ) )
    { sc->data->ddk = ddkDeadEnd; dude.sn_deepest_dead_end_entr = storey; }
  }

  /* Maybe use timezip */
  if ( (ddk == ddkMain) && (pref_feat >= 7) && (storey >= 10 - difficulty) &&
    (__randpercent(5 * (difficulty + 1) + storey / 2 +
    50 * cond2bool(storey >= dude.sn_allhasted))) )
  { section->flags |= randbytefromrange(1, 3); }

  /* Add some creatures */
  if (rogue == NULL) rogue_create();
  creature_fill_section(); /* put some other creatures into this section */

  /* Maybe add some ground features */
  if ( ( (ddk == ddkMain) || (ddk == ddkDeadEnd) ) && (pref_feat > 0) )
  { if (storey > difficulty / 2)
    { /* IMPLEMENTME: fountain, sink, engraving, headstone (with certain
         probabilities, e.g. depending on storey and pref_expl and maybe role)!
      */
    }
    if (storey > 6 - difficulty)
    { /* IMPLEMENTME: add permatraps (invisible in deeper storeys), but not in
         shops! */
    }
  }

  /* Maybe add some obstacles */
  if ( ( (ddk == ddkMain) || (ddk == ddkDeadEnd) ) && (pref_feat > 0) )
  { /* IMPLEMENTME! At least put a boulder somewhere; and generate more
       boulders and obkPileOfStone for roleCaveman; generate obkMirror and
       obkStoneTable for roleWizard. */
  }
}

/* End of section creation code */


static void section_age_objects(const tTicks ticks)
/* ages all objects which are lying around in the current section */
{ const tTicks _diff = aaLie * ticks; /* not tFewTicks here - could overflow!*/
  const tFewTicks diff = (tFewTicks) (MIN(_diff, HIGHEST_ALLOWED_NUMBER));
  tObject* object = current_section->objects;
  while (object != NULL)
  { tObject* const next = object->next; /* <object> might be deallocated... */
    const tSquareFlags sflags = squareflags_get(object->x, object->y);
    tFewTicks old_lt = object->lifetime, d = diff;
    if (sflags & sfFire) d <<= 1; /* faster aging */
    else if (sflags & sfGlazedFrost) /* slower aging, but not permanently 0 */
    { d >>= 1;
      if ( (d == 0) && (ticks == 1) && ( (dungeonclock % 3) == 0 ) ) d = 1;
    }
    object_age_basic(object, ticks); /*at least ofBeingLit could be relevant!*/
    object->lifetime = LIMSUB(object->lifetime, d);
    if (!object_still_exists(object))
    { section_forget_object(object); object_deallocate(object); }
    else if ( (object->flags & ofBeingLit) &&
      (lit_value(old_lt) != lit_value(object->lifetime)) )
    { mrm_light = truE; }
    object = next;
  }
}

static void recalc_origin(void)
/* tries to keep the player character position in the middle of the view */
{ tCoordinate sw = current_section->width, sh = current_section->height,
    vw = viewrect.w, vh = viewrect.h, rx, ry, ox = origin_x, oy = origin_y;
  if ( (sw <= vw) && (sh <= vh) ) /* everything's on screen */
  { origin_x = origin_y = 0; goto finish; }
  rx = rogue->x; ry = rogue->y;
  origin_x = rx - vw / 2; origin_y = ry - vh / 2;
  if (origin_x < 0) origin_x = 0;
  else if (origin_x + vw >= sw) origin_x = sw - vw - 1;
  if (origin_y < 0) origin_y = 0;
  else if (origin_y + vh >= sh) origin_y = sh - vh - 1;
  finish: must_recalc_origin = falsE; mrm_screen = truE;
  if ( (origin_x != ox) || (origin_y != oy) ) mapdirt_all(); /* IMPROVEME! */
#if CONFIG_DEBUG
  current_section->flags &= ~secfEntering;
#endif
}

static void say_dead_end(void* data __cunused)
{ /* if (!rogue->blind) */
  { message_show( (rogue->hallucinating) ? "this feels boring" :
      "this feels like a dead end" );
  }
}

static one_caller void check_aoys(const tSection* section, tObject* pc_object)
/* removes unneeded AoYs from the section resp. creates one if appropriate */
{ tObject* secobj;
  tObjectDetail detail;
  unsigned char misscount, n, d;
  tBoolean found = falsE;
  if (pc_object == NULL) { miss_all: misscount = dude.aoy_pieces; }
  else
  { detail = pc_object->objdetail;
    if (AMULET_FAKE(detail)) goto miss_all;
    misscount = amulet_misscount(detail);
  }
  secobj = section->objects;
  while (secobj != NULL)
  { tObject* const next = secobj->next;
    if ( (secobj->oc != ocAmulet) || (secobj->kind != amkYendor) ) goto next_o;
    found = truE; /* even a _fake_ amulet shall "count" in this regard */
    detail = secobj->objdetail;
    if (AMULET_FAKE(detail)) goto next_o; /* no need to remove fakes */
    AMULET_ND(detail, n, d);
    if (n > misscount) /* must do something */
    { if (misscount <= 0) /* remove it entirely */
      { section_forget_object(secobj); object_deallocate(secobj); }
      else /* make it smaller */
      { secobj->objdetail = (detail & (~_AMULET_NMASK)) | (misscount <<
          _AMULET_NSHIFT);
        misscount = 0;
      }
    }
    else misscount -= n;
    next_o: secobj = next;
  }
  if ( (!found) && (misscount > 0) && (dude.dd[section->dep_idx].kind==ddkMain)
    && (section->storeynum >= dude.sn_aoy) )
  { (void) object_create(ocAmulet, amkYendor, 1);
  }
}

static one_caller tBoolean aoy_good(const tObject* object)
{ const tObjectDetail detail = object->objdetail;
  return(cond2boolean( (!AMULET_FAKE(detail)) &&
    (amulet_misscount(detail) <= 0) ));
}

static void section_enter(tSection* section,
  /*@null@*/ const tSeconn* enterconn, const unsigned char flags0)
/* "flags0 & 1": position the player character; "&2": use earlier coordinates
   if possible; "&4": use random coordinates; "&8": mention dept change */
{ const tStoreyNumber storey = section->storeynum;
  tTicks diff = dungeonclock - section->when_leaving;
  tTrapsLeft min_traps_left = difficulty / 2 + randbytemod(2);
  tCreature* creature;
  tObject* object;
  tCoordinate sw = section->width, sh = section->height, vw = screen_width - 1,
    vh = screen_height - 2, x, y;
  tSeconn* seconn;
  tSectionEvent* event;
  tDungeonDepKind ddk;
  unsigned char zip;

  viewrect.x1 = ( (vw <= sw) ? 0 : ((vw - sw) / 2) );
  viewrect.y1 = ( (vh <= sh) ? 0 : ((vh - sh) / 2) );
  if (vw > sw) vw = sw;
  if (vh > sh) vh = sh;
  viewrect.w = vw; viewrect.h = vh; dude.curr_dd = section->dep_idx;
  mrm_fov = mrm_light = must_recalc_origin = truE;

  if (min_traps_left < 1) min_traps_left = 1;
  min_traps_left = TRAPPEDVAL * min_traps_left + 1;
  if (section->traps_left < min_traps_left)
  { /* player character enters this section again, "re-activate" some traps */
    section->traps_left = min_traps_left;
  }

  if (diff > 0) { section_age_objects(diff); creature_age_inventories(diff); }
  event = section->events;
  while (event != NULL)
  { tTickout* const t = &(event->tickout);
    t->expiry += t2mt(diff); tickout_schedule(t); event = event->next;
  }
  /* IMPLEMENTME: if the player character was already in this section in the
     past, let all creatures in perform about 30 (but at most <diff>!) actions
     and let section events proceed a little in order to simulate that some
     time elapsed! */
  /* IMPLEMENTME: amnesia! */

  if (!(flags0 & 1)) { /* nothing at all; the PC was already placed */ }
  else if ( (flags0 & 2) && ( (x = section->rogue_lastx) != coordNone ) )
  { /* put player character at earlier location */
    rogue->x = x; rogue->y = section->rogue_lasty;
    /* rogue->room = section->rogue_lastroom; */ goto placed;
  }
  else if ( (!(flags0 & 4)) && (enterconn != NULL) )
  { /* put at location of entrance connector */
    rogue->x = x = enterconn->x; rogue->y = y = enterconn->y;
    /* rogue->room = location2room(x, y); */ goto placed;
  }
  else /* put in a random place */
  { tUint16 count = 50000;
    while (--count > 0)
    { tRoomIndex r;
      tSquareFlags sflags;
      section_randlocationinroom_nonshop(&x, &y, &r);
      sflags = squareflags_get(x, y);
      if (!(sflags & (sfCreature | sfObstacle | sfFatalGround | sfPlatform |
        sfBarrier | sfSeconn | sfWall)))
      { rogue->x = x; rogue->y = y; /* rogue->room = r; */ goto placed; }
    }
    do_quit_msg(ecRuntime,
      "BUG: could not place the player character anywhere!"); /* argh... */
    placed: squareflags_or(rogue->x, rogue->y, sfCreature);
  }
  if (rogue_chained)
  { roguedata.iron_ball_x = rogue->x; roguedata.iron_ball_y = rogue->y; }
  if (roguedata.rds.sn_deepest < storey) roguedata.rds.sn_deepest = storey;
  ddk = dude.dd[section->dep_idx].kind; object = _rogue_aoy_find();
  check_aoys(section, object);
  if ( (storey == 1) && (object != NULL) && (aoy_good(object)) &&
    (ddk == ddkMain) && (!(dude.flags & dudefHasTopstairs)) &&
    (section_randseconnlocation(truE, &x, &y)) && ( (seconn =
    seconn_create(section, x, y, ((difficulty >= dkVeryHard) ? sckHoleUp :
    sckStairsUp), NULL)) != NULL ) )
  { dude.flags |= dudefHasTopstairs; seconn->flags |= scfRequiresAoY; }
  rogue->last_action = _dungeonclock; creature_tickout_recalc(rogue, 1);

  for_each_currentsectioncreature(creature)
  { tSquareFlags sflags;
    if (creature->flags & cfDead) continue;
    x = creature->x; y = creature->y;
    sflags = squareflags_get(x, y);
    if ( (sflags & sfHidden) && (sflags & (sfTunnel | sfDoorway)) &&
         (map_can_player_see(x, y, mfCreature)) )
    { squareflags_andnot(x, y, sfHidden); }
    /* (If a creature is there and the player can see it, he knows there must
        be some doorway, tunnel, ...; so that's no longer hidden.) */
    creature->last_action = _dungeonclock; creature_tickout_recalc(creature,1);
  }

  if ( (zip = section_timezip(section->flags)) > 0 ) /* print a hint message */
  { char* ptr = strbuf;
    *ptr++ = 'Z'; *ptr++ = 'z'; *ptr++ = 'z';
    while ( (zip > 0) && (__randpercent(75)) ) { *ptr++ = 'z'; zip--; }
    *ptr++ = 'i'; *ptr++ = 'p'; *ptr++ = '!';
    while (zip > 0) { *ptr++ = '!'; zip--; }
    *ptr = '\0'; message_show(strbuf);
  }

  object_handle_sectionchange(); may_display_section = truE; map_reset();
  statistics_invalidate(svfLevel); /* map_do_update_all(); */
  if ( (flags0 & 8) && (difficulty <= dkNormal) &&
    (current_dd->kind == ddkDeadEnd) )
  { aftermath_schedule(say_dead_end, NULL); }
#if CONFIG_DEBUG
  section->flags |= secfEntering;
#endif
}

static void section_leave(tSection* section)
{ const tCoordinate x = rogue->x, y = rogue->y;
  tTickout* t;
  may_display_section = falsE;
  section->when_leaving = dungeonclock; section->rogue_lastx = x;
  section->rogue_lasty = y; /* section->rogue_lastroom = rogue->room; */
  squareflags_andnot(x, y, sfCreature); cleanse_price_tags();
  while ( (t = active_tickouts) != NULL )
  { if (!(t->keep)) tickout_detach(t); }
}

static /*@null@*/ tSeconn* oppoconn(const tSeconn* seconn)
{ const tSeconnData* const scd = seconn->data;
  tSeconn* retval = scd->from;
  if (retval == seconn) retval = scd->to;
  return(retval);
}

static one_caller void arrange_fallen_objects(const tSeconn* seconn)
{ const tCoordinate x = seconn->x, y = seconn->y;
  tSeconnData* const scd = seconn->data;
  tObject* o;
  while ( (o = scd->objects) != NULL )
  { object_remove_from_list(&(scd->objects), o);
    if (!object_randlocatearound(o, x, y)) object_deallocate(o);
  }
}

tBoolean pass_seconn(const tSeconn* seconn, const unsigned char flags0)
/* "flags0 & 1": don't "actually" use all specific seconn information, only
   rough guidance */
{ tBoolean retval;
  const tBoolean rough = cond2boolean(flags0 & 1);
  const tSeconnFlags scf = seconn->flags;
  const tBoolean downward = cond2boolean(sck_down(seconn->kind));
  tSeconnData* scd;
  tSeconn* opp;
  unsigned char enterflags = 1;
  if (rough) { enterflags |= 4; goto do_rough; }
  if (scf & scfDefunct)
  { defu: message_show("this seems broken"); message_show("too dangerous!");
    /* IMPLEMENTME: nicer, more specific text; esp. it should become clear that
       this is a gameplay feature, not a technical problem! :-) */
    failed: retval = falsE; goto out;
  }
  if (scf & scfRequiresAoY)
  { const tObject* const aoy = _rogue_aoy_find();
    tObjectDetail detail;
    if (aoy == NULL)
    { no_aoy: message_show("need a proper Amulet of Yendor"); goto failed; }
    detail = aoy->objdetail;
    if ( (AMULET_FAKE(detail)) || (amulet_misscount(detail) > 0) ) goto no_aoy;
    /* shall allow passing if amulet is cursed... */
  }
  do_rough: scd = seconn->data;
  if (scd->to == NULL) /* must create the destination section first */
  { const tDungeonDepKind ddk = scd->ddk;
    const signed char sig = cond2sign(downward);
    const tStoreyNumber storey = current_section->storeynum,newstor=storey+sig;
    tDepIdx dep_idx;
    tDepData* dd;
    if (ddk != 0) /* must create a new department */
    { const tBoolean is_de = cond2boolean(ddk == ddkDeadEnd);
      tStoreyNumber firststor = newstor, laststor = (is_de ? (firststor + (2 +
        randbytemod(3)) * sig) : firststor), hm = dude.sn_hardmax, st;
      if (sig > 0) { if (laststor > hm) laststor = hm; }
      else
      { if (laststor < 1) laststor = 1;
        st = firststor; firststor = laststor; laststor = st;
      }
      scd->ddk = 0;
      if ( (dep_idx = department_create(ddk, firststor, laststor)) == 0 )
        goto defu; /* too many departments; "should not happen" */
      dd = &(dude.dd[dep_idx]);
      if (sig < 0) dd->flags |= ddfUpward;
      if (is_de) dd->name = "Dead End";
    }
    else dep_idx = seconn->section->dep_idx;
    section_leave(current_section); current_section = section_allocate();
    section_create(current_section, dep_idx, newstor, seconn);
    opp = oppoconn(seconn);
  }
  else
  { opp = oppoconn(seconn); section_leave(current_section);
    current_section = opp->section;
  }
  if ( (opp != NULL) && (dude.dd[seconn->section->dep_idx].kind !=
    dude.dd[opp->section->dep_idx].kind) )
  { enterflags |= 8; }
  section_enter(current_section, opp, enterflags);
  if ( (downward) && (opp != NULL) && (scd->objects != NULL) )
    arrange_fallen_objects(opp);
  retval = _do_tick = truE;
  out: return(retval);
}

tBoolean updownsec(const tBoolean upward)
/* tries to go one storey up/down within the current department and puts the
   player character at a random position in the destination section */
{ tBoolean retval;
  const unsigned char mask = (upward ? 2 : 1);
  tSection* const section = current_section;
  tSeconn* seconn = section->seconns;
  const tDepData* dd;
  tStoreyNumber storey;
  tCoordinate x, y;
  while (seconn != NULL)
  { if (seconn_downup(seconn) & mask) /* leads toward the desired direction */
    { const tSeconnData* const sd = seconn->data;
      const tSeconn* const to = sd->to;
      if (to == NULL)
      { if (sd->ddk == 0) /* nothing special */
        { try_pass: retval = pass_seconn(seconn, 1); goto out; }
      }
      else if (to->section->dep_idx == section->dep_idx) goto try_pass;
    }
    seconn = seconn->next;
  }
  /* No existing connector leads in the desired up/down direction. Check
     whether we may create a new connector/section for that direction. */
  dd = current_dd; storey = current_storeynum;
  if (upward) { if (storey <= dd->min_storey) goto failed; }
  else { if (storey >= dd->max_storey) goto failed; }
  if ( (section_randseconnlocation(falsE, &x, &y)) &&
    ( (seconn = seconn_create(section, x, y, (upward ? sckStairsUp :
    sckStairsDown), NULL)) != NULL ) )
  { goto try_pass; }
  failed: retval = falsE;
  out: return(retval);
}

void section_forget_object(tObject* object)
/* "forgets" that the object is lying around in the current section; call this
   e.g. when a creature picks up the object. */
{ const tCoordinate x = object->x, y = object->y;
  squareflags_andnot(x, y, sfObject);
  map_update(x, y, mfObjectDisappeared);
  object_remove_from_list(&(current_section->objects), object);
  if (object_might_emit_light(object)) mrm_light = truE;
}

/*@null@*/ tObject* section_get_objects(void)
/* returns a pointer to the list of objects lying around in the current
   section; callers from outside of this file must not change the list data
   directly! */
{ return(current_section->objects); }

tCreatureCount section_countcreatures(void)
{ return(current_section->creaturescount); }

static one_caller void section_inccreaturescount(void)
/* increments the count of creatures in the current section */
{ current_section->creaturescount++; }

static one_caller void section_deccreaturescount(void)
/* decrements the count of creatures in the current section */
{
#if CONFIG_DEBUG
  if (current_section->creaturescount == 0)
    message_showdebug("BUG: section_deccreaturescount()");
  else
#endif
  { current_section->creaturescount--; }
}

/*@null@*/ tCreature* section_get_creatures(void)
/* returns a pointer to the list of creatures in the current section;
   callers from outside of this file must not change the list data directly! */
{ return(current_section->creatures); }

void mrm_fov_giant(const tCreature* creature)
{ if ( (!(creature->flags & cfInvisible)) &&
    (creature_size(creature) >= csGiant) )
  { mrm_fov = truE; }
}

void section_register_creature(tCreature* creature)
/* "registers" the creature in the current section in terms of squareflags, map
   and creatures list */
{ const tCoordinate x = creature->x, y = creature->y;
  squareflags_or(x, y, sfCreature);
  if ( (creature == rogue) || ( (rogue == NULL) && (creature->cc == ccRogue) ))
    map_update(x, y, mfRogueAppeared); /* it's the player character */
  else
  { creature->next = current_section->creatures;
    current_section->creatures = creature;
    section_inccreaturescount(); map_update(x, y, mfCreatureAppeared);
    mrm_fov_giant(creature);
  }
  /* creature->room = location2room(x, y); */ creature->tx = coordNone;
  creature->lastdir = dirNone; creature->last_action = _dungeonclock;
  creature_tickout_recalc(creature, 1);
}

void section_unregister_creature(tCreature* creature)
/* "unregisters" the creature; cf. section_register_creature() */
{ tCoordinate x, y;
  if (creature->flags & cfDead) return; /* already dead/unregistered */
  x = creature->x; y = creature->y; squareflags_andnot(x, y, sfCreature);
  tickout_detach(&(creature->tickout));
  if (creature == rogue) map_update(x, y, mfRogueDisappeared);
  else
  { map_update(x, y, mfCreatureDisappeared); section_deccreaturescount();
#if CONFIG_DEBUG
    /* Enforce a crash if we have a bug somewhere: */
    creature->x = creature->y = coordNone; /* creature->room = roomNone; */
#endif
    mrm_fov_giant(creature);
  }
}

static one_caller void section_remove_dead_creatures(tSection* section)
/* removes all dead creatures from the creatures list of the current section;
   call this at a "quiet moment" (if secfHasDeadCreatures) */
{ tCreature *creature, *c;
  loop:
  creature = section->creatures;
  if (creature == NULL) goto done; /* list empty */
  if (creature->flags & cfDead)
  { section->creatures = creature->next; memory_deallocate(creature);
    goto loop;
  }
  while ( (c = creature->next) != NULL )
  { if (c->flags & cfDead) { creature->next = c->next; memory_deallocate(c); }
    else creature = c;
  }
  done: section->flags &= ~secfHasDeadCreatures;
}

static tBoolean section_doorwaynear(const tCoordinate x, const tCoordinate y)
/* checks whether a doorway exists immediately near (x, y) in horizontal or
   vertical direction (if so, don't put an obstacle at (x, y)!) */
{ tBoolean retval;
  tDirection dir;
  for_each_horvertdir(dir)
  { const tCoordinate i = x + xdir[dir], j = y + ydir[dir];
    if ( (is_location_valid(i, j)) && (squareflags_get(i, j) & sfDoorway) )
    { retval = truE; goto out; }
  }
  retval = falsE;
  out: return(retval);
}

void section_put_object(tObject* object, const tCoordinate x,
  const tCoordinate y, const unsigned char flags0)
/* puts the object into the current section at the given location;
   "flags0 & 1": may hide the object in a chest */
{ object->x = x; object->y = y; squareflags_or(x, y, sfObject);
  object_put_on_list(&(current_section->objects), object);
  map_update(x, y, mfObjectAppeared);
  if (object_might_emit_light(object)) mrm_light = truE;
  if ( (flags0 & 1) && (pref_feat > 0) &&
    (!(squareflags_get(x, y) & (sfTunnel | sfCreature | sfSeconn |sfObstacle)))
    && (!section_doorwaynear(x, y)) &&
    (randpercent(difficulty + pref_expl + current_storeynum / 3)) )
  { (void) section_obstacle_create(obkChest, (randpercent(10 * (difficulty+1))
      ? _CHESTMETALFLAG : 0), x, y);
  }
}

void section_put_object_carefully(tObject* object, const tCoordinate x,
  const tCoordinate y, const unsigned char flags0)
/* tries to put the object into the current section at the given location;
   "carefully" means that it checks whether there's "fatal ground" or already
   another object lying at that location and possibly tries to put the objects
   together etc.; call this e.g. as a result of object dropping or throwing;
   "flags0 & 1": function is called due to a Rogue-related action, so show
   messages if appropriate; "&2": allow object_randlocatearound() */
{ const tBoolean rogue_related = cond2boolean(flags0 & 1),
    allow_around = cond2boolean(flags0 & 2);
  const tSquareFlags sflags = squareflags_get(x, y);
  tSeconn* seconn = NULL;
  tObject* object2;

  if (sflags & sfPlatform) goto not_falling;
  if (sflags & sfFatalGround) goto falling;
  if ( (sflags & sfSeconn) && ( (seconn = location2seconn(x, y)) != NULL ) )
  { if (seconn_downup(seconn) & 1) goto falling;
    seconn = NULL; /* forget meaningless stuff */
  }
  goto not_falling;
  { falling:
    if ( (rogue_related) && (map_can_player_see(x, y, mfObject)) &&
      (map_can_player_see(x, y, mfGround)) )
    { const char *temp;
      tBoolean use_plural;
      if (sflags & sfWater) temp = "water";
      else if (sflags & sfSwamp) temp = "swamp";
      else if (seconn != NULL) temp = seconn_desc(seconn, 1);
      else /* "should not happen" */
      { temp = "void";
#if CONFIG_DEBUG
        sprintf(debugstrbuf, "BUG: section_poc(%d)",
          (sflags & sfFatalGround));
        message_showdebug(debugstrbuf);
#endif
      }
      use_plural = object_get_minidesc(object, strbuf, 1, NULL);
      sprintf(strbuf + strlen(strbuf), " vanish%s in the %s",
        (use_plural ? strEmpty : strEs), temp);
      message_show(strbuf);
    }
    if ( (seconn == NULL) || ( (object->oc != ocAmulet) && (__randpercent(5 +
      10 * difficulty)) ) )
    { object_deallocate(object); }
    else object_put_on_list(&(seconn->data->objects), object);
    return; /* the object is "gone" */
  }
  not_falling: {}

  if (sflags & sfObstacle) goto try_around;
  if (!(sflags & sfObject))
  { /* No object lying around here yet and no obstacle in the way, so no
       problem at all: */
    do_put: section_put_object(object, x, y, 0); return; /* done */
  }
  if ( (object2 = location2object(x, y)) == NULL ) /* "should not happen" */
  { object_deallocate(object); return; } /* (at least avoids memory leaks) */
  if (object_ttpt(object2, object, rogue_related))
  { object_deallocate(object); return; } /* got put together */
  if (object_importance(object) > object_importance(object2))
  { /* The more important object destroys whatever was there: */
    section_forget_object(object2); object_deallocate(object2); goto do_put;
  }
  try_around: if ( (allow_around) && (object_randlocatearound(object, x, y)) )
  { /* fine */ }
  else
  { if ( (rogue_related) && (map_can_player_see(x, y, mfObject)) )
    { const tBoolean use_plural = object_get_minidesc(object, strbuf, 1, NULL);
      sprintf(strbuf + strlen(strbuf), " vanish%s as %s hit%s the ground",
        (use_plural ? strEmpty : strEs), (use_plural ? strThey : strIt),
        (use_plural ? strEmpty : strS));
      /* IMPLEMENTME: obstacle name instead of "ground" if sfObstacle and
         map_can_player_see()! */
      message_show(strbuf);
    }
    object_deallocate(object);
  }
}

tBoolean section_randput_object(tObject* object)
/* tries to put the object into the current section at a random position;
   returns whether it found an appropriate position. (IMPORTANT: if it returns
   falsE, the _caller_ must deallocate the object or do whatever else might be
   appropriate!) */
{ tCoordinate x, y;
  tSquareFlags flags;
  unsigned short count;
  const tBoolean is_amulet = cond2boolean(object->oc == ocAmulet),
    tunnel_allowed = randpercent(2 * current_storeynum + 10 * difficulty);
    /* (The higher the difficulty/storey, the more objects may be placed on
        "tunnel" squares.) */

  /* At first, try to place the object "anywhere" (room or tunnel) */
  count = (is_amulet ? 10000 : 50);
  while (count-- > 0)
  { section_randlocation_nonshop(&x, &y); flags = squareflags_get(x, y);
    if ( (flags & sfRoom) || ( (tunnel_allowed) && (flags & sfTunnel) ) )
    { if (! (flags & (sfObject | sfSeconn | sfFatalGround | sfBarrier|sfWall)))
      { do_put: section_put_object(object, x, y, 1); return(truE); }
    }
  }

  /* The above failed, so try to place the object inside a room */
  count = (is_amulet ? 10000 : 50);
  while (count-- > 0)
  { section_randlocationinroom_nonshop(&x, &y, NULL);
    flags = squareflags_get(x, y);
    if (! (flags & (sfObject | sfSeconn | sfFatalGround | sfBarrier | sfWall)))
    { goto do_put; }
  }

  /* All failed... */
  return(falsE);
}

tBoolean section_randlocatearound(const tCoordinate x, const tCoordinate y,
  const tSquareFlags forbidden, tCoordinate* _x2, tCoordinate* _y2)
/* tries to find a "good" location adjacent to (x, y) */
{ tBoolean retval;
  tDirection dir;
  for_each_randdir(dir)
  { const tCoordinate x2 = x + xdir[dir], y2 = y + ydir[dir];
    tSquareFlags sflags;
    if (!is_location_valid(x2, y2)) continue;
    sflags = squareflags_get(x2, y2);
    if (! (sflags & (sfRoom | sfTunnel | sfDoorway)) ) continue;
    if (sflags & (sfObstacle | forbidden)) continue;
    if ( (sflags & sfFatalGround) && (!(sflags & sfPlatform)) ) continue;
    *_x2 = x2; *_y2 = y2; retval = truE; goto out; /* found "good" location */
  }
  retval = falsE;
  out: return(retval);
}

tBoolean section_reveal_objects(void)
{ tBoolean found_something = falsE;
  const tObject* o;
  for_each_object(section_get_objects(), o)
  { const tObjectClass oc = o->oc;
    const tCoordinate x = o->x, y = o->y;
    if ( ( ((1 << oc) & ocbMagic) ||
           (oc == ocAmulet) || (oc == ocFood) ) &&
         (!map_can_player_see(x, y, mfObjectSpecial)) &&
         (!(squareflags_get(x, y) & sfCreature)) )
    { squareflags_or(x, y, sfSpotlight); map_update(x, y, mfObject);
      mrm_light = found_something = truE;
    }
  }
  return(found_something);
}

void section_unhide_square(const tCoordinate x, const tCoordinate y)
{ squareflags_andnot(x, y, sfHidden); map_update(x, y, mfOther); mrm_fov=truE;
}

void section_light_room_up(const tRoomIndex r)
{ tRoom* const ro = &(current_section->room[r]);
  ro->lightmode = lmEqual; ro->lightvalue = 255; mrm_light = truE;
}

static tBoolean section_flood(tCoordinate x, tCoordinate y)
/* floods the section starting from the location (x, y); returns whether it
   made some progress */
{ unsigned char c = 20;
  tSquareFlags sflags;

  loop:
  sflags = squareflags_get(x, y);
  if ( (sflags == sfNone) || (sflags & (sfWall | sfSeconn | sfBarrier)) )
  { failed: return(falsE); }
  if ( (sflags & sfFatalGround) && (!(sflags & sfWater)) ) goto failed;
  if (sflags & sfWater) /* already flooded, go to a random adjacent square */
  { tDirection dir;
    if (!c--) goto failed;
    dir = randdir(); x += xdir[dir], y += ydir[dir];
    if (!is_location_valid(x, y)) goto failed;
    goto loop;
  }

  /* We found a square which can be flooded: */
  squareflags_or(x, y, sfWater); map_update(x, y, mfGround);
  if (sflags & sfGlazedFrost) /* let it thaw */
    squareflags_andnot(x, y, sfGlazedFrost);
  sflags = squareflags_get(x, y); /* revalidate */

  /* See what happens to chest/object/creature on this newly flooded square: */
  if ( (sflags & sfFatalGround) && (!(sflags & sfPlatform)) )
  { if (sflags & sfObject)
    { tObject* const o = location2object(x, y);
      if (o != NULL) { section_forget_object(o); object_deallocate(o); }
    }
    if (sflags & sfObstacle)
    { tObstacle* const obstacle = location2obstacle(x, y);
      if (obstacle != NULL) section_crash_obstacle(obstacle, 0);
    }
    if (sflags & sfCreature)
    { tCreature* dc = location2creature(x, y); /* "dc": "drowning creature" */
      if (dc != NULL)
      { if (dc == rogue) message_show("a tidal wave flows out of the wall");
        if (!creature_is_in_the_air(dc))
        { if (dc == rogue) rogue_died(drDrown, NULL);
          else (void) creature_died(dc, drDrown, NULL);
        }
      }
    }
  }
  return(truE);
}

static tBoolean material_events_exist_at(const tCoordinate x,
  const tCoordinate y)
{ tBoolean retval;
  const tSectionEvent* event = current_section->events;
  while (event != NULL)
  { if ((event_is_material(event->kind)) && (event->x == x) && (event->y == y))
    { retval = truE; goto out; }
    event = event->next;
  }
  retval = falsE;
  out: return(retval);
}

static /* __sallocator */ /*@null@*/ tSectionEvent* __callocator
  section_event_create(const tCoordinate x, const tCoordinate y,
  const tSectionEventKind kind, tTickoutHandler handler,
  const tSectionEventCount count)
/* allocates an event structure, does some basic initialization and puts the
   event into the event list for the current section; can return NULL! */
{ tSectionEvent *event, *e;
  tTickout* t;
  if ( (event_is_material(kind)) && (material_events_exist_at(x, y)) )
    return(NULL); /* avoid conflicts between "materials" */
  event = memory_allocate(sizeof(tSectionEvent)); event->x = x; event->y = y;
  event->kind = kind; event->count = count; t = &(event->tickout);
  t->handler = handler; t->data = (void*) event;
  tickout_set_debuginfo(t, "section event");
  event->next = e = current_section->events;
  if (e != NULL) e->prev = event;
  current_section->events = event;
  return(event);
}

static my_inline tBoolean section_event_is_blocked(const tCoordinate x,
  const tCoordinate y)
{ return(cond2boolean(squareflags_get(x, y) & (sfWall | sfBarrier)));
  /* CHECKME: respect obkTempWall? respect obkBoulder? */
}

static void section_event_remove(tSectionEvent* event)
{ tTickout* const t = &(event->tickout);
  tickout_detach(t);
  if (current_section->events == event) current_section->events = event->next;
  if (event->next != NULL) event->next->prev = event->prev;
  if (event->prev != NULL) event->prev->next = event->next;
  memory_deallocate(event);
}

static void section_event_start(tSectionEvent* event, const tMilliticks mt)
{ tTickout* const t = &(event->tickout);
  t->expiry = _dungeonclock + mt; tickout_schedule(t);
}

#define se_schedule section_event_start /* (currently the same) */
#define SE_WATER_EXPIRY (2000 - 500 * (difficulty - dkNormal))
#define SE_CREATUREBRINGOUT_EXPIRY (1500 - 400 * (difficulty - dkHard))

static void se_water_tickout_handler(tTickout* t, const tTickoutAction ta)
{ if (ta == taTrigger)
  { tSectionEvent* const event = (tSectionEvent*) (t->data);
    const tCoordinate x = event->x, y = event->y;
    if (section_event_is_blocked(x, y))
    { failed:
      if (event->failure >= 30) /* too little progress - stop it */
      { rem: section_event_remove(event); return; }
      else event->failure++;
    }
    else if (!section_flood(x, y)) goto failed;
    else if (--event->count <= 0) goto rem; /* produced enough water */
    se_schedule(event, SE_WATER_EXPIRY);
  }
}

static void se_creature_tickout_handler(tTickout* t, const tTickoutAction ta)
{ if (ta == taTrigger)
  { tSectionEvent* const event = (tSectionEvent*) (t->data);
    const tCoordinate x = event->x, y = event->y;
    const tSquareFlags sflags = squareflags_get(x, y);
    const tSectionEventKind kind = event->kind;
    const tBoolean is_delayed = cond2boolean(kind == sekCreatureDelayed);
    tBoolean may_show_msg, from_water;
    const tCreature* creature;
    unsigned char flags;
    if ( (sflags & sfCreature) || (section_event_is_blocked(x, y)) )
    { failed:
      if (event->failure >= 30) /* too little progress - stop it */
      { rem: section_event_remove(event); return; }
      event->failure++; goto sched; /* try again later */
    }
    if (is_delayed)
    { from_water = cond2boolean(event->flags & 2);
      if ( (from_water) && (!(sflags & sfWater)) )
      { /* The creature vanished in the water, and the water vanished
           later, so the creature can't re-appear out of the water... */
        goto rem;
      }
      may_show_msg = truE;
    }
    else { may_show_msg = from_water = falsE; }
    flags = 1; /* new creature must not sleep */
    if ( ( (sflags & sfFatalGround) && (!(sflags & sfPlatform)) ) ||
         (sflags & sfObstacle) )
      flags |= 2; /* new creature must be levitated */
    /* CHECKME: don't create creature if obstacle is "too high", e.g. a closed
       door! */
    if ( (creature = creature_create_at(event->cc, x, y, flags)) == NULL )
      goto failed;
    if ( (may_show_msg) && (event->flags & 1) &&
         (map_can_player_see(x, y, mfCreature)) )
    { const char* from_where = strEmpty; /* default */
      if (from_water)
      { if (map_can_player_see(x, y, mfGround))
          from_where = " out of the water";
      }
      else if (map_can_player_see(x, y, mfOther))
        from_where = " out of a puff of smoke";
      creature_get_name(creature, strbuf, 1);
      sprintf(strbuf + strlen(strbuf), " re-appears%s", from_where);
      message_show(strbuf);
      /* IMPLEMENTME: animation! */
    }
    if (is_delayed) goto rem; /* it was a one-time effect */
    else if (--event->count <= 0) goto rem; /* created enough creatures */
    sched: se_schedule(event, SE_CREATUREBRINGOUT_EXPIRY);
  }
}

void section_bring_out_creatures(const tCoordinate x, const tCoordinate y,
  const tSectionEventCount count, const tCreatureClass cc)
{ tSectionEvent* const event = section_event_create(x, y, sekCreature,
    se_creature_tickout_handler, count);
  if (event != NULL)
  { const char* noise;
    event->cc = cc;
    section_event_start(event, SE_CREATUREBRINGOUT_EXPIRY);
    if (rogue_can_hear) /* IMPLEMENTME: call noise_caused_at() instead! */
    { if (cc == ccBee) noise = "humm";
      else if (cc == ccSnake) noise = "hiss";
      else noise = "burrow";
      sprintf(strbuf, "%s hear a %sing noise%s", rogue_pronoun(0), noise,
        (creature_same_room_xy(rogue, x, y) ? strEmpty : " in the distance"));
      message_show(strbuf);
    }
    else strangemsg();
    /* CHECKME: print the message only if we're called due to a Rogue-related
       action? */
    /* CHECKME: is "creature_same_room_xy()" a reasonable condition? */
  }
}

void section_recreate_creature(const tCoordinate x, const tCoordinate y,
  const tCreatureClass cc, const unsigned char flags0)
/* lets a creature of the given class appear after some delay; to the player,
   this seems as if a (died) creature re-appears */
{ tSectionEvent* const event = section_event_create(x, y, sekCreatureDelayed,
    se_creature_tickout_handler, 0);
  if (event != NULL)
  { unsigned char delay = 2 * (6 - difficulty) + randbytemod(5);
    if (delay < 4) delay = 4;
    event->cc = cc;
    if (flags0 & 1) event->flags |= 1; /* player could see this */
    if (flags0 & 2)
    { const tSectionEventCount dly = 3 + randbytemod(3);
      if (delay > dly) delay = dly; /* shorter delay */
      event->flags |= 2; /* "water", not "puff of smoke" */
    }
    section_event_start(event, 1000 * delay);
  }
}

static one_caller tBoolean section_mistify(tSectionEvent* event,
  const tCoordinate x, const tCoordinate y)
{ tBoolean retval;
  retval = falsE;
  return(retval);
}

static tBoolean section_demistify(tSectionEvent* event)
{ return(falsE);
}

static void se_mist_tickout_handler(tTickout* t, const tTickoutAction ta)
{ if (ta == taTrigger)
  { tSectionEvent* const event = (tSectionEvent*) (t->data);
    tMilliticks mt;
    const tCoordinate x = event->x, y = event->y;
    if (event->flags & 1) /* remove existing mist */
    { if (section_demistify(event)) se_schedule(event, 2000);
      else section_event_remove(event); /* done */
      return;
    }
    if (section_event_is_blocked(x, y))
    { failed:
      if (event->failure >= 30) goto slow_rem; /* too little progress */
      else event->failure++;
    }
    else if (!section_mistify(event, x, y)) goto failed;
    else if (--event->count <= 0) /* schedule slow removal of the mist */
    { slow_rem: event->flags |= 1; se_schedule(event, 5000); return; }
    /* If more mist is yet to appear, there's a higher "pressure" and therefore
       the next amount appears more quickly. */
    if (event->count >= 10) mt = 300;
    else mt = 3000 - 300 * event->count;
    se_schedule(event, mt);
  }
}

static one_caller void start_mist(const tCoordinate x, const tCoordinate y,
  const unsigned char flags0)
/* "flags0 & 1": related to player character, show message */
{ tSectionEvent* const event = section_event_create(x, y, sekMist,
    se_mist_tickout_handler, randbytefromrange(3, 5 + difficulty +
    current_storeynum / 8));
  if (event != NULL)
  { if (flags0 & 1)
    { if (rogue_can_hear)
      { sprintf(strbuf, "%s hear a slight whizzing", rogue_pronoun(0));
        message_show(strbuf);
      }
      else strangemsg();
    }
    event->ex1 = event->ex2 = x; event->ey1 = event->ey2 = y;
    section_event_start(event, 42 * 42);
  }
}

void section_hoe_wall(tCoordinate x, tCoordinate y, const tDirection dir,
  const tHoeValue flags0)
/* hoes the wall; note that we don't _remove_ the wall - that causes lots of
   trouble with map handling etc. (Been there, done that:-) Instead we hoe a
   little hole into the wall and let something come out of that.
   "flags0 & 15": four-bit probability indicator; "flags0 & 16": Rogue-related;
   "flags0 & 32": magic */
{ const tBoolean is_rogue = cond2boolean(flags0 & 16),
    is_magic = cond2boolean(flags0 & 32);
  tSquareFlags sflags = squareflags_get(x, y);
  tStoreyNumber storey;
  tRandomByte rb;
  if (pref_feat <= 0) return;
  if (!(sflags & sfWall)) return; /* "should not happen" */
  if ( (is_rogue) && (!(sflags & sfKnownWall)) && (!is_magic) ) /* don't hoe */
    return;
  if (sflags & sfBarrier) return; /* barrier prevents hoeing */
  if (!__randpercent(5 * ( (flags0 & 15) + 1 ))) return; /* don't hoe */
  _do_tick = truE;
  if ( (is_rogue) && (sflags & sfDoorway) && (sflags & sfHidden) )
  { /* don't hoe, only reveal the hidden doorway */
    section_unhide_square(x, y);
    if (map_can_player_see(x, y, mfOther))
    { message_show(strWhoops);
      if (is_magic) rogue_incmagicexp(1);
    }
    return;
  }

  /* Now we know that we want to start an event. */

  if (dir == dirNone) return; /* "should not happen" */
  if ( (is_rogue) && (map_can_player_see(x, y, mfWall)) )
    message_show("hoeing the wall");
  x -= xdir[dir]; y -= ydir[dir]; /* going one square "backwards" */
  if (!is_location_valid(x, y)) return; /* "should not happen" */

  /* IMPLEMENTME: if strong physical cause, create obkPileOfStone of increasing
     height (and resistance) after very few ticks! */
  rb = randbytemod(100); storey = current_storeynum;
  if ( (storey >= 3) && (rb <= 20) ) /* bees */
  { tSectionEventCount count = MIN(difficulty, 2) +
      randbytemod(3 + cond2bool(storey >= 10));
    if (count < 3) count = 3;
    section_bring_out_creatures(x, y, count, ccBee);
  }
  else if ( (storey >= 5 - difficulty) && (rb <= 40) ) /* snakes */
    section_bring_out_creatures(x, y, randbytefromrange(2, 4), ccSnake);
  else if ( (storey >= 7 - difficulty) && (rb <= 50) ) /* worms */
    section_bring_out_creatures(x, y, randbytefromrange(4, 6), ccWorm);
  else if ( (storey >= 9 - difficulty) && (rb <= 60)) /* spiders */
    section_bring_out_creatures(x, y, randbytefromrange(2, 3), ccSpider);
  else if ( (storey >= 11 - difficulty) && (rb <= 70) ) /* slime */
    section_bring_out_creatures(x, y, randbytefromrange(1, 3), ccSlime);
  else if ( (!(squareflags_get(x, y) & sfTunnel)) /* (don't flood tunnels) */
    && (storey >= 7 - difficulty) && (rb <= 80) )
  { tSectionEvent* event;
    tSectionEventCount count = (5 * difficulty + storey) / 8,
      _min = difficulty / 2, min = zero2one(_min);
    if (count < min) count = min;
    if (__randpercent(3 + 19 * difficulty)) count++;
    if ( (event = section_event_create(x, y, sekWater,
      se_water_tickout_handler, count)) != NULL )
    { section_event_start(event, SE_WATER_EXPIRY);
      if (is_rogue)
      { if (rogue_can_hear) /* IMPLEMENTME: call noise_caused_at() instead! */
        { sprintf(strbuf, "%s hear a low murmur", rogue_pronoun(0));
          message_show(strbuf);
        }
        else strangemsg();
      }
    }
  }
  else if ( (pref_feat + difficulty >= 5) && (!(rb & 3)) )
    start_mist(x, y, cond2bool(is_rogue));
  /* "else": hoeing didn't have an effect this time - don't care */
}

typedef struct
{ const char* name;
  tBasicObjectMaterials bom;
  unsigned char size; /* relative default size, 1..15; 15: unpassable, even if
    creature in the air */
  tObstacleResistance res; /* default resistance */
  tObstacleFlags flags;
} tObstacleData;

static const tObstacleData obstacledata[maxObstacleKind + 1] =
{ { "chest", bomSpecial, 4, 2, obfCrCr },
  { "door", bomSpecial, 15, 5, obfNone },
  { "crate", bomWood,  8, 4, obfHvPushable | obfHvPullable | obfCrCr },
  { "pile of stone", bomStone, 14, 15, obfCrCr },
  { "boulder", bomStone, 12, 12, obfPushable | obfCrCr },
  { "glass cube", bomGlass, 15, 15, obfNone },
  { "mirror", bomGlass,  8, 1, obfNone },
  { "black hole", bomMagic,  1, 15, obfNone },
  { "teleporter", bomMetalS, 10, 15, obfNone },
  { "stone table", bomStone, 6, 14, obfNone },
  { "temporary wall", bomStone, 15, 14, obfNone },
  { "stalagmite", bomStone, 5, 5, obfNone },
  { "switch", bomMetalS, 10, 15, obfNone },
  { "iron bars", bomMetalS, 15, 10, obfPlural }
};

/*@null@*/ tObstacle* section_obstacle_create(const tObstacleKind kind,
  const tObstacleDetail detail, const tCoordinate x, const tCoordinate y)
{ tObstacle* obstacle = current_section->obstacles;
  const tObstacleData* od;
  unsigned char _mat;
  while (obstacle != NULL)
  { if ( (obstacle->x == x) && (obstacle->y == y) )
    { /* already one there; "should not happen" */
      return(NULL);
    }
    obstacle = obstacle->next;
  }
  obstacle = (tObstacle*) memory_allocate(sizeof(tObstacle));
  obstacle->next = current_section->obstacles;
  current_section->obstacles = obstacle; obstacle->kind = kind;
  obstacle->x = x; obstacle->y = y; od = &(obstacledata[kind]);
  if ( ( (obstacle->size = od->size) < 14 ) && (randcointoss()) )
    obstacle->size++;
  obstacle->obsdetail = detail; obstacle->resistance = od->res;
  obstacle->flags = od->flags; squareflags_or(x, y, sfObstacle);
  map_update(x, y, mfObstacle); mrm_fov = truE;
  switch (kind)
  { case obkChest:
      if (chest_is_metal(detail)) obstacle->resistance += 2;
      break;
    case obkDoor:
      _mat = _door_material(detail) + 1;
      obstacle->resistance += 2 * _mat; break;
    case obkStalagmite: /* natural matter, big differences */
      obstacle->size = randbytefromrange(5, 10);
      obstacle->resistance = randbytefromrange(8, 12); break;
  }
  return(obstacle);
}

static void section_obstacle_remove(tObstacle* obstacle)
{ const tCoordinate x = obstacle->x, y = obstacle->y;
  squareflags_andnot(x, y, sfObstacle); map_update(x, y, mfObstacle);
  mrm_fov = truE; tickout_detach(&(obstacle->tickout));
  remove_from_list(&(current_section->obstacles), obstacle, tObstacle);
  memory_deallocate(obstacle);
  /* IMPLEMENTME: animation? */
}

tBasicObjectMaterials obstacle_materials(const tObstacle* obstacle)
{ static const tBasicObjectMaterials doormat[4] =
  { bomGlass, bomWood, bomStone, bomMetalN };
  const tObstacleKind kind = obstacle->kind;
  tBasicObjectMaterials retval = obstacledata[kind].bom;
  if (retval == bomSpecial) /* materials depend on detail */
  { const tObstacleDetail detail = obstacle->obsdetail;
    switch (kind)
    { case obkChest:
        retval = ( (chest_is_metal(detail)) ? bomMetalN : bomWood ); break;
      case obkDoor: retval = doormat[_door_material(detail)]; break;
    }
  }
  return(retval);
}

#if 0
static tObstacleEffects section_obstacle_effect(tObstacle* obstacle,
  const tObject* object, tObstacleContactKind obck,
  __sunused tCoordinate distance __cunused)
/* returns whether the obstacle e.g. stops a thrown object, reflects a zapping
   ray, ... */
{ tObstacleEffects retval = oeNone;
  switch (obstacle->kind)
  { case obkBlackHole:
      if ( (obck == obckZap) && (object->oc == ocWandStaff) )
      { /* magic black hole stops almost all zapping rays */
        retval = ( (object->kind == wskRemove) ? oeRemove : oeStop );
      }
      break;
    case obkMirror:
      if ( (obck == obckZap) && (object->oc == ocWandStaff) )
      { /* magic mirror reflects almost all zapping rays */
        retval = ( (object->kind == wskRemove) ? oeRemove : oeReflect );
      }
      break;
  }
  if (retval == oeRemove) section_obstacle_remove(obstacle);
  return(retval);
}
#endif

tObstacle* location2obstacle(const tCoordinate x, const tCoordinate y)
{ tObstacle* obstacle = current_section->obstacles;
  while (obstacle != NULL)
  { if ( (obstacle->x == x) && (obstacle->y == y) ) break;
    obstacle = obstacle->next;
  }
  return(obstacle);
}

void section_obstaclekindname(const tObstacleKind kind, /*@out@*/ char* dest,
  const unsigned char flags0)
{ const tObstacleData* const od = &(obstacledata[kind]);
  const char* const name = od->name;
  if ( (flags0 & 1) && (!(od->flags & obfPlural)) )
  { const char ch = name[0], *const article = indefinite_article(ch);
    sprintf(dest, "%s %s", article, name);
  }
  else strcpy(dest, name);
}

tBoolean section_obstacle_get_name(const tObstacle* obstacle,
  /*@out@*/ char* dest, const unsigned char flags0)
/* returns whether a verb should use plural form; "flags0 & 1": definite
   article; "&2": show label if any; "&4": mention details */
{ const tObstacleKind kind = obstacle->kind;
  const tObstacleData* const od = &(obstacledata[kind]);
  const char *const name = od->name, *article, *src;
  const tBoolean is_plural = cond2boolean(od->flags & obfPlural);
  tObstacleDetail detail;
  char ch;
  if (flags0 & 1) article = strThe;
  else if (is_plural) goto no_art;
  else article = indefinite_article(name[0]);
  while ( (ch = *article++) != '\0' ) *dest++ = ch;
  *dest++ = ' '; no_art: {}
  if (!map_can_player_see(obstacle->x, obstacle->y, mfObstacle)) goto deflt;
  detail = obstacle->obsdetail;
  switch (kind)
  { case obkChest:
      if (flags0 & 4)
      { src = (chest_is_metal(detail) ? strMetal : strWooden);
        while ( (ch = *src++) != '\0' ) *dest++ = ch;
        *dest++ = ' '; /* proceed with default text below */
      }
      break;
    case obkDoor:
      if (flags0 & 4)
      { const _tDoorMaterial dm = _door_material(detail);
        src = strGwsm[dm]; while ( (ch = *src++) != '\0' ) *dest++ = ch;
        /* IMPLEMENTME: color (hallu!), label (if flags0&2 and player character
           can read); */
        *dest++ = ' '; /* proceed with default text below */
      }
      break;
    case obkBlackHole:
      if (rogue->hallucinating)
      { sprintf(dest, "%s hole", strColor[hallu_color(ccBlack)]); goto done; }
      break;
  }
  deflt: strcpy(dest, name); /* simple default text */
  done: return(is_plural);
}

tObstacleSize obstacle_effsize(const tObstacle* obstacle)
/* "effective size", e.g. for collision tests; 0 means "not really there" */
{ tObstacleSize retval;
  switch (obstacle->kind)
  { case obkDoor:
      retval = ( (door_passable(door_state(obstacle->obsdetail))) ? 0 : 15 );
      break;
    default: retval = obstacle->size; break;
  }
  return(retval);
}

static tPercentage obstacle_transpglass(const tObstacle* obstacle)
/* for transparent glass obstacles (that is, not mirrors!) */
{ return(90 - current_storeynum - obstacle->size); /* slight degradation */
  /* IMPLEMENTME: colored glass reduces transparency more or less! */
}

tPercentage obstacle_transparency(const tObstacle* obstacle)
/* returns roughly how well a ray of light / line of sight can pass (or pass
   through) the obstacle */
{ tPercentage retval;
  tObstacleSize effsize;
  tObstacleKind kind;
  if (obstacle->flags & obfInvisible) { r100: retval = 100; goto out; }
  if ( (effsize = obstacle_effsize(obstacle)) <= 0 ) goto r100;
  switch ( (kind = obstacle->kind) )
  { case obkDoor:
      retval = ( (_door_material(obstacle->obsdetail) == dmGlass) ?
        (obstacle_transpglass(obstacle)) : 0 );
      break;
    case obkIronBars: goto r100; /*@notreached@*/ break;
    default:
      if ( (kind != obkMirror) && (obstacle_materials(obstacle) == bomGlass) )
        retval = obstacle_transpglass(obstacle);
      else retval = ( (effsize >= 12) ? 0 : 100 );
      break;
  }
  out: return(retval);
}

tPercentage obstacle_transzapency(const tObstacle* obstacle)
{ tPercentage retval;
  if ( (obstacle->flags & obfInvisible) || (obstacle_effsize(obstacle) <= 0) )
    retval = 100;
  else if ( (obstacle->kind != obkMirror) &&
    (obstacle_materials(obstacle) == bomGlass) )
  { retval = obstacle_transpglass(obstacle); }
  else retval = 0;
  return(retval);
}

tBoolean obstacle_showable(const tObstacle* obstacle)
{ tBoolean retval;
  if (obstacle->flags & obfInvisible) { failed: retval = falsE; }
  else if ( (obstacle->kind == obkDoor) && (door_state(obstacle->obsdetail) ==
    dsBroken) )
  { goto failed; }
  else retval = truE;
  return(retval);
}

void section_crash_obstacle(tObstacle* obstacle, const unsigned char flags0)
/* Call this e.g. if a creature lands on an sfObstacle square; "flags0 & 1":
   print a message; "flags0 & 2": allow a creature "appearing" where the
   obstacle was (only allowed if Rogue-related!) */
{ const tObstacleFlags obf = obstacle->flags;
  const tCoordinate x = obstacle->x, y = obstacle->y;
  if (flags0 & 1)
  { const tBoolean pl = section_obstacle_get_name(obstacle, strbuf, 1);
    char* ptr = strbuf;
    while (*ptr != '\0') ptr++;
    strcpy(ptr, " break");
    if (!pl) { ptr += 6; *ptr++ = 's'; *ptr = '\0'; }
    message_show(strbuf);
  }
  /* IMPLEMENTME: if the obstacle is a door, call door_state_set() with
     dsBroken (if appropriate, e.g. not removed by wskRemove) instead of
     removing the obstacle! */
  section_obstacle_remove(obstacle);
  /* IMPLEMENTME: call noise_caused_by()! */
  if ( (flags0 & 2) && (pref_feat > 0) && (obf & obfCrCr) &&
    (!(squareflags_get(x, y) & (sfCreature | sfFatalGround | sfFire))) &&
    (__randpercent(5 * difficulty + current_storeynum / 2 + 1)) )
  { /* Let a creature appear where the obstacle was; to the player, this seems
       as if the creature had been in there. (That's why we only allow _small_
       creatures here BTW.) Ya know Schroedinger's cat? :-) */
    static const tCreatureClass classes[3] = {ccRattlesnake, ccSnake, ccSlime};
    const tCreatureClass cc = classes[randbytemod(2 +
      cond2bool(roguedata.rds.sn_deepest >= 10 - difficulty))];
    (void) creature_create_at(cc, x, y, 1);
    /* IMPLEMENTME: let a swarm of bees or several snakes come out of it
       instead sometimes? */
  }
}

tBoolean obstacle_resistance_decrease(tObstacle* obstacle,
  const tObstacleResistance r, const unsigned char flags0)
/* returns whether the obstacle was destroyed; "flags0 & 1": Rogue-related */
{ tBoolean retval;
  if (obstacle->resistance > r) { obstacle->resistance -= r; retval = falsE; }
  else
  { unsigned char f;
    if (flags0 & 1)
    { f = 2;
      if (map_can_player_see(obstacle->x, obstacle->y, mfObstacle)) f |= 1;
    }
    else f = 0;
    section_crash_obstacle(obstacle, f); retval = truE;
  }
  return(retval);
}

void say_obstacle_resists(const tObstacle* obstacle)
{ const tBoolean pl = section_obstacle_get_name(obstacle, strbuf, 1);
  char* ptr = strbuf;
  while (*ptr != '\0') ptr++;
  strcpy(ptr, " resist");
  if (!pl) { ptr += 7; *ptr++ = 's'; *ptr = '\0'; }
  message_show(strbuf);
}

static void maybe_say_obstacle_vanishes(const tObstacle* obstacle,
  /*@null@*/ const tCreature* by __cunused)
{ if ( /* (by == rogue) && */ (map_can_player_see(obstacle->x, obstacle->y,
    mfObstacle)) ) /* actually show a message */
  { const tBoolean pl = section_obstacle_get_name(obstacle, strbuf, 1);
    char* ptr = strbuf;
    while (*ptr != '\0') ptr++;
    strcpy(ptr, " vanish");
    if (!pl) { ptr += 7; *ptr++ = 'e'; *ptr++ = 's'; *ptr = '\0'; }
    message_show(strbuf);
  }
}

tBoolean obstacle_zap(tObstacle* obstacle, const tObject* wand,
  /*@null@*/ const tCreature* by)
/* returns whether the obstacle was destroyed */
{ tBoolean retval = falsE;
  const tWandStaffKind wsk = wand->kind;
  const tObstacleResistance res = obstacle->resistance;
  if (res < 15) /* destroyable */
  { if (res == 14) /* need a very strong wsk */
    { if ( (wsk == wskRemove) && (wand->enchantment >= 0) )
      { maybe_say_obstacle_vanishes(obstacle, by);
        section_obstacle_remove(obstacle); set_destr: retval = truE; goto out;
      }
    }
    else /* a weaker wsk may decrease the resistance */
    { const tHoeValue v = object_hoevalue(wand,obstacle_materials(obstacle),2);
      if ( (v > 0) && (obstacle_resistance_decrease(obstacle, v,
        ( (by == rogue) ? 1 : 0 ))) )
      { goto set_destr; }
    }
  }
  if ( (wsk == wskMakeInvisible) && (!(obstacle->flags & obfInvisible)) )
  { maybe_say_obstacle_vanishes(obstacle, by);
    obstacle->flags |= obfInvisible;
    map_update(obstacle->x, obstacle->y, mfObstacle);
  }
  else if ( (wsk == wskCancel) && (obstacle->flags & obfInvisible) )
  { /* IMPLEMENTME: make visible! */
  }
  switch (obstacle->kind)
  { case obkDoor:
      /* IMPLEMENTME: zapping a locked door with a "cancellation" wand unlocks
         the door, at least if difficulty <= dkNormal! */
      break;
  }
  out: return(retval);
}

void noise_caused_at(const tCoordinate x, const tCoordinate y,
  const tNoiseFlags flags, const tNoiseVolume vol,
  /*@null@*/ const tCreature* by __cunused)
/* "x == coordNone" means "throughout the entire section" */
{ /* const tNoiseFlags kind = nf2kind(flags); */
  /* IMPLEMENTME: wake up nearby creatures if volume is "high enough"! */
  /* if ( (!(flags & nfNoMsg)) && (by != rogue) && (rogue_can_hear) &&
       (near enough || x == coordNone) && (kind != nkFight) )
     { print a message } */
}

void door_state_set(tObstacle* door, const tDoorState state,
  /*@null@*/ const tCreature* creature)
{ const tCoordinate x = door->x, y = door->y;
  door->obsdetail &= ~(3 << doorstateshift);
  door->obsdetail |= ((state) << doorstateshift);
  if (map_can_player_see(x, y, mfObstacle))
  { if ( (creature != rogue) && (creature != NULL) &&
         (map_can_player_see(creature->x, creature->y, mfCreature)) )
    { /* message_show("the ... opened/closed the ... door"); */
    }
    /* else message_show("the ... door opens/closes"); */
  }
  else if ( (rogue_can_hear) && (!door_passable(state)) /* && (same room) */ )
  { /* message_show("a wooden/metal door slams shut"); */ }
  /* IMPLEMENTME: call noise_caused_at() instead! And an opening metal door may
     make a sound too... */
  if ( (creature == rogue) && (state == dsBroken) )
  { /* IMPLEMENTME: if the door was made of stone or metal, let some remains
       (a few stones or pieces of the respective metal) appear around the door
       location sometimes! */
    /* IMPLEMENTME: if (it was the door of a shop)
       { infuriate_shopkeeper(NULL, falsE); } */
  }
  if ( (creature != rogue) &&
    (map_can_player_see(door->x, door->y, mfObstacle)) )
  { /* IMPLEMENTME: print a message? (if cannot see but can hear, print a
       message about the noise) */
  }
  /* IMPLEMENTME: opening metal doors sometimes make a loud creaking sound
     which wakes up nearby creatures or makes them more aggressive! */
  /* IMPLEMENTME: if the player character touched the door directly, he may be
     hurt by some effect, e.g. electricity (if metal) or magic energy or heat!
     He may even freeze for a few ticks when a metal door is ice-cold. And some
     doors should even be "booby-trapped"... */
  mrm_fov = truE;
}

void gush_of_water(void)
{ tObject *armor = rogue->armor, *h = roguedata_os(rdosiHelmet);
  if (!rogue_can_see) message_show(strSomethingHits);
  else
  { sprintf(strbuf, "a gush of water hits %s head", rogue_pronoun(2));
    message_show(strbuf);
  }
  if (h != NULL) object_gets_wet(h, 98 + difficulty, strHelmet);
  if (armor != NULL) object_gets_wet(armor, 90 + 2 * difficulty, strArmor);
}

tBoolean section_might_trap(void)
{ return(cond2boolean( (!rogue_in_shop) && (current_section->traps_left > 0)));
}

static tBoolean trapdoors_allowed(void)
/* for trapdoors and ground-digging */
{ tBoolean retval;
  const tDepData* const dd = current_dd;
  const tDungeonDepKind ddk = dd->kind;
  const tStoreyNumber storey = current_storeynum, max_storey = dd->max_storey;
  if (current_section->flags & secfNoTeleport) /* no teleport, no trapdoors */
  { failed: retval = falsE; goto out; }
  if (rogue_in_shop) goto failed;
  switch (ddk)
  { case ddkMine: case ddkSand: /* may be okay */ break;
    default:
      if (storey < 10 - difficulty) goto failed; /* too "early" during game */
      break;
  }
  if (storey >= max_storey)
    goto failed; /* already at the "bottom" of the department */
  retval = truE;
  out: return(retval);
}

void section_handle_traps(void)
/* checks for one-time dungeon traps; only the player character can get trapped
   this way - other creatures shall know how to avoid triggering such traps
   because they grew up in the dungeon... */
{ tBoolean trapped, trapmsg, trap_fails, is_blind=cond2boolean(!rogue_can_see),
    is_immaterial = cond2boolean(rogue->flags & cfImmaterial);
  const tSquareFlags sflags = squareflags_get(rogue->x, rogue->y);
  const tRandomByte rb = randbytemod(100); /* for trap kind selection */
  const tStoreyNumber storey = current_storeynum;
  const tObject* armor;

  trapped = trapmsg = falsE;
  trap_fails = ( (current_section->traps_left < TRAPPEDVAL) ? truE :
    randpercent(50 - 10 * difficulty - storey + rogue_rings2fortune(20)));

  if ( (!rogue->held) && (!creature_is_in_the_air(rogue)) &&
    (!(sflags & (sfFatalGround | sfPlatform | sfSeconn))) )
  { if ( (rb <= 1) && (!trap_fails) && (trapdoors_allowed()) )
    { current_section->traps_left -= TRAPPEDVAL;
        /* (_must_ be done _manually_ here due to the updownsec() call) */
      if (updownsec(falsE))
      { sprintf(strbuf, "%s fall down a trapdoor", rogue_pronoun(0));
        message_show(strbuf); trapped = trapmsg = truE;
      }
      goto done0;
    }
    if ( (rb <= 3) && (!is_immaterial) && (current_dd->kind == ddkMain) &&
      (roguedata.rds.sn_deepest >= 10 - difficulty) ) /* lil' earthquake */
    { message_show("the ground shakes"); trapmsg = truE;
      if (!trap_fails)
      { tPercentage p = 2 * difficulty + 12 - rogue->experience;
        tObject *object, *next;
        if (p < 5) p = 5;
        creature_confuse(rogue, difficulty + randbytemod(5) + 1);
        if ( ( (object = rogue->weapon) != NULL ) && (!(object->flags &
          ofProtected)) && (randpercent(10 * difficulty + 5 -
          rogue->experience / 3)) )
        { creature_unwield(rogue);
          creature_drop_object(rogue, object, object->quantity, truE);
        }
        object = rogue->objects;
        while (object != NULL)
        { next = object->next;
          if ( (object->oc == ocAmulet) && (difficulty <= dkNormal) )
            goto next_gs_obj; /* don't drop da amulet */
          if ((!(object->flags & (ofInUse|ofProtected))) && (__randpercent(p)))
            creature_drop_object(rogue, object, object->quantity, truE);
          next_gs_obj: object = next;
        }
        /* IMPLEMENTME: let a few small creatures seemingly fall down from the
           ceiling and nearby walls! */
        trapped = truE; rogue_incmagicen(randbytemod(6 - difficulty) + 1);
        goto falling_stones;
      }
      goto done;
    }
    if ( (rb <= 20) && (!is_immaterial) )
    { sprintf(strbuf, "%s get stuck in a bear trap", rogue_pronoun(0));
      message_show(strbuf); trapmsg = truE;
      if (trap_fails) message_show("it breaks");
      else
      { rogue->held = 3 + randbytemod(difficulty + storey / 5 + 2);
        roguedata.holding_cause = hcBearTrap; trapped = truE;
      }
      goto done;
    }
    if ( (rb <= 30) && (storey >= 3) && (!trap_fails) )
    { sprintf(strbuf, "%s fall into a pit", rogue_pronoun(0));
      message_show(strbuf); trapped = trapmsg = truE;
      rogue->held = 3 + randbytemod(difficulty + 1);
      roguedata.holding_cause = hcPit;
      mrm_light = truE; /* esp. necessary if player character carries light */
      goto done;
    }
  }

  if ( (rb <= 40) && (!is_immaterial) && (!trap_fails) &&
       ( (storey > 6 - difficulty) || ( (armor = rogue->armor) == NULL ) ||
         (!(object_materials(armor) & bomMetalN)) ) )
  { /* (The armor tests are just a "be nice to the player in early storeys", so
        that the armor doesn't rust there.) */
    gush_of_water(); trapped = trapmsg = truE; goto done;
  }

  if (rb <= 50)
  { tObject *o, *h;
    falling_stones:
    if ( (o = object_create(ocStone, randcointoss() ? stkGray : stkBlack, 0))
      != NULL )
    { o->quantity = randbytemod(2) + 2; /* >= 2, thus always plural (KISS) */
      o->enchantment = randbytemod(2) - 2;
    }
    if (is_immaterial) trap_fails = truE;
    else if ( (!trap_fails) && ( (h = roguedata_os(rdosiHelmet)) != NULL ) &&
      (object_helmet_armorval(h) > 1) )
    { trap_fails = truE; } /* helmet prevents damage; IMPLEMENTME properly! */
    if (trap_fails)
    { if (!is_blind)
      { message_show("stones fall down from the ceiling"); trapmsg = truE; }
      if ( (o) && (!object_randlocatearound(o, rogue->x, rogue->y)) )
        object_deallocate(o);
    }
    else
    { if (is_blind) message_show(strSomethingHits);
      else
      { sprintf(strbuf, "stones fall on %s head", rogue_pronoun(2));
        message_show(strbuf);
      }
      if (!creature_dec_health(rogue, 1)) rogue_died(drInjury, NULL);
      if (o)
      { if (creature_can_carry_object(rogue, o, 1))
          (void) creature_add_object(rogue, o);
        else section_put_object_carefully(o, rogue->x, rogue->y, 2);
      }
      trapped = trapmsg = truE;
    }
    goto done;
  }

  if (rb <= 75)
  { const char* const name = (is_blind ? strSomething : "a little dart");
    tObject *o = object_create(ocWeapon, wkPoisonDart, 0), *cape;
    tPropertyExtent ce; /* cape enchantment */
    if (o) { o->quantity = 1; o->enchantment = -4 - randbytemod(3); }
    if (is_immaterial) trap_fails = truE;
    else if ( (!trap_fails) && ( (cape = roguedata_os(rdosiCape)) != NULL ) &&
      (cape->kind == tokCapeOilskin) && ( (ce = cape->enchantment) >= 0 ) &&
      (randpercent(95 - 10 * difficulty + 20 * zero2one(ce))) )
    { trap_fails = truE; }
    if (trap_fails)
    { sprintf(strbuf, "%s howls past %s ear", name, rogue_pronoun(2));
      message_show(strbuf);
      if (o)
      { if (!object_randlocatearound(o, rogue->x, rogue->y))
          object_deallocate(o);
      }
    }
    else
    { tDamage damage = 1 + boolean2bool(randpercent(40 - /* what might this */
        ( ( (armor = rogue->armor) != NULL ) ? (10 * /* mean? (-: */
        armor->damage + 5 * armor->enchantment) : (0) ) + 2 * storey + 10 *
        difficulty ));
      sprintf(strbuf, "%s hits %s in the shoulder", name, rogue_pronoun(1));
      message_show(strbuf);
      if (!creature_dec_health(rogue, damage)) rogue_died(drPoison, NULL);
      if (o)
      { if (creature_can_carry_object(rogue, o, 1))
          (void) creature_add_object(rogue, o);
        else section_put_object_carefully(o, rogue->x, rogue->y, 2);
      }
      trapped = truE;
    }
    trapmsg = truE; goto done;
  }

  if (rb <= 94)
  { const tObject* s;
    message_show(strIsogh);
    if ( (!trap_fails) && ( (s = roguedata_os(rdosiShawl)) != NULL ) &&
      (s->enchantment >= 0) )
    { const tObjectKind shk = s->kind;
      if ( (shk == tokShawlGas) || (shk == tokShawlBreath) )
        trap_fails = truE;
    }
    if (!trap_fails)
    { const tFewTicks gt = 3 + randbytemod(4);
      if (randpercent(90 - 20 * difficulty - 3 * current_storeynum))
      { if (!rogue->confused)
        { creature_confuse(rogue, gt);
          if (rogue->confused) trapped = truE;
        }
        else if (!rogue->hallucinating)
        { creature_hallucinate(rogue, gt);
          if (rogue->hallucinating) trapped = truE;
        }
      }
      if (!trapped) /* the "harmless" stuff didn't have an effect */
      { creature_nap(rogue, gt, 0);
        if (rogue->napping) trapped = truE;
      }
    }
    trapmsg = truE; goto done;
  }

  if ( (rb <= 97) && (!is_blind) )
  { if (storey <= 2 - difficulty / 2) trap_fails = truE;
    if (!trap_fails) trapped = rogue_dazzle(falsE);
    else
    { sprintf(strbuf, "%s see a dim flashlight", rogue_pronoun(0));
      message_show(strbuf);
    }
    trapmsg = truE; goto done;
  }

  if ( (storey >= 3) && (!trap_fails) )
  { /* IMPLEMENTME: in rare cases, if high pref_feat and in ddkMain and not
       immaterial, the dungeon is flooded with water (show a little animation)
       and the player character gets flushed into a different section in the
       same storey. */
    creature_teleport(rogue); trapped = trapmsg = truE; goto done;
  }
  /* IMPLEMENTME: spiked pit; rolling boulder trap! */
  /* IMPLEMENTME if high pref_feat: "The room starts contracting." When the
     player character can't get out of the room quickly enough, he gets
     squashed by the contracting walls. */
  /* IMPLEMENTME: if the player character has some magic energy: "you/I feel
     your/my magic energy drain away!" The character's energy is reduced and
     e.g. a few pkMagicEnergy potions appear on nearby squares. To get the
     energy back, the player must collect and quaff these potions, and maybe
     some of them are cursed. */

  /* No trap triggered, that's strange. :-) */
  strangemsg(); trapmsg = truE;

  done:
  if (trapped) current_section->traps_left -= TRAPPEDVAL;
  else
  { /* Even a failing trap causes a decrease. Otherwise it would be possible
       (in theory) that the player is bored with an infinite number of failing
       traps. */
    current_section->traps_left--;
  }
  done0:
  if (trapmsg)
  { roguedata.flags |= rfTrapmsg;
    if (trapped) roguedata.traptick = 0; /* restart step counter */
    else roguedata.traptick /= 3; /* delay next trigger somewhat */
  }
}

void section_perform_tick(void)
/* handles the section-related part of "performing a dungeon clock tick" */
{ tSection* const section = current_section;
  if (section->flags & secfHasDeadCreatures)
    section_remove_dead_creatures(section);
  section_age_objects(1);
  /* IMPLEMENTME: move movable platforms etc. */
  /* IMPLEMENTME: sometimes, fire fountains should come out of the walls - at
     least for some high difficulties/storeys, esp. in ddkLava sections. */
}

tBoolean map_can_player_see(const tCoordinate x, const tCoordinate y,
  const tMapFlags flags0)
/* checks whether the player (character) can see the given square on the map */
{ tBoolean retval;
  tSquareFlags sflags;
  const tMapField* m;
  if ( (flags0 & mfRogueRelated) && (x == rogue->x) && (y == rogue->y) )
  { yes: retval = truE; goto out; }
  sflags = squareflags_get(x, y);
  if (sflags & sfSpotlight) goto yes; /* it's magic */
  if ( (flags0 & mfCreatureRelated) && (rogue->flags & cfSensCreat) ) goto yes;
  if ( (flags0 & mfObjectRelated) && (roguedata.flags & rfSenseObjects) )
    goto yes;
  if (sflags & sfMist) goto failed;
  m = mapfieldptr(x, y);
  if (M_SEEABLE(x, y))
  { if (flags0 & mfCreatureRelated)
    { const tCreature* creature;
      if (!(sflags & sfCreature)) goto failed; /* may happen! */
      creature = location2creature(x, y);
      if ( (creature != NULL) && (creature->flags & cfInvisible) )
        goto failed;
      /* CHECKME: when the player character hallucinates, should we sometimes
         show invisible creatures? */
    }
    if ( (flags0 & mfObjectRelated) && (sflags & sfObstacle) )
    { goto failed; } /* an obstacle (usually a chest) hides the object */
    if ( (flags0 & mfStairsDown) && (sflags & sfPlatform) ) goto failed;
    goto yes;
  }
  failed: retval = falsE;
  out: return(retval);
}

void map_magic(const tBoolean is_ench)
/* puts a "magic map" of the current section on the screen - or "forgets" the
   player's map-knowledge; CHECKME: animation? */
{ static const tMapFlags mf = mfGround | mfStairs | mfOther; /* CHECKME! */
  tCoordinate width = current_section->width, height = current_section->height,
    x, y;
  if (is_ench)
  { for (y = 0; y < height; y++)
    { for (x = 0; x < width; x++)
      { const tSquareFlags sflags = squareflags_get(x, y);
        if (sflags & (sfWall | sfDoorway | sfTunnel | sfSeconn | sfHidden))
        { if (sflags & sfWall) squareflags_or(x, y, sfKnownWall);
          if (sflags & sfDoorway) squareflags_or(x, y, sfKnownDoorway);
          if (sflags & sfTunnel) squareflags_or(x, y, sfKnownTunnel);
          if (sflags & sfSeconn) squareflags_or(x, y, sfKnownSeconn);
          if (sflags & sfHidden) section_unhide_square(x, y);
          map_update(x, y, mf);
        }
      }
    }
  }
  else /* cursed */
  { for (y = 0; y < height; y++)
    { for (x = 0; x < width; x++)
      { if (squareflags_get(x, y) & sfKnownAny)
        { squareflags_andnot(x, y, sfKnownAny); map_update(x, y, mf); }
      }
    }
  }
  mrm_screen = truE;
}

#define FPFACTOR (5)
#define field2point(f) ((f) * FPFACTOR)
#define field2midpoint(f) ((f) * FPFACTOR + FPFACTOR / 2)
#define point2field(f) ((f) / FPFACTOR)

static void losq_around(const tCoordinate x0, const tCoordinate y0)
{ const tSection* const section = current_section;
  tCoordinate x1 = x0 - 1, x2 = x0 + 1, y1 = y0 - 1, y2 = y0 + 1, x, y,
    sw = section->width, sh = section->height;
  if (x1 < 0) x1 = 0;
  if (x2 >= sw) x2 = sw - 1;
  if (y1 < 0) y1 = 0;
  if (y2 >= sh) y2 = sh - 1;
  for (y = y1; y <= y2; y++)
  { for (x = x1; x <= x2; x++)
    { const tSquareFlags sflags = squareflags_get(x, y);
      if (sflags == 0) continue; /* solid rock */
      if ( (sflags & (sfTunnel | sfDoorway)) && (!(sflags & sfHidden)) )
        mapfieldptr(x, y)->losq |= ~0;
    }
  }
}

static void recalc_map_fov(void)
/* calculates the current field of view / line of sight; purely geometrical,
   not related to brightness */
{ const tSection* const section = current_section;
  const tCoordinate rx = rogue->x, ry = rogue->y, origpx = field2midpoint(rx),
    origpy = field2midpoint(ry), sw = section->width, sh = section->height;
  tCoordinate x, y, x1, y1, x2, y2, px, py;
  mapdirt_all(); /* IMPROVEME! */
  for (y = 0; y < sh; y++)
  { for (x = 0; x < sw; x++)
    { mapfieldptr(x, y)->losq = ( (squareflags_get(x, y) & sfSpotlight) ? (~0)
        : (0) );
    }
  }
  mapfieldptr(rx, ry)->losq |= ~0;
  if (!rogue_can_see) { /* nothing */ }
  else if ( (pref_feat <= 0) || (current_dd->kind == ddkPast) || (1) )
  { const tRoomIndex r = creature_room(rogue);
    losq_around(rx, ry);
    if (r != roomNone)
    { const tRoom* const ro = &(section->room[r]);
      x1 = ro->left; x2 = ro->right; y1 = ro->top; y2 = ro->bottom;
      for (y = y1; y <= y2; y++)
      { for (x = x1; x <= x2; x++) mapfieldptr(x, y)->losq |= ~0; }
    }
  }
  else
  { /* FIXME! */
  }
  mrm_fov = falsE; mrm_screen = truE;
}

static tBoolean light_add(/*inout*/ tLightIntensity* _l,
  const tLightIntensity add)
{ tBoolean retval;
  if (*_l >= 255 - add) { *_l = 255; retval = truE; } /* reached limit */
  else { *_l += add; retval = falsE; }
  return(retval);
}

static void light_spread(const tCoordinate x0, const tCoordinate y0,
  const tLightIntensity value, const tUint8 radius)
{ if (radius <= 0) light_add(map_brptr(x0, y0), value); /* most likely case */
  else
  { const tSection* const section = current_section;
    tCoordinate x1 = x0 - radius, x2 = x0 + radius, y1 = y0 - radius,
      y2 = y0 + radius, w = section->width, h = section->height, x, y, rdist;
    if (x1 < 0) x1 = 0;
    if (x2 >= w) x2 = w - 1;
    if (y1 < 0) y1 = 0;
    if (y2 >= h) y2 = h - 1;
    if (radius <= 1) /* rectangular lighting */
    { for (y = y1; y <= y2; y++)
      { for (x = x1; x <= x2; x++)
        { if (squareflags_get(x, y) & (sfRoom | sfDoorway | sfTunnel))
          { const tUint8 v = ( ( (x == x0) && (y==y0) ) ? value : (value-30) );
            light_add(map_brptr(x, y), v);
          }
        }
      }
      return;
    }
    rdist = radius * radius;
    for (y = y1; y <= y2; y++)
    { for (x = x1; x <= x2; x++)
      { const tCoordinate xdist = ABS(x - x0), ydist = ABS(y - y0),
          dist = xdist * xdist + ydist * ydist, v = 30 * dist;
        if (dist <= rdist)
        { tCoordinate v2 = ( (value > v) ? value - v : 0 );
          if (v2 < BRIGHTNESS_CLEAR) v2 = BRIGHTNESS_CLEAR; /* CHECKME! */
          light_add(map_brptr(x, y), v2);
        }
      }
    }
  }
}

static void recalc_map_light(void)
{ const tSection* const section = current_section;
  const tCoordinate sw = section->width, sh = section->height;
  tCoordinate x, y;
  my_memclr(map_brightness, mbsize);
  for (y = 0; y < sh; y++)
  { for (x = 0; x < sw; x++)
    { tSquare* const s = squareptr(x, y);
      const tSquareFlags sflags = s->sflags;
      tLightIntensity v = s->lightvalue;
      tLightMode lm = s->lightmode;
      const tCreature* creature;
      const tObject* object;
      tUint16 result = 0;
      tUint8 radius, maxrad = 0, lsc = 0;
      if (lm == lmRefRoom)
      { const tRoom* const ro = &(section->room[v]);
        lm = ro->lightmode; v = ro->lightvalue;
      }
      switch (lm2bm(lm))
      { case lmEqual: result += v; break;
      }
      if (sflags & sfSpotlight) result += 255;
      if ( (sflags&sfCreature) && ( (creature=location2creature(x, y))!=NULL) )
      { if (creature == rogue)
        { if (lm & lmAroundPc) /* "character light" */
          { cl: result += 255; lsc++;
            if (maxrad < 2) maxrad = 2;
          }
          else if (sflags & sfDoorway) /* IMPLEMENTME properly! */
          { result += 255; lsc++;
            if (maxrad < 1) maxrad = 1;
          }
        }
        else { if (lm & lmAroundNpc) goto cl; }
        if ( (object = creature->objects) != NULL )
        { do
          { if ( (v = object_light(object, &radius)) > 0 )
            { result += v; lsc++;
              if (maxrad < radius) maxrad = radius;
            }
            object = object->next;
          } while (object != NULL);
        }
      }
      if ( (sflags & sfObject) && ( (object=location2object(x, y)) != NULL ) )
      { if ( (v = object_light(object, &radius)) > 0 )
        { result += v; lsc++;
          if (maxrad < radius) maxrad = radius;
        }
      }
      if (result > 255) result = 255;
      if (lsc > 1) maxrad++;
      light_spread(x, y, result, maxrad);
    }
  }
  mrm_light = falsE; mrm_screen = truE;
}

static tBoolean rmfdk_sc, rmfdk_so;

static void recalc_mfdk(const tCoordinate sectx, const tCoordinate secty)
{ const tSquareFlags sflags = squareflags_get(sectx, secty);
  tMapField* const m = mapfieldptr(sectx, secty);
  tMfdk mfdk;
  if (sflags == 0) mfdk = mfdkEmpty; /* quite likely */
  else if ( (sectx == rogue->x) && (secty == rogue->y) ) goto set_creature;
  else if (M_SEEABLE(sectx, secty)) /* can see directly */
  { const tCreature* creature;
    const tObstacle* obstacle;
    if (sflags & (sfMist | sfFire))
    { if ( (sflags & sfCreature) && (rmfdk_sc) ) goto set_creature;
      else if ( (sflags & sfObject) && (rmfdk_so) && (!(sflags & sfFire)) )
        goto set_object;
      else mfdk = mfdkMisty;
    }
    else if ( (sflags & sfCreature) && ( (rmfdk_sc) || ( (creature =
      location2creature(sectx, secty)) == NULL ) || (!(creature->flags &
      cfInvisible))) )
    { set_creature: mfdk = mfdkCreature; }
    else if (sflags & sfBarrier) mfdk = mfdkBarrier;
    else if ( (sflags & sfWall) &&
      ( (!(sflags & sfDoorway)) || (sflags & sfHidden) ) )
    { mfdk = mfdkWall; }
    else if ( (sflags & sfObstacle) && (!(sflags & sfHidden)) && ( (obstacle =
      location2obstacle(sectx,secty))!=NULL ) && (obstacle_showable(obstacle)))
    { mfdk = mfdkObstacle; }
    else if (sflags & sfObject) { set_object: mfdk = mfdkObject; }
    else if (sflags & sfSeconn)
    { if ( (sflags & sfPlatform) && (!(seconn_downup_at(sectx, secty) & 2)) )
        goto set_platform; /* downward stairs are "under" the platform */
      mfdk = mfdkSeconn;
    }
    /* IMPLEMENTME: mfdkChain, mfdkIronBall! */
    else if (sflags & sfPlatform) { set_platform: mfdk = mfdkPlatform; }
    else if ( (sflags & sfFatalGround) /* and not invisible */ )
      mfdk = mfdkGroundFeature;
    else mfdk = mfdkGround;
  }
  else /* maybe sensing or prior knowledge */
  { if ( (sflags & sfCreature) && (rmfdk_sc) ) goto set_creature;
    else if ( (sflags & sfObject) && (rmfdk_so) ) goto set_object;
    else if ( (sflags & sfKnownWall) && (!(sflags & sfKnownDoorway)) )
      mfdk = mfdkWall;
    else if (sflags & sfKnownSeconn) mfdk = mfdkSeconn;
    else if (sflags & sfKnownTunnel) mfdk = mfdkGround;
    else mfdk = mfdkEmpty;
  }
  m->dispkind = mfdk;
}

static one_caller void map_putchar(const tCoordinate sectx,
  const tCoordinate secty)
{ enum { barrnum = 4 };
  static const char barrier[barrnum] = { '-', '|', '/', '\\' };
  const tSquareFlags sflags = squareflags_get(sectx, secty);
  const tMfdk mfdk = mapfieldptr(sectx, secty)->dispkind;
  const tCreature* creature;
  const tObstacle* obstacle;
  const tObject* object;
  chtype ch;
  unsigned char c;
  switch (mfdk)
  { case mfdkEmpty: ch = ' '; break;
    case mfdkWall: ch = ( (sflags & sfVWall) ? '|' : '-' ); break;
    case mfdkCreature:
      if ( (creature = location2creature(sectx, secty)) == NULL )
      { ch = charBug | cbCreature; goto finish; }
      /* FIXME: sensing, vague! */
      if (creature->flags & cfImitates)
      { ch = (chtype) IMI2CHAR(creature->cd); goto handle_objectchar; }
      c = creature->ch;
      if ( (creature->napping | creature->frozen) && (my_isupper(c)) )
        c = _my_tolower(c); /* non-acting creature gets lowercase letter */
      ch = c;
      if (creature != rogue) ch |= cbCreature;
      break;
    case mfdkBarrier: /* (yes, the character "flips") */
      ch = barrier[randbytemod(barrnum)] | cbBarrier; break;
    case mfdkObstacle:
      ch = '{' | cbObstacle; /* "generic" look of obstacles */
      if ( (obstacle = location2obstacle(sectx, secty)) != NULL )
      { tObstacleDetail od;
        switch (obstacle->kind)
        { case obkDoor:
            od = obstacle->obsdetail;
            if (door_state(od) == dsOpen)
              ch = (door_was_vertwall(od) ? '-' : '|');
            else ch = '+'; /* closed/locked */
            break;
          case obkPileOfStone:
            ch = strObjectClassChars[ocStone] | cbObstacle; break;
          case obkBoulder: ch = '`'; break; /* like in NetHack */
          case obkGlassCube: ch = '#' | cbObstacle; break;
          case obkBlackHole: ch = 'o' | cbObstacle; break;
          case obkStoneTable: ch = '_' | cbObstacle; break;
          case obkTempWall: ch = '-' | cbObstacle; break;
          case obkStalagmite: ch = '|' | cbObstacle; break;
          case obkIronBars: ch = 'I' | cbObstacle; break;
        }
      }
      break;
    case mfdkObject:
      if ( (object = location2object(sectx, secty)) == NULL )
      { ch = charBug | cbObject; goto finish; }
      ch = object_get_char(object);
      handle_objectchar: ch |= object_get_charcolor(ch); break;
    case mfdkSeconn: ch = seconn_ch(sectx, secty); break;
    case mfdkPlatform: /* (yes, the same character as for tunnel) */
      ch = '#' | ( (sflags & sfGlazedFrost) ? cbIce : cbPlatform ); break;
    case mfdkGroundFeature:
      ch = '~' | ( (sflags & sfSwamp) ? cbSwamp : cbWater ); break;
    case mfdkGround: ch = ( (sflags & sfTunnel) ? '#' : '.' ); break;
    case mfdkMisty: ch = '~' | ( (sflags & sfFire) ? cbFire : cbMist ); break;
    /* IMPLEMENTME: mfdkChain! */
    case mfdkIronBall: ch = 'o'; break;
    default: /* bug */
      ch = charBug; break;
  }
  finish: (void) addch(ch);
}

static void recalc_map_screen(void)
{ const tCoordinate vx1 = viewrect.x1, vw = viewrect.w, vh = viewrect.h;
  tCoordinate vy1 = viewrect.y1, x, y;
  rmfdk_sc = cond2boolean(rogue->flags & cfSensCreat);
  rmfdk_so = cond2boolean(roguedata.flags & rfSenseObjects);
  for (y = 0; y < vh; y++, vy1++)
  { /* if (!mapdirty(y)) continue; -- too risky for now; IMPLEMENTME! */
    mapdirt_clr(y); (void) move(vy1, vx1);
    for (x = 0; x < vw; x++)
    { if (1)
      { const tCoordinate sectx = origin_x + x, secty = origin_y + y;
        const tSquareFlags sflags = squareflags_get(sectx, secty);
        if (sflags & (sfWall | sfTunnel | sfSeconn))
        { const tMapField* const m = mapfieldptr(sectx, secty);
          if (M_SEEABLE(sectx, secty))
          { if (sflags & sfWall) squareflags_or(sectx, secty, sfKnownWall);
            if ( (sflags & sfDoorway) && (!(sflags & sfHidden)) )
              squareflags_or(sectx, secty, sfKnownDoorway);
            if (sflags & sfTunnel) squareflags_or(sectx, secty, sfKnownTunnel);
            if (sflags & sfSeconn) squareflags_or(sectx, secty, sfKnownSeconn);
          }
        }
        recalc_mfdk(sectx, secty); map_putchar(sectx, secty);
      }
    }
  }
  mrm_screen = falsE; cursor_reset_position();
}

void map_do_update_all(void)
/* updates all map squares on the screen for which this seems to be necessary/
   appropriate; the _caller_ must call doupdate() afterwards! */
{ if (!may_display_section) return; /* procrastinate :-) */
  if (must_recalc_origin) recalc_origin();
  if (mrm_fov) recalc_map_fov();
  if (mrm_light) recalc_map_light();
  if (mrm_screen) recalc_map_screen(); /* final step: show stuff on screen */
}

#if 0 /* OPTION_ANIMATION */
void map_undo_animation(const tCoordinate x, const tCoordinate y)
/* undoes the effect of animation for the given location */
{ map_putchar(x, y);
}
#endif

void __init dungeon_initialize(const unsigned char flags0)
/* initializes the handling of sections and maps and creates the first sec'n */
{ enum { hm = 35 }; /* hard maximum storey number */
  tDepIdx dep_idx;
  tSection* section;
  tStoreyNumber sn;
  unsigned char x;
  my_memclr_var(dude); dude.sn_hardmax = hm;
  if (flags0 & 1) return; /* no full initialization necessary */
  dep_idx = department_create(ddkMain, 1, hm);
  dude.dd[dep_idx].name = "Deep Dungeon";
  dude.sn_aoy = sn = 26; dude.sn_allhasted = sn + 2;
  dude.sn_hitchance100 = sn + 4;
  map_width = max_section_width; map_height = max_section_height;
  mapsize = map_width * map_height * sizeof(tMapField);
  currmap = memory_allocate(mapsize);
  mbsize = map_width * map_height * sizeof(tLightIntensity);
  map_brightness = __memory_allocate(mbsize);
  mapdirtsize = (max_section_height + 7) / 8;
  mapdirt = memory_allocate(mapdirtsize);
  if ( (difficulty <= dkNormal) || (pref_feat <= 0) ) x = 1;
  else
  { const tPercentage p = 6 * difficulty + 4 * (pref_expl + pref_magic) +
      2 * pref_feat;
    x = difficulty - dkNormal + 1;
    if (randpercent(p)) x++;
#if CONFIG_DEBUG
    sprintf(debugstrbuf, "AoY: %d (%d%%)",x,p); message_showdebug(debugstrbuf);
#endif
  }
  dude.aoy_pieces = x;
  if (difficulty > dkNormal) sn = 7 - 2 * (difficulty - dkNormal);
  else sn = 28 - 4 * difficulty - 2 * pref_expl;
  dude.sn_unidir = sn;
  dude.sn_water = ((pref_feat > 0) ? (18 - ((1 << difficulty) / 2)) : (hm+5));
  section = current_section = section_allocate();
  section_create(section, dep_idx, 1, NULL); section_enter(section, NULL, 0);
}
