
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
  UProfContext *context;
  UProfReport *report;
  int i;
  GMainLoop *mainloop;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");


  DBG_PRINTF ("start full timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
              uprof_get_system_counter ());
  UPROF_TIMER_START (context, full_timer);
  for (i = 0; i < 2; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context, loop0_counter);

      DBG_PRINTF ("start simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
      UPROF_TIMER_START (context, loop0_timer);
      DBG_PRINTF ("  <delay: 1/2 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/2;
      nanosleep (&delay, NULL);

      UPROF_TIMER_START (context, loop0_sub_timer);
      DBG_PRINTF ("    <timing sub delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context, loop0_sub_timer);

      UPROF_TIMER_STOP (context, loop0_timer);
      DBG_PRINTF ("stop simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
    }

  for (i = 0; i < 4; i ++)
    {
      struct timespec delay;
      UPROF_COUNTER_INC (context, loop1_counter);

      DBG_PRINTF ("start simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
      UPROF_TIMER_START (context, loop1_timer);
      DBG_PRINTF ("  <delay: 1/4 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      nanosleep (&delay, NULL);

      UPROF_TIMER_START (context, loop1_sub_timer);
      DBG_PRINTF ("    <timing sub delay: 1/2 sec>\n");
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/2;
      nanosleep (&delay, NULL);
      UPROF_TIMER_STOP (context, loop1_sub_timer);

      UPROF_TIMER_STOP (context, loop1_timer);
      DBG_PRINTF ("stop simple timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
                  uprof_get_system_counter ());
    }

  DBG_PRINTF ("stop full timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
              uprof_get_system_counter ());
  UPROF_TIMER_STOP (context, full_timer);

  report = uprof_report_new ("Simple report");
  uprof_report_add_context (report, context);

  mainloop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);


  uprof_report_print (report);
  uprof_report_unref (report);

  uprof_context_unref (context);

  return 0;
}

