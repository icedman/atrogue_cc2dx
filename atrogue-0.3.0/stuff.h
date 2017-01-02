/* atrogue/stuff.h - general stuff
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_stuff_h__
#define __atrogue_stuff_h__

#include ".config"

#include <stdio.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#else
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#endif

#if HAVE_STRING_H
#if (!STDC_HEADERS) && HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>
#endif

#if HAVE_STRINGS_H
#include <strings.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

#if CONFIG_GRAPHICS

#include <gtk/gtk.h>

typedef GtkWidget* tGraphicsWindowHandle;
extern tGraphicsWindowHandle gwhMain;

/* "curses compatibility layer" (wow:-) This noble notion is just a deception;
   this stuff currently is a horrible hack (and most of the attribute handling
   code is probably plain wrong), but much of it gets removed/changed as soon
   as the graphics work... */

#define ERR (-1)
#define COLS (80)
#define LINES (25)
typedef unsigned long chtype;

/* some key codes, found in <gdk/gdkkeysyms.h> */
#define KEY_ENTER     (GDK_Return)
#define KEY_BACKSPACE (GDK_BackSpace)
#define KEY_A1          0534            /* Upper left of keypad */
#define KEY_A3          0535            /* Upper right of keypad */
#define KEY_C1          0537            /* Lower left of keypad */
#define KEY_C3          0540            /* Lower right of keypad */
#define KEY_CANCEL    (GDK_Cancel)
#define KEY_DOWN      (GDK_Down)
#define KEY_UP        (GDK_Up)
#define KEY_LEFT      (GDK_Left)
#define KEY_RIGHT     (GDK_Right)

#define NCURSES_BITS(mask,shift) ((mask) << ((shift) + 8))
#define A_BOLD          NCURSES_BITS(1UL,13)

typedef struct
{ chtype ch[COLS * LINES];
  chtype _attr, _ccpa, _cba;
    /* ("ccpa" = "current color-pair attributes", "cba" = "current background
       attributes) */
  short _curx, _cury;
} tTextualWindowContents;
extern tTextualWindowContents* stdscr;
#define curscr stdscr
#define move(y, x) do { curscr->_curx = (x); curscr->_cury = (y); } while (0)
#define endwin() do { } while(0)
extern chtype mvinch(short, short);
extern int addch(chtype);
extern int mvaddch(short, short, chtype);
extern int clear(void);
extern int addstr(const char*);
#define mvaddstr(y, x, str) do { move((y), (x)); addstr(str); } while (0)
extern int clrtoeol(void);
extern int beep(void);
#define attron(attrs)  do { curscr->_attr |= (attrs); } while (0)
#define attroff(attrs) do { curscr->_attr &= ~(attrs); } while (0)
extern int refresh(void);
#define doupdate refresh
extern int getch(void);
#if OPTION_COLORS
extern int init_pair(short, short, short);
extern int COLOR_PAIR(short);
extern int color_set(short, void*);
extern void bkgdset(chtype);
#endif

#else /* #if CONFIG_GRAPHICS */

#if OPTION_NCURSES && HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#endif /* #if CONFIG_GRAPHICS */

#if OPTION_TEXTMODEMOUSE && (defined(NCURSES_MOUSE_VERSION))
#define CONFIG_DO_TEXTMODEMOUSE 1
#define TEXTMODEMOUSE_MASK (BUTTON1_CLICKED)
#else
#define CONFIG_DO_TEXTMODEMOUSE 0
#endif

/* Helper macros */

#define CONFIG_EXDEBUG 0

#ifndef NULL
#define NULL ((void*) 0)
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef ABS
#define ABS(a) ( ((a)<0) ? (-(a)) : (a) )
#endif

#undef SIGN
  /* (We definitely want to use our own version of this macro; it might have
      been pre-defined in a way which is inappropriate for us...) */
#define SIGN(a) ( ((a)>0) ? (+1) : ( ((a)==0) ? (0) : (-1) ) )
#define SIGN_CHANGED(old, new) ( SIGN(old) != SIGN(new) )
typedef signed char tSign;

/* "limited subtraction" */
#define LIMSUB(a, b) ( ((a)>(b)) ? ((a)-(b)) : (0) )

#ifdef S_SPLINT_S

/* make splint happy; __sunused and __sallocator are splint-specific variants
   of __cunused and __callocator; this rubbish is necessary because splint and
   gcc expect such annotations in different places within a declaration... */

#define __sunused /*@unused@*/
#define __dnr /*@noreturn@*/
#define __sallocator /*@only@*/
#define my_enum1 typedef
#define my_enum2(ctype)

#else

/* make _me_ happy :-) */

#define __sunused
#define __dnr
#define __sallocator
#define my_enum1
#define my_enum2(ctype) ; typedef ctype

#endif

#if USING_CONFIGURE
#define __my_inline inline
#elif defined(__GNUC__)
#define __my_inline inline
#else
#define __my_inline
#endif

#define my_inline __my_inline /* the same (for now) */

#ifdef __GNUC__
#define __cunused __attribute__((__unused__))
#else
#define __cunused
#endif

#define __init
#define __initdata
#define __callocator
#define does_not_return __dnr
#define const_after_init
#define one_caller my_inline

typedef unsigned char tUint8;
typedef signed char tSint8;
typedef unsigned short tUint16;
typedef signed short tSint16;
typedef unsigned int tUint32;
typedef signed int tSint32;

#define __unconstify(_t, _v) ( (_t) (_v) ) /* discard "const", avoid warning */
#define unconstify(str) __unconstify(char*, str)

#define ARRAY_ELEMNUM(arr) (sizeof(arr) / sizeof(arr[0]))

/* We need library function variants which are _guaranteed_ to be
   locale-independent, even if i18n is enabled. */
#define my_isdigit(ch) ( ((ch) >= '0') && ((ch) <= '9') )
#define my_islower(ch) ( ((ch) >= 'a') && ((ch) <= 'z') )
#define my_isupper(ch) ( ((ch) >= 'A') && ((ch) <= 'Z') )
#define my_isalpha(ch) (my_islower(ch) || my_isupper(ch))
#define my_isalnum(ch) (my_isalpha(ch) || my_isdigit(ch))
#define _my_tolower(ch) ((ch) + ('a' - 'A'))
#define my_tolower(ch) ( my_isupper(ch) ? _my_tolower(ch) : (ch) )
#define _my_toupper(ch) ((ch) - ('a' - 'A'))
#define my_toupper(ch) ( my_islower(ch) ? _my_toupper(ch) : (ch) )

#define is_power_of_two(num) ( ((num) != 0) && ( ((num) & ((num) - 1)) == 0 ) )

/* IMPLEMENTME: portability! */
#define my_create(path, flags, mode) open(path, (flags) | O_NOCTTY, mode)
#define my_open(path, flags) open(path, flags)
#define my_close(fd) (void) close(fd)
#define my_read(fd, buf, count) read(fd, buf, count)
#define my_write(fd, buf, count) write(fd, buf, count)
#define my_fstat(fd, buf) fstat(fd, buf)

/* Simple bit manipulation helpers; <address> is the start address of a
   bitfield, <bit> is the number of the desired bit within the field. */
#define __my_byte(address, bit) (*((unsigned char*) ((address) + ((bit) / 8))))
#define __my_bit(bit) (1 << ((bit) % 8))
#define my_bit_set(address, bit) __my_byte(address, bit) |= __my_bit(bit)
#define my_bit_clear(address, bit) __my_byte(address, bit) &= ~__my_bit(bit)
#define my_bit_flip(address, bit) __my_byte(address, bit) ^= __my_bit(bit)
#define my_bit_test(address, bit) (__my_byte(address, bit) & __my_bit(bit))

/* CHECKME: use a sizeof() test with configure to avoid (_bogus_!) compiler
   warnings on 64-bit machines! */
#define MY_INT_TO_POINTER(x) ( (void*) ((int) (x)) )
#define MY_POINTER_TO_INT(x) ((int) (x))

/* Suppress _bogus_ compiler warnings about uninitialized variables (in order
   to make all "real" warnings more visible): */
#if 1
#define SHUT_UP_COMPILER(value) = (value)
#else
#define SHUT_UP_COMPILER(value) /* nothing */
#endif

#define remove_from_list(_headref, _element, _type) \
  do \
  { _type* _head = *_headref; \
    if (_head == NULL) { /* nothing */ } \
    else if (_head == _element) *_headref = _element->next; \
    else \
    { _type* _x = _head; \
      while (_x->next != NULL) \
      { if (_x->next == _element) { _x->next = _element->next; break; } \
        _x = _x->next; \
      } \
    } \
  } while (0)

#if HAVE_MEMCPY
#define my_memcpy(dest, src, cnt) (void) memcpy(dest, src, cnt)
#elif HAVE_BCOPY
#define my_memcpy(dest, src, cnt) bcopy(src, dest, cnt)
#else
#define my_memcpy(dest, src, cnt) \
  do \
  { char* _d = (char*) (dest); \
    const char* _s = (const char*) (src); \
    size_t _c = (cnt); \
    while (_c-- > 0) *_d++ = *_s++; \
  } while (0)
#endif

#if HAVE_MEMSET
#define my_memset(start, value, length) memset(start, value, length)
#else
#define my_memset(start, value, length) \
  do \
  { char* s = (char*) (start); \
    const char v = (char) (value); \
    size_t count = (length); \
    while (count-- > 0) *s++ = v; \
  } while (0)
#endif

#if (!HAVE_MEMSET) && (HAVE_BZERO)
  /* Use bzero() because it might be much faster than our stupid byte-by-byte
     cleaning fallback: */
#define my_memclr(start, length) bzero(start, length)
#else
#define my_memclr(start, length) my_memset(start, 0, length)
#endif

#define my_memclr_var(var) my_memclr(&(var), sizeof(var))
#define my_memclr_arr(arr) my_memclr((arr), sizeof(arr))

#if HAVE_STRCHR
#define my_strchr(str, ch) strchr(str, ch)
#else
extern char* nonbroken_strchr(const char*, int);
#define my_strchr(str, ch) nonbroken_strchr(str, ch)
#endif

/* "More consistent" names for constants from ncurses.h */

#define KEY_WEST KEY_LEFT
#define KEY_EAST KEY_RIGHT
#define KEY_NORTH KEY_UP
#define KEY_SOUTH KEY_DOWN
#define KEY_NORTHWEST KEY_A1
#define KEY_NORTHEAST KEY_A3
#define KEY_SOUTHWEST KEY_C1
#define KEY_SOUTHEAST KEY_C3

/* Boolean */

/* It seems that "false", "False" and "FALSE" are already used by compilers and
   libraries; it can't be true... :-) Let's hope that our choice is safe. */
/*@-booltype tBoolean@*/ /*@-booltrue truE@*/ /*@-boolfalse falsE@*/
my_enum1 enum { falsE = 0, truE = 1 } my_enum2(unsigned char) tBoolean;
#define flip_boolean(var) \
  do { var = ( (var == falsE) ? (truE) : (falsE) ); } while (0)
#define boolean2bool(x) ( ((x) == falsE) ? (0) : (1) )
#define cond2boolean(condition) ( (condition) ? (truE) : (falsE) )
#define cond2bool(condition) ( (condition) ? (1) : (0) )
#define cond2sign(condition) ( (condition) ? (+1) : (-1) )

/* Characters; strings */

#define charNone ((char) 0)
#define charBug  ('$')
#define is_vowel(ch) ( ((ch) == 'a') || ((ch) == 'e') || ((ch) == 'i') || \
  ((ch) == 'o') || ((ch) == 'u') )

extern /*@observer@*/ const char* indefinite_article(char);

#define STRBUF_SIZE (1024)
  /* should be 1024; if you want to lower this in order to save memory: _must_
     be at least 2 * HARD_MAXSECWIDTH at all events, to be cautious... */
extern char strbuf[]; /* buffer for string operations */
extern const char strVersion[], strVersionShort[];
extern const char strNewline[], strSomething[], strSomethingHits[], strYes[],
  strNo[], strSpaceMisses[], strItPasses[], strIsogh[], strQuiteHot[],
  strThe[], strAre[], strIt[], strThey[], strWhoops[], strUnknown[],
  strSpace[], strA[], strIs[], strSpacedDash[], str2D[], strPerccPercd[];
#define strEmpty (strNewline + 1)
#define strS (strIs + 1)
#define strSpaceHits (strSomethingHits + 9)
#define strEs (strSpaceMisses + 5)
#define strPercd (strPerccPercd + 2)
#define _strVersionShort (strVersionShort + 8)
#if CONFIG_DEBUG
extern char debugstrbuf[STRBUF_SIZE]; /* buffer for debug strings */
#endif

extern void strangemsg(void);

/* Exit codes; quitting */

my_enum1 enum
{ ecOkay = 0,    /* user has quit (or similar, harmless, non-error stuff) */
  ecOOM = 1,     /* out of memory */
  ecRuntime = 2, /* other kinds of runtime errors */
  ecDebug = 3,   /* hit a debug check */
  ecArg = 4      /* bad command-line argument */
} my_enum2(unsigned char) tExitcode;
extern tBoolean is_errorsave_enabled, do_log;
#if !CONFIG_GRAPHICS
extern tBoolean need_endwin;
#endif
extern void does_not_return do_quit_msg(tExitcode, const char*);
extern void does_not_return do_quit(tExitcode);
#if CONFIG_DEBUG
extern void does_not_return do_quit_debugmsg(const char* msg);
#endif

/* Memory management */

extern __sallocator /*@notnull@*/ /*@out@*/ void* __callocator
  __memory_allocate(size_t);
extern __sallocator /*@notnull@*/ void* __callocator memory_allocate(size_t);
extern __sallocator /*@notnull@*/ void* memory_reallocate(/*@only@*/ void*,
  size_t);
extern void memory_deallocate(/*@only@*/ /*@notnull@*/ const void*);
extern __sallocator char* __callocator my_strdup(const char*);

/* Dungeon square coordinates */

typedef tSint16 tCoordinate; /* ("signed" only to simplify calculations)*/
typedef struct { tCoordinate x, y; } tLocation; /* just to clarify; not used */
#define coordNone ((tCoordinate) 10000)
  /* a "bad/invalid" coordinate (some high number) */
#define is_location_valid(i, j) ( ((i) >= 0) && ((i) < current_section->width)\
  && ((j) >= 0) && ((j) < current_section->height) )
extern tBoolean are_locations_in_line(tCoordinate, tCoordinate, tCoordinate,
  tCoordinate);

/* screen sizes */
#define HARD_MINSCRWIDTH   (10)
#define HARD_MAXSCRWIDTH  (200)
#define HARD_MINSCRHEIGHT  (10)
#define HARD_MAXSCRHEIGHT (100)

/* dungeon section sizes; if you change the following values, also change the
   sectionsizemeasure calculation! */
#define HARD_MINSECWIDTH   (60)
#define HARD_MAXSECWIDTH  (120)
#define HARD_MINSECHEIGHT  (20)
#define HARD_MAXSECHEIGHT  (40)
extern tCoordinate screen_width, screen_height, max_section_width,
  max_section_height;

/* Directions */

my_enum1 enum
{ dirNone = 0, dirWest = 1, dirEast = 2, dirNorth = 3, dirSouth = 4,
  dirNorthWest = 5, dirNorthEast = 6, dirSouthWest = 7, dirSouthEast = 8
} my_enum2(unsigned char) tDirection;

#define for_each_dir(dir) for (dir = 1; dir <= 8; dir++)
#define for_each_horvertdir(dir) for (dir = 1; dir <= 4; dir++)
#define is_horvertdir(dir) ( ((dir) >= 1) && ((dir) <= 4) )
#define is_diagdir(dir) ( ((dir) >= 5) && ((dir) <= 8) )

extern const tDirection counterdir[9];
extern tDirection __randdir;
extern tDirection _randdir[9];
extern void randdir_permutate(void);

/* The following macro goes through _all_ directions, but in a _random_ order;
   note the absence of surrounding "{ }", so don't say something like
   "if (...) for_each_randdir(...) ..." without some extra "{ }"! */
#define for_each_randdir(dir) \
  randdir_permutate(); \
  for (__randdir = 1; (__randdir <= 8) && (dir = _randdir[__randdir]); \
       __randdir++)

/* The following stuff works like the above, but only for horizontal/vertical
   directions. */
extern tDirection _randhorvertdir[5];
extern void randhorvertdir_permutate(void);
#if 0
#define for_each_randhorvertdir(dir) \
  randhorvertdir_permutate(); \
  for (__randhorvertdir = 1; (__randhorvertdir <= 4) && \
    (dir = _randhorvertdir[__randhorvertdir]); __randhorvertdir++)
#endif

typedef signed char tDeviationIndex; /* (this _must_ be signed; otherwise the
  for_each_deviationdir() macro doesn't work) */
extern tDeviationIndex __deviationindex; /* index for _deviationdir[] */
extern tDirection _deviationdir[8]; /* only 8 directions exist */
extern tDeviationIndex deviationdir_prepare(tDirection, tBoolean);

/* The following macro goes through the main direction (probed at first) and
   its deviation directions (probed afterwards, in a random order/amount). */
#define __for_each_deviationdir(maindir, loopdir, flag) \
  for (__deviationindex = deviationdir_prepare(maindir, flag); \
       (__deviationindex >= 0) && (loopdir = _deviationdir[__deviationindex]);\
       __deviationindex--)
#define for_each_deviationdir(maindir, loopdir) \
  __for_each_deviationdir(maindir, loopdir, falsE)
#define for_each_deviationdir_full(maindir, loopdir) \
  __for_each_deviationdir(maindir, loopdir, truE)

/* stuff for splitting a direction into its horizontal and vertical parts and
   for re-assembling a direction from these parts */
typedef signed char tDirectionOffset;
extern const tDirectionOffset xdir[9], ydir[9];
extern const char* const strDirection[9];
extern tDirection diroffsets2dir(tDirectionOffset, tDirectionOffset);

/* Number-entering */

#define HIGHEST_ALLOWED_NUMBER (32000)
#define ENTERED_NUMBER_INVALID (HIGHEST_ALLOWED_NUMBER + 1)
typedef tSint16 tEnteredNumber;
  /* ("tSint16" in order to match tObjectQuantity and tFewTicks - the two
      contexts for which this stuff can be used -- dropping, throwing, moving,
      resting, sleeping) */
extern tEnteredNumber entered_number;

/* Ticks and tickouts */

typedef tUint32 tMilliticks;
typedef tSint32 tTicks;
typedef tSint16 tFewTicks; /* (just to save memory) */
  /* ("signed" was originally only to simplify calculations; but nowadays the
      creature->slowedhasted timer _may_be_negative_ - meaning that the
      creature is slowed) */
#define MAX_FEWTICKS (HIGHEST_ALLOWED_NUMBER)
#define mt2t(mt) ((mt) / 1000) /* milliticks to ticks */
#define t2mt(t) ((t) * 1000) /* ticks to milliticks */

my_enum1 enum
{ taTrigger = 0, taCleanup = 1
} my_enum2(unsigned char) tTickoutAction;

struct tTickout;
typedef void (*tTickoutHandler)(struct tTickout*, const tTickoutAction);

typedef struct tTickout
{ struct tTickout *next, *prev;
  tTickoutHandler handler;
  /*@null@*/ void* data; /* handler-specific data */
#if CONFIG_DEBUG
  const char* debuginfo;
#define tickout_set_debuginfo(t, str) do { (t)->debuginfo = (str); } while (0)
#else
#define tickout_set_debuginfo(t, str) do { } while (0)
#endif
  tMilliticks expiry;
  tBoolean keep;
} tTickout; /* a time-out in a turn-based game :-) */

extern tMilliticks _dungeonclock; /* our internal, dungeon-wide clock */
#define dungeonclock mt2t(_dungeonclock)
extern tTickout* active_tickouts;
extern tBoolean _do_tick; /* whether the player character "did something" */
extern void tickout_detach(tTickout*);
extern void tickout_schedule(tTickout*);

/* Aftermath handling */

typedef void (*tAftermathHandler)(void*);
extern void aftermath_schedule(tAftermathHandler, void*);

/* Colors */

#define maxColorCode (18) /* for color name strings */
#define colorcodebits (5)
#define colorcoderange (1 << colorcodebits)
#define colorcodemask (colorcoderange - 1)
extern const char* const strColor[maxColorCode + 1];
my_enum1 enum
{ /* The following color numbers must come first and in the given order - they
     must be "in sync" with the ncurses color numbers! */
  ccBlack = 0, ccRed = 1, ccGreen = 2, ccYellow = 3, ccBlue = 4, ccMagenta = 5,
  ccCyan = 6, ccWhite = 7,
  /* The following colors are ours; sorted alphabetically. */
  ccAquamarine = 8, ccBeige = 9, ccBrown = 10, ccBurgundy = 11, ccClear = 12,
  ccGray = 13, ccOrange = 14, ccPink = 15, ccPlaid = 16, ccPurple = 17,
  ccViolet = 18,
  ccNone = maxColorCode + 1
} my_enum2(unsigned char) tColorCode;
/* (Yes, we have some (harmless) namespace clash here: the prefix "cc" can mean
    "color code" as well as "creature class". Never mind.) */

#if OPTION_COLORS
extern tBoolean __use_color;
#define use_color (__use_color)
#endif

/* color-pair numbers */

my_enum1 enum
{ cpnHardwired = 0, cpnDefault = 1, cpnRed = 2, cpnGreen = 3, cpnBlue = 4,
  cpnYellow = 5, cpnCyan = 6,
  cpnMax = 6
} my_enum2(short) tColorPairNumber; /* data type must match init_pair() */

#if OPTION_COLORS
extern unsigned char color_highlight;
#define my_set_color(cpn) \
  do \
  { if (use_color) \
    { (void) attron(COLOR_PAIR(cpn)); \
      /* was "color_set(cpn, NULL);", but with ncurses, color_set() is */ \
      /* only defined in ncurses 5.0 or newer... */ \
      if (cpn == cpnDefault) (void) attroff(A_BOLD); \
      else if ( (color_highlight == 2) || \
           ( (color_highlight == 1) && (cpn == cpnYellow) ) ) \
        (void) attron(A_BOLD); \
      else (void) attroff(A_BOLD); \
    } \
  } while (0)
#else
#define my_set_color(cpn) do { /* nothing */ } while(0)
#endif

/* color bitmasks */

typedef chtype tColorBitmask;

#if OPTION_COLORS
extern tColorBitmask cbRed, cbGreen, cbBlue, cbYellow, cbCyan;
#else
enum { cbRed = 0, cbGreen = 0, cbBlue = 0, cbYellow = 0, cbCyan = 0 };
#endif
/* The color assignments might change some time, that's why they're used by
   meaning (creature, object, ...), not by color (red, green, ...): */
#define cbCreature cbRed
#define cbObject cbGreen
#define cbEnchanted cbBlue
#define cbWater cbBlue
#define cbSwamp cbYellow
#define cbBarrier cbBlue
#define cbPlatform cbGreen
#define cbObstacle cbYellow /* for "generic" obstacles */
#define cbSeconn 0 /* (for now) */
#define cbFire cbRed
#define cbIce cbBlue
#define cbMist cbCyan

/* Gameplay difficulty and other preferences */

my_enum1 enum
{ dkVeryEasy = 0, dkEasy = 1, dkNormal = 2, dkHard = 3, dkVeryHard = 4,
  dkCrazy = 5
} my_enum2(unsigned char) tDifficulty;
#define maxDifficulty (5)
/* NOTE: if you change these, go through the _whole_ program source code and
   make sure everything still works! */

extern const char* const strDifficulty[maxDifficulty + 1];

typedef tSint16 tCreatureInterval; /* interval, time between two actions;
  signed only to simplify calculations; 0..10000 milliticks, where 0 means
  "inactive"; the default value is 1000, which means that the creature can act
  once per dungeon tick */
typedef tSint16 tCreatureData; /* meaning is class-dependent; e.g. for how
  long a vampire can stand liUnlimited yet, a "disguising" object class/kind
  for the imitating xerocs or number of lives for hydras */

my_enum1 enum
{ roleCaveman = 0, roleKnight = 1, roleProguerammer = 2, roleQuantumMech = 3,
  roleRogue = 4, roleScoundrel = 5, roleThief = 6, roleTourist = 7,
  roleTrader = 8, roleWizard = 9,
  maxRole = 9
} my_enum2(unsigned char) tRole; /* role of the player character; sorted in
  alphabetical order */

my_enum1 enum
{ rdfNone = 0, rdfPromotionary = 0x01
} my_enum2(unsigned char) tRoledataFlags;

typedef struct
{ const char* name;
  tCreatureInterval interval; /* basic action interval */
  unsigned char relative_strength, /* 1..5 */
    invsub, /* roughly opposite of <relative_strength> */
    fightskill /* 0..7 */, magic /* 0..3 */, healing /* 0..3 */,
    luck /* 0..3 */, mental_power /* 0..3 */;
  char prefsym;
  tRoledataFlags flags;
} tRoledata;

extern const tRoledata _roledata[maxRole + 1]; /* cf. creature.c */
extern /* not const_after_init */ tRoledata roledata;

my_enum1 enum
{ cgkMale = 0, cgkFemale = 1, cgkNeuter = 2, cgkRandom = cgkNeuter
} my_enum2(unsigned char) tCreatureGenderKind;

my_enum1 enum
{ alignLawful = 0, alignNeutral = 1, alignChaotic = 2, alignRandom = 3
} my_enum2(unsigned char) tCreatureAlignment;

extern const_after_init tDifficulty difficulty;
extern tRole pref_role, startpref_role;
extern tCreatureGenderKind pref_gender, startpref_gender;
extern tCreatureAlignment pref_align, startpref_align;
extern const_after_init unsigned char pref_combat, pref_expl, pref_magic,
  pref_feat, pref_shops;
enum
{ defpref_role = roleRogue, defpref_gender = cgkMale,
  defpref_align = alignNeutral, defpref_combat = 4, defpref_expl = 2,
  defpref_magic = 3, defpref_feat = 5, defpref_shops = 6
};

/* Run-mode of the program */

#if CONFIG_DEMO || CONFIG_REMOTE
enum { rmNormal = 0, rmDemo = 1, rmRemote = 2 };
typedef unsigned char tRunmode;
extern tRunmode runmode;
#endif
#if CONFIG_DEMO
extern const char strI[];
#endif

/* General stuff */

typedef tUint16 tObjectClassesBitmask;
typedef tSint8 tStoreyNumber; /* roughly 1..50; signed for simplicity in some
  algorithms */
typedef tSint8 tStoreyPerc; /* 0..100; ditto */
typedef tUint8 tRoomIndex; /* for the rooms in a section */
typedef tUint8 tLightIntensity; /* basically brightness of a square */

typedef tSint16 tPercentage;
  /* (tSint16 in order to simplify calculations enormously; otherwise "tUint8"
     (0..100) would be sufficient of course) */

typedef tSint16 tDamage;

typedef tSint8 tMagicEnergy; /* roughly how many magic actions the player
  character can perform; 0..99; signed only to simplify calculations */
typedef tSint8 tMagicExp; /* experience with magic actions/objects; 0..99;
  signed only to simplify calculations */

extern tColorCode hallu_color(const tColorCode);
#define enchantmentcolorstr (strColor[hallu_color(ccBlue)])
#if OPTION_COLORS
extern tColorBitmask hallu_cb(const tColorBitmask);
#else
#define hallu_cb(deflt) (deflt)
#endif
extern /*@observer@*/ const char* get_homepath(void);

extern /*@observer@*/ const char* rogue_pronoun(const unsigned char);
extern /*@observer@*/ const char* rogue_verb(void);

my_enum1 enum
{ mkSingleStep = 0, mkUntilStopped = 1, mkUntilNextToSth = 2, mkCountdown = 3,
  mkGliding = 4
} my_enum2(unsigned char) tMoveKind;

my_enum1 enum
{ oeNone = 0, oeStop = 0x01, oeReflect = 0x02, oeRadiate = 0x04,
  oeRemove = 0x08, oeBurn = 0x10, oeMelt = 0x20, oeDamage = 0x40,
  oeCancel = 0x80
} my_enum2(tUint16) tObstacleEffects;
  /* nowadays not limited to obstacles anymore */

#if CONFIG_GRAPHICS
typedef guint tKey; /*struct _GdkEventKey->keyval,/usr/include/gdk/gdktypes.h*/
#else
typedef int tKey;
#endif

#define keyNone ((tKey) 0)
extern tKey my_getch(void);
extern tBoolean key2dir(const tKey, /*@out@*/ tDirection*,
  /*@null@*/ /*@out@*/ tMoveKind*);

#define zero2one(x) ( ((x) == 0) ? (1) : (x) )
#define null2empty(str) ( (str == NULL) ? (strEmpty) : (str) )

#if CONFIG_DEBUG
extern tUint16 __intsqrt(tUint16);
#endif
extern tUint16 intsqrt(const tUint16);

/* Screen-saving and -restoring */

extern void save_screen(void);
extern void restore_screen(void);

extern void show_extended_version_information(void);

/* Scrollable texts */

typedef struct tScrollableTextEntry
{ /*@null@*/ const char* line; /* NULL for empty lines */
  attr_t attr;
  tBoolean do_dealloc;
} tScrollableTextEntry;

struct tScrollableText;
typedef tBoolean (*tStKeyHandler)(struct tScrollableText*, tKey);
typedef tUint16 tStIdx; /* entry index */

my_enum1 enum
{ stfNone = 0, stfDisableLetterScrolling = 0x01
} my_enum2(unsigned char) tStFlags;

typedef struct tScrollableText
{ tScrollableTextEntry* entry;
  /*@null@*/ const char* notice;
  void* data; /* caller-specific data */
  /*@null@*/ tStKeyHandler key_handler;
  void* ptrvalue; /* caller-dependent */
  int value1, value2; /* caller-dependent */
  tKey key; /* result */
  tStIdx num_entries, maxnum_entries;
  unsigned char txtwidth; /* if > 0, show multi-column text if possible */
  tStFlags flags;
} tScrollableText;

extern void st_setup(/*@out@*/ tScrollableText*);
extern void st_append(tScrollableText*, /*@null@*/ const char*,const tBoolean);
#define st_append_nl(stptr) st_append(stptr, NULL, falsE)
#define last_st(stptr) (&((stptr)->entry[(stptr)->num_entries - 1]))
#define st_append_cb(stptr, line, dd, _cb) do { st_append(stptr, line, dd); last_st(stptr)->attr |= (_cb); } while (0)
#define st_append_reverse(stptr, line, dd) do { st_append(stptr, line, dd); last_st(stptr)->attr |= A_REVERSE; } while (0)
extern void st_show(tScrollableText*);
extern void st_cleanup(tScrollableText*);

/* Player character's inventory */

typedef signed char tInventoryIndex; /* index for roguedata.inventory[]; in
  some places, negative values indicate that objects have been put together */

#define iiInvalid ((tInventoryIndex) 120)
#define MAXNUM_INV ((tInventoryIndex) 26)
#define for_each_invindex(i) for (i = 0; i < MAXNUM_INV; i++)
#define is_valid_invkey(key) ( ((key) >= 'a') && ((key) <= 'z') )
#define invkey2index(key) ( (tInventoryIndex) ( ((char) (key)) - 'a' ) )
#define invidx2char(idx) ( ((char) (idx)) + 'a' )
#define invobj2index(object) ((object)->invidx)
#define invobj2char(object) invidx2char(invobj2index(object))

/* An object from the Rogue's inventory is "interesting" iff it's non-NULL and
   its class is one of the allowed classes: */
#define oc_in_classes(oc, classes) ((1 << (oc)) & classes)
#define is_invobject_interesting(object) \
  ( (object != NULL) && (oc_in_classes(object->oc, classes)) )

struct tObject;
extern /*@null@*/ struct tObject* inventory(const unsigned char,
  const tObjectClassesBitmask);

extern void stuff_initialize(void);

#endif /* #ifndef __atrogue_stuff_h__ */
