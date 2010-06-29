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

#ifndef _UPROF_COUNTER_H_
#define _UPROF_COUNTER_H_

#include <uprof-object-state.h>

#include <glib.h>

G_BEGIN_DECLS

typedef struct _UProfCounterState
{
  /*< private >*/
  UProfObjectState  object;

  gboolean          disabled;

  unsigned long     count;

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

} UProfCounterState;


typedef struct _UProfCounter
{
  /** Application defined name for counter */
  const char *name;

  /** Application defined description for counter */
  const char *description;

  /** Application private data */
  unsigned long priv;

  /** private */
  struct _UProfCounterState *state;

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

} UProfCounter;

/**
 * UPROF_COUNTER:
 * @COUNTER_SYMBOL: The name of the C symbol to declare
 * @NAME: The name of the counter used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * Declares a new counter structure which can be used with the
 * UPROF_COUNTER_INC(), UPROF_COUNTER_DEC() and UPROF_COUNTER_ZERO() macros.
 * Usually you should use UPROF_STATIC_COUNTER() instead, but this may be useful
 * for special cases where counters are accessed from multiple files.
 */
#define UPROF_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV) \
  UProfCounter COUNTER_SYMBOL = { \
    .name = NAME, \
    .description = DESCRIPTION, \
    .priv = (unsigned long)(PRIV), \
    .state = NULL \
  }

/**
 * UPROF_STATIC_COUNTER:
 * @COUNTER_SYMBOL: The name of the C symbol to declare
 * @NAME: The name of the counter used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * Declares a new static counter structure which can be used with the
 * UPROF_COUNTER_INC(), UPROF_COUNTER_DEC() and UPROF_COUNTER_ZERO() macros.
 */
#define UPROF_STATIC_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV) \
  static UPROF_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV)

#define _UPROF_COUNTER_INIT_IF_UNSEEN(CONTEXT, COUNTER_SYMBOL) \
  do { \
    (COUNTER_SYMBOL).filename = __FILE__; \
    (COUNTER_SYMBOL).line = __LINE__; \
    (COUNTER_SYMBOL).function = __FUNCTION__; \
    uprof_context_add_counter (CONTEXT, &(COUNTER_SYMBOL)); \
  } while (0)

/**
 * UPROF_COUNTER_INC:
 * @CONTEXT: A UProfContext
 * @COUNTER_SYMBOL: A counter variable
 *
 * Increases the count for the given @COUNTER_SYMBOL.
 */
#define UPROF_COUNTER_INC(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).state) \
      _UPROF_COUNTER_INIT_IF_UNSEEN (CONTEXT, COUNTER_SYMBOL); \
    if ((COUNTER_SYMBOL).state->disabled) \
      break; \
    (COUNTER_SYMBOL).state->count++; \
  } while (0)

/**
 * UPROF_COUNTER_DEC:
 * @CONTEXT: A UProfContext
 * @COUNTER_SYMBOL: A counter variable
 *
 * Decreases the count for the given @COUNTER_SYMBOL.
 */
#define UPROF_COUNTER_DEC(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).state) \
      _UPROF_COUNTER_INIT_IF_UNSEEN (CONTEXT, COUNTER_SYMBOL) \
    if ((COUNTER_SYMBOL).state->disabled) \
      break; \
    (COUNTER_SYMBOL).state->count--; \
  } while (0)

/**
 * UPROF_COUNTER_ZERO:
 * @CONTEXT: A UProfContext
 * @COUNTER_SYMBOL: A counter variable
 *
 * Resets the count for the given @COUNTER_SYMBOL.
 */
#define UPROF_COUNTER_ZERO(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).state) \
      _UPROF_COUNTER_INIT_IF_UNSEEN (CONTEXT, COUNTER_SYMBOL) \
    if ((COUNTER_SYMBOL).state->disabled) \
      break; \
    (COUNTER_SYMBOL).state->count = 0; \
  } while (0)


gint
_uprof_counter_compare_count (struct _UProfCounterState *a,
                              struct _UProfCounterState *b,
                              gpointer data);
#define UPROF_COUNTER_SORT_COUNT_INC \
  ((GCompareDataFunc)_uprof_counter_compare_count)


G_END_DECLS

#endif /* _UPROF_COUNTER_H_ */

