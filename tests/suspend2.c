
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

UPROF_STATIC_TIMER (timer0,
                    NULL, /* no parent */
                    "Suspend timer",
                    "A timer for the test delays",
                    0 /* no application private data */
);

int
main (int argc, char **argv)
{
  UProfReport *report;
  UProfContext *context;
  struct timespec delay;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Test");

  DBG_PRINTF ("starting timer0\n");
  UPROF_TIMER_START (context, timer0);

  DBG_PRINTF ("  <delay: 1 sec>\n");
  delay.tv_sec = 1;
  delay.tv_nsec = 0;
  nanosleep (&delay, NULL);

  DBG_PRINTF ("suspending context\n");
  uprof_context_suspend (context);

  DBG_PRINTF ("  <delay: 1 sec>\n");
  delay.tv_sec = 1;
  delay.tv_nsec = 0;
  nanosleep (&delay, NULL);

  DBG_PRINTF ("stopping timer0\n");
  UPROF_TIMER_STOP (context, timer0);

  DBG_PRINTF ("resuming context\n");
  uprof_context_resume (context);

  DBG_PRINTF ("Expected result = timer0 = 1 seconds accounted for:\n");

  report = uprof_report_new ("Suspend report");
  uprof_report_add_context (report, context);
  uprof_report_print (report);
  uprof_report_unref (report);

  uprof_context_unref (context);

  return 0;
}

