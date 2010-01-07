
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


int
main (int argc, char **argv)
{
  UProfReport *report;
  UProfContext *context;
  struct timespec delay;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");

  UPROF_RECURSIVE_TIMER_START (context, full_timer);

  DBG_PRINTF ("  <delay: 1/2 sec>\n");
  delay.tv_sec = 0;
  delay.tv_nsec = 1000000000/2;
  nanosleep (&delay, NULL);

  UPROF_RECURSIVE_TIMER_START (context, full_timer);
  DBG_PRINTF ("  <delay: 1/2 sec>\n");
  nanosleep (&delay, NULL);
  UPROF_RECURSIVE_TIMER_STOP (context, full_timer);
  UPROF_RECURSIVE_TIMER_STOP (context, full_timer);

  g_print ("Full timer should have a total duration of 1 second:\n");
  report = uprof_report_new ("Recursion report");
  uprof_report_add_context (report, context);
  uprof_report_print (report);
  uprof_report_unref (report);


  uprof_context_unref (context);

  return 0;
}

