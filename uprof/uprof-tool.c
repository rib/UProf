/* This file is part of UProf.
 *
 * Copyright © 2010 Robert Bragg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <config.h>

#include <uprof.h>

#include <glib.h>
/* #include <glib/gi18n-lib.h> */

#include <stdio.h>
#include <string.h>
#include <ncursesw/ncurses.h>
#include <locale.h>

#define UT_MESSAGE_WIN_HEIGHT 3

#define UT_DEFAULT_COLOR 0
#define UT_HEADER_COLOR 1
#define UT_WARNING_COLOR 2
#define UT_KEY_LABEL_COLOR 3

typedef struct
{
  int key0;
  int key1;
  char *desc;
} KeyHandler;

typedef enum
{
  UT_MESSAGE_EMPHASIS_0,
  UT_MESSAGE_EMPHASIS_1
} UTMessageEmphasis;

typedef struct
{
  UTMessageEmphasis emphasis;
  GList *categories;
  char *location;
  char *message;
  int count;
} UTMessageEntry;

typedef struct
{
  GQueue *queue;
  int size;
} UTMessageQueue;

typedef struct
{
  UTMessageQueue *message_queue;
  int height;

  int scroll_pos;
  int cursor_view_pos;
  int cursor_queue_pos;

  gboolean show_details;

  int default_attributes;
  int cursor_attributes;
  chtype background;
} UTMessageQueueView;

typedef enum
{
  UT_WARN_LEVEL_NORMAL = UT_MESSAGE_EMPHASIS_0,
  UT_WARN_LEVEL_HIGH = UT_MESSAGE_EMPHASIS_1
} UTWarnLevel;

static gboolean arg_list = FALSE;
static gboolean arg_zero = FALSE;
static char *arg_bus_name = NULL;
static char *arg_report_name = NULL;
static char **arg_remaining = NULL;

static GMainLoop *mainloop;

static UProfReportProxy *report_proxy;

static int queue_redraw_idle_id;

static int get_report_timeout_id;
static char *timers_counters_report;

static int screen_width, screen_height;

static WINDOW *titlebar_window;

static WINDOW *main_window;

#define UT_MAX_WARNING_COUNT UT_MESSAGE_WIN_HEIGHT
static int wait_for_idle_warnings;
static WINDOW *warning_window;
static UTMessageQueue *warning_queue;
static UTMessageQueueView *warning_queue_view;
static gboolean eating_warnings;

static WINDOW *keys_window;

#define UT_TRACE_LOG_SIZE 10000
static UTMessageQueue *trace_queue;
static UTMessageQueueView *trace_queue_view;

typedef enum
{
  UT_PAGE_TIMERS_COUNTERS,
  UT_PAGE_TRACE,
  UT_PAGE_COUNT
} UTPage;

static int current_page = UT_PAGE_TIMERS_COUNTERS;
static int trace_message_filter_id;

static KeyHandler keys[] = {
  { 'q', 'Q', "Q - Quit" },
};

static void queue_redraw (void);

static gboolean
pre_parse_hook (GOptionContext  *context,
                GOptionGroup    *group,
                gpointer         data,
                GError         **error)
{
  return TRUE;
}

static gboolean
post_parse_hook (GOptionContext  *context,
                 GOptionGroup    *group,
                 gpointer         data,
                 GError         **error)
{
  return TRUE;
}

static GOptionEntry uprof_tool_args[] = {
  { "list", 'l', 0, G_OPTION_ARG_NONE, &arg_list,
    "List available UProf reports", NULL },

  { "bus", 'b', 0, G_OPTION_ARG_STRING, &arg_bus_name,
    "Specify the DBus bus name to find the report on", NULL },

  { "report", 'r', 0, G_OPTION_ARG_STRING, &arg_report_name,
    "Specify a report name to act on", NULL },

  { "zero", 'z', 0, G_OPTION_ARG_NONE, &arg_zero,
    "Reset the timers and counters of a report", NULL },

  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &arg_remaining,
    "COMMAND", NULL },
  { NULL, },
};

static char **
list_reports (void)
{
  char **names;
  GError *error = NULL;

  g_print ("Searching via session bus for "
           "org.freedesktop.UProf.Service objects...\n");
  names = uprof_dbus_list_reports (&error);
  if (!names)
    {
      if (error->domain == UPROF_DBUS_ERROR &&
          error->code == UPROF_DBUS_ERROR_UNKNOWN_REPORT)
        g_print ("None found!\n");
      else
        g_print ("Failed to list reports: %s", error->message);
      g_error_free (error);
    }
  else if (names[0] == NULL)
    g_print ("None found!\n");
  else
    {
      int i;
      g_print ("Found:\n");
      for (i = 0; names[i]; i++)
        {
          char **strv = g_strsplit (names[i], "@", 0);
          g_print ("  --bus=\"%s\" --report=\"%s\"\n", strv[1], strv[0]);
          g_strfreev (strv);
        }
    }

  return names;
}

static void
destroy_windows (void)
{
  if (titlebar_window)
    {
      delwin (titlebar_window);
      titlebar_window = NULL;
    }

  if (main_window)
    {
      delwin (main_window);
      main_window = NULL;
    }

  if (warning_window)
    {
      delwin (warning_window);
      warning_window = NULL;
    }

  if (keys_window)
    {
      delwin (keys_window);
      keys_window = NULL;
    }
}

static gboolean
message_entry_equal (UTMessageEntry *entry,
                     UTMessageEmphasis emphasis,
                     GList *categories,
                     const char *location,
                     const char *message)
{
  GList *l0;
  GList *l1;

  if (!entry)
    return FALSE;

  if (strcmp (entry->message, message) != 0)
    return FALSE;

  if (strcmp (entry->location, location) != 0)
    return FALSE;

  for (l0 = entry->categories, l1 = categories;
       l0 && l1;
       l0 = l0->next, l1 = l1->next)
    {
      char *category0 = l0->data;
      char *category1 = l1->data;
      if (strcmp (category0, category1) != 0)
        return FALSE;
    }
  if (l0 != NULL || l1 != NULL)
    return FALSE;

  return TRUE;
}

static void
ut_message_entry_free (UTMessageEntry *entry)
{
  g_free (entry->message);
  g_list_foreach (entry->categories, (GFunc)g_free, NULL);
  g_list_free (entry->categories);
  g_slice_free (UTMessageEntry, entry);
}

static void
ut_message_queue_vappend (UTMessageQueue *message_queue,
                          UTMessageEmphasis emphasis,
                          GList *categories,
                          const char *location,
                          const char *format,
                          va_list ap)
{
  char *message;
  UTMessageEntry *entry;
  UTMessageEntry *last;

  message = g_strdup_vprintf (format, ap);

  last = g_queue_peek_tail (message_queue->queue);
  if (message_entry_equal (last, emphasis, categories, location, message))
    last->count++;
  else
    {
      if (g_queue_get_length (message_queue->queue) >= message_queue->size)
        {
          entry = g_queue_pop_head (message_queue->queue);
          ut_message_entry_free (entry);
        }

      entry = g_slice_new (UTMessageEntry);
      entry->emphasis = emphasis;
      entry->categories = categories;
      entry->location = g_strdup (location);
      entry->message = message;
      entry->count = 1;
      g_queue_push_tail (message_queue->queue, entry);
    }
}

/* XXX: Note we take over ownership of the given list of categories */
static void
ut_message_queue_append (UTMessageQueue *message_queue,
                         UTMessageEmphasis emphasis,
                         GList *categories,
                         const char *location,
                         const char *format, ...)
{
  va_list ap;

  va_start (ap, format);

  ut_message_queue_vappend (message_queue,
                            emphasis,
                            categories,
                            location,
                            format,
                            ap);

  va_end (ap);
}

UTMessageQueue *
ut_message_queue_new (int size)
{
  UTMessageQueue *message_queue = g_slice_new0 (UTMessageQueue);
  message_queue->queue = g_queue_new ();
  message_queue->size = size;
  return message_queue;
}

static gboolean
eat_warning_cb (void *user_data)
{
  UTMessageEntry *entry = g_queue_pop_head (warning_queue->queue);
  ut_message_entry_free (entry);

  if (warning_queue->queue->head)
    return TRUE;
  else
    {
      eating_warnings = FALSE;
      return FALSE;
    }
}

static gboolean
idle_warnings_cb (void *user_data)
{
  if (!eating_warnings)
    {
      g_timeout_add_seconds (2, eat_warning_cb, NULL);
      eating_warnings = TRUE;
    }
  return FALSE;
}

static void
ut_warning (UTWarnLevel level, const char *format, ...)
{
  va_list ap;

  va_start (ap, format);

  ut_message_queue_vappend (warning_queue,
                            level,
                            NULL,
                            NULL,
                            format,
                            ap);

  va_end (ap);

  /* Restart the wait for idle warnings */
  if (wait_for_idle_warnings)
    g_source_remove (wait_for_idle_warnings);
  wait_for_idle_warnings =
    g_timeout_add_seconds (5, (GSourceFunc)idle_warnings_cb, NULL);
}

static void
get_report_cb (void *user_data)
{
  char *report;
  GError *error;

  error = NULL;
  report = uprof_report_proxy_get_text_report (report_proxy, &error);
  if (!report)
    {
      ut_warning (UT_WARN_LEVEL_HIGH,
                  "Failed to fetch report: %s", error->message);
      g_error_free (error);
    }

  error = NULL;
  if (!uprof_report_proxy_reset (report_proxy, &error))
    {
      ut_warning (UT_WARN_LEVEL_HIGH,
                  "Failed to zero report statistics: %s", error->message);
      g_error_free (error);
    }

  g_free (timers_counters_report);
  timers_counters_report = report;
  queue_redraw ();
}

static void
keys_window_print (void)
{
  int i;

  keys_window = subwin (stdscr, 1, screen_width, screen_height - 1, 0);

  wattrset(keys_window, COLOR_PAIR (UT_KEY_LABEL_COLOR));
  wbkgd(keys_window, COLOR_PAIR (UT_KEY_LABEL_COLOR));
  werase(keys_window);
  wmove (keys_window, 0, 1);
  for (i = 0; i < G_N_ELEMENTS (keys); i++)
    wprintw (keys_window, "%s", keys[i].desc);
}

void
print_timers_counters_page (void)
{
  main_window = subwin (stdscr,
                        screen_height - 2,
                        screen_width,
                        1, 0);

  werase(main_window);
  mvwprintw (main_window, 0, 0, "%s", timers_counters_report);

  keys_window_print ();
}

static void
ut_message_queue_view_set_height (UTMessageQueueView *view, int height)
{
  view->height = height;
}

static void
ut_message_queue_view_print (UTMessageQueueView *view,
                             WINDOW *window)
{
  GList *l;
  GList *messages_tail = view->message_queue->queue->tail;
  int i;

  wattrset (window, view->default_attributes);
  wbkgd (window, view->background);
  werase (window);

  for (l = messages_tail, i = 0;
       l;
       l = l->prev, i++)
    {
      UTMessageEntry *entry = l->data;
      if (view->cursor_view_pos != -1)
        {
          if (i == view->cursor_view_pos)
            {
              wattrset (window, view->cursor_attributes);
              /* TODO: update details window */
            }
          else if (i == (view->cursor_view_pos + 1))
            wattrset (window, view->default_attributes);
        }

      mvwprintw (window, view->height - i, 0, "%s", entry->message);
    }
}

UTMessageQueueView *
ut_message_queue_view_new (UTMessageQueue *message_queue,
                           chtype background,
                           int default_attributes,
                           int cursor_attributes)
{
  UTMessageQueueView *view = g_slice_new0 (UTMessageQueueView);
  view->message_queue = message_queue;

  ut_message_queue_view_set_height (view, 100);
  view->cursor_view_pos = 5;

  view->background = background;
  view->default_attributes = default_attributes;
  view->cursor_attributes = cursor_attributes;

  return view;
}

static void
print_trace_messages_page (void)
{
  int width = screen_width;
  int height = screen_height - 4;

  main_window = subwin (stdscr, height, width, 1, 0);

  ut_message_queue_view_set_height (trace_queue_view, height);
  ut_message_queue_view_print (trace_queue_view, main_window);

  //message_window = subwin (stdscr, 2, width, height - 3, 0);

  keys_window_print ();
}

static gboolean
update_window_cb (void *data)
{
  queue_redraw_idle_id = 0;

  destroy_windows ();

  getmaxyx (stdscr, screen_height, screen_width);

  titlebar_window = subwin (stdscr, 1, screen_width, 0, 0);

  wattrset (titlebar_window, COLOR_PAIR (UT_HEADER_COLOR));
  wbkgd (titlebar_window, COLOR_PAIR (UT_HEADER_COLOR));
  werase (titlebar_window);
  mvwprintw (titlebar_window, 0, 0,
             "     UProfTool version %s       ← Page %d/%d →",
             VERSION,
             current_page, UT_PAGE_COUNT);

  switch (current_page)
    {
    case UT_PAGE_TIMERS_COUNTERS:
      print_timers_counters_page ();
      break;
    case UT_PAGE_TRACE:
      print_trace_messages_page ();
      break;
    }

  if (warning_queue->queue->head)
    {
      warning_window = subwin (stdscr,
                               UT_MESSAGE_WIN_HEIGHT,
                               screen_width,
                               screen_height - UT_MESSAGE_WIN_HEIGHT - 1,
                               0);

      ut_message_queue_view_set_height (warning_queue_view,
                                        UT_MESSAGE_WIN_HEIGHT);
      ut_message_queue_view_print (warning_queue_view,
                                   warning_window);
    }

  refresh ();

  return FALSE;
}

static void
deinit_curses (void)
{
  endwin ();
}

static void
init_curses (void)
{
  initscr ();
  nonl ();
  intrflush (stdscr, FALSE);
  keypad (stdscr, TRUE); /* enable arrow keys etc */

  cbreak (); /* don't buffer input up to \n */

  noecho ();
  curs_set (0); /* don't display the cursor */

  start_color ();
  use_default_colors ();

  init_pair (UT_DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);
  init_pair (UT_HEADER_COLOR, COLOR_WHITE, COLOR_GREEN);
  init_pair (UT_WARNING_COLOR, COLOR_YELLOW, COLOR_BLACK);
  init_pair (UT_KEY_LABEL_COLOR, COLOR_WHITE, COLOR_GREEN);

  g_atexit (deinit_curses);
}

static void
message_filter_cb (UProfReportProxy *proxy,
                   const char *context,
                   const char *categories,
                   const char *location,
                   const char *message,
                   void *user_data)
{
  char **strv = g_strsplit (categories, ",", -1);
  GList *categories_list = NULL;
  int i;

#if 0
  for (i = 0; strv[i]; i++)
    categories_list = g_list_prepend (categories_list,
                                      g_strdup (g_strstrip (strv[i])));
  g_strfreev (strv);
#endif

  ut_message_queue_append (trace_queue,
                           UT_MESSAGE_EMPHASIS_0,
                           categories_list,
                           location,
                           "%s", message);

  if (current_page == UT_PAGE_TRACE)
    queue_redraw ();
}

static void
switch_to_timers_counters_page (void)
{
  get_report_timeout_id =
    g_timeout_add_seconds (1, (GSourceFunc)get_report_cb, NULL);
}

static void
switch_from_timers_counters_page (void)
{
  g_source_remove (get_report_timeout_id);
}

static void
switch_to_trace_page (void)
{
  GError *error = NULL;
  trace_message_filter_id =
    uprof_report_proxy_add_trace_message_filter (report_proxy,
                                                 NULL,
                                                 message_filter_cb,
                                                 NULL,
                                                 &error);
  if (!trace_message_filter_id)
    {
      ut_warning (UT_WARN_LEVEL_HIGH, "Failed to enable tracing: %s",
                  error->message);
      g_error_free (error);
    }
}

static void
switch_from_trace_page (void)
{
  GError *error = NULL;
  if (trace_message_filter_id &&
      !uprof_report_proxy_remove_trace_message_filter (
                                                report_proxy,
                                                trace_message_filter_id,
                                                &error))
    {
      ut_warning (UT_WARN_LEVEL_HIGH, "Failed to disable tracing: %s",
                  error->message);
      g_error_free (error);
    }
}

static gboolean
user_input_cb (GIOChannel *channel,
               GIOCondition condition,
               void *user_data)
{
  int key = wgetch (stdscr);
  int prev_page = current_page;

  if (key == 'q')
    g_main_loop_quit (mainloop);

  if (key == KEY_RIGHT && current_page < (UT_PAGE_COUNT - 1))
    current_page++;

  if (key == KEY_LEFT && current_page > 0)
    current_page--;

  if (prev_page != current_page)
    {
      if (prev_page == UT_PAGE_TIMERS_COUNTERS)
        switch_from_timers_counters_page ();
      else if (prev_page == UT_PAGE_TRACE)
        switch_from_trace_page ();

      if (current_page == UT_PAGE_TIMERS_COUNTERS)
        switch_to_timers_counters_page ();
      else if (current_page == UT_PAGE_TRACE)
        switch_to_trace_page ();
    }

  if (current_page == UT_PAGE_TRACE)
    {
      if (key == KEY_UP)
        {
          trace_queue_view->cursor_view_pos++;
          if (trace_queue_view->cursor_view_pos >= trace_queue_view->height)
            {
              trace_queue_view->cursor_view_pos = trace_queue_view->height - 1;

              trace_queue_view->scroll_pos++;
              if (trace_queue_view->scroll_pos >=
                  trace_queue_view->message_queue->size)
                trace_queue_view->scroll_pos =
                  trace_queue_view->message_queue->size - 1;
            }
        }
      else if (key == KEY_DOWN)
        {
          trace_queue_view->cursor_view_pos--;
          if (trace_queue_view->cursor_view_pos < -1)
            trace_queue_view->cursor_view_pos = -1;
        }
    }

  queue_redraw ();

  return TRUE;
}

static void
queue_redraw (void)
{
  if (queue_redraw_idle_id == 0)
    queue_redraw_idle_id =
      g_idle_add ((GSourceFunc)update_window_cb, NULL);
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GOptionGroup *group;
  GError *error = NULL;
  GIOChannel *stdin_io_channel;
  char *report_location;

  setlocale (LC_ALL,"");

  uprof_init (&argc, &argv);

  g_print ("UProfTool %s\n", VERSION);
  g_print ("License LGPLv2.1+: GNU Lesser GPL version 2.1 or later\n\n");

  context = g_option_context_new (NULL);

  group = g_option_group_new ("uprof",
                              "UProf Options",
                              "Show UProf Options",
                              NULL,
                              NULL);

  g_option_group_set_parse_hooks (group, pre_parse_hook, post_parse_hook);
  g_option_group_add_entries (group, uprof_tool_args);
  g_option_context_set_main_group (context, group);
#if 0
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
#endif
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      return 1;
    }

  if (arg_list)
    {
      g_strfreev (list_reports ());
      return 0;
    }

  if (!arg_report_name)
    {
      g_printerr ("You need to specify a report name if not "
                  "passing -l/--list\n");
      return 1;
    }

  /* If no bus name was given then we search all of them for the first
   * one with a matching report name */
  if (!arg_bus_name)
    {
      char **names = list_reports ();
      int i;

      for (i = 0; names[i]; i++)
        {
          char **strv = g_strsplit (names[i], "@", 0);
          if (strcmp (strv[0], arg_report_name) == 0)
            {
              arg_bus_name = g_strdup (strv[1]);
              g_strfreev (strv);
              break;
            }
          g_strfreev (strv);
        }
      g_strfreev (names);

      if (!arg_bus_name)
        {
          g_printerr ("Couldn't find a report with name \"%s\" on any bus\n",
                      arg_report_name);
          return 1;
        }
    }

  report_location = g_strdup_printf ("%s@%s", arg_report_name, arg_bus_name);
  report_proxy = uprof_dbus_get_report_proxy (report_location,
                                              &error);
  if (!report_proxy)
    {
      g_printerr ("Failed to create a proxy object for report "
                  "\"%s\" on bus \"%s\": %s\n",
                  arg_report_name, arg_bus_name,
                  error->message);
      g_error_free (error);
      return 1;
    }

  init_curses ();

  warning_queue = ut_message_queue_new (UT_MAX_WARNING_COUNT);
  warning_queue_view =
    ut_message_queue_view_new (warning_queue,
                               COLOR_PAIR (UT_WARNING_COLOR), /* background */
                               COLOR_PAIR (UT_WARNING_COLOR), /* default */
                               0); /* cursor */

  trace_queue = ut_message_queue_new (UT_TRACE_LOG_SIZE);
  trace_queue_view =
    ut_message_queue_view_new (trace_queue,
                               COLOR_PAIR (UT_DEFAULT_COLOR), /* background */
                               COLOR_PAIR (UT_DEFAULT_COLOR), /* default */
                               COLOR_PAIR (UT_WARNING_COLOR)); /* cursor */

  mainloop = g_main_loop_new (NULL, FALSE);

  stdin_io_channel = g_io_channel_unix_new (0);
  g_io_add_watch_full (stdin_io_channel,
                       G_PRIORITY_HIGH,
                       G_IO_IN,
                       user_input_cb,
                       NULL,
                       NULL);

  switch_to_timers_counters_page ();

  queue_redraw ();

  g_main_loop_run (mainloop);

  g_io_channel_unref (stdin_io_channel);

  return 0;
}

