/* atrogue/random.h - random number generator
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_random_h__
#define __atrogue_random_h__

typedef tUint8 tRandomByte;

extern void random_initialize(void);
extern void random_entropy_from_key(const tKey);
extern tRandomByte randbyte(void);
extern tRandomByte randbytemod(const int);
#define randbytefromrange(min, max) ((min) + randbytemod((max) - (min) + 1))
extern tUint32 randlongmod(const int);
extern tDirection randdir(void);
extern tDirection randhorvertdir(void);
#define __randpercent(percentage) (cond2boolean(randbytemod(100)<(percentage)))
extern tBoolean randpercent(const tPercentage);
extern tBoolean randcointoss(void);
#define randcoord(modulus) ((tCoordinate) randbytemod(modulus))
#define randcoordfromrange(min, max) ((min) + randcoord((max) - (min) + 1))
#define randfewticks(mod) ((tFewTicks) randbytemod(mod))
  /* IMPORTANT: the caller must ensure "mod <= 256"! */
extern tFewTicks randfewticksfromrange(const tFewTicks, const tFewTicks);
extern tDamage randdamage(tDamage, const unsigned char);

#endif /* #ifndef __atrogue_random_h__ */
