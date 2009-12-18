
#include <uprof.h>

#include <stdio.h>

#define UPROF_DEBUG     1

#ifdef UPROF_DEBUG
#define DBG_PRINTF(fmt, args...)              \
  do                                          \
    {                                         \
      printf ("[%s] " fmt, __FILE__, ##args); \
    }                                         \
  while (0)
#else
#define DBG_PRINTF(fmt, args...) do { } while (0)
#endif

UPROF_STATIC_TIMER (full_timer,
                    NULL, /* no parent */
                    "Full timer",
                    "A timer for the test delays in loop0",
                    0 /* no application private data */
);

UPROF_STATIC_COUNTER (loop0_counter,
                      "Loop0 counter",
                      "A Counter for the first loop",
                      0 /* no application private data */
);

UPROF_STATIC_TIMER (loop0_timer,
                    "Full timer", /* parent */
                    "Loop0 timer",
                    "A timer for the test delays in loop0",
                    0 /* no application private data */
);

UPROF_STATIC_TIMER (loop0_sub_timer,
                    "Loop0 timer", /* parent */
                    "Loop0 sub timer",
                    "An example sub timer for loop0",
                    0 /* no application private data */
);

UPROF_STATIC_COUNTER (loop1_counter,
                      "Loop1 counter",
                      "A Counter for the first loop",
                      0 /* no application private data */
);

UPROF_STATIC_TIMER (loop1_timer,
                    "Full timer", /* parent */
                    "Loop1 timer",
                    "A timer for the test delays in loop1",
                    0 /* no application private data */
);

UPROF_STATIC_TIMER (loop1_sub_timer,
                    "Loop1 timer", /* parent */
                    "Loop1 sub timer",
                    "An example sub timer for loop1",
                    0 /* no application private data */
);


int
main (int argc, char **argv)
{
  UProfReport *report;
  UProfContext *context0;
  UProfContext *context1;
  int i;

  uprof_init (&argc, &argv);

  context0 = uprof_context_new ("context0");
  context1 = uprof_context_new ("context1");

  DBG_PRINTF ("start full timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
              uprof_get_system_counter ());
  UPROF_TIMER_START (context0, full_timer);
  for (i = 0; i < 2; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context0, loop0_counter);

      DBG_PRINTF ("start simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
      UPROF_TIMER_START (context0, loop0_timer);
      DBG_PRINTF ("  <delay: 1/2 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/2;
      nanosleep (&delay, NULL);

      UPROF_TIMER_START (context0, loop0_sub_timer);
      DBG_PRINTF ("    <timing sub delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context0, loop0_sub_timer);

      UPROF_TIMER_STOP (context0, loop0_timer);
      DBG_PRINTF ("stop simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
    }

  for (i = 0; i < 4; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context1, loop1_counter);

      DBG_PRINTF ("start simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
      UPROF_TIMER_START (context1, loop1_timer);
      DBG_PRINTF ("  <delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);

      UPROF_TIMER_START (context1, loop1_sub_timer);
      DBG_PRINTF ("    <timing sub delay: 1/2 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/2;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context1, loop1_sub_timer);

      UPROF_TIMER_STOP (context1, loop1_timer);
      DBG_PRINTF ("stop simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
    }

  DBG_PRINTF ("stop full timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
              uprof_get_system_counter ());
  UPROF_TIMER_STOP (context0, full_timer);

  uprof_context_link (context0, context1);
  report = uprof_report_new ("Linking report");
  uprof_report_add_context (report, context0);
  uprof_report_print (report);
  uprof_report_unref (report);

  uprof_context_unref (context0);
  uprof_context_unref (context1);

  return 0;
}

