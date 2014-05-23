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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfpm-common.h"
#include "xfpm-debug.h"
#include "xfpm-suspend.h"


static void xfpm_suspend_sudo_free (XfpmSuspend *suspend);

static void xfpm_suspend_sudo_childwatch (GPid     pid,
                                          gint     status,
                                          gpointer data);

static gboolean xfpm_suspend_sudo_init (XfpmSuspend *suspend,
                                        GError   **error);


#define XFPM_SUSPEND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_SUSPEND, XfpmSuspendPrivate))

struct XfpmSuspendPrivate
{
    /* sudo helper */
    HelperState     helper_state;
    pid_t           helper_pid;
    FILE           *helper_infile;
    FILE           *helper_outfile;
    guint           helper_watchid;
    gboolean        helper_require_password;
};


G_DEFINE_TYPE (XfpmSuspend, xfpm_suspend, G_TYPE_OBJECT)

static void
xfpm_suspend_init (XfpmSuspend *suspend)
{
    GError *error = NULL;

    suspend->priv = XFPM_SUSPEND_GET_PRIVATE (suspend);
    suspend->priv->helper_state = SUDO_NOT_INITIAZED;
    suspend->priv->helper_require_password = FALSE;
    suspend->priv->helper_infile = NULL;
    suspend->priv->helper_outfile = NULL;
    suspend->priv->helper_pid = 0;
    suspend->priv->helper_watchid = 0;

    if (!xfpm_suspend_sudo_init (suspend, &error))
    {
        g_warning ("xfpm_suspend_sudo_init failed : %s", error->message);
        g_error_free (error);
    }
}

static void
xfpm_suspend_finalize (GObject *object)
{
    XfpmSuspend *suspend = XFPM_SUSPEND (object);

    /* close down helper */
    xfpm_suspend_sudo_free (suspend);

    G_OBJECT_CLASS (xfpm_suspend_parent_class)->finalize (object);
}

static void
xfpm_suspend_class_init (XfpmSuspendClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_suspend_finalize;

    g_type_class_add_private (klass, sizeof (XfpmSuspendPrivate));
}

XfpmSuspend *
xfpm_suspend_get (void)
{
    static gpointer xfpm_suspend_object = NULL;

    if ( G_LIKELY (xfpm_suspend_object != NULL ) )
    {
        g_object_ref (xfpm_suspend_object);
    }
    else
    {
        xfpm_suspend_object = g_object_new (XFPM_TYPE_SUSPEND, NULL);
        g_object_add_weak_pointer (xfpm_suspend_object, &xfpm_suspend_object);
    }

    return XFPM_SUSPEND (xfpm_suspend_object);
}

static void
xfpm_suspend_sudo_free (XfpmSuspend *suspend)
{
    gint status;

    /* close down helper */
    if (suspend->priv->helper_infile != NULL)
    {
        fclose (suspend->priv->helper_infile);
        suspend->priv->helper_infile = NULL;
    }

    if (suspend->priv->helper_outfile != NULL)
    {
        fclose (suspend->priv->helper_outfile);
        suspend->priv->helper_outfile = NULL;
    }

    if (suspend->priv->helper_watchid > 0)
    {
        g_source_remove (suspend->priv->helper_watchid);
        suspend->priv->helper_watchid = 0;
    }

    if (suspend->priv->helper_pid > 0)
    {
        waitpid (suspend->priv->helper_pid, &status, 0);
        suspend->priv->helper_pid = 0;
    }

    /* reset state */
    suspend->priv->helper_state = SUDO_NOT_INITIAZED;
}

static void
xfpm_suspend_sudo_childwatch (GPid     pid,
                              gint     status,
                              gpointer data)
{
    /* close down sudo stuff */
    xfpm_suspend_sudo_free (XFPM_SUSPEND (data));
}

static gboolean
xfpm_suspend_sudo_init (XfpmSuspend *suspend,
                        GError   **error)
{
    gchar  *cmd;
    struct  rlimit rlp;
    gchar   buf[15];
    gint    parent_pipe[2];
    gint    child_pipe[2];
    gint    n;

    /* return state if we succeeded */
    if (suspend->priv->helper_state != SUDO_NOT_INITIAZED)
        return suspend->priv->helper_state == SUDO_AVAILABLE;

    g_return_val_if_fail (suspend->priv->helper_infile == NULL, FALSE);
    g_return_val_if_fail (suspend->priv->helper_outfile == NULL, FALSE);
    g_return_val_if_fail (suspend->priv->helper_watchid == 0, FALSE);
    g_return_val_if_fail (suspend->priv->helper_pid == 0, FALSE);

    /* assume it won't work for now */
    suspend->priv->helper_state = SUDO_FAILED;

    cmd = g_find_program_in_path ("sudo");
    if (G_UNLIKELY (cmd == NULL))
    {
        g_set_error_literal (error, 1, 0,
                             "The program \"sudo\" was not found");
        return FALSE;
    }

    if (pipe (parent_pipe) == -1)
    {
        g_set_error (error, 1, 0,
                     "Unable to create parent pipe: %s",
                     strerror (errno));
        goto err0;
    }

    if (pipe (child_pipe) == -1)
    {
        g_set_error (error, 1, 0,
                     "Unable to create child pipe: %s",
                     strerror (errno));
        goto err1;
    }

    suspend->priv->helper_pid = fork ();
    if (suspend->priv->helper_pid < 0)
    {
        g_set_error (error, 1, 0,
                     "Unable to fork sudo helper: %s",
                     strerror (errno));
        goto err2;
    }
    else if (suspend->priv->helper_pid == 0)
    {
        /* setup signals */
        signal (SIGPIPE, SIG_IGN);

        /* setup environment */
        g_setenv ("LC_ALL", "C", TRUE);
        g_setenv ("LANG", "C", TRUE);
        g_setenv ("LANGUAGE", "C", TRUE);

        /* setup the 3 standard file handles */
        dup2 (child_pipe[0], STDIN_FILENO);
        dup2 (parent_pipe[1], STDOUT_FILENO);
        dup2 (parent_pipe[1], STDERR_FILENO);

        /* close all other file handles */
        getrlimit (RLIMIT_NOFILE, &rlp);
        for (n = 0; n < (gint) rlp.rlim_cur; ++n)
        {
            if (n != STDIN_FILENO && n != STDOUT_FILENO && n != STDERR_FILENO)
                close (n);
        }

        /* execute sudo with the helper */
        execl (cmd, "sudo", "-H", "-S", "-p",
               "XFPM_SUDO_PASS ", "--",
               XFPM_SUSPEND_HELPER_CMD, NULL);

        g_free (cmd);
        cmd = NULL;

        _exit (127);
    }
    else
    {
        /* watch the sudo helper */
        suspend->priv->helper_watchid = g_child_watch_add (suspend->priv->helper_pid,
                                                           xfpm_suspend_sudo_childwatch,
                                                           suspend);
    }

    /* read sudo/helper answer */
    n = read (parent_pipe[0], buf, sizeof (buf));
    if (n < 15)
    {
        g_set_error (error, 1, 0,
                     "Unable to read response from sudo helper: %s",
                     n < 0 ? strerror (errno) : "Unknown error");
        goto err2;
    }

    /* open pipe to receive replies from sudo */
    suspend->priv->helper_infile = fdopen (parent_pipe[0], "r");
    if (suspend->priv->helper_infile == NULL)
    {
        g_set_error (error, 1, 0,
                     "Unable to open parent pipe: %s",
                     strerror (errno));
        goto err2;
    }
    close (parent_pipe[1]);

    /* open pipe to send passwords to sudo */
    suspend->priv->helper_outfile = fdopen (child_pipe[1], "w");
    if (suspend->priv->helper_outfile == NULL)
    {
        g_set_error (error, 1, 0,
                     "Unable to open parent pipe: %s",
                     strerror (errno));
        goto err2;
    }
    close (child_pipe[0]);

    /* check if NOPASSWD is set in /etc/sudoers */
    if (memcmp (buf, "XFPM_SUDO_PASS ", 15) == 0)
    {
        suspend->priv->helper_require_password = TRUE;
    }
    else if (memcmp (buf, "XFPM_SUDO_DONE ", 15) == 0)
    {
        suspend->priv->helper_require_password = FALSE;
    }
    else
    {
        g_set_error (error, 1, 0,
                     "Got unexpected reply from sudo pm helper");
        goto err2;
    }

    XFPM_DEBUG ("suspend->priv->helper_require_password %s",
                suspend->priv->helper_require_password ? "Required" : "Not required");

    /* if we try again */
    suspend->priv->helper_state = SUDO_AVAILABLE;

    return TRUE;

err2:
    xfpm_suspend_sudo_free (suspend);

    close (child_pipe[0]);
    close (child_pipe[1]);

err1:
    close (parent_pipe[0]);
    close (parent_pipe[1]);

err0:
    g_free (cmd);

    suspend->priv->helper_pid = 0;

    return FALSE;
}

gboolean
xfpm_suspend_sudo_try_action (XfpmSuspend       *suspend,
                              XfpmActionType     type,
                              GError           **error)
{
    const gchar *action;
    gchar        reply[256];

    TRACE("entering");

    g_return_val_if_fail (suspend->priv->helper_state == SUDO_AVAILABLE, FALSE);
    g_return_val_if_fail (suspend->priv->helper_outfile != NULL, FALSE);
    g_return_val_if_fail (suspend->priv->helper_infile != NULL, FALSE);

    /* the command we send to sudo */
    if (type == XFPM_SUSPEND)
        action = "SUSPEND";
    else if (type == XFPM_HIBERNATE)
        action = "HIBERNATE";
    else
        return FALSE;

    /* write action to sudo helper */
    if (fprintf (suspend->priv->helper_outfile, "%s\n", action) > 0)
        fflush (suspend->priv->helper_outfile);

    /* check if the write succeeded */
    if (ferror (suspend->priv->helper_outfile) != 0)
    {
        /* probably succeeded but the helper got killed */
        if (errno == EINTR)
            return TRUE;

        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                     _("Error sending command to pm helper: %s"),
                     strerror (errno));
        return FALSE;
    }

    /* get responce from sudo helper */
    if (fgets (reply, sizeof (reply), suspend->priv->helper_infile) == NULL)
    {
        /* probably succeeded but the helper got killed */
        if (errno == EINTR)
            return TRUE;

        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                     _("Error receiving response from pm helper: %s"),
                     strerror (errno));
        return FALSE;
    }

    if (strncmp (reply, "SUCCEED", 7) != 0)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Sleep command failed"));

        return FALSE;
    }

    return TRUE;
}

XfpmPassState
xfpm_suspend_sudo_send_password (XfpmSuspend *suspend,
                                 const gchar *password)
{
    gchar        buf[1024];
    gsize        len_buf, len_send;
    gint         fd;
    gsize        len;
    gssize       result;
    gint         attempts;
    const gchar *errmsg = NULL;

    g_return_val_if_fail (suspend->priv->helper_state == SUDO_AVAILABLE, PASSWORD_FAILED);
    g_return_val_if_fail (suspend->priv->helper_outfile != NULL, PASSWORD_FAILED);
    g_return_val_if_fail (suspend->priv->helper_infile != NULL, PASSWORD_FAILED);
    g_return_val_if_fail (suspend->priv->helper_require_password, PASSWORD_FAILED);
    g_return_val_if_fail (password != NULL, PASSWORD_FAILED);

    /* write password to sudo helper */
    g_snprintf (buf, sizeof (buf), "%s\n", password);
    len_buf = strlen (buf);
    len_send = fwrite (buf, 1, len_buf, suspend->priv->helper_outfile);
    fflush (suspend->priv->helper_outfile);
    bzero (buf, len_buf);

    if (len_send != len_buf || ferror (suspend->priv->helper_outfile) != 0)
    {
        errmsg = "Failed to send password to sudo";
        goto err1;
    }

    fd = fileno (suspend->priv->helper_infile);

    for (len = 0, attempts = 0;;)
    {
        result = read (fd, buf + len, 256 - len);

        if (result < 0)
        {
            errmsg = "Failed to read data from sudo";
            goto err1;
        }
        else if (result == 0)
        {
            /* don't try too often */
            if (++attempts > 20)
            {
                errmsg = "Too many password attempts";
                goto err1;
            }

            continue;
        }
        else if (result + len >= sizeof (buf))
        {
            errmsg = "Received too much data from sudo";
            goto err1;
        }

        len += result;
        buf[len] = '\0';

        if (len >= 15)
        {
            if (g_str_has_suffix (buf, "XFPM_SUDO_PASS "))
            {
                return PASSWORD_RETRY;
            }
            else if (g_str_has_suffix (buf, "XFPM_SUDO_DONE "))
            {
                /* sudo is unlocked, no further passwords required */
                suspend->priv->helper_require_password = FALSE;

                return PASSWORD_SUCCEED;
            }
        }
    }

    return PASSWORD_FAILED;

err1:
    g_printerr (PACKAGE_NAME ": %s.\n\n", errmsg);
    return PASSWORD_FAILED;
}

gboolean
xfpm_suspend_password_required (XfpmSuspend *suspend)
{
    g_return_val_if_fail (XFPM_IS_SUSPEND (suspend), TRUE);

    return suspend->priv->helper_require_password;
}

HelperState
xfpm_suspend_sudo_get_state (XfpmSuspend *suspend)
{
    g_return_val_if_fail (XFPM_IS_SUSPEND (suspend), SUDO_NOT_INITIAZED);

    return suspend->priv->helper_state;
}




#ifdef BACKEND_TYPE_FREEBSD
static gboolean
freebsd_supports_sleep_state (const gchar *state)
{
    gboolean ret = FALSE;
    gchar *sleep_states;

    XFPM_DEBUG("entering");

    sleep_states = up_get_string_sysctl (NULL, "hw.acpi.supported_sleep_state");
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
    gboolean ret = FALSE;
    gchar *command;
    GError *error = NULL;
    gint exit_status;

    XFPM_DEBUG("entering");

    /* run script from pm-utils */
    command = g_strdup_printf ("/usr/bin/pm-is-supported --%s", state);
    g_debug ("excuting command: %s", command);
    ret = g_spawn_command_line_sync (command, NULL, NULL, &exit_status, &error);
    if (!ret) {
        g_warning ("failed to run script: %s", error->message);
        g_error_free (error);
        goto out;
    }
    ret = (WIFEXITED(exit_status) && (WEXITSTATUS(exit_status) == EXIT_SUCCESS));

out:
    g_free (command);

    return ret;
}
#endif


gboolean
xfpm_suspend_can_suspend (void)
{
    XFPM_DEBUG("entering");
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
    XFPM_DEBUG("entering");
#ifdef BACKEND_TYPE_FREEBSD
    return freebsd_supports_sleep_state ("S4");
#endif
#ifdef BACKEND_TYPE_LINUX
    return linux_supports_sleep_state ("hibernate");
#endif
#ifdef BACKEND_TYPE_OPENBSD
    return FALSE;
#endif

    return FALSE;
}
