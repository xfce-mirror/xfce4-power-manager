/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.

 *
 * XXX - since this program is executed with root permissions, it may not
 *       be a good idea to trust glib!!
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

/* XXX */
#ifdef UP_BACKEND_SUSPEND_COMMAND
#undef UP_BACKEND_SUSPEND_COMMAND
#endif
#ifdef UP_BACKEND_HIBERNATE_COMMAND
#undef UP_BACKEND_HIBERNATE_COMMAND
#endif


#ifdef BACKEND_TYPE_FREEBSD
#define UP_BACKEND_SUSPEND_COMMAND "/usr/sbin/zzz"
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/sbin/acpiconf -s 4"
#endif
#ifdef BACKEND_TYPE_LINUX
#define UP_BACKEND_SUSPEND_COMMAND "/usr/sbin/pm-suspend"
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/sbin/pm-hibernate"
#endif
#ifdef BACKEND_TYPE_OPENBSD
#define UP_BACKEND_SUSPEND_COMMAND	"/usr/sbin/zzz"
#define UP_BACKEND_HIBERNATE_COMMAND "/dev/null"
#endif


static gboolean
run (const gchar *command)
{
#if defined(HAVE_SIGPROCMASK)
  sigset_t sigset;
#endif
  gboolean result;
  gchar **argv;
  gchar **envp;
  GError *err;
  gint status;
  gint argc;

#if defined(HAVE_SETSID)
  setsid ();
#endif

#if defined (HAVE_SIGPROCMASK)
  sigemptyset (&sigset);
  sigaddset (&sigset, SIGHUP);
  sigaddset (&sigset, SIGINT);
  sigprocmask (SIG_BLOCK, &sigset, NULL);
#endif

  result = g_shell_parse_argv (command, &argc, &argv, &err);

  if (result)
    {
      envp = g_new0 (gchar *, 1);

      result = g_spawn_sync (NULL, argv, envp,
                             G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                             G_SPAWN_STDERR_TO_DEV_NULL,
                             NULL, NULL, NULL, NULL, &status, &err);

      g_strfreev (envp);
      g_strfreev (argv);
    }

  if (!result)
    {
      g_error_free (err);
      return FALSE;
    }

  return (WIFEXITED (status) && WEXITSTATUS (status) == 0);
}


int
main (int argc, char **argv)
{
  gboolean succeed = FALSE;
  char action[1024];

  /* display banner */
  fprintf (stdout, "XFPM_SUDO_DONE ");
  fflush (stdout);

  if (fgets (action, 1024, stdin) == NULL)
    {
      fprintf (stdout, "FAILED\n");
      return EXIT_FAILURE;
    }

  if (strncasecmp (action, "SUSPEND", 7) == 0)
    {
      succeed = run (UP_BACKEND_SUSPEND_COMMAND);
    }
  else if (strncasecmp (action, "HIBERNATE", 9) == 0)
    {
      succeed = run (UP_BACKEND_HIBERNATE_COMMAND);
    }

  if (succeed)
    {
      fprintf (stdout, "SUCCEED\n");
      return EXIT_SUCCESS;
    }

  fprintf (stdout, "FAILED\n");
  return EXIT_FAILURE;
}
