/* atrogue/apply.c - the keyboard command "apply"
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

/* This file is simply #include'd from main.c; it's taken out for the only
   reason that source files are smaller this way. KISS!
*/

static const char strAlreadyApplying[] = "already applying ";
#define strApplying (strAlreadyApplying + 8)

static tBoolean _apply_amulet(void)
/* handles the application of a suitable ocAmulet or tokPhotograph object; the
   effect depends on chance (of course:-) and on the player character's magic
   energy */
{ tBoolean shall_tick, msg;
  tMagicEnergy energy = roguedata.magic_energy, sub_energy = 0;
  shall_tick = msg = truE;
  if ((energy >= 1) && (roguedata.state >= rsWeak) && (difficulty <= dkNormal))
  { roguedata.actions_left += 1500 + randbytemod(200);
    rogue_checkactionsleft(); sub_energy = 1; msg = falsE;
  }
  else if ( (energy >= 3) && (current_storeynum > current_dd->min_storey) &&
    (!rogue_in_shop) && (__randpercent(10)) ) /* ascend */
  { sub_energy = 3; (void) updownsec(truE); shall_tick = msg = falsE; }
  else if ( (energy >= 2) && (__randpercent(30)) ) /* teleport away */
  { sub_energy = 2; creature_teleport(rogue); msg = falsE;
    /* shall not cause infuriate_shopkeeper() - the amulet is strong... */
  }
  else if ( (energy >= 1) && (__randpercent(50)) ) /* (un-)levitate */
  { sub_energy = 1; msg = falsE;
    if (rogue->levitated) (void) creature_unlevitate(rogue);
    else creature_levitate(rogue, 20 + randfewticks(5));
  }
  if (sub_energy > 0) { rogue_decmagicen(sub_energy); rogue_incmagicexp(1); }
  if (msg) nothing_happens();
  return(shall_tick);
}

static one_caller tBoolean apply_amulet(tObject* amulet)
{ tBoolean shall_tick;
  const tObjectDetail detail = amulet->objdetail;
  unsigned char n, d;
  AMULET_ND(detail, n, d);
  if ( (n < d) && (difficulty > dkNormal) &&
    (__randpercent(89 + 2 * difficulty)) ) /* yes, fakes shall explode too! */
  { const tBoolean worn = cond2boolean(roguedata_os(rdosiNecklace) == amulet),
      cursed = cond2boolean(amulet->enchantment < 0);
    message_show("the amulet explodes"); creature_lose_object(rogue, amulet);
    object_deallocate(amulet);
    if (!creature_dec_health(rogue, n + d + difficulty + (worn ? 5 : 0) +
      (cursed ? 5 : 0) + randbytemod(difficulty)))
    { rogue_died(drInjury, NULL); shall_tick = truE; goto out; }
  }
  if (AMULET_FAKE(detail)) { nothing_happens(); shall_tick = truE; goto out; }
  amulet->lifetime -= 5000; /* (even a failing attempt costs lifetime; but we
    don't care about initial lifetime - it's > 0, this shall be sufficient) */
  if (amulet->enchantment <= 0) goto nh; /* need at least _some_ enchantment */
  if (n < d) { nh: nothing_happens(); shall_tick = truE; }
  else { amulet->enchantment /= 3; shall_tick = _apply_amulet(); }
  creature_check_object(rogue, amulet);
  out: return(shall_tick);
}

static tBoolean apply_change_being_lit(tObject* object)
/* returns whether a dungeon tick shall be done afterwards */
{ /* IMPROVEME: print better messages (with object info) when this function
     might be called from action_rogue()! */
  if (object->flags & ofBeingLit) /* quench the object */
  { (void) object_get_minidesc(object, strbuf, 1, "quenching ");
    message_show(strbuf); object_light_off(object);
    roguedata.flags |= rfInvPropertyChanged;
  }
  else
  { /* Try to light the object - check for flintstones, but don't care about
       their initial lifetime. (It's > 0, that shall be sufficient.) */
    tObject* o;
    for_each_object(rogue->objects, o)
    { if ( (o->oc == ocStone) && (o->kind == stkFlint) &&
           (o->flags & ofKnownMaterial) )
      { (void) object_get_minidesc(object, strbuf, 1, "lighting ");
        message_show(strbuf); object_light_on(object);
        roguedata.flags |= rfInvPropertyChanged; o->lifetime -= aaFriction;
        creature_check_object(rogue, o); goto done;
      }
    }
    (void) object_get_minidesc(object, strbuf, 1, "need flintstone to light ");
    message_show(strbuf);
    return(falsE);
  }
  done:
  return(truE);
}

static void do_expel(const tObject* tool)
{ tCreature* creature;
  const tPropertyExtent ench = tool->enchantment;
  tBoolean is_ench = cond2boolean(ench >= 0), found = falsE;
  for_each_currentsectioncreature(creature)
  { tAggressivity aggr;
    if (creature->flags & cfDead) continue;
    aggr = creature->aggr;
    if (is_ench)
    { if (map_can_player_see(creature->x, creature->y, mfCreature))
      { found = truE;
        creature_wake_up(creature, falsE);
        if (aggr >= -ench) creature_set_aggressivity(creature, -ench - 1);
      }
    }
    else
    { creature->flags &= ~cfPro;
      creature_wake_up(creature, falsE);
      if (aggr < -ench) creature_set_aggressivity(creature, -ench);
    }
  }
  if ( (!is_ench) || (!found) ) laughtermsg();
  else if (!rogue_can_hear) strangemsg();
  else
  { const tToolKind kind = tool->kind;
    if (kind == tokWhistleExpel) toolkind_id[kind] = truE;
    sprintf(strbuf, "%s hear loud squeals", rogue_pronoun(0));
    message_show(strbuf);
  }
}

static one_caller void camera_flash(const tPropertyExtent ench)
{ const tBoolean is_ench = cond2boolean(ench >= 0);
  tCreature* c;
  if (is_ench)
  { const tRoomIndex r = creature_room(rogue);
    if (rogue_can_see) message_show("*flash*");
    if (r == roomNone) return; /* IMPLEMENTME: affect nearby creatures! */
    /* IMPLEMENTME: the flash should make the room permanently brighter; maybe
       the brightness then slowly fades again in deeper storeys? */
    for_each_currentsectioncreature(c)
    { const tCreatureFlags flags = c->flags;
      if (flags & cfDead) continue;
      if (creature_room(c) != r) continue;
      if (flags & cfInvisible) creature_make_visible(c);
      else if (flags & cfImitates)
        creature_unimitate(c, map_can_player_see(c->x, c->y, mfCreature));
      if (creature_is_shopkeeper(c))
      { infuriate_shopkeeper(NULL, falsE); continue; }
      if (c->cc == ccVampire)
      { (void) creature_died(c, drLight, rogue); continue; }
      if ( (creature_can_see(c)) && (ench > 0) ) creature_blind(c, 10 * ench);
    }
    /* IMPLEMENTME: transform some worthless stones in this room into gems! */
    /* IMPLEMENTME: make permatraps visible! */
  }
  else /* cursed */
  { if (rogue_can_see) (void) rogue_dazzle(falsE);
    for_each_currentsectioncreature(c)
    { const tCreatureFlags flags = c->flags;
      if (flags & cfDead) continue;
      if ( (!(flags & cfInvisible)) && (__randpercent(5 * difficulty + 1)) )
      { creature_make_invisible(c);
        /* in the entire section, not just the current room - cursed magic */
      }
    }
    /* IMPLEMENTME: make permatraps invisible! */
  }
}

static void camera_photo(const tBoolean is_ench, const tBoolean did_flash)
{ tObject *photo = object_create(ocTool, tokPhotograph, (is_ench ? 2 : 0)), *o;
  tCoordinate x = rogue->x, y = rogue->y, dist;
  /* const tRoomNumber r = rogue->room; */
  tDirection dir = rogue->lastdir;
  tDirectionOffset dx, dy;
  tSquareFlags sflags;
  const tObstacle* ob;
  const tCreature* c;
  if ( (dir == dirNone) || (randpercent(10 * rogue->confused)) ) dir=randdir();
  dx = xdir[dir]; dy = ydir[dir]; dist = 10;
  /* IMPLEMENTME: let the maximum distance depend on the brightness of the
     current room (if in a room at all) and <did_flash>! */
  while (dist-- > 0)
  { tObjectClass oc;
    tObjectKind okind;
    x += dx; y += dy;
    if (!is_location_valid(x, y))
    { set_rock: photo->objdetail = phkRock; goto finish; }
    sflags = squareflags_get(x, y);
    if ( (sflags == 0) || (sflags & sfWall) ) goto set_rock;
    if ( (sflags & sfCreature) && ( (c = location2creature(x, y)) != NULL ) &&
      (!(c->flags & (cfInvisible | cfImitates))) )
    { photo->objdetail = phkCreature | photo_cc(c->cc); goto finish; }
    if ( (sflags & sfObstacle) && ( (ob = location2obstacle(x, y)) != NULL ) &&
      (!(ob->flags & obfInvisible)) )
    { const tObstacleKind obkind = ob->kind;
      /* FIXME: handle sfHidden! */
      if (obkind == obkMirror) { oc = ocTool; okind = tokCamera; goto set_obj;}
      if ( (obkind == obkDoor) && (!door_visible(door_state(ob->obsdetail))) )
      { /* cannot photograph a broken door, of course - it's "gone" */ }
      else { photo->objdetail = phkObstacle | photo_obk(obkind); goto finish; }
      /* The order is important: if it's a chest, we don't wanna photograph an
         _object_ which is _inside_ the chest. So we handle obstacles before
         objects. */
    }
    if ( (sflags & sfObject) && ( (o = location2object(x, y)) != NULL ) )
    { /* We handle mostly such objects here which may allow the player to
         perform nice tricks later... */
      tObjectDetail detail;
      oc = o->oc; okind = o->kind; /* default values */
      switch (oc)
      { case ocAmulet:
          detail = o->objdetail;
          if ( (!AMULET_FAKE(detail)) && (amulet_misscount(detail) <= 0) )
            goto set_obj; /* a complete, genuine amulet */
          break;
        case ocWeapon:
          if (object_weapon_relsize(o->kind) < 4) break; /* too small */
          /*@fallthrough@*/
        case ocFood: case ocArmor: case ocPotion:
          set_obj: photo->objdetail = full_photobjcode(oc, okind); goto finish;
          /*@notreached@*/ break;
        case ocTool:
          switch (okind)
          { case tokWhistleExpel: /* getting a "photo of expellation" */
            case tokDie: /* getting a "perpetual die" (almost:-) */
            case tokHeartGold: /* getting a "teleportation device" */
            case tokLantern: /* getting a nice light source */
            case tokCamera: /* leads one storey down or up each time, depending
              on whether the the player character has the Amulet; you either
              have to photograph one camera with another or use a mirror - the
              "self-referentiality" of a camera on a photograph distorts the
              space-time continuum of the dungeon! :-) */
            case tokKey: /* getting a "universal key" */
              goto set_obj; /*@notreached@*/ break;
          }
          break;
      }
    }
  }
  photo->objdetail = phkDarkness;
  finish:
  if (creature_can_carry_object(rogue, photo, 1))
    (void) creature_add_object(rogue, photo);
  else section_put_object_carefully(photo, rogue->x, rogue->y, 2);
}

static one_caller void apply_camera(tObject* camera)
{ const tPropertyExtent ench = camera->enchantment;
  const tBoolean is_ench = cond2boolean(ench >= 0);
  tFewTicks lt = camera->lifetime, sub;
  if (rogue_can_hear) message_show("*click*");
  if (lt >= 1000) /* flash and photo */
  { camera_flash(ench); camera_photo(is_ench, truE); sub = 1000; }
  else if (lt >= 300) /* just a photo */
  { camera_photo(is_ench, falsE); sub = 300; }
  else sub = lt;
  camera->lifetime -= sub; creature_check_object(rogue, camera);
}

static void cast_die(const tPropertyExtent ench)
{ const tBoolean is_ench = cond2boolean(ench >= 0);
  tRandomByte factor=ABS(ench), value=randbytemod(6)+1, effect=randbytemod(6);
  sprintf(strbuf, "rolling a %d", value); message_show(strbuf);
  if (factor < 2) factor = 2;
  else if (factor > 4) factor = 4;
  value *= factor;
  if (randcointoss()) value++;
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "ROLL:val=%d,fac=%d,eff=%d", value, factor, effect);
  message_showdebug(debugstrbuf);
#endif
  switch (effect) /* what shall happen? */
  { case 0:
      if (is_ench) creature_haste(rogue, value);
      else creature_slow(rogue, value);
      break;
    case 1:
      if (is_ench) creature_levitate(rogue, value);
      else
      { if (rogue->levitated > value) rogue->levitated -= value;
        else (void) creature_unlevitate(rogue);
      }
      break;
    case 2:
      if (is_ench) (void) do_magic_sleepfreeze(ench, truE);
      else creature_nap(rogue, value, 0);
      break;
    case 3:
      if (is_ench)
      { if (rogue->confused > value) rogue->confused -= value;
        else creature_unconfuse(rogue);
      }
      else creature_confuse(rogue, value);
      break;
    case 4:
      if (is_ench)
      { if (rogue->_blind > value) rogue->_blind -= value;
        else creature_unblind(rogue);
      }
      else creature_blind(rogue, value);
      break;
    case 5:
      if (is_ench)
      { if (rogue->held > value) rogue->held -= value;
        else creature_cancel_being_held(rogue);
      }
      else
      { if (!rogue->held)
        { roguedata.holding_cause = hcOtherClasp; rogue->held = value;
          message_show("something holds");
        }
      }
      break;
  }
}

static void apply_hog(const tObject* tool)
/* applies a Heart Of Gold tool (or the respective photo) */
{ if (tool->enchantment >= 0)
  { if ( (rogue_in_shop) && (rogue_has_unpaid_items) )
      infuriate_shopkeeper(NULL, truE);
    else creature_teleport(rogue); /* what else? :-) */
  }
  else nothing_happens();
}

static one_caller tBoolean apply_photograph(tObject* photo)
{ tBoolean shall_tick = truE;
  const tObjectDetail detail = photo->objdetail;
  tFewTicks sub = 1000;
  tCreatureClass cc;
  tCreature* c;
  tObject* object;
  tObjectClass oc;
  tObjectKind okind;
  switch (photo_kind(detail))
  { default: laugh: laughtermsg(); break;
#if 0
    case phkDarkness: /* IMPLEMENTME: turn the current room dark! */ break;
    case phkRock: break;
    case phkObstacle: break;
#endif
    case phkCreature:
      cc = photo2cc(detail);
      if ( (cc > maxRandCreatureClass) || (cc == ccRogue) ) goto laugh;
      if (photo->enchantment < 0) /* create one such creature */
      { tCoordinate x = rogue->x, y = rogue->y, x2, y2;
        if (section_randlocatearound(x, y, sfCreature, &x2, &y2))
          (void) creature_create_at(cc, x2, y2, 1); /* near player character */
        else (void) creature_create(cc, 1); /* anywhere */
      }
      else /* defeat all such creatures in the current section */
      { tCreatureCount count = 0;
        for_each_currentsectioncreature(c)
        { if (c->flags & cfDead) continue;
          if (c->cc != cc) continue;
          (void) creature_died(c, drRemoval, rogue); count++;
        }
        if (count <= 0) goto laugh;
        rogue_incmagicexp( (count >= 4) ? 2 : 1 );
      }
      break;
    case phkObject:
      if (photo->enchantment < 0) goto laugh;
      photobjback(detail, oc, okind);
      switch (oc)
      { case ocAmulet: shall_tick = _apply_amulet(); break;
        case ocFood: case ocArmor: case ocWeapon: case ocPotion:
          if ( (object = object_create(oc, okind, 2)) != NULL )
          { if (!object_randlocatearound(object, rogue->x, rogue->y))
              section_put_object_carefully(object, rogue->x, rogue->y, 1);
          }
          break;
        case ocTool:
          switch (okind)
          { case tokWhistleExpel: do_expel(photo); break;
            case tokDie:
              cast_die(photo->enchantment); sub = 100 * (difficulty+1); break;
            case tokHeartGold: apply_hog(photo); break;
            /* case tokCamera: IMPLEMENTME! break; */
            /* case tokLantern: used via object_light()! break; */
            /* case tokKey: player must use it via open_door()! break; */
            default: goto laugh; /*@notreached@*/ break;
          }
        default: goto laugh; /*@notreached@*/ break;
      }
      break;
  }
  /* IMPLEMENTME: some photos have special effects when the player character
     holds them toward a mirror; e.g. a new object of the kind shown on the
     photo is created! */
  photo->lifetime -= sub; creature_check_object(rogue, photo);
  return(shall_tick);
}

static void dunno_apply(const tObject* object)
{ (void) object_get_minidesc(object, strbuf, 0, "don't know how to apply ");
  message_show(strbuf);
}

static one_caller tBoolean apply_tool(tObject* tool)
/* returns whether a dungeon tick shall be done afterwards */
{ tBoolean retval = falsE;
  tObjectKind kind = tool->kind;
  tToolClass toc;
  tFewTicks ltsub = 1000;
  tRogueDataOsi rdosi;

  /* non-permanent tools */

  if (kind == tokLantern) return(apply_change_being_lit(tool));
  else if (kind == tokWhistleExpel)
  { do_expel(tool);
    if (tool->enchantment >= 0)
    { declt: tool->lifetime -= ltsub; creature_check_object(rogue, tool); }
    goto tickout;
  }
  else if (kind == tokWhistleSignal)
  { if (tool->enchantment < 0) { laughtermsg(); goto tickout; }
    nothing_happens(); goto tickout;
    /* IMPLEMENTME: e.g. make a drawbridge (un-)available; summon a ferry! */
  }
  else if (kind == tokWhistleNothing)
  { if (tool->enchantment < 0) laughtermsg();
    else nothing_happens();
    goto tickout;
  }
  else if (kind == tokPen)
  { tObject* scr;
    tObjectKind orig_sk;
    const tPropertyExtent tench = tool->enchantment;
    if (!rogue_can_write()) goto out;
    /* The action of the pen shall not affect the cbfDidWrite conduct. The
       player doesn't write directly and can't influence the result, and the
       whole action is magic. */
    /* IMPLEMENTME: roleWizard should be able to influence the result slightly!
    */
    /* IMPLEMENTME: when writing with an enchanted pen into an enchanted empty
       spellbook, the player can write an arbitrary "script" into the
       spellbook. The game then can execute this script at a certain speed
       (depending on player character skills). This allows the player to
       perform special tricks which require a certain timing, and it allows the
       player to move along and do something different while the script
       performs its effects. The "proguerammer" role (combination of "rogue"
       and "programmer") has much better "scripting abilities" than any other
       role. */
    if (!rogue_has_objectclasses(1 << ocScroll))
    { message_show("don't have any scroll to write on"); goto out; }
    if ( (scr=ask_for_invobj("write on which scroll?", 1<<ocScroll)) == NULL )
      goto out;
    if (tool->lifetime < 1000) { nothing_happens(); goto declt; }
    orig_sk = scr->kind;
    if ( (tench < 0) || (roguedata.magic_energy < 1) ||
         (randpercent(4 * difficulty - 3 * roguedata.magic_exp)) )
    { if (orig_sk == skBlankPaper)
      { scr->lifetime = 0; creature_check_object(rogue, scr); }
      else
      { scr->kind = skBlankPaper;
        scribble:
        if (!rogue_can_see)
        { if (rogue_can_hear)
            message_show("the pen seems to scratch over the scroll");
        }
        else
        { message_show("the pen scribbles wildly all over the scroll");
          if (scr->kind != orig_sk)
          { sprintf(strbuf, "the inscription %ses",
              ( (scr->kind == skBlankPaper) ? "vanish" : "chang" ));
            message_show(strbuf);
          }
        }
      }
    }
    else
    { tObjectKind new_sk;
      if (randpercent(difficulty - tench)) new_sk = skBlankPaper;
      else
      { unsigned char count = 20;
        while (--count > 0)
        { new_sk = randbytemod(maxScrollKind);
          if (new_sk != orig_sk) break;
          /* CHECKME: limit allowed kinds by storey! */
        }
        if (new_sk == orig_sk) new_sk = skBlankPaper;
      }
      scr->kind = new_sk;
      goto scribble;
    }
    rogue_decmagicen(1);
    goto declt;
  }
  else if (kind == tokDie) { cast_die(tool->enchantment); goto declt; }
  else if (kind == tokCase)
  { enum { ocbUsable = ( (1 << ocGold) | (1 << ocTool) | (1 << ocFood) |
      (1 << ocPotion) | (1 << ocWandStaff) | (1 << ocRing) ) };
      /* (ocScroll can be treated with a tokPen instead!) */
    static tBoolean did_carrot = falsE;
    tObject *object, *new_object;
    tObjectClass oclass;
    tObjectQuantity quantity;
    char temp[20];

    if (!rogue_has_objectclasses(ocbUsable))
    { message_show("don't have items than can be put into the case"); goto out;
    }
    quantity = entered_quantity(0);
    if (quantity == ENTERED_NUMBER_INVALID) temp[0] = '\0';
    else sprintf(temp, strNumberOf, quantity);
    sprintf(strbuf, "put %swhat into the case?", temp);
    if ( (object = ask_for_invobj(strbuf, ocbUsable)) == NULL ) goto out;
    if (handle_unpaid(object)) goto tickout;
    if ( (object == tool) || (object->flags & ofInUse) )
    { /* (ofInUse can at least mean ofBeingApplied (for permanent tools)) */
      laughtermsg(); goto declt;
    }
    oclass = object->oc; quantity = entered_quantity(1);
    if (quantity > object->quantity) quantity = object->quantity;
    if (quantity < object->quantity)
    { tObject* o;
      roguedata.packweight -= object_weight(object);
      o = object_split(object, quantity);
      roguedata.packweight += object_weight(object);
      object = o;
    }
    else creature_lose_object(rogue, object);

    new_object = NULL;
    if ( (oclass == ocGold) && (object->quantity == 42) )
    { /* We do this even if the case is cursed; the magic power of that good
         ol' number shall be stronger than the worst curse. :-) */
      enum { num = 3 };
      static const tToolKind t[num] = { tokHeartGold, tokNoTea, tokTowel };
        /* what else? (-: */
      tBoolean use_plural, can_see;
      new_object = object_create(ocTool, t[randbytemod(num)], 2);
      case_vanish: can_see = rogue_can_see;
      if (can_see)
      { use_plural = object_get_minidesc(object, strbuf, 1, NULL);
        sprintf(strbuf + strlen(strbuf), " vanish%s in the case",
          (use_plural ? strEmpty : strEs));
        message_show(strbuf);
      }
      object_deallocate(object);
      if (new_object != NULL)
      { if (can_see)
        { use_plural = object_get_minidesc(new_object, strbuf, 0, NULL);
          sprintf(strbuf + strlen(strbuf), " appear%s", (use_plural ? strEmpty
            : strS));
          message_show(strbuf);
        }
        if (creature_can_carry_object(rogue, new_object, 1))
          (void) creature_add_object(rogue, new_object);
        else section_put_object_carefully(new_object, rogue->x, rogue->y, 2);
      }
    }
    else if (tool->enchantment < 0) goto case_vanish;
    else if (oclass == ocTool)
    { tObjectKind okind = object->kind;
      if (object_tool_is_quark(okind))
      { tObjectKind which = object_tool_quark2num(okind);
        tool->quark_count++; tool->quark_value = 6 * tool->quark_value + which;
        if (tool->quark_count == 3)
        { /* got a complete triple (yes, we only allow triples, no pairs; and
             we also don't care whether the given combination is actually
             possible in the "real" world; etc.) */
          new_object = object_tool_quarks2object(tool->quark_value);
          tool->quark_count = 0; tool->quark_value = 0;
        }
        else ltsub = 100; /* the case didn't do much this time... */
        goto case_vanish;
      }
      else /* a non-quark tool */
      { const tBoolean use_plural = object_get_minidesc(object, strbuf, 1,
          "the case rebuffs ");
        message_show(strbuf);
        if (rogue_can_see)
        { sprintf(strbuf, "%s disappear%s in a puff of smoke", (use_plural ?
            strThey : "that"), (use_plural ? strEmpty : strS));
          message_show(strbuf);
          /* rebuff - puff: sounds good :-) */
        }
        object_deallocate(object);
        /* CHECKME/IMPLEMENTME? */
      }
    }
    else if ( (oclass == ocFood) && (object->kind == fkCarrot) && (!did_carrot)
      && (object->enchantment >= 0) )
    { if (rogue_can_see)
      { message_show("Bugs hops out of the case"); /* Bugs Bunny :-) */
        message_show("he thanks you for the carrot");
        message_show("he hops away, munching");
      }
      did_carrot = truE; object_deallocate(object);
      /* IMPLEMENTME: let this have a special effect, e.g. make a certain
         dungeon department kind available somewhere! */
    }
    else
    { tObjectKind orig_kind = object->kind, new_kind, maxkind;
      unsigned char count;
      if (oclass == ocPotion) maxkind = maxPotionKind;
      else if (oclass == ocWandStaff) maxkind = maxWandStaffKind;
      else if (oclass == ocRing) maxkind = maxRingKind;
      else goto case_vanish; /* e.g. ocGold, quantity != 42 */
      count = 20;
      while (--count > 0)
      { new_kind = randbytemod(maxkind + 1);
        if (new_kind != orig_kind) break;
        /* CHECKME: limit the allowed kinds by storey? (cf.
           object_confused_scrollkind()) */
      }
      if (new_kind != orig_kind)
        new_object = object_create(oclass, new_kind, 2);
      goto case_vanish;
    }
    goto declt;
  }
  else if (kind == tokHeartGold) { apply_hog(tool); goto declt; }
  else if (kind == tokNoTea)
  { unsigned char max = 8 - randbytemod(MAX(difficulty, 2) + 1), count = 0;
      /* (The player character's location has at most 8 adjacent locations.) */
    tCoordinate x = rogue->x, y = rogue->y;
    /* Now let's figure this out... "no tea" means "nothing is there at all",
       which means it's a vacuum; a vacuum might fluctuate, ghost particles
       might appear; in the "real" world, those particles would cancel out and
       vanish quite soon, but the magic of the dungeon upholds them. :-) */
    message_show("no tea fluctuates");
    while (count < max)
    { tObject* o = object_create(ocTool, object_randquarkkind, 0);
      if ( (o != NULL) && (!object_randlocatearound(o, x, y)) )
      { object_deallocate(o); break; } /* no further free locations */
      count++;
    }
    if (count == 0) laughtermsg();
    goto declt;
  }
  else if (kind == tokTowel)
  { /* Towels are a hitchhi^Wplayer character's best friends. :-) */
    if ( (roguedata.state < rsWeak) || (tool->enchantment < 0) )
    { tool->lifetime = 0; creature_check_object(rogue, tool); }
    else if (!cant_eatquaff_with_shawl(truE))
    { roguedata.actions_left += 1000 + randbytemod(200);
      rogue_checkactionsleft(); yumyuk(truE); goto declt;
    }
    goto tickout;
  }
  else if (kind == tokCamera) { apply_camera(tool); goto tickout; }
  else if (kind == tokPhotograph)
  { if (apply_photograph(tool)) goto tickout;
    else goto out;
  }
  else if (object_tool_is_quark(kind))
  { dunno: dunno_apply(tool); goto out; }

  /* permanent tools */

  toc = object_toolkind2class(kind);
  if (!toc2rdosi(toc, &rdosi)) goto dunno;
  if (tool->flags & ofBeingApplied)
  { const tObject* bf;
    if ((rdosi == rdosiGlasses) && ( (bf=roguedata_os(rdosiBlindfold))!=NULL))
    { const tBoolean pl = object_get_minidesc(bf, strbuf, 1, NULL);
      sprintf(strbuf + strlen(strbuf), strPercsInTheWay,(pl ? strAre : strIs));
      message_show(strbuf); goto out;
    }
    tool->flags |= ofEnchIdentified;
    if (tool->enchantment < 0) { cant_remove_cursed(tool); goto out; }
    tool->flags &= ~ofBeingApplied; roguedata.flags |= rfInvPropertyChanged;
    roguedata_os(rdosi) = NULL;
    (void) object_get_minidesc(tool, strbuf, 1, "removed ");
    message_show(strbuf);
    /* The player intentionally un-applies the tool; this shall have immediate
       consequences (as opposed to the case where the tool lifetime reaches 0):
    */
    if (kind == tokBootsLevit)
    { if (rogue->levitated) (void) creature_unlevitate(rogue);
    }
    else if (kind == tokCapeInvis)
    { if (rogue->flags & cfInvisible) creature_make_visible(rogue);
    }
  }
  else
  { tObject** _o = &(roguedata_os(rdosi));
    if (*_o != NULL)
    { (void) object_get_minidesc(*_o, strbuf, 0, strAlreadyApplying);
      message_show(strbuf); goto out;
    }
    *_o = tool; tool->flags |= ofBeingApplied;
    (void) object_get_minidesc(tool, strbuf, 1, strApplying);
    message_show(strbuf);
  }
  tickout: retval = truE;
  out: return(retval);
}

static tBoolean applyrelated_hit_creature(tObject* object, tCreature* creature,
  const tCoordinate distance)
/* for wkHarpoon and wkBoomerang */
{ tObjectKind kind = object->kind;
  tBoolean can_see = map_can_player_see(creature->x, creature->y, mfCreature),
    do_hit = randpercent(100 - 20 * difficulty + 10 * creature_size(creature)
             - 3 * distance);
  if (can_see)
  { if (creature->flags & cfImitates) creature_unimitate(creature, truE);
    sprintf(strbuf, "the %s%s ",
      ( (kind == wkHarpoon) ? ("harpoon") : ("boomerang") ),
      ( (do_hit) ? (strSpaceHits) : (strSpaceMisses) ));
    creature_get_name(creature, strbuf + strlen(strbuf), 1);
    message_show(strbuf);
  }
  if (do_hit)
  { const tDamage damage = randbytemod(4 - distance / 5) +
      5 * cond2bool(kind == wkBoomerang) + 2;
#if CONFIG_DEBUG
    sprintf(debugstrbuf, "DMG=%d", damage);
    message_showdebug(debugstrbuf);
#endif
    creature_wake_up(creature, can_see);
    if (!creature_dec_health(creature, damage))
    { (void) creature_died(creature, drInjury, rogue); }
    object->lifetime -= 300;
  }
  return(do_hit);
}

static tBoolean applyrelated_hit_obstacle(tObject* object, tObstacle* obstacle,
  const tCoordinate distance)
/* for wkHarpoon and wkBoomerang */
{ tBoolean retval, hitting, can_see;
  const unsigned char obsize = obstacle_effsize(obstacle);
  tPropertyExtent ench;
  if (obsize <= 0) hitting = falsE; /* nothing in the way */
  else if (obsize >= 14) hitting = truE; /* can't miss a big obstacle... */
  else hitting = randpercent(70 - 4 * difficulty + 3 * obsize - 2 * distance);
  if (!hitting) goto finish;
  if ( ( (ench = object->enchantment) < 0 ) || (obstacle->resistance >= 14) )
  { resists: can_see = map_can_player_see(obstacle->x, obstacle->y,mfObstacle);
    if (can_see) say_obstacle_resists(obstacle);
  }
  else
  { const tObstacleResistance res = 10 - difficulty + MIN(ench, 2) -
      randbytemod(3);
    if (!obstacle_resistance_decrease(obstacle, res, 1)) goto resists;
  }
  object->lifetime -= 400; finish: retval = hitting;
  return(retval);
}

static one_caller tBoolean apply_harpoon(tObject* harpoon)
/* returns whether a dungeon tick shall be done afterwards */
{ tDirection dir;
  tObstacle* obstacle;
  tCoordinate x, y, distance, maxdistance;
  tDirectionOffset dx, dy;
  if (!(harpoon->flags & ofBeingWielded))
  { message_show(strNeedToWield); return(falsE); }
  if (!ask_for_dir(&dir, NULL)) return(falsE);
  maxdistance = MIN(harpoon->lifetime / 1000, 10);
  if (maxdistance < 1) { nothing_happens(); goto done; }
  if (randpercent(10 * rogue->confused)) dir = randdir();
  x = rogue->x; y = rogue->y; dx = xdir[dir]; dy = ydir[dir];
  distance = 0;
  while (++distance <= maxdistance)
  { tSquareFlags sflags;
    tBoolean can_see;
    x += dx; y += dy;
    if (!is_location_valid(x, y)) /* left the dungeon */
    { can_see = cond2boolean(rogue_can_see); /* CHECKME! */
      hit_wall:
      if (can_see) message_show("the harpoon hits the wall");
      harpoon->lifetime -= 1000; goto done;
    }
    sflags = squareflags_get(x, y);
    if ( (sflags == sfNone) || (sflags & sfWall) )
    { /* (It misses any passage which might be there, just hits the wall...) */
      can_see = map_can_player_see(x, y, mfWall); goto hit_wall;
    }
    else if (sflags & sfBarrier)
    { if (map_can_player_see(x, y, mfBarrier))
        message_show("the harpoon hits the barrier");
      harpoon->lifetime = 0; /* magic harpoon/barrier interaction */
      goto done;
    }
    else if (sflags & sfCreature)
    { tCreature* const creature = location2creature(x, y);
      if (!creature) goto done; /* "should not happen" */
      if (creature->flags & cfImmaterial) goto done; /* FIXME! */
      if (applyrelated_hit_creature(harpoon, creature, distance)) goto done;
    }
    else if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x, y))
      != NULL ) && (applyrelated_hit_obstacle(harpoon, obstacle, distance)) )
    { goto done;
    }
    else if ( (sflags & sfObject) &&
              (randpercent(100 - 10 * difficulty - 5 * distance)) )
    { tObject* object = location2object(x, y);
      if ( (object != NULL) && (creature_can_carry_object(rogue, object, 1)) )
      { char* temp = strbuf;
        do_pick_object(&object, object->quantity, &temp);
          /* (It's a _magic_ harpoon!:-) */
        object_get_description(object, temp);
        message_show(strbuf);
      }
      harpoon->lifetime -= 100;
      goto done;
    }
  }
  done:
  creature_check_object(rogue, harpoon);
  return(truE);
}

static one_caller tBoolean apply_boomerang(tObject* boomerang)
/* returns whether a dungeon tick shall be done afterwards */
{ tDirection dir;
  tCoordinate x, y, distance, maxdistance;
  tDirectionOffset dx, dy;
  if (!(boomerang->flags & ofBeingWielded))
  { message_show(strNeedToWield); return(falsE); }
  if (!ask_for_dir(&dir, NULL)) return(falsE);
  maxdistance = MIN(boomerang->lifetime / 1000, 10);
  if (maxdistance < 1) { nothing_happens(); goto done; }
  if (randpercent(10 * rogue->confused)) dir = randdir();
  x = rogue->x; y = rogue->y; dx = xdir[dir]; dy = ydir[dir];
  distance = 0;
  while (++distance <= maxdistance)
  { tSquareFlags sflags;
    x += dx; y += dy;
    if (!is_location_valid(x, y)) goto done; /* left the dungeon */
    sflags = squareflags_get(x, y);
    if ( (sflags == sfNone) || (sflags & sfWall) )
    { /* The boomerang doesn't hit the wall; it "knows" about walls and returns
         before reaching them - it's a _magic_ boomerang, after all... :-) */
      goto done;
    }
    if (sflags & sfBarrier)
    { if (map_can_player_see(x, y, mfBarrier))
        message_show("the boomerang hits the barrier");
      boomerang->lifetime = 0; /* magic boomerang/barrier interaction */
      creature_check_object(rogue, boomerang);
      goto done;
    }
    if (sflags & sfCreature)
    { tCreature* const creature = location2creature(x, y);
      if (creature == NULL) goto done; /* "should not happen" */
      if (creature->flags & cfImmaterial) goto done; /* FIXME! */
      if (applyrelated_hit_creature(boomerang, creature, distance)) goto done;
      /* IMPLEMENTME: if (distance < 3) { boomerang doesn't come back, simply
         falls down } */
    }
    if (sflags & sfObstacle)
    { tObstacle* obstacle = location2obstacle(x, y);
      if ( (obstacle != NULL) && (applyrelated_hit_obstacle(boomerang,
        obstacle, distance)) )
      { goto done; }
    }
  }
  done:
  creature_check_object(rogue, boomerang);
  return(truE);
}

static one_caller tBoolean apply_ring(tObject* ring)
/* returns whether a dungeon tick shall be done afterwards */
{ tObjectKind kind;
  tBoolean kind_id;
  if (!(ring->flags & ofOnEitherHand))
  { /* We encourage the player to put the ring on hand, even if cursed. :-) */
    message_show("can only apply rings which are on hand");
    return(falsE);
  }

  if (roguedata.magic_energy > 0) rogue_decmagicen(1);
  else if (!randpercent(roguedata.magic_exp / 2))
  { nothing_happens(); goto done; }
  /* "else": the player doesn't have any magic energy, but he already got so
     much magic experience that he's able to "invoke" the ring power this
     time. */

  if (ring->enchantment <= 0)
  { /* need _some_ enchantment to apply a ring at will... */
    nothing_happens(); goto decrease_lifetime;
  }

  kind = ring->kind; kind_id = falsE;
  if (kind == rkHoney) /* non-desirable :-) */
  { if (creature_allure_bees()) kind_id = truE; }
  else if (kind == rkTeleport)
  { kind_id = truE;
    if ( (rogue_in_shop) && (rogue_has_unpaid_items) )
      infuriate_shopkeeper(NULL, truE);
    else creature_teleport(rogue);
  }
#if 0
  /* IMPLEMENTME! */
  else if (kind == rkKnightmove)
  { creature_knightmove(rogue, dirNone); kind_id = truE; }
#endif
  else if (kind == rkPolymorph) /* also non-desirable... */
  { (void) creature_polymorph(rogue); kind_id = truE; }
  else
  { /* All the other rings work permanently anyway, so nothing happens when
       their power is "invoked" at will: */
    nothing_happens();
  }

  if (kind_id) ringkind_id[kind] = truE;

  decrease_lifetime:
  if (ring->enchantment >= 0)
  { /* (Don't help the player getting rid of cursed rings:-) */
    ring->lifetime -= aaInvoke;
    creature_check_object(rogue, ring);
  }
  done:
  return(truE);
}

static one_caller void apply(void)
/* allows the player to "apply" an item, e.g. to turn a lantern on */
{ enum { ocbApplyable = ( (1 << ocAmulet) | (1 << ocTool) | (1 << ocWeapon) |
    (1 << ocRing) ) };
  tObject* object;
  if (!rogue_has_objectclasses(ocbApplyable))
  { message_show("don't have any item that can be applied"); return; }
  if ( (object = ask_for_invobj("apply what?", ocbApplyable)) == NULL ) return;
  if (handle_unpaid(object)) return;
  switch (object->oc)
  { case ocAmulet: if (!apply_amulet(object)) { return; } break;
    case ocTool: if (!apply_tool(object)) { return; } break;
    case ocWeapon:
      switch (object->kind)
      { case wkFireBolt: if (!apply_change_being_lit(object)) {return;} break;
        case wkHarpoon: if (!apply_harpoon(object)) { return; } break;
        case wkBoomerang: if (!apply_boomerang(object)) { return; } break;
        default: strHelp = "to _wield_ a weapon, press \"w\"";
          dunno: dunno_apply(object); return; /*@notreached@*/ break;
      }
      break;
    case ocRing: if (!apply_ring(object)) { return; } break;
    default: goto dunno; /*@notreached@*/ break; /* "should not happen" */
  }
  /* IMPLEMENTME: applying a staff/rod means breaking it and causing special
     effects! Only a staff can be broken, wands and rods are too firm; maybe
     rods can be broken with uncursed tokGlovesStrength. */
  rogue_las(cakOther, 0);
}
