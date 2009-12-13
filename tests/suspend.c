
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

UPROF_STATIC_TIMER (loop_timer,
                    NULL, /* no parent */
                    "Loop timer",
                    "A timer for the test delays",
                    0 /* no application private data */
);

UPROF_STATIC_COUNTER (loop_counter,
                      "Loop counter",
                      "A Counter for the loop",
                      0 /* no application private data */
);

int
main (int argc, char **argv)
{
  UProfReport *report;
  UProfContext *context;
  int i;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Test");


  for (i = 0; i < 4; i ++)
    {
      struct timespec delay;

      if (i == 1)
        {
          DBG_PRINTF ("suspending context\n");
          uprof_suspend_context (context);
        }

      UPROF_COUNTER_INC (context, loop_counter);

      UPROF_TIMER_START (context, loop_timer);
      DBG_PRINTF ("  <delay: 1 sec>\n");
      delay.tv_sec = 1;
      delay.tv_nsec = 0;
      nanosleep (&delay, NULL);

      UPROF_TIMER_STOP (context, loop_timer);
      DBG_PRINTF ("stop simple timer (rdtsc = %llu)\n",
                  uprof_get_system_counter ());

      if (i == 2)
        {
          DBG_PRINTF ("resuming context\n");
          uprof_resume_context (context);
        }
    }

  DBG_PRINTF ("Expected result = 2 seconds accounted for and count == 2:\n");

  report = uprof_report_new ("Suspend report");
  uprof_report_add_context (report, context);
  uprof_report_print (report);
  uprof_report_unref (report);

  uprof_context_unref (context);

  return 0;
}

