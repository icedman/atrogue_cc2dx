/* atrogue/creature.h - creature handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_creature_h__
#define __atrogue_creature_h__

#include "stuff.h"
#include "object.h"

my_enum1 enum
{ cfNone = 0, cfPc = 0x01, cfPro = 0x02, cfFlitsMov = 0x04, cfFlitsAtt = 0x08,
  cfFlies = 0x10, cfSupple = 0x20, cfPlantlike = 0x40, cfStationary = 0x80,
  cfSeeksGold = 0x100, cfImitates = 0x200, cfInvisible = 0x400,
  cfImmaterial = 0x800, cfSensCreat = 0x1000, cfFollows = 0x2000,
  cfRegenerates = 0x4000, cfDivides = 0x8000, cfDead = 0x10000,
  cfMetallivore = 0x20000, cfTamable = 0x40000
} my_enum2(unsigned int) tCreatureFlags;
#define cfFlitsAny (cfFlitsMov | cfFlitsAtt)
/* cfFlitsMov: might flit when moving; cfFlitsAtt: might flit when it is
   attacked; IMPLEMENTME: if cfFollows, follow the player character when he
   enters another section/storey */

my_enum1 enum
{ ccfNone = 0, ccfSleepsDeeply = 0x01, ccfHumanoid = 0x02, ccfNonAuto = 0x04,
  ccfNoIndefArt = 0x08
} my_enum2(unsigned char) tCreatureClassFlags;
  /* special flags, for class data only (cf. creature_create_at()) */
  /* ccfHumanoid: in a very general sense! -- use creature_is_humanoid() for
     this; IMPLEMENTME: ccfHatesLight for ccBat, ccVampire, ccZombie and
     ccShrieker */

my_enum1 enum
{ shfNone = 0, shfSplashes = 0x01, shfConfuses = 0x02, shfStealsGold = 0x04,
  shfStealsObject = 0x08, shfStings = 0x10, shfSpins = 0x20, shfHolds = 0x40,
  shfFreezes = 0x80, shfDrainsLife = 0x100, shfFlames = 0x200, shfZaps = 0x400,
  shfDecExp = 0x800, shfEatsMetal = 0x1000, shfSummons = 0x2000
} my_enum2(tUint16) tSpecialHitFlags;
#define shfActive  (shfStealsGold | shfStealsObject | shfStings | shfSpins | \
  shfHolds | shfDrainsLife | shfFlames | shfZaps | shfEatsMetal | shfSummons)
#define shfPassive (shfSplashes | shfConfuses | shfFreezes | shfDecExp)
/* #define shfAny (shfActive | shfPassive) -- not needed */

my_enum1 enum
{ /* the player character */
  ccRogue = 0,
  /* "usual" creatures (standard series of map characters A-Z) */
  ccAquatar = 1, ccBat = 2, ccCentaur = 3, ccDragon = 4, ccEmu = 5,
  ccFlytrap = 6, ccGriffin = 7, ccHobgoblin = 8, ccIcemonster = 9,
  ccJabberwock = 10, ccKestrel = 11, ccLeprechaun = 12, ccMedusa = 13,
  ccNymph = 14, ccOrc = 15, ccPhantom = 16, ccQuagga = 17, ccRattlesnake = 18,
  ccSnake = 19, ccTroll = 20, ccUnicorn = 21, ccVampire = 22, ccWraith = 23,
  ccXeroc = 24, ccYeti = 25, ccZombie = 26,
  /* "less usual" creatures */
  ccArcher = 27, ccBee = 28, ccIronic = 29, ccSlime = 30, ccSpider = 31,
  ccWorm = 32,
  /* "special" creatures */
  ccBouncer = 33, ccCoxswain = 34, ccGuard = 35, ccHeisenbug = 36, ccHydra =37,
  ccKidnapper = 38, ccSchroedincat = 39, ccShopkeeper = 40, ccShrieker = 41,
  ccSiren = 42, ccTrogue = 43, ccVarydamage = 44, ccWizard = 45
} my_enum2(unsigned char) tCreatureClass;
#define maxRandCreatureClass (ccWorm)
#define maxCreatureClass (ccWizard)
#define ccNone (maxCreatureClass + 1)
#define ccShift (6)
#define ccMask ((1 << ccShift) - 1)

typedef tSint16 tHealth; /* signed only to simplify calculations */
typedef tSint16 tStrength; /* ditto */
typedef tSint16 tExperience; /* ditto */
typedef tSint8 tAggressivity;
typedef tSint8 tCreatureActionSpeed; /* -7..+7 */

my_enum1 enum
{ csSmall = 0, csMedium = 1, csNormal = 2, csGiant = 3
} my_enum2(unsigned char) tCreatureSize;
/* IMPORTANT: never assign to the player character another size than csNormal -
   or go through the whole source code and make sure that everything still
   works... */

my_enum1 enum
{ cakUnknown = 0, cakOther = 1, cakRest = 2, cakMove = 3, cakHit = 4,
  cakEat = 5, cakQuaff = 6, cakThrow = 7, cakDrop = 8, cakKick = 9,
  cakSearch = 10, cakPick = 11, cakZap = 12, cakCast = 13, cakScroll = 14
} my_enum2(unsigned char) tCreatureActionKind;
#define cak_magic(cak) ((cak) >= cakZap)

/* Special napping values for cases where the exact nap _duration_ shall not be
   set in advance: "SLEEP" means "might wake up when an enemy is nearby",
   "DEEPSLEEP" means "only wakes up when hit". */
#define NAPKIND_SLEEP     (MAX_FEWTICKS - 1)
#define NAPKIND_DEEPSLEEP (MAX_FEWTICKS)

#define IMI_BUILD(c, kind) ((c) | ((kind) << ocBits))
#define IMI2CLASS(cd) ((cd) & ocMask)
#define IMI2CHAR(cd) (strObjectClassChars[IMI2CLASS(cd)])
#define IMI2KIND(cd) ((cd) >> ocBits)

typedef struct tCreature
{ tTickout tickout; /* when the creature shall act again */
  struct tCreature* next; /* pointer to next creature (if in list context) */
  /*@null@*/ tObject* objects; /* objects carried by this creature ("pack") */
  /*@null@*/ tObject *armor, *weapon; /* for humanoid creatures only */
  tCreatureFlags flags;
  tMilliticks last_action;
  tCoordinate x, y; /* current location of the creature */
  tCoordinate tx, ty; /* target location the creature should go to */
  tCreatureClass cc;
  tHealth health;
  tStrength strength;
  tExperience experience; /* REMOVEME? */
  tSpecialHitFlags shf;
  tFewTicks _blind, confused, hallucinating, slowedhasted, napping, levitated,
    held, frozen;
  tCreatureData cd;
  tAggressivity aggr;
  tDirection lastdir; /* used for gliding over sfGlazedFrost squares etc. */
  tCreatureActionKind cak;
  tCreatureActionSpeed cas;
  char ch;
} tCreature;


/* Special data for the player character */

my_enum1 enum
{ /* "permanent" flags */
  rfNone = 0, rfSenseObjects = 0x01, rfDied = 0x02,
  /* tick-specific flags */
  rfTickHits = 0x04, rfTickIsHit = 0x08, rfTickIsZapped = 0x10,
  rfTickIsDazzled = 0x20,
  /* action-specific flags */
  rfHits = 0x40, rfIsHit = 0x80, rfIsZapped = 0x100, rfStateChanged = 0x200,
  rfInventoryChanged = 0x400, rfInvPropertyChanged = 0x800, rfTrapmsg = 0x1000,
  rfIsSearching = 0x2000, rfIsDazzled = 0x4000, rfAttemptedMove = 0x8000,
  rfRestless = 0x10000
#if CONFIG_DEBUG
  , rfDebugmsg = 0x20000
#else
  , rfDebugmsg = 0
#endif
} my_enum2(unsigned int) tRogueFlags;
#define rfStoppers (rfHits | rfIsHit | rfIsZapped | rfDied | rfStateChanged | rfInventoryChanged | /* rfInvPropertyChanged | */ rfDebugmsg | rfIsDazzled | rfTrapmsg)
#define rfTickNoheal (rfTickHits | rfTickIsHit | rfTickIsZapped | rfTickIsDazzled)

/* Rogue state values; related to actions_left; if you change these, adapt
   rogue_checkactionsleft() and statistics_update()! */
my_enum1 enum
{ rsNormal = 0, rsHungry = 1, rsWeak = 2, rsFaint = 3, rsStarve = 4
} my_enum2(unsigned char) tRogueState;
#define maxRogueState (4)

my_enum1 enum
{ hcUnknown = 0, hcBearTrap = 1, hcPit = 2, hcCobweb = 3, hcCreatureClasp = 4,
  hcOtherClasp = 5
} my_enum2(unsigned char) tHoldingCause;

typedef tSint8 tPackCount; /* 0..MAXNUM_INV; signed for simplicity */
typedef tUint16 tCreatureCount;

my_enum1 enum
{ cbfNone = 0,
  /* food-related conduct: */
  cbfFoodless = 0, cbfFoodVegan = 1, cbfFoodVegetarian = 2, cbfFoodAny = 3,
  /* flags: */
  cbfDidRead = 0x04, cbfDidWrite = 0x08, cbfDidWish = 0x10, cbfDidPolyPc =0x20,
  cbfDidPolyObj = 0x40, cbfDidTryIdentify = 0x80, cbfDidTrade = 0x100,
  cbfHadPet = 0x200
} my_enum2(tUint16) tCbf; /* conduct/behavior flags */
#define cbfFoodmask (0x03)

my_enum1 enum
{ /* miscellaneous objects: */
  rdosiBoots = 0, rdosiGloves = 1, rdosiCape = 2, rdosiGlasses = 3,
  rdosiShawl = 4, rdosiHelmet = 5, rdosiNecklace = 6, rdosiBlindfold = 7,
  /* rings _must_ be last: */
  rdosiRing1 = 8, rdosiRing2 = 9, rdosiRingNull = 10
} my_enum2(tUint8) tRogueDataOsi;
  /* "object shortcut index" for special tools and such */
#define maxRdosi (10)

typedef struct
{ tUint32 num_actions, num_weaponhits, num_seeing;
  tCreatureCount defeatcounter[maxCreatureClass + 1], alldefeats;
  tUint16 deathcount, inventorized;
  tInventoryIndex inventoresque;
  tStoreyNumber sn_deepest; /* how far the player character got */
} tRogueDataStats; /* statistics */

typedef struct
{ tRogueDataStats rds;
  tObject* inventory[MAXNUM_INV]; /* entries for inventory handling */
  tObject* os[maxRdosi + 1]; /* for performance and simplicity reasons */
  tObjectWeight packweight, maxpackweight;
  tHealth maxhealth;
  tStrength maxstrength;
  tRogueFlags flags;
  tFewTicks actions_left; /*number of actions left before dying of starvation*/
  tMagicEnergy magic_energy;
  tMagicExp magic_exp;
  tTicks napping_since, frozen_since, iron_ball_off;
  tFewTicks endure_freeze; /*how many ticks the Rogue can endure being frozen*/
  tFewTicks traptick; /* number of moves the Rogue wasn't affected by a trap */
  tCoordinate iron_ball_x, iron_ball_y;
  tRogueState state;
  tHoldingCause holding_cause; /* only valid if rogue->held > 0 */
  tPackCount currpackcount, maxpackcount; /* for the inventory */
  tCbf cbf;
  tUint8 digestcount, attemptedmovecount, petrifying, cakcount;
} tRogueData; /* information about the player character */
extern tRogueData roguedata;
#define roguedata_os(rdosi) (roguedata.os[rdosi])

#define rogue_can_hear (truE) /* IMPLEMENTME? (e.g. tokEarmuff) */
#define rogue_chained (roguedata.iron_ball_x != coordNone)


/* Other stuff */

my_enum1 enum
{ drUnknown = 0, drInternal = 1, drStarvation = 2, drPoison = 3,
  drHypothermia = 4, drFight = 5, drDrown = 6, drSink = 7, drInjury = 8,
  drFire = 9, drRemoval = 10, drPolymorph = 11, drLight = 12, drSilver = 13,
  drPetrifying = 14
} my_enum2(unsigned char) tDeathReason;
#define maxDeathReason (14)

#define for_each_currentsectioncreature(c) \
  for (c = section_get_creatures(); c != NULL; c = c->next)
/* IMPORTANT: if you use this, you must check for cfDead! */

my_enum1 enum
{ afNone = 0, afActuallyHit = 0x01, afAttemptedZap = 0x02, afSurvived = 0x04
} my_enum2(unsigned char) tActionFlags; /* action flags */

extern /*@relnull@*/ tCreature* rogue;


/* Functions */

extern tCreature* creature_create_at(tCreatureClass, tCoordinate, tCoordinate,
  const unsigned char);
extern /*@null@*/ tCreature* creature_create(const tCreatureClass,
  const unsigned char);
extern void creature_autocreate(void);
#if CONFIG_DEBUG
extern void creature_dump_sc(void);
#endif

extern void creature_moveabs(tCreature*, const tCoordinate, const tCoordinate,
  const tDirection);
extern tBoolean creature_is_flitting(const tCreature*, const tBoolean);
extern tBoolean creature_randflit(const tCreature*, const tCreature*);
extern tBoolean creature_try_to_flit(tCreature*);
extern tBoolean creature_get_sustain_strength(const tCreature*);
extern tCreatureSize _creature_size(const tCreatureClass);
#define creature_size(c) _creature_size((c)->cc)
extern tHealth creature_defaulthealth(const tCreature*);
extern tBoolean creature_is_humanoid(const tCreature*);
extern void creature_set_aggressivity(tCreature*, const tAggressivity);
extern tBoolean creature_drifts(const tCreature*);
extern tBoolean creature_is_ill(const tCreature*);
extern tBoolean creature_is_in_the_air(const tCreature*);
extern tBoolean creature_land(tCreature*);

extern void creature_age_inventories(tTicks);
extern void creature_fill_section(void);
extern const char* _creatureclassname(const tCreatureClass);
extern void creatureclassname(const tCreatureClass, /*@out@*/ char*,
  const unsigned char);
extern void creature_get_name(const tCreature*, /*@out@*/ char*,
  const unsigned char);

extern void creature_wear(tCreature*, tObject*);
extern void creature_unwear(tCreature*);
extern void creature_wield(tCreature*, tObject*);
extern void creature_unwield(tCreature*);

extern tInventoryIndex creature_add_object(tCreature*, tObject*);
extern void creature_lose_object(tCreature*, tObject*);
extern void creature_check_object(tCreature*, tObject*);

extern void creature_drop_object(tCreature*, tObject*, const tObjectQuantity,
  const tBoolean);
extern tBoolean creature_can_carry_object(tCreature*, const tObject*,
  const unsigned char);
extern void creature_throw(tCreature*, tObject*, tObjectQuantity, tDirection);

extern void creature_make_invisible(tCreature*);
extern void creature_make_visible(tCreature*);
extern tBoolean creature_can_see(const tCreature*);
extern void creature_blind(tCreature*, tFewTicks);
extern void creature_unblind(tCreature*);
extern void creature_confuse(tCreature*, tFewTicks);
extern void creature_unconfuse(tCreature*);
extern void creature_hallucinate(tCreature*, tFewTicks);
extern void creature_unhallucinate(tCreature*);
extern void creature_nap(tCreature*, const tFewTicks, const unsigned char);
extern void creature_unimitate(tCreature*, const tBoolean);
extern void creature_wake_up(tCreature*, const tBoolean);
extern void creature_freeze(tCreature*, tFewTicks);
extern void creature_unfreeze(tCreature*);
extern void creature_levitate(tCreature*, const tFewTicks);
extern tBoolean creature_unlevitate(tCreature*);
extern void creature_slow(tCreature*, const tFewTicks);
extern void creature_haste(tCreature*, const tFewTicks);
extern void creature_decslowedhasted(tCreature*);
extern tBoolean toc2rdosi(const tToolClass, /*@out@*/ tRogueDataOsi*);
extern tBoolean creature_is_shopkeeper(const tCreature*);
extern void infuriate_shopkeeper(/*@null@*/ tCreature*, tBoolean);
extern void creature_cancel_being_held(tCreature*);
extern void cancel_petrifying(void);
extern void break_iron_ball(void);
extern void creature_teleport(tCreature*);
extern void creature_knightmove(tCreature*, tDirection);
extern tBoolean creature_polymorph(tCreature*);

extern tBoolean creature_aggravate(void);
extern tBoolean creature_allure_bees(void);

extern tBoolean creature_died(tCreature*, tDeathReason, /*@null@*/ tCreature*);
extern tBoolean creature_dec_health(tCreature*, tHealth);

extern tBoolean __creature_can_act_diagonally(const tCreature*, tCoordinate,
  tCoordinate, tCoordinate, tCoordinate);
#define creature_can_act_diagonally(c, x2, y2) \
  __creature_can_act_diagonally(c, (c)->x, (c)->y, x2, y2)
extern tBoolean creature_can_see_creature(const tCreature*, const tCreature*,
  const tBoolean);
extern tBoolean creature_enemies(const tCreature*, const tCreature*);
extern void creature_make_enemies(const tCreature*, tCreature*);

extern tBoolean creature_can_affect_location(const tCreature*,
  const tCoordinate, const tCoordinate);
extern void creature_last_action_set(tCreature*, const tCreatureActionKind,
  tCreatureActionSpeed);
extern tActionFlags creature_zap(tCreature*, tObject*, tDirection);
extern tActionFlags creature_specialhit(tCreature*, tCreature*, tBoolean);
extern void creature_hit(tCreature*, tCreature*);

extern /*@null@*/ tCreature* location2creature(tCoordinate, tCoordinate);
#define creature_room(creature) (location2room((creature)->x, (creature)->y))
extern tBoolean creature_same_room(const tCreature*, const tCreature*);
extern tBoolean creature_same_room_xy(const tCreature*, const tCoordinate,
  const tCoordinate);

extern void player_tickout_handler(tTickout*, const tTickoutAction); /*main.c*/
extern void creature_tickout_handler(tTickout*, const tTickoutAction);
extern void creature_tickout_recalc(tCreature*, const unsigned char);
extern void creatures_perform_tick(void);

extern void rogue_create(void);
extern void rogue_died(const tDeathReason, /*@null@*/ const tCreature*);
extern void rogue_checkactionsleft(void);
extern void rogue_decactionsleft(const tFewTicks);
extern void rogue_incmagicexp(tMagicExp);
extern void rogue_decmagicexp(tMagicExp);
extern void rogue_incmagicen(const tMagicEnergy);
extern void rogue_decmagicen(const tMagicEnergy);
extern tBoolean rogue_dazzle(const tBoolean);
extern tPercentage rogue_rings2fortune(const tPercentage);
extern void cleanse_price_tags(void);
extern tBoolean _rogue_in_shop(void);
#define rogue_in_shop (_rogue_in_shop())
extern tBoolean _rogue_has_unpaid_items(void);
#define rogue_has_unpaid_items (_rogue_has_unpaid_items())
extern const char* _role_name(const tRole, const tCreatureGenderKind,
  const tBoolean);
#define role_name(plural) _role_name(pref_role, pref_gender, plural)
extern tBoolean rogue_can_read(void);
extern tBoolean rogue_can_write(void);
#define _rogue_can_see (roguedata_os(rdosiBlindfold) == NULL)
#define rogue_can_see ( (!rogue->_blind) && (_rogue_can_see) )
extern void rogue_las(const tCreatureActionKind, tCreatureActionSpeed);
extern tBoolean rogue_has_objectclasses(const tObjectClassesBitmask);
extern /*@null@*/ tObject* _rogue_aoy_find(void);

extern void creature_initialize(void);

/* Stuff for handling the player character's rings on hands */

extern tRogueDataOsi __foreachwornring; /* global temporary loop variable */
#define for_each_worn_ring(ring) for (__foreachwornring = rdosiRing1; (ring = roguedata_os(__foreachwornring)) != NULL; __foreachwornring++)

/* The following macro sums up the enchantments of all currently worn rings of
   a certain kind. */
#define worn_rings_enchsum(enchvar, ringvar, ringkind) \
do \
{ enchvar = 0; \
  for_each_worn_ring(ringvar) \
  { if (ringvar->kind == (ringkind)) enchvar += ringvar->enchantment; } \
} while (0)

/* The following macro is like the previous, but also counts the number of
   rings of the given kind. */
#define worn_rings_enchsumcount(enchvar, ringvar, ringkind, ringcountvar) \
do \
{ enchvar = 0; ringcountvar = 0; \
  for_each_worn_ring(ringvar) \
  { if (ringvar->kind == (ringkind)) \
    { ringcountvar++; enchvar += ringvar->enchantment; } \
  } \
} while (0)

#endif /* #ifndef __atrogue_creature_h__ */
