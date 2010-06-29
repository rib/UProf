/* This file is part of UProf.
 *
 * Copyright © 2008, 2009, 2010 Robert Bragg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef _UPROF_TIMER_H_
#define _UPROF_TIMER_H_

#include <glib.h>

#include <uprof-object-state.h>

G_BEGIN_DECLS

typedef struct _UProfTimerState UProfTimerState;
struct _UProfTimerState
{
  /*< private >*/

  UProfObjectState  object;

  gboolean          disabled;
  int               recursion;

  char             *parent_name;

  unsigned long     count;
  guint64           start;
  guint64           total;
  guint64           partial_duration;

  guint64           fastest;
  guint64           slowest;

  /* note: not resolved until sorting @ report time */
  UProfTimerState  *parent;
  GList            *children;

  unsigned long padding0;
  unsigned long padding1;
  unsigned long padding2;
  unsigned long padding3;
  unsigned long padding4;
  unsigned long padding5;
  unsigned long padding6;
  unsigned long padding7;
  unsigned long padding8;
  unsigned long padding9;
};

typedef struct _UProfTimer
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
  struct _UProfTimerState *state;

  const char *filename;
  unsigned long line;
  const char *function;

  unsigned long padding0;
  unsigned long padding1;
  unsigned long padding2;
  unsigned long padding3;
  unsigned long padding4;
  unsigned long padding5;
  unsigned long padding6;
  unsigned long padding7;
  unsigned long padding8;
  unsigned long padding9;

} UProfTimer;

/**
 * UPROF_STATIC_TIMER:
 * @TIMER_SYMBOL: The name of the C symbol to declare
 * @PARENT: The name of a parent timer (To clarify; this should be a string
 *          name identifying the parent, not the C symbol name for the parent)
 * @NAME: The timer name used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * This can be used to declare a new timer structure which can then
 * be used with the UPROF_TIMER_START() and UPROF_TIMER_STOP() macros.
 * Usually you should use UPROF_STATIC_TIMER() instead but this can be useful
 * for special cases where you need to access a timer from multiple files.
 */
#define UPROF_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV) \
  UProfTimer TIMER_SYMBOL = { \
    .name = NAME, \
    .description = DESCRIPTION, \
    .parent_name = PARENT, \
    .priv = (unsigned long)(PRIV), \
    .state = NULL \
  }

/**
 * UPROF_STATIC_TIMER:
 * @TIMER_SYMBOL: The name of the C symbol to declare
 * @PARENT: The name of a parent timer (it should really be the name given to
 *	    the parent, not the C symbol name for the parent)
 * @NAME: The timer name used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * This can be used to declare a new static timer structure which can then
 * be used with the UPROF_TIMER_START() and UPROF_TIMER_STOP() macros.
 */
#define UPROF_STATIC_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV) \
  static UPROF_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV)

#define _UPROF_TIMER_INIT_IF_UNSEEN(CONTEXT, TIMER_SYMBOL) \
  do { \
    (TIMER_SYMBOL).filename = __FILE__; \
    (TIMER_SYMBOL).line = __LINE__; \
    (TIMER_SYMBOL).function = __FUNCTION__; \
    uprof_context_add_timer (CONTEXT, &(TIMER_SYMBOL)); \
  } while (0)

#ifdef UPROF_DEBUG
#define _UPROF_TIMER_DEBUG_CHECK_FOR_RECURSION(CONTEXT, TIMER_SYMBOL) \
  do { \
    if ((TIMER_SYMBOL).state->start) \
      { \
        g_warning ("Recursive starting of timer (%s) unsupported! \n" \
                   "You should use UPROF_RECURSIVE_TIMER_START if you " \
                   "need recursion", \
                   (TIMER_SYMBOL).name); \
      } \
  } while (0)
#else
#define _UPROF_TIMER_DEBUG_CHECK_FOR_RECURSION(CONTEXT, TIMER_SYMBOL)
#endif

/**
 * UPROF_TIMER_START:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Starts the timer timing. It is an error to try and start a timer that
 * is already timing; if you need to do this you should use
 * UPROF_RECURSIVE_TIMER_START() instead.
 */
#define UPROF_TIMER_START(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).state) \
      _UPROF_TIMER_INIT_IF_UNSEEN (CONTEXT, TIMER_SYMBOL); \
    _UPROF_TIMER_DEBUG_CHECK_FOR_RECURSION (CONTEXT, TIMER_SYMBOL); \
    (TIMER_SYMBOL).state->start = uprof_get_system_counter (); \
  } while (0)

/**
 * UPROF_RECURSIVE_TIMER_START:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Starts the timer timing like UPROF_TIMER_START() but with additional guards
 * to allow the timer to be started multiple times, such that
 * UPROF_RECURSIVE_TIMER_STOP() must be called an equal number of times to
 * actually stop it timing.
 */
#define UPROF_RECURSIVE_TIMER_START(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).state) \
      _UPROF_TIMER_INIT_IF_UNSEEN (CONTEXT, TIMER_SYMBOL); \
    if ((TIMER_SYMBOL).state->recursion++ == 0) \
      { \
        (TIMER_SYMBOL).state->start = uprof_get_system_counter (); \
      } \
  } while (0)

#ifdef UPROF_DEBUG
#define _UPROF_TIMER_DEBUG_CHECK_TIMER_WAS_STARTED(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).state->start) \
      { \
        g_warning ("Stopping an un-started timer! (%s)", \
                   (TIMER_SYMBOL).name); \
      } \
  } while (0)
#else
#define _UPROF_TIMER_DEBUG_CHECK_TIMER_WAS_STARTED(CONTEXT, TIMER_SYMBOL)
#endif

#define _UPROF_TIMER_UPDATE_TOTAL_AND_CMP_FAST_SLOW(TIMER_SYMBOL) \
  do { \
    if (G_UNLIKELY (duration < (TIMER_SYMBOL).state->fastest)) \
      (TIMER_SYMBOL).state->fastest = duration; \
    else if (G_UNLIKELY (duration > (TIMER_SYMBOL).state->slowest)) \
      (TIMER_SYMBOL).state->slowest = duration; \
    (TIMER_SYMBOL).state->total += duration; \
  } while (0)

#define _UPROF_TIMER_UPDATE_TOTAL_FASTEST_SLOWEST(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (G_LIKELY (!(TIMER_SYMBOL).state->disabled)) \
      { \
        guint64 duration = uprof_get_system_counter() - \
                           (TIMER_SYMBOL).state->start + \
                           (TIMER_SYMBOL).state->partial_duration; \
        (TIMER_SYMBOL).state->partial_duration = 0; \
        _UPROF_TIMER_UPDATE_TOTAL_AND_CMP_FAST_SLOW (TIMER_SYMBOL); \
      } \
    else if (G_UNLIKELY ((TIMER_SYMBOL).state->partial_duration)) \
      { \
        guint64 duration = (TIMER_SYMBOL).state->partial_duration; \
        (TIMER_SYMBOL).state->partial_duration = 0; \
        _UPROF_TIMER_UPDATE_TOTAL_AND_CMP_FAST_SLOW (TIMER_SYMBOL); \
      } \
  } while (0)

/**
 * UPROF_TIMER_STOP:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Stops the timer timing. It is an error to try and stop a timer that
 * isn't actually timing. It is also an error to use this macro if the
 * timer was started using UPROF_RECURSIVE_TIMER_START(); you should
 * use UPROF_RECURSIVE_TIMER_STOP() instead.
 */
#define UPROF_TIMER_STOP(CONTEXT, TIMER_SYMBOL) \
  do { \
    _UPROF_TIMER_DEBUG_CHECK_TIMER_WAS_STARTED (CONTEXT, TIMER_SYMBOL); \
    _UPROF_TIMER_UPDATE_TOTAL_FASTEST_SLOWEST (CONTEXT, TIMER_SYMBOL); \
    (TIMER_SYMBOL).state->count++; \
    (TIMER_SYMBOL).state->start = 0; \
  } while (0)

/**
 * UPROF_RECURSIVE_TIMER_STOP:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Stops a recursive timer timing. It is an error to try and stop a timer that
 * isn't actually timing. It is also an error to use this macro if the timer
 * was started using UPROF_TIMER_START(); you should use UPROF_TIMER_STOP()
 * instead.
 */
#define UPROF_RECURSIVE_TIMER_STOP(CONTEXT, TIMER_SYMBOL) \
  do { \
    g_assert ((TIMER_SYMBOL).state->recursion > 0); \
    if ((TIMER_SYMBOL).state->recursion-- == 1) \
      { \
        UPROF_TIMER_STOP (CONTEXT, TIMER_SYMBOL); \
      } \
  } while (0)

/* XXX: We should consider system counter wrap around issues */

gint
_uprof_timer_compare_total_times (struct _UProfTimerState *a,
                                  struct _UProfTimerState *b,
                                  gpointer data);
gint
_uprof_timer_compare_start_count (struct _UProfTimerState *a,
                                  struct _UProfTimerState *b,
                                  gpointer data);
#define UPROF_TIMER_SORT_TIME_INC \
  ((GCompareDataFunc)_uprof_timer_compare_total_times)
#define UPROF_TIMER_SORT_COUNT_INC \
  ((GCompareDataFunc)_uprof_timer_compare_start_count)


G_END_DECLS

#endif /* _UPROF_TIMER_H_ */

