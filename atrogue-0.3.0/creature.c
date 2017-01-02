/* atrogue/creature.c - creature handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "creature.h"
#include "dungeon.h"
#include "message.h"
#include "random.h"
#include "stat.h"
#include "action.h"
#include "animation.h"

/* prototypes */
static tActionFlags creature_zap_creature(tCreature*, tObject*, tCreature*,
  const unsigned char);
static void rogue_incexp(tExperience);
static tPercentage rogue_rings2hitchance(const tPercentage);

static const char strWhee[] = "whee!", strSpaceJust[] = " just",
  strBackToNormalSpeed[] = "back to normal speed";
tCreature* rogue = NULL;

typedef struct
{ const char* name;
  tStoreyPerc minstorp, maxstorp; /* where the creature normally "lives" */
  tCreatureFlags flags;
  tCreatureClassFlags ccf;
  tSpecialHitFlags shf;
  tHealth health; /* initial health; should be >= 4, cf. creature_is_ill() */
  tStrength strength; /* initial strength */
  tCreatureSize size;
  char ch;
} tCcd; /* creature class data; class-specific data about creatures */

static const tCcd creatureclassdata[maxCreatureClass + 1] =
{ { "rogue", 0, 0, cfPro, ccfHumanoid, shfNone, 12, 16, csNormal, '@' },
  /* Creatures of the following classes may be created using a random-class
     generator (creature_randcreate()) */
  { "aquatar", 27, 54, cfFollows | cfDivides | cfSensCreat, ccfNone,
    shfSplashes, 25, 1, csMedium, 'A' /* large drop of water :-) */ },
  { "bat", 1, 24, cfFlies | cfFlitsAny, ccfNone, shfNone, 10, 3, csSmall, 'B'},
  { "centaur", 21, 48, cfNone, ccfNone, shfNone, 32, 18, csNormal, 'C'
    /* half man, half horse */ },
  { "dragon", 63, 100, cfNone, ccfNonAuto, shfFlames, 145, 60, csGiant, 'D' },
  { "emu", 1, 21, cfTamable, ccfNone, shfNone, 11, 7, csMedium, 'E'
    /* big bird with undeveloped wings */ },
  { "venus flytrap", 36, 100, cfStationary | cfPlantlike, ccfNonAuto, shfHolds,
    73, 25, csMedium /* (csSmall would be too harmless) */, 'F' },
  { "griffin", 60, 100, cfFlies | cfTamable, ccfNone, shfNone, 115, 50,
    csMedium, 'G' /* half eagle, half lion */ },
  { "hobgoblin", 1, 30, cfNone, ccfHumanoid, shfNone, 15, 5, csMedium, 'H'
    /* bad elf */ },
  { "ice monster", 6, 33, cfDivides, ccfSleepsDeeply | ccfNonAuto, shfFreezes,
    15, 1, csMedium, 'I' /* little iceberg */ },
  { "jabberwock", 63, 100, cfNone, ccfNone, shfNone, 132, 50, csGiant, 'J' },
  { "kestrel", 1, 18, cfFlies | cfTamable, ccfNone, shfNone, 10, 5, csMedium,
    'K' /* little falcon */ },
  { "leprechaun", 18, 48, cfSeeksGold, ccfSleepsDeeply | ccfHumanoid |
    ccfNonAuto, shfStealsGold, 25, 1, csMedium, 'L' /* a stealing elf */ },
  { "medusa", 54, 100, cfNone, ccfHumanoid, shfConfuses, 97, 37, csNormal,'M'},
  { "nymph", 30, 57, cfNone, ccfHumanoid | ccfNonAuto, shfStealsObject, 25, 1,
    csNormal, 'N' },
  { "orc", 12, 39, cfSeeksGold, ccfHumanoid, shfNone, 25, 11, csNormal, 'O' },
  { "phantom", 45, 72, cfInvisible | cfImmaterial | cfFlitsAny, ccfNone,
    shfNone, 76, 20, csNormal, 'P' },
  { "quagga", 24, 51, cfNone, ccfNone, shfNone, 30, 16, csNormal, 'Q'
    /* extinct ass/zebra */ },
  { "rattlesnake", 9, 36, cfFlitsAtt | cfSupple, ccfNone, shfStings, 15, 10,
    csSmall, 'R' },
  { "snake", 1, 27, cfFlitsAtt | cfSupple, ccfNone, shfNone, 8, 3,csSmall,'S'},
  { "troll", 39, 66, cfNone, ccfHumanoid, shfNone, 75, 28, csMedium, 'T' },
  { "black unicorn", 51, 78, cfFollows, ccfNone, shfNone, 90, 40,csNormal,'U'},
  { "vampire", 57, 100, cfFollows, ccfNone, shfDrainsLife, 55, 18, csMedium,
    'V' },
  { "wraith", 51, 69, cfFlitsAtt | cfImmaterial | cfDivides | cfSensCreat,
    ccfNone, shfDecExp, 45, 16, csMedium, 'W' },
  { "xeroc", 48, 76, cfImitates | cfFollows, ccfNonAuto, shfNone, 42, 24,
    csMedium, 'X' }, /* aka "mimic" */
  { "yeti", 33, 60, cfNone, ccfNone, shfNone, 35, 18, csNormal, 'Y' },
  { "zombie", 15, 42, cfRegenerates /* something like "continuous reanimation"
    */, ccfHumanoid, shfNone, 21, 8, csMedium, 'Z' },

  { "archer", 30, 60, cfNone, ccfHumanoid, shfNone, 10, 4, csMedium, 'A' },
  { "bee", 15, 81, cfFlies | cfFlitsAtt | cfSupple | cfFollows, ccfNone,
    shfStings, 4, 1, csSmall, 'B' },
  { "ironic", 30, 51, cfMetallivore, ccfNone, shfEatsMetal, 15, 10, csNormal,
    'I' }, /* iron, ironic... :-) vaguely cf. "rust monster" */
  { "slime", 30, 60, cfFlitsAtt | cfSupple | cfRegenerates | cfDivides,
    ccfNoIndefArt, shfHolds, 10, 10, csSmall, 'S' },
  { "spider", 21, 42, cfSupple, ccfNone, shfSpins, 5, 1, csSmall, 'S' },
  { "worm", 15, 36, cfSupple | cfRegenerates | cfDivides, ccfNone, shfStings,
    4, 3, csSmall, 'W' },

  /* Most creatures of the following classes are only created under special
     circumstances and only by specific source code - IMPLEMENTME! */
  { "bouncer", 45, 75, cfImmaterial | cfDivides, ccfNone, shfNone, 10, 20,
    csSmall, 'B' },
  { "coxswain", 36, 100, cfNone, ccfHumanoid, shfNone, 15, 15, csNormal, 'C' },
  { "guard", 66, 100, cfNone, ccfHumanoid, shfNone, 20, 30, csNormal, 'G' },
  { "heisenbug", 42, 100, cfFlitsAtt, ccfNone, shfNone, 5, 2, csSmall, 'H' },
  { "hydra", 72, 100, cfRegenerates /* has extra lives and regenerates */,
    ccfNone, shfNone, 50, 10, csMedium, 'H' },
  { "kidnapper", 53, 75, cfNone, ccfNone, shfNone, 30, 20, csNormal, 'K' },
    /* can "kidnap" the player character and move him to another place */
  { "schroedincat", 50, 100, cfSupple | cfImitates | cfFollows, ccfNone,
    shfNone, 10, 7, csSmall, 'S' },
  { "shopkeeper", 1, 75, cfStationary | cfSensCreat, ccfHumanoid | ccfNonAuto,
    shfNone, 50, 50, csNormal, '@' },
  { "shrieker", 60, 78, cfNone, ccfNone, shfNone, 10, 10, csSmall, 'S' },
  { "siren", 48, 75, cfStationary, ccfNone, shfNone, 14, 8, csNormal, 'S' },
  { "trogue", 60, 100, cfPro | cfRegenerates, ccfHumanoid, shfHolds, 40, 40,
    csNormal, 'T' /* troglodyte */ },
  { "varydamage", 51, 100, cfImmaterial | cfRegenerates | cfDivides |
    cfSensCreat, ccfNone, shfNone, 15, 40, csMedium, 'V' },
  { "wizard", 60, 100, cfImmaterial | cfSensCreat, ccfHumanoid, shfZaps, 30,
    10, csNormal, 'W' }
};

const tRoledata _roledata[maxRole + 1] = /* player character roles */
{ /* name              i rs is fs ma  h  l mp   ps  fl */
  { "caveman",      1150, 5, 0, 5, 0, 3, 0, 0, 'c', rdfNone },
  { "knight",       1050, 4, 1, 7, 1, 2, 1, 2, 'k', rdfNone },
  { "proguerammer",  750, 4, 0, 7, 3, 3, 2, 3, 'p', rdfPromotionary },
  { "quantum mech",  300, 3, 0, 7, 3, 3, 3, 3, 'q', rdfPromotionary },
  { "rogue",         950, 3, 1, 6, 2, 2, 2, 1, 'r', rdfNone },
  { "scoundrel",    1100, 1, 3, 2, 0, 0, 0, 1, 's', rdfPromotionary },
  { "thief",         800, 2, 2, 4, 1, 1, 3, 1, 't', rdfNone },
  { "tourist",      1075, 1, 3, 0, 0, 0, 1, 1, 'O', rdfNone },
  { "trader",       1075, 2, 1, 1, 1, 1, 0, 2, 'R', rdfNone },
  { "wizard",        850, 2, 2, 3, 3, 2, 2, 3, 'w', rdfNone }
};

tRoledata roledata;
tRogueData roguedata;
tRogueDataOsi __foreachwornring;

static __my_inline tCreature* creature_allocate(void)
{ return((tCreature*) memory_allocate(sizeof(tCreature))); }

static tBoolean creature_keep_active(const tCreature* creature)
{ tBoolean retval;
  const tCreatureClass cc = creature->cc;
  /* Creatures of the following classes shall always be able to act and thus
     don't sleep/freeze: */
  if ( (cc == ccAquatar) || (cc == ccBouncer) || (cc == ccVarydamage))
    retval = truE;
  /* Stationary creatures shall always be able to act. (Their resistance
     against sleep/freeze can be seen as a magic compensation for their
     immobility.) */
  else if (creature->flags & cfStationary) retval = truE;
  else retval = falsE;
  return(retval);
  /* CHECKME: avatars should freeze? */
}

__my_inline tCreatureSize _creature_size(const tCreatureClass cc)
{ return(creatureclassdata[cc].size); }

void creature_tickout_handler(tTickout* t, const tTickoutAction ta)
{ if (ta == taTrigger) action_creature((tCreature*) (t->data)); }

static void creature_put_into_section(tCreature* creature,
  const tBoolean is_rogue)
{ tTickout* const tickout = &(creature->tickout);
  tickout->data = creature; tickout_set_debuginfo(tickout, "creature");
  tickout->handler = (is_rogue ? player_tickout_handler :
    creature_tickout_handler);
  section_register_creature(creature);
}

static __my_inline void _creature_last_action_set(tCreature* creature,
  const tCreatureActionKind cak, const tCreatureActionSpeed cas)
{ creature->cak = cak; creature->cas = cas; }

tCreature* creature_create_at(const tCreatureClass cc, const tCoordinate x,
  const tCoordinate y, const unsigned char flags0)
/* creates a creature of the given class at the given location in the current
   section (and might give it some objects as appropriate);
   "flags0 & 1": creature must not sleep; "flags0 & 2": must be levitated */
{ tCreature* const creature = creature_allocate();
  const tBoolean creating_original_rogue = cond2boolean((!cc) && (!rogue));
  const tStoreyNumber storey = current_storeynum;
  const tCcd* const ccd = &(creatureclassdata[cc]);
  tCreatureFlags cflags = ccd->flags;
  const tCreatureClassFlags ccf = ccd->ccf;
  tAggressivity aggr;

  creature->cc = cc;
  if (flags0 & 1) cflags &= ~cfImitates; /* must not sleep, so can't imitate */
  creature->flags = cflags; creature->shf = ccd->shf;
  if (difficulty < dkNormal)
  { creature->flags &= ~(cfImmaterial | cfDivides);
    if (difficulty <= dkVeryEasy) creature->flags &= ~cfInvisible;
  }
  else if ( (difficulty >= dkCrazy) && (storey > 5) &&
            (__randpercent(2 * storey)) )
  { creature->flags |= cfInvisible; }

  /* if (creating_original_rogue) { don't haste }
  else */ if ( (storey >= dude.sn_allhasted) || ( (storey > 6 - difficulty) &&
    (randpercent(2 * difficulty + storey / 2 + pref_combat + 1)) ) )
  { creature->slowedhasted = 400*difficulty + randfewticksfromrange(100,200); }
  creature->x = x; creature->y = y;
  creature->health = ccd->health; creature->strength = ccd->strength;
  if (creating_original_rogue) creature->flags |= cfPc;
  else
  { creature->experience = (creature->health + creature->strength) / 10 +
      3 * creature_size(creature);
    if (creature->experience < 5) creature->experience = 5;
    else if (creature->experience > 20) creature->experience = 20;
  }
  if (flags0 & 2) creature->levitated = 10 + randbytemod(5);
  if (cc == ccShopkeeper) aggr = 0;
  else aggr = randbytemod(2) + cond2bool(difficulty > dkVeryEasy);
  creature_set_aggressivity(creature, aggr);
  creature->ch = ( (cc == ccVarydamage) ? '9' : ccd->ch );

  /* Put the creature to sleep/nap if "appropriate" */

  if (creating_original_rogue) { /* don't sleep */ }
  else if (cflags & cfImitates)
  { /* imitates an object, and objects don't move; IMPLEMENTME: can also
       imitate an obstacle or staircase! */
    creature->napping = NAPKIND_DEEPSLEEP;
    creature->cd = (tCreatureData) ( ( cc == ccSchroedincat) ?
      IMI_BUILD(ocTool, tokBox) : object_get_randimi() );
    /* A ccSchroedincat should specifically imitate a _large_ box... */
  }
  else if (creature_keep_active(creature)) { /* don't sleep */ }
  else if ( (cc) && (!(flags0 & 1)) )
  { if ( (ccf & ccfSleepsDeeply) ||
         ( (storey < 5) && (__randpercent(7 - difficulty)) ) )
    { creature->napping = NAPKIND_DEEPSLEEP; }
    else if ( (storey < 10 - difficulty) &&
              (randpercent(30 - 2 * storey - 5 * difficulty)) )
    { creature->napping = NAPKIND_SLEEP; }
    else if ( (storey < 12 - difficulty) &&
              (randpercent(50 - 2 * storey - 10 * difficulty)) )
    { creature->napping = randfewticksfromrange(15 + sectionsizemeasure / 1000,
        150 - 2 * storey - 10 * difficulty);
    }
  }

  /* Check whether the creature should get some initial objects, and do some
     other class-dependent initializations */

  if ( (cc == ccLeprechaun) || (cc == ccCoxswain) ) /* have gold */
  { tObject* const o = object_create(ocGold, randbytemod(maxGoldKind + 1), 0);
    if (o != NULL)
    { o->quantity = 50 + randbytemod(50);
      (void) creature_add_object(creature, o);
    }
  }
  else if (cc == ccNymph) /* might have some stolen objects */
  { unsigned char count = randbytemod(3);
    while (count-- > 0)
    { tObject* const o = object_randcreate(0);
      if (o != NULL) (void) creature_add_object(creature, o);
    }
  }
  else if (cc == ccVampire) /* how long can it stand rliUnlimited yet? */
  { creature->cd = 50 * (difficulty + 1) + randbytemod(50);
  }
  else if (cc == ccArcher) /* has bow/crossbow and arrows/bolts */
  { const tBoolean cross = __randpercent(20);
    tObject* o = object_create(ocWeapon, cross ? wkCrossbow : wkBow, 2);
    if (o)
    { (void) creature_add_object(creature, o);
      creature_wield(creature, o);
    }
    o = object_create(ocWeapon, cross ? wkCrossbowBolt : wkArrow, 2);
    if (o)
    { tObjectQuantity q = 2 * difficulty + randbytemod(5);
      if (q < 2) q = 2;
      o->quantity = q;
      (void) creature_add_object(creature, o);
    }
  }
  else if (cc == ccGuard) /* might have a lantern */
  { if (randpercent(current_storeyperc))
    { tObject* o = object_create(ocTool, tokLantern, 0);
      if (o) { (void) creature_add_object(creature, o); }
    }
  }
  else if (cc == ccHydra) /* has extra lives */
  { creature->cd = randbytefromrange(1, MAX(difficulty, 1));
  }
  else if (cc == ccWizard) /* has a wand, might have a rebirth ring */
  { enum { num = 9 };
    static const tWandStaffKind wsk[num] = { wskTeleport, wskSlow, wskConfuse,
      wskSleep, wskFreeze, wskHoe, wskRemoteHit, wskCancel,wskCreateCreature };
    unsigned char count = 1;
    if (difficulty < dkNormal)
    { if (randpercent(50 - 10 * difficulty)) count = 0; }
    else { if (randpercent(10 * difficulty)) count = 2; }
    while (count-- > 0)
    { tObject* o = object_create(ocWandStaff, wsk[randbytemod(num)], 2);
      if (o) { (void) creature_add_object(creature, o); }
    }
    if (__randpercent(49 + 5 * difficulty))
    { tObject* o = object_create(ocRing, rkRebirth, 0);
      if (o) { (void) creature_add_object(creature, o); }
    }
  }
  else if ( (!creating_original_rogue) && (creature_is_humanoid(creature)) &&
    (creature_size(creature) >= csMedium) )
  { /* might have a weapon; IMPLEMENTME: might have some armor/stones? */
    if (randpercent(8 + 4 * difficulty + 2 * storey))
    { enum { num = 5 };
      static const tWeaponKind wk[num] = { wkDart, wkPoisonDart, wkDagger,
        wkShuriken, wkSpear };
      tObject* o = object_create(ocWeapon, wk[randbytemod(num)], 2);
      if (o)
      { (void) creature_add_object(creature, o);
        if (randpercent(current_storeyperc)) creature_wield(creature, o);
      }
    }
  }

  /* Put the creature into the current section */

  creature_put_into_section(creature, creating_original_rogue);
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "creature_create_at(cc=%d,x=%d,y=%d,flags=%d,ch=%c%s)",
    creature->cc, creature->x, creature->y, creature->flags, creature->ch,
    ( (creature->flags & cfInvisible) ? (",invis") : (strEmpty) ));
  message_log(debugstrbuf);
#endif
  return(creature);
}

static one_caller void creature_divide(tCreature* creature,
  const tCoordinate x2, const tCoordinate y2)
{ tCreature* const c2 = creature_allocate();
  tTickout* const t = &(c2->tickout);
  creature->health = ((creature->health * 2) / 3) - 1;
  _creature_last_action_set(creature, cakUnknown, 0);
  (*c2) = (*creature); my_memclr(t, sizeof(tTickout)); c2->x = x2; c2->y = y2;
  c2->next = NULL; c2->objects = c2->armor = c2->weapon = NULL;
  creature_put_into_section(c2, falsE);
  if (__randpercent(95 - 18 * difficulty))
  { /* The original creature can't divide any further: */
    creature->flags &= ~cfDivides;
  }
  if (map_can_player_see(creature->x, creature->y, mfCreature))
  { creature_get_name(creature, strbuf, 1); strcat(strbuf, " divides");
    message_show(strbuf);
  }
  /* IMPLEMENTME: animation! */
}

static void faint_cry(void)
{ if (rogue_can_hear)
    message_show("a faint cry of anguish sounds in the distance");
}

/*@null@*/ tCreature* creature_create(const tCreatureClass cc,
  const unsigned char flags0)
/* creates a creature of the given class at a random location in a random room
   in the current section */
{ tBoolean creating_original_rogue = cond2boolean( (!cc) && (!rogue) );
  unsigned short count = ( creating_original_rogue ? 50000 : 100 );
    /* (try harder when creating the player character) */

  while (count-- > 0)
  { tCoordinate x, y;
    tSquareFlags sflags;
    section_randlocationinroom_nonshop(&x, &y, NULL);
    sflags = squareflags_get(x, y);
    if (! (sflags & (sfCreature | sfSeconn | sfObstacle | sfFatalGround |
                     sfFire | sfBarrier | sfWall)) )
    { /* found an appropriate location */
      /* IMPLEMENTME: distribute new creatures _equally_ over all rooms? */
      return(creature_create_at(cc, x, y, flags0));
    }
  }
  if (creating_original_rogue)
  { do_quit_msg(ecRuntime,
      "Could not create the player character. Please try again!");
      /* argh... luckily this almost "can't happen" :-) */
  }
  faint_cry();
  return(NULL);
}

static /*@null@*/ tCreature* creature_randcreate(const unsigned char flags0)
/* creates a creature of a random class (currently except Rogue class) at a
   random location in a random room in the current section;
   "flags0 & 1": only auto-creatable allowed (and it must not sleep) */
{ const tStoreyPerc sp = current_storeyperc;
  unsigned char count = 100;
  while (count-- > 0)
  { const tCreatureClass cc = randbytefromrange(1, maxRandCreatureClass);
    const tCcd* const ccd = &(creatureclassdata[cc]);
    if ( (sp >= ccd->minstorp) && (sp <= ccd->maxstorp) &&
         (! ( (flags0 & 1) && (ccd->ccf & ccfNonAuto) ) ) )
    { return(creature_create(cc, flags0)); }
  }
  /* Didn't "find" an appropriate creature class... */
  faint_cry();
  return(NULL);
}

void creature_autocreate(void)
{ (void) creature_randcreate(1);
}

#if CONFIG_DEBUG
void creature_dump_sc(void)
/* dumps the storey/creature associations ("which lives where"); call this only
   from process_commandline() and quit afterwards! */
{ tStoreyNumber storey;
  for (storey = 1; storey <= dude.sn_hardmax; storey++)
  { const tStoreyPerc sp = storeyperc(storey);
    tCreatureClass cc, count1 = 0, count2 = 0, sum = 0;
    char* temp = debugstrbuf;
    temp += sprintf(temp, "Storey %d (perc %d):", storey, sp);
    for (cc = 1; cc <= maxCreatureClass; cc++)
    { const tCcd* const ccd = &(creatureclassdata[cc]);
      if ( (sp >= ccd->minstorp) && (sp <= ccd->maxstorp) )
      { temp += sprintf(temp, "%s %s", (sum++ ? "," : strEmpty), ccd->name);
        if (cc <= maxRandCreatureClass) count1++;
        else count2++;
      }
    }
    sprintf(temp, " (%d+%d=%d)", count1, count2, sum);
    (void) puts(debugstrbuf);
  }
}
#endif /* #if CONFIG_DEBUG */

static tBoolean creature_randlocatearound(tCreature* creature,
  const tCoordinate x, const tCoordinate y)
/* tries to put the creature somewhere near (x, y); returns whether that was
   possible */
{ tCoordinate x2, y2;
  if (section_randlocatearound(x, y, sfCreature, &x2, &y2))
  { creature_moveabs(creature, x2, y2, dirNone); return(truE); }
  return(falsE);
}

my_inline tBoolean creature_is_ill(const tCreature* creature)
{ return(cond2boolean(creature->health <=
    creatureclassdata[creature->cc].health / 4));
}

static tCreatureInterval creature_interval(const tCreature* creature)
/* returns the current "action interval" for the creature */
{ tCreatureInterval interval;
  tCreatureFlags flags;
  const tObject* object;
  tFewTicks sh;
  tBoolean zip;
  if ( (creature->napping | creature->frozen) /* ||
       (creature->flags & cfImitates) */ )
  { interval = 0; goto out; } /* not acting at all */
  flags = creature->flags;
  if (creature == rogue)
  { /* const tObject* boots; */
    interval = roledata.interval + 256 * roguedata.petrifying + 16*difficulty;
    interval += (roguedata.packweight / 6) + (roguedata.currpackcount * 3);
      /* Don't carry too much! :-) */
#if 0
    if ( ( (boots = roguedata_os(rdosiBoots)) != NULL ) &&
      (boots->kind == tokBootsHaste) )
    { const tPropertyExtent __ench = boots->enchantment, _ench = ( (__ench
        >= 2) ? 2 : __ench ), ench = zero2one(_ench);
      interval -= 300 * ench; break;
    }
#endif
  }
  else
  { const tCreatureSize size = creature_size(creature);
    interval = 1000;
    if (size <= csSmall) interval -= 100; /* slightly faster */
    else if (size >= csGiant) interval *= 2; /* very slow */
    if (flags & cfFlies) interval -= 100; /* currently flying */
  }
  interval -= 50 * creature->cas; /* speed of last action */
  if ( (object = creature->armor) != NULL )
    interval += 10 * object_armor_hindrance(object);
  if ( (object = creature->weapon) != NULL )
    interval += 10 * object_weapon_hindrance(object);
  if (flags & cfSupple) interval -= 50;
  else if (flags & cfStationary) interval += 50;
  if (creature_is_ill(creature)) interval += 100;
  if ( (sh = creature->slowedhasted) != 0 )
  { if (sh > 0) interval /= 2; /* hasted */
    else interval *= 2; /* slowed */
  }
  if (creature->confused | creature->held | creature->_blind)
    interval = (10 * interval) / 8; /* slightly slower */
  switch (section_timezip(current_section->flags))
  { case 1: zip = cond2boolean(creature->x & 1); break;
    case 2: zip = cond2boolean(creature->y & 1); break;
    case 3: zip = cond2boolean((creature->x ^ creature->y) & 1); break;
    default: goto without_zip; /*@notreached@*/ break;
  }
  if (zip) interval >>= 1; /* faster */
  else interval <<= 1; /* slower */
  without_zip:
  if (interval < 200) interval = 200; /* esp. avoid values <= 0 */
  /* IMPLEMENTME (elsewhere): movements should be slower ("deferred") if
     levitated! The player character's movements should be deferred by an iron
     ball and loadstones! (Other activities shouldn't be affected by this.) */
  out:
#if CONFIG_EXDEBUG
  sprintf(debugstrbuf, "interval(%p, %d): %d", creature, creature->cc,
    interval);
  message_log(debugstrbuf);
#endif
  return(interval);
}

void creature_tickout_recalc(tCreature* creature,
  const unsigned char flags0 __cunused)
/* If "flags0 & 1", something changed "fundamentally" and the function _must_
   recalculate; otherwise only the _extent_ of something (e.g. napping) changed
   and recalculation may be superfluous. For now we always recalculate, just to
   avoid problems. */
{ const tCreatureInterval interval = creature_interval(creature);
  tTickout* const tickout = &(creature->tickout);
  tickout_detach(tickout);
  if (interval <= 0) return; /* creature doesn't act at all, e.g. napping */
  tickout->expiry = creature->last_action + interval;
  tickout_schedule(tickout);
}

static void _ccrl(const tCreature* creature, const tRoomIndex r)
{ const tLightMode lm = current_section->room[r].lightmode;
  if (lm & lmAroundCreatures) /* rare */
  { if (creature == rogue) { if (lm & lmAroundPc) { chg: mrm_light = truE; } }
    else { if (lm & lmAroundNpc) goto chg; }
  }
}

static one_caller void check_creatureroomlight(const tCreature* creature,
  const tCoordinate x, const tCoordinate y, const tCoordinate x2,
  const tCoordinate y2)
{ const tRoomIndex r = location2room(x, y), r2 = location2room(x2, y2);
  if (r != roomNone) _ccrl(creature, r);
  if ( (r2 != roomNone) && (r2 != r) ) _ccrl(creature, r2);
  if (!mrm_light)
  { enum { sf = sfTunnel | sfDoorway };
    const tObject* object;
    if ( (creature == rogue) && ( (squareflags_get(x, y) & sf) ||
      (squareflags(x2, y2) & sf) ) )
    { mrm_light = truE; return; } /* IMPLEMENTME properly! */
    object = creature->objects;
    while (object != NULL)
    { if (object_might_emit_light(object)) { mrm_light = truE; return; }
      object = object->next;
    }
  }
}

static void varydamage_change(tCreature* varydamage)
/* changes ("varies") the damage of a ccVarydamage creature */
{ if (varydamage->ch == '1') varydamage->ch = '9';
  else varydamage->ch--;
}

void creature_moveabs(tCreature* creature, const tCoordinate x2,
  const tCoordinate y2, const tDirection dir)
/* moves/relocates the creature to the absolute coordinates (in terms of
   internal data and visible map) */
{ const tBoolean is_rogue = cond2boolean(creature == rogue);
  tSquareFlags sflags2 = squareflags_get(x2, y2);
  tMapFlags mflags, mflags2 = mfNone;
  tCoordinate x, y;
  tBoolean died = falsE;
  tDeathReason reason = drUnknown;

  /* Handle the origin square */

  if (is_rogue)
  { if (rogue_chained) { /* FIXME: pull the iron ball around as necessary! */ }
    mrm_fov = must_recalc_origin = truE; mflags = mfRogueDisappeared;
  }
  else
  { if (creature_size(creature) >= csGiant) mrm_fov = truE;
    mflags = mfCreatureDisappeared;
  }
  x = creature->x; y = creature->y; creature->x = x2; creature->y = y2;
  squareflags_andnot(x, y, sfCreature); map_update(x, y, mflags);
  if (!mrm_light) check_creatureroomlight(creature, x, y, x2, y2);

  /* Check whether the target square is "dangerous" */

  if ( (!creature_is_in_the_air(creature)) && (sflags2 & sfFatalGround) &&
       (!(sflags2 & sfPlatform)) )
  { /* IMPLEMENTME: an aquatar shall not drown!? */
    if (sflags2 & sfWater) reason = drDrown;
    else if (sflags2 & sfSwamp) reason = drSink;
    else
    { reason = drUnknown;
#if CONFIG_DEBUG
      sprintf(debugstrbuf, "BUG: moveabs(%d)", (sflags2 & sfFatalGround));
      message_showdebug(debugstrbuf);
#endif
    }
    if (is_rogue) died = truE;
      /* (player character's death / possible rebirth is handled later!) */
    else died = creature_died(creature, reason, NULL);
    sflags2 = squareflags_get(x2, y2); /* (necessary in case of rebirth!) */
  }

  /* Handle the target square, put the creature there (what a rhyme:-) */

  if ( (sflags2 & sfHidden) && (sflags2 & (sfTunnel | sfDoorway)) &&
       (map_can_player_see(x2, y2, mfCreature)) )
  { /* If the player can see that a creature could go there, he knows there
       must be some tunnel or doorway, so that's not hidden: */
    section_unhide_square(x2, y2);
  }

  recheck_died:
  if (!died)
  { if (sflags2 & sfSpotlight) /* magic light is disturbed by creature */
    { squareflags_andnot(x2, y2, sfSpotlight); mrm_light = truE; }
    squareflags_or(x2, y2, sfCreature);
    if (is_rogue) mflags2 |= mfRogueAppeared; 
    else mflags2 |= mfCreatureAppeared;
    /* creature->room = location2room(x2, y2); */
    creature->lastdir = dir;
    if (creature->cc == ccVarydamage) varydamage_change(creature);
    if ( (x2 == creature->tx) && (y2 == creature->ty) )
      creature->tx = coordNone; /* reached target */
    else if (squareflags_get(x2, y2) & sfTunnel)
      creature->tx = coordNone; /* CHECKME! */
  }

  map_update(x2, y2, mflags2);
  if ( (is_rogue) && (died) )
  { rogue_died(reason, NULL); died = falsE; goto recheck_died; }
}

tBoolean creature_is_flitting(const tCreature* creature,
  const tBoolean is_attacked)
/* checks whether the creature is currently flitting */
{ tBoolean retval = falsE;
  const tCreatureFlags cflags = creature->flags;
  if (!(cflags & cfFlitsAny)) goto out; /* the most likely case */
  if (creature->napping | creature->held | creature->frozen) goto out;
  if (cflags & (is_attacked ? cfFlitsAtt : cfFlitsMov)) retval = truE;
  out: return(retval);
}

tBoolean creature_randflit(const tCreature* aggressor,
  const tCreature* defender)
/* returns whether a currently flitting defender should actually try to avoid a
   normal hit by flitting out of the way */
{ tPercentage avoid = 16;
  if (creature_is_flitting(aggressor, falsE)) /* both are flitting */
    avoid -= 10;
  if (aggressor->slowedhasted)
  { if (aggressor->slowedhasted > 0) avoid -= 10;
    else avoid += 10;
  }
  if (defender->slowedhasted)
  { if (defender->slowedhasted > 0) avoid += 10;
    else avoid -= 10;
  }
  return(randpercent(avoid));
}

tBoolean creature_try_to_flit(tCreature* creature)
/* returns whether the creature was actually able to flit within its current
   environment */
{ tBoolean retval;
  tDirection dir;
  const tCoordinate x = creature->x, y = creature->y;
  for_each_randdir(dir)
  { const tCoordinate x2 = x + xdir[dir], y2 = y + ydir[dir];
    tSquareFlags sflags;
    const tObstacle* obstacle;
    if (!is_location_valid(x2, y2)) continue;
    if ( (is_diagdir(dir)) &&
         (!creature_can_act_diagonally(creature, x2, y2)) )
    { continue; }
    sflags = squareflags_get(x2, y2);
    if (!(sflags & (sfRoom | sfTunnel | sfDoorway))) continue;
    if ( (sflags & sfWall) && (!(sflags & sfDoorway)) ) continue;
    if (sflags & (sfCreature | sfBarrier | sfFire)) continue;
    if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x2, y2)) !=
      NULL ) && (obstacle_effsize(obstacle) > 0) )
    { continue; }
    if ( (sflags & sfFatalGround) && (!(sflags & sfPlatform)) ) continue;
    /* IMPLEMENTME: if ( (sflags & sfGlazedFrost) && (not in air) ) { check
       whether the creature possibly glides onto a "dangerous" location
       later; cf. action.c } */
    /* found a "good" location, so use it: */
#if CONFIG_DEBUG
    message_showdebug("FLIT");
#endif
    creature_moveabs(creature, x2, y2, dir); retval = truE; goto out;
  }
  retval = falsE;
  out: return(retval);
}

tBoolean creature_get_sustain_strength(const tCreature* creature)
/* returns whether the strength of the creature should be sustained (i.e. the
   strength must not be decreased) */
{ tPropertyExtent ench;
  const tObject* ring;
  unsigned char count;
  if (creature != rogue) return(falsE); /* (_currently_!) */
  worn_rings_enchsumcount(ench, ring, rkSustainStrength, count);
  if ( (count > 0) && (ench >= 0) )
  { /* The player character currently wears at least one ring of that kind, and
       the sum of the enchantments is not negative, so: */
    return(truE);
  }
  return(falsE);
}

tHealth creature_defaulthealth(const tCreature* creature)
{ return(creatureclassdata[creature->cc].health);
}

tBoolean creature_is_humanoid(const tCreature* creature)
/* returns whether the creature is of a humanoid creature class; why do we have
   this function? Hint: polymorphing */
{ return(cond2boolean(creatureclassdata[creature->cc].ccf & ccfHumanoid));
}

void creature_set_aggressivity(tCreature* creature,
  const tAggressivity new_value)
{ const tAggressivity old_value = creature->aggr;
  if (SIGN_CHANGED(old_value, new_value)) /* must re-calculate target */
    creature->tx = coordNone;
  creature->aggr = new_value;
}

static tBoolean creature_might_drift(const tCreature* creature)
{ return(cond2boolean( (creature->levitated) && (!creature->held) &&
    (!(creature->flags & cfStationary)) ));
}

tBoolean creature_drifts(const tCreature* creature)
{ if (!creature_might_drift(creature)) return(falsE);
  if (creature == rogue) return(__randpercent(3 * difficulty + 1));
  else return(__randpercent(10 - 3 * creature_size(creature)));
}

static tDamage creature_fire_sensitivity(const tCreature* creature)
{ tDamage retval = 1; /* default value */
  switch (creature->cc)
  { case ccAquatar: case ccIcemonster: retval += 2; break;
    case ccDragon: case ccVarydamage: retval = 0; break;
    case ccYeti: retval++; break;
  }
  return(retval);
}

tBoolean creature_is_in_the_air(const tCreature* creature)
/* returns whether the creature currently is in the air (flying/levitated) */
{ tBoolean retval;
  if (creature->levitated) retval = truE; /* it's magic */
  else if (creature->napping | creature->frozen) retval = falsE;
  else if (creature->flags & cfFlies) retval = truE;
  else retval = falsE; /* the most likely case */
  return(retval);
}

tBoolean creature_land(tCreature* creature)
/* makes the creature land on the ground; call this if creature_is_in_the_air()
   was truE but isn't any longer; returns whether the creature survived the
   landing */
{ const tCoordinate x = creature->x, y = creature->y;
  tSquareFlags sflags = squareflags_get(x, y);
  const tBoolean is_rogue = cond2boolean(creature == rogue);
  tObstacle* obstacle;
  char* ptr;

  if (sflags & sfObstacle)
  { obstacle = location2obstacle(x, y);
    if ( (obstacle != NULL) && (obstacle_effsize(obstacle) <= 0) )
      obstacle = NULL;
  }
  else obstacle = NULL;

#if CONFIG_DEBUG
  sprintf(debugstrbuf, "creature_land(%d,%d,%d)", creature->cc, x, y);
  message_log(debugstrbuf);
#endif

  if (is_rogue)
  { sprintf(strbuf, "%s float %sto the ground", rogue_pronoun(0),
      (obstacle ? strEmpty : "gently "));
    /* (This text shall not depend on blindness etc.!) */
    message_show(strbuf);
  }

  if (obstacle != NULL)
  { const tCreatureSize cs = creature_size(creature);
    if ( (cs <= csSmall) || (obstacle->resistance >= 14) )
    { /* It's not plausible that a small creature could break the obstacle when
         falling on it; so we let it glide off the obstacle and land near it.
         CHECKME: what about _immaterial_ creatures? */
      obstacle_resists:
      if (!creature_dec_health(creature, 1)) /* (falling on the obs. hurts) */
      { if (is_rogue) rogue_died(drInjury, NULL);
        else if (creature_died(creature, drInjury, NULL)) return(falsE);
      }
      if (!creature_randlocatearound(creature, x, y))
      { (void) creature_dec_health(creature, creature->health);
        if (is_rogue) rogue_died(drInjury, NULL);
        else { (void) creature_died(creature,drInternal,NULL); return(falsE); }
        /* (We assume that the creature was hurt more than previously expected
            by falling on the obstacle... CHECKME: we should rather let the
            creature break e.g. a chest or teleport away!? But note that
            teleportation doesn't _guarantee_relocation_ by the time of this
            writing. Etc.) */
      }
    }
    else /* the creature may be able to break the obstacle */
    { if ( (is_rogue) && (rogue_can_see) )
      { /* (not "map_can_player_see()" here - just for once) */
        ptr = strbuf + sprintf(strbuf, "%s hit ", rogue_pronoun(0));
        (void) section_obstacle_get_name(obstacle,ptr,1); message_show(strbuf);
      }
      if (!obstacle_resistance_decrease(obstacle, 1 << creature_size(creature),
        boolean2bool(is_rogue)))
      { goto obstacle_resists; }
      if (!creature_dec_health(creature, randbytemod(2) + 1))
      { if (is_rogue) rogue_died(drInjury, NULL);
        else { if (creature_died(creature, drInjury, NULL)) return(falsE); }
      }
      sflags = squareflags_get(x, y); /* (useful - the obstacle is gone!) */
    }
  }
  /* (Don't say "_else_ if" here - we might want to hide some fatal ground
      under chests in later versions!) */
  if ( (sflags & sfFatalGround) && (!(sflags & sfPlatform)) )
  { tDeathReason reason;
    if (sflags & sfWater) reason = drDrown;
    else if (sflags & sfSwamp) reason = drSink;
    else
    { reason = drUnknown;
#if CONFIG_DEBUG
      sprintf(debugstrbuf, "BUG: creature_land(%d)", (sflags & sfFatalGround));
      message_showdebug(debugstrbuf);
#endif
    }
    if (is_rogue) rogue_died(reason, NULL);
    else { if (creature_died(creature, reason, NULL)) return(falsE); }
  }
  return(truE); /* creature survived */
}

void creature_age_inventories(tTicks ticks)
/* ages the objects of all creatures (except player character) in the current
   section */
{ tTicks diff = aaCarry * ticks; /* not "tFewTicks" here - might overflow! */
  tCreature* creature;
  for_each_currentsectioncreature(creature)
  { tObject* object;
    if (creature->flags & cfDead) continue;
    object = creature->objects;
    while (object != NULL)
    { tObject* next = object->next; /* <object> might be deallocated... */
      object_age_basic(object, ticks);
      object->lifetime = LIMSUB(object->lifetime, diff);
      creature_check_object(creature, object);
      object = next;
    }
  }
}

void creature_fill_section(void)
/* puts some creatures into the current, newly created section */
{ const tStoreyNumber storey = current_storeynum;
  const tStoreyPerc sp = current_storeyperc;
  unsigned char min = 1 + (storey / 10) + (pref_combat / 2),
    max = 2 + (storey / 5) + (difficulty / 2) + ((2*pref_combat) / 3),
    count = sectionsizemeasure / 2500 + randbytefromrange(min, max),
    specialcount = pref_combat / 3, specialtry;

  /* Put some "normal" creatures into the section: */
  if (pref_feat <= 0) { count += specialcount; specialcount = 0; }
  while (count-- > 0) (void) creature_randcreate(0);

  /* Try to put a few "more or less special" creatures into the section: */
  if (specialcount <= 0) goto no_special;
  specialtry = 10 * specialcount;
  while (--specialtry > 0)
  { enum { num = 6 };
    static const tCreatureClass c[num] =
    { ccGuard, ccHydra, ccSlime, ccTrogue, ccVarydamage, ccWizard };
    const tCreatureClass cc = c[randbytemod(num)];
    const tCcd* const ccd = &(creatureclassdata[cc]);
    if ( (sp >= ccd->minstorp) && (sp <= ccd->maxstorp) )
    { (void) creature_create(cc, 0);
      if (--specialcount <= 0) break; /* did enough */
    }
  }
  no_special: {}
  /* IMPLEMENTME: populate "party" rooms, esp. if pref_combat is high! */
}

const char* _creatureclassname(const tCreatureClass cc)
{ return(creatureclassdata[cc].name);
}

void creatureclassname(const tCreatureClass cc, /*@out@*/ char* dest,
  const unsigned char flags0)
{ const tCcd* const ccd = &(creatureclassdata[cc]);
  const char *const name = ccd->name, ch = name[0];
  if ( (flags0 & 1) && (!(ccd->ccf & ccfNoIndefArt)) )
    sprintf(dest, "%s %s", indefinite_article(ch), name);
  else strcpy(dest, name);
}

void creature_get_name(const tCreature* creature, /*@out@*/ char* dest,
  const unsigned char flags0)
/* "flags0 & 3": article code (0 = none, 1 = definite, 2 = indefinite) */
{ const tCcd* const ccd = &(creatureclassdata[creature->cc]);
  const char* const name = ((creature==rogue) ? role_name(falsE) : ccd->name);
  const unsigned char article = flags0 & 3;
  if (article == 1) sprintf(dest, "the %s", name);
  else if ( (article == 2) && (!(ccd->ccf & ccfNoIndefArt)) )
    sprintf(dest, "%s %s", indefinite_article(name[0]), name);
  else strcpy(dest, name);
}

void creature_wear(tCreature* creature, tObject* object)
{
#if CONFIG_DEBUG
  if (!creature_is_humanoid(creature))
  { message_show("BUG: creature_wear(A)"); return; }
  if (creature->armor != NULL)
  { message_show("BUG: creature_wear(B)"); return; }
#endif
  creature->armor = object; object->flags |= ofBeingWorn;
  if (creature == rogue)
  { roguedata.flags |= rfInvPropertyChanged; statistics_invalidate(svfArmor); }
}

void creature_unwear(tCreature* creature)
{
#if CONFIG_DEBUG
  if (!creature_is_humanoid(creature))
  { message_show("BUG: creature_unwear(A)"); return; }
  if (creature->armor == NULL)
  { message_show("BUG: creature_unwear(B)"); return; }
#endif
  creature->armor->flags &= ~ofBeingWorn; creature->armor = NULL;
  if (creature == rogue)
  { roguedata.flags |= rfInvPropertyChanged; statistics_invalidate(svfArmor); }
}

void creature_wield(tCreature* creature, tObject* object)
{
#if CONFIG_DEBUG
  if (!creature_is_humanoid(creature))
  { message_showdebug("BUG: creature_wield(A)"); return; }
  if (creature->weapon != NULL)
  { message_showdebug("BUG: creature_wield(B)"); return; }
#endif
  creature->weapon = object; object->flags |= ofBeingWielded;
  if (creature == rogue) roguedata.flags |= rfInvPropertyChanged;
}

void creature_unwield(tCreature* creature)
{
#if CONFIG_DEBUG
  if (!creature_is_humanoid(creature))
  { message_showdebug("BUG: creature_unwield(A)"); return; }
  if (creature->weapon == NULL)
  { message_showdebug("BUG: creature_unwield(B)"); return; }
#endif
  creature->weapon->flags &= ~ofBeingWielded;
  creature->weapon = NULL;
  if (creature == rogue) roguedata.flags |= rfInvPropertyChanged;
}

static tBoolean knowledge_material(const tObject* object)
/* returns whether the knowledge of object "material" is material :-) */
{ tBoolean retval = falsE;
  const tObjectClass oc = object->oc;
  switch (oc)
  { case ocTool:
      switch (object->kind)
      { case tokPhotograph: case tokKey: goto yup; /*@notreached@*/ break;
      }
      break;
    case ocStone: yup: retval = truE; break;
    default:
      if (ocbMagic & (1 << oc)) goto yup;
      break;
  }
  return(retval);
}

static void update_visual_knowledge(tObject* object)
/* When the player character gets to "see" the object, ... */
{ object->flags |= ofKnownPrice;
  if (knowledge_material(object)) object->flags |= ofKnownMaterial;
}

static void handle_inventoresque(const tObject* object, const tBoolean adding)
{ tInventoryIndex* const ptr = &(roguedata.rds.inventoresque);
  tBoolean special = falsE; /* AoY or other "crucial" object? */
  switch (object->oc)
  { case ocAmulet:
      if (object->kind == amkYendor) special = truE;
      break;
  }
  if (!special)
  { if (adding) (*ptr)++;
    else if ((*ptr) > 0) (*ptr)--;
  }
}

static void purse_feels_lighter(void)
{ sprintf(strbuf, "%s purse feels lighter", rogue_pronoun(2));
  message_show(strbuf);
}

static void maybe_melt_gold(void* data __cunused)
/* ensures that the player character can never have more than a certain number
   of gold pieces; this especially should ensure that algorithms like in
   gold_pieces() don't come close to an overflow */
{ tObjectQuantity sum, max = (storeyperc(roguedata.rds.sn_deepest) *
    MAX_OBJECT_QUANTITY) / (100 * (difficulty + 1));
  tObject *object, *chosen, *aoy;
  switch (pref_role)
  { case roleQuantumMech: return; /*@notreached@*/ break; /* full amount */
    case roleThief: case roleTourist: case roleTrader:
      if (max < 3000) max = 3000; /* important in the first storeys */
      break;
    default: max >>= 1; break;
  }
  if ( (aoy = _rogue_aoy_find()) != NULL )
  { const tObjectDetail detail = aoy->objdetail;
    if ( (!AMULET_FAKE(detail)) && (amulet_misscount(detail) <= 0) ) max >>= 1;
  }
  loop: chosen = NULL; sum = 0;
  for_each_object(rogue->objects, object)
  { if (object->oc == ocGold)
    { const tObjectQuantity q = object->quantity;
      if (sum > MAX_OBJECT_QUANTITY - q) sum = MAX_OBJECT_QUANTITY;
      else sum += q;
      if ( (q > 1) && ( (chosen == NULL) || (randcointoss()) ) )
        chosen = object;
    }
    /* CHECKME: handle gold which is inside containers! */
  }
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "mmg(): sum=%d, max=%d, ch=%c, q=%d", sum, max,
    ( (chosen != NULL) ? invobj2char(chosen) : '-' ), ( (chosen != NULL) ?
    (chosen->quantity) : (-42) ));
  message_showdebug(debugstrbuf);
#endif
  if ( (sum > max) && (chosen != NULL) )
  { message_show("the banachtarskic hits");
      /* (alludes to the Banach-Tarski paradox) */
    roguedata.packweight -= object_weight(chosen); chosen->quantity >>= 1;
    roguedata.packweight += object_weight(chosen); purse_feels_lighter();
    goto loop; /* check whether some more gold should be removed */
  }
}

tInventoryIndex creature_add_object(tCreature* creature, tObject* object)
/* adds the object to the creature's pack; IMPORTANT: only call this if the
   object does not lie around in the section or belong to any other list any
   more - this function may deallocate the object structure when putting
   objects together! */
{ tInventoryIndex retval = iiInvalid, idx;
  const tBoolean is_rogue = cond2boolean(creature == rogue);
  tObject* object_within_list;
  if (object_might_emit_light(object)) mrm_light = truE; /*must be done early*/
  if (is_rogue)
  { const tObjectClass oc = object->oc;
    roguedata.flags |= rfInventoryChanged;
    if (rogue_can_see) update_visual_knowledge(object);
    if (oc == ocTool)
    { /* Initialize some special-effects information: when the player dropped
         a case, it shall have "forgotten" what has been put into it; when the
         player dropped a photo, the magic of the dungeon shall make it
         entirely black and therefore ineffective - but it shall keep its image
         while lying around in the dungeon, e.g. cf. scary_object(). Picking up
         any of these objects again finally causes the "forgetting" to actually
         take place. */
      switch (object->kind)
      { case tokCase: object->quark_count = 0; object->quark_value = 0; break;
        case tokPhotograph:
          if ( (difficulty > dkVeryEasy) && (!(object->flags & ofProtected)) )
            object->objdetail = phkDarkness;
          break;
      }
    }
    if (pref_role == roleQuantumMech) /*can figure out everything immediately*/
    { object->flags |= ofFullyIdentified; goto id_done; }
    if ( (!(object->flags & ofEnchIdentified)) &&
      (randpercent(roguedata.magic_exp/2 + ((pref_role == roleWizard)?30:0))))
    { object->flags |= ofEnchIdentified; }
    if ( (!(object->flags & ofDamageIdentified)) &&
         (randpercent(rogue->experience / 4)) )
    { object->flags |= ofDamageIdentified; }
    id_done: {}
    /* IMPLEMENTME: sometimes set ..kind_id[object->kind] for magic objects? */
  }
  else
  { /* Another creature gets the object, so the player character can't
       "remember" the properties. */
    object->flags &= ~(ofEnchIdentified | ofEnchExtIdentified |
      ofDamageIdentified | ofChargeIdentified | ofKnownMaterial |ofKnownPrice);
    if (object->oc == ocAmulet) object->objdetail &= ~_AMULET_FAKEKNOWN;
    object->price_tag = 0;
    /* e.g. quench a lantern to keep the player (character) in the dark, and
       quench a weapon to avoid damage to creature or quick burn-down/loss of
       the object */
    object->flags &= ~ofBeingLit;
  }

  object_within_list =
    object_try_to_put_together(creature->objects, object, is_rogue);

  if (object_within_list != NULL) /* successfully put together, so free old */
  { object_deallocate(object);
    if (is_rogue)
    { idx = invobj2index(object_within_list); retval = -1 - idx; }
  }
  else /* put new object into inventory */
  { object_put_on_list(&(creature->objects), object);
    object->flags |= ofBeingCarried;
    if (is_rogue) /* calculate new inventory index etc. */
    { roguedata.currpackcount++; handle_inventoresque(object, truE);
      for_each_invindex(idx)
      { if (roguedata.inventory[idx] == NULL) /* found a free entry */
        { roguedata.inventory[idx] = object; retval = object->invidx = idx;
          break;
        }
      }
    }
  }
  if ( (is_rogue) && (object->oc == ocGold) )
  { /* It would be nicer to handle the gold-melting immediately. But the player
       must get the normal inventory handling message first, before any message
       which is related to melting. */
    aftermath_schedule(maybe_melt_gold, NULL);
  }
  return(retval);
}

void creature_lose_object(tCreature* creature, tObject* object)
/* Call this if the creature "loses" the object for any reason - e.g. object
   lifetime expired, creature died, creature throws object. */
{ const tBoolean is_rogue = cond2boolean(creature == rogue);
  if (object_might_emit_light(object)) mrm_light = truE;
  if (creature->armor == object) creature_unwear(creature);
  else if (creature->weapon == object) creature_unwield(creature);
  else if (is_rogue)
  { const tObjectFlags oflags = object->flags;
    tRogueDataOsi rdosi;
    if (oflags & ofOnEitherHand)
    { if (roguedata_os(rdosiRing1) == object)
      { roguedata_os(rdosiRing1) = roguedata_os(rdosiRing2);
        roguedata_os(rdosiRing2) = NULL;
      }
      else if (roguedata_os(rdosiRing2) == object)
        roguedata_os(rdosiRing2) = NULL;
      object->flags &= ~ofOnEitherHand;
    }
    else if ( (object->oc == ocTool) && (oflags & ofBeingApplied) &&
      (toc2rdosi(object_toolkind2class(object->kind), &rdosi)) &&
      (roguedata_os(rdosi) == object) )
    { roguedata_os(rdosi) = NULL; object->flags &= ~ofBeingApplied; }
    /* IMPLEMENTME: rdosiNecklace (non-tool stuff for amulets)! */
  }

  object_remove_from_list(&(creature->objects), object);
  object->flags &= ~(ofInUse | ofBeingCarried); /* CHECKME! */

  if (is_rogue)
  { const tInventoryIndex idx = invobj2index(object);
    if (idx != iiInvalid) roguedata.inventory[idx] = NULL;
    roguedata.flags |= rfInventoryChanged; handle_inventoresque(object, falsE);
    if (object->quantity > 0)
    { roguedata.packweight -= object_weight(object);
      if (roguedata.packweight < 0) roguedata.packweight = 0; /* CHECKME! */
    }
    roguedata.currpackcount--;
    if (roguedata.currpackcount < 0) roguedata.currpackcount = 0;/* CHECKME! */
  }
}

void creature_check_object(tCreature* creature, tObject* object)
/* checks whether the object in the creature's pack still exists and removes it
   if not; IMPORTANT: call this _always_ after decreasing the quantity or
   lifetime value of an object in the creature's pack! */
{ if (object_still_exists(object)) return; /* nothing to do */
    /* CHECKME: if ( (creature == rogue) &&
       (object->flags & ofChargeIdentified) && (charge might have changed) )
       { roguedata.flags |= rfInvPropertyChanged; } ? */

  if ( (creature == rogue) && (object->quantity > 0) ) /* print a message */
  { const char *verbsuffix = ((object->quantity==1) ? strS : strEmpty), *desc;
    const tObjectClass oc = object->oc;
    if ( (oc == ocTool) && (object->kind == tokNoTea) )
    { verbsuffix = strEs; desc = " (%c) vanish%s"; }
    else if (object_materials(object) == bomPaper) desc=" (%c) turn%s to dust";
    else desc = " (%c) fall%s to pieces"; /* the most likely case */
    object_get_description(object, strbuf);
    sprintf(strbuf + strlen(strbuf), desc, invobj2char(object), verbsuffix);
    message_show(strbuf);
    if ( (oc == ocTool) && (object->flags & ofBeingApplied) )
    { switch (object->kind)
      { case tokCapeInvis:
          if (rogue->flags & cfInvisible) creature_make_visible(rogue);
          break;
        /* FIXME: handle boots too, maybe call creature_tickout_recalc()! */
      }
    }
  }
  creature_lose_object(creature, object); object_deallocate(object);
}

static tPackCount creature_maxpackcount(const tCreature* creature)
/* returns how many objects the <creature> can carry at most */
{ tPackCount retval;
  if (creature == rogue) retval = roguedata.maxpackcount;
  else
  { const tCreatureSize size = creature_size(creature);
    if (size <= csSmall) retval = 0; /* too small to carry anything */
    else retval = 1 << size;
  }
  return(retval);
}

static void creature_lose_all_objects(tCreature* creature,
  const tBoolean allow_remains)
{ tObject *object, *remains = NULL;
  tPackCount mpc;

  while ( (object = creature->objects) != NULL )
  { creature_lose_object(creature, object);
    if (allow_remains)
    { if (remains == NULL) { remains = object; continue; }
      else if (randpercent(10 * difficulty))
      { object_deallocate(remains); remains = object; continue; }
    }
    object_deallocate(object);
  }

  if ( (remains == NULL) && (allow_remains) )
  { const tPercentage p = 15 - 4 * difficulty + current_storeynum / 5;
    if (creature->cc == ccSlime)
    { /* don't create a nice wskCreateCreature due to cfDivides, at most create
         slime molds instead; slime creature, slime mold - got it? :-) */
      if (randpercent(2 * p)) remains = object_create(ocFood, fkFruit, 0);
    }
    else if ( ((mpc = creature_maxpackcount(creature)) > 0) &&
      (randpercent(p + mpc)) ) /* rare, valuable stuff */
    { const tSpecialHitFlags shf = creature->shf;
      if (shf & shfDrainsLife)
        remains = object_create(ocWandStaff, wskDrainLife, 0);
      else if (shf & shfFlames)
        remains = object_create(ocWandStaff, wskFlame, 0);
      else if (shf & shfConfuses)
        remains = object_create(ocWandStaff, wskConfuse, 0);
      else if (creature->flags & cfDivides)
      { if (__randpercent(31 - 6 * difficulty))
          remains = object_create(ocWandStaff, wskCreateCreature, 0);
      }
      else if (shf & shfFreezes)
      { if (randpercent(20 * difficulty - 3 * pref_magic))
          remains = object_create(ocFood, fkSolidWater, 0);
        else
          remains = object_create(ocWandStaff, wskFreeze, 0);
      }
    }
  }
  if (remains != NULL)
  { section_put_object_carefully(remains, creature->x, creature->y, 2); }
}

void creature_drop_object(tCreature* creature, tObject* object,
  const tObjectQuantity quantity, const tBoolean forced)
/* lets the <creature> drop <quantity> things of the <object> */
{ const tBoolean is_rogue = cond2boolean(creature == rogue);
  if (quantity < object->quantity)
  { /* The object has more "things" than the creature drops. So we split it: */
    if (is_rogue)
    { tObject* o;
      roguedata.packweight -= object_weight(object);
      o = object_split(object, quantity);
      roguedata.packweight += object_weight(object);
      object = o;
    }
    else object = object_split(object, quantity);
  }
  else creature_lose_object(creature, object);
  if (is_rogue)
  { strcpy(strbuf, "dropping "); object_get_description(object, strbuf + 9);
    message_show(strbuf);
  }

  /* Now let's see what happens to the object: */
  if ( (!(object->flags & ofProtected)) && (ground_is_hard(creature->x,
    creature->y)) && (object_mainmat(object) & bomGlass) &&
    ( (forced) || (creature_is_in_the_air(creature)) ) )
  { const tObjectClass oc = object->oc;
    if (is_rogue)
    { if (map_can_player_see(rogue->x, rogue->y, mfObject))
      { const tBoolean pl = object_get_minidesc(object, strbuf, 1, NULL);
        char* ptr = strbuf;
        while (*ptr != '\0') ptr++;
        strcpy(ptr, " break");
        if (!pl) { ptr += 6; *ptr++ = 's'; *ptr = '\0'; }
        message_show(strbuf);
      }
      else if (rogue_can_hear) message_show("*shatter*");
    }
    switch (oc) /* IMPLEMENTME! */
    { case ocPotion: break;
      case ocWandStaff: break;
    }
    object_deallocate(object);
  }
  else
  { section_put_object_carefully(object, creature->x, creature->y,
      boolean2bool(is_rogue) | 2);
  }
}

static /*@null@*/ tObject* creature_select_heaviest_object(const tCreature*
  creature)
/* returns the heaviest object from the creature's pack that's not in use */
{ tObject *retval = NULL, *o;
  tObjectWeight maxweight = -1;
  for_each_object(creature->objects, o)
  { if (!(o->flags & ofInUse))
    { const tObjectWeight weight = object_weight(o);
      if (maxweight < weight) { maxweight = weight; retval = o; }
    }
  }
  return(retval);
}

tBoolean creature_can_carry_object(tCreature* creature, const tObject* object,
  const unsigned char flags0)
/* returns whether the creature can put the object into its pack (as opposed to
   "the pack would become too big/heavy then"); "flags0 & 1": the creature must
   "auto-drop" some other object if that's the only way to get <object> added
   to the pack */
{ const tObjectWeight ow = object_weight(object);
  tObject* o;
  loop:
  if (creature == rogue)
  { if ( (roguedata.currpackcount < creature_maxpackcount(rogue)) &&
         (roguedata.packweight + ow < roguedata.maxpackweight) )
    { /* can carry; the weight check might have some small "rounding" problems
         for ocGold things, but this is only relevant if the pack is "almost"
         full, so we don't care about this - it happens very rarely and doesn't
         really matter, and the test is on the "safe" side... */
      return(truE);
    }
    if (flags0 & 1) /* forcibly drop the heaviest object that's not in use */
    { if ( (o = creature_select_heaviest_object(rogue)) != NULL )
      { creature_drop_object(rogue, o, o->quantity, truE); goto loop; }
    }
  }
  else /* object _weight_ shall not matter much for other creatures */
  { tPackCount counter;
    if (ow >= 200) goto out; /* just to avoid extreme nonsense */
    counter = creature_maxpackcount(creature);
    if (counter <= 0) goto out; /* can't carry anything */
    for_each_object(creature->objects, o)
    { if (--counter == 0) /* pack full */
      { if (!(flags0 & 1)) goto out;
        if ((o = creature_select_heaviest_object(creature)) == NULL) goto out;
        creature_drop_object(creature, o, o->quantity, truE); goto loop;
      }
    }
    return(truE);
  }
  out: return(falsE);
}

static my_inline void __hitmiss(const tBoolean hit)
{ strcpy(strbuf, (hit ? "hitting " : "missing ")); }

static void __hitmissmsg(const tCreature* c, const tBoolean hit)
{ __hitmiss(hit); creature_get_name(c, strbuf + 8, 1); message_show(strbuf); }

#define hitmissmsg(flag) \
  do \
  { if ( (is_rogue) && (map_can_player_see(x, y, mfCreature)) && \
         (map_can_player_see(x, y, mfObjectSpecial)) ) \
      __hitmissmsg(c, flag); \
  } while (0)

static void __hitmissmsgobs(const tObstacle* obstacle, const tBoolean hit)
{ __hitmiss(hit); (void) section_obstacle_get_name(obstacle, strbuf + 8, 1);
  message_show(strbuf);
}

#define hitmissmsgobs(obstacle, flag) do \
  { if ( (is_rogue) && (map_can_player_see(x, y, mfObstacle)) && \
         (map_can_player_see(x, y, mfObjectSpecial)) ) \
      __hitmissmsgobs(obstacle, flag); \
  } while (0)

static tBoolean creature_crash_potion(tCreature* creature,
  const tObject* potion, /*@null@*/ tCreature* causer)
/* lets the creature be hit by the (e.g. thrown) potion; note that we don't
   implement _all_ potion kinds here - that's intentional, some potions shall
   only have an effect when a creature drinks them. -- Returns whether the
   creature survived */
{ const tObjectKind kind = potion->kind;
  tDeathReason reason;
  tHealth health;

  if (!randpercent(10 * MIN(10, MAX(3, potion->quantity))))
  { /* The potion doesn't penetrate into the body of the creature - it doesn't
       do anything, so the creature obviously survives: */
    return(truE);
  }

  if (kind == pkHeal)
  { health = 1;
    increase_health:
    health += creature->health;
    if (creature == rogue)
    { if (health > 99) health = 99;
      if (health > roguedata.maxhealth) health = roguedata.maxhealth;
      if (rogue->health < health)
      { rogue->health = health; statistics_invalidate(svfHealth); }
    }
    else creature->health = MIN(health, 999); /* (just some limit) */
  }
  else if (kind == pkGreatlyHeal) { health = 2; goto increase_health; }
  else if (kind == pkMagicEnergy)
  { if (creature == rogue) rogue_incmagicen(2 + randbytemod(3));
  }
  else if (kind == pkPoison)
  { health = 1; reason = drPoison;
    decrease_health:
    if (!creature_dec_health(creature, health))
    { if (creature == rogue) rogue_died(reason, NULL);
      else if (creature_died(creature, reason, causer)) return(falsE);
    }
  }
  else if (kind == pkLiquidFire)
  { if (creature->cc != ccDragon) /* (dragons are immune to fire) */
    { health = 2; reason = drFire; goto decrease_health; }
  }
  else if (kind == pkConfuse)
  { if (creature->cc != ccMedusa) /* (medusas are immune to confusion) */
      creature_confuse(creature, 10 + randbytemod(5));
  }
  else if (kind == pkHallucinate)
    creature_hallucinate(creature, 20 + randbytemod(5));
  else if (kind == pkBlindness) creature_blind(creature, 10 + randbytemod(5));
  else if (kind == pkLevitate) creature_levitate(creature, 5 + randbytemod(3));
  else if (kind == pkSlow) creature_slow(creature, 5 + randbytemod(3));
  else if (kind == pkHaste) creature_haste(creature, 5 + randbytemod(3));

  return(truE); /* survived */
}

void creature_throw(tCreature* creature, tObject* object,
  tObjectQuantity quantity, tDirection dir)
{ const tBoolean is_rogue = cond2boolean(creature == rogue);
  const tObject* const weapon = creature->weapon;
  const tObjectClass oclass = object->oc;
  const tObjectKind okind = object->kind,
    wkind = ( (weapon != NULL) ? (weapon->kind) : (okNone) );
  const tObjectQuantity objquantity = object->quantity;
  tObjectQuantity maxquantity = MAX_OBJECT_QUANTITY;
  tObjectWeight weight;
  tCoordinate x = creature->x, y = creature->y, distance = 0, maxdistance;
  const tDirectionOffset dx = xdir[dir], dy = ydir[dir];
  unsigned short spoc_flags = boolean2bool(is_rogue), sqroot;
  tSquareFlags sflags;
  tDamage damage, mindamage;
  tObstacle* obstacle;
  tObstacleSize effsize;
  ANIMATION_DECLARE(animdata)

  if (object->flags & ofBeingWielded) creature_unwield(creature);
  if ( (oclass == ocTool) && (okind == tokLantern) )
    object_light_off(object);

  damage = object->damage + object->enchantment / 2 + creature->strength / 10;
  if ( (oclass == ocWeapon) && (object->flags & ofBeingLit) ) damage += 3;

  if ( (creature->strength < 30) && (creature->cc != ccWizard) )
    maxdistance = creature->strength / 3;
  else maxdistance = 10;
  if (is_rogue) maxdistance -= difficulty / 2;

  if (weapon != NULL)
  { if (weapon == object)
    { /* The creature has spent the extra effort of wielding the object before
         throwing it; this should pay. :-) */
      maxdistance += 2; damage += 2;
    }
    else
    { /* Check whether/how the weapon "accelerates" the thrown object (cf.
         object_weapon_combinable()), possibly resulting in higher maxdistance
         and damage values; basic rule: heavier objects can't fly that far, but
         might cause more damage. */
      if ( (wkind == wkBow) && (oclass == ocWeapon) && (okind == wkArrow) )
      { maxdistance += 4; damage += 4; maxquantity = 1; }
      else if ( (wkind == wkCrossbow) && (oclass == ocWeapon) &&
                ( (okind == wkCrossbowBolt) || (okind == wkFireBolt) ) )
      { maxdistance += 5; damage += 3; maxquantity = 1; }
      else if ( (wkind == wkSling) && (oclass == ocStone) )
      { maxdistance += 3; damage += 5; maxquantity = 1; }
      else if ( (wkind == wkBlowgun) && (oclass == ocWeapon) &&
                ( (okind == wkDart) || (okind == wkPoisonDart) ) )
      { maxdistance += 5; damage += 3; maxquantity = 1; }
    }
    /* IMPLEMENTME: if player character and not combinable, check
       tokGlovesStrength! */
  }

  /* Don't throw more than what exists resp. is allowed: */
  if (quantity > objquantity) quantity = objquantity;
  if (quantity > maxquantity) quantity = maxquantity;

  if (quantity < objquantity)
  { /* The object has more "things" than the creature throws now. So we split
       it into one part which remains in the creature's pack and one part which
       is thrown. Since we need not care about the former part any further, we
       simply "overwrite" the pointer <object> with the latter part. */
    if (!is_rogue) object = object_split(object, quantity);
    else
    { tObject* o;
      roguedata.packweight -= object_weight(object);
      o = object_split(object, quantity);
      roguedata.packweight += object_weight(object);
      object = o;
    }
  }
  else
  { /* The creature throws all "things" of this object. */
    creature_lose_object(creature, object);
  }

  if ( (!is_rogue) && (map_can_player_see(x, y, mfCreature)) &&
       (map_can_player_see(x, y, mfObjectSpecial)) )
  { /* (This can't be done earlier because the message text might depend on the
        actual quantity, e.g. "arrows" vs. "an arrow".) */
    creature_get_name(creature, strbuf, 1);
    (void) object_get_minidesc(object, strbuf + strlen(strbuf), 0,
      ( ( (weapon != NULL) && (object_weapon_combinable(weapon, object)) ) ?
      " shoots " : " throws " ));
    message_show(strbuf);
  }

  weight = object_weight(object);
  maxdistance -= OBJECT_WEIGHT2DISTANCESUB(weight);
  if (maxdistance < 1) maxdistance = 1;

  sqroot = intsqrt(MIN(quantity, 100));
  mindamage = ( (sqroot >= 2) ? 2 : 1 );
  if (damage < mindamage) damage = mindamage;
  damage *= sqroot;
    /* (e.g. 20 arrows shall not give 20 times the damage of 1 arrow...) */
  damage *= OBJECT_WEIGHT2DAMAGEFACTOR(weight);
  if (damage < 1) damage = 1; /* "can't happen" */

#if CONFIG_DEBUG
  sprintf(debugstrbuf, "T(cc=%d,maxdist=%d,sqrt=%d,dmg=%d)",
    creature->cc, maxdistance, sqroot, damage);
  message_showdebug(debugstrbuf);
#endif

  ANIMATION_ALLOCATE(animdata, acThrow, x, y, object_get_char(object));
  while (++distance <= maxdistance)
  { if ( (damage > 1) && ( (distance % 2) == 0) ) damage--;
    x += dx; y += dy;
    if (!is_location_valid(x, y)) goto stopped; /* left the dungeon */
    sflags = squareflags_get(x, y);
    if (sflags == sfNone) goto stopped; /* hit solid rock */
    if (sflags & sfBarrier)
    { if ( (oclass == ocWandStaff) && (wandstaff_can_break_barrier(okind)) &&
           (randpercent(object->lifetime / 100)) ) /* break the barrier */
      { squareflags_andnot(x, y, sfBarrier); map_update(x, y, mfBarrier);
        animation_step_and_crash(animdata, x, y, truE);
        object->lifetime -= 1000;
        if (object_still_exists(object)) goto put_object;
        else { object_deallocate(object); goto done; }
      }
      else if (oclass == ocAmulet)
      { /* The player throws an Amulet; this always breaks the barrier, but
           also "destroys" the Amulet. */
        squareflags_andnot(x, y, sfBarrier); map_update(x, y, mfBarrier);
        animation_step_and_crash(animdata, x, y, truE);
        object_deallocate(object);
        goto done;
      }
      else goto stopped; /* (other objects can't break the barrier at all) */
    }
    if ( (sflags & sfHidden) && (sflags & (sfTunnel | sfDoorway)) )
    { if (randpercent(20 - 4 * difficulty))
      { /* Say the object flies through a little crevice or makes the hiding
           rock fall to pieces in some other way - tunnel/doorway no longer
           hidden */
        section_unhide_square(x, y);
      }
      else goto stopped;
    }
    if ( (sflags & sfWall) && (!(sflags & sfDoorway)) )
    { /* check whether the player character hoes the wall */
      tHoeValue p = object_hoevalue(object, bomStone, 0);
      if (p > 0)
      { animation_step_and_crash(animdata, x, y, falsE);
        if ( (damage > 7) && (p < 14) ) p++;
        section_hoe_wall(x, y, dir, p | 16);
      }
      goto stopped;
    }
    animation_step(animdata, x, y);
    if (sflags & sfCreature)
    { tPercentage hit;
      tCreature* const c = location2creature(x, y);
      tCreatureSize size;
      tBoolean is_immaterial;
      if (!c) continue; /* "should not happen" */
      size = creature_size(c);
      is_immaterial = cond2boolean(c->flags & cfImmaterial);
      if (oclass == ocAmulet)
      { if (!__randpercent(90 + 3 * size))
        { hitmissmsg(falsE); continue; } /* missed the creature */
        if (c == rogue) { if (!is_immaterial) goto handle_hit; }
        else
        { hitmissmsg(truE);
          animation_crash_in_between(animdata, truE);
          (void) creature_died(c, drRemoval, creature); /* (no rebirth) */
        }
        if (is_immaterial)
        { /* The Amulet is only slowed down a little bit... */
          maxdistance -= 2; continue;
        }
        else goto put_object;
      }
      else if (oclass == ocWandStaff)
      { if (!randpercent(95 - ( (is_rogue) ? (10 * difficulty) : (20) )
              - 2 * distance))
        { hitmissmsg(falsE); continue; } /* missed the creature */
        /* (The percentage gives nearly 75 for dkNormal, that's the same value
            as in the original Rogue game. - Note that the conditions don't
            depend on the object lifetime, also like in the original (which
            doesn't have any such "lifetime" concept:-).)*/
        /* if (c == rogue) message_show(strSomethingHits); */
        hitmissmsg(truE); animation_crash_in_between(animdata, truE);
        (void) creature_zap_creature(creature, object, c, 1);
          /* FIXME: respect function result! */
        object->lifetime -= 1000;
        if (!object_still_exists(object))
        { object_deallocate(object); goto done; }
        if (is_immaterial)
        { /* The wand is only slowed down... */
          maxdistance -= 4; continue;
        }
        else goto stopped; /* wand has zapped and now rebounds */
      }
      if (is_immaterial) continue;
        /* (can strike immaterial creatures only in the above cases) */
      hit = 90 - 4 * distance + 10 * size + creature->experience / 4 -
        ( (is_rogue) ? (10 * difficulty) : (20) );
      if ( (c->napping) || (c->frozen) ) hit += 20;
      else if ( (c->flags & cfStationary) || (c->held) ) hit += 10;
      if (!randpercent(hit)) /* missed the creature */
      { hitmissmsg(falsE); continue; }

      /* Now handle the hit: */
      handle_hit:
#if CONFIG_DEBUG
      sprintf(debugstrbuf, "MAXDMG=%d", damage);
      message_showdebug(debugstrbuf);
#endif
      if (c == rogue)
      { if (map_can_player_see(rogue->x, rogue->y, mfObjectSpecial))
        { sprintf(strbuf, "%s %s hit by ", rogue_pronoun(0), rogue_verb());
          object_get_description(object, strbuf + strlen(strbuf));
          message_show(strbuf);
        }
        else message_show(strSomethingHits);
      }
      else
      { creature_wake_up(c, cond2boolean( (is_rogue) && (map_can_player_see(x,
          y, mfCreature)) ));
        hitmissmsg(truE);
      }
      if (oclass == ocPotion)
      { (void) creature_crash_potion(c, object, creature);
#if CONFIG_DEBUG
        message_showdebug("CRASH_POTION");
#endif
        object_deallocate(object);
        goto done;
      }
      else if (!creature_dec_health(c, randdamage(damage, 8)))
      { if (c == rogue) rogue_died(drInjury, creature);
        else { (void) creature_died(c, drInjury, creature); }
      }
      else if (creature_can_carry_object(c, object, 1))
      { (void) creature_add_object(c, object); goto done; }
      goto put_object; /* object falls down from the creature */
      /* IMPLEMENTME: glass breaks, the object vanishes (i.e. no creature can
         take the object - remove the ...add_object() call above!):
         if (object_materials(object) & bomGlass)
         { object_deallocate(object); object = NULL; }
      */
    }
    if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x, y)) !=
      NULL ) && ( (effsize = obstacle_effsize(obstacle)) > 0 ) &&
      ( (effsize >= 12) || (randpercent(70 + 3 * effsize - ( (is_rogue) ? (5 *
      difficulty) : (10) ) - 2 * distance + creature->experience)) ) )
    { /* (Note: this does not depend on roguedata.magic_experience, because the
         player character doesn't _zap_ but _throw_ here. And the _percentage_
         doesn't depend on the object lifetime because the obstacle usually
         isn't a magic thingy like the barriers handled above; there the
         lifetime was relevant for the percentage because of the "interference"
         of magic object and (magic) barrier...) */
      /* we hit the obstacle; check whether we _break_ it */
      tHoeValue h = object_hoevalue(object, obstacle_materials(obstacle), 0),
        subh = distance / (4 - difficulty / 2);
      hitmissmsgobs(obstacle, truE);
      animation_crash_in_between(animdata, truE);
      /* IMPLEMENTME: glass objects usually break! */
      if ( (h > subh) && (obstacle_resistance_decrease(obstacle, h - subh,
        (is_rogue ? 1 : 0))) ) /* destroyed the obstacle */
      { goto put_object; }
      if ( (oclass == ocWandStaff) && (pref_feat > 1) && (is_rogue) &&
        (object->enchantment >= 0) &&
        ( (pref_role == roleWizard) || (randpercent(90 - 5 * difficulty)) ) )
      { const tFewTicks lt = object->lifetime;
        if ( (pref_role == roleWizard) || (lt >= 1000) ||
          ( (lt >= 300) && (__randpercent(lt / 10)) ) ) /* zap */
        { tBoolean did_destroy;
          if (map_can_player_see(x, y, mfObject))
          { (void) object_get_minidesc(object, strbuf, 1, NULL);
            strcat(strbuf, " zaps"); message_show(strbuf);
          }
          did_destroy = obstacle_zap(obstacle, object, creature);
          if (lt <= 1000) /* gone is the wand */
          { object_deallocate(object); goto done; }
          object->lifetime -= 1000;
          if (did_destroy) goto put_object;
        }
      }
      goto stopped; /* hit, but didn't break the obstacle */
    }
  }
  goto put_object; /* finally simply falls down... */

  stopped:
  x -= dx; y -= dy; /* going one square "backwards" */

  put_object:
  if ( (is_rogue) && (object->oc == ocAmulet) )
  { /* The player character threw the Amulet away, thus demonstrating lack of
       respect for this very holy thing. :-) Now he has to stand all the
       consequences... Note that the enchantment may become 0 (if the Amulet is
       thrown several times) and how this leads to "problems" for the player
       when trying to leave the dungeon. :-) */
    object->damage /= 3; object->enchantment /= 3;
  }
  if (squareflags_get(x, y) & sfObstacle)
  { if (!object_randlocatearound(object, x, y))
    { if ( (object->oc == ocAmulet) && ( (obstacle = location2obstacle(x, y))
        != NULL ) && (obstacle->resistance < 15) )
      { if (obstacle_effsize(obstacle) > 0) section_crash_obstacle(obstacle,0);
          /* CHECKME: print a message (s/0/1/)? */
        goto do_put;
      }
      else object_deallocate(object);
    }
  }
  else
  { spoc_flags |= 2;
    do_put:
    section_put_object_carefully(object, x, y, spoc_flags);
  }

  done: {}
  /* IMPLEMENTME: if ( (is_rogue) && (creature_might_drift(rogue)) &&
     (weight >= foo)) { player character drifts one square in the opposite
     direction } */
  ANIMATION_DEALLOCATE(animdata);
}

static void creature_vischg(const tCreature* creature)
{ tMapFlags mf;
  if (creature != rogue) mf = mfCreatureChanged;
  else { statistics_invalidate(svfInvisible); mf = mfRogueChanged; }
  map_update(creature->x, creature->y, mf); mrm_fov_giant(creature);
}

void creature_make_invisible(tCreature* creature)
{ creature_vischg(creature); creature->flags |= cfInvisible;
  if (creature == rogue)
  { sprintf(strbuf, "%s become invisible", rogue_pronoun(0));
    message_show(strbuf);
  }
}

void creature_make_visible(tCreature* creature)
{ creature->flags &= ~cfInvisible; creature_vischg(creature);
  if (creature == rogue)
  { sprintf(strbuf, "%s become visible again", rogue_pronoun(0));
    message_show(strbuf);
  }
}

tBoolean creature_can_see(const tCreature* creature)
{ return( ( (creature->_blind) || ( (creature == rogue) &&
    (!_rogue_can_see) ) ) ? falsE : truE );
}

void creature_blind(tCreature* creature, tFewTicks add)
{ const tBoolean was_blind = cond2boolean(creature->_blind > 0);
  if (creature == rogue)
  { if (was_blind) { eh: sprintf(strbuf, "%s eyes hurt", rogue_pronoun(2)); }
    else
    { statistics_invalidate(svfBlind);
      if (!_rogue_can_see) goto eh;
      mrm_fov = truE;
      if (rogue->hallucinating)
        message_show("Oh bummer, everything is dark! Help!");
      else
      { sprintf(strbuf, ( (roguedata.flags & rfIsDazzled) ?
          "a glaring flash of light dazzles %s" :
          "a cloak of darkness falls around %s" ), rogue_pronoun(1));
        message_show(strbuf);
      }
    }
  }
  creature->_blind += add;
  creature_tickout_recalc(creature, (was_blind ? 0 : 1));
}

static void canseeagainmsg(const tBoolean check_h)
{ if ( (check_h) && (rogue->hallucinating) )
    message_show("far out, everything's all cosmic again!");
  else
  { sprintf(strbuf, "%s can see again", rogue_pronoun(0));
    message_show(strbuf);
  }
}

void creature_unblind(tCreature* creature)
{ if (!creature->_blind) return; /* nothing to do */
  creature->_blind = 0; creature_tickout_recalc(creature, 1);
  if (creature == rogue)
  { statistics_invalidate(svfBlind);
    if (_rogue_can_see)
    { tObject* o;
      for_each_object(rogue->objects, o) update_visual_knowledge(o);
      roguedata.flags |= rfInvPropertyChanged;
      canseeagainmsg(truE); mrm_fov = truE;
    }
  }
}

void creature_confuse(tCreature* creature, tFewTicks ticks)
{ const tBoolean was_confused = cond2boolean(creature->confused > 0);
  if (creature == rogue)
  { const tObject* ring;
    tPropertyExtent ench;
    unsigned char count, mental;
    worn_rings_enchsumcount(ench, ring, rkResistConfusion, count);
    if ( (count > 0) && (ench >= 0) ) return; /* resist confusion */
    if ( (mental = roledata.mental_power) > 1 )
    { ticks /= mental;
      if (ticks < 1) ticks = 1;
    }
    if (!was_confused) /* not yet confused */
    { statistics_invalidate(svfConfused);
      if (rogue->hallucinating) message_show("what a trippy feeling!");
      else
      { sprintf(strbuf, "%s feel confused", rogue_pronoun(0));
        message_show(strbuf);
      }
    }
  }
  creature->confused += ticks; creature->tx = coordNone;
  creature_tickout_recalc(creature, (was_confused ? 0 : 1));
}

void creature_unconfuse(tCreature* creature)
{ if (!creature->confused) return; /* nothing to do */
  if (creature == rogue)
  { statistics_invalidate(svfConfused);
    sprintf(strbuf, "%s feel less %s now", rogue_pronoun(0),
      ( (rogue->hallucinating) ? ("trippy") : ("confused") ));
    message_show(strbuf);
  }
  creature->confused = 0; creature_tickout_recalc(creature, 1);
}

void creature_hallucinate(tCreature* creature, tFewTicks add)
{ if (creature == rogue)
  { if (!rogue->hallucinating)
      message_show("oh wow, everything seems soo cosmic!");
    if (add > 5)
    { /* When the player character gets hallucinated by that much hallucination
         energy, some of it shall become usable magic energy: */
      const unsigned char diff = ( (pref_role == roleWizard) ? 3 : 1 );
      add -= diff; rogue_incmagicen(diff);
    }
  }
  creature->hallucinating += add;
}

void creature_unhallucinate(tCreature* creature)
{ if (!creature->hallucinating) return; /* nothing to do */
  if (creature == rogue) message_show("everything's soo boring now");
  creature->hallucinating = 0;
}

void creature_nap(tCreature* creature, const tFewTicks ticks,
  const unsigned char flags0)
/* "flags0 & 1": player character is fainting from hunger, print appropriate
   message; "flags0 & 2": player character doesn't resist sleep (used for the
   command key 'S'!) */
{ tBoolean is_rogue;
  tFewTicks oldnap;
  if (creature_keep_active(creature)) return;
  is_rogue = cond2boolean(creature == rogue);
  if ( (is_rogue) && (!(flags0 & 2)) )
  { const tObject* ring;
    tPropertyExtent ench;
    unsigned char count;
    worn_rings_enchsumcount(ench, ring, rkResistSleep, count);
    if ( (count > 0) && (ench >= 0) ) return; /* resist sleep */
  }
  oldnap = creature->napping;
  if (!oldnap) /* not yet napping */
  { tBoolean oldair = creature_is_in_the_air(creature);
    tMapFlags mf;
    creature->napping = ticks;
    if (!is_rogue) mf = mfCreatureChanged;
    else
    { statistics_invalidate(svfNapping); roguedata.napping_since=dungeonclock;
      if (flags0 & 1) sprintf(strbuf, "%s faint from hunger",rogue_pronoun(0));
      else sprintf(strbuf, "%s fall asleep", rogue_pronoun(0));
      message_show(strbuf);
      mf = mfRogueChanged;
    }
    map_update(creature->x, creature->y, mf);
    if ( (oldair) && (!creature_is_in_the_air(creature)) )
    { if (!creature_land(creature)) return; } /* died */
    if (creature->aggr > 0) /* sleep lowers aggressivity */
    { creature_set_aggressivity(creature, creature->aggr - 1); }
  }
  else
  { if (oldnap == NAPKIND_DEEPSLEEP) { /* nothing! */ }
    else if (oldnap == NAPKIND_SLEEP) creature->napping = ticks;
    else creature->napping += ticks;
  }
  creature_tickout_recalc(creature, 1);
}

void creature_unimitate(tCreature* creature, const tBoolean printmsg)
{ creature->flags &= ~cfImitates;
  if (creature->napping == NAPKIND_DEEPSLEEP) creature->napping = 0;
  map_update(creature->x, creature->y, mfObjectDisappeared|mfCreatureAppeared);
  if (printmsg)
  { strcpy(strbuf, "look, that's ");
    creature_get_name(creature, strbuf + 13, 2);
    strcat(strbuf, "!"); message_show(strbuf);
  }
  creature_tickout_recalc(creature, 1);
}

static const char* after_n_ticks(const tTicks t)
{ static char _buf[50];
  const char* retval;
  if ( (t > 0) && (((tTicks) dungeonclock) > t + 1) )
  { sprintf(_buf, " after %d ticks", dungeonclock - t); retval = _buf; }
  else retval = strEmpty;
  return(retval);
}

void creature_wake_up(tCreature* creature, const tBoolean printmsg)
{ tBoolean is_rogue;
  if (!creature->napping) return; /* nothing to do */
  creature->napping = 0; is_rogue = cond2boolean(creature == rogue);
  if (is_rogue)
  { sprintf(strbuf, "waking up%s", after_n_ticks(roguedata.napping_since));
    message_show(strbuf); roguedata.napping_since = 0;
    statistics_invalidate(svfNapping); 
  }
  if (creature->flags & cfImitates) creature_unimitate(creature, printmsg);
  else creature_tickout_recalc(creature, 1); /* need explicit recalc here */
  map_update(creature->x, creature->y,
    (is_rogue ? mfRogueChanged : mfCreatureChanged));
}

void creature_freeze(tCreature* creature, tFewTicks ticks)
{ tBoolean was_frozen;
  if (creature_keep_active(creature)) return;
  if (creature->cc == ccIcemonster) return; /* already "frozen" :-) */
  if (creature == rogue)
  { const tObject* ring;
    tPropertyExtent ench;
    unsigned char count;
    worn_rings_enchsumcount(ench, ring, rkResistFreeze, count);
    if ( (count > 0) && (ench >= 0) ) return; /* resist freeze */
  }
  was_frozen = cond2boolean(creature->frozen > 0);
  if (!was_frozen) /* not yet frozen */
  { const tBoolean oldair = creature_is_in_the_air(creature);
    tMapFlags mf;
    creature->frozen = ticks;
    if (creature != rogue) mf = mfCreatureChanged;
    else
    { statistics_invalidate(svfFrozen); roguedata.frozen_since = dungeonclock;
      sprintf(strbuf, "%s freeze", rogue_pronoun(0));
      message_show(strbuf); mf = mfRogueChanged;
    }
    map_update(creature->x, creature->y, mf);
    if ( (oldair) && (!creature_is_in_the_air(creature)) )
    { if (!creature_land(creature)) return; } /* died */
  }
  else creature->frozen += ticks;
  creature_tickout_recalc(creature, (was_frozen ? 0 : 1));
}

void creature_unfreeze(tCreature* creature)
{ tMapFlags mf;
  if (!creature->frozen) return; /* nothing to do */
  creature->frozen = 0; creature_tickout_recalc(creature, 1);
  if (creature != rogue) mf = mfCreatureChanged;
  else
  { sprintf(strbuf,"no longer frozen%s",after_n_ticks(roguedata.frozen_since));
    message_show(strbuf); roguedata.frozen_since = 0;
    statistics_invalidate(svfFrozen); mf = mfRogueChanged;
  }
  map_update(creature->x, creature->y, mf);
}

void creature_levitate(tCreature* creature, const tFewTicks add)
{ if (creature->held) creature_cancel_being_held(creature);
  if ( (creature == rogue) && (!rogue->levitated) )
  { statistics_invalidate(svfLevitated);
    if (rogue_chained) break_iron_ball();
    sprintf(strbuf, "%s start floating in the air", rogue_pronoun(0));
    message_show(strbuf);
  }
  creature->levitated += add; creature_tickout_recalc(creature, 1);
}

tBoolean creature_unlevitate(tCreature* creature)
/* returns whether the creature survives */
{ if (!creature->levitated) return(truE); /* nothing to do */
  creature->levitated = 0; creature_tickout_recalc(creature, 1);
  if (creature == rogue) statistics_invalidate(svfLevitated);
  if (creature_is_in_the_air(creature)) return(truE); /* still in the air */
  else return(creature_land(creature));
}

void creature_slow(tCreature* creature, const tFewTicks ticks)
{ const tFewTicks orig_sh = creature->slowedhasted;
  creature->slowedhasted -= ticks;
  if (creature->slowedhasted < -10000) creature->slowedhasted = -10000;
    /* (just to stop it from running away, e.g. in case of bugs) */
  if (SIGN_CHANGED(orig_sh, creature->slowedhasted))
  { /* The sign of ->slowedhasted changed, so inform the player: */
    if (creature == rogue)
    { statistics_invalidate(svfSlowedHasted);
      if (rogue->slowedhasted == 0) message_show(strBackToNormalSpeed);
      else
      { sprintf(strbuf, "%s feel %sself slowing down", rogue_pronoun(0),
          rogue_pronoun(2));
        message_show(strbuf);
      }
    }
    /* IMPLEMENTME: else if (player can see creature) { print message }! */
  }
}

void creature_haste(tCreature* creature, const tFewTicks ticks)
{ const tFewTicks orig_sh = creature->slowedhasted;
  creature->slowedhasted += ticks;
  if (creature->slowedhasted > 10000) creature->slowedhasted = 10000;
    /* (just to stop it from running away, e.g. in case of bugs) */
  if (SIGN_CHANGED(orig_sh, creature->slowedhasted))
  { /* The sign of ->slowedhasted changed, so inform the player: */
    if (creature == rogue)
    { statistics_invalidate(svfSlowedHasted);
      if (rogue->slowedhasted == 0) message_show(strBackToNormalSpeed);
      else
      { sprintf(strbuf, "%s feel %sself speeding up", rogue_pronoun(0),
          rogue_pronoun(2));
        message_show(strbuf);
      }
    }
    /* IMPLEMENTME: else if (player can see creature) { print message }! */
  }
}

void creature_decslowedhasted(tCreature* creature)
/* "decrements" (i.e. brings it closer to 0) the ->slowedhasted timer of the
   creature; call this every tick under the condition "->slowedhasted != 0". */
{ if (creature->slowedhasted > 0) creature_slow(creature, 1);
  else /* < 0 */ creature_haste(creature, 1);
}

tBoolean toc2rdosi(const tToolClass toc, /*@out@*/ tRogueDataOsi* _osi)
{ tBoolean retval;
  switch (toc)
  { case tocBoots: *_osi = rdosiBoots; break;
    case tocGloves: *_osi = rdosiGloves; break;
    case tocCape: *_osi = rdosiCape; break;
    case tocGlasses: *_osi = rdosiGlasses; break;
    case tocShawl: *_osi = rdosiShawl; break;
    case tocHelmet: *_osi = rdosiHelmet; break;
    default: retval = falsE; goto out; /*@notreached@*/ break; /* dunno */
  }
  retval = truE;
  out: return(retval);
}

tBoolean creature_is_shopkeeper(const tCreature* creature)
{ return(cond2boolean(creature->cc == ccShopkeeper));
}

#define GIVE_SHOPKEEPER(mask, oc, kind) \
do \
{ if ( (!(packbits & (mask))) && ( (o = object_create(oc, kind, 2)) != NULL ))\
  { if (creature_can_carry_object(s, o, 0)) creature_add_object(s, o); \
    else object_deallocate(o); \
  } \
} while (0)

void infuriate_shopkeeper(/*@null@*/ tCreature* s, tBoolean immediate)
{ tObject *object, *o;
  unsigned char packbits;
  if (s == NULL)
  { /* IMPLEMENTME: find the shopkeeper! */
    if (s == NULL) return; /* unfindable creatures are uninfuriatable :-) */
  }
  sprintf(strbuf, "the shopkeeper curses %s", rogue_pronoun(1));
  message_show(strbuf);
  if (difficulty >= dkVeryHard) immediate = truE;
  if (immediate) rogue_died(drRemoval, s);
  s->flags &= ~(cfPro | cfStationary); s->flags |= cfImmaterial;
  s->shf |= shfZaps; creature_set_aggressivity(s, 5 * (difficulty + 1));
  packbits = 0;
  for_each_object(s->objects, object)
  { switch (object->oc)
    { case ocWeapon:
        if (object->kind == wkPoisonDart) packbits |= 1;
        break;
      case ocPotion:
        if (object->kind == pkLiquidFire) packbits |= 2;
        break;
      case ocWandStaff:
        switch (object->kind)
        { case wskSlow: packbits |= 4; break;
          case wskConfuse: packbits |= 8; break;
          case wskRemoteHit: packbits |= 16; break;
        }
        break;
    }
  }
  GIVE_SHOPKEEPER(1, ocWeapon, wkPoisonDart);
  GIVE_SHOPKEEPER(2, ocPotion, pkLiquidFire);
  GIVE_SHOPKEEPER(4, ocWandStaff, wskSlow);
  GIVE_SHOPKEEPER(8, ocWandStaff, wskConfuse);
  GIVE_SHOPKEEPER(16, ocWandStaff, wskRemoteHit);
  /* IMPLEMENTME: provide some different, more specific messages and reactions
     depending on what the player did! Especially, provide a softer reaction
     when the player character is confused or blind (unless difficulty is
     high). */
  creature_tickout_recalc(s, 1);
}

void creature_cancel_being_held(tCreature* creature)
{ tHoldingCause cause;
  if (!creature->held) return; /* nothing to do */
  creature->held = 0; creature_tickout_recalc(creature, 1);
  if (creature != rogue) return; /* done */

  cause = roguedata.holding_cause;
  if (cause == hcBearTrap) message_show("the bear trap falls to pieces");
  else if (cause == hcPit)
  { sprintf(strbuf, "%s leave the pit", rogue_pronoun(0));
    message_show(strbuf);
    mrm_light = truE; /* esp. necessary if player character carries a light */
    if (map_can_player_see(rogue->x, rogue->y, mfGround))
    { sprintf(strbuf,"it vanishes in a dim %s flashlight",enchantmentcolorstr);
      message_show(strbuf);
    }
  }
  else if (cause == hcCobweb) message_show("the cobweb tears");
  else if (cause == hcCreatureClasp)
    message_show("the magic creature clasp loosens");

  sprintf(strbuf, "%s can move again", rogue_pronoun(0));
  message_show(strbuf);
}

void cancel_petrifying(void)
{ roguedata.petrifying = 0; creature_tickout_recalc(rogue, 1);
  sprintf(strbuf, "%s %s agile again", rogue_pronoun(0), rogue_verb());
  message_show(strbuf);
}

void break_iron_ball(void)
{ message_show("the iron ball breaks"); roguedata.iron_ball_x = coordNone;
  creature_tickout_recalc(rogue, 1); mrm_screen = truE;
}

void creature_teleport(tCreature* creature)
/* "teleports" the creature, i.e. puts it to a different location in the
   current section without "continuous moving" */
{ const tSectionFlags secf = current_section->flags;
  tRoomIndex old_room, r;
  tBoolean choose_room;
    /* (If the section has more than one room, we can try to choose a room
        which is different from the current room of the creature.) */
  tCoordinate x, y;
  unsigned char count;
  tSquareFlags sflags;

  /* CHECKME: should the creature wake up (sometimes/always)? */

  if (creature == rogue) { if (secf & secfNoTeleportPc) goto fake; }
  else { if (secf & secfNoTeleportNpc) goto fake; }
  /* IMPLEMENTME: secfNoTeleportPcSelf! */
  choose_room = cond2boolean(current_section->num_rooms > 1);
  old_room = creature_room(creature); count = 50;
  try_again:
  while (--count > 0)
  { section_randlocationinroom_nonshop(&x, &y, &r);
    if ( (choose_room) && (r == old_room) ) continue;
    sflags = squareflags_get(x, y);
    if (!(sflags & (sfCreature | sfObstacle | sfWall | sfBarrier |
      sfFatalGround | sfFire | sfWall)))
    { /* IMPLEMENTME: don't simply take this first "usable" location. Try to
         find three and use the one which is farthest away from the current
         location of the creature! */
      creature_moveabs(creature, x, y, dirNone);
      creature->tx = coordNone; /* need to calculate new target... */
      if (creature == rogue)
      { message_show(strWhee); cleanse_price_tags();
        if ( (roguedata.petrifying > 0) && (difficulty <= dkNormal) )
          cancel_petrifying(); /* shall end it */
        if (rogue_chained)
        { roguedata.iron_ball_x = x; roguedata.iron_ball_y = y; }
        /* IMPLEMENTME: silently lose all unpaid shop items! */
      }
      creature_cancel_being_held(creature);
      return; /* done */
    }
  }

  /* If we get here, our prior teleportation attempts failed, so we try to
     loosen the conditions: */
  if (choose_room) { choose_room = falsE; count = 20; goto try_again; }

  /* If we get here, that didn't help (bad luck), so we "fake" a teleport;
     this doesn't actually relocate the creature: */
  fake:
  if (creature->cc == ccVarydamage) varydamage_change(creature);
  creature->lastdir = dirNone;
}

void creature_knightmove(__sunused tCreature* creature __cunused,
  __sunused tDirection maindir __cunused)
{
#if 0
  tDirection subdir;
  if (maindir == dirNone) maindir = randdir();
  subdir = randcrossdir(maindir);
  if (creature == rogue) message_show(strWhee);
  /* IMPLEMENTME! (Do knightmoves only within rooms.) */
#endif
}

tBoolean creature_polymorph(tCreature* creature)
/* "polymorphs" the creature (i.e. changes its class); returns whether the
   creature survived the polymorphing */
{ /* We currently let the creature simply die - this is not the intended effect
     of this function :-) so IMPLEMENTME further - but make sure that humanoid
     creatures only can polymorph to other _humanoid_ creature classes! */
  tBoolean check_storey = truE, check_humanoid =creature_is_humanoid(creature);
  const tStoreyPerc sp = current_storeyperc;
  unsigned char count;
  tCreatureClass /* old_cc, */ cc;
  const tCcd* ccd;

  loop:
  count = 100;
  while (--count > 0)
  { cc = randbytefromrange(1, maxRandCreatureClass);
    ccd = &(creatureclassdata[cc]);
    if ( (check_storey) &&
         (! ((sp >= ccd->minstorp) && (sp <= ccd->maxstorp)) ))
      continue;
    if ( (check_humanoid) && (!(ccd->ccf & ccfHumanoid)) ) continue;
    break; /* found a usable class */
  }
  if (!count) /* no applicable class found */
  { if (check_storey)
    { check_storey = falsE; goto loop; } /* loosen restrictions, try again */
    return(truE); /* can't find a class, don't polymorph */
  }

  if (!(creature->flags & cfInvisible))
  { if ( (_creature_size(creature->cc) >= csGiant) !=
     (_creature_size(cc) >= csGiant) )
    { mrm_fov = truE; }
  }
  /* creature->cc = cc; unwield/unwear/unapply as needed! */
  if (creature == rogue)
  { roguedata.cbf |= cbfDidPolyPc; message_show(strWhee);
    rogue_died(drPolymorph, NULL); creature_cancel_being_held(rogue);
    creature_confuse(rogue, 5 * difficulty + randbytemod(10));
    /* IMPLEMENTME: lose objects! */
    return(truE);
  }
  else
  { creature_cancel_being_held(creature);
    /* IMPLEMENTME: confuse; lose objects; actually polymorph! */
    creature->shf = shfNone; creature->cd = 0;
    return(cond2boolean(!creature_died(creature, drPolymorph, NULL)));
  }
}

static tBoolean do_aggravate(const tBoolean any)
{ tBoolean found_something = falsE;
  tCreature* creature;
  for_each_currentsectioncreature(creature)
  { if ( (!(creature->flags & cfDead)) && 
         ( (any) || (creature->cc == ccBee) ) )
    { creature_wake_up(creature, falsE); creature->flags &= ~cfPro;
      creature->flags |= cfSensCreat;
      if ( (any) && (creature->aggr < 2) )
        creature_set_aggressivity(creature, 2);
      found_something = truE;
    }
  }
  if (found_something)
  { if (rogue_can_hear) /* IMPLEMENTME: use noise_caused_at()! */
    { sprintf(strbuf, "%s hear a %s humming noise", rogue_pronoun(0),
        ( (any) ? ("high pitched") : ("low") ));
      message_show(strbuf);
    }
    else strangemsg();
  }
  return(found_something);
}

tBoolean creature_aggravate(void)
{ return(do_aggravate(truE));
}

tBoolean creature_allure_bees(void)
{ return(do_aggravate(falsE));
}

static void creature_perform_rebirth(tCreature* creature,
  const tDeathReason reason, const tPropertyExtent extra)
{ const tCcd* const ccd = &(creatureclassdata[creature->cc]);
  const tCoordinate x = creature->x, y = creature->y;
  const tSquareFlags sflags = squareflags_get(x, y);
  const tBoolean is_rogue = cond2boolean(creature == rogue);
  tObstacle* obstacle;

  creature_wake_up(creature, falsE); creature_unfreeze(creature);
  creature_cancel_being_held(creature); (void) creature_unlevitate(creature);
  creature->flags &= ~cfFlies; creature->shf &= ~shfActive;
  creature->lastdir = dirNone; creature->tx = coordNone;
  creature_set_aggressivity(creature, 1);
  creature->health = ccd->health + extra;
  creature->strength = ccd->strength + extra;
  if (is_rogue)
  { statistics_invalidate(svfHealth | svfStrength);
    roguedata.actions_left = 1000 + 100 * extra + randbytemod(100);
    rogue_checkactionsleft(); roguedata.flags |= rfDied;
  }

  if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x,y)) != NULL )
    && (obstacle->resistance < 15 ) )
  { /* CHECKME: we can't simply let the obstacle exist in the other cases... */
    if (obstacle_effsize(obstacle) > 0) section_crash_obstacle(obstacle, 0);
  }
  squareflags_andnot(x, y, sfBarrier);

  /* Remove the death reason so that the creature doesn't die again immediately
     and for the same reason. (But it can die for another reason...) */
  if (reason == drDrown) squareflags_andnot(x, y, sfWater);
  else if (reason == drSink) squareflags_andnot(x, y, sfSwamp);
  else if (reason == drFire) squareflags_andnot(x, y, sfFire);
  else if (is_rogue)
  { if (reason == drHypothermia)
      roguedata.endure_freeze = 50 - 9 * difficulty + randfewticks(10);
    else if (reason == drPetrifying) roguedata.petrifying = 0;
  }

  /* Finish */
  creature_tickout_recalc(creature, 1);
  map_update(x, y, mfGround | (is_rogue ? mfRogueRelated : mfCreatureRelated));
  /* IMPLEMENTME: if the creature drowned, let its objects rust? */
}

static tBoolean dr_dusty(const tDeathReason dr)
/* returns whether the death reason turns a vampire to dust */
{ tBoolean retval;
  switch (dr)
  { case drLight: case drSilver: retval = truE; break;
    default: retval = falsE; break;
  }
  return(retval);
}

static tBoolean dr_corpsable(const tDeathReason dr)
/* returns whether the death reason allows to leave a corpse behind */
{ tBoolean retval;
  switch (dr)
  { case drStarvation: case drPoison: case drHypothermia: case drFight:
    case drInjury:
      retval = truE; break;
    default: retval = falsE; break;
  }
  return(retval);
}

tBoolean creature_died(tCreature* creature, tDeathReason reason,
  /*@null@*/ tCreature* causer)
/* returns whether the creature actually died; IMPORTANT: don't call this
   function if "creature == rogue"! (In this case, call rogue_died().) */
{ static const char strIsReborn[] = " is reborn";
  tCoordinate x, y;
  tBoolean allow_remains, can_see;
  tCreatureClass cc;
  const tCcd* ccd;

  if (creature->flags & cfDead) return(truE); /* already dead */

  cc = creature->cc; ccd = &(creatureclassdata[cc]); x = creature->x;
  y = creature->y; allow_remains = cond2boolean(pref_feat > 1);
  can_see = map_can_player_see(x, y, mfCreature);

  if (reason == drInternal)
  { /* The creature has been "killed" for internal/"important" reasons and
       _must_not_be_reborn_! */
    allow_remains = falsE; goto actually_died;
  }
  else if (reason == drRemoval)
  { /* The creature has been killed by very strong magic: */
    allow_remains = falsE; goto actually_died;
  }

  if (cc == ccVampire)
  { if ( (!dr_dusty(reason)) && (creature->cd > 0) &&
         (__randpercent(50 - 5*difficulty*cond2sign(creature->flags & cfPro))))
    { if (can_see)
      { creature_get_name(creature, strbuf, 1); strcat(strbuf, strIsReborn); }
      creature_lose_all_objects(creature, truE);
      creature->cc = ccBat; /* reborn as a bat (but non-flitting etc.) */
      creature->ch = creatureclassdata[ccBat].ch;
      creature->cd = 0;
      if (can_see)
      { strcat(strbuf, " as ");
        creature_get_name(creature, strbuf + strlen(strbuf), 2);
      }
      perform_rebirth:
      if (can_see) message_show(strbuf);
      creature_perform_rebirth(creature, reason, 0);
      return(falsE); /* creature survived */
    }
  }
  else if (cc == ccHydra)
  { if (creature->cd > 0)
    { creature->cd--;
      blah_rebirth:
      if (can_see)
      { creature_get_name(creature, strbuf, 1); strcat(strbuf, strIsReborn); }
      goto perform_rebirth;
    }
  }
  else if (cc == ccWizard)
  { if ( (!(creature->flags & cfPro)) && (__randpercent(10 * difficulty + 1)) )
    { const tObject* o;
      for_each_object(creature->objects, o)
      { if ( (o->oc == ocRing) && (o->kind == rkRebirth) &&
             (o->enchantment >= 0) )
          goto blah_rebirth;
      }
    }
  }
  else if (cc == ccUnicorn)
  { if (causer == rogue)
    { /* IMPLEMENTME: change luck, depending on "alignment" of the unicorn! */
    }
  }

  if ( (difficulty >= dkCrazy) && (!(creature->flags & cfPro)) &&
       (__randpercent(7)) )
    goto blah_rebirth;

  actually_died:
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "creature_died(%d,%d,%d,%d)", cc, x, y, reason);
  message_log(debugstrbuf);
#endif

  /* IMPLEMENTME: some creatures should "explode" when dying, thus hurting any
     other creatures in the neighborhood and maybe burning some objects - all
     depending on chance. Possible creatures: acid blob, dragon, jabberwock,
     xeroc, zombie, slime, varydamage, wizard? Animation? */
  if ( (cc == ccVampire) && (dr_dusty(reason)) )
  { allow_remains = falsE;
    if (can_see)
    { creature_get_name(creature, strbuf, 1); strcat(strbuf, " turns to dust");
      message_show(strbuf);
    }
  }

  creature_lose_all_objects(creature, allow_remains);
  section_unregister_creature(creature); creature->flags |= cfDead;
  current_section->flags |= secfHasDeadCreatures;
  if (causer == rogue)
  { roguedata.rds.defeatcounter[cc]++; roguedata.rds.alldefeats++; }

  if (allow_remains)
  { /* IMPLEMENTME: sometimes create a little statue (tokStatue) of the
       creature, esp. if the creature died from certain wand/staff kinds! The
       statue can consist of a random material (_tStatueMaterial), but with a
       few exceptions: always metal if cfMetallivore/shfEatsMetal; always stone
       if ccMedusa/ccCentaur/ccOrc; always glass if ccAquatar/ccIcemonster/
       ccPhantom/ccWraith/ccWizard; otherwise, often let statue material depend
       on wand/staff/rod material. The statue may contain (some of) the objects
       that were carried by the creature. */
    if ( (creature->flags & cfPlantlike) && (reason == drFight) &&
         (__randpercent(100 - 20 * difficulty)) )
    { /* the plantlike creature "falls to pieces" by a hit, leaving some
         eatable "vegetables" behind; this is a top-secret :-) feature allowing
         the player character to get food if otherwise none is available */
      tObject* const o = object_create(ocFood, fkVegetables, 0);
      if (o != NULL) section_put_object_carefully(o, x, y, 2);
    }
    else if ( (cc == ccAquatar) && (reason == drDrown) &&
              (randpercent(20 * difficulty)) )
    { /* let a new aquatar appear after a while; for the player, it seems as if
         the old aquatar re-appears fully refreshed out of the water */
      section_recreate_creature(x, y, cc, boolean2bool(can_see) | 2);
    }
    else if (cc == ccIcemonster)
    { if ( (reason == drFight) && (!(squareflags_get(x, y) & (sfPlatform |
        sfSeconn))) && (randcointoss()) )
      { /* the ice monster "falls to pieces" by a hit, and some of the pieces
           "transform" and cover the ground with glazed frost */
        squareflags_or(x, y, sfGlazedFrost); map_update(x, y, mfGround);
      }
      else if ( (reason == drDrown) && (randpercent(20 * difficulty)) )
      { /* the ice monster "melts" in the water and becomes an aquatar */
        section_recreate_creature(x, y, ccAquatar, /* not "1|"! */ 2);
      }
    }
    else if ( (cc == ccMedusa) && (__randpercent(10 + 15 * difficulty)) )
    { /* some snakes from the medusa's hair survive (and yes, they shall not be
         cfPro, even if the medusa was) */
      section_bring_out_creatures(x, y, 1 + difficulty / 2 + randbytemod(2),
        ccSnake);
    }
    else if ( (difficulty >= dkNormal) && (!(creature->flags & cfPro)) &&
              (__randpercent(difficulty / 2 + current_storeynum / 3)) )
    { /* let a new creature of the same class appear after some delay; for the
         player, it seems as if the old creature re-appears */
      section_recreate_creature(x, y, cc, boolean2bool(can_see));
    }
    else if ( (dr_corpsable(reason)) &&
      (!(squareflags_get(x, y) & (sfObject | sfObstacle))) )
    { const tCreatureSize size = creature_size(creature);
      if ( (size > csSmall) && (size < csGiant) &&
           (randpercent(2 * current_storeynum)) ) /* leave a corpse behind */
      { tObject* const o = object_create(ocFood, fkCorpse, 0);
        o->objdetail = cc; section_put_object_carefully(o, x, y, 2);
      }
    }
  }
  return(truE); /* creature died */
}

tBoolean creature_dec_health(tCreature* creature, tHealth sub)
/* returns whether the creature's still alive after decreasing the health;
   IMPORTANT: the _caller_ must call creature_died()/rogue_died() if the
   creature died! */
{ tBoolean retval;
  tCoordinate x2, y2;
  const tCreatureFlags cflags = creature->flags;

  /* At first, check whether the creature divides */
  if ( (cflags & cfDivides) && (!(cflags & cfPro)) &&
       (creature->health >= 3) &&
       (randpercent(10 * difficulty + 3 * sub + current_storeynum)) &&
       (section_randlocatearound(creature->x, creature->y,sfCreature,&x2,&y2)))
  { /* The creature magically transforms the damage energy into its special
       division power; the new and old creature each get about 2/3 of the
       original health, that's about 4/3 of the original health altogether! */
    creature_divide(creature, x2, y2); return(truE); /* creature survived */
  }

  /* Decrease the health */
  if (creature->health > sub)
  { creature->health -= sub;
    retval = truE;
    if (creature_is_ill(creature))
    { /* Ill creatures shall lose the abilities to perform active special hits,
         to flit and to fly. (Say, the wings are severely hurt and can never
         regenerate.) */
      creature->flags &= ~cfFlitsAny;
      creature->shf &= ~shfActive;
      if (cflags & cfFlies)
      { tBoolean oldair = creature_is_in_the_air(creature);
        creature->flags &= ~cfFlies;
        if ( (oldair) && (!creature_is_in_the_air(creature)) )
        { retval = creature_land(creature); }
      }
    }
  }
  else
  { creature->health = 0; /* The _caller_ must do the rest! */
    retval = falsE;
  }
  if (creature == rogue) statistics_invalidate(svfHealth);
  return(retval);
}

tBoolean __creature_can_act_diagonally(const tCreature* creature,
  tCoordinate x, tCoordinate y, tCoordinate x2, tCoordinate y2)
/* checks whether the creature is able to "act" in a diagonal direction (from
   the location (x, y) towards the adjacent location (x2, y2)); in the context
   of this function, "act" means "move" or "normal hit" - so e.g. throwing and
   zapping is a completely different story... */
{ tBoolean retval = falsE;
  tSquareFlags sf;
  if (creature->flags & cfSupple) goto cad;
    /* (This was the main reason for implementing cfSupple.) */
  if ( (squareflags_get(x, y) & sfDoorway) ||
       (squareflags_get(x2, y2) & sfDoorway) )
  { goto out; }
  /* Please note _which_ locations we check in the following code! */
  sf = squareflags_get(x, y2);
  if (! (sf & (sfRoom | sfTunnel)) ) goto out;
  if ((sf & sfTunnel) && (sf & sfHidden) && (creature == rogue)) goto out;
  sf = squareflags_get(x2, y);
  if (! (sf & (sfRoom | sfTunnel)) ) goto out;
  if ((sf & sfTunnel) && (sf & sfHidden) && (creature == rogue)) goto out;
  cad: retval = truE;
  out: return(retval);
}

tBoolean creature_can_see_creature(const tCreature* observer,
  const tCreature* target, const tBoolean directly_only)
/* IMPORTANT: if "observer == rogue", use map_can_player_see() instead! */
{ tBoolean retval;
  tCoordinate x, y;
  if ( (!directly_only) && (observer->flags & cfSensCreat) )
  { const tObject* h;
    if ( (target == rogue) && ( (h = roguedata_os(rdosiHelmet)) != NULL ) &&
      (h->kind == tokHelmetTinfoil) && (h->enchantment >= 0) )
    { /* maybe one of the _below_ cases yields truE */ }
    else { retval = truE; goto out; } /* magic sensing */
  }
  if ( (observer->_blind) || (target->flags & cfInvisible) )
  { retval = falsE; goto out; }
  if ( (creature_same_room(observer, target)) /* FIXME: use line of sight! */
    && (brightness_vague(target->x, target->y)) )
  { retval = truE; goto out; }
  x = observer->x; y = observer->y;
  if ( (squareflags_get(x, y) & sfTunnel) &&
       (ABS(target->x - x) <= 1) && (ABS(target->y - y) <= 1) )
  { retval = truE; goto out; }
  /* FIXME: e.g. in lit tunnels at greater distances; use general "line of
     sight" algorithms! */
  retval = falsE;
  out: return(retval);
}

tBoolean creature_enemies(const tCreature* c1, const tCreature* c2)
/* returns whether the two creatures are enemies */
{ if ( (c1->flags & cfPro) != (c2->flags & cfPro) ) return(truE);
  else return(falsE);
}

void creature_make_enemies(const tCreature* aggressor, tCreature* defender)
{ if (defender != rogue)
  { const tAggressivity aggr = defender->aggr;
    if (aggr < 1) creature_set_aggressivity(defender, 1);
    else if (aggr < 5) creature_set_aggressivity(defender, aggr + 1);
    if (aggressor->flags & cfPro) defender->flags &= ~cfPro;
    else defender->flags |= cfPro;
    /* Now they're officially enemies (even if they weren't already). */
  }
}

void creature_last_action_set(tCreature* creature,
  const tCreatureActionKind cak, tCreatureActionSpeed cas)
/* (for the player character, call rogue_las() instead!) */
{ switch (creature->cc)
  { case ccCentaur:
      if ( (cak == cakMove) && (cas < 3) ) cas = 3;
      break;
    case ccWizard:
      if ( (cak_magic(cak)) && (cas < 5) ) cas = 5;
      break;
  }
  _creature_last_action_set(creature, cak, cas);
}

static tDamage creature_strength2damage(const tCreature* creature)
/* calculates the basic damage for a normal hit from the aggressor's strength*/
{ tDamage damage;
  if (creature->cc == ccVarydamage) damage = 2 * (creature->ch - '0') + 1;
  else damage = creature->strength / 5;
  if (creature_is_ill(creature)) damage /= 2;
  if (damage < 1) damage = 1;
  else if (damage > 20) damage = 20; /* make sure it can't be "unlimited" */
  return(damage);
}

static tActionFlags creature_zap_creature(tCreature* actor, tObject* wandstaff,
  tCreature* affected, const unsigned char flags0)
/* "flags0 & 1": the zapping is a result of an immediate contact of <wandstaff>
   and <affected> (e.g. when <wandstaff> is thrown at <affected>; "contact"
   does not mean "real-world" contact here, but rather "magic" contact - look
   how immaterial creatures are handled) */
/* IMPLEMENTME: affect any surrounding creatures (including the player
   character, if so), just to a small extent! */
{ tActionFlags retval = afAttemptedZap;
  tBoolean survived = truE, has_contact = cond2boolean(flags0 & 1);
  tObjectKind wsk = wandstaff->kind;
  const tCreatureClass affclass = affected->cc;
  tDamage damage;

  /* Check whether the <affected> creature is really affected or rather immune
     to the effect of wands/staves of that kind; note that some cases of
     immunity are handled elsewhere, using creature_keep_active(). */

  if ( (affected->flags & cfImmaterial) && (!has_contact) &&
       ( (wsk == wskHoe) || (wsk == wskFlame) || (wsk == wskRemoteHit) ) )
  { /* Zapping itself is a magic action; but the wand/staff has a non-magic
       effect (the effect is a "real-world", "materializing" one) and does not
       have immediate "contact" to <affected>, and <affected> is immaterial. */
    goto done;
  }

  if ( (wsk == wskPolymorph) &&
       ( (affclass == ccAquatar) || (affclass == ccXeroc) ||
         (affclass == ccBouncer) || (affclass == ccSlime) ||
         (affclass == ccVarydamage) || (affclass == ccWizard) ) )
  { /* These shall not polymorph. */
    goto done;
  }

  if (affclass == ccDragon)
  { /* Dragons produce flames themselves, thus they're immune to flames. */
    if (wsk == wskFlame) goto done;
  }
  else if ( (affclass == ccIcemonster) || (affclass == ccYeti) )
  { /* These got used to living in cold environments, so they don't freeze. */
    if (wsk == wskFreeze) goto done;
  }
  else if (affclass == ccMedusa)
  { /* Mrs. Medusa confuses others, thus is immune to confusion. :-) */
    if (wsk == wskConfuse) goto done;
  }
  else if (affclass == ccWizard)
  { /* Wizards are immune to the "strongest" / "worst" effects; note that
       wskPolymorph has already been handled. */
    if ( (wsk == wskRemove) || (wsk == wskTeleport) || (wsk == wskDrainLife) )
      goto done;
  }

  if (affected->shf & shfDrainsLife) /* vampire or bat (if reborn) */
  { if (wsk == wskDrainLife) goto done; }

  /* Now we know that <affected> isn't immune, so actually zap it. Note that
     some effects depend on the relationship of <actor> and <affected> or on
     the enchantment of <wandstaff> etc.; that's often rather a design
     decision than "it _must_ be this way" - we just want some diversity... */

  if (affected == rogue)
  { tObject* object;
    sprintf(strbuf, "%s sense a magic touch", rogue_pronoun(0));
    message_show(strbuf);
    if ( ( (object = roguedata_os(rdosiCape)) != NULL ) &&
      (object->kind == tokCapeShield) && (object->enchantment >= 0) )
    { message_show(strItPasses); object->lifetime -= aaKeepZap;
      creature_check_object(rogue, object); goto done;
    }
    roguedata.flags |= rfIsZapped | rfTickIsZapped;
  }
  retval |= afActuallyHit;

  if (wsk == wskRemove)
  { if (affected == rogue) rogue_died(drRemoval, actor);
    else survived = cond2boolean(!creature_died(affected, drRemoval, actor));
  }
  else if (wsk == wskTeleport)
  { if ( (actor == rogue) && (rogue_in_shop) )
    { if (creature_is_shopkeeper(affected))
        infuriate_shopkeeper(affected, truE);
      else if ( (affected == rogue) && (rogue_has_unpaid_items) )
        infuriate_shopkeeper(NULL, truE);
    }
    else if ( (wandstaff->enchantment<0) && (creature_enemies(actor, affected))
      && (creature_randlocatearound(affected, actor->x, actor->y)) )
    { /* nothing further to do */ }
    else creature_teleport(affected);
  }
  else if (wsk == wskPolymorph) survived = creature_polymorph(affected);
  else if (wsk == wskSlow)
  { tFewTicks t = randfewticks(10) + ( (has_contact) ? (30) : (10) );
    tBoolean are_enemies = creature_enemies(actor, affected),
      is_cursed = cond2boolean(wandstaff->enchantment < 0);
    if (are_enemies == is_cursed) creature_haste(affected, t);
    else creature_slow(affected, t);
  }
  else if (wsk == wskConfuse)
  { tBoolean are_enemies = creature_enemies(actor, affected),
      is_cursed = cond2boolean(wandstaff->enchantment < 0);
    if (are_enemies == is_cursed) creature_unconfuse(affected);
    else creature_confuse(affected, randfewticks(20) +(has_contact ? 60 : 20));
  }
  else if (wsk == wskMakeInvisible)
  { if (!(affected->flags & cfInvisible)) creature_make_invisible(affected);
  }
  else if (wsk == wskMakeImmaterial) affected->flags |= cfImmaterial;
  else if (wsk == wskSleep)
  { const tBoolean are_enemies = creature_enemies(actor, affected),
      is_cursed = cond2boolean(wandstaff->enchantment < 0);
    if (are_enemies == is_cursed)
      creature_wake_up(affected, cond2boolean(affected == rogue));
    else
    { tFewTicks t = randfewticks(3) + ( (has_contact) ? (6) : (2) );
      if (actor == rogue) t += 5 - difficulty;
      else if (affected == rogue) t += 2 * difficulty;
      creature_nap(affected, t, 0);
    }
  }
  else if (wsk == wskFreeze)
  { tObject* o = affected->objects;
    tFewTicks t = randfewticks(3) + ( (has_contact) ? (6) : (2) );
    if (actor == rogue) t += 3 - difficulty / 2;
    else if (affected == rogue) t += difficulty;
    creature_freeze(affected, t);
    while (o != NULL)
    { tObject* const next = o->next;
      if ( (o->oc == ocPotion) && (!(o->flags & ofProtected)) &&
           (__randpercent(75)) )
      { creature_lose_object(affected, o);
        survived = creature_crash_potion(affected, o, actor);
        object_deallocate(o);
        if (!survived) goto done; /* (we don't check further objects in list)*/
      }
      o = next;
    }
  }
  else if (wsk == wskHoe)
  { tBoolean is_rogue;
    tObject* armor;
    damage = 2 + randbytemod(3);
    do_hit:
    if (has_contact) damage += 2;
    is_rogue = cond2boolean(affected == rogue);
    if (is_rogue) message_show(strSomethingHits);
    armor = affected->armor;
    if (armor)
    { damage = damage - armor->damage - MAX(2 * armor->enchantment, -3);
      if (damage < 1) damage = 1; /* Armor shall not hold _all_ damage off! */
      if (!(armor->flags & ofProtected))
      { armor->lifetime /= ( (wsk == wskHoe) ? (3) : (2) );
        creature_check_object(affected, armor);
      }
    }
    if (!creature_dec_health(affected, damage))
    { if (is_rogue) rogue_died(drInjury, actor);
      else survived = cond2boolean(!creature_died(affected, drInjury, actor));
    }
  }
  else if (wsk == wskFlame)
  { tBoolean is_rogue = cond2boolean(affected == rogue);
    tObject* o = affected->objects;
    if (is_rogue) message_show(strQuiteHot);

    /* Burn some objects of the creature */

    while (o != NULL)
    { tBasicObjectMaterials bom;
      tObject* next = o->next;
      if (o->flags & ofProtected) goto do_next;
      if ( (o->oc == ocTool) && (object_tool_is_quark(o->kind)) )
      { o->lifetime = 0; goto do_check; } /* extremely sensitive */
      bom = object_materials(o);
      if ( (bom & bomPaper) && (__randpercent(90)) )
      { o->lifetime = 0; } /* paper burns very easily */
      else if ( (bom & bomTextile) && (__randpercent(80)) )
      { o->lifetime = 0; } /* textile also burns quite easily */
      else if ( (o->oc == ocPotion) && (__randpercent(70)) ) /* explodes */
      { survived = creature_crash_potion(affected, o, actor); }
      else if (bom & (bomGlass | bomLiquid | bomWood))
      { /* glass might break, liquid might evaporate, wood burns */
        o->lifetime -= 1000 * (1 + randbytemod(10));
      }
      else goto do_next;

      do_check: creature_check_object(affected, o);
      do_next: o = next;
    }

    /* Now treat the creature itself (if it's still alive) */

    if ( (survived) && (!creature_dec_health(affected, 3 + randbytemod(2) +
      2 * cond2bool(affclass == ccIcemonster))) )
    { if (is_rogue) rogue_died(drFire, actor);
      else survived = cond2boolean(!creature_died(affected, drFire, actor));
    }
  }
  else if (wsk == wskRemoteHit) { damage = 4 + randbytemod(2); goto do_hit; }
  else if (wsk == wskDrainLife)
  { tBoolean is_rogue = cond2boolean(affected == rogue);
    damage = 3 + randbytemod(3);
    if (is_rogue)
    { sprintf(strbuf, "%s feel sick", rogue_pronoun(0));
      message_show(strbuf);
      damage += difficulty / 2;
    }
    actor->health += 2 * damage; /* (magically doubled) */
    if (actor == rogue)
    { if (actor->health > roguedata.maxhealth)
        actor->health = roguedata.maxhealth;
      statistics_invalidate(svfHealth);
    }
    else if (actor->health > 999) actor->health = 999; /* (just some limit) */
    if ((affected->strength > 1) && (!creature_get_sustain_strength(affected)))
    { affected->strength--;
      if (is_rogue) statistics_invalidate(svfStrength);
    }
    if (!creature_dec_health(affected, damage))
    { if (is_rogue) rogue_died(drInjury, actor); /* CHECKME: any better drX? */
      else survived = cond2boolean(!creature_died(affected, drInjury, actor));
    }
  }
  else if (wsk == wskCancel)
  { const tCreatureFlags aflags = affected->flags;
    if (affected->shf != shfNone)
    { affected->shf = shfNone;
      if ( (affected != rogue) && (creature_enemies(actor, affected)) )
      { affected->flags |= cfRegenerates; } /* the disadvantage :-) */
    }
    if (aflags & cfInvisible)
    { if (aflags & cfPro)
      { if (__randpercent(30 + 10 * difficulty)) /* (50% for dkNormal) */
        { make_visible: creature_make_visible(affected); }
      }
      else
      { if (__randpercent(70 - 10 * difficulty)) /* (50% for dkNormal) */
          goto make_visible;
      }
    }
    if (aflags & cfImmaterial)
    { if (randcointoss()) affected->flags &= ~cfImmaterial; }
    if (aflags & cfSensCreat)
    { if (randcointoss()) affected->flags &= ~cfSensCreat; }
    if ( (aflags & cfImitates) && (actor == rogue) && (__randpercent(90)) &&
         (map_can_player_see(affected->x, affected->y, mfCreature)) )
    { creature_unimitate(affected, truE); } /* cancel imitating */
    if ( (affected->frozen) && (randpercent(100 - 10 * affected->frozen)) )
    { creature_unfreeze(affected); }
    if ( (affected->held) && (randpercent(100 - 5 * affected->held)) )
    { creature_cancel_being_held(affected); }
    if ( (affected->levitated) && (__randpercent(80)) )
    { if (!creature_unlevitate(affected)) { survived = falsE; goto done; }
    }
    if ( (affected = rogue) && (rogue_chained) && ( (difficulty <= dkNormal) ||
      (__randpercent(100 - 19 * difficulty)) ) )
    { break_iron_ball(); }
    /* IMPLEMENTME: cancel the protection of / remove curse/rusting/worm from
       some of the creature's objects? */
  }
  else if (wsk == wskDoSubtle)
  { tObject* o;
    if (affected == rogue)
    { if (__randpercent(15 + 3 * difficulty))
      { roguedata.actions_left = roguedata.actions_left / 2 + 1;
        rogue_checkactionsleft();
      }
      else if (roguedata.endure_freeze < 20) /* (just some low number) */
        roguedata.endure_freeze += randbytemod(5) + 6 - difficulty;
      if (__randpercent(25))
      { if (roguedata.flags & rfRestless) roguedata.flags &= ~rfRestless;
        else roguedata.flags |= rfRestless;
      }
    }
    else
    { const tCreatureFlags aflags = affected->flags;
      if ( (aflags & cfFollows) && (__randpercent(80 - 10 * difficulty)) )
      { affected->flags &= ~cfFollows; }
      if (aflags & cfRegenerates)
      { if (randcointoss()) affected->flags &= ~cfRegenerates; }
      else affected->flags |= cfRegenerates;
    }
    for_each_object(affected->objects, o)
    { if (__randpercent(33)) /* flip protection */
      { if (o->flags & ofProtected) o->flags &= ~ofProtected;
        else o->flags |= ofProtected;
      }
      if ( (!(o->flags & (ofProtected | ofWormy))) &&
        (object_materials(o) & bomWood) && (randcointoss()) )
      { o->flags |= ofWormy; }
    }
    /* IMPLEMENTME: do some other subtle things here! */
  }

  done:
  if (survived)
  { retval |= afSurvived;
    /* IMPLEMENTME: if (creature_might_drift(affected)) { Let it drift! } */
  }
  else if (actor == rogue) rogue_incmagicexp(1);
  return(retval);
}

tActionFlags creature_zap(tCreature* actor, tObject* wandstaff, tDirection dir)
/* A creature zaps with a wand/staff in a certain direction. */
{ tActionFlags retval = afAttemptedZap;
  const tObjectKind kind = wandstaff->kind;
  tCoordinate x2 = actor->x, y2 = actor->y, distance = 0;
  const tDirectionOffset dx = xdir[dir], dy = ydir[dir];
  const tBoolean is_rogue = cond2boolean(actor == rogue);
  tObstacle* obstacle;
  tSquareFlags sflags;
  ANIMATION_DECLARE(animdata)

  /* Find out what gets zapped */

  ANIMATION_ALLOCATE(animdata, acZap, x2, y2, charNone);
  while (1)
  { distance++;
    if (distance == 15) goto nothing; /* more than maximal possible distance */
    x2 += dx; y2 += dy;
    if (!is_location_valid(x2, y2)) goto stopped; /* left the section */
    sflags = squareflags_get(x2, y2);
    if (sflags == sfNone) goto stopped; /* hit solid rock */
    if ( (sflags & sfWall) && (!(sflags & sfDoorway)) )
    { tHoeValue p = object_hoevalue(wandstaff, bomStone, 2);
      if (p > 0)
      { if (is_rogue) p |= 16;
        section_hoe_wall(x2, y2, dir, p | 32);
      }
      goto stopped;
    }
    animation_step(animdata, x2, y2);
    if (sflags & sfBarrier)
    { animation_crash(animdata, truE);
      if ( (wandstaff_can_break_barrier(kind)) && (randpercent(100 -
           3 * current_storeynum + 20 * cond2bool(kind == wskRemove))) )
      { /* Break the magic barrier: */
        squareflags_andnot(x2, y2, sfBarrier); map_update(x2, y2, mfBarrier);
        goto done;
      }
      else
      { goto stopped;
        /* IMPLEMENTME: let the ray bounce from the barrier sometimes? */
      }
    }
    else if (sflags & sfCreature)
    { tCreature* const c = location2creature(x2, y2);
      tPercentage miss;
      if (!c) goto cont; /* "should not happen" */
      miss = 2 * distance + actor->hallucinating - 2 * creature_size(c);
      if (actor == rogue)
      { miss += 5 * difficulty - 2 * roguedata.magic_exp;
        if (pref_role == roleWizard) miss -= 10;
      }
      else if (c == rogue)
      { miss -= 10 * difficulty;
        if (pref_role == roleWizard) miss += 15;
      }
      if (c->flags & cfInvisible) miss += 20;
      else miss -= actor->experience/2;
      if (c->cc == ccWizard) miss += 20; /* can't be zapped so easily... */
      if ( (c->napping) || (c->frozen) ) miss -= 20;
      else if ( (c->flags & cfStationary) || (c->held) ) miss -= 10;
      else miss += 10 * SIGN(c->slowedhasted) + 2 * c->experience;
      if (randpercent(miss)) /* missed */
      { if ( (actor == rogue) && (map_can_player_see(c->x, c->y, mfCreature)) )
        { sprintf(strbuf, "%s miss ", rogue_pronoun(0));
          creature_get_name(c, strbuf + strlen(strbuf), 1);
          message_show(strbuf);
        }
        goto cont; /* ray moves on */
      }
      /* Handle the hit: */
      if ( (kind == wskCreateObject) || (kind == wskCreateCreature) ||
           (kind == wskBuildBarrier) )
        goto stopped; /* yes! (think about it:-) */
      /* IMPLEMENTME: print message "I/you hit the ..." if actor == rogue? */
      animation_crash(animdata, truE);
      retval = creature_zap_creature(actor, wandstaff, c, 0);
      goto done;
    }
    else if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x2, y2))
      != NULL ) )
    { const tObstacleSize effsize = obstacle_effsize(obstacle);
      tPercentage xzap;
      tBoolean zapfin;
      if (sflags & sfHidden) /* unhide hidden obstacles (mostly doors) */
        section_unhide_square(x2, y2);
      if (effsize <= 0) goto cont;
      if ( (effsize < 12) && (randpercent(5 * difficulty + 3 * distance +
        3 * (12 - effsize) - 2 * roguedata.magic_exp)) )
      { goto cont; } /* missed; ray moves on */
      xzap = obstacle_transzapency(obstacle); zapfin = cond2boolean(xzap < 50);
      if (zapfin) animation_crash(animdata, truE); 
      (void) obstacle_zap(obstacle, wandstaff, actor);
      if (zapfin) goto done;
    }

    cont:
    if (!(sflags & (sfObstacle | sfSeconn)))
    { if ( (kind == wskFreeze) && (!(sflags & (sfPlatform | sfFire))) )
      { /* let the ground freeze a little bit :-) */
        if (difficulty >= dkNormal)
        { squareflags_or(x2, y2, sfGlazedFrost);
          map_update(x2, y2, mfGround);
        }
        if (squareflags_get(x2, y2) & sfObject)
        { tObject* object = location2object(x2, y2);
          if (!object) continue; /* "should not happen" */
          if ( (object->oc == ocPotion) && (!(object->flags & ofProtected))
               && (__randpercent(100 - 3 * distance)) )
          { /* The potion freezes, its container breaks, the potion vanishes */
            section_forget_object(object); object_deallocate(object);
          }
        }
      }
      else if (kind == wskFlame)
      { if (sflags & sfGlazedFrost) /* thaw it */
        { tMapFlags mf = mfGround;
          squareflags_andnot(x2, y2, sfGlazedFrost);
          sflags = squareflags_get(x2, y2); /* "revalidate" sflags */
          if ( (sflags & sfObject) && (sflags & sfFatalGround) &&
               (!(sflags & sfPlatform)) )
          { /* The object vanishes in the fatal ground. */
            tObject* o = location2object(x2, y2);
            if (o != NULL) { section_forget_object(o); object_deallocate(o); }
            sflags = squareflags_get(x2, y2); /* "revalidate" sflags again */
            mf |= mfObjectDisappeared;
          }
          map_update(x2, y2, mf);
        }
        if (sflags & sfObject)
        { tObject* o = location2object(x2, y2);
          if ( (o != NULL) && (!(o->flags & ofProtected)) )
          { const tBasicObjectMaterials bom = object_materials(o);
            if ( ( (o->oc == ocTool) && (object_tool_is_quark(o->kind)) ) ||
                 ( (bom & bomPaper) && (__randpercent(90)) ) ||
                 ( (bom & bomTextile) && (__randpercent(80)) ) )
            { /* quarks are extremely sensitive, paper/textile burns easily */
              section_forget_object(o); object_deallocate(o);
            }
          }
        }
      }
      else if (kind == wskCancel)
      { if (sflags & sfObject)
        { tObject* const o = location2object(x2, y2);
          if ( (o != NULL) && (o->oc == ocAmulet) && (o->price_tag < 0) )
            o->price_tag = 0; /* cure a player's goof */
        }
      }
      else if (kind == wskPolymorph)
      { if ( (sflags & sfObject) && (!(sflags & sfCreature)) )
        { tObject* const o = location2object(x2, y2);
          if ( (o != NULL) && (o->oc == ocFood) && (o->kind == fkCorpse) &&
            (randpercent(20 * difficulty + 2 * current_storeynum)) )
          { /* turn the corpse back into a creature sometimes */
            const tObjectDetail detail = o->objdetail;
            section_forget_object(o); object_deallocate(o);
            (void) creature_create_at((tCreatureClass) detail, x2, y2, 1);
            if (actor == rogue) roguedata.cbf |= cbfDidPolyObj;
          }
          /* IMPLEMENTME: slime mold -> slime creature! */
        }
      }
      /* IMPLEMENTME: break the lock of a large tokBox with a wskCancel, wskHoe
         etc.! */
    } /* "cont:" stuff */
  } /* while (1) */

  nothing:
  /* if (actor == rogue) nothing_happens(); -- CHECKME! */
  goto done;

  stopped: /* the ray has been stopped by something */
  if (distance <= 1) goto done; /* no distance, nothing possible */
  x2 -= dx; y2 -= dy; /* going one step "backwards" */
  if (!is_location_valid(x2, y2)) goto done; /* "can't happen" */
  if (!(squareflags_get(x2, y2) & (sfWall | sfBarrier | sfCreature |
    sfSeconn | sfObstacle)))
  { if (kind == wskBuildBarrier)
    { squareflags_or(x2, y2, sfBarrier); map_update(x2, y2, mfBarrier); }
    else if (kind == wskCreateCreature)
    { enum { num = 7 };
      static const tCreatureClass cc[num] = { ccBat, ccDragon, ccLeprechaun,
        ccSnake, ccVampire, ccWraith, ccGuard };
      tCreature* c = creature_create_at(cc[randbytemod(num)], x2, y2, 1);
      if (c != NULL)
      { creature_set_aggressivity(c, 2);
        if (cond2bool(actor->flags & cfPro) ==
            cond2bool(wandstaff->enchantment >= 0))
          c->flags |= cfPro;
        else c->flags &= ~cfPro;
      }
    }
    else if ( (kind == wskCreateObject) &&
              (!(squareflags_get(x2, y2) & sfObject)) )
    { tObject* const o = object_randcreate(0);
      if (o != NULL) section_put_object_carefully(o, x2, y2, 2); /* no msgs! */
    }
  }

  done:
  ANIMATION_DEALLOCATE(animdata);
  return(retval);
}

tBoolean creature_can_affect_location(const tCreature* creature,
  const tCoordinate x2, const tCoordinate y2)
/* returns whether the creature can "affect" the remote or adjacent location */
{ tDirectionOffset dx, dy;
  tCoordinate x = creature->x, y = creature->y;
  if ( (x == x2) && (y == y2) ) /* "should not happen" */
  { failed: return(falsE); }
  if (!are_locations_in_line(x, y, x2, y2)) goto failed;
  dx = SIGN(x2 - x); dy = SIGN(y2 - y);
  while (1)
  { tSquareFlags sflags;
    const tObstacle* obstacle;
    x += dx; y += dy;
    if (!is_location_valid(x, y)) goto failed; /* "should not happen" */
    sflags = squareflags_get(x, y);
    if (sflags == sfNone) goto failed; /* hit solid rock */
    if ( (x == x2) && (y == y2) ) return(truE); /* gotcha! */
    if ( (sflags & (sfCreature | sfBarrier)) ||
         ( (sflags & sfWall) && (!(sflags & sfDoorway)) ) )
    { goto failed; } /* something in the way */
    if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x, y)) !=
      NULL ) && (obstacle_effsize(obstacle) > 0) )
    { /* an obstacle in the way; but IMPLEMENTME: for some actions, very _low_
         obstacles aren't really "in the way"! */
      goto failed;
    }
  }
}

static tBoolean creature_moves_towards(__sunused const tCreature* c1 __cunused,
  __sunused const tCreature* c2 __cunused)
/* returns whether c1 moves towards c2 (based on c1->lastdir) */
{ /* IMPLEMENTME! */
  return(falsE);
}

static tBoolean creature_moves_away_from(__sunused const tCreature* c1
  __cunused, __sunused const tCreature* c2 __cunused)
/* returns whether c1 moves away from c2 (based on c1->lastdir) */
{ /* IMPLEMENTME! */
  return(falsE);
}

tActionFlags creature_specialhit(tCreature* aggressor, tCreature* defender,
  tBoolean may_try_adjacent)
/* performs a "special" hit (sting, confuse, freeze, ...) */
{ tActionFlags retval = afNone;
  tBoolean defeated = falsE; /* (ignores possible rebirth) */
  tCreatureFlags defflags = defender->flags;
  tSpecialHitFlags aggshf = aggressor->shf, defshf = defender->shf;
  tObject *o, *o2;

  if (defender != rogue) goto done; /* (_currently_!) */

  /* And now another promising candidate for the block-nesting award... :-) */
  if (aggshf & shfZaps)
  { if ( (creature_can_affect_location(aggressor, defender->x, defender->y)) &&
         ( (defender != rogue) ||
           (randpercent(5 * difficulty + aggressor->aggr)) ) )
    { tObject* object;
      for_each_object(aggressor->objects, object)
      { if ( (object->oc == ocWandStaff) && (object->lifetime > 1000) )
        { tDirection dir = diroffsets2dir(SIGN(defender->x - aggressor->x),
            SIGN(defender->y - aggressor->y));
          if (dir != dirNone)
          { object->lifetime -= 1000;
            retval = creature_zap(aggressor, object, dir);
            goto done0;
          }
        }
      }
    }
  }
  else if (aggshf & shfConfuses)
  { if ( (!(aggressor->flags & cfInvisible)) && (creature_can_see(defender)) &&
         (!(defshf & shfConfuses)) && (creature_same_room(aggressor,
           defender)) && (creature_moves_towards(defender, aggressor)) &&
         (!defender->confused) )
    { if (defender == rogue)
      { if ( (map_can_player_see(aggressor->x, aggressor->y, mfCreature)) &&
             (__randpercent(3 * difficulty + 1)) )
        { creature_confuse(defender, 2 + difficulty + randbytemod(3));
          /* IMPLEMENTME: slowly turn player actor to stone, at least in higher
             difficulties and if no helping ring is on hand and if he doesn't
             have an (enchanted) amulet and with some probability and if not
             already petrifying! Message: "I/you slowly turn to stone";
             roguedata.petrifying = 1; */
          retval |= afActuallyHit;
          /* IMPLEMENTME: animation! */
        }
      }
      else
      { if ( (creature_can_see_creature(defender, aggressor, truE)) &&
             (__randpercent(5)) )
        { creature_confuse(defender, 4 + randbytemod(4));
          retval |= afActuallyHit;
        }
      }
    }
  }
  else if (aggshf & shfFlames)
  { if ( (!(defflags & cfImmaterial)) && (!(defshf & shfFlames)) &&
         (creature_same_room(aggressor, defender)) &&
         (creature_moves_away_from(defender, aggressor)) )
    { if (defender == rogue)
      { if (randpercent(difficulty))
        { message_show(strQuiteHot);
          if (!creature_dec_health(defender, 2 + randbytemod(2))) /* CHECKME!*/
          { rogue_died(drFire, aggressor); }
          /* IMPLEMENTME: animation! */
          retval |= afActuallyHit;
        }
      }
      else
      { if (__randpercent(2))
        { if (!creature_dec_health(defender, 4 - creature_size(defender)))
          { (void) creature_died(defender, drFire, aggressor); defeated=truE; }
          retval |= afActuallyHit;
        }
      }
    }
  }
  else if (!may_try_adjacent)
  { /* The remaining special hit types would require that the creatures are on
       adjacent squares, and we are told not to presuppose that they are. */
    goto done;
  }
  else if (aggshf & shfStings)
  { if ( (!(defflags & cfImmaterial)) && (!(aggressor->flags & cfImmaterial))&&
         (!(defshf & shfStings)) )
    { if (defender == rogue)
      { if (randpercent(3 * difficulty))
        { if (map_can_player_see(aggressor->x, aggressor->y, mfCreature))
            creature_get_name(aggressor, strbuf, 1);
          else strcpy(strbuf, strSomething);
          strcat(strbuf, " stings"); message_show(strbuf);
          if ( (defender->strength > 1) &&
               (!creature_get_sustain_strength(defender)) )
          { defender->strength--; statistics_invalidate(svfStrength); }
          if (!creature_dec_health(defender, zero2one(difficulty / 2)))
            rogue_died(drPoison, aggressor);
          retval |= afActuallyHit;
        }
      }
      else
      { if (__randpercent(5))
        { if (!creature_dec_health(defender, 1))
          { (void) creature_died(defender,drPoison,aggressor); defeated=truE; }
          retval |= afActuallyHit;
        }
      }
    }
  }
  else if (aggshf & shfSplashes)
  { if ( (defender == rogue) && (!(defflags & cfImmaterial)) &&
         (randpercent(difficulty)) )
    { if (map_can_player_see(aggressor->x, aggressor->y, mfCreature))
      { creature_get_name(aggressor, strbuf, 1);
        strcat(strbuf, " splashes");
        message_show(strbuf);
      }
      gush_of_water();
      retval |= afActuallyHit;
    }
  }
  else if (aggshf & shfFreezes)
  { if ( (!(defshf & shfFreezes)) && (!defender->frozen) &&
         ( ( (defender == rogue) && (__randpercent(8 + 2 * difficulty)) ) ||
           ( (defender != rogue) && (__randpercent(10)) ) ) )
    { if ( (defender == rogue) &&
           (map_can_player_see(aggressor->x, aggressor->y, mfCreature)) )
      { creature_get_name(aggressor, strbuf, 1);
        sprintf(strbuf + strlen(strbuf), " touches %s", rogue_pronoun(1));
        message_show(strbuf);
      }
      creature_freeze(defender, 3 + randbytemod(3)); retval |= afActuallyHit;
    }
  }
  else if (aggshf & shfHolds)
  { if ( (defender == rogue) && (!defender->held) && (!defender->napping) &&
         (!defender->frozen) && (!(defflags & cfImmaterial)) &&
         (randpercent(difficulty)) )
    { /* CHECKME: shall immateriality really protect against the _magic_
         holding? Nah! */
      const tBoolean can_see = map_can_player_see(aggressor->x, aggressor->y,
        mfCreature);
      if ( ( (o = roguedata_os(rdosiCape)) != NULL ) &&
        (o->kind == tokCapeOilskin) && (o->enchantment >= 0) )
      { if (can_see)
        { message_show("the oilskin cape prevents the holding attack");
            /* IMPLEMENTME: nicer message text, e.g. mention creature class! */
          if (!toolkind_id[tokCapeOilskin])
          { toolkind_id[tokCapeOilskin] = truE;
            roguedata.flags |= rfInvPropertyChanged;
          }
        }
        goto dont_hold;
      }
      defender->held = randbytemod(5) + difficulty + 1;
      roguedata.holding_cause = hcCreatureClasp;
      if (can_see) creature_get_name(aggressor, strbuf, 1);
      else strcpy(strbuf, strSomething);
      strcat(strbuf, " holds"); message_show(strbuf);
      retval |= afActuallyHit; dont_hold: {}
    }
  }
  else if (aggshf & shfSpins)
  { if ( (defender == rogue) && (!defender->held) &&
         (!(defflags & cfImmaterial)) )
    { defender->held = 3 + randbytemod(difficulty + current_storeynum/5 + 1);
      roguedata.holding_cause = hcCobweb;
      if (map_can_player_see(aggressor->x, aggressor->y, mfCreature))
      { creature_get_name(aggressor, strbuf, 1);
        sprintf(strbuf + strlen(strbuf), " wraps %s in cobweb",
          rogue_pronoun(1));
      }
      else sprintf(strbuf, "%s get stuck in cobweb", rogue_pronoun(0));
      message_show(strbuf);
      retval |= afActuallyHit;
    }
  }
  else if (aggshf & shfDecExp)
  { if ( (defender == rogue) && (map_can_player_see(aggressor->x, aggressor->y,
      mfCreature)) && (rogue->experience > 5) && (roledata.mental_power > 0) &&
      (__randpercent(difficulty / 2 + 1)) )
    { /* The perception of such a creature in any way causes trouble - unless
         the perceiver is "too stupid" or applies tokGlassesDark. */
      { if ( ( (o = roguedata_os(rdosiGlasses)) != NULL ) &&
          (o->kind == tokGlassesDark) && (o->enchantment >= 0) )
        { goto dont_decexp; } /* is protected */
      }
      rogue->experience--; statistics_invalidate(svfExperience);
      if (rogue->confused)
      { /* creature_confuse() doesn't print a message in this case, so we have
           to do print something here */
        strangemsg();
      }
      creature_confuse(rogue, randbytemod(3 + difficulty) + 2);
      retval |= afActuallyHit; dont_decexp: {}
    }
  }
  else if (aggshf & shfStealsGold)
  { if ( (defender == rogue) && (__randpercent(5 * difficulty + 1)) )
    { for_each_object(rogue->objects, o)
      { if ( (o->oc == ocGold) && (o->quantity >= 5) )
        { tObjectQuantity q = 20 * difficulty + 2 * current_storeynum +
            randbytemod(10); /* how much he'd like to steal */
          if (q >= o->quantity) q = o->quantity - 1; /* avoid inv. handling */
          roguedata.packweight -= object_weight(o); o->quantity -= q;
          roguedata.packweight += object_weight(o); o2 = object_allocate();
          *o2 = *o; o2->quantity = q;
          if (creature_can_carry_object(aggressor, o2, 0))
            (void) creature_add_object(aggressor, o2);
          else object_deallocate(o2); /* let it vanish silently */
          if (aggressor->aggr >= 0)
            creature_set_aggressivity(aggressor, -1); /* run away now */
          purse_feels_lighter(); retval |= afActuallyHit; goto gold_stolen;
        }
      }
    }
    gold_stolen: {}
  }
  else if (aggshf & shfEatsMetal)
  { tObject* weapon;
    if ( (defender == rogue) && (!(rogue->flags & cfImmaterial)) &&
      ( (weapon = rogue->weapon) != NULL ) &&
      (object_mainmat(weapon) & bomMetalN) && (__randpercent(3*difficulty+5)) )
    { char* ptr = strbuf;
      if (map_can_player_see(aggressor->x, aggressor->y, mfCreature))
        creature_get_name(aggressor, ptr, 1);
      else strcpy(ptr, strSomething);
      ptr += strlen(ptr); strcpy(ptr, " eats "); ptr += 6;
      object_get_description(weapon, ptr); message_show(strbuf);
      creature_lose_object(rogue, weapon); object_deallocate(weapon);
      retval |= afActuallyHit;
    }
  }

  /* IMPLEMENTME: all the other special hits! */

  done: if (!defeated) retval |= afSurvived;
  done0: return(retval);
}

void creature_hit(tCreature* aggressor, tCreature* defender)
{ tPercentage hitchance = 0;
  tHealth sub; /* how many health points the defender loses when hit */
  tBoolean defender_survived, must_hit = falsE;
  tBoolean can_see_defender = /* (must be done here due to possible flitting)*/
    map_can_player_see(defender->x, defender->y, mfCreature);
  tObject *armor, *weapon; /* defender's armor, aggressor's weapon */
  tActionFlags af;

  if ( (aggressor == rogue) && (defender->flags & cfImitates) &&
       (can_see_defender) )
  { creature_unimitate(defender, truE);
      /* (necessary for correct message handling, even in case of a "miss"!) */
  }

  /* At first, check whether we should perform a special hit */

  if (aggressor->shf != shfNone)
  { af = creature_specialhit(aggressor, defender, truE);
    if (af & afActuallyHit)
    { defender_survived = cond2boolean(af & afSurvived);
      goto finish; /* (if a special hit happened, no normal hit shall happen)*/
    }
  }

  /* Check whether the aggressor hits or misses */

  if (defender->flags & cfImmaterial)
  { /* can't hit immaterial creatures with non-magic actions */
    goto do_miss;
  }
  if ( (current_storeynum >= dude.sn_hitchance100) && (defender->flags&cfPro) )
  { must_hit = truE; goto do_hit; }

  if ( (creature_is_flitting(defender, truE)) &&
       (creature_randflit(aggressor, defender)) &&
       (creature_try_to_flit(defender)) )
  { goto do_miss; } /* defender managed to flit out of the way */

  if (aggressor == rogue)
  { const tObject* const object = roguedata_os(rdosiGloves);
    hitchance = 90 - 5 * difficulty + rogue_rings2hitchance(10 - difficulty) +
      5 * (roledata.fightskill - 5) - current_storeynum / 3;
      /* (Hitchance-rings are quite effective when the player character hits,
          less when he _is_hit_; that's why we have different factors here and
          below!) */
    if ( (object != NULL) && (object->kind == tokGlovesSkill) )
      hitchance += 10 * zero2one(object->enchantment);
  }
  else if (defender == rogue)
  { hitchance = 60 + 5 * difficulty - rogue_rings2hitchance(6 - difficulty) -
      2 * (roledata.fightskill - 5) + current_storeynum / 2;
    /* IMPLEMENTME: tokCapeDisplacement! */
  }
  else hitchance = 70;

  if (aggressor->weapon) hitchance += 5 * aggressor->weapon->enchantment;
  hitchance += MIN(aggressor->experience, 20);
  if (!creature_can_see(aggressor)) hitchance -= 20;
  if (aggressor->confused) hitchance -= 10;
  if (aggressor->hallucinating) hitchance -= 5;
  if (aggressor->slowedhasted) hitchance += 10 * SIGN(aggressor->slowedhasted);
  if (aggressor->held) hitchance -= 5;
  if (aggressor->flags & cfStationary) hitchance -= 10;
  if (aggressor->flags & cfSupple) hitchance += 5;

  if (!creature_can_see(defender)) hitchance += 10;
  if (defender->confused) hitchance += 10;
  if (defender->slowedhasted) hitchance -= 10 * SIGN(defender->slowedhasted);
  if (defender->napping | defender->frozen) hitchance += 20;
  else
  { hitchance -= MIN(defender->experience, 20);
    if ( (defender->flags & cfStationary) || (defender->held) )
      hitchance += 10;
    if ( (armor = defender->armor) != NULL )
      hitchance += object_armor_hindrance(armor);
    if ( (weapon = defender->weapon) != NULL )
      hitchance += 2 * object_weapon_hindrance(weapon);
  }
  if (defender->flags & cfSupple) hitchance -= 5;
  if (defender->flags & cfInvisible) hitchance -= 10;
  hitchance += 5 * (creature_size(defender) - csNormal);
  if (hitchance < 5) hitchance = 5;
  else if (hitchance > 95) hitchance = 95;

#if CONFIG_DEBUG
  sprintf(debugstrbuf, "HC=%d", hitchance);
  message_show(debugstrbuf);
#endif

  if (__randpercent(hitchance)) goto do_hit;

  /* Handle a "miss" */

  do_miss:
  if (aggressor == rogue)
  { char* s = strbuf;
    s += sprintf(s, "%s%s miss", rogue_pronoun(0), ( (hitchance >= 95) ?
      strSpaceJust : strEmpty ));
    if (can_see_defender) { *s++ = ' '; creature_get_name(defender, s, 1); }
    message_show(strbuf);
  }
  else if (defender == rogue)
  { if (map_can_player_see(aggressor->x, aggressor->y, mfCreature))
    { creature_get_name(aggressor, strbuf, 1);
      if (hitchance >= 95) strcat(strbuf, strSpaceJust);
      strcat(strbuf, strSpaceMisses); message_show(strbuf);
    }
    /* "else": don't print any message like "something misses" */
  }
  else if ( (map_can_player_see(aggressor->x, aggressor->y, mfCreature)) &&
            (can_see_defender) )
  { creature_get_name(aggressor, strbuf, 1);
    strcat(strbuf, strSpaceMisses); strcat(strbuf, strSpace);
    creature_get_name(defender, strbuf + strlen(strbuf), 1);
    message_show(strbuf);
  }
  return; /* done with the "miss" */

  /* Handle a "hit" */

  do_hit:
  armor = defender->armor; weapon = aggressor->weapon;
  sub = creature_strength2damage(aggressor); /* basic damage */
  if (aggressor == rogue) /* look for tokGlovesStrength etc. */
  { const tObject* object = roguedata_os(rdosiGloves);
    if ( (object != NULL) && (object->kind == tokGlovesStrength) )
    { const tPropertyExtent ench = object->enchantment;
      if (ench >= 0) sub += (6 - difficulty) * zero2one(ench);
      else sub += (difficulty * ench) / 2;
    }
    if ( ( (object = roguedata_os(rdosiNecklace)) != NULL ) &&
      (object->oc == ocAmulet) && (object->kind == amkYendor) &&
      (object->enchantment > 0) )
    { sub += 3; }
    if (weapon != NULL) roguedata.rds.num_weaponhits++;
  }
  else if (defender == rogue) /* look for tokCapeShield etc. */
  { const tObject* object = roguedata_os(rdosiCape);
    if ( (object != NULL) && (object->kind == tokCapeShield) )
    { const tPropertyExtent ench = object->enchantment;
      if (ench >= 0) sub -= (6 - difficulty) * zero2one(ench);
      /* "else": nothing */
    }
    if ( ( (object = roguedata_os(rdosiNecklace)) != NULL ) &&
      (object->oc == ocAmulet) && (object->kind == amkYendor) &&
      (object->enchantment > 0) )
    { sub -= 3; }
  }
  if (weapon != NULL) /* add weapon damage */
  { const tObjectQuantity q = weapon->quantity;
    tDamage d = intsqrt(MIN(q, 100)) * (weapon->damage+weapon->enchantment/2);
      /* (intsqrt() because e.g. 20 arrows shall not give 20 times the damage
          of 1 arrow...) */
    if (d > 0) sub += d;
    else sub--; /* quite cursed */
  }
  if (sub < 1)
  { if (must_hit) sub = 1; /* make sure that _some_ damage occurs */
    else goto do_miss; /* no damage would occur, so it's just like a miss */
  }
  if (armor != NULL)
  { sub = sub - armor->damage - armor->enchantment / 2;
    if (sub < 1) sub = 1; /* armor shall not hold _all_ damage off */
  }

#if CONFIG_DEBUG
  sprintf(debugstrbuf, "MAXDMG=%d", sub); message_showdebug(debugstrbuf);
#endif

  if ( (sub == 1) && (!creature_is_ill(aggressor)) && (__randpercent(10)) )
    sub = 2;
  else
  { unsigned char f = 4;
    if (aggressor == rogue) f |= 1;
    else if (defender == rogue) f |= 2;
    sub = randdamage(sub, f);
  }

#if CONFIG_DEBUG
  sprintf(debugstrbuf, "DMG=%d", sub); message_showdebug(debugstrbuf);
#endif

  creature_make_enemies(aggressor, defender);
  if ( (defender->napping) && (defender != rogue) )
  { tBoolean p = cond2boolean( (aggressor == rogue) && (can_see_defender) );
    creature_wake_up(defender, p); /* when it's attacked, it surely wakes up */
  }
  if ( (aggressor == rogue) && (creature_is_shopkeeper(defender)) )
    infuriate_shopkeeper(defender, falsE);
  defender_survived = creature_dec_health(defender, sub);
    /* FIXME: e.g. this call can produce a "the aquatar divides" messages which
       should appear _after_ the "you hit the aquatar" message! */

#if CONFIG_DEBUG
  sprintf(debugstrbuf, "DEFH=%d", defender->health);
  message_showdebug(debugstrbuf);
#endif

  if (aggressor == rogue)
  { roguedata.flags |= rfHits | rfTickHits;
      /* CHECKME: move this to top of function? */
    roguedata.digestcount += 2;
    if (!defender_survived) rogue_incexp(1);
    if ( (!defender_survived) && (can_see_defender) )
    { sprintf(strbuf, "%s defeated ", rogue_pronoun(0));
      creature_get_name(defender, strbuf + strlen(strbuf), 1);
    }
    else
    { sprintf(strbuf, "%s hit ", rogue_pronoun(0));
      if (can_see_defender)
        creature_get_name(defender, strbuf + strlen(strbuf), 1);
      else strcat(strbuf, strSomething);
    }
    message_show(strbuf);
  }
  else if (defender == rogue)
  { roguedata.flags |= rfIsHit | rfTickIsHit; roguedata.digestcount++;
    if (map_can_player_see(aggressor->x, aggressor->y, mfCreature))
      creature_get_name(aggressor, strbuf, 1);
    else
    { strcpy(strbuf, strSomething);
      /* IMPLEMENTME: e.g. show a gray "i" ("invisible") on the map temporarily
         where the presumed aggressor is at this moment! */
    }
    strcat(strbuf, strSpaceHits); message_show(strbuf);
    if ( (pref_role == roleQuantumMech) && (__randpercent(95 - 18*difficulty)))
    { /* quantum mechanics are a very confusing topic :-) */
      creature_confuse(aggressor, 10 + 3 * randbytemod(7 - difficulty));
    }
  }
  else if ( (map_can_player_see(aggressor->x, aggressor->y, mfCreature)) &&
            (can_see_defender) )
  { creature_get_name(aggressor, strbuf, 1);
    strcat(strbuf, strSpaceHits); strcat(strbuf, strSpace);
    creature_get_name(defender, strbuf + strlen(strbuf), 1);
    message_show(strbuf);
  }

  if (weapon)
  { weapon->lifetime -= aaHitting; creature_check_object(aggressor, weapon); }

  if ( (armor) && (!(armor->flags & ofProtected)) )
  { armor->lifetime -= aaBeingHit; creature_check_object(defender, armor); }

  /* IMPLEMENTME: other creatures (if any) in the same room (if in any) should
     wake up due to the fight noise - depending on chance, of course... Call
     noise_caused_by(). */

  finish:
  if (!defender_survived)
  { if (defender == rogue) rogue_died(drFight, aggressor);
    else if (creature_died(defender, drFight, aggressor))
    { if ( (aggressor == rogue) && (defender->cc == ccLeprechaun) &&
           (randpercent(50 - 10 * difficulty + roguedata.magic_exp)) )
      { /* In this very special case, some "treasures" in the current section
           shall appear on the map (if in a chest, break the chest!) - somewhat
           following the Irish folklore that a leprechaun reveals the location
           of treasures when being caught. */
        if (section_reveal_objects()) message_show("wow!");
      }
    }
  }
}

tCreature* location2creature(tCoordinate x, tCoordinate y)
{ tCreature* c;
  for_each_currentsectioncreature(c)
  { if ( (!(c->flags & cfDead)) && (c->x == x) && (c->y == y) ) return(c); }
  if ( (rogue->x == x) && (rogue->y == y) ) return(rogue);
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "BUG: location2creature(%d,%d)", x, y);
  message_showdebug(debugstrbuf);
#endif
  return(NULL);
}

tBoolean creature_same_room(const tCreature* c1, const tCreature* c2)
{ const tRoomIndex r1 = creature_room(c1);
  return(cond2boolean((r1 != roomNone) && (r1 == creature_room(c2))));
}

tBoolean creature_same_room_xy(const tCreature* creature, const tCoordinate x,
  const tCoordinate y)
{ const tRoomIndex r = location2room(x, y);
  return(cond2boolean((r != roomNone) && (r == creature_room(creature))));
}

static one_caller tPropertyExtent basic_quietness(void)
{ tPropertyExtent retval = 0;
  const tObject *ring, *boots, *armor;
  for_each_worn_ring(ring)
  { if (ring->kind == rkQuiet) retval += zero2one(ring->enchantment); }
  if ( ( (boots = roguedata_os(rdosiBoots)) != NULL ) &&
      (boots->kind == tokBootsQuiet) )
  { retval += zero2one(boots->enchantment); }
  if ( ((armor = rogue->armor) != NULL) && (ARMOR_IS_METAL(armor->kind)) )
    retval -= 2; /* worn metal armor makes noise */
  if (pref_role == roleThief) retval += 3; /* thieves are steal(th)y */
  return(retval);
}

void creatures_perform_tick(void)
{ const tPropertyExtent quietness = basic_quietness(), q3 = 3 * quietness,
    _qval = 25 - q3, qval = MAX(0, _qval);
  const tBoolean may_reg = cond2boolean( (dungeonclock % 3) == 0 );
  tCreature* creature;
  for_each_currentsectioncreature(creature)
  { const tCreatureFlags cflags = creature->flags;
    tCreatureClass cc;
    tFewTicks napping;
    tCoordinate x, y;
    if (cflags & cfDead) goto next_c;
    if ( (cflags & cfRegenerates) && (may_reg) &&
      (creature->health < creature_defaulthealth(creature)) )
    { creature->health++; }
    x = creature->x; y = creature->y;
    if ( (cc = creature->cc) == ccVampire )
    { if (map_br(x, y) >= 50 + 40 * difficulty)
      { if (creature->cd > 1) creature->cd--;
        else if (creature_died(creature, drLight, NULL)) goto next_c; /*died*/
      }
    }
    if (squareflags_get(x, y) & sfFire)
    { const tHealth sub = creature_fire_sensitivity(creature);
      if ( (sub > 0) && (!creature_dec_health(creature, sub)) )
      { if (creature_died(creature, drFire, NULL)) goto next_c; } /* died */
      /* IMPLEMENTME: burn pack! */
    }
    /* CHECKME: call unblind(), unconfuse() etc.? (Currently doesn't make any
       difference, but...) */
    if (creature->_blind) creature->_blind--;
    if (creature->confused) creature->confused--;
    if (creature->hallucinating) creature->hallucinating--;
    if (creature->slowedhasted) creature_decslowedhasted(creature);
    napping = creature->napping;
    if (napping == NAPKIND_DEEPSLEEP) { /* nothing, let it sleep */ }
    else if (napping == NAPKIND_SLEEP) /* maybe should wake up */
    { tCoordinate dista, distb;
      tPercentage percentage;
      if (!creature_same_room(rogue, creature))
        goto sleepcheck_done; /* no player character, thus no reason :-) */
      dista = ABS(rogue->x - x); distb = ABS(rogue->y - y);
      if (dista * dista + distb * distb > qval)
        goto sleepcheck_done; /* player character is "too far away" */
      percentage = 8 * difficulty - rogue->experience - q3;
      if (percentage <= 0) percentage = 1; /* at least _some_ chance... */
      if (__randpercent(percentage)) creature_wake_up(creature, falsE);
    }
    else if (napping > 0)
    { if (napping == 1) creature_wake_up(creature, falsE);
      else creature->napping--;
    }
    sleepcheck_done: {}
    if (creature->levitated)
    { creature->levitated--;
      if (!creature_is_in_the_air(creature))
      { if (!creature_land(creature)) goto next_c; } /* died when landing */
    }
    if (creature->held) creature->held--;
    if (creature->frozen)
    { if (creature->frozen == 1) creature_unfreeze(creature);
      else creature->frozen--;
    }
    /* creature_tickout_recalc(creature, 1); */
    next_c: {}
  }
}

typedef struct
{ tObjectClass oc;
  tObjectKind kind;
} tPackSpec;

static one_caller void rsp_create(const tPackSpec* p)
{ const tObjectClass oc = p->oc;
  const tObjectKind kind = p->kind;
  tObject* const object = object_create(oc, kind, 2);
  tObjectQuantity q;
  switch (oc)
  { case ocGold:
      switch (pref_role)
      { case roleThief: q = 150; break;
        case roleTourist: q = 500; break;
        case roleTrader: q = 370; break;
        default: q = 0; break;
      }
      object->quantity = 400 + q + randbytemod(50);
      break;
    case ocArmor: object->enchantment = randbytemod(2) + 1; break;
    case ocWeapon:
      switch (kind)
      { case wkArrow:
          object->quantity = randbytefromrange(20, 30 - difficulty); break;
      }
      break;
    case ocPotion: potionkind_id[kind] = truE; break;
    case ocWandStaff:
      object->lifetime += 5000; wandstaffkind_id[kind] = truE; break;
    case ocScroll:
      switch (kind)
      { case skIdentify: object->lifetime += 10000; break;
      }
      scrollkind_id[kind] = truE; break;
    case ocRing: object->lifetime += 5000; ringkind_id[kind] = truE; break;
  }
  object->flags |= ofFullyIdentified; creature_add_object(rogue, object);
  switch (oc)
  { case ocArmor: creature_wear(rogue, object); break;
    case ocWeapon:
      if (rogue->weapon == NULL) creature_wield(rogue, object);
      break;
  }
  /* return(object); */
}

#define RSP(array) do { p = array; num = ARRAY_ELEMNUM(array); goto rsp; } while (0)
#define RSPTWO(val1, val2) ( (val1) | ((val2) << 5) )

static one_caller void rogue_setup_pack(void)
{ static const tPackSpec ps[] = { { ocArmor, akLeather },
    { ocArmor, akStuddedLeather }, { ocArmor, akRing }, { ocArmor, akChain },
    { ocWeapon, wkClub }, { ocWeapon, wkMace }, { ocWeapon, wkLongSword },
    { ocWeapon, wkBow }, { ocWeapon, wkArrow }, { ocGold, okNone },
    { ocFood, fkRation }, { ocFood, fkSolidWater }, { ocTool, tokCamera },
    { ocScroll, skIdentify }, { ocRing, rkTeleport }, { ocRing, rkHeal },
    { ocPotion, pkGreatlyHeal }, { ocWandStaff, wskRemove },
    { ocWandStaff, wskRemoteHit } };
  static const tUint16
    rsp_caveman[] = { 1, 4 }, rsp_knight[] = { 3, 6 },
    rsp_rogue[] = { RSPTWO(1, 2), 5 }, rsp_thief[] = { 0, 7, 8, 9 },
    rsp_tourist[] = { 9, 10, RSPTWO(10, 11), 12 }, rsp_trader[] = { 1, 9 },
    rsp_wizard[] = { 0, 13, 14, RSPTWO(15, 16), RSPTWO(17, 18) };
  const unsigned short* p;
  unsigned short val1, val2;
  unsigned char num, idx;
  switch (pref_role)
  { case roleCaveman: RSP(rsp_caveman);
      rsp: for (idx = 0; idx < num; idx++)
      { val1 = p[idx]; val2 = val1 >> 5;
        rsp_create(&(ps[((val2 > 0) && (randcointoss())) ? val2 : (val1&31)]));
      }
      break;
    case roleKnight: RSP(rsp_knight); break;
    case roleRogue: RSP(rsp_rogue); break;
    case roleThief: RSP(rsp_thief); break;
    case roleTourist: RSP(rsp_tourist); break;
    case roleTrader: RSP(rsp_trader); break;
    case roleWizard: RSP(rsp_wizard); break;
  }
  /* IMPLEMENTME: take selected species into account! */
  /* IMPLEMENTME: maybe roleRogue should additionally have bow and arrows, at
     least in easy gameplay difficulties! Or do something special if e.g.
     "pref_combat + pref_expl + pref_magic + pref_feat + 2 * difficulty" == 42;
     maybe depending on role. */
  roguedata.flags &= ~(rfInventoryChanged | rfInvPropertyChanged);
    /* (ignore uninteresting inventory changes which trivially happened with
        the above creature_add_object(rogue, ....) calls) */
}

#undef RSP
#undef RSPTWO

void rogue_create(void)
/* creates the player character and sets up data and inventory */
{ my_memclr_var(roguedata); rogue = creature_create(ccRogue, 1);
  roguedata.maxpackweight = OBJECT_WEIGHT_ROGUEINITMAX;
  roguedata.maxpackcount = 26 - 1 - difficulty - roledata.invsub;
  roguedata.maxhealth = rogue->health = rogue->health +
    2 * (roledata.healing - 2); /* don't change this, or check heal_rogue() */
  roguedata.maxstrength = rogue->strength = rogue->strength +
    2 * (roledata.relative_strength - 2);
  roguedata.actions_left = randfewticksfromrange(1100, 1300);
  roguedata.magic_energy = 15 - difficulty + randbytemod(5);
  if (pref_role == roleWizard)
  { roguedata.magic_energy += 20;
    roguedata.magic_exp = 20 - 3 * difficulty - randbytemod(3);
  }
  roguedata.endure_freeze = 100 - 10*difficulty + randfewticksfromrange(0, 10);
  roguedata.iron_ball_x = roguedata.iron_ball_y = coordNone;
  roguedata.state = rsNormal; rogue_setup_pack();
}

void rogue_died(const tDeathReason reason, /*@null@*/ const tCreature*
  aggressor)
{ static const char* const strDeathReason[maxDeathReason + 1] =
  { "for an unknown reason", "(internal)", "of starvation", "of poisoning",
    "of hypothermia", "in a fight", "of drowning", "of sinking in a swamp",
    "of an injury", "of burning", "of magic removal", "of polymorphing",
    "of too much light", "of silver", "of petrifying" };
    /* (The "(internal)" reason never occurs here - drInternal is only for
        creature_died() calls!) */
  tPropertyExtent rebirth = 0;
  tObject* ring;
  char* s;

  /* Make sure the final screen is up-to-date */

  statistics_update(); map_do_update_all();

  /* Print the death message */

  s = strbuf;
  s += sprintf(s, "%s died %s", rogue_pronoun(0), strDeathReason[reason]);
  if ( (reason == drFight) && (aggressor != NULL) )
  { s += sprintf(s, " with "); creature_get_name(aggressor, s, 2); }
  message_show(strbuf); roguedata.rds.deathcount++;

  /* Check for rebirth */

  if ( (aggressor != NULL) && (aggressor->cc == ccShopkeeper) &&
    (rogue_in_shop) && (difficulty >= dkNormal) )
  { goto no_rebirth; } /* rebirth shall be impossible */
  for_each_worn_ring(ring)
  { tPropertyExtent ench;
    if (ring->kind != rkRebirth) continue;
    ench = ring->enchantment;
    rebirth += ench;
    /* Rebirth costs some of the energy of the ring(s): */
    if (ench > 0) ring->enchantment--;
    if (ench >= 0)
    { ring->lifetime -= 1000;
      if (ring->lifetime < 1) ring->lifetime = 3;
        /* (only to avoid inventory handling inside the for_each_worn_ring()
            loop, which could cause trouble -- CHECKME!) */
    }
  }
  if (rebirth > 0)
  { creature_perform_rebirth(rogue, reason, rebirth);
    sprintf(strbuf, "%s %s reborn", rogue_pronoun(0), rogue_verb());
    message_show(strbuf); _do_tick = truE; return; /* done */
  }
  /* IMPLEMENTME: if the player character has a usable wskDrainLife, try to
     drain some life from creatures in the neighborhood? -- What if he has an
     Amulet? */
  if (rogue->cd > 0) /* top-secret feature! :-) */
  { rogue->cd--; creature_perform_rebirth(rogue, reason, 0);
    message_show("beginning a new life"); _do_tick = truE; return; /* done */
  }
  no_rebirth: message_die(); /* no kind of rebirth possible */
  /*@notreached@*/
}

void rogue_checkactionsleft(void)
/* Call this _always_ after changing roguedata.actions_left (except from
   rogue_create() - not necessary then). */
{ static tFewTicks oldleft = -1;
  tRogueState new_state;
  const tFewTicks al = roguedata.actions_left;
  if (al > 300) new_state = rsNormal;
  else if (al > 150) new_state = rsHungry;
  else if (al > 50) new_state = rsWeak;
  else if (al > 0) new_state = rsFaint;
  else new_state = rsStarve;
  if (roguedata.state != new_state)
  { roguedata.state = new_state; statistics_invalidate(svfState);
    if ( (oldleft > al) && ( (new_state == rsHungry) || (new_state == rsWeak)))
    { /* Tell the player that the state became worse. (We don't care about
         rsFaint here - the player gets "fainting" messages then anyway.) */
      sprintf(strbuf, "%s feel %s", rogue_pronoun(0),
        ( (new_state == rsHungry) ? "hungry" : "weak" ));
      message_show(strbuf); roguedata.flags |= rfStateChanged;
    }
  }
  oldleft = al;
  if (new_state == rsStarve) rogue_died(drStarvation, NULL);
}

void rogue_decactionsleft(const tFewTicks num)
{ if (roguedata.actions_left > num) roguedata.actions_left -= num;
  else roguedata.actions_left = 0;
  rogue_checkactionsleft();
}

void rogue_incexp(tExperience exp)
/* increases the player character's experience by <exp> and possibly the
   maxhealth and maxstrength values (all up to some maximum values...) */
{ tExperience old_exp = rogue->experience, new_exp = old_exp + exp;
  if (new_exp > 99) new_exp = 99;
  if (old_exp != new_exp)
  { tStatisticsValidityFlags svf = svfExperience;
    tExperience temp = old_exp;
    rogue->experience = new_exp;
    while (++temp <= new_exp)
    { if ( (temp % 5) == 0 )
      { /* Increase either the maxhealth or maxstrength value; note that we
           don't care if the player character had already reached the current
           experience some time in the past (and decreased by shfDecExp attacks
           and re-increased); this can even be used as a little trick: just get
           attacked by a cfDecExp creature from time to time and re-gain
           experience to increase maxhealth or maxstrength - but don't tell
           anybody! :-) */
        if ( (temp % 10) == 0 )
        { tStrength s = roguedata.maxstrength + 2 + new_exp / 20;
          if (s > 99) s = 99;
          if (roguedata.maxstrength < s)
          { roguedata.maxstrength = s; svf |= svfMaxStrength; }
        }
        else
        { tHealth h = roguedata.maxhealth + 2 + new_exp / 20;
          if (h > 99) h = 99;
          if (roguedata.maxhealth < h)
          { roguedata.maxhealth = h; svf |= svfMaxHealth; }
        }
      }
    }
    statistics_invalidate(svf);
  }
}

void rogue_incmagicexp(tMagicExp exp)
/* increases the magic experience of the player character */
{ exp = roguedata.magic_exp + exp;
  if (exp > 99) exp = 99;
  if (roguedata.magic_exp < exp)
  { roguedata.magic_exp = exp; statistics_invalidate(svfMagicExp); }
}

void rogue_decmagicexp(tMagicExp exp)
/* decreases the magic experience of the player character */
{ exp = roguedata.magic_exp - exp;
  if (exp < 0) exp = 0;
  if (roguedata.magic_exp > exp)
  { roguedata.magic_exp = exp; statistics_invalidate(svfMagicExp); }
}

void rogue_incmagicen(const tMagicEnergy e0)
{ tMagicEnergy orig_e = roguedata.magic_energy, e = orig_e + e0;
  if (e > 99) e = 99;
  if (orig_e != e)
  { roguedata.magic_energy = e; statistics_invalidate(svfMagicEnergy); }
}

void rogue_decmagicen(const tMagicEnergy e0)
{ tMagicEnergy orig_e = roguedata.magic_energy, e = orig_e - e0;
  if (e < 0) e = 0;
  if (orig_e != e)
  { roguedata.magic_energy = e; statistics_invalidate(svfMagicEnergy); }
}

tBoolean rogue_dazzle(const tBoolean must)
/* Call this normally only if "rogue_can_see"! */
{ tBoolean do_dazzle = truE;
  if (!must) /* look for helpful glasses */
  { const tObject* const object = roguedata_os(rdosiGlasses);
    if ( (object != NULL) && (object->kind == tokGlassesDark) &&
      (object->enchantment >= 0) )
    { do_dazzle = falsE; }
  }
  if (do_dazzle)
  { roguedata.flags |= rfIsDazzled | rfTickIsDazzled;
    creature_blind(rogue, 2 + difficulty / 2 + randbytemod(2 + difficulty));
    if (__randpercent(19 * difficulty + 1))
      creature_confuse(rogue, MAX(1, difficulty) + randbytemod(3));
  }
  else
  { message_show("the glasses become dark for a moment");
    canseeagainmsg(falsE);
  }
  return(do_dazzle);
}

tPercentage rogue_rings2fortune(const tPercentage factor)
{ tPercentage retval = 0;
  const tObject *ring, *o;

  /* check rings */
  for_each_worn_ring(ring)
  { if (ring->kind == rkFortune) retval += factor * ring->enchantment; }

  /* check Amulet */
  for_each_object(rogue->objects, o)
  { if (o->oc == ocAmulet)
    { tPropertyExtent ench = o->enchantment / 2;
      if (ench > 20) ench = 20;
      else if (ench < -20) ench = -20;
      retval += ench;
      break;
    }
  }

  return(retval);
}

static tPercentage rogue_rings2hitchance(const tPercentage factor)
{ tPercentage retval = 0;
  const tObject* ring;
  for_each_worn_ring(ring)
  { if (ring->kind == rkFightFortune) retval += factor * ring->enchantment;
    else if (ring->kind == rkFortune) retval += (factor * ring->enchantment)/2;
    /* (The "specific" fight-fortune rings apply with the full factor here; the
        "more general" fortune rings apply with half the factor.) */
  }
  /* IMPLEMENTME: amulets? */
  return(retval);
}

void cleanse_price_tags(void)
{ tObject* object = rogue->objects;
  tBoolean did = falsE;
  while (object != NULL)
  { tObject* const next = object->next;
    const tPriceTag p = object->price_tag;
    if (p != 0) /* rare */
    { if (p < 0) { object->price_tag = 0; did = truE; } /* forget the offer */
      else /* silently remove "stolen" objects */
      { creature_lose_object(rogue, object); object_deallocate(object); }
    }
    object = next;
  }
  if (did) roguedata.flags |= rfInvPropertyChanged;
}

tBoolean _rogue_in_shop(void)
{ const tRoomIndex r = creature_room(rogue);
  return(cond2boolean( (r != roomNone) && (current_section->room[r].kind ==
    rokShop) ));
}

tBoolean _rogue_has_unpaid_items(void)
{ tBoolean retval;
  const tObject* object;
  for_each_object(rogue->objects, object)
  { if (object_is_unpaid(object)) { retval = truE; goto out; } }
  retval = falsE;
  out: return(retval);
}

const char* _role_name(const tRole role, const tCreatureGenderKind gender,
  const tBoolean plural)
{ static char buf[20];
  const char* retval = _roledata[role].name; /* default */
  switch (role) /* handle a few special cases */
  { case roleCaveman:
      sprintf(buf, "cave%sm%cn", ( (gender == cgkFemale) ? "wo" :
        strEmpty ), (plural ? 'e' : 'a'));
      retval = buf; goto out; /*@notreached@*/ break;
    case roleThief:
      if (plural) retval = "thieves";
      goto out; /*@notreached@*/ break;
  }
  if (plural) { strcpy(buf, retval); strcat(buf, strS); retval = buf; }
  out: return(retval);
}

static void say_role_cant(const char* what)
{ sprintf(strbuf, "%s can't %s", role_name(truE), what); message_show(strbuf);
}

tBoolean rogue_can_read(void)
{ tBoolean retval;
  if (pref_role == roleCaveman)
  { say_role_cant("read"); failed: retval = falsE; goto out; }
  if (!rogue_can_see)
  { message_show("can't read while being blind"); goto failed; }
  retval = truE;
  out: return(retval);
}

tBoolean rogue_can_write(void)
{ tBoolean retval;
  if (pref_role == roleCaveman)
  { say_role_cant("write"); retval = falsE; goto out; }
  /* CHECKME: can't write if blind, at least in higher gameplay difficulties?
     Or mis-spells random parts if blind? */
  retval = truE;
  out: return(retval);
}

void rogue_las(const tCreatureActionKind cak, tCreatureActionSpeed cas)
{ if (cak == cakMove)
  { /* IMPLEMENTME: if (affected by an iron ball or loadstones) cas -= 3; */ }
  else if (cak_magic(cak))
  { if ( (pref_role == roleWizard) && (cas < 3) ) cas = 3; }
  _creature_last_action_set(rogue, cak, cas); roguedata.cakcount++;
  _do_tick = truE;
}

tBoolean rogue_has_objectclasses(const tObjectClassesBitmask classes)
/* checks whether the player character has objects of the given classes */
{ const tObject* object;
  for_each_object(rogue->objects, object)
  { if (is_invobject_interesting(object)) return(truE); }
  return(falsE);
}

/*@null@*/ tObject* _rogue_aoy_find(void)
{ tObject *retval = NULL, *object;
  tSint8 best_value = -100, value;
  for_each_object(rogue->objects, object)
  { tObjectDetail detail;
    if ( (object->oc != ocAmulet) || (object->kind != amkYendor) ) continue;
    detail = object->objdetail; value = -3 * amulet_misscount(detail);
    if (!AMULET_FAKE(detail)) value += 30;
    if (object->enchantment > 0) value++;
    if (best_value < value) { best_value = value; retval = object; }
  }
  return(retval);
}

void __init creature_initialize(void)
{ roledata = _roledata[pref_role]; /* get a modifiable copy */
  if (pref_gender == cgkFemale) /* make the game more realistic :-) */
  { if (roledata.relative_strength > 1) roledata.relative_strength--;
    roledata.invsub++; roledata.interval += 50;
    if (roledata.fightskill > 1) roledata.fightskill--;
    if (roledata.healing < 2) roledata.healing++;
    if ( (roledata.luck < 2) && (randcointoss()) ) roledata.luck++;
    if (roledata.mental_power < 2) roledata.mental_power++;
  }
  /* IMPLEMENTME: take selected species into account! */
}
