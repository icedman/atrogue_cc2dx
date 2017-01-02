/* atrogue/action.c - creature actions ("artificial intelligence")
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "creature.h"
#include "dungeon.h"
#include "random.h"
#include "message.h"
#include "action.h"

static tBoolean action_nonmove(tCreature* creature)
/* lets the creature perform a non-move action (hit) concerning adjacent
   locations */
{ tBoolean retval = falsE, can_control = cond2boolean(!creature->confused);
  tDirection dir0;
  tCoordinate x, y;

  if ( (can_control) && (creature->aggr < 1) ) goto out; /* don't hit at all */

  x = creature->x; y = creature->y;
  for_each_randdir(dir0)
  { const tCoordinate x2 = x + xdir[dir0], y2 = y + ydir[dir0];
    tSquareFlags sflags2;
    tCreature* c;
    if (!is_location_valid(x2, y2)) continue;
    sflags2 = squareflags_get(x2, y2);
    if (sflags2 & sfBarrier) continue;
    if (!(sflags2 & sfCreature)) continue; /* noone there */
    if ( (is_diagdir(dir0)) &&
         (!creature_can_act_diagonally(creature, x2, y2)) )
    { continue; }
    c = location2creature(x2, y2);
    if (c == NULL) continue; /* "should not happen" */
    if (c->flags & cfImmaterial) continue; /* cannot actually hit it anyway */
    if (can_control && (!creature->_blind) && (!creature_enemies(creature, c)))
      continue; /* don't hit friends intentionally */
    if ( (c == rogue) && (difficulty <= dkNormal) && (creature_is_ill(c)) &&
      (__randpercent(90 - 20 * difficulty)) )
    { continue; } /* be less aggressive in easier difficulties */
    creature_hit(creature, c); creature_last_action_set(creature, cakHit, 0);
    retval = truE; break; /* done */
  }
  out: return(retval);
}

static tBoolean scary_object(const tObject* object)
{ tBoolean retval = falsE; /* the most likely result */
  switch (object->oc)
  { case ocAmulet: yup: retval = truE; break;
    case ocTool:
      if ( (object->kind == tokPhotograph) && (object->enchantment >= 0) )
      { const tObjectDetail detail = object->objdetail;
        if (photo_kind(detail) == phkObject)
        { tObjectClass oc;
          tObjectKind kind;
          photobjback(detail, oc, kind);
          if (oc == ocAmulet) goto yup; /* any amulet kind shall work here */
          else if ( (oc == ocTool) && (kind == tokWhistleExpel) ) goto yup;
        }
      }
      break;
    case ocScroll:
      if ( (object->kind == skScare) && (object->enchantment >= 0) ) goto yup;
      break;
    case ocRing:
      if ( (object->kind == rkFear) && (object->enchantment >= 0) ) goto yup;
      break;
  }
  return(retval);
}

static tBoolean scary_field(const tCoordinate x, const tCoordinate y)
{ if ( (x ^ y) == 42 ) { }
  /* IMPLEMENTME: check engraving! */
  return(falsE);
}

#define TMOUT(dummy) goto out

static tBoolean try_move(tCreature* creature, const tDirection dir,
  const tBoolean enforced)
/* tries to move the creature in the given direction; returns whether that was
   possible */
{ tBoolean retval = falsE, can_control, in_air, is_diag;
  const tDirectionOffset dx = xdir[dir], dy = ydir[dir];
  const tCoordinate x = creature->x, y = creature->y, x2 = x + dx, y2 = y + dy;
  tSquareFlags sflags2;
  const tObstacle* obstacle;
  if (!is_location_valid(x2, y2)) TMOUT(1);
  sflags2 = squareflags_get(x2, y2);
  if (! (sflags2 & (sfRoom | sfTunnel | sfDoorway)) ) TMOUT(2); /* no way */
  is_diag = cond2boolean(is_diagdir(dir));
  if ( (is_diag) && (!creature_can_act_diagonally(creature, x2, y2))) TMOUT(3);
  if (sflags2 & sfBarrier)
  { TMOUT(4);
    /* FIXME: if (enforced) { decrease health; retval = truE; goto out; } */
  }
  if (sflags2 & sfCreature)
  { tCreature* c2;
    if (!enforced) TMOUT(5);
    /* hit that creature (ramming it due to gliding) */
    c2 = location2creature(x2, y2);
    if (c2 != NULL) /* "should" be true */
    { creature_hit(creature, c2); creature_last_action_set(creature, cakHit,0);
      retval = truE; goto out;
    }
  }

  in_air = creature_is_in_the_air(creature);
  if ( (sflags2 & sfObstacle) && ( (obstacle = location2obstacle(x2, y2)) !=
    NULL ) )
  { const tObstacleSize effsize = obstacle_effsize(obstacle);
    if (effsize <= 0) goto no_obstacle;
    if ( (!in_air) || (effsize >= 12) ) TMOUT(6); /* can't pass the obstacle */
  }
  no_obstacle: {}

  if (enforced) goto do_move; /* no choice */

  can_control = cond2boolean(!creature->confused);
  if (can_control)
  { if (sflags2 & sfFire) TMOUT(7);
    if ( (!in_air) && (sflags2 & sfFatalGround) && (!(sflags2 & sfPlatform)) )
      TMOUT(8);
    /* Check whether there's something "scaring" on the square: */
    if (sflags2 & sfObject)
    { const tObject* const o = location2object(x2, y2);
      if ( (o != NULL) && (scary_object(o)) ) TMOUT(9);
    }
    if (scary_field(x2, y2)) TMOUT(10);
    if ( (sflags2 & sfGlazedFrost) && (!in_air) && (!creature->_blind) )
    { /* gliding can be dangerous, so the creature looks ahead before it starts
         that; FIXME: creatures can't always look that far, e.g. in a tunnel */
      tBoolean go_there = truE;
      tCoordinate x3 = x2, y3 = y2;
      while (1)
      { tSquareFlags sflags3;
        x3 += dx; y3 += dy;
        if (!is_location_valid(x3, y3)) break;
        sflags3 = squareflags_get(x3, y3);
        if (! (sflags3 & (sfRoom | sfTunnel | sfDoorway)) ) break;
        if ( (is_diag) && (!__creature_can_act_diagonally(creature, x3 - dx,
             y3 - dy, x3, y3)) )
          break;
        if (sflags3 & sfBarrier)
        { if ( (creature->aggr == 0) || (creature->health < 3) )
            go_there = falsE; /* don't risk an injury */
          break;
        }
        if ( /* (!in_air) && */ (sflags3 & sfFatalGround) &&
             (!(sflags3 & sfPlatform)) )
        { go_there = falsE; break; }
        /* CHECKME: even if there's _currently_ an sfPlatform, this might
           vanish (e.g. thaw) before the creature reaches that location, and
           there might be fatal ground below the former sfPlatform, so this
           decision is dangerous. Etc. */
        if (!(sflags3 & sfGlazedFrost)) break; /* don't glide further */
      }
      if (!go_there) TMOUT(11); /* would be dangerous */
    }
  }

  /* It's all right, so do the move: */
  do_move:
  creature_moveabs(creature, x2, y2, dir);
  creature_last_action_set(creature, cakMove, 0); retval = truE;
  out:
  return(retval);
}

#undef TMOUT

#define GLIDEOUT(dummy) goto out

static tBoolean action_glide(tCreature* creature)
/* lets the creature glide over sfGlazedFrost */
{ tBoolean retval = falsE;
  tDirection dir;
  if (!(squareflags_get(creature->x, creature->y) & sfGlazedFrost))
    GLIDEOUT(1);
  if (creature_is_in_the_air(creature)) GLIDEOUT(2);
  dir = creature->lastdir;
  if (dir == dirNone) GLIDEOUT(3);
  if (!try_move(creature, dir, truE)) GLIDEOUT(4);
  retval = truE;
  out:
  return(retval);
}

#undef GLIDEOUT

static tBoolean move_towards_dir(tCreature* creature, const tDirection dir)
/* tries to move towards the given direction */
{ tBoolean retval = falsE;
  if (dir != dirNone)
  { tDirection dir0;
    for_each_deviationdir_full(dir, dir0)
    { if (try_move(creature, dir0, falsE)) { retval = truE; break; } }
  }
  return(retval);
}

static tBoolean move_towards(tCreature* creature, const tCoordinate tx,
  const tCoordinate ty, const tBoolean run_away)
/* tries to move towards (or away from) the given coordinates */
{ tBoolean retval = falsE;
  tDirection dir = diroffsets2dir(SIGN(tx - creature->x),
    SIGN(ty - creature->y));
  if (dir != dirNone)
  { if (run_away) dir = counterdir[dir];
    if (move_towards_dir(creature, dir)) retval = truE;
  }
  return(retval);
}

static tBoolean action_move1(tCreature* creature)
/* lets the creature perform a move (concerning confusion and flitting) */
{ if (creature->confused)
  { tDirection dir0;
    random_move:
    for_each_randdir(dir0)
    { if (try_move(creature, dir0, falsE)) return(truE); }
    goto out;
  }
  if ( (creature->flags & cfFlitsMov) &&
       (squareflags_get(creature->x, creature->y) & sfRoom) )
  { const tCreatureClass cc = creature->cc;
    if (cc == ccBat)
    { if (__randpercent(40 + 3*difficulty*cond2sign(creature->flags & cfPro)))
        goto random_move;
    }
    else if (cc == ccPhantom)
    { if (__randpercent(20 + 2*difficulty*cond2sign(creature->flags & cfPro)))
        goto random_move;
    }
    /* In these cases, we get the usual confused behavior apart from the fact
       that can_control == truE here; that's exactly what we want. */
    /* FIXME: do this not only in sfRoom, but everywhere; it's currently
       restricted to sfRoom - it would otherwise break calculations which are
       based on ->lastdir because that is overwritten with a random
       direction. */
  }
  /* IMPLEMENTME: let ccVampire leave rliUnlimited as fast as possible! */
  out: return(falsE);
}

static tBoolean action_move2(tCreature* creature)
/* lets the creature perform a move (concerning enemies) */
{ tBoolean retval = falsE, run_away;
  const tCreature *c, *tc;
  const tCoordinate x = creature->x, y = creature->y;
  tCoordinate mindistance;
  tAggressivity aggr = creature->aggr;

  /* IMPLEMENTME: if ( (creature->flags & cfSeeksGold) && (creature can see
     some gold) ) { go to the gold location }
     This way, the player can divert such enemies by throwing gold somewhere */

  if (aggr == 0) goto out; /* don't care about other creatures at all */
  run_away = cond2boolean(aggr < 0);

  /* Look for the worst/nearest enemy */

  if ( (!(creature->flags & cfPro)) &&
       (creature_can_see_creature(creature, rogue, truE)) )
  { const tCoordinate rx = rogue->x, ry = rogue->y;
    const tSquareFlags rsf = squareflags_get(rx, ry);
    if ( (rsf & sfDoorway) && (squareflags_get(x, y) & (sfRoom | sfDoorway)) )
    { /* Make sure the creature follows the player character if he leaves the
         room. This is some sort of (not-so-evil:-) hack, so CHECKME! */
      creature->tx = rx; creature->ty = ry;
    }
    if (move_towards(creature, rx, ry, run_away)) { retval = truE; goto out; }
  }

  tc = NULL; mindistance = HIGHEST_ALLOWED_NUMBER;
  for_each_currentsectioncreature(c)
  { tCoordinate dista, distb, distance;
    if (c->flags & cfDead) continue;
    if (c == creature) continue; /* don't run towards (or away from) yourself*/
    if (!creature_enemies(creature, c)) continue;
    if (!creature_can_see_creature(creature, c, truE)) continue;
    dista = ABS(c->x - x); distb = ABS(c->y - y);
    distance = dista * dista + distb * distb;
    if (mindistance > distance) { mindistance = distance; tc = c; }
  }
  if ( (tc != NULL) && (move_towards(creature, tc->x, tc->y, run_away)) )
    retval = truE;
  out: return(retval);
}

static tBoolean action_move3(tCreature* creature)
/* lets the creature perform a move (concerning "longer-term, non-urgent tours"
   like traversal of empty rooms) */
{ tBoolean retval = falsE;
  tCoordinate tx = creature->tx, ty = creature->ty;
  if (tx == coordNone) /* no target is specified, calculate one */
  { const tCoordinate x = creature->x, y = creature->y;
    if (!(squareflags_get(x, y) & sfRoom))
    { tDirection dir;
      use_lastdir:
      dir = creature->lastdir;
      if (dir == dirNone) goto random_move;
      retval = move_towards_dir(creature, dir);
      goto finish;
    }
    section_randdoorway(x, y, &tx, &ty);
    if (tx == coordNone) goto use_lastdir; /* CHECKME! */
    if ( (tx == x) && (ty == y) ) goto use_lastdir; /* CHECKME! */
    creature->tx = tx; creature->ty = ty; /* store new target */
#if CONFIG_EXDEBUG
    sprintf(debugstrbuf, "NEWT(cc=%d,r=%d,x=%d,y=%d,tx=%d,ty=%d)",
      creature->cc, r, x, y, tx, ty);
    message_showdebug(debugstrbuf);
#endif
  }
  if (move_towards(creature, tx, ty, falsE)) retval = truE;

  finish:
  if (retval == falsE)
  { /* All the above didn't work; probably the current target is rubbish... */
    tDirection dir0;
    creature->tx = coordNone;
    random_move:
    for_each_randdir(dir0)
    { if (try_move(creature, dir0, falsE)) { retval = truE; break; } }
  }
  return(retval);
  /* IMPLEMENTME: if the creature couldn't approach its target coordinates
     noticeably for e.g. 30 ticks, calculate a new target. This can e.g. happen
     when a non-enemy is sleeping directly in front of a doorway. */
}

#define __TEST_REMOTE(c) \
  ( (creature_enemies(creature, c)) && \
    (creature_can_see_creature(creature, c, falsE)) && \
    (creature_can_affect_location(creature, c->x, c->y)) )

#define TRY_RSH(c) \
  ( (__TEST_REMOTE(c)) && \
    (creature_specialhit(creature, c, falsE) & afActuallyHit) )

static tBoolean action_rsh(tCreature* creature)
/* lets the creature perform a remote special hit (_adjacent_ special hits are
   handled in action_nonmove()/creature_hit() instead) */
{ tBoolean retval = falsE;
  tCreature* c;
  if (creature->confused | creature->hallucinating) goto out; /* CHECKME! */
  if (TRY_RSH(rogue))
  { did_rsh: creature_last_action_set(creature, cakOther, 0); retval = truE;
    goto out;
  }
  for_each_currentsectioncreature(c)
  { if (c->flags & cfDead) continue;
    if (c == creature) continue; /* don't hit yourself */
    if (TRY_RSH(c)) goto did_rsh;
  }
  out: return(retval);
}

static tBoolean action_throw(tCreature* creature)
/* lets the creature throw something; CHECKME: only allow this for humanoid
   creatures? */
{ tBoolean retval = falsE;
  tStoreyNumber storey;

  if (creature->objects == NULL) goto out; /* can't throw anything (likely) */
  if (difficulty < dkNormal) goto out;
  if (creature->confused | creature->hallucinating) goto out; /* CHECKME! */

  storey = current_storeynum;
  if ( (storey > 6 - difficulty) && (__TEST_REMOTE(rogue)) &&
       (__randpercent(1 + difficulty + storey / 5)) )
  { tObject *const weapon = creature->weapon, *object = NULL, *o;
    tDamage maxvalue = 0, value;
    const tDirection dir = diroffsets2dir(SIGN(rogue->x - creature->x),
      SIGN(rogue->y - creature->y));
    if (dir == dirNone) goto out; /* "should not happen" */
    for_each_object(creature->objects, o)
    { if (o->flags & ofInUse) continue;
      if ( (weapon != NULL) && (object_weapon_combinable(weapon, o)) )
      { value = 11 + o->enchantment; goto handle_value; } /* best case */
      switch (o->oc)
      { case ocStone: value = 4 + o->kind; break;
        case ocWeapon: value = 3; break;
        case ocPotion: value = 2; break;
        case ocWandStaff:
          if ( (!(creature->shf & shfZaps)) || (o->lifetime<1000) ) value = 1;
          else continue;
          break;
        default: continue; /*@notreached@*/ break;
      }
      handle_value: if (maxvalue < value) { maxvalue = value; object = o; }
    }
    if (object != NULL) /* found something, so throw it */
    { creature_throw(creature, object, 1, dir);
      creature_last_action_set(creature, cakThrow, 0); retval = truE;
    }
  }
  out: return(retval);
}

#undef TRY_RSH
#undef __TEST_REMOTE

static tBoolean action_drop(tCreature* creature)
{ tBoolean retval = falsE;
  tObject* object;
  for_each_object(creature->objects, object)
  { if (object->flags & ofInUse) continue;
    if (scary_object(object))
    { creature_drop_object(creature, object, object->quantity, falsE);
      creature_last_action_set(creature, cakDrop, 0); retval = truE; goto out;
    }
  }
  /* IMPLEMENTME further; esp. a pet should drop any object which it picked up
     accidentally. */
  out: return(retval);
}

static tBoolean _pick_eat(tCreature* creature, tObject* object)
{ tBoolean retval;
  if ( (creature->flags & cfMetallivore) && (object_mainmat(object)&bomMetalN))
  { section_forget_object(object); object_deallocate(object);
    creature_last_action_set(creature, cakEat, 0);
    did: retval = truE;
  }
  else if (creature_can_carry_object(creature, object, 0))
  { section_forget_object(object); (void) creature_add_object(creature,object);
    creature_last_action_set(creature, cakPick, 0); goto did;
  }
  else retval = falsE;
  /* IMPLEMENTME: show a message (if player character can see this action)! */
  return(retval);
}

static tBoolean action_pick_eat(tCreature* creature)
{ tBoolean retval = falsE;
  const tCoordinate x = creature->x, y = creature->y;
  if (squareflags_get(x, y) & sfObject) /* some object here; unlikely */
  { tObject* const object = location2object(x, y);
    /* IMPLEMENTME: if (creature in shop) goto out; */
    if ( (object == NULL) || (scary_object(object)) ) goto out;
    if (__randpercent(1 + 5 * difficulty)) retval = _pick_eat(creature,object);
  }
  out: return(retval);
}

static tBoolean action_armor(tCreature* creature)
/* lets a humanoid creature wear armor */
{ tBoolean retval = falsE;
  tObject* o;
  if ( (creature->armor == NULL) && ((o = creature->objects) != NULL) )
  { tObject* best_armor = NULL;
    tDamage best_armor_damage = 0;
    __for_each_object(o)
    { if ( (o->oc == ocArmor) && (o->enchantment >= 0) &&
        (!(o->flags & ofInUse)) )
      { const tDamage damage = o->damage;
        if (best_armor_damage < damage)
        { best_armor_damage = damage; best_armor = o; }
      }
    }
    if (best_armor != NULL)
    { creature_wear(creature, best_armor);
      creature_last_action_set(creature, cakOther, 0); retval = truE;
    }
  }
  return(retval);
}

static tBoolean action_weapon1(tCreature* creature)
/* lets a humanoid creature try to do "something" with a wielded weapon, e.g.
   creature_throw() an arrow using a bow */
{ tBoolean retval = falsE;
  tObject* const weapon = creature->weapon;
  if (weapon != NULL)
  { /* IMPLEMENTME! */
  }
  return(retval);
}

static tBoolean action_weapon2(tCreature* creature)
/* lets a humanoid creature (un-)wield a weapon */
{ tBoolean retval = falsE;
  if (creature->weapon == NULL)
  { tObject* best_weapon = NULL;
    /* IMPLEMENTME! */
    if (best_weapon != NULL)
    { creature_wield(creature, best_weapon);
      creature_last_action_set(creature, cakOther, 0); retval = truE;
    }
  }
  else if ( (difficulty < dkNormal) && (__randpercent(12 - 4 * difficulty)) )
  { creature_unwield(creature);
    creature_last_action_set(creature, cakOther, 0); retval = truE;
  }
  else
  { /* IMPLEMENTME: if e.g. the current weapon is a bow and the creature
       doesn't have arrows, but has a crossbow and bolts, let it unwield the
       bow and later wield the crossbow! */
  }
  return(retval);
}

static tBoolean action_refresh(tCreature* creature)
/* lets a creature "refresh" itself by special means */
{ tBoolean retval = falsE;
  const tCreatureClass cc = creature->cc;
  if (cc == ccAquatar)
  { tHealth deflt = creature_defaulthealth(creature);
    if ( (creature->health < deflt / 2) && (!creature_is_in_the_air(creature)))
    { /* look for sfWater on adjacent squares */
      const tCoordinate x = creature->x, y = creature->y;
      tDirection dir0;
      for_each_randdir(dir0)
      { const tCoordinate x2 = x + xdir[dir0], y2 = y + ydir[dir0];
        tSquareFlags sflags2;
        if (!is_location_valid(x2, y2)) continue;
        sflags2 = squareflags_get(x2, y2);
        if (!(sflags2 & sfWater)) continue; /* location not interesting */
        if (sflags2 & sfPlatform)
        { if ((sflags2 & sfPlatform) == sfGlazedFrost)
          { /* the water is only covered by glazed frost, and aquatars can melt
               this when they have to */
            squareflags_andnot(x2, y2, sfGlazedFrost);
            slurp:
            squareflags_andnot(x2, y2, sfWater); map_update(x2, y2, mfGround);
            creature->health = deflt;
            creature_last_action_set(creature, cakOther, 0); retval = truE;
            break;
          }
        }
        else goto slurp; /* water not covered by any sfPlatform */
      }
    }
  }
  /* IMPLEMENTME further! */
  return(retval);
}

typedef tBoolean (*tAction)(tCreature*);

my_enum1 enum
{ ackNonmove = 0, ackGlide = 1, ackMove1 = 2, ackMove2 = 3, ackMove3 = 4,
  ackRsh = 5, ackThrow = 6, ackDrop = 7, ackPickEat = 8, ackArmor = 9,
  ackWeapon1 = 10, ackWeapon2 = 11, ackRefresh = 12,
  maxActionKind = 12
} my_enum2(unsigned char) tActionKind;

static const tAction action[maxActionKind + 1] =
{ action_nonmove, action_glide, action_move1, action_move2, action_move3,
  action_rsh, action_throw, action_drop, action_pick_eat, action_armor,
  action_weapon1, action_weapon2, action_refresh
};

void action_creature(tCreature* creature)
/* lets creatures except the player character act (if they want to:-) */
{ const tCreatureFlags cflags = creature->flags;
  /* const tCoordinate x = creature->x, y = creature->y; */
  tBoolean can_move, is_humanoid;
  creature_last_action_set(creature, cakUnknown, 0);
#if CONFIG_EXDEBUG
  if (cflags & cfInvisible)
  { sprintf(debugstrbuf, "INVIS(%d,%d,%d,%ld)", creature->cc, x, y, cflags);
    message_log(debugstrbuf);
  }
#endif
#if 0
  if ( (squareflags_get(x, y) & sfGlazedFrost) &&
       (!(cflags & cfStationary)) && (!creature->held) &&
       (!creature_is_in_the_air(creature)) && (creature->lastdir != dirNone))
  { dir = creature->lastdir;
    goto do_handle_move; /* IMPLEMENTME! */
  }
#endif
  if (creature->napping | creature->frozen) goto finish; /* can't act now */
  can_move = cond2boolean( (!(cflags & cfStationary)) && (!creature->held) );
  is_humanoid = creature_is_humanoid(creature);

#define TACK(ack) do { if ((action[ack])(creature)) goto finish; } while (0)
  if (can_move) TACK(ackGlide); /* (_must_ be first; uncontrollable) */
  TACK(ackRefresh); TACK(ackNonmove); TACK(ackRsh);
  if (is_humanoid) TACK(ackWeapon1);
  TACK(ackThrow);
  if (can_move) { TACK(ackMove1); TACK(ackMove2); }
  TACK(ackPickEat);
  if (is_humanoid) { TACK(ackArmor); TACK(ackWeapon2); }
  if ( (can_move) && (!creature->confused) ) TACK(ackMove3);
#undef TACK
  finish:
  creature->last_action = _dungeonclock; creature_tickout_recalc(creature, 1);
}

#if CONFIG_DEMO
void action_rogue(void)
{ /* We currently don't do anything here (that is, we let the player character
     rest). Of course, this means that the player character is killed soon
     without interesting gameplay, but we don't care too much for now - let's
     implement more important stuff first. */
  rogue_las(cakRest, 0);
}
#endif /* #if CONFIG_DEMO */
