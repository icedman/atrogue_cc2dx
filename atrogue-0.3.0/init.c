/* atrogue/init.c - game initialization
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "init.h"
#include "dungeon.h"
#include "message.h"
#include "random.h"
#include "stat.h"
#include "animation.h"

static tCoordinate argscrw = 0, argscrh = 0, argsecw = 0, argsech = 0;

#if OPTION_COLORS
static tBoolean arg_allows_color = truE, terminal_supports_color = falsE;
unsigned char color_highlight = 0; /* 0 = none, 1 = yellow, 2 = any */
#define set_aac(x) arg_allows_color = (x)
#define set_color_highlight(x) color_highlight = (x)
#else
#define set_aac(x) do { } while (0)
#define set_color_highlight(x) do { } while (0)
#endif

#if OPTION_ANIMATION
static tBoolean arg_allows_animation = truE;
#define set_aaa(x) arg_allows_animation = (x)
#else
#define set_aaa(x) do { } while (0)
#endif

#if OPTION_COLORS

static tColorBitmask cpn2cb(const tColorPairNumber cpn)
{ tColorBitmask retval = COLOR_PAIR(cpn);
  if ((color_highlight == 2) || ((color_highlight == 1) && (cpn == cpnYellow)))
    retval |= A_BOLD;
  return(retval);
}

static one_caller void __init initialize_color(void)
/* initializes the color-pair numbers and color bitmasks; use the bitmasks like
   "char | color_bitmask" - unconditionally, i.e. without putting them into
   some "#if OPTION_COLORS" or "if (use_color)" blocks! */
{ if (use_color)
  { /* - NOTE 1: ncurses 5.2 documentation ("man color") says that color pair 0
         is hardwired to white-on-black and can't be changed...
       - NOTE 2: the first argument of init_pair() must be in the range
         0..COLOR_PAIRS-1! (Not 0..COLOR_PAIRS!) */
    (void) init_pair(cpnDefault, ccWhite, ccBlack);
    (void) init_pair(cpnRed, ccRed, ccBlack);
    (void) init_pair(cpnGreen, ccGreen, ccBlack);
    (void) init_pair(cpnBlue, ccBlue, ccBlack);
    (void) init_pair(cpnYellow, ccYellow, ccBlack);
    (void) init_pair(cpnCyan, ccCyan, ccBlack);
    cbRed = cpn2cb(cpnRed); cbGreen = cpn2cb(cpnGreen);
    cbBlue = cpn2cb(cpnBlue); cbYellow = cpn2cb(cpnYellow);
    cbCyan = cpn2cb(cpnCyan); bkgdset(COLOR_PAIR(cpnDefault));
    my_set_color(cpnDefault);
  }
}

#endif

#if (!OPTION_COLORS) || (!OPTION_ANIMATION) || (!CONFIG_DEMO) || (!CONFIG_REMOTE)

static void __init no_config(const char* s)
{ sprintf(strbuf, "%s not available in this version/configuration", s);
  do_quit_msg(ecArg, strbuf);
}

#endif

static const char strheight[] = "height";

static void start_logging(void)
{ static tBoolean did_start = falsE;
  if (did_start) return; /* try only once */
  did_start = truE;
  strcpy(strbuf, get_homepath()); strcat(strbuf, ".atrogue-log");
  logfileid = my_create(strbuf, O_CREAT | O_TRUNC | O_WRONLY,
    S_IRUSR | S_IWUSR);
#if CONFIG_DEBUG
  if (logfileid >= 0)
  { const time_t t = time(NULL);
    message_log("atrogue " ATROGUE_VERSION " log file (<http://atrogue.sourceforge.net/>)");
    sprintf(strbuf, "game started %s", ctime(&t));
    message_log(strbuf);
  }
#endif
  /* IMPLEMENTME: check whether the player is playing several games
     simultaneously with logging enabled (which he shouldn't!) - this would
     lead to "intermixed" log file entries... */
}

static tBoolean argcoord(const char* str, const char* what,
  const unsigned char whatlen, /*@out@*/ tCoordinate* _x)
{ tBoolean retval = cond2boolean(!strncmp(str, what, whatlen));
  if (retval)
  { unsigned int x;
    (void) sscanf(str + whatlen, "%u", &x);
    if ( (x < 10) || (x > 200) ) { retval = falsE; goto out; }
    *_x = x;
  }
  out: return(retval);
}

static one_caller void preset_pref(const char* str)
{ unsigned char count, *v;
  char ch;
  loop: switch (*str)
  { case 'd':
      ch = str[1];
      if ( (ch >= '0') && (ch <= '0' + maxDifficulty) ) difficulty = ch - '0';
      skipnum: do { ch = *++str; } while (my_isdigit(ch));
      break;
    case 'r':
      str++; if ( (ch = *str++) == '\0' ) return;
      for (count = 0; count <= maxRole; count++)
      { const tRoledata* const rd = &(_roledata[count]);
        if (rd->prefsym == ch)
        { if (!(rd->flags & rdfPromotionary)) pref_role = count;
          break;
        }
      }
      break;
    case 'c':
      v = &pref_combat; set09: ch = str[1];
      if (my_isdigit(ch)) *v = ch - '0';
      goto skipnum; /*@notreached@*/ break;
    case 'm': v = &pref_magic; goto set09; /*@notreached@*/ break;
    case 'f': v = &pref_feat; goto set09; /*@notreached@*/ break;
    case 'e': v = &pref_expl; goto set09; /*@notreached@*/ break;
    default: str++; break;
  }
  if (*str != '\0') goto loop;
}

static one_caller void __init process_commandline(int argc, const char **argv)
/* processes the command-line */
{ tBoolean ignoreunknownarg = falsE;

  while (--argc > 0)
  { const char* const str = *++argv;

    if ( (!strcmp(str, "--version")) || (!strcmp(str, "-v")) )
    {(void)puts(strVersion);
     (void)puts("atrogue is basically released under certain versions of the");
     (void)puts("GNU General Public License and WITHOUT ANY WARRANTY.");
     (void)puts("The project home page is <http://atrogue.sourceforge.net/>.");
     do_quit(ecOkay);
    }
    else if (!strcmp(str, "--colors=on")) set_aac(truE);
    else if (!strcmp(str, "--colors=off")) set_aac(falsE);
    else if (!strcmp(str, "--colorhl=none")) set_color_highlight(0);
    else if (!strcmp(str, "--colorhl=yellow")) set_color_highlight(1);
    else if (!strcmp(str, "--colorhl=any")) set_color_highlight(2);
    else if (!strncmp(str, "--pref=", 7)) preset_pref(str + 7);
    else if (!strcmp(str, "--animation=on")) set_aaa(truE);
    else if (!strcmp(str, "--animation=off")) set_aaa(falsE);
    else if (argcoord(str, "--scr-width=", 12, &argscrw)) { }
    else if (argcoord(str, "--scr-height=", 13, &argscrh)) { }
    else if (argcoord(str, "--sec-width=", 12, &argsecw)) { }
    else if (argcoord(str, "--sec-height=", 13, &argsech)) { }
    else if (!strcmp(str, "--log=on")) { do_log = truE; start_logging(); }
    else if (!strcmp(str, "--log=off")) do_log = falsE;

    else if (!strcmp(str, "--demo"))
#if CONFIG_DEMO
      runmode = rmDemo;
#else
      no_config("Demo play");
#endif

     else if (!strcmp(str, "--remote"))
#if CONFIG_REMOTE
      runmode = rmRemote;
#else
      no_config("Remote play");
#endif

#if CONFIG_DEBUG
    else if (!strcmp(str, "--sc")) /* storey/creature associations */
    { dungeon_initialize(1); creature_dump_sc(); do_quit(ecOkay); }
    else if (!strcmp(str, "--sqrt"))
    { /* test the intsqrt() algorithm; you should pipe the output to "less" */
      unsigned short i, __maxd = 0;
      double maxdeltaperc = 0.0;
      for (i = 1; i <= HIGHEST_ALLOWED_NUMBER; i++)
      { unsigned short s = intsqrt(i), __s = __intsqrt(i), __d = ABS(s - __s),
          delta = ABS(i - (s * s));
        double deltaperc = ((double)delta) * 100.0;
        deltaperc /= ((double) i);
        if (maxdeltaperc < deltaperc) maxdeltaperc = deltaperc;
        if (__maxd < __d) __maxd = __d;
        sprintf(debugstrbuf, "sqrt(%d) = %d[%d]; deltaperc = %e", i, s, __s,
          deltaperc);
        (void) puts(debugstrbuf);
      }
      sprintf(debugstrbuf, "maxdeltaperc: %e; __maxd: %d",maxdeltaperc,__maxd);
      (void) puts(debugstrbuf); do_quit(ecOkay);
    }
#endif

    else if (!strcmp(str, "--ignoreunknownarg")) ignoreunknownarg = truE;
    else if (!ignoreunknownarg)
    { snprintf(strbuf, STRBUF_SIZE / 2, "Bad command-line argument: %s", str);
      strbuf[STRBUF_SIZE / 2 - 1] = '\0';
      do_quit_msg(ecArg, strbuf);
    }
  }
}

static void __init init_finish(void)
/* finishes the initialization */
{
#if OPTION_COLORS
  initialize_color();
#endif
#if CONFIG_DO_TEXTMODEMOUSE
  (void) mousemask(TEXTMODEMOUSE_MASK, NULL);
#endif
  message_initialize(); random_initialize(); stuff_initialize();
  statistics_initialize(); object_initialize(); creature_initialize();
  dungeon_initialize(0); statistics_update(); message_show(strVersionShort);
  message_show("press \"V\" for information"); is_errorsave_enabled = truE;
}

#if (OPTION_COLORS) || (OPTION_ANIMATION)
static const char strDisabledByArg[] __initdata =
  "no (disabled on command-line)";
static const char* const strPrefs[2] __initdata =
  { "no (disabled by user)", strYes };
#endif

#if (!OPTION_COLORS) || (!OPTION_ANIMATION)
static const char strNoConf[] __initdata =
  "no (not available in this configuration/version)";
#endif

typedef struct
{ const char* txt;
  unsigned char y, txtx, valx, flags;
} tPref;
  /* "flags & 1": put cursor at value; "&2": skip input focus (because value
     not editable anyway) */

static tPref pref[] =
{ { "Play", 4, 3, 9, 0 },
  { "Difficulty", 5, 3, 15, 0 },
  { "Role", 6, 3, 9, 0 },
  { "Combat", 7, 3, 11, 1 },
  { "Magic", 7, 14, 21, 1 },
  { "Features", 7, 24, 34, 1 },
  { "Exploration", 7, 37, 50, 1 },
  { "Colors", 8, 3, 11, 0 },
  { "Animation", 9, 3, 14, 0 },
  { "Screen width", 10, 3, 17, 1 },
  { strheight, 10, 22, 30, 1 },
  { "Section width", 11, 3, 18, 1 },
  { strheight, 11, 23, 31, 1 },
  { "Quit", 12, 3, 0, 0 }
};

my_enum1 enum
{ prefnum = ARRAY_ELEMNUM(pref), piPlay = 0, piDiff = 1, piRole = 2,
  piCombat = 3, piMagic = 4, piFeat = 5, piExpl = 6, piColors = 7, piAnim = 8,
  piScrw = 9, piScrh = 10, piSecw = 11, piSech = 12, piQuit = 13
} my_enum2(unsigned char) tPrefIdx;

static void show_threedigit(const tPrefIdx idx, const short value)
{ const tPref* const p = &(pref[idx]);
  sprintf(strbuf, "%-3d", value); mvaddstr(p->y, p->valx, strbuf);
}

static void __init show_prefval(const tPrefIdx idx)
{ const tPref* const p = &(pref[idx]);
  const char* s;
  unsigned char v1;
  move(p->y, p->valx);
  switch (idx)
  { case piPlay: addstr("normal user-interaction"); break;
      /* IMPLEMENTME: allow "demo" here when demo mode is implemented! */
    case piDiff: addstr(strDifficulty[difficulty]); (void) clrtoeol(); break;
    case piRole: addstr(_roledata[pref_role].name); (void) clrtoeol(); break;
    case piCombat: v1 = pref_combat; show_v1: addch('0' + v1); break;
    case piMagic: v1 = pref_magic; goto show_v1; /*@notreached@*/ break;
    case piFeat: v1 = pref_feat; goto show_v1; /*@notreached@*/ break;
    case piExpl: v1 = pref_expl; goto show_v1; /*@notreached@*/ break;
    case piColors:
#if OPTION_COLORS
      if (!arg_allows_color) s = strDisabledByArg;
      else if (!terminal_supports_color)
        s = "no (not supported by terminal)";
      else s = strPrefs[boolean2bool(__use_color)];
#else
      s = strNoConf;
#endif
      addstr(s); (void) clrtoeol(); break;
    case piAnim:
#if OPTION_ANIMATION
      if (!arg_allows_animation) s = strDisabledByArg;
      else s = strPrefs[boolean2bool(do_animation)];
#else
      s = strNoConf;
#endif
      addstr(s); (void) clrtoeol(); break;
    case piScrw: show_threedigit(idx, screen_width); break;
    case piScrh: show_threedigit(idx, screen_height); break;
    case piSecw: show_threedigit(idx, max_section_width); break;
    case piSech: show_threedigit(idx, max_section_height); break;
  }
}

static const char strArrow[] = "->", strNoArrow[] __initdata = "  ";
static tPrefIdx prefidx = 0; /* currently selected entry */
static short restrow;

static void setprefcursor(void)
{ const tPref* const p = &(pref[prefidx]);
  if (p->flags & 1) move(p->y, p->valx);
  else move(restrow, 0);
}

static void edit_coord(/*inout*/ tCoordinate* _v, const tCoordinate minval,
  const tCoordinate maxval, const signed char diff)
{ tCoordinate v = *_v + diff;
  if (v < minval) v = maxval;
  else if (v > maxval) v = minval;
  *_v = v;
}

#define edit_scrw(diff) edit_coord(&screen_width, HARD_MINSCRWIDTH, HARD_MAXSCRWIDTH, diff)
#define edit_scrh(diff) edit_coord(&screen_height, HARD_MINSCRHEIGHT, HARD_MAXSCRHEIGHT, diff)
#define edit_secw(diff) edit_coord(&max_section_width, HARD_MINSECWIDTH, HARD_MAXSECWIDTH, diff)
#define edit_sech(diff) edit_coord(&max_section_height, HARD_MINSECHEIGHT, HARD_MAXSECHEIGHT, diff)

static one_caller void __init handle_preferences(void)
{ const tPref* p;
  tPrefIdx idx;
  unsigned char pf, x; /* pref flags */
  unsigned char* v1;
  tKey key;
#if OPTION_ANIMATION
  do_animation = arg_allows_animation;
#endif

  mvaddstr(0, 0, strVersionShort); mvaddstr(2, 0, "Preferences");
  for (idx = 0; idx < prefnum; idx++)
  { p = &(pref[idx]); mvaddstr(p->y, p->txtx, p->txt);
    if (idx != piQuit) { addch(':'); show_prefval(idx); }
  }

  restrow = pref[prefnum - 1].y + 2;
  mvaddstr(restrow, 0, "To navigate, press Cursor-Up/Down or k/j.\nTo edit, press Cursor-Left/Right or h/l.\nTo start, navigate to the \"Play\" line and press Return.\nFor information about atrogue, press V.");
  restrow += 4;

#if OPTION_COLORS
  if ( (!arg_allows_color) || (!terminal_supports_color) )
#endif
  { pref[piColors].flags |= 2; }
#if OPTION_ANIMATION
  if (!arg_allows_animation)
#endif
  { pref[piAnim].flags |= 2; }

  mvaddstr(pref[prefidx].y, 0, strArrow);
  prefkey: setprefcursor(); (void) doupdate(); key = my_getch();
  if ( (key == KEY_UP) || (key == 'k') )
  { unsigned char old_row, new_row;
    idx = prefidx; uploop:
    if (idx > 0) idx--;
    else idx = prefnum - 1;
    if ( (pf = pref[idx].flags) & 2) goto uploop; /* skip this entry */
    handle_row: old_row = pref[prefidx].y; new_row = pref[idx].y;
    if (old_row != new_row)
    { mvaddstr(old_row, 0, strNoArrow); mvaddstr(new_row, 0, strArrow); }
    prefidx = idx; setprefcursor();
  }
  else if ( (key == KEY_DOWN) || (key == 'j') )
  { idx = prefidx; downloop:
    if (idx < prefnum - 1) idx++;
    else idx = 0;
    if ( (pf = pref[idx].flags) & 2 ) goto downloop; /* skip this entry */
    goto handle_row;
  }
  else if ( (key == KEY_LEFT) || (key == 'h') )
  { switch (prefidx)
    { case piDiff:
        if (difficulty == 0) difficulty = maxDifficulty;
        else difficulty--;
        updval: show_prefval(prefidx); setprefcursor(); break;
      case piRole:
        do
        { if (pref_role == 0) pref_role = maxRole;
          else pref_role--;
        } while (_roledata[pref_role].flags & rdfPromotionary);
        goto updval; /*@notreached@*/ break;
      case piCombat: v1 = &(pref_combat); dec_v1:
        if (*v1 <= 0) *v1 = 9;
        else (*v1)--;
        goto updval; /*@notreached@*/ break;
      case piMagic: v1 = &(pref_magic); goto dec_v1; /*@notreached@*/ break;
      case piFeat: v1 = &(pref_feat); goto dec_v1; /*@notreached@*/ break;
      case piExpl: v1 = &(pref_expl); goto dec_v1; /*@notreached@*/ break;
#if OPTION_COLORS
      case piColors: flip_colors: flip_boolean(__use_color); goto updval;
        /*@notreached@*/ break;
#endif
#if OPTION_ANIMATION
      case piAnim: flip_animation: flip_boolean(do_animation);
        goto updval; /*@notreached@*/ break;
#endif
      case piScrw: edit_scrw(-1); goto updval; /*@notreached@*/ break;
      case piScrh: edit_scrh(-1); goto updval; /*@notreached@*/ break;
      case piSecw: edit_secw(-1); goto updval; /*@notreached@*/ break;
      case piSech: edit_sech(-1); goto updval; /*@notreached@*/ break;
    }
  }
  else if ( (key == KEY_RIGHT) || (key == 'l') )
  { switch (prefidx)
    { case piDiff:
        if (difficulty == maxDifficulty) difficulty = 0;
        else difficulty++;
        goto updval; /*@notreached@*/ break;
      case piRole:
        do
        { if (pref_role == maxRole) pref_role = 0;
          else pref_role++;
        } while (_roledata[pref_role].flags & rdfPromotionary);
        goto updval; /*@notreached@*/ break;
      case piCombat: v1 = &(pref_combat); inc_v1:
        if (*v1 >= 9) *v1 = 0;
        else (*v1)++;
        goto updval; /*@notreached@*/ break;
      case piMagic: v1 = &(pref_magic); goto inc_v1; /*@notreached@*/ break;
      case piFeat: v1 = &(pref_feat); goto inc_v1; /*@notreached@*/ break;
      case piExpl: v1 = &(pref_expl); goto inc_v1; /*@notreached@*/ break;
#if OPTION_COLORS
      case piColors: goto flip_colors; /*@notreached@*/ break;
#endif
#if OPTION_ANIMATION
      case piAnim: goto flip_animation; /*@notreached@*/ break;
#endif
      case piScrw: edit_scrw(+1); goto updval; /*@notreached@*/ break;
      case piScrh: edit_scrh(+1); goto updval; /*@notreached@*/ break;
      case piSecw: edit_secw(+1); goto updval; /*@notreached@*/ break;
      case piSech: edit_sech(+1); goto updval; /*@notreached@*/ break;
    }
  }
  else if (my_isdigit(key)) /* direct single-digit input */
  { switch (prefidx)
    { case piDiff:
        if ( (x = key-'0') <= maxDifficulty ) { difficulty = x; goto updval; }
        break;
      case piCombat: v1 = &(pref_combat); set_v1: (*v1) = (key - '0');
        goto updval; /*@notreached@*/ break;
      case piMagic: v1 = &(pref_magic); goto set_v1; /*@notreached@*/ break;
      case piFeat: v1 = &(pref_feat); goto set_v1; /*@notreached@*/ break;
      case piExpl: v1 = &(pref_expl); goto set_v1; /*@notreached@*/ break;
    }
  }
  else if (my_islower(key)) /* quick navigation */
  { const char ch = _my_toupper(key);
    for (idx = 0; idx < prefnum; idx++)
    { p = &(pref[idx]);
      if ( (p->txt[0] == ch) && (!(p->flags & 2)) ) goto handle_row;
    }
  }
  else if (key == KEY_ENTER)
  { switch (prefidx)
    { case piPlay: /* done with the preferences, start the game */
        pl: (void) clear(); init_finish(); return; /*@notreached@*/ break;
      case piQuit: quit: do_quit(ecOkay); /*@notreached@*/ break;
    }
  }
  else if (key == 'P') goto pl;
  else if (key == 'V') show_extended_version_information();
  else if (key == 'Q') goto quit;
  goto prefkey;
}

void __init initialize(int argc, const char** argv)
{ tSectionSizeMeasure width, height;
  if (do_log) start_logging();
  process_commandline(argc, argv);

  /* Initialize the curses library, calculate screen size */

  if (initscr() == NULL) do_quit_msg(ecRuntime, "can't initialize terminal");
    /* (Some old curses libraries, e.g. on Minix 2.0, might return NULL instead
        of exiting with an error message.) */
  need_endwin = truE;
#if OPTION_COLORS
  if (arg_allows_color)
  { terminal_supports_color =
      cond2boolean( (start_color() != ERR) && (has_colors() == TRUE) &&
                    (COLOR_PAIRS > cpnMax) );
  }
#endif
  (void) cbreak(); (void) noecho(); (void) nonl();
  (void) intrflush(stdscr, FALSE); (void) keypad(stdscr, TRUE); (void) clear();

  screen_width = (argscrw ? argscrw : COLS);
  if (screen_width < HARD_MINSCRWIDTH) screen_width = HARD_MINSCRWIDTH;
  else if (screen_width > HARD_MAXSCRWIDTH) screen_width = HARD_MAXSCRWIDTH;
  screen_height = (argscrh ? argscrh : LINES);
  if (screen_height < HARD_MINSCRHEIGHT) screen_height = HARD_MINSCRHEIGHT;
  else if (screen_height > HARD_MAXSCRHEIGHT) screen_height=HARD_MAXSCRHEIGHT;

  /* Calculate maximum section size */

  max_section_width = (argsecw ? argsecw : (screen_width - 1));
  if (max_section_width < HARD_MINSECWIDTH) max_section_width=HARD_MINSECWIDTH;
  else if (max_section_width > HARD_MAXSECWIDTH)
   max_section_width = HARD_MAXSECWIDTH;
  max_section_height = (argsech ? argsech : (screen_height - 2));
  if (max_section_height < HARD_MINSECHEIGHT)
    max_section_height = HARD_MINSECHEIGHT;
  else if (max_section_height > HARD_MAXSECHEIGHT)
   max_section_height = HARD_MAXSECHEIGHT;

  /* "basic size" of sections */
  width = max_section_width - HARD_MINSECWIDTH;
  height = max_section_height - HARD_MINSECHEIGHT;
  /* "magic correction" :-) */
  if ( (width == 0) && (height != 0) ) width = height / 4;
  else if ( (width != 0) && (height == 0) ) height = width / 12;
    /* (12 / 4 == 3 == HARD_MINSECWIDTH / HARD_MINSECHEIGHT ==
        HARD_MAXSECWIDTH / HARD_MAXSECHEIGHT) */
  /* and here we go... */
  sectionsizemeasure  =
      ( (100 * width) / (HARD_MAXSECWIDTH - HARD_MINSECWIDTH) )
    * ( (100 * height) / (HARD_MAXSECHEIGHT - HARD_MINSECHEIGHT) );

  /* Allow users to set game preferences (if appropriate). */

#if OPTION_COLORS
  __use_color = cond2boolean(arg_allows_color && terminal_supports_color);
#endif
#if CONFIG_REMOTE
  if (runmode == rmRemote)
  {
#if OPTION_COLORS
    __use_color = falsE; /* color could possibly confuse the remote program */
#endif
#if OPTION_ANIMATION
    do_animation = falsE; /* ditto */
#endif
  }
  else
#endif
  { handle_preferences(); }
  startpref_role = pref_role; startpref_gender = pref_gender;
  startpref_align = pref_align;
}
