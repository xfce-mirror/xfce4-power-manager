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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_USER_H
#include <sys/user.h>
#endif

#if defined(__FreeBSD__)
#include <sys/stat.h>
#elif defined(__SVR4) && defined(__sun)
#include <fcntl.h>
#include <procfs.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "xfpm-polkit.h"
#include "xfpm-debug.h"

#include "xfpm-common.h"

static void xfpm_polkit_finalize   (GObject *object);

#define XFPM_POLKIT_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), XFPM_TYPE_POLKIT, XfpmPolkitPrivate))

struct XfpmPolkitPrivate
{
    GDBusConnection   *bus;

#ifdef ENABLE_POLKIT
    GDBusProxy        *proxy;
    GVariant          *subject;
    GVariant          *details;

    gulong             destroy_id;
    gboolean           subject_valid;
#endif
};

enum
{
    AUTH_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XfpmPolkit, xfpm_polkit, G_TYPE_OBJECT)

#ifdef ENABLE_POLKIT
#if defined(__FreeBSD__)
/**
 * Taken from polkitunixprocess.c code to get process start
 * time from pid.
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 **/
static gboolean
get_kinfo_proc (pid_t pid, struct kinfo_proc *p)
{
    int mib[4];
    size_t len;
    
    len = 4;
    sysctlnametomib ("kern.proc.pid", mib, &len);
    
    len = sizeof (struct kinfo_proc);
    mib[3] = pid;
    
    if (sysctl (mib, 4, p, &len, NULL, 0) == -1)
	return FALSE;

    return TRUE;
}
#endif /*if defined(__FreeBSD__)*/

static guint64
get_start_time_for_pid (pid_t pid)
{
    guint64 start_time = 0;
#if defined(__linux)
    gchar *filename;
    gchar *contents;
    size_t length;
    gchar **tokens;
    guint num_tokens;
    gchar *p;
    gchar *endp;
  
    start_time = 0;
    contents = NULL;
    
    filename = g_strdup_printf ("/proc/%d/stat", pid);
    
    if (!g_file_get_contents (filename, &contents, &length, NULL))
	goto out;

    /* start time is the token at index 19 after the '(process name)' entry - since only this
     * field can contain the ')' character, search backwards for this to avoid malicious
     * processes trying to fool us
     */
    p = strrchr (contents, ')');
    if (p == NULL)
    {
	goto out;
    }
    
    p += 2; /* skip ') ' */

    if (p - contents >= (int) length)
    {
	g_warning ("Error parsing file %s", filename);
	goto out;
    }
    
    tokens = g_strsplit (p, " ", 0);
    
    num_tokens = g_strv_length (tokens);
    
    if (num_tokens < 20)
    {
	g_warning ("Error parsing file %s", filename);
	goto out;
    }

    start_time = strtoull (tokens[19], &endp, 10);
    if (endp == tokens[19])
    {
	g_warning ("Error parsing file %s", filename);
	goto out;
    }
    g_strfreev (tokens);

 out:
    g_free (filename);
    g_free (contents);
    
#elif defined(__FreeBSD__)

    struct kinfo_proc p;
    
    start_time = 0;

    if (! get_kinfo_proc (pid, &p))
    {
	g_warning ("Error obtaining start time for %d (%s)",
		   (gint) pid,
		   g_strerror (errno));
	goto out;
    }

    start_time = (guint64) p.ki_start.tv_sec;
    
out:
#elif defined(__SVR4) && defined(__sun)

    psinfo_t p;
    gchar *filename;
    int fd;

    start_time = 0;

    filename = g_strdup_printf ("/proc/%d/psinfo", (int) pid);
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
	g_warning ("Error opening %s (%s)",
		   filename,
		   g_strerror (errno));
	goto out;
    }
    if (read(fd, &p, sizeof (p)) != sizeof (p))
    {
	g_warning ("Error reading %s",
		   filename);
	close(fd);
	goto out;
    }
    start_time = (guint64) p.pr_start.tv_sec;
    close(fd);
out:
    g_free (filename);
#endif
    
    return start_time;
}
#endif /*ENABLE_POLKIT*/


#ifdef ENABLE_POLKIT
static gboolean
xfpm_polkit_free_data (gpointer data)
{
    XfpmPolkit *polkit;
    
    polkit = XFPM_POLKIT (data);

    g_assert (polkit->priv->subject_valid);

    XFPM_DEBUG ("Destroying Polkit data");

    g_variant_unref (polkit->priv->details);
    g_variant_unref (polkit->priv->subject);
    
    polkit->priv->details      = NULL;
    polkit->priv->subject      = NULL;

    polkit->priv->destroy_id = 0;
    polkit->priv->subject_valid = FALSE;
    
    return FALSE;
}

static void
xfpm_polkit_init_data (XfpmPolkit *polkit)
{
    gint pid;
    guint64 start_time;
    GVariantBuilder builder;
    const gchar *subject_kind = NULL;

    if (polkit->priv->subject_valid)
	return;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

    pid = getpid ();

    start_time = get_start_time_for_pid (pid);

    if ( G_LIKELY (start_time != 0 ) )
    {
	GVariant *var;

	subject_kind = "unix-process";

        var = g_variant_new ("u", (guint32)pid);
        g_variant_builder_add (&builder, "{sv}", "pid", var);

        var = g_variant_new ("t", start_time);
        g_variant_builder_add (&builder, "{sv}", "start-time", var);

	XFPM_DEBUG ("Using unix session polkit subject");
    }
    else
    {
	g_warning ("Unable to create polkit subject");
    }

    polkit->priv->subject =
	g_variant_ref_sink (g_variant_new ("(sa{sv})",
					   subject_kind,
					   &builder));
    
    /**
     * Polkit details, will leave it empty.
     **/
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));
    polkit->priv->details =
	g_variant_ref_sink (g_variant_new ("a{ss}",
					   &builder));
    
    /*Clean these data after 1 minute*/
    polkit->priv->destroy_id = 
	g_timeout_add_seconds (60, (GSourceFunc) xfpm_polkit_free_data, polkit);
    
    polkit->priv->subject_valid = TRUE;
}
#endif /*ENABLE_POLKIT*/

static gboolean
xfpm_polkit_check_auth_intern (XfpmPolkit *polkit, const gchar *action_id)
{
#ifdef ENABLE_POLKIT
    GError *error = NULL;
    gboolean is_authorized = FALSE;
    GVariant *var;
    
    /**
     * <method name="CheckAuthorization">      
     *   <arg type="(sa{sv})" name="subject" direction="in"/>      
     *   <arg type="s" name="action_id" direction="in"/>           
     *   <arg type="a{ss}" name="details" direction="in"/>         
     *   <arg type="u" name="flags" direction="in"/>               
     *   <arg type="s" name="cancellation_id" direction="in"/>     
     *   <arg type="(bba{ss})" name="result" direction="out"/>     
     * </method>
     *
     **/
    
    g_return_val_if_fail (polkit->priv->proxy != NULL, FALSE);
    g_return_val_if_fail (polkit->priv->subject_valid, FALSE);
    
    var = g_variant_new ("(@(sa{sv})s@a{ss}us)",
                         polkit->priv->subject,
                         action_id,
                         polkit->priv->details,
                         0,
                         NULL);

    var = g_dbus_proxy_call_sync (polkit->priv->proxy, "CheckAuthorization",
                                  var,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL,
                                  &error);
    
    if ( G_LIKELY (var) )
    {
	g_variant_get (var, "((bba{ss}))",
		       &is_authorized, NULL, NULL);
    }
    else if ( error )
    {
	g_warning ("'CheckAuthorization' failed with %s", error->message);
	g_error_free (error);
    }

    g_variant_unref (var);
    
    XFPM_DEBUG ("Action=%s is authorized=%s", action_id, xfpm_bool_to_string (is_authorized));
    
    return is_authorized;
#endif /*ENABLE_POLKIT*/
    return TRUE;
}

#ifdef ENABLE_POLKIT
static void
xfpm_polkit_changed_cb (GDBusProxy *proxy, XfpmPolkit *polkit)
{
    XFPM_DEBUG ("Auth changed");
    g_signal_emit (G_OBJECT (polkit), signals [AUTH_CHANGED], 0);
}
#endif

static void
xfpm_polkit_class_init (XfpmPolkitClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xfpm_polkit_finalize;

    signals [AUTH_CHANGED] = 
        g_signal_new ("auth-changed",
                      XFPM_TYPE_POLKIT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(XfpmPolkitClass, auth_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

    g_type_class_add_private (klass, sizeof (XfpmPolkitPrivate));
}

static void
xfpm_polkit_init (XfpmPolkit *polkit)
{
    GError *error = NULL;
    
    polkit->priv = XFPM_POLKIT_GET_PRIVATE (polkit);

#ifdef ENABLE_POLKIT
    polkit->priv->destroy_id   = 0;
    polkit->priv->subject_valid   = FALSE;
    polkit->priv->proxy        = NULL;
    polkit->priv->subject      = NULL;
    polkit->priv->details      = NULL;
#endif /*ENABLE_POLKIT*/
    
    polkit->priv->bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    
    if ( error )
    {
	g_critical ("Error getting system bus connection : %s", error->message);
	g_error_free (error);
	goto out;
    }

#ifdef ENABLE_POLKIT
    polkit->priv->proxy = 
	g_dbus_proxy_new_sync (polkit->priv->bus,
			       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
			       NULL,
			       "org.freedesktop.PolicyKit1",
			       "/org/freedesktop/PolicyKit1/Authority",
			       "org.freedesktop.PolicyKit1.Authority",
			       NULL,
			       NULL);
    
    if (G_LIKELY (polkit->priv->proxy) )
    {
	g_signal_connect (polkit->priv->proxy, "Changed",
			  G_CALLBACK (xfpm_polkit_changed_cb), polkit);
    }
    else
    {
	g_warning ("Failed to create proxy for 'org.freedesktop.PolicyKit1'");
    }
#endif /*ENABLE_POLKIT*/

out:
    ;
}

static void
xfpm_polkit_finalize (GObject *object)
{
    XfpmPolkit *polkit;

    polkit = XFPM_POLKIT (object);

#ifdef ENABLE_POLKIT
    if ( polkit->priv->proxy )
    {
	g_signal_handlers_disconnect_by_func (polkit->priv->proxy,
					     G_CALLBACK (xfpm_polkit_changed_cb), polkit);
	g_object_unref (polkit->priv->proxy);
    }

    if ( polkit->priv->subject_valid )
    {
	xfpm_polkit_free_data (polkit);
	if (polkit->priv->destroy_id != 0 )
	    g_source_remove (polkit->priv->destroy_id);
    }
#endif /*ENABLE_POLKIT*/


    if ( polkit->priv->bus )
	g_object_unref (polkit->priv->bus);

    G_OBJECT_CLASS (xfpm_polkit_parent_class)->finalize (object);
}

XfpmPolkit *
xfpm_polkit_get (void)
{
    static gpointer xfpm_polkit_obj = NULL;
    
    if ( G_LIKELY (xfpm_polkit_obj) )
    {
	g_object_ref (xfpm_polkit_obj);
    }
    else
    {
	xfpm_polkit_obj = g_object_new (XFPM_TYPE_POLKIT, NULL);
	g_object_add_weak_pointer (xfpm_polkit_obj, &xfpm_polkit_obj);
    }
    
    return XFPM_POLKIT (xfpm_polkit_obj);
}

gboolean xfpm_polkit_check_auth	(XfpmPolkit *polkit, const gchar *action_id)
{
#ifdef ENABLE_POLKIT
    xfpm_polkit_init_data (polkit);
#endif
    return xfpm_polkit_check_auth_intern (polkit, action_id);
}
