#include <config.h>

#include <uprof.h>

#include <glib.h>
/* #include <glib/gi18n-lib.h> */

#include <stdio.h>
#include <string.h>
#include <ncursesw/ncurses.h>
#include <locale.h>

#define UPROF_MESSAGE_WIN_HEIGHT 3

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

static gboolean arg_list = FALSE;
static gboolean arg_zero = FALSE;
static char *arg_bus_name = NULL;
static char *arg_report_name = NULL;
static char **arg_remaining = NULL;

static GMainLoop *mainloop;

static WINDOW *titlebar_window;
static WINDOW *main_window;
static WINDOW *message_window;
static WINDOW *keys_window;

static KeyHandler keys[] = {
  { 'q', 'Q', "Q - Quit" },
};

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

  if (message_window)
    {
      delwin (message_window);
      message_window = NULL;
    }

  if (keys_window)
    {
      delwin (keys_window);
      keys_window = NULL;
    }
}

static void
create_windows (void)
{
  int maxx, maxy;
  getmaxyx (stdscr, maxy, maxx);

  titlebar_window = subwin (stdscr, 1, maxx, 0, 0);
  main_window = subwin (stdscr, maxy - UPROF_MESSAGE_WIN_HEIGHT - 1, maxx,
                        1, 0);
  message_window = subwin (stdscr, UPROF_MESSAGE_WIN_HEIGHT, maxx,
                           maxy - UPROF_MESSAGE_WIN_HEIGHT - 1, 0);
  keys_window = subwin (stdscr, 1, maxx, maxy - 1, 0);
}

gboolean
update_window_cb (void *data)
{
  char *report;
  GError *error;
  int message_line = 0;
  int i;

  destroy_windows ();
  create_windows ();

  wattrset(titlebar_window, COLOR_PAIR(UT_HEADER_COLOR));
  wbkgd(titlebar_window, COLOR_PAIR(UT_HEADER_COLOR));
  werase(titlebar_window);
  mvwprintw (titlebar_window, 0, 0, "     UProfTool version %s", VERSION);

  wattrset(message_window, COLOR_PAIR(UT_WARNING_COLOR));
  wbkgd(message_window, COLOR_PAIR(UT_WARNING_COLOR));
  werase(message_window);

  error = NULL;
  report = uprof_dbus_get_text_report (arg_bus_name, arg_report_name, &error);
  if (!report)
    {
      mvwprintw (message_window, message_line++, 0,
                 "Failed to fetch report: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      werase(main_window);
      mvwprintw (main_window, 0, 0, "%s", report);
      g_free (report);
    }

  error = NULL;
  if (!uprof_dbus_reset_report (arg_bus_name, arg_report_name, &error))
    {
      mvwprintw (message_window, message_line++, 0,
                 "Failed to zero report statistics: %s",
                 error->message);
      g_error_free (error);
    }

  wattrset(keys_window, COLOR_PAIR(UT_KEY_LABEL_COLOR));
  wbkgd(keys_window, COLOR_PAIR(UT_KEY_LABEL_COLOR));
  werase(keys_window);
  wmove (keys_window, 0, 1);
  for (i = 0; i < G_N_ELEMENTS (keys); i++)
    wprintw (keys_window, "%s", keys[i].desc);

  refresh ();

  return TRUE;
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
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE); /* enable arrow keys etc */

  cbreak(); /* don't buffer input up to \n */

  noecho();
  curs_set(0); /* don't display the cursor */

  start_color();
  use_default_colors();

  init_pair(UT_DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);
  init_pair(UT_HEADER_COLOR, COLOR_WHITE, COLOR_GREEN);
  init_pair(UT_WARNING_COLOR, COLOR_YELLOW, COLOR_BLACK);
  init_pair(UT_KEY_LABEL_COLOR, COLOR_WHITE, COLOR_GREEN);

  g_atexit (deinit_curses);
}

gboolean
user_input_cb (GIOChannel *channel,
               GIOCondition condition,
               void *user_data)
{
#if 0
  GError *error = NULL;
  char character;

  while (status = g_io_channel_read_chars (channel,
                                           &character,
                                           1,
                                           &read,
                                           &error)
         == G_IO_STATUS_AGAIN)
    ;

  if (status == G_IO_STATUS_ERROR)
    g_critical ("Failed to read user input: %s", error->message);

  if (status == G_IO_STATUS_NORMAL)
    {
      if (
    }
#endif
  int key = wgetch (stdscr);
  if (key == 'q')
    g_main_loop_quit (mainloop);

  return TRUE;
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GOptionGroup *group;
  GError *error = NULL;
  GIOChannel *stdin_io_channel;

  setlocale(LC_ALL,"");

  uprof_init (&argc, &argv);

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
      g_error ("%s", error->message);
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
      g_warning ("You need to specify a report name if not passing -l/--list");
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
          g_warning ("Couldn't find a report with name \"%s\" on any bus",
                     arg_report_name);
          return 1;
        }
    }

  init_curses ();

  mainloop = g_main_loop_new (NULL, FALSE);

  stdin_io_channel = g_io_channel_unix_new (0);
  g_io_add_watch (stdin_io_channel,
                  G_IO_IN,
                  user_input_cb,
                  NULL);

  g_timeout_add_seconds (1, update_window_cb, NULL);

  g_main_loop_run (mainloop);

  g_io_channel_unref (stdin_io_channel);

  return 0;
}

