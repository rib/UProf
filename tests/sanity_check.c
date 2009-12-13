
#include <uprof.h>

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

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
  uint64_t prev_counter;
  uint64_t counter;
  int i;

  uprof_init (&argc, &argv);

  context = uprof_context_new ("Simple context");

  prev_counter = uprof_get_system_counter ();
  for (i = 0; i < 1000; i++)
    {
      uint64_t diff;
      counter = uprof_get_system_counter ();
      diff = counter - prev_counter;
      prev_counter = counter;
      g_print ("diff = %llu\n", (unsigned long long)diff);
    }

  g_print ("<start Full timer>\n");
  g_print ("  <delay: 1 sec>\n");

  UPROF_TIMER_START (context, full_timer);
  sleep (1);
  UPROF_TIMER_STOP (context, full_timer);
  g_print ("<stop Full timer>\n");

  g_print ("Full timer should have a total duration of 1 second:\n");
  report = uprof_report_new ("Sanity Check report");
  uprof_report_add_context (report, context);
  uprof_report_print (report);
  uprof_report_unref (report);


  uprof_context_unref (context);

  return 0;
}

