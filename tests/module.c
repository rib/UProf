#include <uprof.h>

UPROF_STATIC_COUNTER (run_counter,
                      "Run counter",
                      "Counter how many times the module is run",
                      0 /* no application private data */
);

UPROF_STATIC_TIMER (run_timer,
                    "Full timer", /* parent */
                    "Run timer",
                    "Time the running of this module",
                    0 /* no application private data */
);

extern UProfContext *shared_context;

void
run (void)
{
  struct timespec delay;

  UPROF_COUNTER_INC (shared_context, run_counter);
  UPROF_TIMER_START (shared_context, run_timer);

  delay.tv_sec = 0;
  delay.tv_nsec = 1000000000/2;
  nanosleep (&delay, NULL);

  UPROF_TIMER_STOP (shared_context, run_timer);
}

