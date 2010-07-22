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

  /* relative to the bottom of the screen */
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

typedef enum
{
  UT_OPTION_TYPE_BOOLEAN
} UTOptionType;

typedef struct
{
  UTOptionType type;
  char *context;
  char *name;
  char *name_formatted;
  char *description;
  gboolean known_value;
  union
    {
      gboolean boolean_value;
    };
} UTOption;

typedef struct
{
  char *name;
  GList *options;
} UTOptionGroup;

static gboolean arg_list = FALSE;
static gboolean arg_zero = FALSE;
static char *arg_bus_name = NULL;
static char *arg_report_name = NULL;
static char **arg_remaining = NULL;

static GMainLoop *mainloop;

static UProfReportProxy *report_proxy;

#define OPTION_GROUP_HEADER_SIZE 3
static GList *option_groups;
/* relative to the top of the screen */
static int options_view_scroll_pos;
/* static int options_view_cursor_pos; */
static int options_view_height;
static GList *current_option_group_link;
static GList *current_option_link;

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

static WINDOW *details_window;

typedef enum
{
  UT_PAGE_TIMERS_COUNTERS,
  UT_PAGE_OPTIONS,
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

static int
utf8_width (const char *utf8_string)
{
  glong n_chars;
  gunichar *ucs4_string = g_utf8_to_ucs4_fast (utf8_string, -1, &n_chars);
  int i;
  int len = 0;

  for (i = 0; i < n_chars; i++)
    {
      if (g_unichar_iswide (ucs4_string[i]))
        len += 2;
      else if (g_unichar_iszerowidth (ucs4_string[i]))
        continue;
      else
        len++;
    }

  g_free (ucs4_string);

  return len;
}

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

  if ((entry->location &&
       strcmp (entry->location, location) != 0)
      || (entry->location == NULL && location == NULL))
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
  if (timers_counters_report)
    mvwprintw (main_window, 0, 0, "%s", timers_counters_report);

  keys_window_print ();
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
print_trace_messages_page (void)
{
  int width = screen_width;
  int height = screen_height - 4;

  main_window = subwin (stdscr, height, width, 1, 0);

  ut_message_queue_view_set_height (trace_queue_view, height);
  ut_message_queue_view_print (trace_queue_view, main_window);

  details_window = subwin (stdscr, 2, width, height - 3, 0);

  keys_window_print ();
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

  for (i = 0; strv[i]; i++)
    categories_list = g_list_prepend (categories_list,
                                      g_strdup (g_strstrip (strv[i])));
  g_strfreev (strv);

  ut_message_queue_append (trace_queue,
                           UT_MESSAGE_EMPHASIS_0,
                           categories_list,
                           location,
                           "%s", message);

  if (current_page == UT_PAGE_TRACE)
    queue_redraw ();
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

static void
handle_trace_messages_page_input (int key)
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

static void
print_option_group_header (UTOptionGroup *group, int *line)
{
  (*line)++;

  if (*line >= 0 && *line < options_view_height)
    mvwprintw (main_window, *line, 0, "%s", group->name);
  (*line)++;

  if (*line >= 0 && *line < options_view_height)
    {
      int i;
      int len = strlen (group->name);
      GString *underline = g_string_new ("");

      for (i = 0; i < len; i++)
        g_string_append_c (underline, '-');
      mvwprintw (main_window, *line, 0, "%s", underline->str);
      g_string_free (underline, TRUE);
    }
}

static void
print_option (UTOption *option, int name_field_width, int line)
{
  gboolean value;
  GError *error = NULL;
  int max_description_size;
  gboolean elipsize = FALSE;

  if (!uprof_report_proxy_get_boolean_option (report_proxy,
                                              option->context,
                                              option->name,
                                              &value,
                                              &error))
    {
      ut_warning (UT_WARN_LEVEL_HIGH,
                  "Failed to get value of option \"%s\": %s",
                  option->name,
                  error->message);
      g_error_free (error);
    }
  option->known_value = TRUE;
  option->boolean_value = value;

  if (current_option_link && option == current_option_link->data)
    {
      wattrset (main_window, COLOR_PAIR (UT_WARNING_COLOR));
      mvwprintw (details_window, 0, 0, "%s", option->description);
    }

  max_description_size = screen_width - name_field_width - 12;
  if (utf8_width (option->description) >= max_description_size)
    elipsize = TRUE;
  if (elipsize)
    {
      mvwprintw (main_window, line, 0, "%-5s | %-*s | %-.*s...",
                 value ? "TRUE" : "FALSE",
                 name_field_width, option->name_formatted,
                 max_description_size - 3, option->description);
    }
  else
    mvwprintw (main_window, line, 0, "%-5s | %-*s | %-*s",
               value ? "TRUE" : "FALSE",
               name_field_width, option->name_formatted,
               max_description_size, option->description);

  if (current_option_link && option == current_option_link->data)
    wattrset (main_window, COLOR_PAIR (UT_DEFAULT_COLOR));
}

static void
print_options (GList *options, int name_field_width, int *line)
{
  GList *l;
  for (l = options; l; l = l->next)
    {
      if (*line >= 0 && *line < options_view_height)
        print_option (l->data, name_field_width, *line);
      (*line)++;
    }
}

static void
print_options_view (void)
{
  GList *l;
  GList *l2;
  int max_name_width = 0;
  int pos;
  GList *first_group_link = NULL;
  int line = 0;

  options_view_height = screen_height - 4;

  main_window = subwin (stdscr, options_view_height, screen_width, 1, 0);
  werase (main_window);

  details_window = subwin (stdscr, 2, screen_width, screen_height - 3, 0);
  wattrset (details_window, COLOR_PAIR (UT_WARNING_COLOR));
  wbkgd (details_window, COLOR_PAIR (UT_WARNING_COLOR));
  werase (details_window);

  for (l = option_groups; l; l = l->next)
    {
      UTOptionGroup *group = l->data;
      for (l2 = group->options; l2; l2 = l2->next)
        {
          UTOption *option = l2->data;
          max_name_width = MAX (max_name_width,
                                utf8_width (option->name_formatted));
        }
    }

  for (l = option_groups, pos = 0; l; l = l->next)
    {
      UTOptionGroup *group = l->data;
      int group_height =
        g_list_length (group->options) + OPTION_GROUP_HEADER_SIZE;
      if ((pos + group_height) > options_view_scroll_pos)
        {
          first_group_link = l;
          break;
        }
      pos += group_height;
    }

  line = pos - options_view_scroll_pos;
  for (l = first_group_link; l && line < options_view_height; l = l->next)
    {
      UTOptionGroup *group = l->data;
      print_option_group_header (group, &line);
      print_options (group->options, max_name_width, &line);
    }
}

static int
get_position_of_option (GList *groups, UTOption *option)
{
  int pos = 0;
  GList *l;
  GList *l2;

  /* Special case the first option so we always scroll up
   * a bit more to see the first group header... */
  if (groups)
    {
      UTOptionGroup *group = groups->data;
      if (group->options->data == option)
        return 0;
    }

  for (l = groups; l; l = l->next)
    {
      UTOptionGroup *group = l->data;
      pos += OPTION_GROUP_HEADER_SIZE;
      for (l2 = group->options; l2; l2 = l2->next)
        {
          if (l2->data == option)
            return pos;
          pos++;
        }
    }

  return pos;
}

static void
update_option_view_scroll_pos (void)
{
  int pos;

  if (current_option_link)
    pos = get_position_of_option (option_groups, current_option_link->data);
  else
    pos = 0;

  /* If we are still in view, there's no need to scroll */
  if (pos >= options_view_scroll_pos &&
      pos < (options_view_scroll_pos + options_view_height))
    return;

  /* scrolling up or down? */
  if (pos < options_view_scroll_pos)
    options_view_scroll_pos = pos;
  else
    options_view_scroll_pos = pos - options_view_height + 1;
}

static void
print_options_page (void)
{
  print_options_view ();
  keys_window_print ();
}

static gboolean
add_option_cb (UProfReportProxy *proxy,
               const char *context,
               UProfReportProxyOption *proxy_option,
               void *user_data)
{
  GList **option_groups = user_data;
  UTOptionGroup *group;
  UTOption *option;
  GList *l;

  for (l = *option_groups; l; l = l->next)
    {
      group = l->data;
      if (strcmp (group->name, proxy_option->group) == 0)
        break;
    }
  if (!l)
    {
      group = g_slice_new0 (UTOptionGroup);
      group->name = g_strdup (proxy_option->group);
      *option_groups = g_list_prepend (*option_groups, group);
    }

  option = g_slice_new0 (UTOption);

  option->type = UT_OPTION_TYPE_BOOLEAN;
  option->context = g_strdup (proxy_option->context);
  option->name = g_strdup (proxy_option->name);
  option->name_formatted = g_strdup (proxy_option->name_formatted);
  option->description = g_strdup (proxy_option->description);
  option->known_value = FALSE;
  option->boolean_value = FALSE;

  group->options = g_list_prepend (group->options, option);
  return TRUE;
}

static void
free_options (GList *options)
{
  GList *l;

  for (l = options; l; l = l->next)
    {
      UTOption *option = l->data;

      g_free (option->context);
      g_free (option->name);
      g_free (option->name_formatted);
      g_free (option->description);
      g_slice_free (UTOption, option);
    }
  g_list_free (options);
}

static void
free_option_groups (GList *groups)
{
  GList *l;

  for (l = groups; l; l = l->next)
    {
      UTOptionGroup *group = l->data;
      g_free (group->name);
      free_options (group->options);
    }
  g_list_free (option_groups);
}

static void
update_option_groups (void)
{
  char *current_group = NULL;
  char *current_context = NULL;
  char *current_name = NULL;
  GError *error = NULL;

  if (current_option_group_link && current_option_link)
    {
      UTOptionGroup *group = current_option_group_link->data;
      UTOption *option = current_option_link->data;

      current_group = g_strdup (group->name);
      current_context = g_strdup (option->context);
      current_name = g_strdup (option->name);
    }

  current_option_group_link = NULL;
  current_option_link = NULL;

  free_option_groups (option_groups);
  option_groups = NULL;
  if (!uprof_report_proxy_foreach_option (report_proxy,
                                          NULL, /* all contexts */
                                          add_option_cb,
                                          &option_groups,
                                          &error))
    {
      ut_warning (UT_WARN_LEVEL_HIGH, "Failed to fetch list of options: %s\n",
                  error->message);
      return;
    }

  /*
   * Find the option that had focus before the update...
   */

  if (current_group)
    {
      GList *l;

      for (l = option_groups; l; l = l->next)
        {
          UTOptionGroup *group = l->data;
          if (strcmp (group->name, current_group) == 0)
            {
              current_option_group_link = l;
              break;
            }
        }
    }
  else
    current_option_group_link = option_groups;

  if (current_option_group_link)
    {
      UTOptionGroup *group = current_option_group_link->data;
      if (current_name)
        {
          GList *l;
          for (l = group->options; l; l = l->next)
            {
              UTOption *option = l->data;
              if (strcmp (option->name, current_name) == 0 &&
                  strcmp (option->context, current_context) == 0)
                {
                  current_option_link = l;
                  break;
                }
            }
        }
      else
        current_option_link = group->options;
    }
}

static void
switch_to_options_page (void)
{
  update_option_groups ();
}

static void
switch_from_options_page (void)
{

}

static void
handle_options_page_input (int key)
{
  if (key == KEY_UP &&
      current_option_group_link && current_option_link)
    {
      if (current_option_link->prev)
        current_option_link = current_option_link->prev;
      else if (current_option_group_link->prev)
        {
          UTOptionGroup *group;
          current_option_group_link = current_option_group_link->prev;
          group = current_option_group_link->data;
          current_option_link = g_list_last (group->options);
        }
      update_option_view_scroll_pos ();
    }
  else if (key == KEY_DOWN &&
           current_option_group_link && current_option_link)
    {
      if (current_option_link->next)
        current_option_link = current_option_link->next;
      else if (current_option_group_link->next)
        {
          UTOptionGroup *group;
          current_option_group_link = current_option_group_link->next;
          group = current_option_group_link->data;
          current_option_link = group->options;
        }
      update_option_view_scroll_pos ();
    }
  else if ((key == '\r' || key == KEY_BACKSPACE ||
            key == ' ' || key == '\t') &&
           current_option_link)
    {
      UTOption *option = current_option_link->data;
      if (option->type == UT_OPTION_TYPE_BOOLEAN &&
          option->known_value)
        {
          gboolean value = !option->boolean_value;
          GError *error = NULL;

          if (!uprof_report_proxy_set_boolean_option (report_proxy,
                                                      option->context,
                                                      option->name,
                                                      value,
                                                      &error))
            {
              ut_warning (UT_WARN_LEVEL_HIGH,
                          "Failed to set value of option \"%s\": %s",
                          option->name,
                          error->message);
              g_error_free (error);
            }
        }

      /* Changing an option could potentially add new options... */
      update_option_groups ();
    }
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

  if (details_window)
    {
      delwin (details_window);
      details_window = NULL;
    }

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
    case UT_PAGE_OPTIONS:
      print_options_page ();
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

static gboolean
user_input_cb (GIOChannel *channel,
               GIOCondition condition,
               void *user_data)
{
  int key = wgetch (stdscr);
  int prev_page = current_page;

  if (key == 'q' || key == 'Q')
    g_main_loop_quit (mainloop);

  if (key == KEY_RIGHT && current_page < (UT_PAGE_COUNT - 1))
    current_page++;

  if (key == KEY_LEFT && current_page > 0)
    current_page--;

  if (prev_page != current_page)
    {
      if (prev_page == UT_PAGE_TIMERS_COUNTERS)
        switch_from_timers_counters_page ();
      else if (prev_page == UT_PAGE_OPTIONS)
        switch_from_options_page ();
      else if (prev_page == UT_PAGE_TRACE)
        switch_from_trace_page ();

      if (current_page == UT_PAGE_TIMERS_COUNTERS)
        switch_to_timers_counters_page ();
      else if (current_page == UT_PAGE_OPTIONS)
        switch_to_options_page ();
      else if (current_page == UT_PAGE_TRACE)
        switch_to_trace_page ();
    }

  if (current_page == UT_PAGE_TRACE)
    handle_trace_messages_page_input (key);
  else if (current_page == UT_PAGE_OPTIONS)
    handle_options_page_input (key);

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

  free_option_groups (option_groups);

  return 0;
}

