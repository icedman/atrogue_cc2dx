/* atrogue/object.h - object handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_object_h__
#define __atrogue_object_h__

#include "stuff.h"

/* Basic differences between the classes of magic objects:
   - potions do much when drunk, less when thrown
   - wands operate remotely and in certain directions
   - scrolls operate non-direction-oriented
   - rings operate permanently (when put on a hand)
   - tools (if magic at all): "permanent" tools operate permanently (what
     else:-) while being applied, the others work non-direction-oriented; so
     tools are an interesting combination of rings and scrolls */

extern const char strObjectClassChars[], strMetal[], strWooden[], strWooden[],
  strArmor[], strHelmet[], strDown[], strUp[];
extern const char* const strGwsm[4];

my_enum1 enum
{ ocAmulet = 0, ocGold = 1, ocTool = 2, ocStone = 3, ocFood = 4,
  ocArmor = 5, ocWeapon = 6, ocPotion = 7, ocWandStaff = 8,
  ocScroll = 9, ocRing = 10
} my_enum2(tUint8) tObjectClass; /* inventory item object classes */
#define maxObjectClass (10)
#define ocInvalid (maxObjectClass + 42)
#define ocBits (4)
#define ocMask ((1 << ocBits) - 1)
/* NOTE 1: if you change these, don't forget to change object class data,
   strObjectClassChars, ocbAll, object_weight() and object_materials() in
   object.c, key classes in message.[ch], tObjectClassesBitmask in stuff.h and
   the inventory_sorter() array as appropriate!
   NOTE 2: ocAmulet _must_ be 0, or we get lots of trouble with
   object_randcreate() etc. */
/* IMPLEMENTME: generalize ocGold to ocMetalPiece and use most/all
   strWandMaterial[] kinds! The <objdetail> field should have a "fake" bit. */

enum
{ ocbAll = ((1 << (maxObjectClass + 1)) - 1), /* all object classes */
  ocbCombat = (1 << ocArmor) | (1 << ocWeapon), /* combat-related */
  ocbMagic = (1 << ocPotion) | (1 << ocWandStaff) | (1 << ocScroll) |
    (1 << ocRing) /* classes of magic objects */
}; /* some object classes bitmasks (tObjectClassesBitmask) */

typedef tUint8 tObjectKind;
#define okNone (255)
  /* (some large number - higher than the highest of maxArmorKind,
      maxWeaponKind, maxRingKind, ...) */

my_enum1 enum
{ amkYendor = 0, amkReflection = 1, amkMagicEnergy = 2, amkShockResistance = 3,
  amkHealing = 4, amkLifeSaving = 5, amkStrang = 6, amkShield = 7, amkLuck = 8,
  amkStayActive = 9, amkIncAccu = 10, amkBranchport = 11, amkRegeneration = 12,
  amkBreath = 13, amkUnchanging = 14, amkCastcost = 15, amkTeleport = 16,
  amkConflict = 17, amkConserv = 18, amkDoNothing = 19
  /* IMPORTANT: amkDoNothing must be last (or change skRemoveCurse handling in
     main.c::do_cast_spell())! */
} my_enum2(tObjectKind) tAmuletKind; /* IMPLEMENTME! */
#define maxAmuletKind (0)
#define AMULET_NECKLACE(detail) ((detail) & 3) /* 0: none; 1: fixed; 2: loose*/
#define _AMULET_FAKE (4)
#define AMULET_FAKE(detail) ((detail) & _AMULET_FAKE)
#define _AMULET_FAKEKNOWN (8)
#define AMULET_FAKEKNOWN(detail) ((detail) & _AMULET_FAKEKNOWN)
#define _AMULET_NSHIFT (4)
#define _AMULET_DSHIFT (7)
#define _AMULET_NMASK (7 << _AMULET_NSHIFT)
#define AMULET_FRACTION(numerator, denominator) \
  (((numerator) << _AMULET_NSHIFT) | ((denominator) << _AMULET_DSHIFT))
#define AMULET_ND(detail, nvar, dvar) do { nvar = ((detail) >> _AMULET_NSHIFT) & 7; dvar = ((detail) >> _AMULET_DSHIFT) & 7; } while (0)

#define maxGoldKind (0)

my_enum1 enum
{ tokBootsQuiet = 0, tokBootsHaste = 1, tokBootsLevit = 2, tokBootsKick = 3,
  tokBootsJump = 4, tokBootsSwim = 5, tokBootsFriction = 6,
  tokGlovesStrength = 7, tokGlovesSkill = 8, tokCapeInvis = 9,
  tokCapeDisplacement = 10, tokCapeShield = 11, tokCapeOilskin = 12,
  tokGlassesSearch = 13, tokGlassesDark = 14, tokQuarkUp = 15,
  tokQuarkDown = 16, tokQuarkCharm = 17, tokQuarkStrange = 18,
  tokQuarkTop = 19, tokQuarkBottom = 20, tokWhistleExpel = 21,
  tokWhistleSignal = 22, tokWhistleNothing = 23, tokShawlGas = 24,
  tokShawlFood = 25, tokShawlBreath = 26, tokPen = 27, tokDie = 28,
  tokCase = 29, tokHeartGold = 30, tokNoTea = 31, tokTowel = 32,
  tokLantern = 33, tokCamera = 34, tokPhotograph = 35, tokKey = 36,
  tokBox = 37, tokDynamite = 38, tokStatue = 39, tokHelmetTinfoil = 40,
  tokHelmetDunceCap = 41, tokHelmetNeckhat = 42, tokNecklace = 43
} my_enum2(tObjectKind) tToolKind;
#define maxToolKind (43)
#define minToolQuark (tokQuarkUp) /* first quark in our enumeration */
#define object_randquarkkind (minToolQuark + randbytemod(6))
#define object_tool_is_quark(kind) \
  ( ((kind) >= minToolQuark) && ((kind) <= (minToolQuark + 5)) )
#define object_tool_quark2num(kind) ((kind) - minToolQuark)

#define key_color(detail) ((detail) & colorcodemask)
#define key_is_metal(detail) ((detail) & (1 << colorcodebits))
#define box_is_large(detail) ((detail) & 1) /* unpickable (too heavy) */

my_enum1 enum
{ smGlass = 0, smWood = 1, smStone = 2, smMetal = 3
} my_enum2(unsigned char) _tStatueMaterial;
  /* tokStatue material; sorted by increasing weight/solidity */

#define _statue_material(detail) ((detail) & 3)
#define statue_cc(detail) ((detail) >> 2) /* creature class */
#define statue_build(sm, cc) ( ((cc) << 2) | (sm) )

/* tokGlassesDark are quite strange: on the one hand, they can emit flashes of
   light themselves; on the other hand, they hold flashes from "external"
   sources off the player character if enchanted.
   Applying a shawl means putting it over mouth and nose - or was that mouse
   and noth'? :-) */

/* classes of tools */
my_enum1 enum
{ tocNone = 0, tocBoots = 1, tocGloves = 2, tocCape = 3, tocGlasses = 4,
  tocQuark = 5, tocWhistle = 6, tocShawl = 7, tocHelmet = 8
} my_enum2(unsigned char) tToolClass;
#define maxToolClass (8)

my_enum1 enum
{ stkFlint = 0, stkGray = 1, stkBlack = 2
  /* sorted from soft/light to hard/heavy */
} my_enum2(tObjectKind) tStoneKind;
#define maxStoneKind (2)

my_enum1 enum
{ fkRation = 0, fkFruit = 1, fkSolidWater = 2, fkVegetables = 3, fkCorpse = 4,
  fkSpinach = 5, fkCarrot = 6
} my_enum2(tObjectKind) tFoodKind;
#define maxFoodKind (6)

my_enum1 enum
{ akLeather = 0, akStuddedLeather = 1, akRing/*mail*/ = 2, akScale = 3,
  akChain = 4, akBanded = 5, akSplint = 6, akPlate = 7
  /* keep the armor kinds sorted by effectiveness, from weakest to best! */
} my_enum2(tObjectKind) tArmorKind;
#define maxArmorKind (7)
#define ARMOR_LAST_LEATHER (1)
#define ARMOR_IS_LEATHER(kind) ( (kind) <= ARMOR_LAST_LEATHER )
#define ARMOR_IS_METAL(kind) ( (kind) > ARMOR_LAST_LEATHER )

my_enum1 enum
{ wkDart = 0, wkPoisonDart = 1, wkDagger = 2, wkShuriken = 3, wkArrow = 4,
  wkBow = 5, wkCrossbow = 6, wkCrossbowBolt = 7, wkFireBolt = 8, wkSpear = 9,
  wkSling = 10, wkBlowgun = 11, wkHoe = 12, wkClub = 13, wkMace = 14,
  wkSword = 15, wkLongSword = 16, wkTwoHandedSword = 17, wkHarpoon = 18,
  wkBoomerang = 19, wkPolearm = 20
} my_enum2(tObjectKind) tWeaponKind;
#define maxWeaponKind (20)

#define polearm_tipobj(polearm) ((polearm)->contents) /* usually NULL */
  /* the object at the tip of the polearm, if any */

my_enum1 enum
{ pkIncreaseStrength = 0, pkRestoreStrength = 1, pkIncreaseExp = 2,
  pkHeal = 3, pkGreatlyHeal = 4, pkMagicEnergy = 5, pkPoison = 6,
  pkLiquidFire = 7, pkConfuse = 8, pkHallucinate = 9, pkBlindness = 10,
  pkSenseObjects = 11, pkSenseCreatures = 12, pkLevitate = 13, pkSlow = 14,
  pkHaste = 15, pkMakeSupple = 16
} my_enum2(tObjectKind) tPotionKind;
#define maxPotionKind (16)

my_enum1 enum
{ wskCreateObject = 0, wskCreateCreature = 1, wskRemove = 2, wskTeleport = 3,
  wskPolymorph = 4, wskSlow = 5, wskConfuse = 6, wskMakeInvisible = 7,
  wskMakeImmaterial = 8, wskSleep = 9, wskFreeze = 10, wskHoe = 11,
  wskFlame = 12, wskRemoteHit = 13, wskBuildBarrier = 14, wskDrainLife = 15,
  wskCancel = 16, wskDoSubtle = 17, wskDoNothing = 18
  /* IMPORTANT: wskDoNothing must be last (or change skRemoveCurse handling in
     main.c::do_cast_spell())! */
  /* IMPLEMENTME: wskDuplicate? */
} my_enum2(tObjectKind) tWandStaffKind;
#define maxWandStaffKind (18)
/* (We don't use the shorter type identifier "tWandKind" because the associated
    constant prefix would be "wk", the same as the tWeaponKind constants
    prefix - and "tWandStaffKind" makes it more clear that any of these objects
    may be wand (metal) _or_staff_ (wood), respectively... On the other hand,
    the identifier isn't "tWandStaffRodKind" because this would be "too long"
    and simply because rods were implemented later.) */

my_enum1 enum
{ skIdentify = 0, skProtectArmor = 1, skEnchantArmor = 2, skEnchantWeapon = 3,
  skTeleport = 4, skRemoveCurse = 5, skScare = 6, skSleep = 7, skFreeze = 8,
  skMap = 9, skSenseHidden = 10, skLight = 11, skRecharge = 12, skHold = 13,
  skAggravate = 14, skHeat = 15, skWisdom = 16, skPetrify = 17,
  skBlankPaper = 18
  /* IMPORTANT:
     - skBlankPaper must be last (or change skRemoveCurse handling in
       main.c::do_cast_spell(), loop condition in object_dump_associations()
       and tokPen handling in main.c::apply_tool())!
     - If we ever have more than 26 scroll kinds, spell-casting handling must
       be changed (main.c::cast_spell(), object.c::object_dump_associations())!
  */
} my_enum2(tObjectKind) tScrollKind;
#define maxScrollKind (18)

my_enum1 enum
{ rkHeal = 0, rkSustainStrength = 1, rkFortune = 2, rkFightFortune = 3,
  rkSenseHidden = 4, rkQuiet = 5, rkSlowDigestion = 6, rkFear = 7,
  rkResistSleep = 8, rkResistFreeze = 9, rkResistConfusion = 10,
  rkRebirth = 11, rkHoney = 12, rkTeleport = 13, rkKnightmove = 14,
  rkPolymorph = 15, rkSearch = 16, rkDoNothing = 17
  /* IMPORTANT: rkDoNothing must be last (or change skRemoveCurse handling in
     main.c::do_cast_spell())! */
  /* IMPLEMENTME: rkIncreaseDamage? */
} my_enum2(tObjectKind) tRingKind;
#define maxRingKind (17)

extern tBoolean
  potionkind_id[maxPotionKind + 1], wandstaffkind_id[maxWandStaffKind + 1],
  scrollkind_id[maxScrollKind + 1], ringkind_id[maxRingKind + 1],
  toolkind_id[maxToolKind + 1];

my_enum1 enum
{ ofNone = 0, ofKindIdentified = 0x01, ofEnchIdentified = 0x02,
  ofEnchExtIdentified = 0x04, ofDamageIdentified=0x08, ofChargeIdentified=0x10,
  ofBeingWorn = 0x20, ofBeingWielded = 0x40, ofBeingApplied = 0x80,
  ofOnLeftHand = 0x100, ofOnRightHand = 0x200, ofBeingCarried = 0x400,
  ofBeingLit = 0x800, ofProtected = 0x1000, ofRusts = 0x2000, ofWormy = 0x4000,
  ofKnownMaterial = 0x8000, ofKnownPrice = 0x10000
} my_enum2(unsigned int) tObjectFlags;
#define ofFullyIdentified (ofKindIdentified | ofEnchIdentified | \
  ofEnchExtIdentified | ofDamageIdentified | ofChargeIdentified)
#define ofOnEitherHand (ofOnLeftHand | ofOnRightHand)
#define ofInUse (ofBeingWorn | ofBeingWielded | ofBeingApplied |ofOnEitherHand)

typedef tSint16 tObjectQuantity; /* "signed" to simplify calculations */
typedef tSint16 tObjectWeight; /* ditto */
typedef tSint8 tPropertyExtent; /* _not_ ditto! */
typedef tUint16 tObjectDetail;
typedef tUint16 tHoeValue;

typedef tSint16 tPriceTag; /* 0 means "paid", 1 "ask shopkeeper", >1 a
  specific price, <0 "the shopkeeper offered to buy at that price" */
#define price_is_unpaid(price) ((price) > 0)
#define object_is_unpaid(object) (price_is_unpaid((object)->price_tag))

#define MAX_OBJECT_QUANTITY (HIGHEST_ALLOWED_NUMBER)

#define OBJECT_WEIGHT_ROGUEINITMAX (1000 - 30 * difficulty)
#define OBJECT_WEIGHT_ROGUEDIGEST  (25)
  /* (the value is chosen so that e.g. any sword causes extra digestion) */
#define OBJECT_WEIGHT2DISTANCESUB(weight) ((weight) / 25)
#define OBJECT_WEIGHT2DAMAGEFACTOR(weight) (1) /* IMPLEMENTME! */

typedef struct tObject
{ /*@null@*/ struct tObject *next, /* ptr to next object (in list context) */
    *contents; /* e.g. for tokBox and tokStatue and wkPolearm */
  tObjectFlags flags;
  tObjectClass oc;
  tObjectKind kind; /* meaning depends on object class */
  tPropertyExtent enchantment; /* == 0 means neutral, < 0 means cursed */
  tObjectQuantity quantity; /* number of "individual" objects ("things") */
  tDamage damage; /* how much damage each of its "things" might do when thrown
    "normally" (for armor: how much damage it holds away from its user) */
  tFewTicks lifetime; /* how long the object can exist yet */
  tCoordinate x, y; /* only valid if the object is lying around in a section */
  tObjectDetail objdetail;
  tPriceTag price_tag; /* for the shop system */
  tInventoryIndex invidx; /* only valid if object in player character's pack */
  /* IMPORTANT: if you add other stuff here, make sure that object_ttpt() still
     works correctly! */
} tObject;

/* The following may only be used while the case is in the player character's
   inventory! Otherwise the count/value stuff e.g. means coordinates. */
#define quark_count x
#define quark_value y
typedef tCoordinate tQuarkValue; /* == typeof(y) */

my_enum1 enum
{ /* kinds */
  phkDarkness = 0, phkRock = 1, phkCreature = 2, phkObject = 3, phkObstacle=4,
  /* meta-values */
  phkBits = 3, phkMask = (1 << phkBits) - 1, phdOcShift = phkBits,
  phdOkShift = phdOcShift + ocBits
} my_enum2(tObjectDetail) tPhotoKind;

#define photo_kind(detail) ((detail) & phkMask)
#define _photobjcode(oc, okind) (((oc) << phdOcShift) | ((okind)<<phdOkShift))
#define is_photobjcode(detail, oc, okind) ( ((detail) & ~phkMask) == _photobjcode(oc, okind) )
#define full_photobjcode(oc, okind) ( phkObject | _photobjcode(oc, okind) )
#define photobjback(detail, ocvar, objkindvar) do { ocvar = ((detail) >> phdOcShift) & ocMask; objkindvar = ((detail) >> phdOkShift) & 255; } while (0)
#define photo_cc(cc) ((cc) << phkBits)
#define photo2cc(detail) ( ((detail) >> phkBits) & 255 )
#define photo_obk photo_cc
#define photo2obk photo2cc

my_enum1 enum
{ aaLie = 1, aaWear = 2, aaWield = 1, aaOnHand = 1, aaCarry = 2,
  aaLitLantern = 5, aaLitWeapon = 1000, aaHitting = 20, aaBeingHit = 30,
  aaFriction = 10, aaBeingApplied = 5, aaRusts = 50, aaWormy = 100,
  aaInvoke = 500, aaKeepZap = 500
} my_enum2(tFewTicks) tAgingAmount;

/* "Basic" object materials; don't mix these up with other, "non-basic"
   material notions in this game (ring gems, staff woods etc.)! -- bomSpecial
   is used in the objectclass[] array to indicate that the object materials
   don't only depend on the class, but also on the _kind_ of an object. -- To
   find out the actual basic materials of an object, use object_materials(). */

my_enum1 enum
{ bomNone = 0, bomMetalS = 0x01, bomMetalN = 0x02, bomWood = 0x04,
  bomLeather = 0x08, bomStone = 0x10, bomLiquid = 0x20, bomGlass = 0x40,
  bomPaper = 0x80, bomTextile = 0x100, bomBone = 0x200, bomMagic = 0x400,
  bomOther = 0x800
} my_enum2(tUint16) tBasicObjectMaterials;
#define bomSpecial (bomNone)
#define bomMetalAny (bomMetalS | bomMetalN) /* stainless or normal */

/* Functions */

extern __sallocator /*@notnull@*/ tObject* __callocator object_allocate(void);
extern void object_deallocate(/*@only@*/ const tObject*);
extern unsigned char amulet_misscount(const tObjectDetail);
extern tToolClass object_toolkind2class(tToolKind);
extern tDamage object_helmet_armorval(const tObject*);
extern tUint8 object_armor_hindrance(const tObject*);
extern tBoolean object_weapon_combinable(const tObject*, const tObject*);
extern unsigned char object_weapon_relsize(const tWeaponKind);
extern tUint8 object_weapon_hindrance(const tObject*);
extern unsigned char wandstaff_can_affect_bom(const tWandStaffKind,
  const tBasicObjectMaterials, /*@out@*/ tObstacleEffects*);
extern tBoolean wandstaff_can_break_barrier(const tWandStaffKind);
extern tMagicEnergy object_spellcastingcost(const tScrollKind);
extern tBoolean object_confused_scrollkind(/*inout*/ tScrollKind*);

extern /*@null@*/ __sallocator tObject* __callocator
  object_create(const tObjectClass, const tObjectKind, const unsigned char);
extern /*@null@*/ __sallocator tObject* __callocator
  object_randcreate(const unsigned char);
extern /*@null@*/ __sallocator tObject* __callocator
  object_tool_quarks2object(tQuarkValue);
extern tBoolean object_randlocatearound(tObject*, tCoordinate, tCoordinate);
extern /*@notnull@*/ __sallocator tObject* __callocator
  object_split(tObject*, tObjectQuantity);
extern void object_call(tObjectClass, tObjectKind, char*);

extern void object_age_basic(tObject*, const tTicks);
extern void object_handle_sectionchange(void);

extern void object_put_on_list(tObject**, tObject*);
extern void object_remove_from_list(tObject**, tObject*);

extern /*@null@*/ tObject* location2object(const tCoordinate,
  const tCoordinate);

extern char object_get_classchar(const tObjectClass);
extern const char* object_get_classname(const tObjectClass);
extern char object_get_char(const tObject*);
extern tCreatureData object_get_randimi(void);
extern chtype object_get_charcolor(const char);
extern /*@observer@*/ const char*
  object_classes2chars(const tObjectClassesBitmask);
extern void object_get_description(const tObject*, char*);
extern tBoolean oc_pluralable(const tObjectClass);
extern tBoolean _object_get_minidesc(const tObjectClass, tObjectKind,
  const tObjectQuantity, /*@out@*/ char*, const unsigned char, const char*);
#define object_get_minidesc(obj, dest, flags0, intro) _object_get_minidesc((obj)->oc, (obj)->kind, (obj)->quantity, dest, flags0, intro)
extern unsigned char object_importance(const tObject*);
extern tObjectWeight object_weight(const tObject*);
extern tHoeValue object_hoevalue(/*@null@*/ const tObject*,
  const tBasicObjectMaterials, const unsigned char);
extern tBasicObjectMaterials object_materials(const tObject*);
extern tBasicObjectMaterials object_mainmat(const tObject*);
extern tBoolean object_is_enchantable(const tObject*);

extern /*@null@*/ tObject* object_ttpt(tObject* /*list-element*/,
  tObject* /*object*/, const tBoolean);
extern /*@null@*/ tObject* object_try_to_put_together(tObject* /*list*/,
  tObject* /*object*/, const tBoolean);

extern void object_light_off(tObject*);
extern void object_light_on(tObject*);
extern tLightIntensity object_light(const tObject*, /*@null@*/ /*@out@*/
  tUint8*);
#define object_might_emit_light(object) (object_light(object, NULL) > 0)
#define lit_value(lt) ( ((lt) >= 1000) ? 2 : 1 )
extern void object_gets_wet(tObject*, const tPercentage,
  /*@null@*/ const char*);

extern tObjectKind object_dump_associations(const unsigned char);
#if CONFIG_DEBUG
extern void object_dump_roca(void);
#endif

extern void object_initialize(void);

#define object_still_exists(object) \
  ( (object->quantity > 0) && (object->lifetime > 0) )

#define __for_each_object(temp) for (; temp != NULL; temp = temp->next)
#define for_each_object(first, temp) \
  for (temp = (first); temp != NULL; temp = temp->next)
#define for_each_currentsectionobject(temp) \
  for_each_object(section_get_objects(), temp)
/* (All these can't be used if you're deallocating objects from the list!) */

#endif /* #ifndef __atrogue_object_h__ */
