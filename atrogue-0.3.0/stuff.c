/* atrogue/stuff.c - general stuff
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "random.h"
#include "creature.h"

/* Strings */

char strbuf[STRBUF_SIZE]; /* buffer for string operations */
#if CONFIG_DEBUG
char debugstrbuf[STRBUF_SIZE]; /* buffer for debug string operations */
#endif

const char strVersion[] = "atrogue " ATROGUE_VERSION
  " - Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>",
  strVersionShort[] = "atrogue " ATROGUE_VERSION;

const char strNewline[] = "\n", strWhoops[] = "whoops!", strYes[] = "yes",
  strNo[] = "no", strSpaceMisses[] = " misses", strSomething[] = "something",
  strSomethingHits[] = "something hits", strItPasses[] = "it passes",
  strIsogh[] = "it smells of gas here!", strQuiteHot[] ="it's quite hot here!",
  strThe[] = "the", strAre[] = "are", strIt[] = "it", strThey[] = "they",
  strUnknown[] = "(unknown)", strSpace[] =" ", strA[] = "a", strIs[] = "is",
  strSpacedDash[] = " - ", str2D[] = "%-2d", strPerccPercd[] = "%c%d";

const char* const strDifficulty[maxDifficulty + 1] =
{ "0 (very easy)", "1 (easy)", "2 (normal)", "3 (hard)", "4 (very hard)",
  "5 (drive me crazy)"
};


/* Colors */

#if OPTION_COLORS
tColorBitmask cbRed = 0, cbGreen = 0, cbBlue = 0, cbYellow = 0, cbCyan = 0;
tBoolean __use_color;
#endif

/* color names for potions etc.; must be in sync with the color code numbers */
const char* const strColor[maxColorCode + 1] =
{ /* ncurses colors: */
  "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
  /* "our" colors: */
  "aquamarine", "beige", "brown", "burgundy", "clear", "gray", "orange",
  "pink", "plaid", "purple", "violet"
};


/* General variables */

const_after_init tDifficulty difficulty = dkNormal;
tRole pref_role = defpref_role, startpref_role;
tCreatureGenderKind pref_gender = defpref_gender, startpref_gender;
tCreatureAlignment pref_align = defpref_align, startpref_align;
const_after_init unsigned char pref_combat = defpref_combat,
  pref_expl = defpref_expl, pref_magic = defpref_magic,
  pref_feat = defpref_feat, pref_shops = defpref_shops;
tMilliticks _dungeonclock = 0;
tCoordinate maxSectionWidth, maxSectionHeight;
tBoolean is_errorsave_enabled = falsE;
tEnteredNumber entered_number = ENTERED_NUMBER_INVALID;

#if OPTION_ANIMATION
tBoolean arg_allows_animation = truE;
#endif

#if CONFIG_DEMO || CONFIG_REMOTE
tRunmode runmode = rmNormal;
#endif

#if CONFIG_DEBUG
tBoolean do_log = truE;
#else
tBoolean do_log = falsE;
#endif

#if !CONFIG_GRAPHICS
tBoolean need_endwin = falsE;
#endif


/* Quitting */

static one_caller void save_due_to_error(void)
{ if (!is_errorsave_enabled) /* nothing created yet, so nothing to save */
    return;
  /* IMPLEMENTME! */
}

void do_quit_msg(tExitcode exitcode, const char* msg)
{
#if !CONFIG_GRAPHICS
  if (need_endwin) (void) endwin();
#endif

  if ( (msg != NULL) && (msg[0] != '\0') ) (void) puts(msg);
  if (exitcode != ecOkay) save_due_to_error();

#if CONFIG_GRAPHICS
  gtk_exit(exitcode);
#endif
  exit(exitcode);
}

my_inline void do_quit(tExitcode exitcode)
{ do_quit_msg(exitcode, strEmpty); /* quit without any message */
}

#if CONFIG_DEBUG
my_inline void do_quit_debugmsg(const char* msg)
{ do_quit_msg(ecDebug, msg);
}
#endif


/* Memory management */

static my_inline void does_not_return out_of_memory(void)
{ do_quit_msg(ecOOM, "out of memory");
}

void* __memory_allocate(size_t size)
{ void* ptr = malloc(size);
  if (!ptr) out_of_memory();
  return(ptr);
}

void* memory_allocate(size_t size)
{ void* ptr = __memory_allocate(size);
  my_memclr(ptr, size);
  return(ptr);
}

void* memory_reallocate(void* ptr, size_t size)
{ void* ptr2 = realloc(ptr, size);
  if (ptr2 == NULL) out_of_memory();
  return(ptr2);
}

my_inline void memory_deallocate(const void* ptr)
{ free(__unconstify(void*, ptr));
}


/* String functions */

char* my_strdup(const char* str)
/* duplicates a string; the standard library function strdup() isn't portable,
   and callers always have to check against a NULL return value - two reasons
   for using something different... */
{ const size_t size = strlen(str) + 1;
  char* retval = __memory_allocate(size);
  my_memcpy(retval, str, size);
  return(retval);
}

const char* indefinite_article(const char ch)
{ static const char* const article[2] = { strA, "an" };
  return(article[cond2bool(is_vowel(ch))]);
}

#if CONFIG_DEMO

const char strI[] = "I";

my_inline const char* rogue_pronoun(const unsigned char _case)
/* <_case>: 0=nominative, 1=accusative, 2=genitive */
{ static const char* const pronoun[5] = { strI, "me", "my", "you", "your" };
  if (runmode != rmDemo) _case = 3 + cond2bool(_case == 2);
  return(pronoun[_case]);
}

my_inline char* rogue_verb(void)
{ static const char* const verb[2] = { strAre, "am" };
  return(unconstify(verb[cond2bool(runmode == rmDemo)]));
}

#else /* #if CONFIG_DEMO */

my_inline const char* rogue_pronoun(const unsigned char _case)
{ static const char* const pronoun[2] = { "you", "your" };
  return(pronoun[cond2bool(_case == 2)]);
}

my_inline const char* rogue_verb(void)
{ return(strAre);
}

#endif /* #if CONFIG_DEMO */


/* Direction-related stuff */

const tDirection counterdir[9] = { dirNone, dirEast, dirWest, dirSouth,
  dirNorth, dirSouthEast, dirSouthWest, dirNorthEast, dirNorthWest };

const tDirectionOffset xdir[9] = { 0, -1, +1, 0, 0, -1, +1, -1, +1 };
const tDirectionOffset ydir[9] = { 0, 0, 0, -1, +1, -1, -1, +1, +1 };

const char* const strDirection[9] = { strUnknown, "west", "east", "north",
  "south", "northwest", "northeast", "southwest", "southeast" };

tDirection diroffsets2dir(tDirectionOffset xoff, tDirectionOffset yoff)
{ tDirection dir;
  for_each_dir(dir)
  { if ( (xdir[dir] == xoff) && (ydir[dir] == yoff) )
      return(dir);
  }
  return(dirNone); /* "should not happen" */
}

tBoolean are_locations_in_line(tCoordinate x, tCoordinate y, tCoordinate x2,
  tCoordinate y2)
/* checks whether the given locations are "in line", i.e. on the same
   horizontal, vertical or diagonal line */
{ tCoordinate a = ABS(x2 - x), b = ABS(y2 - y);
  if ( (a == 0) || (b == 0) ) return(truE); /* vertical/horizontal */
  else if (a == b) return(truE); /* diagonal */
  else return(falsE);
}

tDirection __randdir; /*global loop variable for the for_each_randdir() macro*/
tDirection _randdir[9]; /* all directions in a random order (indices 1..8) */

void randdir_permutate(void)
/* permutates the random directions */
{ unsigned char count = 3;
  while (count-- > 0)
  { tDirection dir, dir2, temp;
    for_each_dir(dir)
    { dir2 = randdir(); temp = _randdir[dir];
      _randdir[dir] = _randdir[dir2]; _randdir[dir2] = temp;
    }
  }
}

#if 0
static tDirection __randhorvertdir;
#endif
tDirection _randhorvertdir[5];

void randhorvertdir_permutate(void)
{ unsigned char count = 5; /* less elements than _randdir[], thus more loops */
  while (count-- > 0)
  { tDirection dir, dir2, temp;
    for_each_horvertdir(dir)
    { dir2 = randhorvertdir(); temp = _randhorvertdir[dir];
      _randhorvertdir[dir] = _randhorvertdir[dir2];
      _randhorvertdir[dir2] = temp;
    }
  }
}

tDeviationIndex __deviationindex;
  /* (global loop-variable for the __for_each_deviationdir() macro) */
tDirection _deviationdir[8];

static const struct
{ tDirection dir[4];
} deviate[8] = /* holds the second- and third-best deviations */
{ /*dirWest*/  { { dirNorthWest, dirSouthWest, dirNorth, dirSouth } },
  /*dirEast*/  { { dirNorthEast, dirSouthEast, dirNorth, dirSouth } },
  /*dirNorth*/ { { dirNorthWest, dirNorthEast, dirWest, dirEast } },
  /*dirSouth*/ { { dirSouthWest, dirSouthEast, dirWest, dirEast } },
  /*dirN.W.*/  { { dirNorth, dirWest, dirNorthEast, dirSouthWest } },
  /*dirN.E.*/  { { dirNorth, dirEast, dirNorthWest, dirSouthEast } },
  /*dirS.W.*/  { { dirSouth, dirWest, dirNorthWest, dirSouthEast } },
  /*dirS.E.*/  { { dirSouth, dirEast, dirNorthEast, dirSouthWest } }
};

#define COINTOSS (rb & (bitmask <<= 1))
#define SET(di2) _deviationdir[retval++] = deviate[di1].dir[di2]

tDeviationIndex deviationdir_prepare(tDirection maindir, tBoolean all)
/* "prepares" the _deviationdir[] array, i.e. fills it with deviation
   directions in some random order (and random amount - unless <all>!); to
   understand why it works as it is, note that for_each_deviationdir() goes
   through the _deviationdir[] array _backwards_ - by _decrementing_ the
   counter... */
{ tDeviationIndex retval = 0;
  const unsigned char di1 = maindir - 1; /* deviate[] index */
  tRandomByte rb = randbyte();   /* used for several...      */
  tRandomByte bitmask = 1;       /* ...fifty-fifty decisions */
#if CONFIG_EXDEBUG
  tDeviationIndex dbg;
  char* dbgtemp = debugstrbuf + 3;
#endif

  /* check if we should add _any_ deviation directions */
  if ( (all) || (COINTOSS) || (COINTOSS) || (COINTOSS) ) /* 87.5% */
  { /* check if we should add the third-best deviation directions */
    if ( (all) || (COINTOSS) ) /* 50% */
    { if (COINTOSS) { SET(2); SET(3); }
      else { SET(3); SET(2); }
    }
    /* add the second-best deviation directions */
    if (COINTOSS) { SET(0); SET(1); }
    else { SET(1); SET(0); }
  }

  /* add the "main" direction */
  _deviationdir[retval] = maindir;
  return(retval);
}

#undef SET
#undef COINTOSS

tBoolean key2dir(const tKey key, tDirection* dir, tMoveKind* _movekind)
/* converts a keycode into a direction and move kind; <_movekind> may be NULL;
   returns whether it could actually do the conversion */
{ static const char str[4 * 8] =
    "hlkjyubnHLKJYUBN\010\014\013\012\031\025\002\01646827913";
  static const tKey ck[8] = { KEY_WEST, KEY_EAST, KEY_NORTH, KEY_SOUTH,
    KEY_NORTHWEST, KEY_NORTHEAST, KEY_SOUTHWEST, KEY_SOUTHEAST };
    /* cursor keys */
  unsigned char i;
  const char* match;

  /* Check for cursor keys; this _must_ come first because apparently some
     cursor key codes can interfere with the control-letter key codes. (?) */
  for (i = 0; i < 8; i++)
  { if (key == ck[i])
    { *dir = i + 1;
      if (_movekind != NULL) *_movekind = mkSingleStep;
      return(truE);
    }
  }

  /* Check for certain lowercase, uppercase & control letters and digits */
  match = my_strchr(str, (char) key);
  if (match != NULL)
  { int offset = match - str;
    *dir = (offset % 8) + 1;
    if (_movekind != NULL)
    { *_movekind = offset / 8;
      if (*_movekind == 3) *_movekind = mkSingleStep;
    }
    return(truE);
  }

  /* No match at all */
  return(falsE);
}


/* Other functions */

static chtype* old_screen;
static short old_cursorx, old_cursory;

void save_screen(void)
/* stores the screen contents in a memory buffer and erases the screen */
{ short w = screen_width, h = screen_height, x, y;
  chtype* temp;
  old_screen = temp = __memory_allocate(w * h * sizeof(chtype));
  old_cursorx = curscr->_curx; old_cursory = curscr->_cury;
  for (y = 0; y < h; y++)
  { for (x = w - 2; x >= 0; x--)
    { temp[x] = mvinch(y, x); } /* IMPROVEME! */
    (void) clrtoeol(); /* (works because x counted _downwards_!) */
    temp += w;
  }
}

void restore_screen(void)
/* restores formerly saved screen contents and deallocates the buffer; never
   call this without a prior call to save_screen() - or we segfault... */
{ short w = screen_width, h = screen_height, x, y;
  const chtype* temp = old_screen;
  for (y = 0; y < h; y++)
  { for (x = 0; x < w - 1; x++)
    { mvaddch(y, x, temp[x]); } /* IMPROVEME - use mvaddnstr()? */
    temp += w;
  }
  (void) move(old_cursory, old_cursorx);
  memory_deallocate(old_screen);
}

void show_extended_version_information(void)
{ short y;
  tKey k;
  save_screen();
  mvaddstr(0, 0, strVersion);
  mvaddstr(1, 0, "Extended version/software information");
  mvaddstr(3, 0,"atrogue is a \"Rogue-like game\" created by Arne Thomassen.");
  mvaddstr(4, 0, "It is basically released under certain versions of the");
  mvaddstr(5, 0, "GNU General Public License and");
  mvaddstr(7, 5, "WITHOUT ANY WARRANTY.");
  mvaddstr(9, 0, "Please read the file COPYING for license details, README");
  mvaddstr(10, 0, "for game information (in the source code directory).");
  mvaddstr(12, 0, "The project home page is <atrogue.sourceforge.net>.");

#ifdef NCURSES_VERSION

/* work-around for ncurses silliness... */
#if (defined(NCURSES_VERSION_MAJOR)) && (NCURSES_VERSION_MAJOR >= 5)
#define my_curses_version curses_version()
#else
#define my_curses_version NCURSES_VERSION
#endif

  mvaddstr(14, 0, "You're currently using ncurses library version");
  sprintf(strbuf, "\"%s\".", my_curses_version);
  mvaddstr(15, 0, strbuf);
  y = 17;

#else
  y = 14;
#endif
  mvaddstr(y, 0, "Press <Space> to continue");
  (void) doupdate();
  do { k = my_getch(); } while (k != ' ');
  restore_screen();
}

tColorCode hallu_color(const tColorCode deflt)
{ tColorCode retval;
  const tFewTicks h = rogue->hallucinating;
  if ( (h > 0) && (randpercent(5 * rogue->hallucinating)) )
    retval = randbytemod(maxColorCode + 1);
  else retval = deflt;
  return(retval);
}

#if OPTION_COLORS
tColorBitmask hallu_cb(const tColorBitmask deflt)
{ tColorBitmask retval;
  if (rogue->hallucinating)
  { static const tColorBitmask* const cb[] =
    { &cbRed, &cbGreen, &cbBlue, &cbYellow };
    retval = *(cb[randbytemod(ARRAY_ELEMNUM(cb))]);
  }
  else retval = deflt;
  return(retval);
}
#endif

const char* get_homepath(void)
/* tries to find out the user's home directory */
{ static const char* homepath = NULL;
  if (homepath == NULL) /* not yet calculated */
  { const char* env = getenv("HOME"); /* user's home directory */
    size_t envlen;
    if ( (env == NULL) || (*env == '\0') || ( (envlen = strlen(env)) > 50 ) )
    { /* (The length test is there in order to avoid message-line overflows and
          memory wastage...) */
      homepath = strEmpty; /* files go into the "current" directory */
    }
    else
    { const int no_trailslash = cond2bool(env[envlen - 1] != '/');
      char ch, *dest;
      homepath = dest = __memory_allocate(envlen + no_trailslash + 1);
      while ( (ch = *env++) != '\0' ) *dest++ = ch;
      if (no_trailslash) *dest++ = '/';
      *dest = '\0';
    }
  }
  return(homepath);
}

#if !HAVE_STRCHR
char* nonbroken_strchr(const char* s, int _wanted_ch)
{ const char wanted_ch = (char) _wanted_ch;
  char ch;
  while ( (ch = *s) != '\0' )
  { if (ch == wanted_ch) return(unconstify(s));
    s++;
  }
  return(NULL);
}
#endif

one_caller void __init stuff_initialize(void)
/* initializes random-direction arrays - KISS */
{ tDirection dir;
  _randdir[0] = _randhorvertdir[0] = dirNone; /* (never used) */
  for_each_dir(dir) _randdir[dir] = dir;
  for_each_horvertdir(dir) _randhorvertdir[dir] = dir;
}
