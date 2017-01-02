/* atrogue/animation.h - animation of events and remote actions
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_animation_h__
#define __atrogue_animation_h__

#if OPTION_ANIMATION

#include "stuff.h"

my_enum1 enum
{ acUnknown = 0, acThrow = 1, acHarpoon = 2, acBoomerang = 3, acZap = 4,
  acConfuse = 5, acDivide = 6, acDrown = 7, acSink = 8
} my_enum2(unsigned char) tAnimationClass;

#if 0
#define ANIMATION_IS_MAGIC(ac) ( (ac == acHarpoon) || (ac == acZap) || \
  (ac == acConfuse) || (ac == acDivide) )
#endif

my_enum1 enum
{ anfNone = 0, anfEnded = 0x01
} my_enum2(unsigned char) tAnimationFlags;

extern tBoolean do_animation;

typedef signed char tAnimationStepcount;
#define MAX_ANIMSTEPCOUNT (50)

typedef /*@abstract@*/ /*@immutable@*/ struct
{ tCoordinate startx, starty; /* section coordinates */
  tAnimationClass ac;
  tAnimationStepcount stepcount;
  tAnimationFlags flags;
  char ch;
} tAnimationData;

#define ANIMATION_DECLARE(var) tAnimationData* var = NULL;
#define ANIMATION_ALLOCATE(var, ac, x, y, ch) \
  var = animation_start(ac, x, y, ch)
#define ANIMATION_DEALLOCATE(var) animation_deallocate(var)

/* Functions */

extern /*@null@*/ tAnimationData* animation_start(const tAnimationClass,
  const tCoordinate, const tCoordinate, const char);
extern void animation_step(/*@null@*/ tAnimationData*, const tCoordinate,
  const tCoordinate);
extern void animation_crash_in_between(/*@null@*/ tAnimationData*,
  const tBoolean);
extern void animation_crash(/*@null@*/ tAnimationData*, const tBoolean);
extern void animation_step_and_crash(/*@null@*/ tAnimationData*,
  const tCoordinate, const tCoordinate, const tBoolean);
extern void animation_deallocate(/*@null@*/ /*@only@*/ tAnimationData*);

extern void animation_change_step_duration(const tBoolean);

#else /* #if OPTION_ANIMATION */

#define ANIMATION_DECLARE(var)
#define ANIMATION_ALLOCATE(a, b, c, d, e) do { } while (0)
#define animation_step(a, b, c) do { } while (0)
#define animation_crash_in_between(a, b) do { } while (0)
#define animation_crash(a, b) do { } while (0)
#define animation_step_and_crash(a, b, c, d) do { } while (0)
#define animation_end(a) do { } while (0)
#define ANIMATION_DEALLOCATE(a) do { } while (0)

#endif /* #if OPTION_ANIMATION */

#endif /* #ifndef __atrogue_animation_h__ */
