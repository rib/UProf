
#include <uprof.h>

#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>

UPROF_STATIC_TIMER (full_timer,
                    NULL, /* no parent */
                    "Full timer",
                    "A timer for the test delays in loop0",
                    0 /* no application private data */
);

UProfContext *shared_context;

static void
load_and_run_module ()
{
  void *handle;
  void (*run)(void);

  handle = dlopen ("./module.so", RTLD_LAZY);
  if (!handle)
    g_critical ("Failed to load test module: %s", dlerror ());

  run = dlsym(handle, "run");
  if (!run)
    g_critical ("Failed to find run symbol: %s", dlerror ());

  run ();
  dlclose (handle);
}

int
main (int argc, char **argv)
{
  UProfReport *report;

  uprof_init (&argc, &argv);

  shared_context = uprof_context_new ("Simple context");

  printf ("start full timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
          uprof_get_system_counter ());
  UPROF_TIMER_START (shared_context, full_timer);

  load_and_run_module ();
  load_and_run_module ();

  printf ("stop full timer (rdtsc = %" G_GUINT64_FORMAT ")\n",
          uprof_get_system_counter ());
  UPROF_TIMER_STOP (shared_context, full_timer);

  report = uprof_report_new ("dlopen report");
  uprof_report_add_context (report, shared_context);
  uprof_report_print (report);
  uprof_report_unref (report);


  uprof_context_unref (shared_context);

  return 0;
}

