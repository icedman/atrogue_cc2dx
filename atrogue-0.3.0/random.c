/* atrogue/random.c - random number generator
   This file is part of atrogue, a "Rogue-like game" created by Arne Thomassen;
   atrogue is basically released under certain versions of the GNU General
   Public License and WITHOUT ANY WARRANTY.
   Read the file COPYING for license details, README for game information.
   Copyright (C) 2001-2010 Arne Thomassen <arne@arne-thomassen.de>
*/

#include "stuff.h"
#include "random.h"
#include "message.h"
#include "dungeon.h"

static int randomfileid = -1;
#define RANDOMBUFFERSIZE (1024)
static char randombuffer[RANDOMBUFFERSIZE];
static tSint16 randombufferpos = -1;

static const char randfile[] __initdata = OPTION_RNG;

my_enum1 enum
{ rnsFile = 0, rnsBuiltin = 1
} my_enum2(unsigned char) tRandomNumberSource;
static tRandomNumberSource source;

/* The standard library function rand() gives poor results in some
   implementations, and better functions such as mrand48() aren't portable
   enough. After lengthy attempts to improve the results of rand() with
   additional entropy, I finally gave up and implemented a simple, (hopefully)
   proper PRNG... It shows very good results in tests which matter for this
   game, e.g. randcointoss() and randdir(). */

static tUint32 state[4], last;
#define LAST_STEP last = last * 1103515245 + 12345; /* an LCG; cf. SUSv4 */

static one_caller void my_srand(tUint32 seed)
{ tUint8 i;
  for (i = 0; i < 4; i++) { LAST_STEP state[i] = (seed&255)^last; seed >>= 8; }
}

void random_entropy_from_key(const tKey key)
/* Wikipedia article "Random_number_generation": "Another common entropy source
   is the behavior of human users of the system. While humans are not
   considered good randomness generators upon request, they generate random
   behavior quite well in the context of playing mixed strategy games." :-) */
{ if (source == rnsBuiltin)
  { const tUint8 idx = (key ^ state[key & 3]) & 3;
    state[idx] ^= (key >> 2);
  }
}

static tUint32 my_rand(void)
{ const tUint8 idx = state[last & 3] & 3;
  LAST_STEP state[idx] ^= last; state[idx ^ 1] += state[idx ^ 2] ^ (last<<11);
  return(last ^ state[idx ^ 3]);
}

static void use_builtin_rng(void)
{ static tBoolean did_seed = falsE;
  source = rnsBuiltin;
  if (!did_seed) /* try to calculate a "good" seed value */
  { tUint32 x = getpid() + getppid();
#if HAVE_GETTIMEOFDAY
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) x ^= tv.tv_sec ^ tv.tv_usec;
    else
#endif
    { x ^= time(NULL); }
    my_srand(x); did_seed = truE;
#if CONFIG_DEBUG
    sprintf(debugstrbuf, "Using builtin RNG (seed %u)", x);
    message_showdebug(debugstrbuf);
#endif
  }
}

void __init random_initialize(void)
/* initializes the random number generator */
{ if ( (randomfileid = my_open(randfile, O_RDONLY)) >= 0 )
  { /* CHECKME: do all these tests only if HAVE_SYS_STAT_H? (But the tests are
       too important to leave them out...) */
    struct stat statbuf;
    int result;
    unsigned char loopcount = 0;
    fstatloop: if ( (result = my_fstat(randomfileid, &statbuf)) != 0 )
    { if ( (result == -1) && (errno == EINTR) && (++loopcount < 100) )
        goto fstatloop; /* try again */
      close_fd: my_close(randomfileid); randomfileid = -1; goto use_builtin;
    }
    if (!S_ISCHR(statbuf.st_mode)) goto close_fd;
    source = rnsFile; goto out; /* usable, done */
  }
  use_builtin: use_builtin_rng();
  out: {}
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "RNG: source=%d, HAVE_GETTIMEOFDAY=%d", source,
    HAVE_GETTIMEOFDAY);
  message_log(debugstrbuf);
#endif
}

tRandomByte randbyte(void)
/* returns a single random byte */
{ tRandomByte retval;
  if (source == rnsFile) /* get random numbers from device file */
  { if (randombufferpos < 0) /* buffer exhausted, refill it */
    { int err;
      unsigned char loopcount = 0;
      loop: err = my_read(randomfileid, randombuffer, RANDOMBUFFERSIZE);
      if (err < RANDOMBUFFERSIZE / 16) /* (error code or too little) */
      { if ( (err == -1) && (errno == EINTR) && (++loopcount<100) ) goto loop;
        my_close(randomfileid); randomfileid = -1; use_builtin_rng();
        goto use_builtin;
      }
      randombufferpos = err - 1;
    }
    retval = randombuffer[randombufferpos--];
  }
  else /* get random numbers from builtin function */
  { static tUint32 rndval;
    use_builtin: if (randombufferpos < 0)
    { rndval = my_rand(); randombufferpos = sizeof(tUint32) - 1; }
    retval = rndval & 255; rndval >>= 8; randombufferpos--;
  }
  return(retval);
}

tRandomByte randbytemod(const int modulus)
{ /* (<modulus>: "< 0" would be bug, ">= 0" is valid) */
  if (modulus <= 1) return(0);
  else return(randbyte() % modulus); /* FIXME: don't use simple "%"! */
}

tUint32 randlongmod(const int modulus)
{ const tUint32 a = randbyte(), b = randbyte(), c = randbyte(), d = randbyte(),
    x = (a << 24) | (b << 16) | (c << 8) | (d /* << 0 */);
  return(x % modulus); /* FIXME: don't use simple "%"! */
}

tDirection randdir(void)
/* returns a random direction */
{ static tUint8 a = 0, b;
  return( (tDirection) ((( (++a & 1) ? (b = randbyte()) : (b>>3) ) & 7) + 1) );
}

tDirection randhorvertdir(void)
/* returns a random horizontal or vertical direction */
{ static tUint8 a = 0, b;
  tUint8 retval;
  if (a <= 0) { a = 4; b = randbyte(); }
  retval = b & 3; b >>= 2; a--;
  return( (tDirection) (retval + 1) );
}

tBoolean randpercent(const tPercentage percentage)
/* returns "truE" with a probability of <percentage> percent; we optimize for
   the cases "percentage <= 0" and ">= 100" - they occur often enough
   (especially for "extreme" difficulties/storeys and for things like
   "if (randpercent(rogue->confused))"), and so we need not call the PRNG that
   frequently. */
{ tBoolean retval;
  if (percentage <= 0) retval = falsE;
  else if (percentage >= 100) retval = truE;
  else retval = __randpercent(percentage);
  return(retval);
}

tBoolean randcointoss(void)
/* returns truE or falsE in a random, fifty-fifty manner */
{ static tUint8 count = 0, rb;
  tBoolean retval;
  if (count <= 0) { count = 8; rb = randbyte(); }
  retval = cond2boolean(rb & 1); rb >>= 1; count--;
  return(retval);
}

tFewTicks randfewticksfromrange(const tFewTicks min, const tFewTicks max)
/* returns a random number of (few) ticks; IMPORTANT: the caller must ensure
   that "max - min <= 255" holds true! */
{ /* REMOVEME? (Callers can always use randfewticks() instead!) */
#if CONFIG_DEBUG
  if (max < min) do_quit_debugmsg("randfewticksfromrange(): max < min");
  if (max - min > 255) do_quit_debugmsg("randfewticksfromrange(): bad range");
#endif
  return(min + randfewticks(max - min + 1));
}

tDamage randdamage(tDamage max, const unsigned char flags0)
/* calculates a random damage value; "extreme" values are less likely.
   "flags0 & 1": actor is player character; "&2": defender is player character;
   "&4": called from creature_hit() for a "normal" hit or similar; "&8": called
   from a "throwing context" */
{ tDamage retval;
  if (max > 255) max = 255; /* "can't happen" */
  if (flags0 & 4)
  { tDamage half, x, corr, spread;
    if (max <= 3) /* can't do much here */
    { retval = randbytemod(max) + 1; goto out; }
    half = max >> 1;
    if (flags0 & (1 | 2))
    { enum { dc = 15 }; /* "difficulty correlation" */
      tPercentage _p = (100 * (dc * difficulty + current_storeynum)) /
        (dc * maxDifficulty + dude.sn_hardmax), p = _p;
      if (p < 50) p = 50;
      else if (p > 150) p = 150;
      corr = (half * p) / 100;
      if (flags0 & 1) x = max - corr;
      else x = half + corr; /* "flags0 & 2" */
    }
    else x = half;
    spread = max >> 2;
    /* if (spread < 1) spread = 1; -- we have max >= 4, thus spread >= 1 here!
    else */ if (spread > 10) spread = 10;
    retval = x + randbytemod(spread + 1) - (spread >> 1);
    if (retval < 1) retval = 1;
    else if (retval > max) retval = max;
  }
  else if (flags0 & 8) retval = max - randbytemod(MIN(max, 3));
  else retval = max; /* "should not happen" */
  out:
#if CONFIG_DEBUG
  sprintf(debugstrbuf, "randdamage(%d, %d): %d", max, flags0, retval);
  message_log(debugstrbuf);
#endif
  return(retval);
}
