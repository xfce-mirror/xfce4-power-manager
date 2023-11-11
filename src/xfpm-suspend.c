/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfpm-common.h"
#include "xfpm-debug.h"
#include "xfpm-suspend.h"



#ifdef BACKEND_TYPE_FREEBSD
static gchar *
get_string_sysctl (GError **err, const gchar *format, ...)
{
  va_list args;
  gchar *name;
  size_t value_len;
  gchar *str = NULL;

  g_return_val_if_fail(format != NULL, FALSE);

  va_start (args, format);
  name = g_strdup_vprintf (format, args);
  va_end (args);

  if (sysctlbyname (name, NULL, &value_len, NULL, 0) == 0)
  {
    str = g_new (char, value_len + 1);

    if (sysctlbyname (name, str, &value_len, NULL, 0) == 0)
      str[value_len] = 0;
    else {
      g_free (str);
      str = NULL;
    }
  }

  if (!str)
    g_set_error (err, 0, 0, "%s", g_strerror(errno));

  g_free(name);

  return str;
}

static gboolean
freebsd_supports_sleep_state (const gchar *state)
{
  gboolean ret = FALSE;
  gchar *sleep_states;

  sleep_states = get_string_sysctl (NULL, "hw.acpi.supported_sleep_state");

  if (sleep_states != NULL) {
    if (strstr (sleep_states, state) != NULL)
      ret = TRUE;
  }

  g_free (sleep_states);

  return ret;
}
#endif

#ifdef BACKEND_TYPE_LINUX
static gboolean
linux_supports_sleep_state (const gchar *state)
{
  gchar *command;
  GError *error = NULL;
  gint status;

  /* run script from pm-utils */
  command = g_strdup_printf ("/usr/bin/pm-is-supported --%s", state);
  XFPM_DEBUG ("Executing command: %s", command);

  if (!g_spawn_command_line_sync (command, NULL, NULL, &status, &error)
      || !g_spawn_check_wait_status (status, &error))
  {
    g_warning ("Failed to run script: %s", error->message);
    g_error_free (error);
    g_free (command);
    return FALSE;
  }

  g_free (command);
  return TRUE;
}
#endif


gboolean
xfpm_suspend_can_suspend (void)
{
#ifdef BACKEND_TYPE_FREEBSD
  return freebsd_supports_sleep_state ("S3");
#endif
#ifdef BACKEND_TYPE_LINUX
  return linux_supports_sleep_state ("suspend");
#endif
#ifdef BACKEND_TYPE_OPENBSD
  return TRUE;
#endif

  return FALSE;
}

gboolean
xfpm_suspend_can_hibernate (void)
{
#ifdef BACKEND_TYPE_FREEBSD
  return freebsd_supports_sleep_state ("S4");
#endif
#ifdef BACKEND_TYPE_LINUX
  return linux_supports_sleep_state ("hibernate");
#endif
#ifdef BACKEND_TYPE_OPENBSD
  return TRUE;
#endif

  return FALSE;
}

gboolean
xfpm_suspend_try_action (XfpmActionType type)
{
  const gchar *action;
  GError *error = NULL;
  gint status;
  gchar *command = NULL;

  if (type == XFPM_SUSPEND)
    action = "suspend";
  else if (type == XFPM_HIBERNATE)
    action = "hibernate";
  else
    return FALSE;

  command = g_strdup_printf ("pkexec " SBINDIR "/xfce4-pm-helper --%s", action);
  XFPM_DEBUG ("Executing command: %s", command);

  if (!g_spawn_command_line_sync (command, NULL, NULL, &status, &error)
      || !g_spawn_check_wait_status (status, &error))
  {
    g_warning ("Failed to suspend/hibernate: %s", error->message);
    g_error_free (error);
    g_free (command);
    return FALSE;
  }

  g_free (command);
  return TRUE;
}
