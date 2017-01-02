/* atrogue/dungeon.h - dungeon storeys, sections and rooms; map handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_dungeon_h__
#define __atrogue_dungeon_h__

#include "stuff.h"
#include "creature.h"

extern tBoolean mrm_fov, mrm_light, mrm_screen, must_recalc_origin;
  /* "mrm": must_recalc_map; "fov": field_of_view */

/* We need a way of measuring how large the sections are; e.g. it would be
   nonsense to put the same number of objects in the smallest and largest
   possible sections. So we declare the following measure and say that it shall
   be 0 for the smallest and 10000 for the largest possible section size: */
typedef unsigned short tSectionSizeMeasure;
extern tSectionSizeMeasure sectionsizemeasure;

#define MAXNUM_ROOMS (15) /* maximum number of rooms in one section */
#define roomNone (MAXNUM_ROOMS + 1)
typedef tUint16 tRoomBits; /* must be big enough to hold "maxRoomNumber + 1"
  bits! */

typedef tUint8 tDoorwayIdx;
#define MAXNUM_DOORWAYS (4) /* at most one doorway for each "side" of a room */
#define doorwayNone (255) /* invalid index */

my_enum1 enum
{ rokNormal = 0, rokShop = 1, rokParty = 2, rokHut = 3, rokStorage = 4,
  rokCavern = 5, rokBigRoom = 6
} my_enum2(tUint8) tRoomKind;

my_enum1 enum
{ rsRectangle = 0, rsHexagon = 1, rsOctagon = 2, rsCircle = 3
} my_enum2(tUint8) tRoomShape;

my_enum1 enum
{ lmNone = 0,
  /* basic modes: */
  lmRefRoom = 1, lmEqual = 2, lmCentered = 3,
  /* flags: */
  lmWalls = 0x04, lmAroundPc = 0x08, lmAroundNpc = 0x10
} my_enum2(tUint8) tLightMode;
#define lmAroundCreatures (lmAroundPc | lmAroundNpc)
#define lm2bm(lm) ((lm) & 3)

typedef struct
{ tCoordinate left, right, top, bottom; /* position of the room */
  tCoordinate doorway_x[MAXNUM_DOORWAYS], doorway_y[MAXNUM_DOORWAYS];
  tDoorwayIdx num_doorways; /* number of existing doorways for this room */
  tLightMode lightmode;
  unsigned char lightvalue; /* meaning depends on <lightmode> */
  tRoomKind kind;
  tRoomShape shape;
} tRoom;

my_enum1 enum
{ ddkUnknown = 0, ddkMain = 1, ddkDeadEnd = 2, ddkMine = 3, ddkMagic = 4,
  ddkSpooky = 5, ddkGraveyard = 6, ddkDark = 7, ddkCrystal = 8, ddkPlant = 9,
  ddkRiver = 10, ddkFrozen = 11, ddkLava = 12, ddkPrison = 13, ddkTown = 14,
  ddkSand = 15, ddkCastle = 16, ddkAnthole = 17, ddkPast = 18, ddkFuture = 19,
  ddkMaze = 20
} my_enum2(unsigned char) tDungeonDepKind; /* dungeon department kind */
#define _ground_is_hard (current_dd->kind != ddkSand)

/* Squares */

my_enum1 enum
{ sfNone = 0, sfRoom = 0x01, sfHWall = 0x02, sfVWall = 0x04, sfDoorway = 0x08,
  sfTunnel = 0x10, sfCreature = 0x20, sfObject = 0x40, sfObstacle = 0x80,
  sfHBarrier = 0x100, sfVBarrier = 0x200, sfSeconn = 0x400, sfWater = 0x800,
  sfSwamp = 0x1000, sfFire = 0x2000, sfGlazedFrost = 0x4000,
  sfStepstones = 0x8000, sfFerry = 0x10000, sfHidden = 0x20000,
  sfSpotlight = 0x40000, sfKnownDoorway = 0x80000, sfKnownWall = 0x100000,
  sfKnownTunnel = 0x200000, sfKnownSeconn = 0x400000, sfMist = 0x800000
} my_enum2(tUint32) tSquareFlags;
#define sfWall (sfHWall | sfVWall)
#define sfBarrier (sfHBarrier | sfVBarrier)
#define sfFatalGround (sfWater | sfSwamp)
#define sfPlatform (sfGlazedFrost | sfStepstones | sfFerry)
#define sfKnownAny \
  (/* sfKnownDoorway | */ sfKnownWall | sfKnownTunnel | sfKnownSeconn)
/* IMPLEMENTME: sfFire, sfStepstones, sfFerry, sfBridge! */

typedef struct
{ tSquareFlags sflags;
  tRoomIndex room; /* to which room the square belongs (default: roomNone) */
  tLightMode lightmode;
  tLightIntensity lightvalue; /* "basic" lighting of the square */
} tSquare;

/* On any square, we can have the following stuff, sorted from the ground
   upwards: fatal ground - platform - object - obstacle - creature. Stuff like
   room, wall and barrier doesn't describe what's _on_ the square, it rather
   describes the "kind" of the square. */

/* Some other possibly interesting squareflags to think about:
   - for sfFatalGround: sfPrecipice */

#define _squareptr(section,x,y) (&((section)->squares[section->width*(y)+(x)]))
#define squareptr(x, y) _squareptr(current_section, x, y)
#define squareflags(x, y) (squareptr((x), (y))->sflags)
#define squareflags_get(x, y) squareflags((x), (y))
#define squareflags_set(x, y, value) squareflags((x), (y)) = (value)
#define squareflags_or(x, y, value) squareflags((x), (y)) |= (value)
#define squareflags_andnot(x, y, value) squareflags((x), (y)) &= (~(value))

/* Ground features */

my_enum1 enum
{ /* normal ground features */
  grfkWater = 0, grfkSwamp = 1, grfkLava = 2, grfkFountain = 3, grfkSink = 4,
  grfkEngraving = 5, grfkSqueakyBoard = 6, grfkDrawbridge = 7,
  grfkThrone = 8, grfkHeadstone = 9, grfkSignboard = 10, grfkQuicksand = 11,
  grfkVibrating = 12, grfkArrow = 13, grfkScale = 14, grfkHollow = 15
  /* permatraps (permanent traps) */
  /* IMPLEMENTME! */
} my_enum2(unsigned char) tGrfKind; /* ground feature kind */

my_enum1 enum
{ grffNone = 0, grffInvisible = 0x01
} my_enum2(unsigned char) tGrfFlags; /* ground feature flags */

typedef struct
{ /*@null@*/ const char* text; /* e.g. engraved text */
  tCoordinate x, y;
  unsigned short grfdetail; /* e.g. direction and buoyancy of river water */
  tPropertyExtent enchantment;
  tGrfKind kind;
  tGrfFlags flags;
  unsigned char wear_off; /* counter; e.g. engraving becomes less readable,
    hollow becomes deeper */
} tGrf; /* ground feature */

extern tBoolean ground_is_hard(const tCoordinate, const tCoordinate);

/* Obstacles */

my_enum1 enum
{ obkChest = 0, obkDoor = 1, obkCrate = 2, obkPileOfStone = 3, obkBoulder = 4,
  obkGlassCube = 5, obkMirror = 6, obkBlackHole = 7, obkTeleporter = 8,
  obkStoneTable = 9, obkTempWall = 10, obkStalagmite = 11, obkSwitch = 12,
  obkIronBars = 13,
  maxObstacleKind = obkIronBars
} my_enum2(unsigned char) tObstacleKind;

my_enum1 enum
{ obfNone = 0, obfDeallocLabel = 0x01, obfPushable = 0x02, obfPullable = 0x04,
  obfHvPushable = 0x08, obfHvPullable = 0x10, obfCrCr = 0x20, obfPlural = 0x40,
  obfInvisible = 0x80
} my_enum2(unsigned char) tObstacleFlags;
#define obfMovable (obfPushable | obfPullable)

typedef unsigned char tObstacleSize;
typedef unsigned char tObstacleResistance; /* 0..15; extent of firmness; 15
  means "undestroyable", 14 means "only destroyable by strongest magic" */
typedef unsigned short tObstacleDetail;

typedef struct tObstacle
{ tTickout tickout;
  /*@null@*/ struct tObstacle* next;
  /*@null@*/ const char* label;
  tCoordinate x, y;
  tObstacleKind kind;
  tObstacleSize size;
  tObstacleResistance resistance;
  tObstacleFlags flags;
  tObstacleDetail obsdetail; /* kind-dependent details */
} tObstacle;
/* label: e.g. doors: "Food Shop" or "Blacksmith" or "Closed for whatever
   reason"; also nice for switches, stating the effect of the switch; a chest
   may read "Acme Corp.", indicating that it'll explode :-) */

#define _CHESTMETALFLAG (1)
#define chest_is_metal(detail) ((detail) & _CHESTMETALFLAG)

my_enum1 enum
{ dsLocked = 0, dsClosed = 1, dsOpen = 2, dsBroken = 3
} my_enum2(tUint8) tDoorState;

my_enum1 enum
{ dmGlass = 0, dmWood = 1, dmStone = 2, dmMetal = 3
} my_enum2(tUint8) _tDoorMaterial; /* sorted by increasing resistance */

#define door_color(detail) ((detail) & colorcodemask)
#define doorstateshift (colorcodebits)
#define door_state(detail) ( ((detail) >> doorstateshift) & 3 )
#define doormaterialshift (doorstateshift + 2)
#define _door_material(detail) ( ((detail) >> doormaterialshift) & 3 )
#define door_is_metal(detail) (_door_material(detail) == dmMetal)
#define DOOR_VERTWALL (1 << 9)
#define door_was_vertwall(detail) ((detail) & DOOR_VERTWALL) /* else horiz. */
#define door_unclosable(detail) ((detail) & (1 << 10))
#define door_unopenable(detail) ((detail) & (1 << 11))
#define door_visible(state) ((state) != dsBroken)
#define door_passable(state) ((state) >= dsOpen)
extern void door_state_set(tObstacle*, const tDoorState,
  /*@null@*/ const tCreature*);

my_enum1 enum
{ obckZap = 0, obckThrow = 1, obckHarpoon = 2, obckBoomerang = 3
} my_enum2(unsigned char) tObstacleContactKind;

/* Seconns (section connectors) */

my_enum1 enum
{ /* downward */
  sckStairsDown = 0, sckLadderDown = 1, sckHoleDown = 2,
  /* upward */
  sckStairsUp = 3, sckLadderUp = 4, sckHoleUp = 5,
  /* possibly bi-directional */
  sckElevator = 6
} my_enum2(unsigned char) tSeconnKind;
#define sck_down(sck) ((sck) < sckStairsUp)

my_enum1 enum
{ scfNone = 0, scfBidirectional = 0x01, scfInvisible = 0x02, scfDefunct = 0x04,
  scfRequiresAoY = 0x08, scfExit = 0x10
} my_enum2(unsigned char) tSeconnFlags;
#define scfVanished (scfInvisible | scfDefunct)

struct tSection;
struct tSeconn;

typedef struct
{ struct tSeconn *from, *to; /* "to" may be NULL */
  tObject* objects; /* fell down the stairs :-) */
  unsigned char scdetail;
  tDungeonDepKind ddk; /* if "!= 0", the connection leads to a different, new
    department of that given kind */
  unsigned char refcount; /* sckElevator can have more than two endpoints! */
} tSeconnData; /* connection information */

typedef struct tSeconn
{ tTickout tickout;
  struct tSeconn* next;
  tSeconnData* data;
  struct tSection* section;
  tCoordinate x, y;
  tSeconnKind kind;
  tSeconnFlags flags;
} tSeconn; /* endpoint-specific information */

extern unsigned char sck_downup(const tSeconnKind);
#define seconn_downup(s) sck_downup((s)->kind)
  /* IMPLEMENTME properly for sckElevator! */
extern unsigned char seconn_downup_at(const tCoordinate, const tCoordinate);
extern const char* seconn_desc(const tSeconn*, const unsigned char);
extern /*@null@*/ tSeconn* location2seconn(const tCoordinate,
  const tCoordinate);

/* Sections */

typedef signed char tSectionEventCount;
typedef signed char tTrapsLeft; /* ("signed" only to simplify calculations) */
typedef unsigned char tDepIdx; /* department index; 0 is the ddkMain
  department; in some contexts, 0 means "not yet specified" or "invalid" */
struct tSectionEvent;

my_enum1 enum
{ shkGeneral = 0, shkArmor = 1, shkWeapon = 2, shkPotion = 3, shkSouvenir = 4,
  shkFood = 5, shkTool = 6, shkGem = 7, shkMagic = 8, shkBook = 9, shkIpr = 10,
  shkRing = 11, shkPet = 12, shkAntique = 13
} my_enum2(tUint8) tShopKind;

my_enum1 enum
{ secfNone = 0, secfTimezipMask = 0x03,
  secfHasDeadCreatures = 0x08, secfVolatile = 0x10, secfNoTeleportPc = 0x20,
  secfNoTeleportPcSelf = 0x40, secfNoTeleportNpc = 0x80, secfDidObjs = 0x100,
  secfEntering = 0x200
} my_enum2(tUint16) tSectionFlags;
#define section_timezip(flags) ((flags) & secfTimezipMask)
#define secfNoTeleport (secfNoTeleportPc | secfNoTeleportPcSelf | secfNoTeleportNpc)

typedef struct tSection
{ tRoom room[MAXNUM_ROOMS];
  tSquare* squares; /* pointer to square data */
  tCreature* creatures; /* list of creatures (except player character) */
  tObject* objects; /* list of objects lying around */
  tObstacle* obstacles; /* list of obstacles */
  tSeconn* seconns; /* list of section connectors */
  struct tSectionEvent* events; /* list of events going on */
  tTicks when_leaving, force_out;
  tCreatureCount creaturescount; /* number of creatures (except player
    character) */
  tCoordinate width, height, rogue_lastx, rogue_lasty;
  tRoomIndex num_rooms /* number of rooms in this section */
    /* , rogue_lastroom */ ;
  tStoreyNumber storeynum;
  tDepIdx dep_idx; /* department to which this section belongs */
  tSectionFlags flags;
  tTrapsLeft traps_left; /* how often the player character might get trapped */
  tShopKind shop_kind; /* kind of shops in this section, if any */
} tSection;

extern tSection* current_section;
#define current_storeynum (current_section->storeynum)
#define storeyperc(storey) ( ((storey) * 100) / dude.sn_hardmax )
#define current_storeyperc storeyperc(current_section->storeynum)
  /* IMPLEMENTME everywhere! */

typedef unsigned char tNoiseVolume;

my_enum1 enum
{ nfNone = 0,
  /* basic "kinds" of noise */
  nkDoorSlam = 1, nkDoorCreak = 2, nkHum = 3, nkHiss = 4, nkBurrow = 5,
  nkFight = 6, nkShriek = 7, nkExplosion = 8,
  /* flags */
  nfNoMsg = 0x10, nfWood = 0x20, nfMetal = 0x40, nfStone = 0x80
} my_enum2(unsigned char) tNoiseFlags;
#define nf2kind(flags) ((flags) & 15)

/* Dungeon details (overall structuring of the dungeon) */

my_enum1 enum
{ ddfNone = 0, ddfUpward = 0x01, ddfUnreachBranchport = 0x02
} my_enum2(unsigned char) tDepDataFlags;

typedef struct
{ /*@null@*/ const char* name; /* e.g. for messages like "Entering/Leaving the
    Mines"; also for controlled-branchporting menus */
  /*@null@*/ tSeconnData *enter, *leave;
  tDungeonDepKind kind;
  tStoreyNumber min_storey, max_storey; /* 0 means "not yet specified" */
  tDepDataFlags flags;
} tDepData; /* department data */

typedef unsigned short tDsi; /* dungeon section index (array index) */

my_enum1 enum
{ dudefNone = 0, dudefHasTopstairs = 0x01
} my_enum2(unsigned char) tDudeFlags;

typedef struct
{ tDepData* dd; /* department data array */
  tSection** sections; /* all existing sections of the dungeon */
  tDsi num_ds, maxnum_ds; /* for <sections> */
  tDepIdx num_dd, maxnum_dd, curr_dd, main_fork, mine, town, castle, rolequest;
  tStoreyNumber sn_hardmax, sn_aoy, sn_allhasted, sn_hitchance100,
    sn_deepest_dead_end_entr, sn_deepest_shop, sn_unidir, sn_water;
  tDudeFlags flags;
  unsigned char aoy_pieces;
} tDude;

extern tDude dude; /* "dungeon details" :-) */
#define current_dd (&(dude.dd[dude.curr_dd]))

/* Functions */

extern tBoolean scrloc2secloc(const tCoordinate, const tCoordinate,
  /*@out@*/ tCoordinate*, /*@out@*/ tCoordinate*);
extern tBoolean secloc2scrloc(const tCoordinate, const tCoordinate,
  /*@out@*/ tCoordinate*, /*@out@*/ tCoordinate*);
#define location2room(x, y) (squareptr(x, y)->room)
extern void section_randlocationinroom(/*@out@*/ tCoordinate*,
  /*@out@*/ tCoordinate*, /*@null@*/ /*@out@*/ tRoomIndex*);
extern void section_randlocationinroom_nonshop(/*@out@*/ tCoordinate*,
  /*@out@*/ tCoordinate*, /*@null@*/ /*@out@*/ tRoomIndex*);
extern void section_randdoorway(tCoordinate, tCoordinate, tCoordinate*,
  tCoordinate*);

extern tBoolean pass_seconn(const tSeconn*, const unsigned char);
extern tBoolean updownsec(const tBoolean);
extern void section_forget_object(tObject*);
extern /*@null@*/ tObject* section_get_objects(void);

extern tCreatureCount section_countcreatures(void);
extern /*@null@*/ tCreature* section_get_creatures(void);
extern void mrm_fov_giant(const tCreature*);
extern void section_register_creature(tCreature*);
extern void section_unregister_creature(tCreature*);
extern void section_put_object(tObject*, const tCoordinate, const tCoordinate,
  const unsigned char);
extern void section_put_object_carefully(tObject*, const tCoordinate,
  const tCoordinate, const unsigned char);
extern tBoolean section_randput_object(tObject*);
extern tBoolean section_randlocatearound(const tCoordinate, const tCoordinate,
  const tSquareFlags, /*@out@*/ tCoordinate*, /*@out@*/ tCoordinate*);
extern tBoolean section_reveal_objects(void);
extern void section_unhide_square(const tCoordinate, const tCoordinate);
extern void section_light_room_up(const tRoomIndex);

extern void section_bring_out_creatures(const tCoordinate, const tCoordinate,
  const tSectionEventCount, const tCreatureClass);
extern void section_recreate_creature(const tCoordinate, const tCoordinate,
  const tCreatureClass, const unsigned char);
extern void section_hoe_wall(tCoordinate, tCoordinate, const tDirection,
  const tHoeValue);

extern /* __sallocator */ /*@null@*/ tObstacle* __callocator
  section_obstacle_create(const tObstacleKind, const tObstacleDetail,
  const tCoordinate, const tCoordinate);
extern tBasicObjectMaterials obstacle_materials(const tObstacle*);
#if 0
extern tObstacleEffects section_obstacle_effect(tObstacle*, const tObject*,
  tObstacleContactKind, tCoordinate);
#endif
extern /*@null@*/ tObstacle* location2obstacle(const tCoordinate,
  const tCoordinate);
extern void section_obstaclekindname(const tObstacleKind, /*@out@*/ char*,
  const unsigned char);
extern tBoolean section_obstacle_get_name(const tObstacle*, /*@out@*/ char*,
  const unsigned char);
extern tObstacleSize obstacle_effsize(const tObstacle*);
extern tPercentage obstacle_transparency(const tObstacle*);
extern tPercentage obstacle_transzapency(const tObstacle*);
extern tBoolean obstacle_showable(const tObstacle*);
extern void section_crash_obstacle(tObstacle*, const unsigned char);
extern tBoolean obstacle_resistance_decrease(tObstacle*,
  const tObstacleResistance, const unsigned char);
extern void say_obstacle_resists(const tObstacle*);
extern tBoolean obstacle_zap(tObstacle*, const tObject*,
  /*@null@*/ const tCreature*);
extern void noise_caused_at(const tCoordinate, const tCoordinate,
  const tNoiseFlags, const tNoiseVolume, /*@null@*/ const tCreature*);
#define noise_caused_by(c, nf, nv) noise_caused_at((c)->x, (c)->y, nf, nv, c)
extern void gush_of_water(void);
extern tBoolean section_might_trap(void);
extern void section_handle_traps(void);
extern void section_perform_tick(void);
extern void dungeon_initialize(const unsigned char);

/* Map[-update] flags; when used with map_update(), they mean: "an update might
   be necessary due to change of object/creature/... on the given square";
   when used with map_can_player_see(), they mean: "Can the player see what's
   on the given square WRT the flags?" */

my_enum1 enum
{ mfNone = 0, mfObject = 0x01, mfObjectSpecial = 0x02, mfRogue = 0x04,
  mfCreature = 0x08, mfGround = 0x10, /* (related to water/swamp/platform) */
  mfStairsUp = 0x20, /* (that's not ground-related...) */
  mfStairsDown = 0x40, /* CHECKME: integrate with mfGround? */
  mfBarrier = 0x80, mfOther = 0x100
} my_enum2(unsigned short) tMapFlags;
#define mfObjectAppeared      (mfObject)
#define mfObjectDisappeared   (mfObject)
#define mfObjectChanged       (mfObject)
#define mfObjectRelated       (mfObject)
/* (mfObject | mfObjectAppeared | mfObjectDisappeared | mfObjectChanged), but
   _not_ mfObjectSpecial! */
#define mfRogueAppeared       (mfRogue)
#define mfRogueDisappeared    (mfRogue)
#define mfRogueChanged        (mfRogue)
#define mfRogueRelated        (mfRogue)
/* (mfRogue | mfRogueAppeared | mfRogueDisappeared | mfRogueChanged) */
#define mfCreatureAppeared    (mfCreature)
#define mfCreatureDisappeared (mfCreature)
#define mfCreatureChanged     (mfCreature)
#define mfCreatureRelated     (mfCreature)
/* (mfCreature | mfCreatureAppeared | mfCreatureDisappeared |
    mfCreatureChanged) */ /* creature except player character */
#define mfStairs              (mfStairsUp | mfStairsDown)
#define mfWall                (mfOther)
#define mfObstacle            (mfOther)
#define mfAnything \
  (mfObjectRelated | mfRogueRelated | mfCreatureRelated | mfGround | \
   mfStairs | mfBarrier | mfOther)

my_enum1 enum
{ mfdkEmpty = 0, mfdkWall = 1, mfdkCreature = 2, mfdkBarrier = 3,
  mfdkObstacle = 4, mfdkObject = 5, mfdkSeconn = 6, mfdkPlatform = 7,
  mfdkGroundFeature = 8, mfdkGround = 9, mfdkMisty = 10, mfdkChain = 11,
  mfdkIronBall = 12, mfdkMax = mfdkIronBall
} my_enum2(unsigned char) tMfdk;
  /* "map field display kind"; what to show on screen */

extern const char* const strMfdk[mfdkMax + 1];

my_enum1 enum
{ mffNone = 0, mffIndistinct = 0x01
} my_enum2(unsigned char) tMapFieldFlags;

typedef struct
{ tMapFlags mf; /* what to update */
  tMfdk dispkind;
  unsigned char losq; /* line of sight quality */
  tMapFieldFlags mff; /* special stuff */
} tMapField;

extern tCoordinate map_width, map_height; /* number of squares */
extern tMapField* currmap;
extern tLightIntensity* map_brightness;
#define mapfieldptr(x, y) (&(currmap[(y) * map_width + (x)]))
#define map_br(x, y) (map_brightness[(y) * map_width + (x)])
#define map_brptr(x, y) (&(map_br(x, y)))
#define BRIGHTNESS_VAGUE (10)
#define BRIGHTNESS_CLEAR (20)
#define brightness_vague(x, y) (map_br(x, y) >= BRIGHTNESS_VAGUE)
#define brightness_clear(x, y) (map_br(x, y) >= BRIGHTNESS_CLEAR)

extern void cursor_reset_position(void);
extern void map_update(const tCoordinate, const tCoordinate, const tMapFlags);
extern tBoolean map_can_player_see(const tCoordinate, const tCoordinate,
  const tMapFlags);
extern void map_magic(const tBoolean);
extern void map_do_update_all(void);

#if 0 /* OPTION_ANIMATION */
extern void map_undo_animation(const tCoordinate, const tCoordinate);
#endif

#endif /* #ifndef __atrogue_dungeon_h__ */
