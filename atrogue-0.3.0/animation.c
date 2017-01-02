/* atrogue/animation.c - animation of events and remote actions
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "animation.h"
#include "dungeon.h"
#include "message.h"

tBoolean do_animation;

typedef signed short tAnimationStepDuration; /* in milliseconds */
static tAnimationStepDuration animation_step_duration = OPTION_ANIMATION;

static const char dir2char[9] = "$--||\\//\\";

typedef struct
{ tCoordinate scrx, scry;
  chtype ch;
} tAnimUndoEntry;

static tAnimUndoEntry anim_undo[MAX_ANIMSTEPCOUNT + 1];

static void animation_sleep(void)
/* used for sleeping between animation steps */
{
#if HAVE_SELECT
  struct timeval duration;
  duration.tv_sec = animation_step_duration / 1000;
  duration.tv_usec = ((long) (animation_step_duration % 1000)) * 1000;
  (void) select(0, NULL, NULL, NULL, &duration);
#else
  /* IMPLEMENTME? */
#endif
}

static void animation_store_undo(const tAnimationStepcount idx,
  const tCoordinate scrx, const tCoordinate scry)
{ tAnimUndoEntry* const au = &(anim_undo[idx]);
  au->scrx = scrx; au->scry = scry; au->ch = mvinch(scry, scrx);
}

static void animation_undo(const tAnimationStepcount idx)
{ const tAnimUndoEntry* const au = &(anim_undo[idx]);
  mvaddch(au->scry, au->scrx, au->ch); cursor_reset_position();
}

static void animation_do_step(tAnimationData* data, const tCoordinate sectx,
  const tCoordinate secty)
{ tAnimationClass ac;
  tAnimationStepcount stepcount = data->stepcount;
  tBoolean do_refresh, do_sleep;
  tCoordinate scrx, scry;
  if (stepcount >= MAX_ANIMSTEPCOUNT) return; /* "should not happen" */
  if (!secloc2scrloc(sectx, secty, &scrx, &scry)) return; /* off-screen */
  do_refresh = do_sleep = falsE; ac = data->ac;
  if (ac == acThrow)
  { if (stepcount > 0) { animation_undo(0); data->stepcount = 0; }
    if (!map_can_player_see(sectx, secty, mfObjectSpecial)) goto refr;
    animation_store_undo(0, scrx, scry);
    (void) addch(dir2char[diroffsets2dir(SIGN(sectx - data->startx), SIGN(secty
      - data->starty))] | cbGreen);
    do_refresh = do_sleep = truE;
  }
  else if (ac == acZap)
  { if (!rogue_can_see) return;
    animation_store_undo(stepcount, scrx, scry);
    (void) addch(dir2char[diroffsets2dir(SIGN(sectx - data->startx), SIGN(secty
      - data->starty))] | hallu_cb(cbBlue));
    do_refresh = do_sleep = truE;
  }
  data->stepcount++;
  if (do_refresh) { refr: cursor_reset_position(); (void) refresh(); }
  if (do_sleep) animation_sleep();
}

/*@null@*/ tAnimationData* animation_start(const tAnimationClass ac,
  const tCoordinate sectx, const tCoordinate secty, const char ch)
{ tAnimationData* data;
  if ( (!do_animation) || (animation_step_duration <= 0) ) return(NULL);
  data = memory_allocate(sizeof(tAnimationData));
  data->ac = ac; data->startx = sectx; data->starty = secty;
  /* data->currx = data->curry = coordNone; */ data->ch = ch;
  return(data);
}

static void animation_end(/*@null@*/ tAnimationData* data)
{ tAnimationClass ac;
  if ( (data == NULL) || (data->flags & anfEnded) ) return;
  ac = data->ac;
  if (ac == acThrow) { if (data->stepcount > 0) animation_undo(0); }
  else if (ac == acZap)
  { tAnimationStepcount count = data->stepcount, idx;
    for (idx = 0; idx < count; idx++)
    { animation_undo(idx); (void) refresh(); animation_sleep(); }
  }
  data->flags |= anfEnded;
}

void animation_step(tAnimationData* data, const tCoordinate sectx,
  const tCoordinate secty)
{ if ( (data == NULL) || (data->flags & anfEnded) ) return;
  animation_do_step(data, sectx, secty);
  /* data->currx = sectx; data->curry = secty; */
}

static void animation_do_crash(__sunused tAnimationData* data __cunused,
  __sunused const tBoolean magic __cunused)
{ /* IMPLEMENTME! */
}

void animation_crash_in_between(tAnimationData* data, const tBoolean magic)
{ if ( (data == NULL) || (data->flags & anfEnded) ) return;
  animation_do_crash(data, magic);
}

void animation_crash(tAnimationData* data, const tBoolean magic)
{ if ( (data == NULL) || (data->flags & anfEnded) ) return;
  animation_do_crash(data, magic); animation_end(data);
}

void animation_step_and_crash(tAnimationData* data, const tCoordinate sectx,
  const tCoordinate secty, const tBoolean magic)
{ if ( (data == NULL) || (data->flags & anfEnded) ) return;
  animation_step(data, sectx, secty); animation_crash(data, magic);
}

void animation_deallocate(tAnimationData* data)
{ if (data == NULL) return;
  if (!(data->flags & anfEnded)) animation_end(data);
  memory_deallocate(data);
}

void animation_change_step_duration(const tBoolean increase)
{ animation_step_duration += 50 * cond2sign(increase);
  if (animation_step_duration < 0) animation_step_duration = 0;
  else if (animation_step_duration > 10000) animation_step_duration = 10000;

  if (animation_step_duration <= 0) message_show("animation off");
  else
  { sprintf(strbuf, "animation step duration: %d milliseconds",
      animation_step_duration);
    message_show(strbuf);
  }
}
