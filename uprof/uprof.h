#ifndef _UPROF_H_
#define _UPROF_H_

#include <glib.h>

typedef struct _UProfContext UProfContext;

typedef struct _UProfCounter
{
  /** Application defined name for counter */
  char *name;

  /** private */
  guint count;

} UProfCounter;

typedef struct _UProfTimer
{
  /** Application defined name for timer */
  char *name;

  /** Application defined grouping */
  guint group;

  /** 0 is highest priority */
  guint priority;

  /** private */
  guint count;
  unsigned long long start;
  unsigned long long total;

  guint	running:1;

} UProfTimer;

/**
 * uprof_init:
 * FIXME
 */
void
uprof_init (int *argc, char ***argv);

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
 * uprof_context_declare_counters:
 * @context: A UProf context
 * @counters: Your application counters FIXME
 *
 * Declares a set of application counters. FIXME
 */
void
uprof_context_declare_counters (UProfContext *context, UProfCounter *counters);

/**
 * uprof_context_declare_timers:
 * @context: A UProf context
 * @counters: Your application timers FIXME
 *
 * Declares a set of application counters. FIXME
 */
void
uprof_context_declare_timers (UProfContext *context, UProfTimer *timers);

/**
 * uprof_get_system_counter:
 * FIXME
 */
unsigned long long
uprof_get_system_counter (void);

/**
 * uprof_context_output_report:
 * @context: A UProf context
 *
 * Generates a report of the accumulated timer and counter data associated with
 * the given context.
 */
void
uprof_context_output_report (UProfContext *context);

//#define UPROF_TIMER_START()

#endif /* _UPROF_H_ */

