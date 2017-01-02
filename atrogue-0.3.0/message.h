/* atrogue/message.h - message handling
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_message_h__
#define __atrogue_message_h__

extern const char strKeylistYn[], strKeylistLr[];
extern int logfileid;

extern void message_clear(void);
extern void message_show(const char*);
extern void message_show_previous(void);
extern void does_not_return message_die(void);

#if CONFIG_DEBUG
extern void message_log(const char*);
extern void message_showdebug(const char*);
#endif

extern tKey ask_for_key(const char*, const char*);
#define ask_for_key_yn(str) ask_for_key(str, strKeylistYn)
#define ask_for_key_lr(str) ask_for_key(str, strKeylistLr)
extern tBoolean ask_for_dir(/*@out@*/ tDirection*,
  /*@null@*/ /*@out@*/ tMoveKind*);
extern /*@observer@*/ char* ask_for_string(const char*);
extern tEnteredNumber ask_for_number(const char*);
extern struct tObject* ask_for_invobj(const char*,const tObjectClassesBitmask);

extern void message_initialize(void);

#endif /* #ifndef __atrogue_message_h__ */
