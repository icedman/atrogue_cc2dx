/* atrogue/message.c - display messages, ask the user
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "message.h"
#include "object.h"
#include "dungeon.h"
#include "stat.h"

const char strKeylistYn[] = "yn", strKeylistLr[] = "lr";

static tBoolean may_need_keypress;
static tCoordinate messages_row, messages_col, messages_maxcol;
static tBoolean reset_position = truE;
static char msgbuf[2 * HARD_MAXSCRWIDTH];
int logfileid = -1;

#define maxHistory (8)
static char* history[maxHistory + 1];
typedef signed short tHistoryIndex;
static tHistoryIndex hiCurrent = -1, hiMaxvalid = -1;
static tBoolean showing_history = falsE;

static one_caller void message_reset(void)
{ messages_col = 0;
  may_need_keypress = falsE;
}

void message_clear(void)
{ (void) move(messages_row, 0); (void) clrtoeol();
  if (reset_position) cursor_reset_position();
  (void) doupdate(); message_reset();
}

#if !CONFIG_DEBUG
static
#endif
void message_log(const char* msg)
/* logs a message to a log file */
{ if ( (logfileid >= 0) /* && (msg[0] != '\0') */ )
  { (void) my_write(logfileid, msg, strlen(msg));
    (void) my_write(logfileid, strNewline, strlen(strNewline));
  }
}

void message_show(const char* msg)
/* shows a message in the message row */
{ const int newlen = messages_col + strlen(msg) + 8;
  tBoolean clrtoeol_done = falsE;
  message_log(msg); /* save the message into the log file */
  if (!showing_history) /* store the new message in the history array */
  { tHistoryIndex hi;
    if (hiMaxvalid == maxHistory) /* need to forget oldest message */
    { memory_deallocate(history[hiMaxvalid]); hiMaxvalid--; }
    if (hiMaxvalid > -1) /* shift old messages */
    { for (hi = hiMaxvalid; hi >= 0; hi--) history[hi + 1] = history[hi]; }
    history[0] = __memory_allocate(strlen(msg) + 1);
    strcpy(history[0], msg);
    msg = history[0]; hiMaxvalid++; hiCurrent = -1;
  }

  if (may_need_keypress && newlen > messages_maxcol) /* we must "block" */
  { if (rogue != NULL) /* only if there's already something to update... */
    { map_do_update_all(); statistics_update(); }
    mvaddstr(messages_row, messages_col, "-more-");
    (void) doupdate();
    while (1)
    { const tKey key = my_getch();
      /* IMPLEMENTME: let the player call save_screenshot() here! */
      if (key == ' ') break;
#if CONFIG_DEMO
      if ( (runmode == rmDemo) && (key == KEY_CANCEL) )
      { runmode = rmNormal; break; }
#endif
    }
    message_clear(); clrtoeol_done = truE;
  }
  else if (messages_col > 0) /* need a separator */
  { mvaddstr(messages_row, messages_col, strSpacedDash); messages_col += 3; }
  may_need_keypress = truE;
  mvaddstr(messages_row, messages_col, msg);
  if (!clrtoeol_done) (void) clrtoeol();
  messages_col += strlen(msg);
  if (reset_position) cursor_reset_position();
  (void) doupdate();
}

#if CONFIG_DEBUG
void message_showdebug(const char* msg)
/* (This wrapper exists just in order to make source code clearer...) */
{ message_show(msg);
}
#endif

void message_show_previous(void)
{ if (hiCurrent == hiMaxvalid) hiCurrent = 0;
  else hiCurrent++;
  showing_history = truE;
  if (history[hiCurrent])
  { sprintf(strbuf, "[%d] %s", hiCurrent + 1, history[hiCurrent]);
    message_show(strbuf);
  }
  else { message_show("(no previous message)"); hiCurrent = -1; }
  showing_history = falsE;
}

void message_die(void)
/* quits the game after the player character died */
{ reset_position = falsE; message_show("press Q to quit"); (void) doupdate();
  while (1)
  { const tKey key = my_getch();
    if (key == 'Q') break;
#if CONFIG_DO_TEXTMODEMOUSE
    else if (key == 'M')
      (void) mousemask(0, NULL); /* it might be useful to enable copy-paste */
#endif
    /* IMPLEMENTME: let the player call save_screenshot(), base_attributes()
       and object_dump_associations() here! */
  }
  do_quit(ecOkay);
}

tKey ask_for_key(const char* str, const char* keylist)
/* returns one of the desired keys - or KEY_CANCEL (if the player cancelled,
   which is _always_ allowed) */
{ tKey key;
  const char* match;
  sprintf(strbuf, "%s \"%s\" ", str, keylist);
  reset_position = falsE; message_show(strbuf);
  while (1)
  { key = my_getch();
    if (key == KEY_CANCEL) break; /* cancelling is _always_ allowed */
    if ( (match = my_strchr(keylist, key)) != NULL ) { key = *match; break; }
  }
  reset_position = truE; message_clear();
  return(key);
}

tBoolean ask_for_dir(/*@out@*/ tDirection* _dir,
  /*@null@*/ /*@out@*/ tMoveKind* _mk)
{ tBoolean retval;
  reset_position = falsE; message_show("direction?");
  while (1)
  { const tKey key = my_getch();
    if (key == KEY_CANCEL) { retval = falsE; break; }
    if (key2dir(key, _dir, _mk)) { retval = truE; break; }
  }
  reset_position = truE; message_clear();
  return(retval);
}

static void __curleft(void)
{ (void) move(stdscr->_cury, stdscr->_curx - 1);
}

static void __curright(void)
{ (void) move(stdscr->_cury, stdscr->_curx + 1);
}

static void __delchar(void)
{ __curleft();  (void) addch(' ');  __curleft(); (void) doupdate();
}

#define MAXLEN (20)
static char buf[MAXLEN + 1];

static /*@observer@*/ char* do_ask_for_string(const char* s,
  const unsigned char flags0)
/* "flags0 & 1": only digits allowed */
{ char* retval;
  unsigned short len = 0;
  const tBoolean only_digits_allowed = cond2boolean(flags0 & 1);
  reset_position = falsE; message_show(s);
  __curright(); /* keep cursor away from text */
  while (1)
  { tKey key = my_getch();
    if (key == KEY_CANCEL) { retval = NULL; break; } /* cancelled */
    if (key == KEY_ENTER) { buf[len] = '\0'; retval = buf; break; } /* done */
    if (key == KEY_BACKSPACE) { if (len > 0) { len--; __delchar(); } continue;}
    if (len >= MAXLEN) continue; /* buffer full */
    if (my_isdigit(key))
    { do_append: buf[len++] = (char) key; (void) addch(key); continue; }
    if (only_digits_allowed) continue;
    if ( (my_isalpha(key)) || (key == ' ') ) goto do_append;
  }
  reset_position = truE; message_clear();
  return(retval);
}

#undef MAXLEN

char* ask_for_string(const char* str)
{ return(do_ask_for_string(str, 0));
}

tEnteredNumber ask_for_number(const char* str)
{ unsigned long retval;
  signed short i;
  char ch;
  const char* s = do_ask_for_string(str, 1);
  if (!s) { invalid: return(ENTERED_NUMBER_INVALID); } /* player cancelled */

  retval = 0; i = 0;
  while ( (ch = s[i++]) != '\0' )
  { retval = 10 * retval + (ch - '0');
    if (retval > HIGHEST_ALLOWED_NUMBER)
      goto invalid;
  }
  if (retval < 1) goto invalid;
  return((tEnteredNumber) retval);
}

tObject* ask_for_invobj(const char* str, const tObjectClassesBitmask classes)
{ tObject* retval;
  if (classes != ocbAll)
  { /* not all inventory keys are allowed - show object class chars */
    const char *clch = object_classes2chars(classes),
      sepch = '"'; /* CHECKME: use "'" if amulet character '"' in string! */
    char ch, *ptr = msgbuf;
    while ( (ch = *str++) != '\0' ) *ptr++ = ch;
    *ptr++ = ' '; *ptr++ = sepch;
    while ( (ch = *clch++) != '\0' ) *ptr++ = ch;
    *ptr++ = sepch; *ptr++ = ' '; *ptr = '\0'; str = msgbuf;
      /* (trailing space to keep cursor away from text) */
  }
  reset_position = falsE; message_show(str);
  while (1)
  { const tKey key = my_getch();
    const char* match;
    unsigned char invflags;
    if (key == KEY_CANCEL) { retval = NULL; break; }
    if (key == '>') /* show inventory of all allowed object classes */
    { invflags = 1 | 2; try_inv:
      if ( (retval = inventory(invflags, classes)) != NULL ) break;
    }
    else if (key == '<') { invflags = 1 | 2 | 8; goto try_inv; }
    else if (is_valid_invkey(key))
    { /* user entered an inventory character; check whether proper: */
      retval = roguedata.inventory[invkey2index(key)];
      if ( (retval != NULL) && ( (1 << retval->oc) & classes ) ) break;
    }
    else if ( (match = my_strchr(strObjectClassChars, key)) != NULL )
    { const int offset = match - strObjectClassChars;
      if ( (1 << offset) & classes )
      { /* The program wants the user to press an inventory entry letter, but
           the user pressed a character which represents a corresponding object
           class in order to see an inventory of that class. */
        if ( (retval = inventory(1 | 2, 1 << offset)) != NULL ) break;
      }
    }
  }
  reset_position = truE; message_clear(); return(retval);
}

void __init message_initialize(void)
/* initializes the message handler */
{ messages_row = screen_height - 2; messages_maxcol = screen_width - 2;
  if (messages_maxcol < 79) messages_maxcol = 79;
  message_clear();
  if ( (do_log) && (logfileid < 0) )
  { do_log = falsE; message_show("Can't open/create/find log file"); }
}
