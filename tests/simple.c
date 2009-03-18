
#include <uprof.h>

#include <stdio.h>

UPROF_DECLARE_TIMER (full_timer,
		     NULL, /* no parent */
		     "Full timer",
		     "A timer for the test delays in loop0",
		     0 /* no application private data */
		     );

UPROF_DECLARE_COUNTER (loop0_counter,
		       "Loop0 counter",
		       "A Counter for the first loop",
		       0 /* no application private data */
		       );

UPROF_DECLARE_TIMER (loop0_timer,
		     "Full timer", /* parent */
		     "Loop0 timer",
		     "A timer for the test delays in loop0",
		     0 /* no application private data */
		     );

UPROF_DECLARE_TIMER (loop0_sub_timer,
		     "Loop0 timer", /* parent */
		     "Loop0 sub timer",
		     "An example sub timer for loop0",
		     0 /* no application private data */
		     );

UPROF_DECLARE_COUNTER (loop1_counter,
		       "Loop1 counter",
		       "A Counter for the first loop",
		       0 /* no application private data */
		       );

UPROF_DECLARE_TIMER (loop1_timer,
		     "Full timer", /* parent */
		     "Loop1 timer",
		     "A timer for the test delays in loop1",
		     0 /* no application private data */
		     );

UPROF_DECLARE_TIMER (loop1_sub_timer,
		     "Loop1 timer", /* parent */
		     "Loop1 sub timer",
		     "An example sub timer for loop1",
		     0 /* no application private data */
		     );


int
main (int argc, char **argv)
{
  UProfContext *context;
  int i;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");


  printf ("start full timer (rdtsc = %llu)\n", uprof_get_system_counter ());
  UPROF_TIMER_START (context, full_timer);
  for (i = 0; i < 2; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context, loop0_counter);

      printf ("start simple timer (rdtsc = %llu)\n", uprof_get_system_counter ());
      UPROF_TIMER_START (context, loop0_timer);
      printf ("  <delay: 1/2 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/2;
      nanosleep (&delay, NULL);

      UPROF_TIMER_START (context, loop0_sub_timer);
      printf ("    <timing sub delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context, loop0_sub_timer);

      UPROF_TIMER_STOP (context, loop0_timer);
      printf ("stop simple timer (rdtsc = %llu)\n", uprof_get_system_counter ());
    }

  for (i = 0; i < 4; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context, loop1_counter);

      printf ("start simple timer (rdtsc = %llu)\n", uprof_get_system_counter ());
      UPROF_TIMER_START (context, loop1_timer);
      printf ("  <delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);

      UPROF_TIMER_START (context, loop1_sub_timer);
      printf ("    <timing sub delay: 1/2 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/2;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context, loop1_sub_timer);

      UPROF_TIMER_STOP (context, loop1_timer);
      printf ("stop simple timer (rdtsc = %llu)\n", uprof_get_system_counter ());
    }

  printf ("stop full timer (rdtsc = %llu)\n", uprof_get_system_counter ());
  UPROF_TIMER_STOP (context, full_timer);

  uprof_context_output_report (context);

  uprof_context_unref (context);

  return 0;
}

