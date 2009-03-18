/*
 * This file is part of UProf.
 *
 * Copyright © 2008, 2009 Robert Bragg
 *
 * UProf is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * UProf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with UProf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _UPROF_H_
#define _UPROF_H_

#include <glib.h>

typedef struct _UProfContext UProfContext;

typedef struct _UProfCounter
{
  /** Application defined name for counter */
  const char *name;

  /** Application defined description for counter */
  const char *description;

  /** Application private data */
  unsigned long priv;

  /** private */
  const char *filename;
  long line;
  const char *function;

  long count;

  guint seen:1;
} UProfCounter;

typedef struct _UProfTimer UProfTimer;
struct _UProfTimer
{
  /** Application defined name for timer */
  const char *name;

  /** Application defined description for timer */
  const char *description;

  /** Application defined parent */
  const char *parent_name;

  /** Application private data */
  unsigned long priv;

  /** private */
  const char *filename;
  long line;
  const char *function;

  unsigned long count;
  unsigned long long start;
  unsigned long long total;

  unsigned long long fastest;
  unsigned long long slowest;

  /* note: not resolved until sorting @ report time */
  UProfTimer *parent;
  GList *children;

  guint seen:1;

};

/**
 * uprof_init:
 * FIXME
 */
void
uprof_init (int *argc, char ***argv);

/**
 * uprof_get_system_counter:
 * FIXME
 */
unsigned long long
uprof_get_system_counter (void);

/**
 * uprof_get_system_counter_hz:
 * FIXME
 */
unsigned long long
uprof_get_system_counter_hz (void);

/**
 * uprof_context_new:
 * @name: The top most name to categorize your profiling results
 *
 * This creates a new profiling context with a given name. After creating
 * a context you should declare your timers and counters, and once
 * you have accumulated data you can print the results using
 * uprof_get_report()
 */
UProfContext *
uprof_context_new (const char *name);

/**
 * uprof_context_ref:
 * @context: A UProf context
 *
 * Take a reference on a uprof context.
 */
void
uprof_context_ref (UProfContext *context);

/**
 * uprof_context_imref:
 * @context: A UProf context
 *
 * Release a reference on a uprof context. When the reference count reaches
 * zero the context is freed.
 */
void
uprof_context_unref (UProfContext *context);

/**
 * uprof_context_add_counter:
 * @context: A UProf context
 * FIXME
 */
void
uprof_context_add_counter (UProfContext *context, UProfCounter *counter);

/**
 * uprof_context_add_timer:
 * @context: A UProf context
 * FIXME
 */
void
uprof_context_add_timer (UProfContext *context, UProfTimer *timer);

/**
 * uprof_context_output_report:
 * @context: A UProf context
 *
 * Generates a report of the accumulated timer and counter data associated with
 * the given context.
 */
void
uprof_context_output_report (UProfContext *context);


/************
 * Counters
 ************/

/**
 * UPROF_DECLARE_COUNTER:
 * @COUNTER_SYMBOL: The name of the C symbol to declare
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can get to if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * This can be used to declare a new static counter structure
 */
#define UPROF_DECLARE_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV) \
  static UProfCounter COUNTER_SYMBOL = { \
    .name = NAME, \
    .description = DESCRIPTION, \
    .priv = (unsigned long)(PRIV), \
    .seen = 0 \
  }

#define INIT_UNSEEN_COUNTER(CONTEXT, COUNTER_SYMBOL) \
  do { \
    (COUNTER_SYMBOL).filename = __FILE__; \
    (COUNTER_SYMBOL).line = __LINE__; \
    (COUNTER_SYMBOL).function = __FUNCTION__; \
    (COUNTER_SYMBOL).seen = 1; \
    (COUNTER_SYMBOL).count = 0; \
    uprof_context_add_counter (CONTEXT, &(COUNTER_SYMBOL)); \
  } while (0)

#define UPROF_COUNTER_INC(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).seen) \
      INIT_UNSEEN_COUNTER (CONTEXT, COUNTER_SYMBOL); \
    (COUNTER_SYMBOL).count++; \
  } while (0)

#define UPROF_COUNTER_DEC(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).seen) \
      INIT_UNSEEN_COUNTER (CONTEXT, COUNTER_SYMBOL) \
    (COUNTER_SYMBOL).count--; \
  } while (0)

#define UPROF_COUNTER_ZERO(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).seen) \
      INIT_UNSEEN_COUNTER (CONTEXT, COUNTER_SYMBOL) \
    (COUNTER_SYMBOL).count = 0; \
  } while (0)



/************
 * Timers
 ************/

/**
 * UPROF_DECLARE_TIMER:
 * @TIMER_SYMBOL: The name of the C symbol to declare
 * @PARENT: The name of a parent timer (it should really be the name given to
 *	    the parent, not the C symbol name for the parent)
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can get to if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 * This can be used to declare a new static timer structure
 */
#define UPROF_DECLARE_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV) \
  static UProfTimer TIMER_SYMBOL = { \
    .name = NAME, \
    .description = DESCRIPTION, \
    .parent_name = PARENT, \
    .priv = (unsigned long)(PRIV), \
    .seen = 0 \
  }

#define UPROF_TIMER_START(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).seen) \
      { \
	(TIMER_SYMBOL).filename = __FILE__; \
	(TIMER_SYMBOL).line = __LINE__; \
	(TIMER_SYMBOL).function = __FUNCTION__; \
	uprof_context_add_timer (CONTEXT, &(TIMER_SYMBOL)); \
	(TIMER_SYMBOL).count = 0; \
	(TIMER_SYMBOL).start = 0; \
	(TIMER_SYMBOL).total = 0; \
	(TIMER_SYMBOL).slowest = 0; \
	(TIMER_SYMBOL).fastest = 0; \
	(TIMER_SYMBOL).seen = 1; \
      } \
    (TIMER_SYMBOL).start = uprof_get_system_counter (); \
  } while (0)

/* XXX: We should add debug profiling to also verify that the timer isn't already
 * running. */

/* XXX: We should consider system counter wrap around issues */

#define UPROF_TIMER_STOP(CONTEXT, TIMER_SYMBOL) \
  do { \
    unsigned long long duration; \
    (TIMER_SYMBOL).count++; \
    duration = uprof_get_system_counter() - (TIMER_SYMBOL).start; \
    if ((duration < (TIMER_SYMBOL).fastest)) \
      (TIMER_SYMBOL).fastest = duration; \
    else if ((duration > (TIMER_SYMBOL).slowest)) \
      (TIMER_SYMBOL).slowest = duration; \
    (TIMER_SYMBOL).total += duration; \
  } while (0)

/* TODO: */
#if 0
#define UPROF_TIMER_PAUSE()
#define UPROF_TIMER_CONTINUE()
#endif

/* XXX: We should keep in mind the slightly thorny issues of handling shared
 * libraries, where various constants can dissapear as well as the timers
 * themselves. Since the UProf context is always on the heap, we can always
 * add some kind of uprof_context_internalize() mechanism later to ensure that
 * timer/counter statistics are never lost even though the underlying
 * timer structures may come and go. */

/* XXX: We might want the ability to declare "alias" timers so that we
 * can track multiple (filename, line, function) constants for essentially
 * the same timer. */

/* XXX: It would be nice if we could find a way to generalize how
 * relationships between timers and counters are made so we don't need
 * specialized reporting code to handle these use cases:
 *
 * - Having a frame_counter, and then other counters reported relative
 *   to the frame counter; such as averages of minor counters per frame.
 *
 * - Being able to report timer stats in relation to a counter (such as
 *   a frame counter). For example reporting the average time spent
 *   painting, per frame.
 */

/* XXX: static globals shouldn't be the only way to declare timers and
 * counters; exported globals or malloc'd variables might be handy too.
 */

#endif /* _UPROF_H_ */

