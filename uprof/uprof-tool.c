#include <config.h>

#include <uprof.h>

#include <glib.h>
//#include <glib/gi18n-lib.h>

#include <stdio.h>

static gboolean arg_list = FALSE;
static char *arg_bus_name = NULL;
static char *arg_report_name = NULL;
static char **arg_remaining = NULL;

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

  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &arg_remaining,
    "COMMAND", NULL },
  { NULL, },
};

static void
list_reports (void)
{
  char **names;

  g_print ("Searching via session bus for "
           "org.freedesktop.UProf.Service objects...\n");
  names = uprof_dbus_list_reports ();
  if (!names)
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
  g_strfreev (names);
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GOptionGroup *group;
  GError *error = NULL;
  //GMainLoop *mainloop;

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

  if (!arg_report_name)
    arg_list = TRUE;

  if (arg_list)
    list_reports ();

  if (arg_report_name)
    {
      char *report = uprof_dbus_get_text_report (arg_bus_name,
                                                 arg_report_name);
      printf ("%s", report);
      g_free (report);
    }

  return 0;
}

