/* atrogue/main.c - main routine, user interaction
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "init.h"
#include "creature.h"
#include "action.h"
#include "dungeon.h"
#include "message.h"
#include "random.h"
#include "stat.h"
#include "animation.h"

static /*@null@*/ const char* strHelp = NULL; /* for gameplay help texts */
static const char strQuote[] = "\"", strNowBracedCharSpace[] = "now (%c) ",
  strNumberOf[] = "%d of ", strWheeSlippery[] = "whee, it's slippery here!",
  strPackFull[] = "pack full", strPercsInTheWay[] = " %s in the way",
  strNeedToWield[] = "need to wield it first", strFeelNow[] = "%s feel %s now";
#define strBracedCharSpace (strNowBracedCharSpace + 4)
#define strSlippery (strWheeSlippery + 6)
#define ACTIONDIGEST (6 - difficulty)

tBoolean _do_tick;


/* Helper functions */

static my_inline void nothing_happens(void)
{ message_show("nothing happens?");
}

static my_inline tObjectQuantity entered_quantity(unsigned short flags0)
/* "flags0 & 1": reset entered_number */
{ if (entered_number == ENTERED_NUMBER_INVALID)
    return(ENTERED_NUMBER_INVALID);
  else
  { tObjectQuantity retval = (tObjectQuantity) entered_number;
    if (flags0 & 1)
    { entered_number = ENTERED_NUMBER_INVALID;
      statistics_invalidate(svfCount);
    }
    return(retval);
  }
}

static my_inline tFewTicks entered_fewticks(unsigned short flags0)
/* "flags0 & 1": reset entered_number */
{ if (entered_number == ENTERED_NUMBER_INVALID)
    return(1);
  else
  { tFewTicks retval = (tFewTicks) entered_number;
    if (flags0 & 1)
    { entered_number = ENTERED_NUMBER_INVALID;
      statistics_invalidate(svfCount);
    }
    return(retval);
  }
}

static tFewTicks restcounter = 0;
  /* for how long the player character is still resting */

static void interrupt_rest(void)
{ message_show("interrupting the rest");
  restcounter = 0;
}

#define SAVSCRBUFSIZE (1024)
static char savscrbuf[SAVSCRBUFSIZE];
static int savscr_fd;
static unsigned short savscridx;

static void savscr_flush(void)
{ (void) my_write(savscr_fd, savscrbuf, savscridx);
}

static void savscr_ch(const char ch)
{ if (savscridx >= SAVSCRBUFSIZE) { savscr_flush(); savscridx = 0; }
  savscrbuf[savscridx++] = ch;
}

static void save_screenshot(void)
{ unsigned char i;
  char *s, *s2 = strbuf + 6; /* s: file number string; s2: complete file name*/
  short x, y;
  strcpy(strbuf, "saved "); strcat(strbuf, get_homepath());
  strcat(strbuf, ".atrogue-screenshot-"); s = strbuf + strlen(strbuf);

  /* Try to find a filename which has not yet been used. (We don't overwrite
     old screenshots - maybe the user hasn't moved an "interesting" old one out
     of the way or presses the save-screenshot key combination accidentally.)*/

  for (i = 1; i <= 99; i++)
  { sprintf(s, "%02d", i);
    savscr_fd = my_create(s2, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (savscr_fd == -1)
    { if (errno == EEXIST) continue;
      else { message_show("could not create file"); return; }
    }
    else goto save;
  }
  message_show("too many screenshot files in directory");
  message_show("please delete some of them"); return;

  save: savscridx = 0;
  for (y = 0; y < screen_height; y++) /* IMPROVEME! */
  { for (x = 0; x < screen_width - 1; x++) { savscr_ch((char) mvinch(y, x)); }
    savscr_ch('\n');
  }
  if (savscridx > 0) savscr_flush();
  my_close(savscr_fd); message_show(strbuf);
}

#define SAVEBUFSIZE (1024)
static tUint8 savebuf[SAVEBUFSIZE], *savepos;
#define save_ch(ch) do { } while (0)
#define save_str(str) do { } while (0)

static void save_dungeon(void)
/* For now, this is merely an experimental draft and doesn't yet save anything
   anywhere. The data should be saved in a file with the suffix ".bdd" ("binary
   dungeon data"), to distinguish it from possible later verbose _textual_ save
   files (".tdd") in nested-list form. */
{ savepos = savebuf;
  save_str("atrogue-dungeon"); save_ch('\0'); /* file type identification */
  save_ch(0); save_ch(0); /* file format version: 0.0 */
  save_ch(0); /* flags */
  save_ch(0); /* checksum byte; reduces technical issues with partially-/
    wrongly-transmitted save files and makes cheating harder :-) */
  save_ch(1); /* "interpreter" which saved the file; 0 means unspecified, 1
    means atrogue, all other values are reserved */
  save_str(_strVersionShort); save_ch('\0'); /* interpreter version */
  /* Write the current "real-world" time */
  /* Here come all "global" preferences, e.g. difficulty and pref_... and
     startpref_... variables; other "global" stuff such as the dungeonclock and
     <dude> and max_section_(width|height) and screen_(width|height); roledata
     and roguedata; which RNG was used and in which state the built-in RNG is.
     The associations between magic object kinds and "materials" and the name
     strings for magic spells (scroll titles) and the ...kind_id[] information
     and the player's ...call[] guesses. */
  /* for each section "s"
     { save basic tSection information, esp. size, squareflags, obstacles,
       events
       save objects (including contained) which are lying around in section
       for each creature "c" in this section 
       { save basic tCreature information
         save objects (including contained; including coordinates, which have
         special meaning for some object kinds) which are in the pack of the
         creature
       }
     }
     save connections between sections
     mark which of the creatures are player characters (easy: cfPc) and which
     of those is the current one
  */
}

tKey my_getch(void)
{ tKey key;
  do
  { errno = 0;
    key = getch();
  } while ( (key == ERR) && (errno == EINTR) );
  switch (key) /* "remap" some special keys */
  { case '\010': key = KEY_BACKSPACE; break;
    case '\015': key = KEY_ENTER; break;
#if CONFIG_GRAPHICS
    case GDK_Escape: key = KEY_CANCEL; break;
#else
    case '\033': case 5: key = KEY_CANCEL; break; /* the "escape" key */
#endif
  }
  return(key);
}

tTickout* active_tickouts = NULL;

void tickout_detach(tTickout* t)
{ if (active_tickouts == t) active_tickouts = t->next;
  if (t->next != NULL) t->next->prev = t->prev;
  if (t->prev != NULL) t->prev->next = t->next;
  t->next = t->prev = NULL;
}

void tickout_schedule(tTickout* tout)
{ if (active_tickouts == NULL) active_tickouts = tout;
  else
  { const tMilliticks this_expiry = tout->expiry;
    tTickout* t = active_tickouts;
    while (this_expiry > t->expiry)
    { tTickout* next = t->next;
      if (next == NULL) { t->next = tout; tout->prev = t; return; }
      t = next;
    }
    if (t->prev != NULL) { t->prev->next = tout; tout->prev = t->prev; }
    else /* active_tickouts == t */ active_tickouts = tout;
    t->prev = tout; tout->next = t;
  }
}

#if CONFIG_DEBUG
static void debug_tickouts(void)
{ tScrollableText st;
  const tTickout* t = active_tickouts;
  st_setup(&st); 
  sprintf(debugstrbuf, "Tickouts at %d/%d", _dungeonclock, dungeonclock);
  st_append(&st, my_strdup(debugstrbuf), truE);
  while (t != NULL)
  { sprintf(debugstrbuf, "tickout %p, data %p, info \"%s\", expiry %d/%d", t,
      t->data, t->debuginfo, t->expiry, mt2t(t->expiry));
    st_append(&st, my_strdup(debugstrbuf), truE);
    t = t->next;
  }
  st_show(&st); st_cleanup(&st);
}
#endif

typedef struct tAftermathEntry
{ struct tAftermathEntry* next;
  tAftermathHandler handler;
  void* data;
} tAftermathEntry;

static tAftermathEntry* aftermath_head = NULL;

void aftermath_schedule(tAftermathHandler handler, void* data)
{ tAftermathEntry* const e = memory_allocate(sizeof(tAftermathEntry));
  e->handler = handler; e->data = data; e->next = aftermath_head;
  aftermath_head = e;
}

static one_caller void aftermath_run(void)
{ const tAftermathEntry* e = aftermath_head;
  while (e != NULL)
  { const tAftermathEntry* const next = e->next;
    ((e->handler)(e->data)); memory_deallocate(e); e = next;
  }
  aftermath_head = NULL;
}

#if !CONFIG_DEBUG
static
#endif
my_inline unsigned short __intsqrt(tUint16 n)
/* calculates the "integer square-root" of <n>; this is some kind of black
   magic and thus fits very well into our magic dungeon game. :-) Note that
   this algorithm isn't "good" (not even monotonously growing), but it's fast
   and "almost good enough" (see below). */
{ tUint16 retval = n;
  while (n > 1) { n >>= 2; retval >>= 1; }
  if (n) retval -= retval >> 2;
  return(retval);
}

unsigned short intsqrt(const tUint16 n)
/* refines __intsqrt(); slows things down, but gives much higher precision */
{ tUint16 retval = __intsqrt(n);

  loop: /* IMPROVEME! */
  if (retval >= 1)
  { const tUint16 test = retval - 1, s1 = test * test, s2 = retval * retval;
    if (ABS(n-s1) < ABS(n-s2)) { retval = test; goto loop; }
  }
  if (retval <= 174)
  { const tUint16 test = retval + 1, s1 = test * test, s2 = retval * retval;
    if (ABS(n-s1) < ABS(n-s2)) { retval = test; goto loop; }
  }

  return(retval);
}

#define si_strcpy_lit(ptr, strlit) do { static const char __str[] = (strlit); strcpy(ptr, __str); ptr += sizeof(__str) - 1; } while (0)

static char* si_strcpy(char* ptr, const char* str)
{ char ch;
  while ( (ch = *str++) != '\0' ) *ptr++ = ch;
  return(ptr);
}

void st_setup(/*@out@*/ tScrollableText* st)
{ my_memclr(st, sizeof(tScrollableText));
}

void st_append(tScrollableText* st, /*@null@*/ const char* line,
  const tBoolean do_dealloc)
{ tScrollableTextEntry *arr = st->entry, *e;
  tStIdx num = st->num_entries, maxnum = st->maxnum_entries;
  if (num >= maxnum) /* need more space */
  { if (maxnum < 8) maxnum = 8;
    else maxnum <<= 1;
    st->entry = arr = memory_reallocate(arr,
      maxnum * sizeof(tScrollableTextEntry));
    st->maxnum_entries = maxnum;
  }
  e = &(arr[num]); my_memclr(e, sizeof(tScrollableTextEntry)); e->line = line;
  e->do_dealloc = do_dealloc; st->num_entries++;
}

#define ST_NOTICE(str) \
  do \
  { if (first_notice) first_notice = falsE; \
    else (void) addstr(strSpacedDash); \
    (void) addstr(str); \
  } while (0)

void st_show(tScrollableText* st)
{ tStKeyHandler key_handler = st->key_handler;
  tScrollableTextEntry *const arr = st->entry, *e;
  const tStIdx num = st->num_entries, h = screen_height - 1;
  const tBoolean might_scroll = cond2boolean(num > h),
    may_end = cond2boolean(key_handler == NULL),
    disls = cond2boolean(st->flags & stfDisableLetterScrolling);
  tBoolean first_notice;
  const char *const notice = st->notice, *str;
  tStIdx top = 0, limit, idx;
  unsigned char space_effect; /* 0: none; 1: leaf; 2: end */
  tCoordinate row;
  tKey key;
  tDirection dir;
  tMoveKind mk;
  save_screen();
  redraw:
  row = 0; limit = MIN(num, top + h);
  for (idx = top; idx < limit; idx++, row++)
  { e = &(arr[idx]); str = e->line; move(row, 0);
    if (str != NULL)
    { const attr_t attr = e->attr;
      if (attr == 0) (void) addstr(str); /* the most likely case; optimize */
      else
      { char ch;
        while ( (ch = *str++) != '\0' )
          addch( ((attr_t) ((unsigned char) ch)) | attr );
      }
    }
    (void) clrtoeol();
  }
  move(row, 0); first_notice = truE; (void) attron(A_REVERSE);
  if (limit < num) space_effect = 1;
  else if (may_end) space_effect = 2;
  else space_effect = 0;
  if (notice != NULL) ST_NOTICE(notice);
  switch (space_effect)
  { case 1: ST_NOTICE("<Space> to leaf forward"); break;
    case 2: ST_NOTICE("<Space> to end"); break;
  }
  if (might_scroll) ST_NOTICE("up/down keys to scroll");
  if ( (space_effect != 2) || (key_handler != NULL) )
    ST_NOTICE("Escape to cancel"); /* explicit hint */
  (void) attroff(A_REVERSE); (void) clrtoeol(); (void) doupdate();
  keyloop: key = my_getch();
  if ( (might_scroll) && (key2dir(key, &dir, &mk)) &&
    ( (!disls) || (!is_valid_invkey(key)) ) )
  { switch (dir)
    { case dirNorth:
        if (top > 0) { top--; goto redraw; }
        break;
      case dirSouth:
        if (top + h < num) { top++; goto redraw; }
        break;
    }
    /* IMPLEMENTME: page-wise scrolling up/down, depending on <mk>! */
  }
  else if (key == ' ')
  { switch (space_effect)
    { case 1:
        top += h;
        if (top > num - h) top = num - h;
        goto redraw; /*@notreached@*/ break;
      case 2: goto done; /*@notreached@*/ break;
    }
  }
  else if (key == KEY_CANCEL) goto done;
  else if ( (key_handler != NULL) && ((key_handler)(st, key)) ) goto done;
  goto keyloop;
  done: restore_screen(); st->key = key;
}

void st_cleanup(tScrollableText* st)
{ const tScrollableTextEntry *arr = st->entry, *e;
  tStIdx idx, num = st->num_entries;
  for (idx = 0; idx < num; idx++)
  { e = &(arr[idx]);
    if (e->do_dealloc) memory_deallocate(e->line);
  }
  if (arr != NULL) memory_deallocate(arr);
}


/* All the inventory handling... NBN (nasty but necessary:-) */

#if CONFIG_GRAPHICS
static void refresh_graphics_inventory(void)
{ /* IMPLEMENTME! */
}
#endif /* #if CONFIG_GRAPHICS */

static struct
{ const tObject* objsort[MAXNUM_INV];
  unsigned char selectables[(MAXNUM_INV + 7) / 8];
    /* bitfield of selectable inventory entries */
  tInventoryIndex numsort;
  tBoolean user_may_select;
} invdata; /* inventory data */

static void invline_append(tScrollableText* st, const tObject* object)
{ const tObjectClass oc = object->oc;
  tColorBitmask cb;
  strbuf[0] = object_get_char(object); strbuf[3] = invobj2char(object);
  object_get_description(object, strbuf + 6);
  switch (oc)
  { case ocArmor: cb = cbGreen; break;
    case ocWeapon: cb = cbYellow; break;
    default:
      if ((oc == ocAmulet) || ( (1 << oc) & ocbMagic )) cb = cbBlue;
      else cb = 0;
      break;
  }
  st_append_cb(st, my_strdup(strbuf), truE, cb);
}

static one_caller void ocline_append(tScrollableText* st,
  const tObjectClass oc)
{ static char buf[50];
  const char* src = object_get_classname(oc);
  char ch, *dest = buf;
  *dest++ = (*src++) - ('a' - 'A'); /* uppercase first letter */
  while ( (ch = *src++) != '\0' ) *dest++ = ch;
  if (oc_pluralable(oc)) *dest++ = 's'; /* plural */
  *dest = '\0'; st_append_reverse(st, my_strdup(buf), truE);
}

static int inventory_sorter(const void* _a, const void* _b)
{ static const signed char v[maxObjectClass + 1] =
  { 20, 5, 14, 21, 3, 0, 1, 16, 17, 18, 19 }; /* for a "nicer" sorting */
  int retval;
  const tObject **a = (const tObject**) _a, **b = (const tObject**) _b;
  if ( (retval = v[(*a)->oc] - v[(*b)->oc]) == 0 )
    retval = (*a)->invidx - (*b)->invidx;
  return(retval);
}

static tBoolean inventory_key_handler(tScrollableText* st, const tKey key)
{ tBoolean retval = falsE;
  if (is_valid_invkey(key))
  { const tInventoryIndex ii = invkey2index(key);
    if (my_bit_test(invdata.selectables, ii))
    { st->data = roguedata.inventory[ii]; retval = truE; }
  }
  return(retval);
}

static const char* no_des_obj(const tObjectClassesBitmask classes)
{ sprintf(strbuf, "no objects of the desired kind%s in pack",
    (is_power_of_two(classes) ? strEmpty : strS));
  return(strbuf);
}

/*@null@*/ tObject* inventory(const unsigned char flags,
  const tObjectClassesBitmask classes)
/* displays the player character's inventory; <classes>: which object classes
   to show; "flags & 1": we're called from ask_for_invobj(), so we must not
   call message.c functions; "&2": allow entry (object) selection; "&4": show
   burden; "&8": sort by object classes, not by letters; "&16": only unpaid */
{ tObject* retval;
  tScrollableText st;
  tInventoryIndex i;
  const tBoolean dont_call_message_c = cond2boolean(flags & 1), may_select =
    cond2boolean(flags & 2), ocsort = cond2boolean(flags & 8), only_unpaid =
    cond2boolean(flags & 16);
  tBoolean found_anything, show_all, unpaid_x = falsE;
  tPriceTag unpaid_sum = 0;
  const char* partialstr;

  /* Pre-check the inventory */

  if (rogue->objects == NULL) /* the cheaper / more general check first */
  { if (dont_call_message_c) (void) beep();
    else message_show("nothing in pack");
    failed: retval = NULL; goto out;
  }
  if (!rogue_has_objectclasses(classes))
  { if (dont_call_message_c) (void) beep();
    else message_show(no_des_obj(classes));
    goto failed;
  }

  /* Now we can assume we have to output something "interesting". */

  my_memclr_var(invdata); st_setup(&st);
  if (may_select)
  { st.flags |= stfDisableLetterScrolling; invdata.user_may_select = truE;
    st.key_handler = inventory_key_handler;
    st.notice = "letter to select an entry";
  }
  show_all = cond2boolean((classes & ocbAll) == ocbAll);
  if (only_unpaid) { partialstr = "unpaid"; goto partial; }
  else if (show_all) st_append(&st, "Complete Inventory", falsE);
  else
  { char* ptr;
    partialstr = object_classes2chars(classes); partial: ptr = strbuf;
    si_strcpy_lit(ptr, "Partial Inventory \"");
    ptr = si_strcpy(ptr, partialstr);
    strcpy(ptr, strQuote); st_append(&st, my_strdup(strbuf), truE);
  }

  st_append_nl(&st); found_anything = falsE;
  strcpy(strbuf, "x (y) "); /* (x -> object-class char; y -> inventory char) */
  for_each_invindex(i)
  { const tObject* const object = roguedata.inventory[i];
    if (!is_invobject_interesting(object)) continue;
    if (only_unpaid)
    { const tPriceTag p = object->price_tag;
      if (price_is_unpaid(p))
      { if (p == 1) unpaid_x = truE;
        else unpaid_sum += p;
      }
      else continue;
    }
    if (ocsort) invdata.objsort[invdata.numsort++] = object;
    else invline_append(&st, object);
    found_anything = truE; my_bit_set(invdata.selectables, i);
  }
  if (!found_anything)
  { if (only_unpaid) st_append(&st, "no unpaid objects in pack", falsE);
    else st_append(&st, my_strdup(no_des_obj(classes)), truE);
    goto do_show;
  }
  if (ocsort)
  { tInventoryIndex num = invdata.numsort, count;
    tObjectClass prev_oc = ocInvalid, oc;
    if (num > 1) qsort(invdata.objsort, num,sizeof(tObject*),inventory_sorter);
    for (count = 0; count < num; count++)
    { const tObject* const object = invdata.objsort[count];
      oc = object->oc;
      if (prev_oc != oc) /* begin a new class */
      { prev_oc = oc; ocline_append(&st, oc); }
      invline_append(&st, object);
    }
  }
  if (flags & 4)
  { const short p = (roguedata.packweight * 100) / roguedata.maxpackweight;
    const char* str;
    if (p < 40) str = "light";
    else if (p < 70) str = "medium";
    else str = "high";
    sprintf(strbuf, "Burden: %s", str); st_append(&st, my_strdup(strbuf),truE);
  }
  else if (only_unpaid)
  { sprintf(strbuf, "Sum: %d pieces of gold%s", unpaid_sum, (unpaid_x ?
      "+x; just ask" : strEmpty));
    st_append(&st, my_strdup(strbuf), truE);
  }
  do_show: st_show(&st); retval = st.data; st_cleanup(&st);
  out: return(retval);
}

static void maybe_partial_inventory(const tKey key)
{ const char* const match = my_strchr(strObjectClassChars, (char) key);
  if (match != NULL) /* show partial inventory for that object class */
  { const int offset = match - strObjectClassChars;
    (void) inventory(0, 1 << offset);
  }
}

static tObjectQuantity __gold_pieces;

static void _gold_pieces(const tObject* object)
{ while (object != NULL)
  { const tObject* o;
    if (object->oc == ocGold)
    { const tObjectQuantity q = object->quantity;
      if (__gold_pieces > MAX_OBJECT_QUANTITY - q)
      { __gold_pieces = MAX_OBJECT_QUANTITY; break; }
      __gold_pieces += q;
    }
    else if ( (o = object->contents) != NULL ) _gold_pieces(o);
    object = object->next;
  }
}

static my_inline tObjectQuantity gold_pieces(void)
{ __gold_pieces = 0; _gold_pieces(rogue->objects); return(__gold_pieces); }

static one_caller void special_inventory(void)
{ char keylist[maxObjectClass + 10], *dest = keylist;
  const char* src = strObjectClassChars;
  tObjectQuantity oq;
  tKey key;
  *dest++ = '>'; *dest++ = '<'; *dest++ = 'g'; *dest++ = 'u';
  while ( (*dest++ = *src++) ) { /* copy */ }
  key = ask_for_key("which special inventory?", keylist);
  switch (key)
  { case '>': (void) inventory(0, ocbAll); break;
    case '<': (void) inventory(8, ocbAll); break;
    case 'g':
      oq = gold_pieces();
      sprintf(strbuf, "%s have %d piece%s of gold", rogue_pronoun(0), oq,
        ( (oq != 1) ? strS : strEmpty ));
      message_show(strbuf); break;
    case 'u': (void) inventory(16, ocbAll); break;
    default: maybe_partial_inventory(key); break;
  }
}

static one_caller void age_rogue_inventory(void)
/* ages all objects in the player character's inventory */
{ tObject* object = rogue->objects;
  while (object != NULL)
  { tObject* const next = object->next; /* <object> might be deallocated... */
    tFewTicks arlt; /* "approximate remaining lifetime ticks" */
    object_age_basic(object, 1);
    object->lifetime = LIMSUB(object->lifetime, aaCarry);
    if ( (object->oc == ocTool) && (object->kind == tokLantern) &&
         (object->flags & ofBeingLit) && (object_still_exists(object)) &&
         ( (arlt = object->lifetime / aaLitLantern) < 50 ) &&
         (randpercent(5 - arlt / 10 + difficulty)) )
    { object_light_off(object); roguedata.flags |= rfInvPropertyChanged;
      if (rogue_can_see)
      { object_get_description(object, strbuf);
        sprintf(strbuf + strlen(strbuf), " (%c) flickers",invobj2char(object));
        message_show(strbuf);
      }
    }
    creature_check_object(rogue, object);
    object = next;
  }
}

static my_inline void yumyuk(const tBoolean yum)
/*prints a little message which should depend on the enchantment of an object*/
{ message_show(yum ? "yum!" : "yuk!");
}

static tBoolean cant_eatquaff_with_shawl(const tBoolean eat)
{ tBoolean retval = falsE; /* assume no problem */
  if ( (difficulty >= dkNormal) || (!eat) )
  { /* not being able to eat with a non-removable (cursed) shawl for a long
       time can be deadly, so this is limited to higher difficulties; in lower
       difficulties, let's say that the shawl can be moved a little bit for a
       moment so that eating/quaffing is possible */
    if (roguedata_os(rdosiShawl) != NULL)
    { sprintf(strbuf, "can't %s while applying a shawl", (eat?"eat":"quaff"));
      message_show(strbuf); retval = truE; goto out;
    }
  }
  out: return(retval);
}

static tBoolean handle_unpaid(tObject* object)
{ const tPriceTag p = object->price_tag;
  const tBoolean is_unpaid = cond2boolean(price_is_unpaid(p)), in_shop =
    cond2boolean(rogue_in_shop), retval = cond2boolean(is_unpaid && in_shop);
  if (retval)
  { sprintf(strbuf, "%s didn't yet pay for ", rogue_pronoun(0));
    (void) object_get_minidesc(object, strbuf + strlen(strbuf), 1, NULL);
    message_show(strbuf);
    if (difficulty > dkNormal) infuriate_shopkeeper(NULL, falsE);
  }
  else if (p < 0)
  { /* the player wants to change the object, so the old offer may become
       inappropriate (esp. too high); shopkeepers are careful */
    object->price_tag = 0; roguedata.flags |= rfInvPropertyChanged;
  }
  return(retval);
}

static one_caller void eat(void)
{ tObject* object;
  tPropertyExtent ench;
  tFoodKind kind;
  tFewTicks max;
  tCbf cbf;

  if (!rogue_has_objectclasses(1 << ocFood))
  { message_show("don't have any food to eat"); return; }
  if (cant_eatquaff_with_shawl(truE)) return;

  if ( (object = ask_for_invobj("eat what?", 1 << ocFood)) == NULL ) return;
  if (handle_unpaid(object)) return;
  switch (kind = object->kind)
  { case fkCorpse:
      switch (object->objdetail)
      { case ccBouncer: case ccVarydamage: cbf = cbfFoodVegan; break;
        default: cbf = cbfFoodAny; break;
      }
      break;
    default: cbf = cbfFoodVegan; break;
  }
  if ( (roguedata.cbf & cbfFoodmask) < cbf ) /* conduct deteriorated */
    roguedata.cbf = (roguedata.cbf & ~cbfFoodmask) | cbf;
  if (kind == fkCorpse)
  { if (!creature_dec_health(rogue, difficulty / 2 + randbytefromrange(1, 2)))
      rogue_died(drPoison, NULL);
    if (object->objdetail == ccBat) /* cf. NetHack */
      creature_confuse(rogue, 5 * difficulty + randbytemod(5) + 1);
    ench = -10; goto finish;
  }
  ench = object->enchantment;

  max = 500 - 100 * difficulty;
  if (roguedata.actions_left > max) roguedata.actions_left = max;
    /* (to prevent the player from eating tons of food "in advance") */
  roguedata.actions_left += 1000 + 100 * ench + sectionsizemeasure / 100 +
    randfewticksfromrange(0, 150 - 20 * difficulty);
  /* IMPLEMENTME: the change should depend on the lifetime of the object, and
     food may be "rotten" if the lifetime is near 0. And different food kinds
     should have different "nutrition" values. And food should have different
     "sizes", so that eating takes different amounts of time. */
  rogue_checkactionsleft();

  if (ench >= 0)
  { const tMagicEnergy e = zero2one(ench);
    rogue_incmagicen((pref_role == roleWizard) ? (2 * e) : e);
  }

  finish: yumyuk(cond2boolean(ench >= 0));
  if ( (kind == fkSpinach) && (ench >= 0) )
  { tStatisticsValidityFlags svf = svfNone;
    tStrength str = rogue->strength + (6 - difficulty) + ench;
    if (str > 99) str = 99;
    if (str > roguedata.maxstrength)
    { if (roguedata.maxstrength < 99)
      { roguedata.maxstrength++; svf |= svfMaxStrength; }
      str = roguedata.maxstrength;
    }
    if (rogue->strength != str)
    { rogue->strength = str; svf |= svfStrength;
      sprintf(strbuf, "%s feel like Popeye now!", rogue_pronoun(0));
      message_show(strbuf); /* shamelessly stolen from NetHack :-) */
      /* IMPLEMENTME: different text if hallucinating! */
    }
    if (svf != svfNone) statistics_invalidate(svf);
  }
  else if ( (kind == fkCarrot) && (ench >= 0) ) creature_unblind(rogue);
  roguedata.packweight -= object_weight(object);
  object->quantity--;
  if (object->quantity > 0) roguedata.packweight += object_weight(object);
  creature_check_object(rogue, object); rogue_las(cakEat, 0);
}

static one_caller void zap(void)
{ tObject* object;
  tDirection dir;
  tBoolean do_zap;
  if (!rogue_has_objectclasses(1 << ocWandStaff))
  { message_show("don't have any wand for zapping"); return; }
  if ( (object = ask_for_invobj("zap with what?", 1 << ocWandStaff)) == NULL )
    return;
  if (handle_unpaid(object)) return;
  /* IMPLEMENTME: allow zapping at arbitrary angles! */
  if (!ask_for_dir(&dir, NULL))
  { if (difficulty <= dkNormal) return; /* simply "accept" the cancellation */
    message_show("the magical energy is released!");
    rogue_decmagicexp(1); dir = rogue->lastdir;
    if (dir == dirNone) { dir = randdir(); goto after_conf; }
  }
  if (randpercent(10*rogue->confused)) { rogue_decmagicexp(1);dir = randdir();}
  after_conf: {}
  if ( (object->enchantment < 0) && (difficulty >= dkNormal) && (pref_feat > 0)
    && (randpercent(3 * difficulty + 2 * pref_magic - 5 * object->enchantment -
    roguedata.magic_exp / 4 + current_storeynum / 3)) )
  { /* IMPLEMENTME: let the wand explode and vanish; or, if metal, let it have
       a bad electrical effect! */
  }

  do_zap = falsE;
  if (object->lifetime >= 1000)
  { /* the wand/staff has enough energy itself */
    if (roguedata.magic_energy >= 1)
    { object->lifetime -= 1000; rogue_decmagicen(1); do_zap = truE; }
  }
  else if (object->lifetime >= 500)
  { /* the wand/staff has little energy, so zapping costs more */
    if (roguedata.magic_energy >= 2)
    { object->lifetime -= 500; rogue_decmagicen(2); do_zap = truE; }
  }

  if (do_zap) (void) creature_zap(rogue, object, dir);
  else
  { object->lifetime -= 100; /* even a failing attempt shall cost lifetime */
    nothing_happens();
  }
  if (object->flags & ofChargeIdentified)
    roguedata.flags |= rfInvPropertyChanged;
  creature_check_object(rogue, object); rogue_las(cakZap, 0);
}

static void strengthmsg(const tBoolean stronger)
{ sprintf(strbuf, strFeelNow, rogue_pronoun(0),
    (stronger ? "stronger" : "feebler" ));
  message_show(strbuf);
}

static void healthmsg(const tBoolean better)
{ sprintf(strbuf, strFeelNow, rogue_pronoun(0), (better ? "better" : "worse"));
  message_show(strbuf);
}

static my_inline const char* hallu_stranorm(void)
{ return(rogue->hallucinating ? "normal" : "strange");
}

void strangemsg(void)
{ sprintf(strbuf, "%s have a %s feeling for a moment", rogue_pronoun(0),
    hallu_stranorm());
  message_show(strbuf); message_show(strItPasses);
}

static void laughtermsg(void)
{ rogue_decmagicexp(1);
  if (rogue_can_hear)
  { sprintf(strbuf, "%s hear a maniacal laughter in the distance",
      rogue_pronoun(0));
    message_show(strbuf);
  }
  else strangemsg();
}

static one_caller void quaff(void)
/* The player quaffs a potion here - it's never any kind of beer... :-) */
{ tObject* potion;
  tPropertyExtent ench;
  tObjectKind kind;
  tBoolean kind_id;
    /* (whether the player can identify the potion kind from the resulting
        changes/messages) */
  tBoolean yum;
  tStatisticsValidityFlags svf;
    /* (handling a bit sloppy - sometimes invalidating more than necessary) */

  if (!rogue_has_objectclasses(1 << ocPotion))
  { message_show("don't have any potion to quaff"); return; }
  if (cant_eatquaff_with_shawl(falsE)) return;
  if ( (potion=ask_for_invobj("quaff what?", 1 << ocPotion)) == NULL ) return;
  if (handle_unpaid(potion)) return;

  kind = potion->kind; kind_id = falsE; ench = potion->enchantment;
  yum = cond2boolean(ench >= 0); /* (might be changed later) */

  if (kind == pkIncreaseStrength)
  { tBoolean msg = truE;
    tStrength str = rogue->strength + zero2one(ench);
    svf = svfNone;
    if ( (str < rogue->strength) && (creature_get_sustain_strength(rogue)) )
      str = rogue->strength;
    else if (str < 1) str = 1; /* CHECKME: die instead? */
    else if (str > 99) str = 99;
    if (rogue->strength != str)
    { strengthmsg(cond2boolean(rogue->strength < str));
      rogue->strength = str; svf |= svfStrength; msg = falsE;
    }
    if (roguedata.maxstrength < str)
    { roguedata.maxstrength = str; svf |= svfMaxStrength; }
    if (svf != svfNone) { statistics_invalidate(svf); kind_id = truE; }
    if (!msg) goto no_message;
  }
  else if (kind == pkRestoreStrength)
  { if (ench >= 0)
    { if (rogue->strength < roguedata.maxstrength)
      { rogue->strength = roguedata.maxstrength;
        statistics_invalidate(svfStrength);
        strengthmsg(truE); kind_id = truE;
      }
    }
    else if ( (rogue->strength > 1) && (!creature_get_sustain_strength(rogue)))
    { rogue->strength--; statistics_invalidate(svfStrength);
      strengthmsg(falsE); kind_id = truE;
    }
  }
  else if (kind == pkIncreaseExp)
  { tExperience exp = rogue->experience;
    if (ench == 0) exp++;
    else exp += 2 * ench;
    if (exp < 0) exp = 0;
    else if (exp > 99) exp = 99;
    if (rogue->experience != exp)
    { rogue->experience = exp;
      statistics_invalidate(svfExperience);
      strangemsg(); kind_id = truE;
    }
  }
  else if (kind == pkHeal)
  { tHealth health = rogue->health + roledata.healing + zero2one(ench);
    if (health < 1) health = 1; /* be nice to the player :-) */
    else if (health > roguedata.maxhealth) health = roguedata.maxhealth;
    if (rogue->health != health)
    { healthmsg(cond2boolean(rogue->health < health)); kind_id = truE;
      rogue->health = health; statistics_invalidate(svfHealth);
    }
  }
  else if (kind == pkGreatlyHeal) /* no enchantment here */
  { tHealth health = rogue->health;
    svf = svfHealth;
    health += 5 + 2 * roledata.healing;
    if (health > roguedata.maxhealth + 2) health = roguedata.maxhealth + 2;
    if (health > 99) health = 99;
    if (roguedata.maxhealth < health)
    { roguedata.maxhealth = health; svf |= svfMaxHealth; }
    statistics_invalidate(svf); creature_unblind(rogue);
    healthmsg(truE); kind_id = truE;
    if ( (roguedata.petrifying > 0) && (difficulty <= dkNormal) )
      cancel_petrifying();
  }
  else if (kind == pkMagicEnergy) /* no enchantment here */
  { rogue_incmagicen(10 - difficulty + roledata.magic + randbytemod(5));
    kind_id = truE;
  }
  else if (kind == pkPoison) /* no enchantment here */
  { if (!creature_dec_health(rogue, 2 + randbytemod(MIN(3 - roledata.healing,
      difficulty))))
    { rogue_died(drPoison, NULL); kind_id = truE; }
    else healthmsg(falsE);
    creature_unhallucinate(rogue); goto no_message;
  }
  else if (kind == pkLiquidFire) /* no enchantment here */
  { if (!creature_dec_health(rogue, 4 - roledata.healing + randbytemod(2)))
    { rogue_died(drFire, NULL); kind_id = truE; }
    else healthmsg(falsE);
    goto no_message;
  }
  else if (kind == pkConfuse)
  { if (!rogue->confused) kind_id = truE;
    creature_confuse(rogue, 20 + 5 * difficulty - 5 * ench);
    yum = falsE;
  }
  else if (kind == pkHallucinate)
  { tBoolean msg;
    if (!rogue->hallucinating) { kind_id = truE; msg = falsE; }
    else { msg = truE; }
    creature_hallucinate(rogue, 50 + 10 * difficulty - 10 * ench);
    if (!msg) goto no_message;
    else yum = falsE;
  }
  else if (kind == pkBlindness)
  { tFewTicks t = 2 * current_storeynum + 5 * difficulty - 10 * ench;
    if (t < 5) t = 5;
    else if (t > 50) t = 50;
    creature_blind(rogue, t + randbytemod(5));
    kind_id = truE; goto no_message;
  }
  else if (kind == pkSenseObjects)
  { if (ench >= 0)
    { if (!(roguedata.flags & rfSenseObjects))
      { roguedata.flags |= rfSenseObjects; kind_id = truE; }
    }
    else
    { if (roguedata.flags & rfSenseObjects)
      { roguedata.flags &= ~rfSenseObjects; kind_id = truE; }
    }
    if (section_get_objects() == NULL) strangemsg();
  }
  else if (kind == pkSenseCreatures)
  { if (ench >= 0)
    { if (!(rogue->flags & cfSensCreat))
      { rogue->flags |= cfSensCreat; kind_id = truE; }
    }
    else
    { if (rogue->flags & cfSensCreat)
      { rogue->flags &= ~cfSensCreat; kind_id = truE; }
    }
    if (section_get_creatures() == NULL) strangemsg();
  }
  else if (kind == pkLevitate)
  { if (!rogue->levitated) kind_id = truE;
    creature_levitate(rogue, 30 + 5 * ench); yum = truE;
  }
  else if (kind == pkSlow)
  { const tFewTicks old = rogue->slowedhasted;
    creature_slow(rogue, 50 - 10 * ench); yum = falsE;
    if (SIGN_CHANGED(old, rogue->slowedhasted)) kind_id = truE;
  }
  else if (kind == pkHaste)
  { const tFewTicks old = rogue->slowedhasted;
    creature_haste(rogue, 30 + 10 * ench); yum = truE;
    if (SIGN_CHANGED(old, rogue->slowedhasted)) kind_id = truE;
  }
  else if (kind == pkMakeSupple)
  { if (ench >= 0)
    { if (!(rogue->flags & cfSupple)) { rogue->flags|=cfSupple; kind_id=truE; }
      if ( (ench > 0) && (roguedata.petrifying > 0) )
      { cancel_petrifying(); kind_id = truE; }
    }
    else
    { if (rogue->flags & cfSupple) { rogue->flags &= ~cfSupple; kind_id=truE;}}
    if ( (kind_id) || (potionkind_id[kind]) ) strangemsg();
  }
#if CONFIG_DEBUG
  else
  { sprintf(debugstrbuf, "BUG: quaff(%d)", kind);
    message_showdebug(debugstrbuf);
  }
#endif

  yumyuk(yum); no_message: {}

  if ( (yum) && (kind != pkMagicEnergy) && (roguedata.magic_energy < 99) )
  { tMagicEnergy energy = roledata.magic / 2 + 1;
    if ( (ench > 0) && (randpercent(90 - 20 * difficulty + 15 * ench)) )
      energy++;
    rogue_incmagicen(energy);
  }

  roguedata.packweight -= object_weight(potion); potion->quantity--;
  if (potion->quantity > 0) roguedata.packweight += object_weight(potion);
  if (kind_id) potionkind_id[kind] = truE;
  creature_check_object(rogue, potion); rogue_las(cakQuaff, 0);
}

static tBoolean search_for_hidden(const tBoolean magic)
/* searches for hidden stuff around the player character and on his square */
{ tCoordinate xmax = current_section->width - 1, ymax = current_section->height
    - 1, x0 = rogue->x, y0 = rogue->y, x, y;
  const tObject *ring, *glasses;
  tBoolean found_something = falsE;
  tPercentage perc = 40 - 10 * difficulty + (magic ? roguedata.magic_exp :
    rogue->experience) / 2 + rogue_rings2fortune(10) - current_storeynum / 2;
  unsigned char ringcount;
  tPropertyExtent ench;

  if ( (magic) || (pref_role == roleWizard) ) perc += 40;
  else if (pref_role == roleThief) perc += 25;
  if (pref_gender == cgkFemale) perc += 11 - 2 * difficulty;
  /* (general fortune rings: factor 10; specific search rings/glasses: factor
      20; strong <magic>: factor 40) */
  worn_rings_enchsumcount(ench, ring, rkSearch, ringcount);
  if ( (ringcount > 0) && (ench == 0) ) ench = 1; /* at least _some_ effect */
  if (ench != 0) perc += 20 * ench;
  if ( (glasses = roguedata_os(rdosiGlasses)) != NULL )
  { if (glasses->kind != tokGlassesSearch) glasses = NULL; /* wrong kind */
    else perc += 20 * zero2one(glasses->enchantment);
  }
  if (perc <= 0) goto finish; /* no chance */

  /* IMPLEMENTME: if in a room and (magic || has non-cursed search glasses),
     search the entire room at once, at least in non-hard difficulties! */
  for (y = MAX(y0 - 1, 0); y <= MIN(y0 + 1, ymax); y++)
  { for (x = MAX(x0 - 1, 0); x <= MIN(x0 + 1, xmax); x++)
    { if ( ((squareflags_get(x, y) & sfHidden)) && (randpercent(perc)) )
      { section_unhide_square(x, y); found_something = truE; }
    }
  }
  finish:
  roguedata.flags |= rfIsSearching; roguedata.digestcount++;
  return(found_something);
}

static one_caller tBoolean identify(void)
/* only called from do_cast_spell(); lets the player identify objects; returns
   whether the player actually identified something (truE) or cancelled resp.
   already knew the kind (falsE). */
{ tBoolean retval;
  tObject* object;
  tObjectClass oc;
  tObjectKind kind;
  tObjectDetail detail;

  if ( (object = ask_for_invobj("identify what?", ocbAll)) == NULL )
  { non_id: retval = falsE; goto out; }
  if (handle_unpaid(object)) goto non_id;
  roguedata.cbf |= cbfDidTryIdentify; oc = object->oc; kind = object->kind;

  switch (oc)
  { case ocAmulet:
      detail = object->objdetail;
      if (AMULET_FAKEKNOWN(detail)) goto idd;
      object->objdetail |= _AMULET_FAKEKNOWN; goto finish;
      /* IMPLEMENTME: amuletkind_id[]! */
      /*@notreached@*/ break;
    case ocTool: /* somewhat special (partly magic, partly not) */
      if (toolkind_id[kind]) goto idd;
      else { toolkind_id[kind] = truE; goto finish; }
      break;
  }

  if (!((1 << oc) & ocbMagic)) /* not a magic object class */
  { tObjectFlags old_id;
    idd:
    old_id = object->flags & ofFullyIdentified;
    if (old_id == ofFullyIdentified)
    { (void) object_get_minidesc(object, strbuf, 1,
        "you already know everything about ");
      message_show(strbuf); goto non_id;
    }
    else
    { object->flags |= ofFullyIdentified;
      if ( (rogue->armor == object) && (!(old_id & ofDamageIdentified)) )
        statistics_invalidate(svfArmor); /* now damage has been identified */
      goto finish;
    }
  }

  switch (oc)
  { case ocPotion:
      if ( (object->flags&ofKnownMaterial) && (potionkind_id[kind]) ) goto idd;
      else { object->flags |= ofKnownMaterial; potionkind_id[kind] = truE; }
      break;
    case ocWandStaff:
      if ((object->flags&ofKnownMaterial) && (wandstaffkind_id[kind]))goto idd;
      object->flags |= ofKnownMaterial; wandstaffkind_id[kind] = truE; break;
    case ocScroll:
      if ( (object->flags&ofKnownMaterial) && (scrollkind_id[kind]) ) goto idd;
      object->flags |= ofKnownMaterial; scrollkind_id[kind] = truE; break;
    case ocRing:
      if ( (object->flags & ofKnownMaterial) && (ringkind_id[kind]) ) goto idd;
      object->flags |= ofKnownMaterial; ringkind_id[kind] = truE; break;
#if CONFIG_DEBUG
    default:
      sprintf(debugstrbuf, "BUG: identify(%d,%d)", oc, kind);
      message_showdebug(debugstrbuf); goto non_id; /*@notreached@*/ break;
#endif
  }
  finish:
  rogue_incmagicexp(1); roguedata.flags |= rfInvPropertyChanged;
  sprintf(strbuf, strBracedCharSpace, invobj2char(object));
  object_get_description(object, strbuf + 4);
  message_show(strbuf); retval = truE;
  out: return(retval);
}

static tBoolean do_magic_sleepfreeze(const tPropertyExtent ench,
  const tBoolean is_kind_sleep)
/* performs magically caused sleeping and freezing effects */
/* IMPLEMENTME for freezing: don't let only creatures freeze, but also any
   sfWater become sfGlazedFrost; stop lekWater, lekFire and lekLava events;
   ccAquator should become ccIcemonster? */
{ const tRoomIndex r = creature_room(rogue);
  const tBoolean in_room = cond2boolean(r != roomNone);
  tFewTicks ticks;
  tCreature* creature;
  tBoolean kind_id = truE;

  if ( (in_room) && (ench >= 0) )
  { tBoolean found = falsE;
    ticks = randfewticksfromrange(0, current_storeynum / 5) +
      ( (is_kind_sleep) ? (3 + ench) : (2 + ench / 2) );
    for_each_currentsectioncreature(creature)
    { if (creature->flags & cfDead) continue;
      if (creature_room(creature) == r)
      { if (is_kind_sleep) creature_nap(creature, ticks, 0);
        else creature_freeze(creature, ticks);
        found = truE;
      }
    }
    if (!found) { laughtermsg(); kind_id = falsE; }
  }
  else
  { ticks = randfewticksfromrange(0, 2 + difficulty / 2) +
      2 + current_storeynum / 5 + boolean2bool(is_kind_sleep);
    if (in_room) /* implies "ench < 0" */
    { for_each_currentsectioncreature(creature)
      { if (creature->flags & cfDead) continue;
        if (creature_room(creature) == r)
        { if (is_kind_sleep) creature_wake_up(creature, falsE);
          else creature_unfreeze(creature);
        }
      }
    }
    else laughtermsg(); /* tried to read this scroll outside any room */
    if (is_kind_sleep) creature_nap(rogue, ticks, 0);
    else creature_freeze(rogue, ticks);
  }
  return(kind_id);
}

static unsigned char do_cast_spell(const tScrollKind kind,
  /*@null@*/ tObject* scr, const tPropertyExtent ench)
{ unsigned char retval = 0;
    /* "retval & 1": kind identified; "& 2": don't decrease lifetime;
       "& 4": do tick (otherwise player cancelled) */
  tBoolean kind_id = truE, is_blind = cond2boolean(!rogue_can_see),
    shall_tick = truE;
  /* IMPLEMENTME: let the spell-casting fail sometimes when the player
     character wears metal armor or a metal helmet/gloves! */
  if (kind == skIdentify) { if (!identify()) kind_id = shall_tick = falsE; }
  else if (kind == skProtectArmor)
  { tObject* const armor = rogue->armor;
    if (armor == NULL)
    { skin_crawls:
      sprintf(strbuf, "%s skin crawls", rogue_pronoun(2));
      message_show(strbuf);
    }
    else
    { if (ench >= 0) armor->flags |= ofProtected;
      else armor->flags &= ~ofProtected;
      armor_glows:
      if (is_blind) kind_id = falsE;
      else
      { sprintf(strbuf, "%s armor glows %s for a moment", rogue_pronoun(2),
          enchantmentcolorstr);
        message_show(strbuf);
      }
    }
  }
  else if (kind == skEnchantArmor)
  { tObject* const armor = rogue->armor;
    tPropertyExtent ench2;
    if (armor == NULL) goto skin_crawls;
    ench2 = armor->enchantment + zero2one(ench);
    if (ench2 < -10) ench2 = -10;
    else if (ench2 > 10) ench2 = 10;
    armor->enchantment = ench2;
    goto armor_glows;
  }
  else if (kind == skEnchantWeapon)
  { tObject* const weapon = rogue->weapon;
    if (weapon == NULL)
    { sprintf(strbuf, "%s hands tingle", rogue_pronoun(2));
      message_show(strbuf);
    }
    else if (!object_is_enchantable(weapon)) { laughtermsg(); kind_id = falsE;}
    else
    { tPropertyExtent ench2 = weapon->enchantment + zero2one(ench);
      if (ench2 < -10) ench2 = -10;
      else if (ench2 > 10) ench2 = 10;
      weapon->enchantment = ench2;
      if (is_blind) kind_id = falsE;
      else
      { const tBoolean pl = object_get_minidesc(weapon, strbuf, 1, NULL);
        sprintf(strbuf + strlen(strbuf), " glow%s %s for a moment", (pl ?
          strEmpty : strS), enchantmentcolorstr);
        message_show(strbuf);
      }
    }
  }
  else if (kind == skTeleport)
  { if ( (rogue_in_shop) && (rogue_has_unpaid_items) )
      infuriate_shopkeeper(NULL, truE);
    else creature_teleport(rogue);
  }
  else if (kind == skRemoveCurse)
  { tObject* o;
    tBoolean is_first_kindchange = truE,
      always = cond2boolean(difficulty <= dkNormal);
    const tPercentage perc = 80 + 10 * (ench - difficulty) + 5*roledata.magic;
    /* Print a message... */
    if (rogue->hallucinating)
    { sprintf(strbuf, "%s feel in touch with the universal oneness",
        rogue_pronoun(0));
    }
    else
    { sprintf(strbuf, "%s feel as though someone is watching over %s",
        rogue_pronoun(0), rogue_pronoun(1));
    }
    message_show(strbuf);
    /* ...and treat the objects */
    for_each_object(rogue->objects, o)
    { tObjectKind maxkind;
      if ( (always) || (randpercent(perc)) )
      { if (o->enchantment < 0)
        { if (o != scr) o->enchantment = 0; /* (can't remove _own_ curse) */
          o->flags &= ~ofProtected; /* the disadvantage :-) */
        }
        if (ench >= 0) o->flags &= ~(ofRusts | ofWormy);
        if ( (o->oc == ocScroll) && (o->kind == skBlankPaper) &&
             (randpercent(100 - 20*difficulty + 10*ench)) )
        { maxkind = maxScrollKind;
          change_kind:
          o->kind = randbytemod((maxkind + 1) - 1);
            /* yes: no kind restriction (except we don't wanna get
               skBlankPaper/wskDoNothing/rkDoNothing/amkDoNothing again, of
               course) */
          if (is_first_kindchange)
          { is_first_kindchange = falsE;
            sprintf(strbuf, "%s skin itches", rogue_pronoun(2));
            message_show(strbuf);
          }
        }
        else if ( (o->oc == ocWandStaff) && (o->kind == wskDoNothing) &&
                  (randpercent(50 - 10*difficulty + 5*ench)) )
        { maxkind = maxWandStaffKind; goto change_kind; }
        else if ( (o->oc == ocRing) && (o->kind == rkDoNothing) &&
                  (o->flags & ofOnEitherHand) && (randpercent(50 + 5*ench)) )
        { maxkind = maxRingKind; goto change_kind; }
        /* IMPLEMENTME: amulets! */
      }
    }
  }
  else if (kind == skScare)
  { /* _Reading_ this kind of scroll is the wrong thing to do... */
    laughtermsg(); kind_id = falsE;
    if (scr != NULL) { sl0: scr->lifetime = 0; retval |= 2; }
  }
  else if (kind == skSleep) kind_id = do_magic_sleepfreeze(ench, truE);
  else if (kind == skFreeze) kind_id = do_magic_sleepfreeze(ench, falsE);
  else if (kind == skMap)
  { const tBoolean is_ench = cond2boolean(ench >= 0);
    /* IMPLEMENTME: if ( (scr != NULL) && (scr->detail > 0) ), this is a
       storey number; the map only works in (one arbitrary section of) that
       storey. Mention the number in the scroll object description: put the
       individual digits between the syllables! Using such a scroll in a
       "wrong" storey gets punished, and the scroll crumbles. (Delete ->detail
       in any algorithm which magically changes the scroll kind...) */
    /* IMPLEMENTME: if (scr == NULL) { only provide a _partial_ map }! */
    map_magic(is_ench);
    if ( (scr != NULL) && (!is_blind) )
    { sprintf(strbuf, "the scroll seems to have a %s map on it",
        (is_ench ? "detailed" : "confusing"));
      message_show(strbuf);
    }
    if (!is_ench) laughtermsg();
  }
  else if (kind == skSenseHidden)
  { tBoolean found_something = search_for_hidden(truE);
    if (section_reveal_objects()) found_something = truE;
    if (!found_something) { strangemsg(); kind_id = falsE; }
    else
    { rogue_incmagicexp(1);
      sprintf(strbuf, "%s sense a mild touch of wind", rogue_pronoun(0));
      message_show(strbuf);
    }
  }
  else if (kind == skLight) /* a _very_ strange kind of scroll/spell :-) */
  { const tRoomIndex r = creature_room(rogue);
    if (r == roomNone) { laughtermsg(); kind_id = falsE; }
    else section_light_room_up(r);
    if (is_blind) kind_id = falsE;
    if (ench < 0)
    { if (!is_blind) (void) rogue_dazzle(falsE);
      if (scr != NULL) scr->enchantment++; /* (yes, really!) */
    }
  }
  else if (kind == skRecharge)
  { tObject* o;
    for_each_object(rogue->objects, o)
    { const tObjectClass oc = o->oc;
      if ( (oc == ocScroll) && (o->kind == skRecharge) )
        continue; /* prevent infinite self-recharging or mutual recharging */
      if (ocbMagic & (1 << oc))
      { tPropertyExtent ench2 = o->enchantment + cond2sign(ench >= 0);
        if (ench2 < -3) ench2 = -3;
        else if (ench2 > 3) ench2 = 3;
        o->enchantment = ench2;
        o->lifetime += (1500 + randfewticksfromrange(0, 100)) * zero2one(ench);
        creature_check_object(rogue, o);
      }
    }
    /* CHECKME: kind_id? */
  }
  else if (kind == skHold)
  { if (ench < 0) /* cursed - player character is held */
    { if (rogue->held) { strange_hold: strangemsg(); kind_id = falsE; }
      else
      { rogue->held = MAX(difficulty, 2) + randbytemod(2 - ench);
        roguedata.holding_cause = hcOtherClasp;
        message_show("something holds");
      }
    }
    else /* creatures near the player character are held */
    { tBoolean found = falsE;
      tCoordinate x = rogue->x, y = rogue->y,
        dist = 1 + boolean2bool(randpercent(20 * ench + roguedata.magic_exp));
      const tFewTicks ticks = 3 + zero2one(ench) + roguedata.magic_exp/10;
      tCreature* creature;
      for_each_currentsectioncreature(creature)
      { if ( (!(creature->flags & cfDead)) && (ABS(creature->x - x) <= dist) &&
             (ABS(creature->y - y) <= dist) )
        { creature->held += ticks + randbytemod(3); found = truE; }
      }
      if (!found) goto strange_hold;
    }
  }
  else if (kind == skAggravate) { if (!creature_aggravate()) kind_id = falsE; }
  else if (kind == skHeat)
  { nothing_happens();
    /* IMPLEMENTME: let any sfWater evaporate, stop any water-generating
       lekWater events, let some ccAquator evaporate, let sfGlazedFrost melt so
       that it becomes water or vanishes! */
  }
  else if (kind == skWisdom)
  { nothing_happens(); goto sl0;
    /* IMPLEMENTME: provide short information about some general tricks, e.g.
       traps, pen, quarks, stone tables; in certain storeys, tell how to reach
       a secret section; details should depend on pref_feat! */
  }
  else if (kind == skPetrify)
  { nothing_happens(); goto sl0; /* IMPLEMENTME! */
  }
#if 0
  else if (kind == skBlankPaper) { /* already handled! */ }
#endif
  else /* "should not happen" */
  { nothing_happens();
#if CONFIG_DEBUG
    sprintf(debugstrbuf, "BUG: do_cast_spell(%d)", kind);
    message_showdebug(debugstrbuf);
#endif
  }
  if (kind_id) retval |= 1;
  if (shall_tick) retval |= 4;
  else retval |= 2;
  return(retval);
}

static one_caller void cast_spell(void)
{ const tObjectKind kind = object_dump_associations(2); /* ask which spell */
  if (kind == okNone) return; /* player cancelled */
  if (do_cast_spell(kind, NULL, 0) & 4)
  { rogue_decmagicen(object_spellcastingcost(kind)); rogue_las(cakCast, 0); }
}

static one_caller void read_scroll(void)
{ tObject* scr;
  tObjectKind kind;
  tBoolean kind_id = falsE, /* whether the player identified the scroll kind */
    shall_tick = truE; /* do a dungeon clock tick? */
  unsigned char casting_retval;

  if (!rogue_can_read()) return;
  if (!rogue_has_objectclasses(1 << ocScroll))
  { message_show("don't have any scroll to read"); return; }
  /* IMPLEMENTME: if (too dark) { message_show("it's too dark here");return;}*/
  if ( (scr = ask_for_invobj("read what?", 1 << ocScroll)) == NULL ) return;
  if (handle_unpaid(scr)) return;

  kind = scr->kind;
  if (kind == skBlankPaper) /* very special case */
  { message_show("this seems to be a dirty piece of paper");
    kind_id = truE; goto done;
  }

  roguedata.cbf |= cbfDidRead;
  if (roguedata.magic_energy < 1)
  { nothing_happens();
    scr->lifetime -= 100; /* even a failing attempt costs lifetime... */
    goto check_entry;
  }
  if (scr->lifetime < 1000) /* not enough "power" left */
  { scr->lifetime = 0;
      /* (now a subsequent creature_check_object() prints a nice message:-) */
    goto check_entry;
  }

  if ( (rogue->confused) && (randpercent(10 * difficulty + 5 * rogue->confused
    - 2 * roguedata.magic_exp)) && (object_confused_scrollkind(&kind)) )
  { sprintf(strbuf, "%s mispronounce the magic words", rogue_pronoun(0));
    message_show(strbuf);
  }

  if ( (kind == skIdentify) && (!scrollkind_id[skIdentify]) )
  { scrollkind_id[kind] = truE; /* (_must_ be done here...) */
    roguedata.flags |= rfInvPropertyChanged;
    message_show("this is a scroll of identify");
  }

  casting_retval = do_cast_spell(kind, scr, scr->enchantment);
  if (casting_retval & 1)
  { if (scr->flags & ofChargeIdentified)
      roguedata.flags |= rfInvPropertyChanged;
    kind_id = truE;
  }
  if (!(casting_retval & 2)) scr->lifetime -= 1000;
  shall_tick = cond2boolean(casting_retval & 4);
  if (shall_tick) rogue_decmagicen(1); /* player actually did something */

  check_entry:
  creature_check_object(rogue, scr);
  done:
  if (kind_id) scrollkind_id[kind] = truE;
  if (shall_tick) rogue_las(cakScroll, 0);
}

static void cant_remove_cursed(const tObject* object)
{ (void) object_get_minidesc(object, strbuf, 2, "can't remove ");
  message_show(strbuf);
}

static my_inline void cant_unwield(void)
{ tObject* const w = rogue->weapon;
  if (!(w->flags & ofEnchIdentified))
  { w->flags |= ofEnchIdentified; roguedata.flags |= rfInvPropertyChanged; }
  message_show("can't unwield a cursed weapon");
}

static one_caller void wield(void)
{ tObject *object, *old_weapon;
  if (!rogue_has_objectclasses(1 << ocWeapon))
  { message_show("don't have any weapon to wield"); return; }
  if ( ( (old_weapon=rogue->weapon) != NULL ) && (old_weapon->enchantment < 0))
  { /* (We do this check already _here_ because it would be unfriendly to ask
        the user for an inventory key only to tell him everything was useless;
        it's better to do it here, despite the fact that we let the user know
        ofEnchIdentified possibly unnecessarily...) */
    cant_unwield(); return;
  }
  if ( (object = ask_for_invobj("wield what?", 1<<ocWeapon)) == NULL ) return;
  if (handle_unpaid(object)) return;
  if (old_weapon != NULL)
  { if (old_weapon == object) { message_show("already wielding this"); return;}
    old_weapon->flags |= ofEnchIdentified; creature_unwield(rogue);
  }
  creature_wield(rogue, object);
  sprintf(strbuf, strBracedCharSpace, invobj2char(object));
  object_get_description(object, strbuf + 4); message_show(strbuf);
  rogue_las(cakOther, 0);
}

static one_caller void unwield(void)
{ tObject* weapon = rogue->weapon;
  if (weapon == NULL) { message_show("not wielding any weapon"); return; }
  if ( (weapon->enchantment < 0) && (!object_is_unpaid(weapon)) )
  { cant_unwield(); return; }
  creature_unwield(rogue); weapon->flags |= ofEnchIdentified;
  sprintf(strbuf, "no longer wielding (%c) ", invobj2char(weapon));
  object_get_description(weapon, strbuf + 23);
  message_show(strbuf); rogue_las(cakOther, 0);
}

static tBoolean cape_in_the_way(void)
{ const tObject* const cape = roguedata_os(rdosiCape);
  const tBoolean retval = cond2boolean(cape != NULL);
  if (retval)
  { const tBoolean pl = object_get_minidesc(cape, strbuf, 1, NULL);
    sprintf(strbuf + strlen(strbuf), strPercsInTheWay, (pl ? strAre : strIs));
    message_show(strbuf);
  }
  return(retval);
}

static one_caller void wear(void)
{ tObject* object;
  if (rogue->armor) /* the cheaper check first */
  { message_show("already wearing some armor"); return; }
  if (!rogue_has_objectclasses(1 << ocArmor))
  { message_show("don't have any armor to wear"); return; }
  if (cape_in_the_way()) return;
  if ( (object = ask_for_invobj("wear what?", 1 << ocArmor)) == NULL ) return;
  if (handle_unpaid(object)) return;
  creature_wear(rogue, object);
  sprintf(strbuf, strBracedCharSpace, invobj2char(object));
  object_get_description(object, strbuf + 4); message_show(strbuf);
  rogue_las(cakOther, 0);
}

static one_caller void unwear(void)
{ tObject* armor = rogue->armor;
  if (armor == NULL) { message_show("not wearing any armor"); return; }
  if (!(armor->flags & ofEnchIdentified))
  { armor->flags |= ofEnchIdentified; roguedata.flags |= rfInvPropertyChanged;}
  if ( (armor->enchantment < 0) && (!object_is_unpaid(armor)) )
  { message_show("can't take off cursed armor"); return; }
  if (cape_in_the_way()) return;
  creature_unwear(rogue);
  sprintf(strbuf, "no longer wearing (%c) ", invobj2char(armor));
  object_get_description(armor, strbuf + 22); message_show(strbuf);
  rogue_las(cakOther, 0);
}

static one_caller void puton_ring(void)
{ tObject* object;
  tRogueDataOsi target; /* shortcut array index for ring */
  tObjectFlags oflags SHUT_UP_COMPILER(ofNone);
  if (!rogue_has_objectclasses(1 << ocRing))
  { message_show("don't have any ring to put on"); return; }
  if ( (object = roguedata_os(rdosiRing1)) != NULL ) /* wearing at least one */
  { if (roguedata_os(rdosiRing2) != NULL) /* wearing two rings */
    { message_show("already wearing two rings"); return; }
    oflags = ( (object->flags & ofOnLeftHand) ? ofOnRightHand : ofOnLeftHand );
    target = rdosiRing2;
  }
  else target = rdosiRing1;

  if ( (object = ask_for_invobj("put on what?", 1 << ocRing)) == NULL ) return;
  if (object->flags & ofOnEitherHand)
  { message_show("already on hand"); return; }
  if (handle_unpaid(object)) return;

  if (target == rdosiRing1) /* not yet wearing any ring */
  { const tKey key = ask_for_key_lr("put ring on which hand?");
    if (key == KEY_CANCEL) return;
    oflags = ( (key == 'l') ? ofOnLeftHand : ofOnRightHand );
  }

  object->flags |= oflags; roguedata_os(target) = object;
  sprintf(strbuf, strBracedCharSpace, invobj2char(object));
  object_get_description(object, strbuf + 4); message_show(strbuf);
  rogue_las(cakOther, 0);
}

static one_caller void remove_ring(void)
{ tRogueDataOsi rdosi; /* shortcut array index for ring */
  tObject* object;

  if (roguedata_os(rdosiRing1) == NULL) /* not wearing any rings */
  { message_show("not wearing any ring to remove"); return; }

  if (roguedata_os(rdosiRing2) != NULL) /* wearing two rings */
  { const tKey key = ask_for_key_lr("remove ring from which hand?");
    if (key == KEY_CANCEL) return;
    rdosi = ( (cond2bool(roguedata_os(rdosiRing1)->flags & ofOnLeftHand) ==
      cond2bool(key == 'l')) ? rdosiRing1 : rdosiRing2 );
  }
  else rdosi = rdosiRing1; /* wearing only one ring */

  /* (Try to) remove the ring */
  object = roguedata_os(rdosi); object->flags |= ofEnchIdentified;
  roguedata.flags |= rfInvPropertyChanged;
  if (object->enchantment < 0) { cant_remove_cursed(object); return; }
  object->flags &= ~ofOnEitherHand;

  /* Adapt the list of worn rings (minor trick - think about it:-) */
  if (rdosi == rdosiRing1) roguedata_os(rdosiRing1) = roguedata_os(rdosiRing2);
  roguedata_os(rdosiRing2) = NULL;

  /* Print a message */
  sprintf(strbuf, "removed (%c) ", invobj2char(object));
  object_get_description(object, strbuf + 12); message_show(strbuf);
  rogue_las(cakOther, 0);
}

static void cantdropthrowmsg(const tObject* object, tBoolean throw)
{ const tBoolean use_plural = object_get_minidesc(object, strbuf, 1, NULL);
  sprintf(strbuf + strlen(strbuf), " %s currently in use, can't %s %s",
    (use_plural ? strAre : strIs), (throw ? "throw" : "drop"),
    (use_plural ? "them" : strIt));
  message_show(strbuf);
}

static one_caller void drop(void)
/* lets the player drop an object from his inventory */
{ tObject* object;
  tObjectQuantity quantity;
  char temp[20];
  if (rogue->objects == NULL)
  { message_show("don't have any object to drop"); return; }
  quantity = entered_quantity(0);
  if (quantity == ENTERED_NUMBER_INVALID) temp[0] = '\0';
  else sprintf(temp, strNumberOf, quantity);
  sprintf(strbuf, "drop %swhat?", temp);
  if ( (object = ask_for_invobj(strbuf, ocbAll)) == NULL ) return;
  if (object->flags & (ofBeingWorn | ofBeingApplied | ofOnEitherHand))
  { cantdropthrowmsg(object, falsE); return; }
  if (object->flags & ofBeingWielded)
  { if (object->enchantment < 0) { cant_unwield(); return; }
    /* Always unwield the whole object, even if only a few of its "things" are
       dropped: */
    creature_unwield(rogue);
  }
  quantity = entered_quantity(1);
  if (quantity > object->quantity) /* can't drop more than what's there */
    quantity = object->quantity;
  creature_drop_object(rogue, object, quantity, falsE); rogue_las(cakDrop, 0);
}

static one_caller void throw(void)
/* lets the player throw an object from his inventory */
{ tObject* object;
  tObjectQuantity quantity;
  tDirection dir;
  char temp[20];
  if (rogue->objects == NULL)
  { message_show("don't have any item to throw"); return; }
  quantity = entered_quantity(0);
  if (quantity == ENTERED_NUMBER_INVALID) temp[0] = '\0';
  else sprintf(temp, strNumberOf, quantity);
  sprintf(strbuf, "throw %swhat?", temp);
  if ( (object = ask_for_invobj(strbuf, ocbAll)) == NULL ) return;
  if (handle_unpaid(object)) return;
  if (object->flags & (ofBeingWorn | ofBeingApplied | ofOnEitherHand))
  { cantdropthrowmsg(object, truE); return; }
  if ( (object->flags & ofBeingWielded) && (object->enchantment < 0) )
  { cant_unwield(); return; }

  /* At this point we know that the object _can_ be thrown, so let's ask for a
     direction */
  if (!ask_for_dir(&dir, NULL)) return;
  if (randpercent(10 * rogue->confused)) dir = randdir();
  creature_throw(rogue, object, entered_quantity(1), dir);
  rogue_las(cakThrow, 0);
}

static void do_pick_object(tObject** object, const tObjectQuantity quantity,
  char** temp)
{ tInventoryIndex idx;
  if (quantity >= (*object)->quantity) section_forget_object(*object);
  else
  { /* Don't pick _all_ things of that object: */
    *object = object_split(*object, quantity);
  }
  idx = creature_add_object(rogue, *object);
  if (idx == iiInvalid) /* "should not happen" */
  { message_show("BUG: inventory handling, iiInvalid"); return; }
  else if (idx < 0) /* has been put together */
  { idx = -1 - idx;
    *temp += sprintf(*temp, strNowBracedCharSpace, invidx2char(idx));
  }
  else /* new inventory entry */
  { *temp += sprintf(*temp, strBracedCharSpace, invidx2char(idx));
  }
  *object = roguedata.inventory[idx];
}

static one_caller void pick_object(void)
{ tObject* object;
  tObjectQuantity q;
  tCoordinate x = rogue->x, y = rogue->y;
  char* temp;
  tBoolean can_carry;
  if (rogue->levitated)
  { message_show("can't pick anything while floating in the air"); return; }
  if (!(squareflags_get(x, y) & sfObject))
  { nothing: message_show("nothing here to pick"); return; }
  object = location2object(x, y);
  if (object == NULL) goto nothing; /* "should not happen" */
  q = entered_quantity(0);
  if (q < object->quantity) /* the player doesn't want to pick all "things" */
  { tObject otest = *object;
    otest.quantity = q; can_carry = creature_can_carry_object(rogue, &otest,0);
  }
  else can_carry = creature_can_carry_object(rogue, object, 0);
  if (!can_carry) { message_show(strPackFull); return; }

  /* Actually pick the object: */
  temp = strbuf; do_pick_object(&object, entered_quantity(1), &temp);
  object_get_description(object, temp); message_show(strbuf);
  rogue_las(cakPick, 0);
}

typedef struct
{ tObstacle* door;
  tObject* tool;
  tDirection dir;
} tDoorAndTool;

static tDoorAndTool doorandtool[4];

static /*@null@*/ tObject* matching_key(const tObstacleDetail detail)
{ tObject* object;
  for_each_object(rogue->objects, object)
  { tObjectDetail objd;
    tBoolean key_metal;
    if (object->oc != ocTool) continue;
    switch (object->kind)
    { case tokPhotograph:
        objd = object->objdetail;
        if ( (photo_kind(objd) == phkObject) &&
             (is_photobjcode(objd, ocTool, tokKey)) )
        { goto out; } /* a photograph of a key can unlock any locked door */
        break;
      case tokKey:
        objd = object->objdetail;
        if (door_color(detail) != key_color(objd))
          continue; /* different colors; can't be a matching key */
        key_metal = cond2boolean(key_is_metal(objd));
        switch (_door_material(detail))
        { case dmWood:
            if (!key_metal) goto out; /* matching */
            break;
          case dmMetal:
            if (key_metal) goto out; /* matching */
            break;
          /* "default": no keys match glass/stone doors */
        }
        break;
    }
  }
  out: return(object);
}

static unsigned char find_doors(const tBoolean openable)
{ unsigned char retval = 0; /* just a counter */
  const tCoordinate x = rogue->x, y = rogue->y;
  tDirection dir;
  for_each_horvertdir(dir)
  { const tCoordinate x2 = x + xdir[dir], y2 = y + ydir[dir];
    tDoorAndTool* dt;
    tSquareFlags sflags;
    tObstacle* obstacle;
    tObstacleDetail detail;
    tObject* tool = NULL;
    tDoorState ds;
    if (!is_location_valid(x2, y2)) goto next_dir;
    sflags = squareflags_get(x2, y2);
    if ( (!(sflags & sfObstacle)) || (sflags & sfHidden) ) goto next_dir;
    if ( (obstacle = location2obstacle(x2, y2)) == NULL ) goto next_dir;
    if (obstacle->kind != obkDoor) goto next_dir;
    if (!obstacle_showable(obstacle)) goto next_dir;
    detail = obstacle->obsdetail; ds = door_state(detail);
    if (openable)
    { switch (ds)
      { case dsOpen: goto next_dir; /*@notreached@*/ break; /* already open */
        case dsLocked: /* locked; look for a matching key */
          if ( (tool = matching_key(detail)) != NULL ) goto do_store;
          else goto next_dir;
          /*@notreached@*/ break;
        default: goto do_store; /*@notreached@*/ break; /* merely closed */
      }
    }
    else
    { if (!door_passable(ds)) goto next_dir; /* already closed/locked */
      if (sflags & sfCreature) goto next_dir; /* creature "in the way" */
    }
    do_store: dt = &(doorandtool[retval]); dt->door = obstacle;
    dt->tool = tool; dt->dir = dir; retval++; next_dir: {}
  }
  return(retval);
}

static void door_stuck(void)
{ message_show("the door seems to be stuck"); }

static tBoolean door_vs_holding(void)
{ tBoolean retval = falsE; /* assume there's no problem */
  if (rogue->held)
  { switch (roguedata.holding_cause)
    { case hcPit:
        message_show("can't reach doors while in a pit"); held: retval = truE;
        goto out; /*@notreached@*/ break;
      case hcCobweb:
        message_show("can't reach doors while stuck in cobweb"); goto held;
        /*@notreached@*/ break;
    }
  }
  out: return(retval);
}

#define HANDLE_DOOR_VS_HOLDING if ( (num > 0) && (door_vs_holding()) ) return;

static one_caller void open_door(void)
{ unsigned char num = find_doors(truE), idx;
  tObstacle* door;
  tObstacleDetail detail;
  tDoorState old_state, new_state;
  tObject* tool;
  tDirection dir;
  const tDoorAndTool* dt;
  tBoolean trouble;
  HANDLE_DOOR_VS_HOLDING
  switch (num)
  { case 0: none: message_show("no openable door"); break;
    case 1:
      dt = &(doorandtool[0]); handle_door: door = dt->door; tool = dt->tool;
      /* IMPLEMENTME: unlocking-and-opening should be an interruptible two-step
         action, at least in non-easy gameplay difficulties! */
      if (tool != NULL) /* locked door was unlocked with tool */
      { if (tool->kind == tokKey)
        { creature_lose_object(rogue, tool); object_deallocate(tool); }
        /* photograph can be used more than once... */
      }
      rogue_las(cakOther, 0); detail = door->obsdetail;
      old_state = door_state(detail);
      if (door_unopenable(detail)) { door_stuck(); trouble = truE; }
      else if ( (old_state == dsClosed) && (__randpercent(7 + 2 * difficulty +
        door->resistance / 3 + current_storeynum / 4)) )
      { /* player oughtta try again */
        message_show("the door resists"); trouble = truE;
      }
      else trouble = falsE;
      if (trouble)
      { if (old_state == dsClosed) break; /* no change */
        new_state = dsClosed; /* unlock */
      }
      else new_state = dsOpen;
      door_state_set(door, new_state, rogue); break;
    default: /* several doors could be meant */
      if (!ask_for_dir(&dir, NULL)) return;
      for (idx = 0; idx < num; idx++)
      { dt = &(doorandtool[idx]);
        if (dt->dir == dir) goto handle_door;
      }
      goto none; /*@notreached@*/ break; /* none in the given direction */
  }
}

static one_caller void close_door(void)
{ unsigned char num = find_doors(falsE), idx;
  tObstacle* door;
  tDirection dir;
  const tDoorAndTool* dt;
  HANDLE_DOOR_VS_HOLDING
  switch (num)
  { case 0: none: message_show("no closable door"); break;
    case 1:
      dt = &(doorandtool[0]); handle_door: rogue_las(cakOther, 0);
      door = dt->door;
      if (door_unclosable(door->obsdetail)) { door_stuck(); break; }
      door_state_set(door, dsClosed, rogue); break;
    default: /* several doors could be meant */
      if (!ask_for_dir(&dir, NULL)) return;
      for (idx = 0; idx < num; idx++)
      { dt = &(doorandtool[idx]);
        if (dt->dir == dir) goto handle_door;
      }
      goto none; /*@notreached@*/ break; /* none in the given direction */
  }
}

static one_caller void kick(void)
{ tDirection dir;
  tCoordinate x2, y2;
  tSquareFlags sflags;
  tCreature* creature;
  tObstacle* obstacle;
  tObject* boots;
  tDamage damage;
  if (creature_is_in_the_air(rogue))
  { message_show("can't kick while in the air"); return; }
  if (rogue->held)
  { if (roguedata.holding_cause == hcPit)
    { message_show("kicking the walls of the pit");
      if ( (difficulty > dkNormal) && (__randpercent(1 << difficulty)) )
      { sprintf(strbuf, "the pit collapses around %s", rogue_pronoun(1));
        message_show(strbuf); rogue_died(drInjury, NULL);
      }
      rogue_las(cakKick, 0); goto ouch;
    }
    message_show("can't kick while being held"); return;
  }
  if (creature_is_ill(rogue))
  { sprintf(strbuf, "%s %s too ill to kick", rogue_pronoun(0), rogue_verb());
    message_show(strbuf); return;
  }
  if (!ask_for_dir(&dir, NULL)) return;
  if (randpercent(10 * rogue->confused)) dir = randdir();
  rogue_las(cakKick, 0); x2 = rogue->x + xdir[dir]; y2 = rogue->y + ydir[dir];
  if (!is_location_valid(x2, y2)) /* kicking solid rock */
  { tBoolean does_hurt;
    ouch: does_hurt = __randpercent(3 * (1 << difficulty));
    sprintf(strbuf, "ouch, %s %sstrained a muscle", rogue_pronoun(0),
      (does_hurt ? strEmpty : "almost "));
    message_show(strbuf);
    if ( (does_hurt) && (!creature_dec_health(rogue, 1)) )
      rogue_died(drInjury, NULL);
    goto finish;
  }
  sflags = squareflags_get(x2, y2);
  if ( (sflags == 0) || ( (sflags & sfWall) && (!(sflags & sfDoorway)) ) )
    goto ouch; /* kicking solid rock */
  if ( (sflags & (sfDoorway | sfHidden)) == (sfDoorway | sfHidden) )
  { section_unhide_square(x2, y2);
    if (map_can_player_see(x2, y2, mfOther)) message_show(strWhoops);
    goto finish;
  }
  damage = rogue->strength / (difficulty + 1);
  if (randpercent(50 - 10 * difficulty + 2 * rogue->experience)) damage++;
  if ( ( (boots = roguedata_os(rdosiBoots)) != NULL ) )
  { switch (boots->kind)
    { case tokBootsKick: damage += 3 * zero2one(boots->enchantment); break;
      case tokBootsQuiet: /* very "soft" boots */
        if (damage > 2) damage = 2;
        goto dmgfin; /*@notreached@*/ break;
    }
  }
  if (damage > 10) damage = 10;
  else if (damage < 3) damage = 3;
  dmgfin: if (is_diagdir(dir)) damage >>= 1;
  damage = randdamage(damage, 1 | 4);

  if ( (sflags & sfCreature) && ( (creature = location2creature(x2, y2)) !=
    NULL ) )
  { const tBoolean csc = map_can_player_see(creature->x, creature->y,
      mfCreature); /* (must be done here due to possible flitting) */
    tBoolean survived;
    if (creature->flags & cfImmaterial) goto do_miss;
    if ( (creature_is_flitting(creature, truE)) &&
       (creature_randflit(rogue, creature)) &&
       (creature_try_to_flit(creature)) )
    { char* s;
      do_miss: s = strbuf; s += sprintf(s, "%s miss", rogue_pronoun(0));
      if (csc) { *s++ = ' '; creature_get_name(creature, s, 1); }
      message_show(strbuf); goto ouch;
    }
    if (__randpercent(difficulty / 2 + 1)) goto do_miss; /* IMPLEMENTME: better
      probability, including creature size and mobility and player character
      experience and csc and boots->enchantment (if boots != NULL)! */
    creature_make_enemies(rogue, creature);
    if (creature->napping) creature_wake_up(creature, csc);
    if (creature_is_shopkeeper(creature)) infuriate_shopkeeper(creature,falsE);
    survived = creature_dec_health(creature, damage);
    sprintf(strbuf, "%s %s ", rogue_pronoun(0), ( ( (survived) || (!csc) ) ?
      "kick" : "defeated" ));
    if (csc) creature_get_name(creature, strbuf + strlen(strbuf), 1);
    else strcat(strbuf, strSomething);
    message_show(strbuf);
    if (!survived) (void) creature_died(creature, drInjury, rogue);
    if (boots != NULL)
    { boots->lifetime -= aaHitting; creature_check_object(rogue, boots); }
    goto finish;
  }
  if ( (sflags & sfObstacle) && ( (obstacle = location2obstacle(x2, y2)) !=
    NULL ) && (obstacle_effsize(obstacle) > 0) )
  { message_show("WHAMMM!!!"); /* IMPLEMENTME: noise_caused_by(); and the sound
      should depend on the material! */
    if (obstacle->resistance >= 14) goto ouch; /* can't destroy by kicking */
    if ( (!obstacle_resistance_decrease(obstacle, damage, 1)) &&
      (obstacle->kind == obkChest) && (__randpercent(10)) && (rogue_can_hear) )
    { sprintf(strbuf, ( (rogue->hallucinating) ? "%s hear a shuffled matter" :
        "%s hear a muffled shatter" ), rogue_pronoun(0) );
      message_show(strbuf);
      /* IMPLEMENTME: actually destroy some contained (glass) objects! */
    }
    /* IMPLEMENTME: if obfPushable, kicking moves the obstacle slightly further
       than simply walking into it would! */
    goto finish;
  }
  goto ouch;
  /* IMPLEMENTME: kicking a ground feature, esp. grfkThrone! */
  /* IMPLEMENTME: if kicking a tokBox ("THUD!"), any potion inside it shatters
     and may have some effect, depending on potion kind! For example, when the
     player puts a certain potion and 42 gold pieces into a box and kicks it,
     ... Kicking a "soft" object (food?) gives a "*splat*" sound and destroys
     the object. Kicking a statue should push it around on the ground. */
  finish: roguedata.flags |= rfHits | rfTickHits; roguedata.digestcount++;
}

static one_caller void wish(void)
{ tObject* object;
  roguedata.cbf |= cbfDidWish; rogue_las(cakOther, 0);
  if (rogue_in_shop) { infuriate_shopkeeper(NULL, falsE); return; }
  /* IMPLEMENTME: ask what the player wishes, e.g. teleportation, levitation,
     food, other kinds of objects, health restoration, enlightenment (a more
     specific help than what is available from oracles), earthquake, permatrap;
     implement an appropriate "price" for each wish! Available choices should
     depend on alignment. The list should be called "Wish List", of course. :-)
  */
  /* IMPLEMENTME: in ddkMain storey 1, wishing for a blindfold allows the
     player to start a "Zen conduct" and has no negative consequences. */
  creature_teleport(rogue); object = rogue->objects;
  while (object != NULL)
  { tObject* next = object->next;
    /* Yep, this may even destroy the AoY! */
    if (__randpercent(15 * difficulty + 10))
    { object->lifetime = 0; creature_check_object(rogue, object); }
    object = next;
  }
}

static void ba_cbf(tScrollableText* st, const char* prefix, const tCbf cbf)
{ sprintf(strbuf, "%s: %s", prefix, ((roguedata.cbf & cbf) ? strYes : strNo));
  st_append(st, my_strdup(strbuf), truE);
}

#define BA_CBF(prefix, flag) ba_cbf(&st, prefix, flag)

static one_caller void base_attributes(void)
{ static const char* const fc[4] = { "none", "vegan", "vegetarian","omnivore"};
  tScrollableText st;
  const tRogueDataStats* const rds = &(roguedata.rds);
  const tCbf cbf = roguedata.cbf;
  st_setup(&st); st_append(&st, "Base Attributes", falsE); st_append_nl(&st);
  sprintf(strbuf, "Role: %s (%s)", role_name(falsE),
    _role_name(startpref_role, startpref_gender, falsE));
  st_append(&st, my_strdup(strbuf), truE);
  /* st_append(&st, "species, gender, alignment (initial and current)", falsE);
    -- IMPLEMENTME! */
  st_append_nl(&st);
  sprintf(strbuf, "--pref=d%dr%cc%dm%df%de%d", difficulty,
    _roledata[startpref_role].prefsym, pref_combat, pref_magic, pref_feat,
    pref_expl);
  st_append(&st, my_strdup(strbuf), truE);
  st_append_nl(&st); st_append_reverse(&st, "Conduct", falsE);
  sprintf(strbuf, "Food: %s", fc[cbf & cbfFoodmask]);
  st_append(&st, my_strdup(strbuf), truE);
  BA_CBF("Did read", cbfDidRead); BA_CBF("Did write", cbfDidWrite);
  BA_CBF("Did wish", cbfDidWish); BA_CBF("Did polymorph", cbfDidPolyPc);
  BA_CBF("Did polymorph objects", cbfDidPolyObj);
  BA_CBF("Tried to identify", cbfDidTryIdentify);
  BA_CBF("Did trade", cbfDidTrade); BA_CBF("Had pet", cbfHadPet);
  st_append_nl(&st); st_append_reverse(&st, "Statistics", falsE);
  sprintf(strbuf, "Number of actions: %d (%d seeing; %d with inventory)",
    rds->num_actions, rds->num_seeing, rds->inventorized);
  st_append(&st, my_strdup(strbuf), truE);
  sprintf(strbuf, "Defeated creatures: %d", rds->alldefeats);
  st_append(&st, my_strdup(strbuf), truE);
  sprintf(strbuf, "Weaponhits: %d", rds->num_weaponhits);
  st_append(&st, my_strdup(strbuf), truE);
  sprintf(strbuf, "Reached storey: %d", rds->sn_deepest);
  st_append(&st, my_strdup(strbuf), truE);
  /* IMPLEMENTME: an "Achievements" section which e.g. lists which quests the
     player solved; move the "reached storey" statistics into this section.
     A "Skills and Aptitudes" section. */
  st_show(&st); st_cleanup(&st);
}

#include "apply.c" /* KISS! */

static one_caller void call_object(void)
/* allows the player to assign texts to kinds of magic objects; so he can write
   up a guess what an object of an unknown kind could be. */
{ tObject* object;
  char ch, *s, *s2, *ptr;
  if (!rogue_has_objectclasses(ocbMagic))
  { message_show("don't have any magic item to call"); return; }
  if ( (object = ask_for_invobj("call what?", ocbMagic)) == NULL ) return;
  if ( (s = ask_for_string("call it how?")) == NULL ) return; /* cancelled */
  s2 = ptr = __memory_allocate(strlen(s) + 3); /* (3: 2x '"' + 1x '\0') */
  *ptr++ = '"';
  while ( (ch = *s++) != '\0' ) *ptr++ = ch;
  *ptr++ = '"'; *ptr = '\0';
  object_call(object->oc, object->kind, s2);
  sprintf(strbuf, strBracedCharSpace, invobj2char(object));
  object_get_description(object, strbuf + 4); message_show(strbuf);
}

static one_caller void show_help_screen(void)
{ static const char* const line[] =
  { "Keyboard Commands", NULL, "Single-step movement:", NULL,
    "       k, 8",
    "        |",
    "  y, 7  |  u, 9",
    "      \\ | /",
    "       \\|/",
    "h, 4 ---+--- l, 6",
    "       /|\\",
    "      / | \\",
    "  b, 1  |  n, 3",
    "        |",
    "       j, 2",
    "also <Cursor> keys", NULL, "Until stopped: <Shift>-letter",
    "To sth. inter.: <Ctrl>-letter", NULL, "a - apply; un-apply",
    "c - close door", "C - give count/number", "d - drop", "e - eat",
    "E - explain/help", "g - glimpse", "i - complete inventory",
    "I - special inventory", "m - move onto", "o - open door", "p - pick up",
    "P - put ring on hand", "q - quaff potion", "Q - quit atrogue",
    "r - read scroll", "R - remove ring", "s - search", "S - sleep",
    "t - throw", "T - take off armor", "v - short version info",
    "V - long version info", "w - wield weapon", "W - wear armor",
    "x - unwield weapon", "X - extended command", "z - zap with wand",
    ". - rest", "> - climb down one storey", "< - climb up one storey",
    "? - show this help", "Ctrl-d - kick", "Ctrl-p - previous message",
    "Ctrl-w - wish", "Ctrl-x - base attributes", "*",
    "<Escape>, Ctrl-e - cancel"
  }; /* IMPLEMENTME: list the "extended" commands; list the "verbose" cmds! */
  tScrollableText st;
  unsigned char idx;
  st_setup(&st); st.txtwidth = 30;
  for (idx = 0; idx < ARRAY_ELEMNUM(line); idx++)
  { const char* const str = line[idx];
    if ( (str != NULL) && (*str == '*') ) /* special, "dynamic" text */
    { const char* src = strObjectClassChars;
      char ch, *ptr = strbuf;
      while ( (ch = *src++) != '\0' ) { if (ch != '?') *ptr++ = ch; }
      strcpy(ptr, " - partial inventory");
      st_append(&st, my_strdup(strbuf), truE);
    }
    else st_append(&st, str, falsE);
  }
  st_show(&st); st_cleanup(&st);
}

static one_caller void does_not_return congrats_quit(const char* txt)
{ const char *prefix, *r = role_name(falsE);
#if CONFIG_DEMO
  if (runmode == rmDemo) prefix = strI;
  else
#endif
  { prefix = "Congratulations - you"; }
  sprintf(strbuf, "%s escaped from the dungeon as %s %s with the Amulet of "
    "Yendor %s!", prefix, indefinite_article(r[0]), r, txt);
  do_quit_msg(ecOkay, strbuf);
  /* IMPLEMENTME: use a nice "end of game" screen (which allows the player to
     press some meaningful keys, e.g. to call object_dump_associations() or to
     save the dungeon) instead of quitting immediately. Print score. Print
     statistics (roguedata.rds), dungeon clock ticks, reached storey (deepest
     storey if won, current/deepest storey if died, how often died;
     <roguedata.rds.sn_deepest>), roguedata.deathcount. Mention which special
     quests/puzzles were solved. Mention the conduct/behavior (cf. the function
     base_attributes()). Mention which department kinds were traversed or at
     least reached/entered. */
}

static one_caller void try_to_leave_the_dungeon(void)
{ const tObject* const object = _rogue_aoy_find();
  const char *prefix = strThe, *txt;
  tObjectDetail detail;
  tObjectQuantity goldcount;
  char txtbuf[100]; /* 100 "should be enough" */
  if (object == NULL) goto failed; /* doesn't even have a "candidate" amulet */
  detail = object->objdetail;
  if (amulet_misscount(detail) > 0)
  { message_show("the Amulet isn't complete"); prefix = "a complete";
    goto failed;
  }
  if (object->enchantment <= 0)
  { message_show("the Amulet has lost its enchantment");
    /* message_show("it's just a worthless piece of metal now..."); */
    message_show("what have you done with it?!");
    prefix = "an enchanted"; goto failed;
  }
  if (AMULET_FAKE(detail))
  { message_show("the Amulet is a fake"); prefix = "a genuine"; goto failed; }
  /* The player has a "good" Amulet, so can leave the dungeon: */
  goldcount = gold_pieces();
  if (goldcount <= 0) txt = "(but without any gold)";
  else
  { sprintf(txtbuf, "and %d piece%s of gold", goldcount, ( (goldcount == 1) ?
      (strEmpty) : (strS) ));
    txt = txtbuf;
  }
  congrats_quit(txt);
  failed: sprintf(strbuf, "need %s Amulet of Yendor", prefix);
  message_show("can't leave the dungeon"); message_show(strbuf);
}

static tMoveKind movekind = mkSingleStep;
static tFewTicks movecountdown = 0;
static tBoolean only_moving_onto = falsE;

static tBoolean next_to_something(void)
/* checks whether the player character is next to something of interest */
{ tBoolean retval;
  const tCoordinate x = rogue->x, y = rogue->y;
  tDirection dir;
  for_each_dir(dir)
  { const tCoordinate x2 = x + xdir[dir], y2 = y + ydir[dir];
    tSquareFlags sflags;
    if (!is_location_valid(x2, y2)) continue;
    sflags = squareflags_get(x2, y2);
    if ( (sflags & sfObject) && (map_can_player_see(x2, y2, mfObject)) )
    { found: retval = truE; goto out; }
    else if ((sflags & sfCreature) && (map_can_player_see(x2, y2, mfCreature)))
      goto found;
    else if ( (sflags & sfDoorway) && (!(sflags & sfHidden)) &&
      (map_can_player_see(x2, y2, mfOther)) )
    { goto found; }
    else if (sflags & sfSeconn)
    { const unsigned char downup = seconn_downup_at(x2, y2);
      if ( (downup & 1) && ( (sflags & sfKnownSeconn) ||
        (map_can_player_see(x2, y2, mfStairsDown)) ) )
      { goto found; }
      else if ( (downup & 2) && ( (sflags & sfKnownSeconn) ||
        (map_can_player_see(x2, y2, mfStairsUp)) ) )
      { goto found; }
    }
  }
  retval = falsE;
  out: return(retval);
  /* IMPLEMENTME: when the player character is moving around, we should return
     truE only if he's currently next to something which he wasn't next to
     previously! */
}

static one_caller void heal_rogue(void)
/* "heals" the player character by increasing the health after a certain number
   of dungeon ticks ("interval") has elapsed; the interval length depends on
   experience - the higher the experience, the faster the healing */
{ static tExperience exp = -1, counter_endvalue = 0, counter = 0;
  tBoolean is_weak = cond2boolean(roguedata.state >= rsWeak); /*weak or worse*/
  if (rogue->health >= roguedata.maxhealth)
  { /* already at maximum health, so can't heal any further */
    dont_heal: counter = 0;
    dont_heal_noreset: return;
  }
  if ( (is_weak) && (difficulty > dkNormal) )
    goto dont_heal; /* weak and high difficulty - don't heal at all */
  if (roguedata.flags & rfTickNoheal)
  { /* currently fighting or frightened - "suspend" healing for this tick */
    roguedata.flags &= ~rfTickNoheal;
    goto dont_heal_noreset;
  }

  if (exp != rogue->experience)
  { /* The experience has changed since we last checked, so adapt the interval
       length. */
    exp = rogue->experience;
    if (exp >= 36) counter_endvalue = 2;
    else counter_endvalue = 20 - exp / 2;
  }

  counter++;
  if ( (rogue->napping) && ( (dungeonclock % 3) == 0 ) )
  { counter++; } /* (heal slightly faster when sleeping) */

  if (counter >= counter_endvalue) /* interval elapsed - actually heal */
  { static tBoolean flipflop = truE, flipflop2 = truE;
    const tObject* ring;
    unsigned char ringcount;
    tPropertyExtent ench;

    counter = 0;

    /* normal healing */

    rogue->health++;
    if (is_weak) goto finish;
      /* (shall not be able to heal more than +1 per interval if weak) */
    flip_boolean(flipflop);
    if (!flipflop) goto finish; /* no extras this time */

    /* now the extras for every second interval */

    rogue->health += roledata.healing / 2;

    /* "ring healing"; we check "ench > 0" below because the player character
       would die quite fast if we allowed decreasing health here - it's
       probably bad enough that the player can't remove a cursed ring... */
    worn_rings_enchsumcount(ench, ring, rkHeal, ringcount);
    if (ench > 0) rogue->health += ench;
    else if ( (ringcount > 0) && (ench == 0) )
      rogue->health++; /* at least _some_ positive effect */

    /* magic energy - some kind of charge exchange between player character and
       dungeon, every fourth interval */
    flip_boolean(flipflop2);
    if ( (flipflop2) && (roguedata.magic_energy <
         MIN(roguedata.magic_exp, current_storeynum)) )
    { rogue_incmagicen( (pref_role == roleWizard) ? 2 : 1 ); }

    finish:
    if (rogue->health > roguedata.maxhealth) rogue->health=roguedata.maxhealth;
    statistics_invalidate(svfHealth);
  }
}

static one_caller void handle_digestion(void)
/* once per dungeon clock tick */
{ const tObject *ring, *weapon;
  tRogueDataOsi rdosi;
  signed char dc, count;
  unsigned char ringcount, dec;
  count = 0; ringcount = dec = 0;
  if ( (rogue->napping) || (rogue->frozen >= 3) )
  { count--; } /* slower digestion when napping or deeply frozen */
  if (rogue->slowedhasted < 0) count -= 2; /* slower when slowed... :-) */
  for_each_worn_ring(ring)
  { if (ring->kind == rkSlowDigestion) count -= zero2one(ring->enchantment);
    else ringcount++; /* any other worn rings cause a little extra digestion */
  }
  if (ringcount > 0) count++;
  for (rdosi = rdosiBoots; rdosi < rdosiRing1; rdosi++)
  { /* each applied tool causes faster digestion */
    if (roguedata_os(rdosi) != NULL) roguedata.digestcount++;
  }
  if ( ((weapon = rogue->weapon) != NULL) && (weapon->enchantment <= 0) &&
       (object_weight(weapon) >= OBJECT_WEIGHT_ROGUEDIGEST) &&
       (!(rogue->napping | rogue->frozen)) )
  { roguedata.digestcount++; }
  if (count > 1) /* the Amulet might reduce digestion somewhat */
  { const tObject* object;
    for_each_object(rogue->objects, object)
    { if (object->oc == ocAmulet)
      { if (object->enchantment > 30) count = 1;
        break;
      }
    }
  }

  dc = roguedata.digestcount + count * zero2one(difficulty / 2);
  if (dc < 0) dc = 0; /* no digestion during this tick */
  else
  { dec = dc / ACTIONDIGEST;
    if (dec > 0) { rogue_decactionsleft(dec); dc -= dec * ACTIONDIGEST; }
  }
  roguedata.digestcount = dc; /* "remainder" for future ticks */
}

static void boots_glow(void)
{ sprintf(strbuf, "%s boots glow %s for a moment", rogue_pronoun(2),
    enchantmentcolorstr);
  message_show(strbuf);
}

static one_caller void perform_rogue_tick(void)
{ const tObject* object;
  tBoolean just_fainted = falsE, spth;

  /* Check for special applied tools */

#define BG boots_glow();
  if ( (object = roguedata_os(rdosiBoots)) != NULL )
  { const tObjectKind kind = object->kind;
    if (kind == tokBootsHaste)
    { const tPropertyExtent ench = object->enchantment;
      if (ench >= 0)
      { if ( (rogue->slowedhasted <= 0) && (__randpercent(95-5*difficulty)) )
        { BG creature_haste(rogue, 10 - difficulty + ench + randbytemod(5)); }
      }
      else
      { if ( (rogue->slowedhasted >= 0) && (__randpercent(3*difficulty + 1)) )
        { BG creature_slow(rogue, 10 - ench + randbytemod(5)); }
      }
    }
    else if (kind == tokBootsLevit)
    { const tPropertyExtent ench = object->enchantment;
      if (ench >= 0)
      { if ( (!rogue->levitated) && (__randpercent(95 - 5 * difficulty)) )
        { BG creature_levitate(rogue, 10 - difficulty + ench+randbytemod(5)); }
      }
      else
      { if ( (rogue->levitated) && (__randpercent(3 * difficulty - ench + 1)) )
        { BG (void) creature_unlevitate(rogue); }
      }
    }
  }
#undef BG
  if ( ( (object = roguedata_os(rdosiCape) ) != NULL ) &&
    (object->kind == tokCapeInvis) )
  { const tPropertyExtent ench = object->enchantment;
    if (ench >= 0)
    { if ( (!(rogue->flags & cfInvisible)) &&
        (randpercent(20 + 10 * zero2one(ench))) )
      { creature_make_invisible(rogue); }
    }
    else if (rogue->flags & cfInvisible) creature_make_visible(rogue);
  }
  if ( ( (object = roguedata_os(rdosiGlasses)) != NULL ) &&
    (object->kind == tokGlassesDark) )
  { const tPropertyExtent ench = object->enchantment;
    if ((ench < 1) && (!rogue->_blind) && (randpercent(difficulty - ench)))
      (void) rogue_dazzle(truE); /* not "explicitly" enchanted... */
  }
  if ( (object = roguedata_os(rdosiShawl)) != NULL )
  { const tObjectKind kind = object->kind;
    if (kind == tokShawlGas)
    { const tPropertyExtent ench = object->enchantment;
      if ( (!rogue->napping) && (ench < 0) && (__randpercent(-ench)) )
      { message_show(strIsogh); creature_nap(rogue, 2 + randbytemod(2), 0); }
    }
    else if (kind == tokShawlFood)
    { if (roguedata.flags & rfSenseObjects) goto no_food; /* not necessary */
      if ( (object->enchantment >= 0) && (__randpercent(6 - difficulty)) )
      { const tObject *o, *food = NULL;
        tCoordinate x = rogue->x, y = rogue->y,
          minfooddist = HIGHEST_ALLOWED_NUMBER;
        for_each_currentsectionobject(o)
        { if (o->oc == ocFood)
          { tCoordinate dista = ABS(o->x - x), distb = ABS(o->y-y),
              dist = dista * dista + distb * distb;
            if (dist == 0) /* player character stands on food location */
              goto no_food;
            if (minfooddist > dist) { minfooddist = dist; food = o; }
          }
        }
        if (food == NULL) goto no_food; /* no food in section */
#if CONFIG_DEBUG
        sprintf(debugstrbuf, "SHAWL-FOOD: %d,%d", food->x, food->y);
        message_showdebug(debugstrbuf);
#endif
        minfooddist /= 10; /* make the value less exact */
        if (minfooddist < 1) goto no_food; /* very close */
        if (map_can_player_see(food->x, food->y, mfObject)) goto no_food;
        sprintf(strbuf, "smelling food in direction %s, distance %d",
          strDirection[diroffsets2dir(SIGN(food->x - x), SIGN(food->y - y))],
          minfooddist);
        message_show(strbuf);
        /* CHECKME: a numerical value is too unromantic... */
      }
      no_food: {}
    }
  }

  /* Handle all those counters */

  if (squareflags_get(rogue->x, rogue->y) & sfFire)
  { tHealth sub = difficulty;
    if (sub < 1) sub = 1;
    else if (sub > 3) sub = 3;
    message_show(strQuiteHot);
    if (!creature_dec_health(rogue, sub)) rogue_died(drFire, NULL);
    spth = truE;
    /* IMPLEMENTME: burn inventory (at least armor/helmet/gloves/shawl/boots)!
    */
  }
  else spth = falsE;
  if (rogue->_blind)
  { if (rogue->_blind == 1) creature_unblind(rogue);
    else rogue->_blind--;
  }
  if (rogue->confused)
  { if (rogue->confused == 1) creature_unconfuse(rogue);
    else rogue->confused--;
  }
  if (rogue->hallucinating)
  { if (rogue->hallucinating == 1) creature_unhallucinate(rogue);
    else rogue->hallucinating--;
  }
  if (rogue->slowedhasted) creature_decslowedhasted(rogue);
  if (rogue->levitated) /* must be checked _after_ ->blind! */
  { if (rogue->levitated == 1) (void) creature_unlevitate(rogue);
    else rogue->levitated--;
  }
  if (rogue->held)
  { const tHoldingCause hc = roguedata.holding_cause;
    if ( (hc == hcPit) && (!roguedata.attemptedmovecount) )
    { rogue->held++; /* sinking deeper into the pit or similar... */
      if (rogue->held >= 10)
      { const tCoordinate x = rogue->x, y = rogue->y;
        squareflags_or(x, y, sfWater); map_update(x, y, mfGround);
        rogue_died(drDrown, NULL);
      }
      else if ( (rogue->held >= 8) && (difficulty <= dkNormal) )
        message_show("the pit fills with water");
    }
    else
    { tFewTicks sub = 1;
      if ( ( (hc == hcBearTrap) || (hc == hcCobweb) ) &&
           (roguedata.attemptedmovecount > 0) &&
           (randpercent(50 - 10 * difficulty + rogue->strength)) )
      { sub++; }
      if (rogue->held <= sub) creature_cancel_being_held(rogue);
      else rogue->held -= sub;
    }
  }
  roguedata.attemptedmovecount = 0;
  if ( (rogue_chained) && (((tTicks)dungeonclock) >= roguedata.iron_ball_off) )
    break_iron_ball();
  if (rogue->frozen)
  { const tFewTicks thaw = (spth ? 2 : 1);
    if (!(--roguedata.endure_freeze)) rogue_died(drHypothermia, NULL);
    if (rogue->frozen <= thaw) creature_unfreeze(rogue);
    else rogue->frozen -= thaw; /* still frozen */
  }
  if (roguedata.petrifying > 0)
  { if (++roguedata.petrifying > 10) rogue_died(drPetrifying, NULL);
  }
  if (rogue->napping) /* must be checked last (due to rsFaint handling) */
  { check_napping:
    if (rogue->napping == 1) creature_wake_up(rogue, falsE);
    else rogue->napping--; /* still sleeping */
  }
  if ( (!just_fainted) && (roguedata.state == rsFaint) && (!(rogue->napping |
    rogue->frozen)) && (__randpercent(4 * MAX(difficulty, 1))) )
  { creature_nap(rogue, randbytefromrange(3, 4 + difficulty) + difficulty, 1);
    if (rogue->napping) /* didn't resist napping */
    { just_fainted = truE; restcounter = 0;
      if (movekind != mkGliding) movekind = mkSingleStep;
      goto check_napping;
    }
  }
  handle_digestion(); heal_rogue();
}

static one_caller void perform_tick(void)
/* performs a dungeon tick (ages objects etc.) */
{ tCreatureCount count;

  if (dungeonclock >= 99999)
  { sprintf(strbuf, "The dungeon collapses and buries %s under a plenty of stone; %s've been down there for too long!", rogue_pronoun(1), rogue_pronoun(0));
    do_quit_msg(ecOkay, strbuf);
  }
  statistics_invalidate(svfTick);

  creatures_perform_tick(); creature_age_inventories(1); perform_rogue_tick();
  age_rogue_inventory(); section_perform_tick();

  /* Make sure that new creatures appear so that the player doesn't get bored
     after defeating all the initially existing ones. :-) */

  count = section_countcreatures();
  if (count < difficulty) { creature_autocreate(); } /* immediately */
  else if ( (count < 4 + pref_combat) && (! (dungeonclock %
            (200 - 10 * difficulty - sectionsizemeasure / 1000
                 - 2 * current_storeynum)) ) )
  { creature_autocreate(); } /* periodically */
}

static void handle_being_held(const tUint8 flags)
{ const char* msg;
  switch (roguedata.holding_cause)
  { case hcBearTrap: msg = "still stuck in the bear trap"; break;
    case hcPit: msg = "still stuck in the pit"; break;
    case hcCobweb: msg = "still stuck in cobweb"; break;
    default: msg = "still being held"; break;
  }
  message_show(msg); movekind = mkSingleStep;
  if (flags) _do_tick = truE;
}

static /*@null@*/ tObstacle* path_obstacled(const tCoordinate x,
  const tCoordinate y)
/* returns whether there's an "actual" obstacle in the way */
{ tObstacle* retval;
  const tSquareFlags sflags = squareflags_get(x, y);
  if (!(sflags & sfObstacle)) { none: retval = NULL; goto out; }
  if ( (retval = location2obstacle(x, y)) == NULL ) /* "should not happen" */
    goto out;
  if (obstacle_effsize(retval) <= 0) goto none; /* nothing in the way */
  out: return(retval);
}

static void rogue_move_to(const tCoordinate x2, const tCoordinate y2,
  const tDirection dir)
{ const tBoolean leaving_shop = cond2boolean( (squareflags_get(x2, y2) &
    sfDoorway) && (rogue_in_shop) );
  creature_moveabs(rogue, x2, y2, dir); rogue_las(cakMove, 0);
  if (leaving_shop) /* rare */
  { if (rogue_has_unpaid_items) infuriate_shopkeeper(NULL, falsE); /* stole */
    else cleanse_price_tags(); /* forget shopkeeper's offers */
  }
}

static void handle_move(tDirection dir, const tBoolean immediate)
{ tCoordinate x = rogue->x, y = rogue->y, x2, y2;
  tSquareFlags sflags2;
  const tObject* ring;
  tPropertyExtent ench;
  tObstacle* obstacle;

  if (immediate) goto do_handle_move;

#if 0
  /* IMPLEMENTME: enable the following code when creature_polymorph() is
     implemented! */
  /* Check for polymorph rings */
  worn_rings_enchsum(ench, ring, rkPolymorph);
  if ( (ench < 0) && (randpercent(difficulty - rogue_rings2fortune(5))) )
  { (void) creature_polymorph(rogue);
    rogue_las(cakOther, 0); movekind = mkSingleStep; return;
  }
#endif

  /* Check for teleport rings */
  worn_rings_enchsum(ench, ring, rkTeleport);
  if ((ench < 0) && (randpercent(4 + 2 * difficulty - rogue_rings2fortune(3))))
  { creature_teleport(rogue); rogue_las(cakOther, 0); movekind = mkSingleStep;
    return;
  }

  if (rogue->confused) { dir = randdir(); movekind = mkSingleStep; }

#if 0
  /* IMPLEMENTME: enable the following code when creature_knightmove() is
     implemented! */
  /* Check for knightmove rings */
  worn_rings_enchsum(ench, ring, rkKnightmove);
  if ((ench < 0) && (randpercent(4 + 2 * difficulty - rogue_rings2fortune(3))))
  { creature_knightmove(rogue, dir);
    /* rogue_las(cakMove, 0); */ movekind = mkSingleStep; return;
  }
#endif

  do_handle_move:

  x2 = x + xdir[dir]; y2 = y + ydir[dir];
  if (!is_location_valid(x2, y2)) { movekind = mkSingleStep; return; }
  if ( (is_diagdir(dir)) && (!creature_can_act_diagonally(rogue, x2, y2)) )
  { movekind = mkSingleStep; return; }
  sflags2 = squareflags_get(x2, y2);
  if (sflags2 & sfBarrier)
  { sprintf(strbuf, "ouch, hit %s barrier", ( map_can_player_see(x2, y2,
      mfBarrier) ? (strA) : (strThe) ));
    message_show(strbuf);
    if ( (randcointoss()) && (!creature_dec_health(rogue, 1)) )
      rogue_died(drInjury, NULL);
    rogue_las(cakOther, 0); movekind = mkSingleStep; return;
  }
  else if (sflags2 & sfCreature)
  { tCreature* const creature = location2creature(x2, y2);
    movekind = mkSingleStep;
    if (!creature) return; /* "should not happen" */
    if ( (only_moving_onto) && (!(creature->flags & cfImitates)) )
    { /* useful if cannot see and tries to avoid fights */
      message_show("there's a creature");
    }
    else { creature_hit(rogue, creature); rogue_las(cakHit, 0); }
  }
  else if ( ( (obstacle = path_obstacled(x2, y2)) != NULL ) &&
    (!(sflags2 & sfHidden)) )
  { const tObstacleSize obsize = obstacle->size;
    if (obstacle->flags & obfPushable) { /* FIXME! */ }
    /* IMPLEMENTME: never try to push if <only_moving_onto>! */
    if ( (obsize < 7) && (creature_is_in_the_air(rogue)) )
    { /* (try to) float over the obstacle */
      if (rogue->held) { handle_being_held(1); return; }
      else rogue_move_to(x2, y2, dir);
    }
    else if (movekind == mkSingleStep) /* (try to) dig the obstacle */
    { const tHoeValue h = object_hoevalue(rogue->weapon,
        obstacle_materials(obstacle), 1);
      if ( (h <= 0) || (!obstacle_resistance_decrease(obstacle, h, 1)) )
        say_obstacle_resists(obstacle);
      rogue_las(cakOther, 0);
    }
    else movekind = mkSingleStep;
  }
  else if (rogue->held) { handle_being_held(1); return; } /* can't move */
  else if ( (sflags2 & sfRoom) ||
            ( (!(sflags2 & sfHidden)) && (sflags2 & (sfTunnel | sfDoorway)) ) )
  { tObject* object;
    char* temp;
    const tBoolean in_air = creature_is_in_the_air(rogue);

    /* Handle the actual move */

    if ( (sflags2 & (sfWater | sfSwamp)) && (difficulty < dkCrazy) &&
         (!in_air) && (movekind != mkGliding) &&
         (!(sflags2 & (sfHidden | sfPlatform))) &&
         (map_can_player_see(x2, y2, mfGround)) )
    { if (sflags2 & sfWater)
      { if (ask_for_key_yn("really step on the water?") != 'y')
        { movekind = mkSingleStep; return; }
      }
      else if (sflags2 & sfSwamp)
      { if (ask_for_key_yn("really step on the swamp?") != 'y')
        { movekind = mkSingleStep; return; }
      }
    }
    rogue_move_to(x2, y2, dir);
    if ( (movekind != mkGliding) && (!in_air) && (sflags2 & sfGlazedFrost) )
    { const tObject* const boots = roguedata_os(rdosiBoots);
      const tBoolean safe = cond2boolean( (boots != NULL) && (boots->kind ==
        tokBootsFriction) && (boots->enchantment >= 0) );
      /* CHECKME: safe if rogue_chained and the chain is maximally stretched?*/
      if (safe) message_show(strSlippery);
      else { message_show(strWheeSlippery); movekind = mkGliding; }
    }

    /* Handle object picking */

/* The following macro is for cases when the player gets a message about the
   object without picking it. Scrolls are an exception because the scroll title
   shall only be readable when the player "unrolls the scroll with his hands" -
   which he can only do when having it in the pack. */
#define SET_KNOWN_MATERIAL \
  do \
  { if ((map_can_player_see(x2, y2, mfObject)) && (object->oc != ocScroll)) \
      object->flags |= ofKnownMaterial; \
  } while (0)

    if (movekind == mkGliding) return; /* can't pick while gliding */
    if (! (sflags2 & sfObject) ) return; /* no object here, nothing to do */
    movekind = mkSingleStep; object = location2object(x2, y2);
    if (object == NULL) return; /* "should not happen" */
    temp = strbuf;
    if (in_air) /* can't pick objects while floating in the air */
    { strcpy(temp, "floating over ");
      temp += 14;
      SET_KNOWN_MATERIAL;
    }
    else if (only_moving_onto) /* player doesn't want to pick */
    { strcpy(temp, "moving onto ");
      temp += 12;
      SET_KNOWN_MATERIAL;
    }
    else if (!creature_can_carry_object(rogue, object, 0)) /* pack full */
    { message_show(strPackFull);
      strcpy(temp, "not picking ");
      temp += 12;
      SET_KNOWN_MATERIAL;
    }
    else /* actually pick the object */
    { do_pick_object(&object, object->quantity, &temp); }
    object_get_description(object, temp);
    message_show(strbuf);
#undef SET_KNOWN_MATERIAL
  }
  else
  { if ( (sflags2 & sfWall) && (!(sflags2 & sfDoorway)) &&
         ( (movekind == mkSingleStep) || (movekind == mkGliding) ) )
    { /* check whether the player character hoes the wall; note that we only do
         this for mkSingleStep (hoeing intentionally) and mkGliding (lost
         control) */
      const tHoeValue p = object_hoevalue(rogue->weapon, bomStone, 1);
      if (p > 0) section_hoe_wall(x2, y2, dir, p | 16);
    }
    movekind = mkSingleStep;
  }
}

static void stagger_resp_drift(void)
{ if (restcounter) interrupt_rest();
  movekind = mkSingleStep;
  handle_move(randdir(), truE);
}

static void do_rest(void)
{ restcounter--; rogue_las(cakRest, 0);
  if ( (randpercent(rogue->confused)) || (creature_drifts(rogue)) )
  { /* Even if only resting, a confused player character might stagger
       sometimes - but we don't let polymorph/teleport/knightmove/traps happen
       then. */
    /* If the player character hangs around while being levitated, he starts
       drifting. Note that the percentage doesn't depend on ->levitated (he's
       just levitated, no matter for how long), in contrast to the handling of
       ->confused (which is an indicator for the _extent_ of confusion). */
    stagger_resp_drift();
  }
}

static one_caller void try_to_go_down(void)
{ const tCoordinate x = rogue->x, y = rogue->y;
  const tSquareFlags sflags = squareflags_get(x, y);
  const tSeconn* seconn;
  if (sflags & sfPlatform)
  { failed: message_show("can't go down here"); return; }
  if (!(sflags & sfSeconn))
  { if (0) /* IMPLEMENTME: if (there's a grfkHollow) */
    { if (rogue->levitated) goto lev;
      if (rogue->held) goto hbh;
      message_show("the hollow isn't yet deep enough");
      message_show("dig some more to create a hole"); return;
    }
    goto failed;
  }
  if ( ( (seconn = location2seconn(x, y)) == NULL ) ||
    (!(seconn_downup(seconn) & 1)) )
  { goto failed; }
  if (rogue->levitated)
  { lev: message_show("can't go down while floating in the air"); return; }
  if (rogue->held) { hbh: handle_being_held(0); return; }
  (void) pass_seconn(seconn, 0);
  /* IMPLEMENTME: increase the player character's experience if he enters a
     newly created storey? Nah, maybe when reaching a special department. */
  /* IMPLEMENTME: if the player character's pack is very heavy, he often falls
     down the stairs/ladder! */
  if ( (difficulty > dkNormal) && (__randpercent(8 * difficulty)) )
  { /* IMPLEMENTME: a dungeon trap triggers, the player character gets dazed,
       falls down the stairs/ladder (-> decrease health by (difficulty -
       dkNormal)) and takes a nap (unless he can resist sleep!). */
  }
}

static one_caller void try_to_go_up(void)
{ const tCoordinate x = rogue->x, y = rogue->y;
  const tSeconn* seconn;
  if (!(squareflags_get(x, y) & sfSeconn))
  { failed: message_show("can't go up here"); return; }
  if ( ( (seconn = location2seconn(x, y)) == NULL ) ||
    (!(seconn_downup(seconn) & 2)) )
  { goto failed; }
  if (rogue->held) { handle_being_held(0); return; }
  if  ( (!rogue->levitated) && (seconn->kind == sckHoleUp) )
  { message_show("can't jump upward through the hole");
    message_show("ceiling too far away"); return;
  }
  if (current_storeynum > 1) (void) pass_seconn(seconn, 0);
  else try_to_leave_the_dungeon();
}

static void what_is_on_square(const tCoordinate scrx, const tCoordinate scry,
  const tBoolean by_mouse)
/* lets the player know what's on the given map square */
{ tCoordinate x, y;
  tSquareFlags sflags;
  const tMapField* m;
  tMfdk mfdk;
  const tCreature* creature;
  const tObstacle* obstacle;
  const tObject* object;
  const tSeconn* seconn;
  const char* str;
  if (!scrloc2secloc(scrx, scry, &x, &y)) { x = scrx; y = scry; goto mousemsg;}
  sflags = squareflags_get(x, y); m = mapfieldptr(x, y); mfdk = m->dispkind;
  str = strMfdk[mfdk]; /* generic default description text */
  switch (mfdk)
  { case mfdkGround:
      if (sflags & sfKnownTunnel) { str = "tunnel"; break; }
      /*@fallthrough@*/
    case mfdkEmpty: mousemsg:
      sprintf(strbuf, "%s: %d, %d", (by_mouse ? "mouse click" : "location"),
        x, y);
      str = strbuf; break;
    case mfdkCreature:
      if ( (creature = location2creature(x, y)) == NULL ) break;
      if (creature == rogue) str = "that's you!"; /* :-) */
      else if (creature->flags & cfImitates)
      { if ((map_br(x, y) >= BRIGHTNESS_CLEAR) || (rogue->flags & cfSensCreat))
        { const tCreatureData cd = creature->cd;
          (void) _object_get_minidesc(IMI2CLASS(cd), IMI2KIND(cd), 1, strbuf,
            0, NULL);
          str = strbuf;
        }
        else str = strMfdk[mfdkObject];
      }
      else if ( (map_br(x, y) >= BRIGHTNESS_CLEAR) ||
        (rogue->flags & cfSensCreat) )
      { creature_get_name(creature, strbuf, 2); str = strbuf; }
      break;
    case mfdkObstacle:
      if ( (obstacle = location2obstacle(x, y)) != NULL )
      { (void) section_obstacle_get_name(obstacle, strbuf,2|4); str = strbuf; }
      break;
    case mfdkObject:
      if ( (map_br(x, y) >= BRIGHTNESS_CLEAR) ||
        (roguedata.flags & rfSenseObjects) )
      { if ( (object = location2object(x, y)) != NULL )
        { (void) object_get_minidesc(object, strbuf, 0, NULL); str = strbuf; }
      }
      break;
    case mfdkSeconn:
      if ( (seconn = location2seconn(x, y)) != NULL )
        str = seconn_desc(seconn, 2);
      break;
    case mfdkPlatform:
      if (sflags & sfGlazedFrost) str = "glazed frost";
      break;
    case mfdkGroundFeature:
      if (sflags & sfWater) str = "water";
      else if (sflags & sfSwamp) str = "swamp";
      break;
    case mfdkMisty:
      if (sflags & sfFire) str = "fire";
      break;
  }
  message_show(str);
}

static one_caller void navigate_on_map(void)
{ tCoordinate sectx0 = rogue->x, secty0 = rogue->y, scrx, scry, dummy;
  tDirection dir;
  tKey key;
  if (!secloc2scrloc(sectx0, secty0, &scrx, &scry)) return; /*"can't happen"*/
  message_show("move the cursor to the location and press <Return>");
  loop0: (void) move(scry, scrx); (void) doupdate();
  loop1: key = my_getch();
  if (key == KEY_CANCEL) return;
  else if (key == KEY_ENTER)
  { message_clear(); what_is_on_square(scrx, scry, falsE); return; }
  else if (key2dir(key, &dir, NULL))
  { const tDirectionOffset dx = xdir[dir], dy = ydir[dir];
    const tCoordinate x2 = scrx + dx, y2 = scry + dy;
    if (scrloc2secloc(x2, y2, &dummy, &dummy)) /* still inside the section */
    { scrx = x2; scry = y2; goto loop0; }
  }
  goto loop1;
}

static one_caller void handle_extended_command(void)
{ const tKey key = ask_for_key("which extended command?", "cCm"
#if CONFIG_DO_TEXTMODEMOUSE
"M"
#endif
"s"
#if OPTION_ANIMATION
"+-"
#endif
    );
  switch (key)
  { case 'c': cast_spell(); break;
    case 'C': call_object(); break;
    case 'm':
      (void) object_dump_associations(0); cursor_reset_position(); break;
#if CONFIG_DO_TEXTMODEMOUSE
    case 'M':
      { static tBoolean is_on = truE;
        if (is_on) { is_on = falsE; (void) mousemask(0, NULL); }
        else { is_on = truE; (void) mousemask(TEXTMODEMOUSE_MASK, NULL);}
      }
      break;
#endif
    case 's': save_screenshot(); break;
#if OPTION_ANIMATION
    case '+': case '-':
      animation_change_step_duration(cond2boolean(key == '+')); break;
#endif
  }
}

static one_caller void handle_key(tKey key)
{ tDirection dir;
  message_clear();
#if CONFIG_DO_TEXTMODEMOUSE
  if (key == KEY_MOUSE)
  { MEVENT event;
    short x, y;
    mmask_t mask;
    if (getmouse(&event) != OK) return;
    mask = event.bstate;
    if (!(mask & TEXTMODEMOUSE_MASK)) return; /* "can't happen" */
    x = event.x; y = event.y;
    what_is_on_square(x, y, truE);
    return;
  }
#endif
  if (key == '\020') { message_show_previous(); return; } /* Ctrl-p */
  if (key == 'E') /* "explain"/help */
  { /* This _must_ be checked early because other keys might cause an action
       which results in changing the specific help text. - The generic help
       texts are sorted by decreasing "urgency". */
    const char* temp;
    if (strHelp != NULL) temp = strHelp; /* specific help text available */
    else if ( (rogue->held) && (roguedata.holding_cause == hcPit) )
      temp = "try to move!";
    else if (roguedata.state >= rsHungry) temp = "eat something!";
    else if ( (current_dd->kind == ddkDeadEnd) && (difficulty <= dkNormal) )
      temp = "get back into the main department!";
    else if (rogue_has_objectclasses(1 << ocAmulet))
      temp = "try to reach the daylight!";
    else temp = "try to find the Amulet of Yendor!";
    message_show(temp); return;
  }

  strHelp = NULL; /* default (no _specific_ help text available) */

  switch (key)
  { case 'a': apply(); break;
    case 'c': close_door(); break;
    case 'C': entered_number = ask_for_number("number?");
      statistics_invalidate(svfCount); break;
    case 'd': drop(); break;
    /* case 'D': IMPLEMENTME: drop certain classes or unpaid objects or
       everything! break; */
    case 'e': eat(); break;
    case 'g': navigate_on_map(); break; /* glimpse at what's on a location */
    case 'i': (void) inventory(4, ocbAll); break; /* show full inventory */
    case 'I': (void) special_inventory(); break; /*certain parts of inventory*/
    case 'm': /* move onto */
      if (ask_for_dir(&dir, NULL)) { only_moving_onto=truE; goto handle_dir; }
      break;
    case 'o': open_door(); break;
    case 'p': pick_object(); break;
    case 'P': puton_ring(); break;
    case 'q': quaff(); break;
    case 'Q':
      if (ask_for_key_yn("really quit?") == 'y') do_quit(ecOkay);
      break;
    case 'r': read_scroll(); break;
    case 'R': remove_ring(); break;
    case 's':
      if (search_for_hidden(falsE))
        message_show(strWhoops); /* CHECKME: only if can _see_ a change? */
      rogue_las(cakSearch, 0); break;
    case 'S': /* sleep */
      { tFewTicks nap = entered_fewticks(1) + randbytemod(3);
        if (creature_is_ill(rogue)) nap += randbytemod(3) + 1;
        creature_nap(rogue, nap, 2); rogue_las(cakOther, 0);
      }
      break;
    case 't': throw(); break;
    case 'T': unwear(); break; /* 'T' as in "take off" */
    case 'v': message_show(strVersion); break;
    case 'V': show_extended_version_information(); break;
    case 'w': wield(); break;
    case 'W': wear(); break;
    case 'x': unwield(); break; /* CHECKME: find a better key! */
    case 'X': handle_extended_command(); break;
    case 'z': zap(); break;
    case 4: kick(); break; /* Ctrl-d */
    case 23: wish(); break; /* Ctrl-w */
    case 24: base_attributes(); break; /* Ctrl-x */
    case '.': case '5': /* rest */
      if (roguedata.flags & rfRestless)
      { sprintf(strbuf, "%s %s restless", rogue_pronoun(0), rogue_verb());
        message_show(strbuf);
      }
      else { restcounter = entered_fewticks(1); do_rest(); }
      break;
    case '?': show_help_screen(); break;
    /* IMPLEMENTME: case 'f': fight in a given direction; if no creature is
       there, do nothing. (Useful if cannot see and guesses where an attacker
       is.) */
    /* case 'F': fight in a random direction; useful when several creatures are
       nearby and the player wants to fight them all. */
    /* IMPLEMENTME: pay one/all unpaid inventory items in a shop! */
    /* IMPLEMENTME: extended command "bring together", e.g. to attach something
       to the tip of a polearm or to dip something into a fountain! */
    /* IMPLEMENTME: verbose command "#dip": dip a non-potion object into a
       potion or any object into a water square or kitchen sink or such? */
    /* IMPLEMENTME: extended command "loot" for large tokBox objects! */
    case '>': try_to_go_down(); break;
    case '<': try_to_go_up(); break;
    /* IMPLEMENTME: a "redraw entire screen" key! */
    default:
      if (key2dir(key, &dir, &movekind)) /* very likely */
      { if ( (movekind == mkSingleStep) &&
          (entered_number != ENTERED_NUMBER_INVALID) )
        { movecountdown = entered_fewticks(1); movekind = mkCountdown; }
        handle_dir: handle_move(dir, falsE); roguedata.attemptedmovecount++;
      }
      else maybe_partial_inventory(key);
      break;
  }
}

static one_caller void one_pc_action(void)
/* allows to perform one player character action */
{ tBoolean can_act = cond2boolean(!(rogue->napping | rogue->frozen));
  if (movekind == mkGliding)
  { const tDirection dir = rogue->lastdir;
    if ( (dir == dirNone) || (rogue->held) ||
         (!(squareflags_get(rogue->x, rogue->y) & sfGlazedFrost)) ||
         (creature_is_in_the_air(rogue)) )
      movekind = mkSingleStep;
    else { handle_move(dir, truE); return; }
  }
  else if (movekind == mkCountdown)
  { tDirection dir;
    if ( (--movecountdown > 0) && ( (dir = rogue->lastdir) != dirNone) )
    { handle_move(dir, truE); can_act = falsE; }
    else movekind = mkSingleStep;
  }
  else if ( (movekind == mkUntilStopped) || (movekind == mkUntilNextToSth) )
  { const tDirection dir = rogue->lastdir;
    if ( (dir != dirNone) &&
         ( (movekind == mkUntilStopped) || (!next_to_something()) ) )
    { handle_move(dir, truE); can_act = falsE; }
    else movekind = mkSingleStep;
  }

  if ( (can_act) && (restcounter > 0) )
  { if (next_to_something()) interrupt_rest();
    else { do_rest(); return; }
  }

  if (!can_act)
  { restcounter = 0; rogue_las(cakUnknown, 0);
    if (creature_drifts(rogue)) stagger_resp_drift();
  }
  else /* let the player play, finally :-) */
  { const tKey key = my_getch();
    if (key != ERR) { random_entropy_from_key(key); handle_key(key); }
  }
}

static one_caller void check_rf_stoppers(void)
{ if (roguedata.flags & rfStoppers)
  { if (restcounter) interrupt_rest();
    if (movekind != mkGliding) movekind = mkSingleStep;
  }
  roguedata.flags &= ~(rfStoppers | rfInvPropertyChanged);
}

static one_caller void try_claptrap(void)
/* Traps must be handled at a relatively "quiet" moment. Don't handle them e.g.
   in the middle of creature_moveabs() - look what would happen there after a
   trapdoor or teleport trap! */
{ if ( (rogue->cak == cakMove) && (section_might_trap()) )
  { const tStoreyNumber storey = current_storeynum;
    roguedata.traptick++;
    if ( (roguedata.traptick > 500 - 20 * difficulty - 7 * storey) ||
         (randpercent(roguedata.traptick / (100 - 5*difficulty - storey/2))) )
    { section_handle_traps(); }
  }
}

void player_tickout_handler(tTickout* t __cunused, const tTickoutAction ta)
{ if (ta == taTrigger)
  { const tUint8 old_cakcount = roguedata.cakcount;
    _do_tick = only_moving_onto = falsE;
    check_rf_stoppers();
    do
    { map_do_update_all(); statistics_update();
      cursor_reset_position(); (void) doupdate();
      /* (all of the above is sometimes necessary, but IMPROVEME!) */
      one_pc_action();
    } while ( (!_do_tick) && (difficulty < dkCrazy) );
    if (roguedata.cakcount == old_cakcount) rogue_las(cakUnknown, 0);
    if (_do_tick)
    { tRogueDataStats* const rds = &(roguedata.rds);
      rds->num_actions++;
      if (rogue_can_see) rds->num_seeing++;
      if ( (rds->inventoresque > 0) && (rds->inventorized < 60000) )
        rds->inventorized++; /* player is violating "inventoryless" conduct */
      roguedata.digestcount += ACTIONDIGEST; try_claptrap();
    }
    aftermath_run();
    rogue->last_action = _dungeonclock; creature_tickout_recalc(rogue, 1);
  }
}

int rogue_main(int argc, const char** argv)
{ tTickout* t;
  tMilliticks expiry, clockskip, tickskip;
  initialize(argc, argv);
  player_tickout_handler(&(rogue->tickout), taTrigger); /* player acts first */
  mainloop:
  if ( (t = active_tickouts) != NULL )
  { if ( (expiry = t->expiry) <= _dungeonclock ) /* must do something now */
    { tickout_detach(t); ((t->handler)(t, taTrigger)); goto mainloop; }
    tickskip = expiry - _dungeonclock;
  }
  else tickskip = 1000; /* just a value that works; higher numbers work too */
  clockskip = t2mt(mt2t(_dungeonclock + 1000)) - _dungeonclock;
  if (clockskip <= tickskip) { _dungeonclock += clockskip; perform_tick(); }
  else _dungeonclock += tickskip;
  goto mainloop;
}
