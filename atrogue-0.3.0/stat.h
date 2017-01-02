/* atrogue/stat.h - game statistics
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#ifndef __atrogue_stat_h__
#define __atrogue_stat_h__

my_enum1 enum
{ svfNone = 0, svfLevel = 0x01, svfState = 0x02, svfHealth = 0x04,
  svfMaxHealth = 0x08, svfStrength = 0x10, svfMaxStrength = 0x20,
  svfExperience = 0x40, svfArmor = 0x80, svfMagicEnergy = 0x100,
  svfMagicExp = 0x200, svfTick = 0x400, svfOtherStuff = 0x800,
  svfCount = svfOtherStuff, svfBlind = svfOtherStuff,
  svfConfused = svfOtherStuff, svfFrozen = svfOtherStuff,
  svfInvisible = svfOtherStuff, svfLevitated = svfOtherStuff,
  svfNapping = svfOtherStuff, svfSlowedHasted = svfOtherStuff
} my_enum2(unsigned short) tStatisticsValidityFlags;
#define svfAll (svfLevel | svfState | svfHealth | svfMaxHealth | svfStrength \
  | svfMaxStrength | svfExperience | svfArmor | svfMagicEnergy | svfMagicExp \
  | svfTick | svfOtherStuff)

/* NOTE: we currently don't display hallucinating and held in the statistics
   line because these begin with the same letter as hasted and the player can
   easily observe these states by other means (messages, map display). */

extern void statistics_invalidate(tStatisticsValidityFlags);
extern void statistics_update(void);
extern void statistics_screenredraw(void);
extern void statistics_initialize(void);

#endif /* #ifndef __atrogue_stat_h__ */
