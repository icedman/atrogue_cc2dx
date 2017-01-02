/* atrogue/object.c - object handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "object.h"
#include "random.h"
#include "creature.h"
#include "dungeon.h"
#include "message.h"

static const char strDoNothing[] = "do nothing", strQuietness[] = "quietness",
  strSlow[] = "slow", strHaste[] = "haste", strLevitate[] = "levitate",
  strSearch[] = "search", strSleep[] = "sleep", strFreeze[] = "freeze",
  strTeleport[] = "teleport", strPolymorph[] = "polymorph", strGold[] = "gold",
  strGlass[] = "glass", strStone[] = "stone", strFood[] = "food",
  strRing[] = "ring", strFourPercs[] = "%s %s %s %s", strHoe[] = "hoe",
  strConfuse[] = "confuse";
#define strThreePercs (strFourPercs + 3)

const char strObjectClassChars[] = ",%&*:[)!/?=", /* for map display etc. */
  strMetal[] = "metal", strWood[] = "wood", strWooden[] = "wooden",
  strArmor[] = "armor", strHelmet[] = "helmet", strDown[] = "down",
  strUp[] = "up";
const char* const strGwsm[4] = { strGlass, strWooden, strStone, strMetal };

static tUint8 foodcount = 0;
  /* total number of food things created during the game */

typedef void (*tObjectDataFiller)(tObject*);
typedef void (*tObjectGetDesc)(const tObject*, char*);

typedef tSint32 tRelativeAmount;
typedef tSint32 tTotalRelativeAmount;
  /* (signed in order to simplify calculations; 32 bits because the numbers
      must be quite large in order to "survive" calculations like
      "(amount * current_storeynum) / dude.sn_hardmax") */

static tRelativeAmount roca[maxObjectClass + 1];
  /* "relative object-class amount"; values depend on storey etc. */
static tTotalRelativeAmount allclasses_amount;

/* prototypes */
static /*@observer@*/ const char* object_get_subdesc(const tObject*);
#if CONFIG_DEBUG
static tObjectWeight object_oldweight(const tObject*);
#endif

my_inline tObject* object_allocate(void)
{ return(memory_allocate(sizeof(tObject)));
}

void object_deallocate(const tObject* object)
{ const tObject *o = object->contents;
  while (o != NULL) /* recursively remove contents, if any */
  { const tObject* const next = o->next;
    object_deallocate(o); o = next;
  }
  memory_deallocate(object);
}

static tBoolean do_enchant(void)
/* returns whether a newly created object should be assigned an enchantment
   value; only depends on chance */
{ tPercentage p = 4 * difficulty + 7 * pref_magic + (3*current_storeynum) / 2;
  if (pref_role == roleWizard) p += MIN(4 * current_storeynum, 30);
  return(randpercent(p));
}

#if CONFIG_DEBUG
#define DEBUG_SUBDESC_FORMAT "%s"
#define DEBUG_SUBDESC , object_get_subdesc(object)
#else
#define DEBUG_SUBDESC_FORMAT
#define DEBUG_SUBDESC
#endif

static void object_create_amulet(tObject* object)
{ tObjectDetail detail;
  object->quantity = 1; object->lifetime = MAX_FEWTICKS;
  if (object->kind == okNone) object->kind = amkYendor;
  if (object->kind == amkYendor)
  { object->damage = 100; object->enchantment = 100;
    /* Note the effects of the high damage and enchantment values when the
       player character throws the Amulet - not many creatures can stand the
       resulting damage; but on the other hand, the player character can throw
       it only once (needs to move and pick it up before he can throw it again)
       and often risks that it falls into an sfWater, loses its enchantment or
       such. */
    detail = 1 | AMULET_FRACTION(1, dude.aoy_pieces); /* fixed necklace */
    if ( (pref_feat >= 5 - difficulty) && (__randpercent(2*(1<<difficulty))) )
      detail |= _AMULET_FAKE;
  }
  else
  { const unsigned char d = ( ((pref_feat > 0) && (randpercent(16 * difficulty
      + current_storeynum / 2))) ? 2 : 1 );
    if (do_enchant()) object->enchantment = randbytemod(5) - 2;
    object->damage = 10; detail = AMULET_FRACTION(1, d);
    if ( (pref_feat <= 0) || (__randpercent(80 - 15 * difficulty)) )
      detail |= 1; /* fixed necklace */
    if ( (pref_feat > 1) && (__randpercent(15 * difficulty + 1)) )
      detail |= _AMULET_FAKE;
  }
  object->objdetail = detail;
}

static void object_getdesc_amulet(const tObject* object, char* dest)
{ static const char strAoy[] = "Amulet of Yendor", strWn[] = " with necklace";
  const tObjectDetail detail = object->objdetail;
  unsigned char n, d;
  const char* src;
  char ch;
  AMULET_ND(detail, n, d);
  if (n < d)
  { *dest++ = '0' + n; *dest++ = '/'; *dest++ = '0' + d; *dest++ = ' '; }
  if (detail & _AMULET_FAKEKNOWN)
  { if (detail & _AMULET_FAKE) { strcpy(dest, "fake "); dest += 5; }
    else { strcpy(dest, "genuine "); dest += 8; }
  }
  src = strAoy; while ( (ch = *src++) != '\0' ) *dest++ = ch;
  if ( (object->kind != amkYendor) && (AMULET_NECKLACE(detail) > 0) )
  { src = strWn; while ( (ch = *src++) != '\0' ) *dest++ = ch; }
  strcpy(dest, object_get_subdesc(object));
}

unsigned char amulet_misscount(const tObjectDetail detail)
{ unsigned char n, d;
  AMULET_ND(detail, n, d);
  return( (n >= d) ? 0 : (d - n) );
}

static void object_create_gold(tObject* object)
{ const tStoreyNumber storey = current_storeynum;
  unsigned short min = 2 * storey, max = 16 * storey;
  if (max > 255) max = 255;
  object->quantity = randbytefromrange(min, max);
}

static void object_getdesc_gold(const tObject* object, char* dest)
{ if (object->quantity == 1)
    sprintf(dest, "a piece of gold" DEBUG_SUBDESC_FORMAT DEBUG_SUBDESC);
  else
  { sprintf(dest, "%d pieces of gold" DEBUG_SUBDESC_FORMAT, object->quantity
      DEBUG_SUBDESC);
  }
}

tBoolean toolkind_id[maxToolKind + 1]; /* whether kind identified */

typedef struct
{ const char* name;
  tToolClass toolclass;
  tStoreyNumber minstorey;
  tObjectWeight weight;
  tBasicObjectMaterials mainmat, furthermat;
  unsigned char flags;
    /* "flags & 1": plural; "&2": assign enchantment; "&4": non-auto;
       "&8": kind trivially identified (used for initialization);
       "&16": not allowed in object_tool_quarks2object(); "&32": is container
    */
} tToolData;

static const tToolData tooldata[maxToolKind + 1] =
{ { strQuietness, tocBoots,    2, 20, bomLeather, bomNone, 1 | 2 },
  { strHaste, tocBoots,        4, 10, bomLeather, bomNone, 1 | 2 },
  { strLevitate, tocBoots,     5,  0, bomLeather, bomNone, 1 | 2 },
  { "kick", tocBoots,          3, 10, bomLeather, bomNone, 1 | 2 },
  { "jump", tocBoots,          5, 10, bomLeather, bomNone, 1 | 2 },
  { "swim", tocBoots,          6, 10, bomLeather, bomNone, 1 | 2 },
  { "friction", tocBoots,      5, 10, bomLeather, bomNone, 1 | 2 },
  { "strength", tocGloves,     4,  4, bomTextile, bomNone, 1 | 2 },
  { "skill", tocGloves,        3,  4, bomTextile, bomNone, 1 | 2 },
  { "invisibility", tocCape,   7, 10, bomTextile, bomNone, 2 },
  { "displacement", tocCape,   6, 10, bomTextile, bomNone, 2 },
  { "shield", tocCape,         5, 10, bomTextile, bomNone, 2 },
  { "oilskin", tocCape,        6, 10, bomTextile, bomNone, 2 }, /* magic... */
  { strSearch, tocGlasses,     3,  5, bomGlass, bomMetalN, 1 | 2 },
  { "darkness", tocGlasses,    2,  5, bomGlass, bomMetalN, 1 | 2 },
  { strUp, tocQuark,           0,  0, bomOther, bomNone, 4 },
  { strDown, tocQuark,         0,  0, bomOther, bomNone, 4 },
  { "charm", tocQuark,         0,  0, bomOther, bomNone, 4 },
  { "strange", tocQuark,       0,  0, bomOther, bomNone, 4 },
  { "top", tocQuark,           0,  0, bomOther, bomNone, 4 },
  { "bottom", tocQuark,        0,  0, bomOther, bomNone, 4 },
  { "expel", tocWhistle,       5,  3, bomWood, bomNone, 2 },
  { "signal", tocWhistle,      6,  3, bomWood, bomNone, 2 },
  { strDoNothing, tocWhistle,  1,  3, bomWood, bomNone, 2 },
  { "hold gas", tocShawl,      3,  8, bomTextile, bomNone, 2 },
  { "find food", tocShawl,     4,  8, bomTextile, bomNone, 2 },
  { "breath", tocShawl,        7,  8, bomTextile, bomNone, 2 },
  { "pen", tocNone,            6,  2, bomWood, bomOther, 2 | 8 | 16 },
  { "die", tocNone,            6,  3, bomWood, bomNone, 2 | 8 },
  { "case", tocNone,           6, 10, bomWood, bomNone, 2 | 8 | 16 /* |32 */ },
  { "heart of gold", tocNone,  0, 20, bomMetalS, bomNone, 4 | 8 | 16 },
  { "no tea", tocNone,         0,  0, bomNone /*(yes:-)*/, bomNone, 4 | 8 |16},
  { "towel", tocNone,          0,  8, bomTextile, bomNone, 4 | 8 | 16 },
  { "lantern", tocNone,        1, 20, bomGlass, bomOther, 8 },
  { "camera", tocNone,         0, 10, bomMetalN, bomOther, 2 | 8 },
  { "photograph", tocNone,     0,  1, bomPaper, bomNone, 4 | 8 | 16 },
  { "key", tocNone,            0,  1, bomSpecial, bomNone, 2 | 4 | 8 },
  { "box", tocNone,            3, 99, bomWood, bomNone, 2 | 4 | 8 | 16 | 32 },
  { "dynamite", tocNone,       0,  5, bomOther, bomNone, 4 | 8 },
  { "statue", tocNone,         2,  5, bomSpecial, bomNone, 2 | 8 | 32 },
  { "tinfoil", tocHelmet,      9,  2, bomMetalN, bomNone, 2 },
  { "dunce cap", tocHelmet,    3,  1, bomPaper, bomNone, 2 },
  { "neckhat", tocHelmet,      7, 10, bomMetalN, bomNone, 2 | 4 },
  { "necklace", tocNone,       6,  7, bomMetalS, bomNone, 2 | 8 }
};

static const struct
{ const char *name, *applytext;
} toolclassdata[maxToolClass + 1] =
{ { strUnknown, strUnknown },
  { "boots", "on feet" },
  { "gloves", "on hands" },
  { "cape", "on shoulders" },
  { "glasses", "on nose" },
  { "quark", strUnknown },
  { "whistle", strUnknown },
  { "shawl", "on face" }, /* CHECKME! */
  { strHelmet, "on head" }
};

static void object_create_tool(tObject* object)
{ tObjectKind kind = object->kind;
  tStoreyNumber stor, featlim;
  unsigned char count;
  if (kind != okNone) goto kind_set; /* already set */

  if (pref_feat <= 0) { kind = tokLantern; goto set_kind; } /* minimalism */
  stor = current_storeynum - difficulty; featlim = 2 * pref_feat;
  if (stor > featlim) stor = featlim;
  if (stor < 1) stor = 1;
  count = 2 * maxToolKind;
  while (--count > 0)
  { const tToolData* t;
    kind = randbytemod(maxToolKind + 1); t = &(tooldata[kind]);
    if (t->flags & 4) continue;
    if (t->minstorey <= stor) goto set_kind;
  }
  kind = (randcointoss() ? tokLantern : tokWhistleNothing); /* fallback */
  set_kind: object->kind = kind;

  kind_set:
  object->quantity = 1; /* must be 1 (or change apply-tool algorithms etc.) */
  if (kind == tokStatue)
  { object->objdetail = statue_build(randbytemod(4),
      randbytemod(maxRandCreatureClass) + 1);
  }
  if ( (tooldata[kind].flags & 2) && (do_enchant()) )
    object->enchantment = randbytemod(5) - 2;
  if (object_tool_is_quark(kind)) object->lifetime = 100 + randbytemod(50);
  else if (kind == tokNecklace) object->lifetime = MAX_FEWTICKS;
  else
  { tFewTicks lt = 2000 * (6 - difficulty) + sectionsizemeasure / 25 +
      randbytemod(200);
    if (kind == tokNoTea)
    { tFewTicks max = 1000 * (6 - difficulty) + randbytemod(200);
      if (lt > max) lt = max;
    }
    object->lifetime = lt;
  }
}

my_inline tToolClass object_toolkind2class(tToolKind tok)
{ return(tooldata[tok].toolclass);
}

static tBoolean getdesc_photograph(const tObject* photo, char* dest)
{ static const char strPhotoOf[] = "a photo of ";
  enum { len = sizeof(strPhotoOf) - 1 };
  tBoolean retval;
  tObjectDetail detail;
  tObjectClass oc;
  tObjectKind objkind;
  if (!(photo->flags & ofKnownMaterial)) { failed: retval = falsE; goto out; }
  detail = photo->objdetail;
  switch (photo_kind(detail))
  { case phkDarkness: strcpy(dest, "a dark photo"); break;
    case phkRock: strcpy(dest, strPhotoOf); strcpy(dest+len, strStone); break;
    case phkCreature: strcpy(dest, strPhotoOf);
      creatureclassname((tCreatureClass) photo2cc(detail), dest + len, 1);
      break;
    case phkObject: strcpy(dest, strPhotoOf); photobjback(detail, oc, objkind);
      (void) _object_get_minidesc(oc, objkind, 1, dest + len, 0, NULL); break;
    case phkObstacle: strcpy(dest, strPhotoOf);
      section_obstaclekindname(photo2obk(detail), dest + len, 1); break;
    default: goto failed; /*@notreached@*/ break;
  }
  retval = truE;
  out: return(retval);
}

static void object_getdesc_tool(const tObject* object, char* dest)
{ const tObjectKind kind = object->kind;
  const char* const subdesc = object_get_subdesc(object);
  if ( (kind == tokPhotograph) && (getdesc_photograph(object, dest)) )
  { if (*subdesc) strcat(dest, subdesc); }
  else if (kind == tokKey)
  { const tObjectDetail detail = object->objdetail;
    const char *colorstr, *infix,
      *matstr = (key_is_metal(detail) ? strMetal : strWooden);
    if (object->flags & ofKnownMaterial)
    { const tColorCode cc = hallu_color(key_color(detail));
      colorstr = strColor[cc]; infix = strSpace;
    }
    else { colorstr = infix = strEmpty; }
    sprintf(dest, "a %s%s%s key%s", colorstr, infix, matstr, subdesc);
  }
  else if (kind == tokStatue)
  { const tObjectDetail detail = object->objdetail;
    dest += sprintf(dest, "a %s statue of ",strGwsm[_statue_material(detail)]);
    creatureclassname(statue_cc(detail), dest, 1);
    if (*subdesc) strcat(dest, subdesc);
  }
  else
  { const tToolClass toc = object_toolkind2class(kind);
    const char* const notion = ( (toc == tocNone) ? tooldata[kind].name :
      toolclassdata[toc].name );
    tBoolean use_plural =
      cond2boolean( (object->quantity != 1) || (tooldata[kind].flags & 1) );
    if (kind == tokNoTea) use_plural = truE; /* grammar hack */
    sprintf(dest, "%s%s%s%s", (use_plural ? strEmpty :
      indefinite_article(notion[0])), (use_plural ? strEmpty : strSpace),
      notion, subdesc);
  }
}

static tBasicObjectMaterials tool_mainmat(const tObject* object)
/* returns the "main" materials of the tool */
{ static const tBasicObjectMaterials statuemat[4] =
  { bomGlass, bomWood, bomStone, bomMetalN };
  tBasicObjectMaterials retval;
  const tObjectKind kind = object->kind;
  switch (kind)
  { case tokKey:
      retval = (key_is_metal(object->objdetail) ? bomMetalN : bomWood); break;
    case tokStatue:
      retval = statuemat[_statue_material(object->objdetail)]; break;
    default: retval = tooldata[kind].mainmat; break; /* the "trivial" cases */
  }
  return(retval);
}

tDamage object_helmet_armorval(const tObject* helmet)
{ tDamage retval; /* how much damage the helmet holds off its user */
  switch (helmet->kind)
  { case tokHelmetDunceCap: retval = 0; break;
    case tokHelmetTinfoil: retval = 1; break;
    default: retval = 2; break;
  }
  retval += helmet->enchantment;
  if (retval < 0) retval = 0;
  return(retval);
}

static void object_create_stone(tObject* object)
{ tObjectKind kind = object->kind;
  tRandomByte rb;
  tDamage damage;
  if (kind != okNone) goto kind_set; /* already set */

  rb = randbytemod(100);
  if (rb <= 15) kind = stkFlint;
  else if (rb <= 30) kind = stkGray;
  else kind = stkBlack;
  object->kind = kind;

  kind_set:
  object->quantity = randbytefromrange(5 - difficulty / 2, 11 - difficulty);
  damage = kind + randbytemod(2);
  if (damage < 1) damage = 1;
  object->damage = damage;
}

static void object_getdesc_stone(const tObject* object, char* dest)
{ static const char* const stone[maxStoneKind + 1] =
  { "flint", "gray ", "black " };
  const char* name = ((object->flags & ofKnownMaterial) ? stone[object->kind] :
    strEmpty);
  const tObjectQuantity q = object->quantity;
  if (q == 1)
    sprintf(dest, "a %sstone" DEBUG_SUBDESC_FORMAT, name DEBUG_SUBDESC);
  else
    sprintf(dest, "%d %sstones" DEBUG_SUBDESC_FORMAT, q, name DEBUG_SUBDESC);
}

static void object_create_food(tObject* object)
{ static const unsigned char
    mod[maxDifficulty + 1] = { 3, 2, 3, 4, 4, 3 },
    sub[maxDifficulty + 1] = { 0, 0, 1, 2, 3, 3 };
  tObjectKind kind = object->kind;
  const tStoreyNumber storey = current_storeynum;
  tObjectQuantity quantity = ( ( (difficulty > dkNormal) ||
    (storey <= 5) || (__randpercent(80)) ) ? 1 : 2 );
  tRandomByte rb;
  if (kind != okNone) goto kind_set; /* already set */

  rb = randbytemod(100);
  if (rb <= 60) kind = fkRation;
  else if ( (rb <= 90) || (pref_feat <= 0) ) kind = fkFruit;
  else if (rb <= 95) kind = fkCarrot;
  else if ( (rb <= 96 + difficulty) || (storey <= 5) ) kind = fkSolidWater;
  else kind = fkSpinach;
  /* (fkVegetables/fkCorpse is never created by chance, only by explicit source
     code) */
  object->kind = kind;

  kind_set:
  object->quantity = quantity;
  if (do_enchant())
    object->enchantment = randbytemod(mod[difficulty]) - sub[difficulty];
  object->lifetime = sectionsizemeasure / 100 + 200 * randfewticksfromrange(15,
    50 - 5 * (difficulty - SIGN(object->enchantment)));

  foodcount += quantity;
  if (foodcount > 200) foodcount = 200; /* (just to stop counting) */
}

static void object_getdesc_food(const tObject* object, char* dest)
{ const tObjectKind kind = object->kind;
  const tObjectQuantity quantity = object->quantity;
  const char* const subdesc = object_get_subdesc(object);
  if (kind == fkSolidWater)
  { if (quantity == 1) sprintf(dest, "some solid water%s", subdesc);
    else sprintf(dest, "%d pieces of solid water%s", quantity, subdesc);
  }
  else if (kind == fkFruit)
  { static const char strFruit[] = "slime mold";
    if (quantity == 1) sprintf(dest, "a %s%s", strFruit, subdesc);
    else sprintf(dest, "%d %ss%s", quantity, strFruit, subdesc);
  }
  else if (kind == fkCorpse)
  { const tCreatureClass cc = object->objdetail;
    const char *const name = _creatureclassname(cc),
      ch = ( (cc == ccUnicorn) ? 'x' : name[0] ); /* grammar hack */
    if (quantity == 1)
    { sprintf(dest, "%s %s corpse%s", indefinite_article(ch), name, subdesc); }
    else sprintf(dest, "%d %s corpses%s", quantity, name, subdesc);
  }
  else if (kind == fkCarrot)
  { if (quantity == 1) sprintf(dest, "a carrot%s", subdesc);
    else sprintf(dest, "%d carrots%s", quantity, subdesc);
  }
  else /* various rations */
  { const char* notion;
    switch (kind)
    { case fkVegetables: notion = "vegetables"; break;
      case fkSpinach: notion = "spinach"; break;
      default: notion = strFood; break;
    }
    if (quantity == 1) sprintf(dest, "a ration of %s%s", notion, subdesc);
    else sprintf(dest, "%d rations of %s%s", quantity, notion, subdesc);
  }
}

static const char* const armorprefix[maxArmorKind + 1] =
{ /* leather: */ strEmpty, "studded ",
  /* metal: */   strRing, "scale", "chain", "banded", "splint", "plate"
};

static void object_create_armor(tObject* object)
{ tDamage damage; /* damage it holds off its user */
  tStoreyNumber stor;
  tObjectKind kind = object->kind;
  if (kind != okNone) goto kind_set; /* already set */

  stor = current_storeynum / zero2one(difficulty);
  kind = randbytemod(MIN(MAX(stor, ARMOR_LAST_LEATHER + 2), maxArmorKind + 1));
    /* (Better armor kinds are only available in deeper storeys. But in any
        storey/difficulty, at least the weakest _metal_ armor shall be
        available.) */
  object->kind = kind;

  kind_set:
  object->quantity = 1; /* must be 1 (or change wear-armor algorithms!) */
  damage = kind + 3 - difficulty;
  if (damage < 1) damage = 1;
  else if (damage > 3) damage = damage + randbytemod(3) - 1;
  object->damage = damage;
  if (do_enchant())
    object->enchantment = randbytemod(3 + difficulty) - difficulty;
  object->lifetime = 10000 + sectionsizemeasure / 20 - 1000 * difficulty +
    200 * kind + randfewticksfromrange(0, 200);
}

static void object_getdesc_armor(const tObject* object, char* dest)
{ const tObjectKind kind = object->kind;
  const tObjectQuantity quantity = object->quantity;
  const char *suffix = (ARMOR_IS_LEATHER(kind) ? "leather armor" : " mail"),
    *prefix = armorprefix[kind], *subdesc = object_get_subdesc(object);
  if (quantity == 1) sprintf(dest, "%s%s%s", prefix, suffix, subdesc);
  else sprintf(dest, "%d pieces of %s%s%s", quantity, prefix, suffix, subdesc);
}

tUint8 object_armor_hindrance(const tObject* object)
{ return((tUint8) (object->kind + 1)); } /* IMPLEMENTME: more details! */

static const char strPoisonDart[] = "poison dart", strCrossbow[] = "crossbow",
  strCrossbowBolt[] = "crossbow bolt", strLongSword[] = "long sword";
#define strDart (strPoisonDart + 7)   /* ug */
#define strBolt (strCrossbowBolt + 9) /* ly */
#define strSword (strLongSword + 5)   /* (-: */
#define strBow (strCrossbow + 5)

typedef struct
{ const char* name;
  tStoreyNumber minstorey;
  tDamage damage; /* (damage when wielded and used for "simple" hits) */
  tUint8 minquant, maxquant; /* (minimal and maximal quantities) */
  tObjectWeight weight;
  unsigned char flags; /* bits 0..2: rough relative size; bits 3..4: pref_feat
    limitation; bits 5..6: hindrance */
  tBasicObjectMaterials mainmat, furthermat;
} tWeaponData;

#define WF(x) | ((x) << 3) /* "weapon feature"; related to pref_feat */
#define WH(x) | ((x) << 5) /* weapon hindrance */
static const tWeaponData weapondata[maxWeaponKind + 1] =
{ { strDart,            1,  1, 4, 15,  3, 0, bomMetalN, bomWood },
  { strPoisonDart,      3,  3, 3,  7,  3, 0 WF(1), bomMetalN, bomWood },
  { "dagger",           2,  5, 1,  1, 10, 2, bomMetalN, bomNone },
  { "shuriken",         3,  3, 1,  3,  4, 2, bomMetalN, bomNone },
  { "arrow",            1,  2, 3, 12,  4, 3, bomMetalN, bomWood },
    /* (damage between dart and poison dart) */
  { strBow,             1,  1, 1,  1,  7, 4 WH(1), bomWood, bomNone },
  { strCrossbow,        2,  1, 1,  1,  9, 4 WF(2), bomWood, bomNone },
  { strCrossbowBolt,    2,  2, 2,  8,  3, 2 WF(2), bomMetalN, bomNone },
  { "fire bolt",        3,  2, 2,  6,  4, 2 WF(2), bomMetalN, bomWood },
    /* (causes more damage when being lit) */
  { "spear",            3,  4, 1,  5, 10, 5 WH(1), bomMetalN, bomWood },
  { "sling",            4,  1, 1,  1,  3, 1 WF(1), bomWood, bomLeather },
    /* (for use with stones) */
  { "blowgun",          4,  1, 1,  1,  3, 2 WF(2), bomWood, bomNone },
    /* (for use with darts and poison darts) */
  { strHoe,             2,  5, 1,  1, 15, 4 WH(1), bomMetalN, bomWood },
  { "club",             1,  2, 1,  1, 15, 3 WH(1), bomWood, bomNone },
  { "mace",             2,  5, 1,  1, 20, 3 WH(1), bomMetalN|bomWood,bomNone },
    /* (spiked club) */
  { strSword,           4,  7, 1,  1, 25, 5 WH(1), bomMetalN, bomNone },
  { strLongSword,       6, 10, 1,  1, 40, 6 WH(2), bomMetalN, bomNone },
  { "two-handed sword", 7, 15, 1,  1, 60, 7 WH(3), bomMetalS, bomNone },
  { "harpoon",          5,  2, 1,  1, 10, 3 WF(3), bomMetalN, bomWood },
  { "boomerang",        4,  1, 1,  1,  5, 2 WF(3), bomWood, bomNone },
  { "polearm",          4,  5, 1,  1, 30, 7 WF(2) WH(2), bomMetalN, bomNone }
};
#undef WF
#undef WH

static void object_create_weapon(tObject* object)
{ tObjectKind kind = object->kind;
  tObjectQuantity minquant, maxquant, hardmax;
  tDamage damage;
  tStoreyNumber storey, stor;
  const tWeaponData* w;
  unsigned char count;
  if (kind != okNone) goto kind_set; /* already set */

  storey = current_storeynum;
  stor = storey / zero2one(MIN(difficulty, 3));
  if ( (stor < 2) && (storey >= 2) && (difficulty <= dkNormal) ) stor = 2;
  if (stor < 1) stor = 1;
  count = 2 * maxWeaponKind;
  while (--count > 0)
  { kind = randbytemod(maxWeaponKind + 1); w = &(weapondata[kind]);
    if (pref_feat < ((w->flags >> 3) & 3)) continue;
    if (w->minstorey <= stor) goto set_kind;
  }
  kind = (randcointoss() ? wkDart : wkArrow); /* fallback */
  set_kind: object->kind = kind;

  kind_set:
  w = &(weapondata[kind]); minquant = w->minquant; hardmax = w->maxquant;
  maxquant = hardmax - difficulty; /* less maxquant for higher difficulties */
  if (maxquant <= minquant) maxquant = minquant + 1; /* "+1" to get a range */
  if (maxquant > hardmax) maxquant = hardmax; /* ...but never beyond hardmax */
  object->quantity = randbytefromrange(minquant, maxquant);

  damage = w->damage;
  if (difficulty > dkNormal) damage--;
  else damage += randbytemod(3) - 1;
  if (damage < 1) damage = 1;
  object->damage = damage;

  if (do_enchant())
    object->enchantment = randbytemod(3 + difficulty) - difficulty;
  object->lifetime = 10000 + sectionsizemeasure / 20 - 1000 * difficulty +
    randfewticksfromrange(0, 200);
}

static void object_getdesc_weapon(const tObject* object, char* dest)
{ const tObjectKind kind = object->kind;
  const char *name = weapondata[kind].name,
    *subdesc = object_get_subdesc(object);
  const tObjectQuantity quantity = object->quantity;
  if (quantity == 1)
    sprintf(dest, "%s %s%s", indefinite_article(name[0]), name, subdesc);
  else sprintf(dest, "%d %ss%s", quantity, name, subdesc);
}

tBoolean object_weapon_combinable(const tObject* wielded,
  const tObject* throw)
/* returns whether the objects are "combinable", i.e. a useful arrangement of a
   wielded weapon and a throwable object; cf. creature_throw(); in NetHack
   terminology, such a wielded weapon is called a "launcher" */
{ tBoolean retval = falsE;
  const tObjectClass tclass = throw->oc;
  const tObjectKind wkind = wielded->kind;
  if (tclass == ocWeapon)
  { const tObjectKind tkind = throw->kind;
    if (wkind == wkBow) { if (tkind == wkArrow) retval = truE; }
    else if (wkind == wkCrossbow)
    { if ( (tkind == wkCrossbowBolt) || (tkind == wkFireBolt) ) retval = truE;}
    else if (wkind == wkBlowgun)
    { if ( (tkind == wkDart) || (tkind == wkPoisonDart) ) retval = truE; }
  }
  else if ( (wkind == wkSling) && (tclass == ocStone) ) retval = truE;
  return(retval);
}

unsigned char object_weapon_relsize(const tWeaponKind kind)
{ return(weapondata[kind].flags & 7); }

tUint8 object_weapon_hindrance(const tObject* object)
{ return((weapondata[object->kind].flags >> 5) & 3); }

#if maxColorCode < maxPotionKind
#error "Too few color names for potions!"
#endif

static tRandomByte potion_kind2colorcode[maxPotionKind + 1];
tBoolean potionkind_id[maxPotionKind + 1]; /* whether kind identified */
static char* potioncall[maxPotionKind + 1]; /* user's name for the potion */

static const char strGreatlyHeal[] = "greatly heal";
#define strHeal (strGreatlyHeal + 8) /* ugly (-: */

static const struct
{ const char* name;
  tStoreyNumber minstorey;
  unsigned char flags; /* "flags & 1": rare potion; "&2": assign enchantment */
} potiondata[maxPotionKind + 1] =
{ {   "increase strength", 3, 2 },
  {    "restore strength", 3, 2 },
  { "increase experience", 4, 1 | 2 },
  {               strHeal, 2, 2 },
  {        strGreatlyHeal, 4, 1 },
  {        "magic energy", 2, 1 },
  {              "poison", 1, 0 },
  {         "liquid fire", 2, 0 },
  {            strConfuse, 1, 2 },
  {         "hallucinate", 1, 2 },
  {           "blindness", 1, 2 },
  {       "sense objects", 5, 1 | 2 },
  {     "sense creatures", 7, 1 | 2 },
  {           strLevitate, 6, 1 | 2 },
  {               strSlow, 2, 2 },
  {              strHaste, 2, 2 },
  {         "make supple", 3, 2 }
};

static void object_create_potion(tObject* object)
{ tStoreyNumber stor;
  tBoolean is_rare_okay;
  unsigned char count;
  tObjectKind kind = object->kind;
  if (kind != okNone) goto kind_set; /* already set */

  stor = current_storeynum - MAX(difficulty, 2) + 2;
  if (stor < 1) stor = 1;
  is_rare_okay = __randpercent(40 - 4 * difficulty); count = 2 * maxPotionKind;
  while (--count > 0)
  { kind = randbytemod(maxPotionKind + 1);
    if (potiondata[kind].minstorey <= stor)
    { if ( (is_rare_okay) || (!(potiondata[kind].flags & 1)) ) goto set_kind; }
  }
  kind = (randcointoss() ? pkHeal : pkPoison); /* fallback */
  set_kind: object->kind = kind;

  kind_set:
  object->quantity = ( (difficulty > dkNormal) ? 1 : randbytefromrange(1, 2) );
  if ( (potiondata[kind].flags & 2) && (do_enchant()) )
    object->enchantment = randbytemod(5) - 2;
  object->lifetime = 4000 + sectionsizemeasure / 50 - 200 * difficulty +
    randfewticksfromrange(0, 200);
}

static void object_getdesc_potion(const tObject* object, char* dest)
{ const tObjectQuantity quantity = object->quantity;
  const char* subdesc = object_get_subdesc(object);
  if (object->flags & ofKnownMaterial)
  { const tObjectKind kind = object->kind;
    const char* colorname = strColor[potion_kind2colorcode[kind]];
    if (quantity == 1)
    { sprintf(dest, "%s %s potion%s", indefinite_article(colorname[0]),
        colorname, subdesc);
    }
    else sprintf(dest, "%d %s potions%s", quantity, colorname, subdesc);
  }
  else
  { if (quantity == 1) sprintf(dest, "a potion%s", subdesc);
    else sprintf(dest, "%d potions%s", quantity, subdesc);
  }
}

#define __numWandStaffMatClasses (3) /* wands, staves, rods */
/* IMPLEMENTME: "sticks"/"bones" (made of bone)! */
#define __numWandStaffMat (14)
  /* (We use the same number for wand, staff and rod materials because
      "rustable", "burnable" and "breakable" shall have equal chance.) */
#define maxWandStaffMaterial ((__numWandStaffMatClasses*__numWandStaffMat) - 1)
  /* (first metals for wands, then woods for staves, then colors for rods) */

static const char* const strWandMaterial[__numWandStaffMat] =
{ "platinum", strGold, "silver", "bronze", "copper", "iron", "steel", "cobalt",
  "titanium", "nickel", "magnesium", "chrome", "tin", "silicon"
};

static const char* const strStaffMaterial[__numWandStaffMat] =
{ "balsa", "birch", "cedar", "cherry", "elm", "ivory", "mahogany", "maple",
  "mixed-splinters", "oak", "pine", "redwood", "teak", "walnut"
};

/* just to clarify: #define strRodMaterial strColor */

#if maxColorCode + 1 < __numWandStaffMat
#error "Too few color names for rods!"
#endif

#if maxWandStaffMaterial < maxWandStaffKind
#error "Too few wand/staff/rod materials!"
#endif

static tRandomByte wandstaff_kind2material[maxWandStaffKind + 1];
tBoolean wandstaffkind_id[maxWandStaffKind + 1]; /* whether kind identified */
static char* wandstaffcall[maxWandStaffKind + 1]; /* user's name for the wand*/
static const char strWand[] = "wand", strStaff[] = "staff",
  strStaves[] = "staves", strRod[] = "rod";

/* "wand/staff notion, singular/plural" */
static const char* const strWsnS[__numWandStaffMatClasses] =
{ strWand, strStaff, strRod };
static const char* const strWsnP[__numWandStaffMatClasses] =
{ "wands", strStaves, "rods" };

typedef struct
{ const char* name;
  tStoreyNumber minstorey;
  tUint8 flags;
    /* "flags & 3": two-bit lifetime indicator; "& 4": rare; "& 8": never
       created by chance, only by _explicit_ code; bits 4..5: pref_feat
       limitation */
} tWandStaffData;

#define WSF(x) | ((x) << 4) /* "wand/staff feature"; related to pref_feat */
static const tWandStaffData wandstaffdata[maxWandStaffKind + 1] =
{ {   "create object", 5, 1 | 4 WSF(2) },
  { "create creature", 7, 0 | 4 WSF(3) },
  {          "remove", 4, 0 | 4 WSF(3) },
  {       strTeleport, 3, 1 | 4 },
  {      strPolymorph, 7, 0 | 4 },
  {           strSlow, 1, 3 },
  {        strConfuse, 2, 2 },
  {  "make invisible", 3, 2 },
  { "make immaterial", 4, 1 | 4 WSF(2) },
  {          strSleep, 1, 3 },
  {         strFreeze, 2, 2 },
  {            strHoe, 3, 3 WSF(1) },
  {           "flame", 4, 1 | 4 WSF(1) },
  {      "remote hit", 1, 2 },
  {         "barrier", 5, 1 | 4 WSF(3) },
  {      "drain life", 1, 1 | 8 WSF(3) },
  {          "cancel", 3, 2 },
  {       "do subtle", 2, 1 },
  {      strDoNothing, 1, 3 }
};
#undef WSF

static void object_create_wandstaff(tObject* object)
{ enum { fbnum = 4 };
  static const tObjectKind fallback_kind[fbnum] =
  { wskDoNothing, wskSlow, wskSleep, wskRemoteHit };
  tStoreyNumber stor;
  tBoolean is_rare_okay;
  tFewTicks lt; /* for lifetime calculation */
  unsigned char count;
  tObjectKind kind = object->kind;
  if (kind != okNone) goto kind_set; /* already set */

  stor = current_storeynum / zero2one(MIN(difficulty, 3));
  if (stor < 1) stor = 1;
  is_rare_okay = __randpercent(40 - 4 * difficulty);
  count = 2 * maxWandStaffKind;
  while (--count > 0)
  { const tWandStaffData* ws;
    unsigned char flags;
    kind = randbytemod(maxWandStaffKind + 1); ws = &(wandstaffdata[kind]);
    flags = ws->flags;
    if (pref_feat < ((flags >> 4) & 3)) continue;
    if ( (!(flags & 8)) && (ws->minstorey <= stor) &&
         ( (is_rare_okay) || (!(flags & 4)) ) )
    { goto set_kind; } /* found an "appropriate" kind */
  }
  kind = fallback_kind[randbytemod(fbnum)]; /* fallback */
  set_kind: object->kind = kind;

  kind_set:
  object->quantity = 1; /* must be 1 (or change zapping-lifetime algorithms!)*/
  if (do_enchant()) object->enchantment = randbytemod(5) - 2;
  lt = wandstaffdata[kind].flags & 3;
  if (lt) lt *= 1000 * randfewticksfromrange(1, 3);
  object->lifetime = 3000 + sectionsizemeasure / 20 - 400 * difficulty + lt +
    randfewticksfromrange(0, 200);
}

static /*@observer@*/ const char* object_wandstaff2matstr(const tRandomByte
  material)
{ static const char* const * const arr[__numWandStaffMatClasses] =
  { strWandMaterial, strStaffMaterial, strColor };
  const tRandomByte matclass = material / __numWandStaffMat,
     idx = material % __numWandStaffMat;
  const char* const * a = arr[matclass];
  return(a[idx]);
}

static void object_getdesc_wandstaff(const tObject* object, char* dest)
{ const tObjectQuantity quantity = object->quantity;
  const tObjectKind kind = object->kind;
  const tRandomByte material = wandstaff_kind2material[kind],
    matclass = material / __numWandStaffMat;
  const char *const subdesc = object_get_subdesc(object), *const notion =
    ((quantity == 1) ? strWsnS : strWsnP)[matclass];
  if (object->flags & ofKnownMaterial)
  { const char* const materialstr = object_wandstaff2matstr(material);
    if (quantity == 1)
    { sprintf(dest, "%s %s %s%s", indefinite_article(materialstr[0]),
        materialstr, notion, subdesc);
    }
    else sprintf(dest, "%d %s %s%s", quantity, materialstr, notion, subdesc);
  }
  else
  { /* (We give the specific notion since the player character can distinguish
        metal, wood and glass with his hands, even if blind.) */
    if (quantity == 1) sprintf(dest, "a %s%s", notion, subdesc);
    else sprintf(dest, "%d %s%s", quantity, notion, subdesc);
  }
}

unsigned char wandstaff_can_affect_bom(const tWandStaffKind wsk,
  const tBasicObjectMaterials bom, /*@null@*/ /*@out@*/ tObstacleEffects* _e)
{ unsigned char retval = 0; /* 0..15; indicates how strong the effect is */
  tObstacleEffects e = oeNone;
  switch (wsk)
  { case wskRemove: retval = 15; e = oeRemove; break;
    case wskHoe: retval = 12; e = oeDamage; break;
    case wskRemoteHit: retval = 10; e = oeDamage; break;
    case wskCancel: retval = 1; e = oeCancel; break;
    case wskFlame:
      if (bom & (bomWood | bomLeather | bomPaper | bomTextile))
      { retval = 8; e = oeBurn; }
      else if (bom & bomMetalN) { retval = 6; e = oeMelt; }
      break;
  }
  /* FIXME! */
  if ( (retval > 0) && (_e != NULL) ) *_e = e;
  return(retval);
}

tBoolean wandstaff_can_break_barrier(const tWandStaffKind wsk)
{ tBoolean retval;
  switch (wsk)
  { case wskRemove: case wskHoe: retval = truE; break;
    case wskCancel: retval = cond2boolean(difficulty <= dkNormal); break;
    default: retval = falsE;
  }
  return(retval);
}

typedef struct
{ const char* name;
  tStoreyNumber minstorey;
  unsigned char flags;
    /* "flags & 7": three-bit magic energy cost indicator for spell-casting
       (0 means impossible / not allowed); "flags & 8": lifetime indicator;
       "flags & 16": rare; "flags & 32": assign enchantment; bits 6..7:
       pref_feat limitation */
} tScrollData;

#define SF(x) | ((x) << 6) /* "scroll feature"; related to pref_feat */
static const tScrollData scrolldata[maxScrollKind + 1] =
{ {       "identify", 1, 1 | 8 },
  {  "protect armor", 3, 0 | 16 | 32 },
  {  "enchant armor", 2, 0 | 16 | 32 },
  { "enchant weapon", 4, 0 | 16 | 32 },
  {      strTeleport, 7, 5 | 16 },
  {   "remove curse", 3, 7 | 16 | 32 },
  {          "scare", 1, 0 | 8 },
  {         strSleep, 1, 3 | 8 | 32 },
  {        strFreeze, 2, 4 | 32 SF(1) },
  {            "map", 5, 7 | 16 },
  {   "sense hidden", 3, 6 | 8 SF(2) },
  {          "light", 5, 3 | 8 | 32 SF(2) },
  {       "recharge", 6, 0 | 16 | 32 SF(1) },
  {           "hold", 4, 2 | 32 },
  {      "aggravate", 2, 1 | 8 },
  {           "heat", 5, 0 | 16 SF(1) },
  {         "wisdom", 3, 2 | 16 SF(3) },
  {        "petrify", 8, 7 | 16 | 32 SF(3) },
  {    "dirty paper", 1, 0 | 8 }
};
#undef SF

static const char* scrolltitle[maxScrollKind + 1];
tBoolean scrollkind_id[maxScrollKind + 1]; /* whether kind identified */
static char* scrollcall[maxScrollKind + 1]; /* user's name for the scroll */

static void object_create_scroll(tObject* object)
{ static tUint8 scrollidentifycount = 0;
  tStoreyNumber stor;
  tBoolean is_rare_okay;
  tFewTicks lt; /* for lifetime calculation */
  const tScrollData* sd;
  unsigned char count, flags;
  tObjectKind kind = object->kind;
  if (kind != okNone) goto kind_set; /* already set */

  stor = current_storeynum;
  if (scrollidentifycount < ( (2 * stor) / 3 ) - difficulty / 2)
  { kind = skIdentify; goto set_kind; }
  stor = stor - MAX(difficulty, 2) + 1;
  if (stor < 1) stor = 1;
  is_rare_okay = __randpercent(40 - 4 * difficulty);
  count = 2 * maxScrollKind;
  while (--count > 0)
  { kind = randbytemod(maxScrollKind + 1); sd = &(scrolldata[kind]);
    flags = sd->flags;
    if (pref_feat < ((flags >> 6) & 3)) continue;
    if (sd->minstorey <= stor)
    { if ( (is_rare_okay) || (!(flags & 16)) ) goto set_kind; }
  }
  kind = (randcointoss() ? skIdentify : skSleep); /* fallback */
  set_kind: object->kind = kind;

  kind_set:
  object->quantity = 1; /* must be 1 (or change scroll-reading algorithm!) */
  sd = &(scrolldata[kind]); flags = sd->flags;
  if ( (flags & 32) && (do_enchant()) )
    object->enchantment = randbytemod(3 + difficulty) - difficulty;
  lt = ( (flags & 8) ? (1000 * randbytefromrange(1, 2)) : (0) );
  object->lifetime = 3000 + sectionsizemeasure / 20 - 400 * difficulty + lt +
    randfewticksfromrange(0, 200);
  if (kind == skIdentify)
  { scrollidentifycount += object->quantity;
    if (scrollidentifycount > 200) scrollidentifycount = 200;
      /* (just to stop counting) */
  }
}

static void object_getdesc_scroll(const tObject* object, char* dest)
{ tObjectQuantity quantity = object->quantity;
  tObjectKind kind;
  const char* const subdesc = object_get_subdesc(object);
  if ( (object->flags & ofKnownMaterial) &&
       ( (kind = object->kind) != skBlankPaper) )
  { /* (Well, "material" isn't a good notion here - imagine that it were
        "ofKnownScrollTitle".) */
    const char* const st = scrolltitle[kind];
    if (quantity == 1) sprintf(dest, "a scroll entitled \"%s\"%s", st,subdesc);
    else sprintf(dest, "%d scrolls entitled \"%s\"%s", quantity, st, subdesc);
  }
  else
  { if (quantity == 1) sprintf(dest, "a scroll%s", subdesc);
    else sprintf(dest, "%d scrolls%s", quantity, subdesc);
  }
}

tMagicEnergy object_spellcastingcost(const tScrollKind kind)
/* helper function for object_dump_associations() and main.c::cast_spell() */
{ tMagicEnergy cost = ((tMagicEnergy) (scrolldata[kind].flags & 7));
  if (cost > 0)
  { cost += difficulty;
    if (rogue->confused) cost++;
    if (rogue->held) cost++; /* magic clasp obstructs, bear trap hurts */
    if (roguedata.state >= rsWeak) cost++;
    if (creature_is_ill(rogue)) cost++;
    if (!rogue_can_see) cost--;
    if (rogue->hallucinating) cost--;
    if (rogue->levitated) cost--;
      /* (When blind, the player character can concentrate better, so he needs
          less magic energy... A reason for the ->hallucinating part can be
          found in creature_hallucinate(), and levitation removes burden...) */
    cost -= roguedata.magic_exp / 10;
    if (cost < 1) cost = 1;
  }
  /* "else": this kind of scroll can't be used for spell-casting at all. */
  return(cost);
}

tBoolean object_confused_scrollkind(/*inout*/ tScrollKind* _kind)
{ tBoolean retval;
  tScrollKind orig_kind = *_kind, kind;
  const tStoreyNumber s = scrolldata[orig_kind].minstorey;
  unsigned char count = 2 * maxScrollKind;
  while (--count > 0)
  { kind = randbytemod(maxScrollKind);
    if ( (kind != orig_kind) && (scrolldata[kind].minstorey <= s) )
    { *_kind = kind; retval = truE; goto out; }
  }
  retval = falsE; /* couldn't find an appropriate replacement */
  out: return(retval);
}

/* "gem" doesn't always mean what you might expect - cf. "coal" :-) */
#define maxGem (21)
static const char* const strGem[maxGem + 1] =
{ "agate", "amethyst", "citrine", "coal", "diamond", "emerald", "garnet",
  "jade", "lapis-lazuli", "opal", "pearl", "peridot", "quartz", "rocky",
  "ruby", "sapphire", "snake-head", "stibotantalite", "tansanite", "tiger-eye",
  "topaz", "turquoise"
};

#if maxGem < maxRingKind
#error "Too few gems!"
#endif

static tRandomByte ring_kind2gem[maxRingKind + 1];
tBoolean ringkind_id[maxRingKind + 1]; /* whether kind identified */
static char* ringcall[maxRingKind + 1]; /* user's name for the ring */
static const char strFightFortune[] = "fight fortune";
#define strFortune (strFightFortune + 6) /* ugly (-: */

typedef struct
{ const char* name;
  tStoreyNumber minstorey;
  unsigned char flags; /* "flags & 1": rare; "& 2": assign enchantment; bits
    2..3: pref_feat limitation */
} tRingData;

#define RF(x) | ((x) << 2) /* "ring feature"; related to pref_feat */
static const tRingData ringdata[maxRingKind + 1] =
{ {            strHeal, 1, 2 },
  { "sustain strength", 5, 2 },
  {         strFortune, 4, 1 | 2 RF(2) },
  {    strFightFortune, 3, 1 | 2 RF(2) },
  {     "sense hidden", 7, 0 RF(2) },
  {       strQuietness, 1, 2 },
  {   "slow digestion", 2, 2 },
  {             "fear", 4, 1 | 2 },
  {     "resist sleep", 5, 1 | 2 RF(1) },
  {    "resist freeze", 5, 1 | 2 RF(1) },
  { "resist confusion", 5, 1 | 2 RF(1) },
  {          "rebirth", 8, 1 | 2 RF(3) },
  {            "honey", 4, 2 RF(2) },
  {        strTeleport, 6, 1 | 2 },
  {       "knightmove", 6, 1 | 2 RF(3) },
  {       strPolymorph, 6, 1 | 2 RF(3) },
  {          strSearch, 3, 2 },
  {       strDoNothing, 1, 2 }
};
#undef RF

static void object_create_ring(tObject* object)
{ tStoreyNumber stor;
  tBoolean is_rare_okay;
  unsigned char count, flags;
  tObjectKind kind = object->kind;
  if (kind != okNone) goto kind_set; /* already set */

  stor = current_storeynum / 2 - MAX(difficulty, 2) + 2;
  if (stor < 1) stor = 1;
  is_rare_okay = __randpercent(40 - 4 * difficulty); count = 2 * maxRingKind;
  while (--count > 0)
  { const tRingData* rd;
    kind = randbytemod(maxRingKind + 1); rd = &(ringdata[kind]);
    flags = rd->flags;
    if (pref_feat < ((flags >> 2) & 3)) continue;
    if (rd->minstorey <= stor)
    { if ( (is_rare_okay) || (!(flags & 1)) ) goto set_kind; }
  }
  kind = ( (randcointoss()) ? rkDoNothing : rkQuiet ); /* fallback */
  set_kind: object->kind = kind;

  kind_set:
  object->quantity = 1; /* must be 1 (or change puton-ring algorithms etc.!) */
  if ( (ringdata[kind].flags & 2) && (do_enchant()) )
  { if (kind == rkSlowDigestion) object->enchantment = randbytemod(3);
    else object->enchantment = randbytemod(3 + difficulty) - difficulty;
  }
  object->lifetime = 1000 * (6 - difficulty) + sectionsizemeasure / 20 +
    randfewticksfromrange(0, 200);
}

static void object_getdesc_ring(const tObject* object, char* dest)
{ const tObjectQuantity quantity = object->quantity;
  const char* const subdesc = object_get_subdesc(object);
  if (object->flags & ofKnownMaterial)
  { const tObjectKind kind = object->kind;
    const tRandomByte material = ring_kind2gem[kind];
    const char* const materialstr = strGem[material];
    if (quantity == 1)
    { sprintf(dest, "%s %s ring%s", indefinite_article(materialstr[0]),
        materialstr, subdesc);
    }
    else sprintf(dest, "%d %s rings%s", quantity, materialstr, subdesc);
  }
  else
  { if (quantity == 1) sprintf(dest, "a ring%s", subdesc);
    else sprintf(dest, "%d rings%s", quantity, subdesc);
  }
}

typedef struct
{ const char* name;
  tObjectDataFiller filler; /* fills object structures with detailed data */
  tObjectGetDesc getdesc; /* produces description texts for objects */
  tObjectFlags flags;/*(if you change these, change object descriptions too!)*/
  tRelativeAmount firstrel, lastrel;
    /* relative amount of this class WRT all classes; firstrel for storey 1,
       lastrel for dude.sn_hardmax */
  tObjectKind maxkind;
  tBasicObjectMaterials bom;
} tOcd; /* object class data */

static const tOcd objectclassdata[maxObjectClass + 1] =
{ { "amulet", object_create_amulet, object_getdesc_amulet, ofNone,
    0, 0, maxAmuletKind, bomSpecial },
  { strGold, object_create_gold, object_getdesc_gold, ofFullyIdentified,
    5000, 10000, maxGoldKind, bomMetalS },
  { "tool", object_create_tool, object_getdesc_tool, ofNone,
    0, 3000, maxToolKind, bomSpecial },
  { strStone, object_create_stone, object_getdesc_stone, ofFullyIdentified,
    3000, 4000, maxStoneKind, bomStone },
  { strFood, object_create_food, object_getdesc_food, ofKindIdentified,
    100, 500, maxFoodKind, bomOther },
  { "armor", object_create_armor, object_getdesc_armor, ofKindIdentified,
    2000, 3000, maxArmorKind, bomSpecial },
  { "weapon", object_create_weapon, object_getdesc_weapon, ofKindIdentified,
    5000, 10000, maxWeaponKind, bomSpecial },
  { "potion", object_create_potion, object_getdesc_potion, ofNone,
    1500, 3000, maxPotionKind, bomLiquid | bomGlass },
  { strWand, object_create_wandstaff, object_getdesc_wandstaff, ofNone,
    1500, 4000, maxWandStaffKind, bomSpecial },
  { "scroll", object_create_scroll, object_getdesc_scroll, ofNone,
    2000, 5000, maxScrollKind, bomPaper },
  { strRing, object_create_ring, object_getdesc_ring, ofNone,
    500, 2000, maxRingKind, bomMetalN | bomOther }
};

tObject* object_create(const tObjectClass oc, const tObjectKind kind,
  const unsigned char flags0)
/* creates an object of the given class and kind (if kind == okNone: of a
   random kind); "flags0 & 1": try to put object into section; "&2": must not
   be cursed */
{ const tOcd* const ocd = &(objectclassdata[oc]);
  tObject* object = object_allocate();
  object->oc = oc;
  object->kind = kind; /* may be okNone - we care later (if filler doesn't) */
  object->flags = ocd->flags; (ocd->filler)(object); /* setup the details */

  /* Set some defaults if the filler didn't care about the values: */
  if (object->kind == okNone) object->kind = randbytemod(ocd->maxkind + 1);
  if (object->quantity <= 0) object->quantity = 1;
  if (object->lifetime <= 0)
  { if (difficulty < dkCrazy) object->lifetime = MAX_FEWTICKS;
    else object->lifetime = 2000 + randfewticksfromrange(0, 200);
  }

  /* finish: */
  if ( (flags0 & 2) && (object->enchantment < 0) ) object->enchantment = 0;
  if (flags0 & 1)
  { if (!section_randput_object(object))
    { object_deallocate(object); object = NULL; goto out; }
    /* IMPLEMENTME: if a suitable container, put some more or less random
       objects into it! If the container is cursed, the objects should be
       mostly cursed too; ensure that contained items in shops cost something:
       ->price_tag = 1. */
  }
  out: return(object);
}

tObject* object_randcreate(const unsigned char flags0)
/* creates an object of a random class (except Amulet class, of course:-) */
{ tObjectClass oc;
  if (foodcount <= current_storeynum / 2) oc = ocFood;
  else /* use a random class */
  { tRelativeAmount a = randlongmod(allclasses_amount), sum = 0;
    tObjectClass i = 1; /* IMPLEMENTME: 0 when amulets are implemented! */
    while (i <= maxObjectClass)
    { sum += roca[i];
      if (sum > a) break;
      i++;
    }
    if (i <= maxObjectClass) oc = i;
    else /* something's horribly wrong... */
    { oc = randbytefromrange(1, maxObjectClass);
#if CONFIG_DEBUG
      message_showdebug("BUG: object_randcreate()");
#endif
    }
  }
  return(object_create(oc, okNone, flags0));
}

tObject* object_tool_quarks2object(tQuarkValue value)
/* creates an object from the quark value; the association between quark values
   and object classes/kinds will obviously change when new object classes/kinds
   are implemented - that's intentional, neither players nor developers shall
   be bored; this is a dungeon _exploration_ adventure game! :-) */
{ tObject* retval = NULL;
  tObjectClass classcount;
  unsigned char loopcount = 5; /* (to definitely avoid inifite loops) */
  loop:
  if (value < 0) goto out; /* "can't happen" */
  for (classcount = 0; classcount <= maxObjectClass; classcount++)
  { tObjectKind maxkind = objectclassdata[classcount].maxkind;
    if (value <= maxkind)
    { tObjectClass oc = classcount;
      tObjectKind kind = (tObjectKind) value;
      if ( (oc == ocAmulet) ||
           ( (oc == ocTool) && (tooldata[kind].flags & 16) ) )
      { /* would make the game too easy */
        oc = (randcointoss() ? ocGold : ocFood); kind = okNone;
      }
      retval = object_create(oc, kind, 2);
      goto out; /* done */
    }
    value = value - maxkind - 1;
  }
  if (--loopcount > 0) goto loop; /* the original <value> seems to be high */
  out: return(retval);
}

#define _STRCAT(_s) \
  do \
  { const char* s = _s; \
    char ch; \
    while ( (ch = *s++) != '\0' ) *str++ = ch; \
  } while (0)

#define STRCAT(s) \
  do \
  { if (is_first) { is_first = falsE; _STRCAT(openbrace); } \
    else { _STRCAT(semicolon); } \
    _STRCAT(s); \
  } while (0)

static const char* object_get_subdesc(const tObject* object)
/* returns a pointer to the "sub-description" of an object (mostly class-
   independent information), e.g. possible information about enchantment
   (depending on player character's knowledge), whether the player character
   currently wields the object (if it's a weapon) etc. */
{ static const char openbrace[] = " (", semicolon[] = "; ";
  static char subdesc[100]; /* 100 "should be enough" */
  const tObjectClass oc = object->oc;
  const tObjectKind kind = object->kind;
  const tObjectFlags flags = object->flags;
  const tPropertyExtent ench = object->enchantment;
  const tDamage damage = object->damage;
  tPriceTag price;
  tBoolean is_first = truE;
  const char* txt;
  char temp[50], temp2[50], *str = subdesc;

  if (flags & ofKnownMaterial)
  { if (oc == ocPotion)
    { if (potionkind_id[kind]) STRCAT(potiondata[kind].name); }
    else if (oc == ocWandStaff)
    { if (wandstaffkind_id[kind]) STRCAT(wandstaffdata[kind].name); }
    else if (oc == ocScroll)
    { if (scrollkind_id[kind]) STRCAT(scrolldata[kind].name); }
    else if (oc == ocRing)
    { if (ringkind_id[kind]) STRCAT(ringdata[kind].name); }
    else if (oc == ocAmulet) { /* IMPLEMENTME! */ }
  }

  if ( (oc == ocTool) && (toolkind_id[kind]) &&
       (object_toolkind2class(kind) != tocNone) )
  { STRCAT(tooldata[kind].name); }

  if (flags & ofEnchIdentified)
  { if (flags & ofEnchExtIdentified)
    { sprintf(temp, strPerccPercd, ( (ench < 0) ? ('-') : ('+') ), ABS(ench));
      STRCAT(temp);
    }
    else if (ench != 0) STRCAT( (ench < 0) ? ("curs") : ("ench") );
    else STRCAT("0");
  }
  else if (flags & ofEnchExtIdentified) /* extent known, but not sign */
  { sprintf(temp, strPercd, ABS(ench));
    STRCAT(temp);
  }

  if ( (damage != 0) && (flags & ofDamageIdentified) )
  { sprintf(temp, "d=%d", damage);
    STRCAT(temp);
  }

  if ( ( (oc == ocWandStaff) || (oc == ocScroll) ) &&
       (flags & ofChargeIdentified) )
  { sprintf(temp, "c=%d", object->lifetime / 1000);
    STRCAT(temp);
  }

  if (flags & ofBeingWorn) STRCAT("being worn");
  else if (flags & ofBeingWielded) STRCAT("in hand");
  else if (flags & ofBeingApplied)
  { txt = "being applied"; /* default; "should not happen" */
    if (oc == ocTool)
    { const tToolClass toc = object_toolkind2class(kind);
      if (toc != tocNone) txt = toolclassdata[toc].applytext;
    }
    STRCAT(txt);
  }
  else if (flags & ofOnLeftHand) STRCAT("on left hand");
  else if (flags & ofOnRightHand) STRCAT("on right hand");
  if (flags & ofBeingLit)
  { if ( (oc == ocTool) && (kind == tokLantern) ) STRCAT("being lit");
    else STRCAT("on fire");
  }

  if (flags & ofKnownMaterial)
  { if (oc == ocPotion)
    { if (potioncall[kind] != NULL) STRCAT(potioncall[kind]); }
    else if (oc == ocWandStaff)
    { if (wandstaffcall[kind] != NULL) STRCAT(wandstaffcall[kind]); }
    else if (oc == ocScroll)
    { if (scrollcall[kind] != NULL) STRCAT(scrollcall[kind]); }
    else if (oc == ocRing)
    { if (ringcall[kind] != NULL) STRCAT(ringcall[kind]); }
  }

  if ( (price = object->price_tag) != 0 )
  { const char* const offer = ( (price < 0) ? " offer" : strEmpty );
    if ( (price == 1) || (!(object->flags & ofKnownPrice)) ) txt = "unknown";
    else { sprintf(temp2, strPercd, ABS(price)); txt = temp2; }
    sprintf(temp, "price%s: %s", offer, txt); STRCAT(temp);
  }

#if CONFIG_DEBUG
  sprintf(temp, "lt=%d,ow=%d,nw=%d", object->lifetime,
    object_oldweight(object), object_weight(object));
  STRCAT(temp);
#endif

  if (!is_first) _STRCAT(")");
  *str = '\0';
  return(subdesc);
  /* (We currently always return <subdesc>, but the callers should not have to
      know this; we might wanna change the implementation later for some
      reason; that's why this function isn't "static _void_"...) */
}

#undef _STRCAT
#undef STRCAT

tBoolean object_randlocatearound(tObject* object, tCoordinate x, tCoordinate y)
/* tries to put the object somewhere near (x, y); returns whether that was
   possible */
{ tBoolean retval;
  tCoordinate x2, y2;
  if (section_randlocatearound(x, y, sfObject | sfSeconn, &x2, &y2))
  { /* IMPLEMENTME: it's not necessary to avoid sfSeconn squares generally,
       only _downward_ connectors! */
    section_put_object(object, x2, y2, 0); retval = truE;
  }
  else retval = falsE;
  return(retval);
}

tObject* object_split(tObject* object, tObjectQuantity new_quantity)
/* "splits" the things of the object by putting <new_quantity> things into a
   new object structure which is returned */
{ tObject* retval = object_allocate();
#if CONFIG_DEBUG
  if (new_quantity < 1)
  { new_quantity = 1;  msg: message_show("BUG: object_split()"); }
  else if (new_quantity >= object->quantity)
  { new_quantity = object->quantity - 1; goto msg; }
#endif
  (*retval) = (*object);
  object->quantity -= new_quantity;
  retval->quantity = new_quantity;
  return(retval);
}

#define SETCALL(array) \
  do \
  { if (array[kind]) memory_deallocate(array[kind]); \
    array[kind] = s; \
  } while (0)

void object_call(tObjectClass oc, tObjectKind kind, char* s)
{
#if CONFIG_DEBUG
  if (oc > maxObjectClass)
  { sprintf(debugstrbuf, "BUG: object_call(class1 %d", oc);
    message_showdebug(debugstrbuf); return;
  }
  if (kind > objectclassdata[oc].maxkind)
  { sprintf(debugstrbuf, "BUG: object_call(class2 %d, kind %d)", oc, kind);
    message_showdebug(debugstrbuf); return;
  }
#endif

  if (oc == ocPotion) SETCALL(potioncall);
  else if (oc == ocWandStaff) SETCALL(wandstaffcall);
  else if (oc == ocScroll) SETCALL(scrollcall);
  else if (oc == ocRing) SETCALL(ringcall);
#if CONFIG_DEBUG
  else
  { sprintf(debugstrbuf, "BUG: object_call(class3 %d)", oc);
    message_showdebug(debugstrbuf);
  }
#endif
  /* IMPLEMENTME: ocAmulet! */
}

#undef SETCALL

void object_age_basic(tObject* object, const tTicks ticks)
/* performs a "basic aging" of the object; IMPORTANT: the _caller_ must handle
   aaLie and aaCarry as appropriate and afterwards check the object with
   object_still_exists() etc.! */
{ const tObjectFlags flags = object->flags;
  tFewTicks lifetime;
  if (!(flags & (ofInUse | ofRusts | ofWormy | ofBeingLit))) /* very likely */
    goto done_with_this;
  lifetime = object->lifetime;
#define OAB(aa) lifetime = LIMSUB(lifetime, ticks * (aa))
  if (flags & ofBeingWorn) OAB(aaWear);
  if (flags & ofBeingWielded) OAB(aaWield);
  if (flags & ofBeingApplied) OAB(aaBeingApplied);
  if (flags & ofOnEitherHand) OAB(aaOnHand);
  if (flags & ofRusts) OAB(aaRusts);
  if (flags & ofWormy) OAB(aaWormy);
  if (flags & ofBeingLit)
  { const tObjectClass oc = object->oc;
    const tObjectKind kind = object->kind;
    if ( (oc == ocTool) && (kind == tokLantern) ) OAB(aaLitLantern);
    else if (oc == ocWeapon) OAB(aaLitWeapon);
#if CONFIG_DEBUG
    else /* "should not happen" */
    { sprintf(debugstrbuf, "BUG: object_age_basic(%d,%d)", oc, kind);
      message_showdebug(debugstrbuf);
    }
#endif
  }
#undef OAB
  object->lifetime = lifetime;
  done_with_this: {}
  /* FIXME: age any contained objects (object->contents); this aging should
     depend on the kind of container, e.g. slower in an ice box (but faster in
     a _cursed_ ice box)! */
}

void object_handle_sectionchange(void)
{ const tStoreyNumber storey = current_storeynum;
  tObjectClass i;

  /* Re-calculate the relative amounts for all object classes; IMPLEMENTME:
     should we take player character experience, difficulty etc. into account?
     IMPLEMENTME: different amounts in different department kinds! */
  allclasses_amount = 0;
  for (i = 1; i <= maxObjectClass; i++)
  { const tOcd* const ocd = &(objectclassdata[i]);
    tRelativeAmount firstrel = ocd->firstrel, lastrel = ocd->lastrel,
      a = firstrel + (((lastrel - firstrel) * storey) / dude.sn_hardmax);
    if (oc_in_classes(i, ocbCombat)) a += (pref_combat - defpref_combat) * 100;
    else if (oc_in_classes(i, ocbMagic))
    { /* IMPLEMENTME: if pref_feat <= 0 or storey < 3, set ocAmulet-related
         value to 0 (and in any case let this loop start at 0, not 1)! */
      a += (pref_magic - defpref_magic) * 100;
      if ( (i == ocScroll) && (pref_role == roleCaveman) )
      { /* A caveman cannot read, so putting many scrolls into the game could
           be nonsense. We still create a few for reasons like the following:
           1. Other creatures can pick them up. 2. They may be useful even if
           unreadable. 3. The player shall not think that scrolls are absent
           due to a bug. 4. The player shall get those funny "cavemen can't
           read" messages. :-) */
        a /= 5;
      }
    }
    else if (i == ocGold)
    { if (current_dd->kind == ddkMine)
      { /* IMPLEMENTME: lots of metal pieces, e.g. "a <<= 1;"! */
      }
    }
    roca[i] = a; allclasses_amount += a;
  }

  if (!(current_section->flags & secfDidObjs)) /* create objects for new sec */
  { tUint16 maxcount = (3 * current_section->num_rooms) / 2,
      count = 3 + sectionsizemeasure / 2500 + randbytemod(storey / 5);
    if (count > maxcount) count = maxcount;
    else if (count < maxcount)
    { const tPercentage percentage = 35 + storey / 2 - 2 * difficulty;
      while ( (count < maxcount) && (randpercent(percentage)) ) count++;
    }
    while (count-- > 0) (void) object_randcreate(1);
    current_section->flags |= secfDidObjs;
  }
}

void object_put_on_list(tObject** list, tObject* object)
{ object->next = *list; *list = object; }

void object_remove_from_list(tObject** list, tObject* object)
{ remove_from_list(list, object, tObject); object->next = NULL; }

tObject* location2object(const tCoordinate x, const tCoordinate y)
{ tObject* o;
  for_each_currentsectionobject(o)
  { if ( (o->x == x) && (o->y == y) ) return(o); }
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "BUG: location2object(%d,%d)", x, y);
  message_showdebug(debugstrbuf);
#endif
  return(NULL);
}

my_inline char object_get_classchar(const tObjectClass oc)
{ return(strObjectClassChars[oc]); }

const char* object_get_classname(const tObjectClass oc)
{ return(objectclassdata[oc].name); }

char object_get_char(const tObject* object)
{ return(object_get_classchar(object->oc)); /* (_currently_!) */ }

tCreatureData object_get_randimi(void)
/* for "cfImitates" creatures */
{ enum { toolnum = 3, weaponnum = 5 };
  static const tToolKind tool[toolnum] = { tokCase, tokLantern, tokBox };
    /* IMPLEMENTME: tokStatue! */
  static const tWeaponKind weapon[weaponnum] = { wkSpear, wkHoe, wkClub,
    wkMace, wkSword };
  const tObjectClass oc = randbytemod(maxObjectClass) + 1;
    /* (We shall never return a value representing the Amulet of Yendor.) */
  tObjectKind kind;
  switch (oc) /* provide various kinds for some classes */
  { case ocTool: kind = tool[randbytemod(toolnum)]; break;
    case ocStone: kind = stkGray; break;
    case ocArmor: kind = randbytemod(maxArmorKind + 1); break;
    case ocWeapon: kind = weapon[randbytemod(weaponnum)]; break;
    default: kind = 0; break;
  }
  return(IMI_BUILD(oc, kind));
}

tColorBitmask object_get_charcolor(const char ch)
{ return( (ch == strObjectClassChars[ocAmulet]) ? cbEnchanted : cbObject ); }

const char* object_classes2chars(const tObjectClassesBitmask classes)
/* converts the bitmask to a string of the corresponding characters */
{ static char __classescharstr[maxObjectClass + 2];
  char* temp = __classescharstr;
  tObjectClassesBitmask mask = 1;
  tObjectClass oc;
  for (oc = 0; oc <= maxObjectClass; oc++)
  { if (classes & mask) *temp++ = object_get_classchar(oc);
    mask <<= 1;
  }
  *temp = '\0';
  return(__classescharstr);
}

void object_get_description(const tObject* object, char* dest)
{ /* The whole work is done by the class-specific handler: */
  (objectclassdata[object->oc].getdesc)(object, dest);
}

tBoolean oc_pluralable(const tObjectClass oc)
{ return(cond2boolean( (oc != ocGold) && (oc != ocFood) && (oc != ocArmor) ));
}

tBoolean _object_get_minidesc(const tObjectClass oc, tObjectKind kind,
  const tObjectQuantity quantity, /*@out@*/ char* dest,
  const unsigned char flags0, const char* intro)
/* produces some "minimal" description text; returns whether a verb should use
   plural form; "flags0 & 1": definite article; "& 2": add the word "cursed" */
{ tBoolean is_countable = oc_pluralable(oc), use_plural, append_s;
  const char *article, *name;
  use_plural = cond2boolean( (is_countable) && (quantity != 1) );
  append_s = use_plural; /* default */
  if (oc == ocTool)
  { const tToolClass toc = object_toolkind2class(kind);
    name = ((toc == tocNone) ? tooldata[kind].name : toolclassdata[toc].name);
    use_plural = cond2boolean(tooldata[kind].flags & 1);
    append_s = falsE; goto finish;
  }
  else if (oc == ocWeapon)
  { /* use a weapon name, but somewhat generalized */
    if (kind == wkPoisonDart) kind = wkDart;
    else if ( (kind == wkCrossbowBolt) || (kind == wkFireBolt) )
    { name = strBolt; goto finish; }
    else if ( (kind == wkLongSword) || (kind == wkTwoHandedSword) )
      kind = wkSword;
    name = weapondata[kind].name; goto finish;
  }
  else if (oc == ocWandStaff)
  { const tRandomByte material = wandstaff_kind2material[kind],
      matclass = material / __numWandStaffMat;
    if (matclass == 1)
    { name = (use_plural ? strStaves : strStaff); append_s = falsE;
      goto finish;
    }
    else if (matclass == 2) { name = strRod; goto finish; }
  }
  name = objectclassdata[oc].name; /* default */
  finish:
  if ( (oc == ocTool) && (kind == tokNoTea) )
    article = strEmpty; /* grammar hack */
  else if (flags0 & 1) article = strThe;
  else if (is_countable && (!use_plural)) article=indefinite_article(name[0]);
  else article = strEmpty;
  sprintf(dest, "%s%s%s%s%s%s", null2empty(intro), article,
    ((article != strEmpty) ? strSpace : strEmpty), ( (flags0 & 2) ? "cursed " :
    strEmpty ), name, (append_s ? strS : strEmpty));
  return(use_plural);
}

unsigned char object_importance(const tObject* object)
{ unsigned char retval = 0;
  if ( (object->oc == ocAmulet) && (!AMULET_FAKE(object->objdetail)) )
  { retval++;
    if (object->kind == amkYendor) retval++;
  }
  return(retval);
}

#if CONFIG_DEBUG
static tObjectWeight object_oldweight(const tObject* object)
/* This is the old, rough weight calculation algorithm. The only reason why
   it's still there is occasional comparison. Simply ignore this. */
{ const tObjectClass oc = object->oc;
  tObjectQuantity q = object->quantity;
  tObjectWeight weight;

  if (oc == ocGold) weight = q/1000;
  else if (oc == ocStone) weight = q/5;
  else if (oc == ocArmor) weight = 1*q;
  else if (oc == ocWeapon)
  { tObjectKind kind = object->kind;
    if ( (kind == wkDart) || (kind == wkPoisonDart) || (kind == wkShuriken) ||
         (kind == wkArrow) || (kind == wkCrossbowBolt) || (kind == wkFireBolt))
    { weight = q/10; }
    else if ( (kind == wkLongSword) || (kind == wkTwoHandedSword) )
    { weight = 2*q; }
    else weight = 1*q;
  }
  else weight = 1*q;
  if (weight < 1) weight = 1; /* CHECKME! */
  return(weight);
}
#endif

tObjectWeight object_weight(const tObject* object)
/* calculates the "weight" of an object; don't try to apply too much real-world
   logic to this... For example, did you ever wonder why/how one can carry
   thousands of gold nuggets in the original Rogue game? */
{ tObjectWeight weight;
  const tObjectClass oc = object->oc;
  const tObjectKind kind = object->kind;
  const tObjectQuantity q = object->quantity;
  tObject* o;
  switch (oc)
  { case ocAmulet: weight = 20; break;
    case ocGold: weight = q / 10; goto no_mul; /*@notreached@*/ break;
    case ocTool:
      weight = tooldata[kind].weight;
      if (kind == tokBox)
      { if (box_is_large(object->objdetail)) weight = 2000; }
      else if (kind == tokStatue)
      { const tObjectDetail detail = object->objdetail;
        weight += 10 * _statue_material(detail) +
          5 * _creature_size(statue_cc(detail));
      }
      break;
    case ocStone: weight = 12 * (kind + 1); break;
    case ocFood:
      weight = 3;
      if (kind == fkCorpse) weight += (1 << _creature_size(object->objdetail));
      break;
    case ocArmor: weight = 15 * (kind + 1); break;
    case ocWeapon: weight = weapondata[kind].weight; break;
    case ocPotion: weight = 4; break;
    case ocWandStaff: /* wand: 6; staff: 4; rod: 2 */
      weight = 6 - 2 * (wandstaff_kind2material[kind] / __numWandStaffMat);
      break;
    case ocScroll: weight = 1; break; /* our "basic weight unit" */
    case ocRing: weight = 5; break;
    default: weight = 1; break; /* "should not happen" */
  }
  weight *= q; no_mul: {} o = object->contents;
  while (o != NULL) { weight += object_weight(o); o = o->next; }
  if (weight < 1) weight = 1; /* e.g. for ocGold */
  return(weight);
}

tHoeValue object_hoevalue(/*@null@*/ const tObject* object,
  const tBasicObjectMaterials bom, const unsigned char flags0)
/* "flags0 & 1": immediate action (wielded weapon); "flags0 & 2": zapping */
{ tHoeValue retval = 0, x;
  if (object != NULL)
  { const tObjectClass oc = object->oc;
    const tObjectKind kind = object->kind;
    const tHoeValue extra = cond2bool(flags0 & 1);
    /* IMPLEMENTME: distinguish in more places which material is hoed, e.g.
       hard rock or a wooden/metal door! */
    if (oc == ocAmulet) retval = 6 + extra; /* CHECKME! */
    else if (oc == ocStone)
    { if (object->quantity > 3 * (maxStoneKind - kind + 1)) retval = 2; }
    else if (oc == ocWeapon)
    { switch (kind)
      { case wkHoe: retval = 7 + 2 * extra; break;
        case wkTwoHandedSword: retval = 5 + extra; break;
        case wkLongSword: retval = 4; break;
        case wkPolearm: retval = 3 + extra;
          /* IMPLEMENTME: special effects, depending on polearm_tipobj()! */
          break;
        case wkDagger: case wkSpear: retval = 1 + extra; break;
        case wkClub: case wkMace:
          if (bom & (bomWood | bomGlass | bomTextile | bomBone))
            retval = 2 + extra;
          else retval = 1;
          break;
      }
    }
    else if (oc == ocWandStaff)
    { if ( (flags0 & 2) &&
           ( (x = wandstaff_can_affect_bom(kind, bom, NULL)) > 0 ) )
      { retval = x; }
      else retval = 1;
    }
    if ( (retval < 10) && (randpercent(10 * retval - 7 * difficulty)) )
    { retval++;
      if (__randpercent(50 - 9 * difficulty)) retval++;
    }
  }
  return(retval);
}

tBasicObjectMaterials object_materials(const tObject* object)
{ static const tBasicObjectMaterials m[__numWandStaffMatClasses] =
  { bomMetalN, bomWood, bomGlass };
  const tObjectClass oc = object->oc;
  tBasicObjectMaterials retval = objectclassdata[oc].bom;
  const tWeaponData* wd;
  tObjectKind kind;
  if (retval != bomSpecial) goto out; /* the "trivial" cases */
  kind = object->kind;
  switch (oc)
  { case ocAmulet:
      retval = ( (kind == amkYendor) ? bomMetalS : bomMetalN ); break;
    case ocTool:
      retval = tool_mainmat(object) | tooldata[kind].furthermat; break;
    case ocArmor:
      retval = ( (ARMOR_IS_LEATHER(kind)) ? bomLeather: bomMetalN ); break;
    case ocWeapon:
      wd = &(weapondata[kind]); retval = wd->mainmat | wd->furthermat; break;
    case ocWandStaff:
      retval = m[wandstaff_kind2material[kind] / __numWandStaffMat]; break;
    default: /* "should not happen" */
#if CONFIG_DEBUG
      sprintf(debugstrbuf, "BUG: object_materials(%d)", oc);
      message_showdebug(debugstrbuf);
#endif
      retval = bomOther; break;
  }
  out: return(retval);
}

tBasicObjectMaterials object_mainmat(const tObject* object)
/* returns the "main" materials of the object */
{ tBasicObjectMaterials retval;
  switch (object->oc)
  { case ocTool: retval = tool_mainmat(object); break;
    case ocWeapon: retval = weapondata[object->kind].mainmat; break;
    default: retval = object_materials(object); break;
  }
  return(retval);
}

tBoolean object_is_enchantable(const tObject* object)
{ if ( (object->oc == ocWeapon) && (weapondata[object->kind].maxquant > 1) )
    return(falsE); /* special case; for do_cast_spell(), skEnchantWeapon */
  return(truE);
}

tObject* object_ttpt(tObject* o, tObject* object, const tBoolean rogue_related)
/* mostly a helper function for object_try_to_put_together() */
{ const tObjectClass oc = o->oc;
  if ( (oc != object->oc) || (o->kind != object->kind) || (o->price_tag != 0) )
  { /* We must _never_ put objects of different classes, ... together: */
    goto out;
  }
  if (oc == ocAmulet)
  { enum { f = ofBeingCarried | ofFullyIdentified | ofKnownMaterial |
      ofKnownPrice };
    unsigned char n1, d1, n2, d2;
    if ( (o->flags & (~f)) != (object->flags & (~f)) ) goto out;
    if ( (o->objdetail & _AMULET_FAKE) != (object->objdetail & _AMULET_FAKE) )
      goto out;
    AMULET_ND(o->objdetail, n1, d1); AMULET_ND(object->objdetail, n2, d2);
    if ( (d1 != d2) || (d1 <= 1) ) /* differently fragmented || unfragmented */
      goto out;
  }
  else if ( (o->damage != object->damage) ||
    ((o->flags & (~ofBeingCarried)) != (object->flags & (~ofBeingCarried))) ||
    (o->enchantment != object->enchantment) )
  { goto out; }
  if ( (oc == ocTool) || (oc == ocArmor) || (oc == ocWandStaff) ||
       (oc == ocScroll) || (oc == ocRing) )
  { /* We must not put objects of such a class together - or we'd have to
       change the algorithms for applying tools, wearing armor, zapping,
       scroll-reading resp. puton-ring! */
    goto out;
  }
  if ( (oc == ocFood) && (o->kind == fkCorpse) &&
    (o->objdetail != object->objdetail) )
  { goto out; } /* different kinds of corpses */
  if (oc == ocAmulet)
  { const tFewTicks lt = object->lifetime;
    tObjectDetail detail = o->objdetail;
    unsigned char n1, d1, n2, d2;
    o->flags |= (object->flags & ofFullyIdentified);
    if (o->lifetime < lt) o->lifetime = lt; /* be generous */
    AMULET_ND(detail, n1, d1); AMULET_ND(object->objdetail, n2, d2);
    o->damage = ((n1 * o->damage) + (n2 * object->damage)) / (n1 + n2);
    o->enchantment = ((n1*o->enchantment) + (n2*object->enchantment))/(n1+n2);
    n1 += n2;
    if (n1 > d1) n1 = d1; /* never more than one entire amulet */
    detail = (detail & (~_AMULET_NMASK)) | (n1 << _AMULET_NSHIFT);
    o->objdetail = detail;
    return(o);
  }
  else if ( (oc == ocGold) || (oc == ocStone) || (oc == ocFood) ||
       ( (oc == ocWeapon) && (weapondata[o->kind].maxquant > 1) ) ||
       (oc == ocPotion) )
  { tObjectQuantity quantity1 = o->quantity, quantity2 = object->quantity,
       total_quantity;
    tTicks lifetime; /* not "tFewTicks", that could overflow here! */
    if (quantity1 > MAX_OBJECT_QUANTITY - quantity2) /* would overflow */
      goto out;
    total_quantity = quantity1 + quantity2;
    if (total_quantity < 1)
    { /* "can't happen"; this check is here just to _absolutely_guarantee_
         that we don't divide by 0 below */
      goto out;
    }
    if (object_weight(o) >= HIGHEST_ALLOWED_NUMBER / 2 - object_weight(object))
    { /* make absolutely sure that weight calculations can't overflow; that's
         probably only paranoia and a purely theoretical problem, but... */
      goto out;
    }

    if (rogue_related) roguedata.packweight -= object_weight(o);

    /* calculate the quantity */
    o->quantity += object->quantity;

    /* magically intermix the lifetimes */
    lifetime =
      (o->lifetime*quantity1 + object->lifetime*quantity2) / total_quantity;
    if (lifetime < 10) lifetime = 10; /* just be nice to the player :-) */
    else if (lifetime > MAX_FEWTICKS) lifetime = MAX_FEWTICKS;
    o->lifetime = (tFewTicks) lifetime;

    if (rogue_related) roguedata.packweight += object_weight(o);
    return(o);
  }
  out: return(NULL);
}

tObject* object_try_to_put_together(tObject* list, tObject* object,
  const tBoolean rogue_related)
/* tries to "put together" the given object with one of the objects on the
   list; returns a pointer to the "combined" object if successful (IMPORTANT:
   then the _caller_ must free/... the <object> from the parameter list!),
   otherwise returns NULL; use this only if any creature is about to pick or
   drop (the latter possibly as a consequence of throwing!) an object; for
   other cases it shouldn't be necessary at all; if it _is_ necessary for other
   cases, check whether the "& (~ofBeingCarried)" logic is still okay. */
{ tObject* o;
  if (object->price_tag != 0) goto nah;
  for_each_object(list, o)
  { tObject* const oy = object_ttpt(o, object, rogue_related);
    if (oy != NULL) return(oy);
    /* "else": try to put together with next list element */
  }
  /* Could not put objects together... */
  nah:
  if (rogue_related) roguedata.packweight += object_weight(object);
  return(NULL);
}

tLightIntensity object_light(const tObject* object,
  /*@null@*/ /*@out@*/ tUint8* _radius)
{ tLightIntensity retval = 0;
  tUint8 radius SHUT_UP_COMPILER(0);
  switch (object->oc)
  { case ocTool:
      switch (object->kind)
      { case tokLantern:
          if (object->flags & ofBeingLit)
          { retval = 255; radius = lit_value(object->lifetime); goto finish; }
          break;
        case tokPhotograph:
          { const tObjectDetail detail = object->objdetail;
            if ( (photo_kind(detail) == phkObject) && 
              (is_photobjcode(detail, ocTool, tokLantern)) )
            { retval = 255; radius = 3; goto finish; }
          }
          break;
      }
      break;
  }
  if (object->flags & ofBeingLit) { retval = 128; radius = 1; }
  finish: if (_radius != NULL) *_radius = radius; /* only valid if retval>0 */
  return(retval);
}

void object_light_off(tObject* object)
{ object->flags &= ~ofBeingLit; mrm_light = truE; }

void object_light_on(tObject* object)
{ object->flags |= ofBeingLit; mrm_light = truE; }

static void maybe_start_rusting(tObject* object,
  const tPercentage p, /*@null@*/ const char* category)
{ if ( (!(object->flags & (ofRusts | ofProtected))) &&
    (object_materials(object) & bomMetalN) && (randpercent(p)) )
  { if ( (rogue_can_see) && (category != NULL) )
    { char* ptr = strbuf;
      if (category == strEmpty) /* a slight hack, for simplicity... */
      { *ptr++ = '('; *ptr++ = invobj2char(object); *ptr++ = ')'; *ptr++ = ' ';
        object_get_description(object, ptr);
        while (*ptr) ptr++; /* go to end of string */
      }
      else ptr += sprintf(ptr, "%s %s", rogue_pronoun(2), category);
      strcpy(ptr, " rusts"); message_show(strbuf);
    }
    object->flags |= ofRusts;
  }
}

void object_gets_wet(tObject* object, const tPercentage p,
  /*@null@*/ const char* category)
{ if ( (object->flags & ofBeingLit) && (randpercent(p)) )
  { object_light_off(object);
    /* IMPLEMENTME: message! */
  }
  maybe_start_rusting(object, p, category);
}

#define CHECK_FIRST(class_str) \
  if (is_first) \
  { is_first = is_first_class = falsE; \
    st_append_nl(&st); st_append_reverse(&st, class_str, falsE); \
  }

#define ODA_APPEND st_append(&st, my_strdup(strbuf), truE);

static tBoolean spellcast_key_handler(tScrollableText* st, const tKey key)
{ return(cond2boolean( (key >= 'a') && (key < MY_POINTER_TO_INT(st->data)) ));
}

tObjectKind object_dump_associations(const unsigned char flags0)
/* prints everything the player already found out about the kinds of magic
   objects - e.g. the associations between potion colors and potion kinds;
   "flags0 & 1": print _everything_ (not only player's knowledge);
   "flags0 & 2": we're called for spell-casting */
{ const tBoolean casting = cond2boolean(flags0 & 2),
    print_all = cond2boolean( (flags0 & 1) && (!casting) );
  tScrollableText st;
  tBoolean is_first, is_first_class = truE;
  tObjectKind castkind = okNone;
  unsigned char castingcount = 0;
  tScrollKind castassoc[26]; /* associates letters with scroll kinds */
  char temp[50];
  tObjectKind kind;

  st_setup(&st);
  st_append(&st, (casting ? "Cast a spell" : "Knowledge about magic objects"),
    falsE);

  is_first = truE;
  for (kind = 0; kind < /*(not "<="!)*/ maxScrollKind; kind++)
  { if ( (print_all) || (scrollkind_id[kind]) )
    { if (casting)
      { const tMagicEnergy cost = object_spellcastingcost(kind);
        if ( (cost == 0) || (cost > roguedata.magic_energy) ) continue;
        sprintf(temp, "(%c, %d) ", ('a' + ((char) castingcount)), cost);
        castassoc[castingcount++] = kind;
      }
      else temp[0] = '\0';
      sprintf(strbuf, "%s%s - %s", temp, scrolldata[kind].name,
        scrolltitle[kind]);
      CHECK_FIRST(casting ? "Known spells" : "Scrolls") ODA_APPEND
    }
  }

  if (casting)
  { if (!is_first) /* can actually cast */
    { st_append(&st, "cast which spell?", falsE);
      st.key_handler = spellcast_key_handler;
      st.data = MY_INT_TO_POINTER((int) ('a' + ((char) castingcount)));
      st.flags |= stfDisableLetterScrolling;
      st.notice = "letter to select spell";
    }
    goto finish;
  }

  is_first = truE;
  for (kind = 0; kind <= maxWandStaffKind; kind++)
  { if ( (print_all) || (wandstaffkind_id[kind]) )
    { static const char* const strMat[__numWandStaffMatClasses] =
      { strMetal, strWood, strGlass };
      tRandomByte material = wandstaff_kind2material[kind],
        matclass = material / __numWandStaffMat;
      sprintf(strbuf, "%s - %s (%s)", wandstaffdata[kind].name,
        object_wandstaff2matstr(material), strMat[matclass]);
      CHECK_FIRST("Wands, Staves & Rods") ODA_APPEND
    }
  }

  is_first = truE;
  for (kind = 0; kind <= maxPotionKind; kind++)
  { if ( (print_all) || (potionkind_id[kind]) )
    { sprintf(strbuf, "%s - %s", potiondata[kind].name,
        strColor[potion_kind2colorcode[kind]]);
      CHECK_FIRST("Potions") ODA_APPEND
    }
  }

  is_first = truE;
  for (kind = 0; kind <= maxRingKind; kind++)
  { if ( (print_all) || (ringkind_id[kind]) )
    { sprintf(strbuf, "%s - %s", ringdata[kind].name,
        strGem[ring_kind2gem[kind]]);
      CHECK_FIRST("Rings") ODA_APPEND
    }
  }

  /* IMPLEMENTME: amulets! */

  finish:
  if (is_first_class)
  { st_append(&st, (casting ? "no spells known yet or not enough magic energy"
      : "no knowledge yet at all"), falsE);
  }
  st_show(&st);
  if (casting)
  { const tKey key = st.key;
    if ( (key >= 'a') && (key < 'a' + castingcount) )
      castkind = castassoc[key - 'a']; /* player wants to cast this spell */
  }
  st_cleanup(&st);
  return(castkind);
}

#undef CHECK_FIRST
#undef ODA_APPEND

#if CONFIG_DEBUG
void object_dump_roca(void)
{ tScrollableText st;
  tRelativeAmount a, ppt, perc, pptsum = 0, percsum = 0;
  tObjectClass i;
  st_setup(&st); st_append(&st, "Relative Object-Class Amounts", falsE);
  st_append_nl(&st);
  for (i = 0; i <= maxObjectClass; i++)
  { a = roca[i]; ppt = (a * 1000) / allclasses_amount; pptsum += ppt;
    perc = ppt / 10; percsum += perc;
    sprintf(debugstrbuf, "class #%d (%s): %d (%d ppt, %d%%)", i,
      objectclassdata[i].name, a, ppt, perc);
    st_append(&st, my_strdup(debugstrbuf), truE);
  }
  sprintf(debugstrbuf, "allclasses_amount: %d; pptsum: %d ppt; percsum: %d%%",
    allclasses_amount, pptsum, percsum);
    /* <percsum> may be less than 100% due to rounding errors! */
  st_append(&st, my_strdup(debugstrbuf), truE); st_show(&st); st_cleanup(&st);
}
#endif

#define maxSyllable (41) /* Arthur Adams meets Douglas Dent! :-) */
static const char* const syllable[maxSyllable + 1] __initdata =
{ "ac", "alz", "bar", "barf", "blech", "bloom", "blorp", "cull", "dpi", "eeh",
  "foo", "flop", "hub", "iki", "ip", "jog", "kont", "mez", "mih", "nho",
  "omph", "orx", "oosh", "pi", "puj", "quor", "raca", "rech", "rep", "rot",
  "slee", "snorf", "swip", "tcp", "tek", "uj", "wee", "xi", "yuci", "zeet",
  "zip", "zom"
};
static const unsigned char syllablen[maxSyllable + 1] __initdata =
{ 2, 3, 3, 4, 5, 5, 5, 4, 3, 3, /*#*/ 3, 4, 3, 3, 2, 3, 4, 3, 3, 3, /*#*/
  4, 3, 4, 2, 3, 4, 4, 4, 3, 3, /*#*/ 4, 5, 4, 3, 3, 2, 3, 2, 4, 4, /*#*/
  3, 3
};
/* Original Rogue documentation mentions that the syllables are from "a dialect
   spoken only by the twenty-seven members of a tribe in Outer Mongolia"; but
   it also says I'm not supposed to know that, so I decided not to know that
   and simply added some more modern ones. :-) */

void __init object_initialize(void)
/* initializes the object handling */
{ tBoolean used[MAX(MAX(maxGem, maxWandStaffMaterial), maxColorCode) + 1];
  tObjectKind kind;

  /* Setup wand/staff materials, ring gems and potion colors */

#define TESTSET(array, kind, material) \
  if (!used[material]) \
  { array[kind] = material; used[material] = truE; break; }

/* the following macro randomly associates object kinds with "materials" */
#define ASSOC(array, maxkind, maxmat) \
  my_memclr_arr(used); \
  for (kind = 0; kind <= maxkind; kind++) \
  { tRandomByte material; \
    unsigned char count = 20; \
    while (--count > 0) \
    { material = randbytemod(maxmat + 1); \
      TESTSET(array, kind, material) \
    } \
    if (!count) /* random assignment failed, do it systematically */ \
    { for (material = 0; material <= maxmat; material++) \
      { TESTSET(array, kind, material) } \
    } \
  }

  ASSOC(wandstaff_kind2material, maxWandStaffKind, maxWandStaffMaterial)
  ASSOC(ring_kind2gem, maxRingKind, maxGem)
  ASSOC(potion_kind2colorcode, maxPotionKind, maxColorCode)

#undef ASSOC
#undef TESTSET

  /* Setup scroll titles */

  for (kind = 0; kind <= maxScrollKind; kind++)
  { const char* temp[4]; /* syllables for one title */
    char* str; /* final title */
    unsigned char len = 0;
    tRandomByte num = randbytefromrange(3, 4), /* #sylls for this title */
      i = 1;
    while (i <= num)
    { const tRandomByte idx = randbytemod(maxSyllable + 1);
      const char* const s = syllable[idx];
      temp[i - 1] = s; len += syllablen[idx];
      if ( (i == 3) && (num == 3) && (len < 7) ) num++;
      i++;
    }
    len += num; /* num == (num - 1) space characters + (1) '\0' byte */
    scrolltitle[kind] = str = __memory_allocate(len);
    if (num == 3) sprintf(str, strThreePercs, temp[0], temp[1], temp[2]);
    else sprintf(str, strFourPercs, temp[0], temp[1], temp[2], temp[3]);
  }

  /* Initialize tool identification */

  for (kind = 0; kind <= maxToolKind; kind++)
    toolkind_id[kind] = cond2boolean(tooldata[kind].flags & 8);

  /* Initialize the user's names for magic items and whether a kind was already
     identified */

  my_memclr_arr(potioncall); my_memclr_arr(potionkind_id);
  my_memclr_arr(wandstaffcall); my_memclr_arr(wandstaffkind_id);
  my_memclr_arr(scrollcall); my_memclr_arr(scrollkind_id);
  my_memclr_arr(ringcall); my_memclr_arr(ringkind_id);

  /* Other stuff */

  roca[0] = 0;
}
