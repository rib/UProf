
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

UPROF_DECLARE_TIMER (simple_sub_timer,
		     "Simple timer", /* parent */
		     "Simple sub timer",
		     "An example sub timer",
		     0 /* no application private data */
		     );

int
main (int argc, char **argv)
{
  UProfContext *context;
  int i;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");


  for (i = 0; i < 10; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context, simple_counter);

      printf ("start simple timer (rdtsc = %llu)\n", uprof_get_system_counter ());
      UPROF_TIMER_START (context, simple_timer);
      printf ("  <delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);

      printf ("  start simple sub timer (rdtsc = %llu)\n", uprof_get_system_counter ());
      UPROF_TIMER_START (context, simple_sub_timer);
      printf ("    <sub delay: 1/8 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/8;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context, simple_sub_timer);
      printf ("  stop simple sub timer (rdtsc = %llu)\n", uprof_get_system_counter ());

      UPROF_TIMER_STOP (context, simple_timer);
      printf ("stop simple timer (rdtsc = %llu)\n", uprof_get_system_counter ());
    }

  uprof_context_output_report (context);

  uprof_context_unref (context);

  return 0;
}

