
#include <uprof.h>

#include <stdio.h>

UProfCounter counters[] = {
  {
    .name = "Simple counter"
  },
  {0}
};

UProfTimer timers[] = {
  {
    .name = "Simple timer"
  },
  {0}
};

int
main (int argc, char **argv)
{
  UProfContext *context;
  int i;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");
  uprof_context_declare_counters (context, counters);
  uprof_context_declare_timers (context, timers);

  /* TODO
   * I think the details about how timers/counters are declared probably needs
   * more thought. The static declaration requirements can possibly be reduced
   * to an enum of timer names, and the name can instead be implicitly
   * associated with the timer the first time it is used: e.g.
   *
   * typedef enum _MyTimers {
   *  MY_TIMER_A,
   *  MY_TIMER_B
   * } MyTimers;
   *
   * #define UPROF_START_TIMER(TIMER, PARENT, DESCRIPTION) ...
   *
   * The internal UProfTimer structure can then have a bitfield to determine
   * if this is the first time we have seen the timer; if yes:
   *  - use the TIMER enum to index into the internal array of timer structures
   *  - associated the __FILE__, __LINE__, __FUNCTION__ and DESCRIPTION with
   *    the internal timer structure.
   *  - save the current rdts value.
   * else:
   *  - use the TIMER enum to index into the internal array of timer structures
   *  - save the current rdts value.
   *
   */

  for (i = 0; i < 10; i ++)
    printf ("rdtsc = %llu\n", uprof_query_system_counter ());

  uprof_context_output_report (context);

  uprof_context_unref (context);

  return 0;
}

