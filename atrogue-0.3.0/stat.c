/* atrogue/stat.c - game statistics
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "stat.h"
#include "creature.h"
#include "dungeon.h"

static tStatisticsValidityFlags stat_invalid;
static tCoordinate statistics_row, colOtherStuff;
static char diffch; /* "difficulty character */

static const char strFiveSpaces[] = "     ", strDD[] = "%d/%d",
  strStatline[] = "L:x00 H:x00/00 S:00/00 E:00 A:00 M:00&00 T:0";
/* (level, health, strength, experience, armor, magic energy&experience,
    elapsed dungeon ticks, other stuff) */

/* constant columns for the values in the statistics line */
enum
{ colLevel = 3, colState = 8, colHealth = 9, colStrength = 17,
  colExperience = 25, colArmor = 30, colMagicEnergy = 35,
  /*colMagicExperience = 38,*/ colTick = 43
};

void statistics_invalidate(tStatisticsValidityFlags flags)
{ stat_invalid |= flags;
}

/* Pad <strbuf> with trailing spaces so that the length becomes 5: */
#define PAD5 \
  do \
  { const size_t len = strlen(strbuf); \
    if (len < 5) strcpy(strbuf + len, strFiveSpaces + len); \
  } while (0)

void statistics_update(void)
/* updates the statistics row as necessary; the _caller_ is responsible for
   calling doupdate() afterwards! */
{ if (stat_invalid == svfNone) return; /* nothing to do */
#if CONFIG_EXDEBUG
  sprintf(debugstrbuf, "invalid stats: %d", stat_invalid);
  message_showdebug(debugstrbuf);
#endif
  if (stat_invalid & svfLevel)
  { sprintf(strbuf, str2D, current_storeynum);
    mvaddstr(statistics_row, colLevel, strbuf);
  }
  if (stat_invalid & svfState)
  { static const char statechar[maxRogueState + 1] = "nhwfs";
    chtype ch = (chtype) statechar[roguedata.state];
    if (roguedata.state > rsHungry) ch |= cbRed;
    mvaddch(statistics_row, colState, ch);
  }
  if (stat_invalid & (svfHealth | svfMaxHealth))
  { sprintf(strbuf, strDD, rogue->health, roguedata.maxhealth); PAD5;
    mvaddstr(statistics_row, colHealth, strbuf);
  }
  if (stat_invalid & (svfStrength | svfMaxStrength))
  { sprintf(strbuf, strDD, rogue->strength, roguedata.maxstrength); PAD5;
    mvaddstr(statistics_row, colStrength, strbuf);
  }
  if (stat_invalid & svfExperience)
  { sprintf(strbuf, str2D, rogue->experience);
    mvaddstr(statistics_row, colExperience, strbuf);
  }
  if (stat_invalid & svfArmor)
  { const tObject* const armor = rogue->armor;
    const char* text;
    if (armor == NULL) text = "- ";
    else if (!(armor->flags & ofDamageIdentified)) text = "? ";
    else { sprintf(strbuf, str2D, armor->damage); text = strbuf; }
    mvaddstr(statistics_row, colArmor, text);
  }
  if (stat_invalid & (svfMagicEnergy | svfMagicExp))
  { sprintf(strbuf, "%d&%d", roguedata.magic_energy,roguedata.magic_exp); PAD5;
    mvaddstr(statistics_row, colMagicEnergy, strbuf);
  }
  if (stat_invalid & svfTick)
  { static tCoordinate oldcolOtherStuff = 0;
    char* ptr = strbuf;
    ptr += sprintf(ptr, strPercd, dungeonclock);
#if CONFIG_DEBUG
    *ptr++ = '.'; *ptr++ = '0' + (_dungeonclock % 1000) / 100; *ptr = '\0';
#endif
    mvaddstr(statistics_row, colTick, strbuf);
    colOtherStuff = colTick + (ptr - strbuf) + 1;
    if (oldcolOtherStuff < colOtherStuff) /* tick string became longer */
    { oldcolOtherStuff = colOtherStuff;
      (void) addch(' '); /* (might be necessary to erase old "other stuff"!) */
      stat_invalid |= svfOtherStuff; /* enforce reprint at new column */
    }
  }
  if (stat_invalid & svfOtherStuff)
  { char* s = strbuf;
    const tFewTicks slowedhasted = rogue->slowedhasted;
    if (entered_number != ENTERED_NUMBER_INVALID)
      s += sprintf(s, "C:%d ", entered_number);
    if (rogue->_blind) *s++ = 'B';
    if (rogue->confused) *s++ = 'C';
    if (rogue->frozen) *s++ = 'F';
    if (slowedhasted > 0) *s++ = 'H';
    if (rogue->flags & cfInvisible) *s++ = 'I';
    if (rogue->levitated) *s++ = 'L';
    if (rogue->napping) *s++ = 'N';
    if (slowedhasted < 0) *s++ = 'S';
    *s = '\0';
    mvaddstr(statistics_row, colOtherStuff, strbuf);
    (void) clrtoeol(); /* erase any old stuff */
  }

  cursor_reset_position(); /* IMPROVEME: only do this when necessary? */
  stat_invalid = svfNone; /* we've validated everything... */
}

#undef PAD5

void statistics_screenredraw(void)
{ mvaddstr(statistics_row, 0, strStatline); mvaddch(statistics_row, 2, diffch);
  stat_invalid = svfAll; /* ensure that the values are redrawn too */
}

void __init statistics_initialize(void)
{ diffch = difficulty + 'a'; /* (constant during the game) */
#if CONFIG_DEBUG /* make dbg'ing obvious during gameplay and in screenshots */
  diffch -= ('a' - 'A');
#endif
  statistics_row = screen_height - 1; statistics_screenredraw();
}
