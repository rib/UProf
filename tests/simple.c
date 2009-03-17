
#include <uprof.h>

#include <stdio.h>

UPROF_DECLARE_COUNTER (simple_counter,
		       "Simple counter",
		       "An example counter",
		       0 /* no application private data */
		       );

UPROF_DECLARE_TIMER (simple_timer,
		     NULL, /* no parent */
		     "Simple timer",
		     "An example timer",
		     0 /* no application private data */
		     );

int
main (int argc, char **argv)
{
  UProfContext *context;
  int i;
  struct timespec delay;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");

  delay.tv_sec = 0;
  delay.tv_nsec = 1000000000/4;

  for (i = 0; i < 10; i ++)
    {
      UPROF_COUNTER_INC (context, simple_counter);
      printf ("rdtsc = %llu\n", uprof_get_system_counter ());

      UPROF_TIMER_START (context, simple_timer);
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context, simple_timer);
    }

  uprof_context_output_report (context);

  uprof_context_unref (context);

  return 0;
}

