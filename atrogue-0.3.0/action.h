/* atrogue/action.h - creature actions ("artificial intelligence")
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_action_h__
#define __atrogue_action_h__

#include "stuff.h"

extern void action_creature(tCreature*);

#if CONFIG_DEMO
extern void action_rogue(void);
#endif /* #if CONFIG_DEMO */

#endif /* #ifndef __atrogue_action_h__ */
